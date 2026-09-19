/*
 * test_brief_int_vp6.c - fixed-point LUT rBRIEF variant (brief_int_vp6),
 *                        built via cstub.
 *
 * Two-level comparison on every case:
 *   (1) HARD: brief_int_vp6 (vector kernel: scrambled tables, MULP/PACKVR
 *       rotation, UNPK split, clamp/linearise, gather stage) must be
 *       BIT-EXACT with brief_int_scalar_kernel (natural-order scalar
 *       reference, same Q7 LUT + (prod+64)>>7 rounding). This validates the
 *       whole vector/scramble machinery.
 *   (2) ERROR METRIC (soft, by design): Hamming distance vs the float
 *       golden descriptor (verbatim refactor ngd_compute_descriptor) is
 *       REPORTED (mean/max over keypoints); the rotation is quantised to
 *       1-degree bins + Q7 sin/cos, so a few bits per keypoint differ.
 *       Gates are calibrated to measured values + margin (see below).
 *
 * Angles deliberately include non-integral values (17.3, 133.7, 271.5,
 * 359.9) to exercise the (int)(angle + 0.5f) binning.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "brief_core.h"
#include "brief_int_vp6.h"
#include "fast9_core.h"      /* fast9_keypoint */
#include "tileManager.h"

#include "orb_tile_manager.h"

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

/* ---- float golden: verbatim refactor ngd_compute_descriptor ---- */
static inline uint8_t ref_pix_clamp(const uint8_t *img, int w, int h, int x, int y) {
    if (x < 0) x = 0; else if (x >= w) x = w - 1;
    if (y < 0) y = 0; else if (y >= h) y = h - 1;
    return img[y * w + x];
}
static void ref_compute_descriptor(const uint8_t *img, int w, int h,
                                   int cx, int cy, float angle, uint8_t *desc) {
    float ar = angle * (float)(3.14159265358979323846 / 180.0);
    float a = (float)cos(ar), b = (float)sin(ar);
    for (int i = 0; i < 32; ++i) {
        int val = 0;
        for (int bit = 0; bit < 8; ++bit) {
            int pair = i * 8 + bit;
            int p1x = brief_bit_pattern_31[pair][0], p1y = brief_bit_pattern_31[pair][1];
            int p2x = brief_bit_pattern_31[pair][2], p2y = brief_bit_pattern_31[pair][3];
            int r1 = (int)rint(p1x * b + p1y * a), c1 = (int)rint(p1x * a - p1y * b);
            int r2 = (int)rint(p2x * b + p2y * a), c2 = (int)rint(p2x * a - p2y * b);
            int t0 = ref_pix_clamp(img, w, h, cx + c1, cy + r1);
            int t1 = ref_pix_clamp(img, w, h, cx + c2, cy + r2);
            val |= (t0 < t1) << bit;
        }
        desc[i] = (uint8_t)val;
    }
}

static int hamming32(const uint8_t *a, const uint8_t *b) {
    int d = 0;
    for (int i = 0; i < 32; ++i) {
        unsigned x = (unsigned)a[i] ^ (unsigned)b[i];
        while (x) { d += (int)(x & 1u); x >>= 1; }
    }
    return d;
}

static int desc_equal(const uint8_t *a, const uint8_t *b, int n) {
    for (int i = 0; i < n * 32; i++) if (a[i] != b[i]) return 0;
    return 1;
}

/* Returns 0 on success (vector==scalar bit-exact AND hamming gates hold). */
static int run_one(xvTileManager *tm, int w, int h, int kind, int n_kps,
                   int mean_gate, int max_gate)
{
    uint8_t *src = (uint8_t *)malloc((size_t)w * h);
    fast9_keypoint *kps = (fast9_keypoint *)malloc(sizeof(fast9_keypoint) * (size_t)(n_kps > 0 ? n_kps : 1));
    uint8_t *d_gold = (uint8_t *)malloc((size_t)(n_kps > 0 ? n_kps : 1) * 32);
    uint8_t *d_scal = (uint8_t *)malloc((size_t)(n_kps > 0 ? n_kps : 1) * 32);
    uint8_t *d_vec  = (uint8_t *)malloc((size_t)(n_kps > 0 ? n_kps : 1) * 32);
    if (!src || !kps || !d_gold || !d_scal || !d_vec) { fprintf(stderr, "oom\n"); free(src); free(kps); free(d_gold); free(d_scal); free(d_vec); return 1; }
    fill_image(src, w, h, kind);

    int8_t lut[722];
    brief_int_build_lut(lut);

    float fixed[8] = { 0.0f, 17.3f, 45.0f, 90.0f, 133.7f, 180.0f, 271.5f, 359.9f };
    for (int i = 0; i < n_kps; i++) {
        kps[i].x = (float)(rand() % w);
        kps[i].y = (float)(rand() % h);
        kps[i].angle = (i < 8 && n_kps >= 8) ? fixed[i] : (float)(rand() % 360);
        kps[i].response = 0; kps[i].octave = 0; kps[i].size = 0;
    }

    for (int i = 0; i < n_kps; i++) {
        int cx = (int)lroundf(kps[i].x), cy = (int)lroundf(kps[i].y);
        ref_compute_descriptor(src, w, h, cx, cy, kps[i].angle, d_gold + (size_t)i * 32);
        brief_int_scalar_kernel(src, w, 0, 0, w, h, cx, cy, kps[i].angle,
                                d_scal + (size_t)i * 32, lut);
    }

    xvFrame *src_frame = xvCreateFrame(tm, src, (uint32_t)w * h, w, h, w, 1, 1, FRAME_EDGE_PADDING, 0);
    if (!src_frame) { fprintf(stderr, "  frame create failed\n"); free(src); free(kps); free(d_gold); free(d_scal); free(d_vec); return 1; }
    BriefIntVp6Config cfg = BRIEF_INT_VP6_DEFAULT_CONFIG;
    int rc = brief_int_vp6_process(tm, src_frame, w, h, kps, d_vec, n_kps, &cfg);
    xvFreeFrame(tm, src_frame);

    int ok = 0;
    if (rc != 0) {
        fprintf(stderr, "  brief_int_vp6_process failed: %d\n", rc);
    } else {
        int same = desc_equal(d_scal, d_vec, n_kps);
        int hmax = 0; long hsum = 0;
        for (int i = 0; i < n_kps; i++) {
            int d = hamming32(d_scal + (size_t)i * 32, d_gold + (size_t)i * 32);
            hsum += d;
            if (d > hmax) hmax = d;
        }
        const double hmean = n_kps ? (double)hsum / n_kps : 0.0;
        ok = same && (n_kps == 0 || (hmean <= mean_gate && hmax <= max_gate));
        printf("  %-9s %dx%d n_kps=%-4d : vec==scalar %s, hamming vs golden mean=%5.1f max=%3d (gates %d/%d)\n",
               img_name(kind), w, h, n_kps, same ? "ok" : "DIFF", hmean, hmax, mean_gate, max_gate);
        if (!same) {
            for (int i = 0; i < n_kps; i++) {
                if (memcmp(d_scal + (size_t)i * 32, d_vec + (size_t)i * 32, 32) != 0) {
                    int cx = (int)lroundf(kps[i].x), cy = (int)lroundf(kps[i].y);
                    fprintf(stderr, "    kp[%d]=(%d,%d) ang=%g scalar=", i, cx, cy, kps[i].angle);
                    for (int b = 0; b < 32; b++) fprintf(stderr, "%02x", d_scal[(size_t)i * 32 + b]);
                    fprintf(stderr, " vec=");
                    for (int b = 0; b < 32; b++) fprintf(stderr, "%02x", d_vec[(size_t)i * 32 + b]);
                    fprintf(stderr, "\n");
                    break;
                }
            }
        }
    }

    free(src); free(kps); free(d_gold); free(d_scal); free(d_vec);
    return ok ? 0 : 1;
}

int main(void)
{
    srand(1234);
    xvTileManager tm;
    if (orb_tile_manager_init(&tm) != 0) return 1;

    printf("=== fixed-point LUT rBRIEF (vec==scalar bit-exact; hamming vs float golden reported) ===\n");

    int fail = 0;
    /* hamming gates: calibrated 2026-09-18 (measured mean/max: gradient
     * 1.2/11, random 14.6/33, checker 2.4/10) x ~3 margin. Real frames are
     * BLURRED (gradient-like) - pure random noise is the adversarial case
     * where every comparison is threshold-sensitive. */
    struct { int w, h, kind, n, mg, xg; } cs[] = {
        {128, 96, IMG_GRADIENT, 100,   8,  40},
        {128, 96, IMG_RANDOM,   200,  32,  96},
        {128, 96, IMG_CHECKER,   50,   8,  40},
        { 64, 64, IMG_RANDOM,    80,  32,  96},
        { 40, 40, IMG_GRADIENT,  20,   8,  40},   /* small frame -> edge-clamp */
        {128, 96, IMG_RANDOM,     1,  32,  96},
        {128, 96, IMG_RANDOM,     0,  32,  96},
        { 50, 35, IMG_CHECKER,   40,   8,  40},
    };
    const int n = (int)(sizeof(cs) / sizeof(cs[0]));
    for (int i = 0; i < n; i++)
        if (run_one(&tm, cs[i].w, cs[i].h, cs[i].kind, cs[i].n, cs[i].mg, cs[i].xg) != 0)
            fail = 1;

    printf("=== %s ===\n", fail ? "FAIL" : "ALL PASS");
    return fail ? 1 : 0;
}
