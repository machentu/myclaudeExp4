/*
 * test_ic_angle_vp6.c - end-to-end test for the VP6 per-keypoint IC angle.
 *
 * Three-way comparison on every case:
 *   (1) ref_ic_angle   - a VERBATUM copy of refactor's ngd_ic_angle /
 *       ngd_pix_clamp (image-based, the oracle). Uses the umax table from
 *       ic_angle_init_umax (1:1 with ngd_orb_init).
 *   (2) ic_angle_full  - the PC reference (ic_angle_kernel on the full image).
 *   (3) ic_angle_vp6   - the VP6 per-keypoint patch-DMA driver.
 *
 * (1)==(2) proves ic_angle_core matches refactor. (2)==(3) proves the VP6 path
 * is bit-exact with the PC reference. Together: VP6 == refactor, per keypoint,
 * as an exact float (same m_01/m_10, same atan2f). Covers gradient/random/
 * checker images, keypoints in the interior AND near the frame edge (exercises
 * the edge-clamp), and several counts (incl. 0 and 1).
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "ic_angle_core.h"
#include "ic_angle_vp6.h"
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

/* ---- (1) verbatim oracle: refactor ngd_pix_clamp / ngd_ic_angle (with fast_atan2) ---- */
/* Bit-exact fast_atan2 from refactor/src/orb.c::ngd_fast_atan2 (cv::fastAtan2). */
#define IC_DBL_EPSILON 2.2204460492503131e-16
static const float ic_atan2_p1 = 0.9997878412794807f*(float)(180.0/3.14159265358979323846);
static const float ic_atan2_p3 = -0.3258083974640975f*(float)(180.0/3.14159265358979323846);
static const float ic_atan2_p5 =  0.1555786518463281f*(float)(180.0/3.14159265358979323846);
static const float ic_atan2_p7 = -0.04432655554792128f*(float)(180.0/3.14159265358979323846);
static float ic_fast_atan2(float y, float x)
{
    float ax = fabsf(x), ay = fabsf(y);
    float a, c, c2;
    if (ax >= ay) {
        c = ay/(ax + (float)IC_DBL_EPSILON);
        c2 = c*c;
        a = (((ic_atan2_p7*c2 + ic_atan2_p5)*c2 + ic_atan2_p3)*c2 + ic_atan2_p1)*c;
    } else {
        c = ax/(ay + (float)IC_DBL_EPSILON);
        c2 = c*c;
        a = 90.f - (((ic_atan2_p7*c2 + ic_atan2_p5)*c2 + ic_atan2_p3)*c2 + ic_atan2_p1)*c;
    }
    if (x < 0) a = 180.f - a;
    if (y < 0) a = 360.f - a;
    return a;
}
static inline uint8_t ref_pix_clamp(const uint8_t *img, int w, int h, int x, int y) {
    if (x < 0) x = 0; else if (x >= w) x = w - 1;
    if (y < 0) y = 0; else if (y >= h) y = h - 1;
    return img[y * w + x];
}
static float ref_ic_angle(const uint8_t *img, int w, int h, float ptx, float pty, const int *umax) {
    int m_01 = 0, m_10 = 0;
    int cx = (int)lroundf(ptx), cy = (int)lroundf(pty);
    for (int u = -15; u <= 15; ++u)
        m_10 += u * (int)ref_pix_clamp(img, w, h, cx + u, cy);
    for (int v = 1; v <= 15; ++v) {
        int v_sum = 0, d = umax[v];
        for (int u = -d; u <= d; ++u) {
            int vp = (int)ref_pix_clamp(img, w, h, cx + u, cy + v);
            int vm = (int)ref_pix_clamp(img, w, h, cx + u, cy - v);
            v_sum += (vp - vm);
            m_10 += u * (vp + vm);
        }
        m_01 += v * v_sum;
    }
    return ic_fast_atan2((float)m_01, (float)m_10);   /* degrees [0,360), == cv::fastAtan2 */
}

/* Returns 0 on full 3-way bit-exact match (every angle float-identical), 1 otherwise. */
static int run_one(xvTileManager *tm, int w, int h, int kind, int n_kps, const int *umax)
{
    uint8_t *src = (uint8_t *)malloc((size_t)w * h);
    fast9_keypoint *kps = (fast9_keypoint *)malloc(sizeof(fast9_keypoint) * (size_t)(n_kps > 0 ? n_kps : 1));
    float *a_ref = (float *)malloc(sizeof(float) * (size_t)(n_kps > 0 ? n_kps : 1));
    float *a_full = (float *)malloc(sizeof(float) * (size_t)(n_kps > 0 ? n_kps : 1));
    float *a_vp6 = (float *)malloc(sizeof(float) * (size_t)(n_kps > 0 ? n_kps : 1));
    if (!src || !kps || !a_ref || !a_full || !a_vp6) { fprintf(stderr, "oom\n"); free(src); free(kps); free(a_ref); free(a_full); free(a_vp6); return 1; }
    fill_image(src, w, h, kind);

    /* keypoints: random integer positions across the WHOLE frame (interior +
     * near-edge), so the edge-clamp path is exercised. */
    for (int i = 0; i < n_kps; i++) {
        kps[i].x = (float)(rand() % w);
        kps[i].y = (float)(rand() % h);
        kps[i].response = 0; kps[i].octave = 0; kps[i].angle = 0; kps[i].size = 0;
    }

    for (int i = 0; i < n_kps; i++) {
        a_ref[i]  = ref_ic_angle(src, w, h, kps[i].x, kps[i].y, umax);
        a_full[i] = ic_angle_full(src, w, h, w, (int)lroundf(kps[i].x), (int)lroundf(kps[i].y), umax);
    }

    xvFrame *src_frame = xvCreateFrame(tm, src, (uint32_t)w * h, w, h, w, 1, 1, FRAME_EDGE_PADDING, 0);
    if (!src_frame) { fprintf(stderr, "  frame create failed\n"); free(src); free(kps); free(a_ref); free(a_full); free(a_vp6); return 1; }
    IcAngleVp6Config cfg = IC_ANGLE_VP6_DEFAULT_CONFIG;
    int rc = ic_angle_vp6_process(tm, src_frame, w, h, kps, a_vp6, n_kps, umax, &cfg);
    xvFreeFrame(tm, src_frame);

    int ok = 0;
    if (rc != 0) {
        fprintf(stderr, "  ic_angle_vp6_process failed: %d\n", rc);
    } else {
        int mism_ref_full = 0, mism_ref_vp6 = 0, first_bad = -1;
        for (int i = 0; i < n_kps; i++) {
            if (a_ref[i] != a_full[i]) mism_ref_full++;
            if (a_ref[i] != a_vp6[i])  { mism_ref_vp6++; if (first_bad < 0) first_bad = i; }
        }
        ok = (mism_ref_full == 0 && mism_ref_vp6 == 0);
        printf("  %-9s %dx%d n_kps=%-4d : full_mism=%d vp6_mism=%d\n",
               img_name(kind), w, h, n_kps, mism_ref_full, mism_ref_vp6);
        if (!ok) {
            fprintf(stderr, "    MISMATCH: full=%d vp6=%d\n", mism_ref_full, mism_ref_vp6);
            if (first_bad >= 0) {
                int i = first_bad;
                int cx = (int)lroundf(kps[i].x), cy = (int)lroundf(kps[i].y);
                fprintf(stderr, "    kp[%d]=(%d,%d) ref=%g full=%g vp6=%g\n",
                        i, cx, cy, a_ref[i], a_full[i], a_vp6[i]);
            }
        }
    }

    free(src); free(kps); free(a_ref); free(a_full); free(a_vp6);
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

    int umax[IC_ANGLE_UMAX_SIZE];
    ic_angle_init_umax(umax);

    printf("=== VP6 IC angle vs PC (3-way: refactor-ref vs ic_angle_full vs ic_angle_vp6) ===\n");
    printf("  umax[0..15]:");
    for (int i = 0; i < 16; i++) printf(" %d", umax[i]);
    printf("\n");

    int fail = 0;
    struct { int w, h, kind, n; } cs[] = {
        {128, 96, IMG_GRADIENT, 100},
        {128, 96, IMG_RANDOM,   200},
        {128, 96, IMG_CHECKER,   50},
        { 64, 64, IMG_RANDOM,    80},
        { 30, 30, IMG_GRADIENT,  20},   /* small frame -> more edge-clamping */
        {128, 96, IMG_RANDOM,     1},   /* single keypoint */
        {128, 96, IMG_RANDOM,     0},   /* zero keypoints */
        { 45, 33, IMG_CHECKER,   40},   /* non-multiple, edge-heavy */
    };
    const int n = (int)(sizeof(cs) / sizeof(cs[0]));
    for (int i = 0; i < n; i++)
        if (run_one(&tm, cs[i].w, cs[i].h, cs[i].kind, cs[i].n, umax) != 0)
            fail = 1;

    printf("=== %s ===\n", fail ? "FAIL" : "ALL PASS");
    return fail ? 1 : 0;
}
