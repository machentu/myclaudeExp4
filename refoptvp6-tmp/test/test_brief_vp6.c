/*
 * test_brief_vp6.c - end-to-end test for the VP6 per-keypoint rotated-BRIEF.
 *
 * Three-way comparison on every case:
 *   (1) ref_compute_descriptor - a VERBATUM copy of refactor's ngd_compute_descriptor
 *       / ngd_pix_clamp (image-based, the oracle). Uses brief_bit_pattern_31 (the
 *       same verbatim table brief_core.c exposes - validated separately against
 *       refactor/src/orb.c::ngd_bit_pattern_31).
 *   (2) brief_full  - the PC reference (brief_kernel on the full image).
 *   (3) brief_vp6   - the VP6 per-keypoint patch-DMA driver.
 *
 * (1)==(2) proves brief_core matches refactor. (2)==(3) proves the VP6 path is
 * bit-exact with the PC reference. Together: VP6 == refactor, per keypoint, all
 * 32 descriptor bytes identical. Covers gradient/random/checker images, keypoints
 * in the interior AND near the frame edge (exercises the edge-clamp), random
 * angles [0,360) plus fixed angles (0/45/90/180), and several counts.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "brief_core.h"
#include "brief_vp6.h"
#include "fast9_core.h"      /* fast9_keypoint */
#include "tileManager.h"

#define POOL_BYTES      (256 * 1024)
#define DMA_DESCR_CNT   32
#define MAX_PIF         16
#ifndef MAX_BLOCK_8
#define MAX_BLOCK_8     8
#endif

#ifndef ALIGN64
#ifdef _MSC_VER
#define ALIGN64 __declspec(align(64))
#else
#define ALIGN64 __attribute__((aligned(64)))
#endif
#endif

static IDMA_BUFFER_DEFINE(idmaObjBuff, DMA_DESCR_CNT, IDMA_2D_DESC);
static ALIGN64 uint8_t bank0[POOL_BYTES];
static ALIGN64 uint8_t bank1[POOL_BYTES];
static void err_cb(idma_error_details_t *d) { (void)d; fprintf(stderr, "iDMA error\n"); }
static void intr_cb(void *p) { (void)p; }

enum { IMG_GRADIENT, IMG_RANDOM, IMG_CHECKER };
static const char *img_name(int k) {
    switch (k) { case IMG_GRADIENT: return "gradient"; case IMG_RANDOM: return "random"; default: return "checker"; }
}
static void fill_image(uint8_t *p, int w, int h, int kind) {
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        int g;
        switch (kind) {
        case IMG_GRADIENT: g = ((x * 255) / (w - 1) + (y * 255) / (h - 1)) / 2; break;
        case IMG_RANDOM:   g = rand() & 255; break;
        default:           g = (((x / 8) + (y / 8)) & 1) ? 255 : 0; break;
        }
        p[(size_t)y * w + x] = (uint8_t)g;
    }
}

/* ---- (1) verbatim oracle: refactor ngd_pix_clamp / ngd_compute_descriptor ---- */
static inline uint8_t ref_pix_clamp(const uint8_t *img, int w, int h, int x, int y) {
    if (x < 0) x = 0; else if (x >= w) x = w - 1;
    if (y < 0) y = 0; else if (y >= h) y = h - 1;
    return img[y * w + x];
}
static void ref_compute_descriptor(const uint8_t *img, int w, int h,
                                   int cx, int cy, float angle, uint8_t *desc) {
    float ar = angle * (float)(3.14159265358979323846 / 180.0);
    float a = cosf(ar), b = sinf(ar);
    for (int i = 0; i < 32; ++i) {
        int val = 0;
        for (int bit = 0; bit < 8; ++bit) {
            int pair = i * 8 + bit;
            int p1x = brief_bit_pattern_31[pair][0], p1y = brief_bit_pattern_31[pair][1];
            int p2x = brief_bit_pattern_31[pair][2], p2y = brief_bit_pattern_31[pair][3];
            int r1 = (int)lroundf(p1x * b + p1y * a), c1 = (int)lroundf(p1x * a - p1y * b);
            int r2 = (int)lroundf(p2x * b + p2y * a), c2 = (int)lroundf(p2x * a - p2y * b);
            int t0 = ref_pix_clamp(img, w, h, cx + c1, cy + r1);
            int t1 = ref_pix_clamp(img, w, h, cx + c2, cy + r2);
            val |= (t0 < t1) << bit;
        }
        desc[i] = (uint8_t)val;
    }
}

static int desc_equal(const uint8_t *a, const uint8_t *b, int n) {
    for (int i = 0; i < n * 32; i++) if (a[i] != b[i]) return 0;
    return 1;
}

/* Returns 0 on full 3-way bit-exact match (all 32 bytes/keypoint identical), 1 otherwise. */
static int run_one(xvTileManager *tm, int w, int h, int kind, int n_kps)
{
    uint8_t *src = (uint8_t *)malloc((size_t)w * h);
    fast9_keypoint *kps = (fast9_keypoint *)malloc(sizeof(fast9_keypoint) * (size_t)(n_kps > 0 ? n_kps : 1));
    uint8_t *d_ref = (uint8_t *)malloc((size_t)(n_kps > 0 ? n_kps : 1) * 32);
    uint8_t *d_full = (uint8_t *)malloc((size_t)(n_kps > 0 ? n_kps : 1) * 32);
    uint8_t *d_vp6 = (uint8_t *)malloc((size_t)(n_kps > 0 ? n_kps : 1) * 32);
    if (!src || !kps || !d_ref || !d_full || !d_vp6) { fprintf(stderr, "oom\n"); free(src); free(kps); free(d_ref); free(d_full); free(d_vp6); return 1; }
    fill_image(src, w, h, kind);

    /* keypoints: random positions across the WHOLE frame (interior + near-edge)
     * and random angles [0,360), with a few fixed angles mixed in. */
    float fixed[4] = { 0.0f, 45.0f, 90.0f, 180.0f };
    for (int i = 0; i < n_kps; i++) {
        kps[i].x = (float)(rand() % w);
        kps[i].y = (float)(rand() % h);
        kps[i].angle = (i < 4 && n_kps >= 4) ? fixed[i] : (float)(rand() % 360);
        kps[i].response = 0; kps[i].octave = 0; kps[i].size = 0;
    }

    for (int i = 0; i < n_kps; i++) {
        int cx = (int)lroundf(kps[i].x), cy = (int)lroundf(kps[i].y);
        ref_compute_descriptor(src, w, h, cx, cy, kps[i].angle, d_ref + (size_t)i * 32);
        brief_full(src, w, h, w, cx, cy, kps[i].angle, d_full + (size_t)i * 32);
    }

    xvFrame *src_frame = xvCreateFrame(tm, src, (uint32_t)w * h, w, h, w, 1, 1, FRAME_EDGE_PADDING, 0);
    if (!src_frame) { fprintf(stderr, "  frame create failed\n"); free(src); free(kps); free(d_ref); free(d_full); free(d_vp6); return 1; }
    BriefVp6Config cfg = BRIEF_VP6_DEFAULT_CONFIG;
    int rc = brief_vp6_process(tm, src_frame, w, h, kps, d_vp6, n_kps, &cfg);
    xvFreeFrame(tm, src_frame);

    int ok = 0;
    if (rc != 0) {
        fprintf(stderr, "  brief_vp6_process failed: %d\n", rc);
    } else {
        int same_full = desc_equal(d_ref, d_full, n_kps);
        int same_vp6  = desc_equal(d_ref, d_vp6,  n_kps);
        ok = same_full && same_vp6;
        printf("  %-9s %dx%d n_kps=%-4d : full=%s vp6=%s\n",
               img_name(kind), w, h, n_kps, same_full ? "ok" : "DIFF", same_vp6 ? "ok" : "DIFF");
        if (!ok) {
            fprintf(stderr, "    MISMATCH: full=%d vp6=%d\n", same_full, same_vp6);
            for (int i = 0; i < n_kps; i++) {
                if (memcmp(d_ref + (size_t)i * 32, d_vp6 + (size_t)i * 32, 32) != 0) {
                    int cx = (int)lroundf(kps[i].x), cy = (int)lroundf(kps[i].y);
                    fprintf(stderr, "    kp[%d]=(%d,%d) ang=%g ref=", i, cx, cy, kps[i].angle);
                    for (int b = 0; b < 32; b++) fprintf(stderr, "%02x", d_ref[(size_t)i * 32 + b]);
                    fprintf(stderr, " vp6=");
                    for (int b = 0; b < 32; b++) fprintf(stderr, "%02x", d_vp6[(size_t)i * 32 + b]);
                    fprintf(stderr, "\n");
                    break;
                }
            }
        }
    }

    free(src); free(kps); free(d_ref); free(d_full); free(d_vp6);
    return ok ? 0 : 1;
}

int main(void)
{
    srand(1234);
    xvTileManager tm;
    void   *banks[2] = { bank0, bank1 };
    int32_t sizes[2] = { POOL_BYTES, POOL_BYTES };
    idma_init(0, MAX_BLOCK_8, MAX_PIF, 0, 0, (idma_err_callback_fn)err_cb);
    uint32_t r = xvCreateTileManager(&tm, idmaObjBuff, 2, banks, sizes,
                                     (idma_err_callback_fn)err_cb,
                                     (idma_callback_fn)intr_cb, NULL,
                                     DMA_DESCR_CNT, MAX_BLOCK_8, MAX_PIF);
    if (r != XVTM_SUCCESS) { fprintf(stderr, "TM init failed: %u\n", r); return 1; }

    printf("=== VP6 rBRIEF vs PC (3-way: refactor-ref vs brief_full vs brief_vp6) ===\n");

    int fail = 0;
    struct { int w, h, kind, n; } cs[] = {
        {128, 96, IMG_GRADIENT, 100},
        {128, 96, IMG_RANDOM,   200},
        {128, 96, IMG_CHECKER,   50},
        { 64, 64, IMG_RANDOM,    80},
        { 40, 40, IMG_GRADIENT,  20},   /* small frame -> more edge-clamping */
        {128, 96, IMG_RANDOM,     1},
        {128, 96, IMG_RANDOM,     0},
        { 50, 35, IMG_CHECKER,   40},
    };
    const int n = (int)(sizeof(cs) / sizeof(cs[0]));
    for (int i = 0; i < n; i++)
        if (run_one(&tm, cs[i].w, cs[i].h, cs[i].kind, cs[i].n) != 0)
            fail = 1;

    printf("=== %s ===\n", fail ? "FAIL" : "ALL PASS");
    return fail ? 1 : 0;
}
