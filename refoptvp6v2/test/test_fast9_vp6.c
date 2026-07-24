/*
 * test_fast9_vp6.c - end-to-end test for the VP6 strip-based FAST-9 detect + NMS.
 *
 * Three-way comparison on every case:
 *   (1) fast9_ref_*  - a VERBATUM copy of refactor's ngd_fast_score /
 *       ngd_fast_detect (image-based, the oracle).
 *   (2) fast9_full   - the PC reference (fast9_detect_kernel on the full image).
 *   (3) fast9_vp6    - the VP6 strip driver (ping/pong DMA, per-strip kernel).
 *
 * (1)==(2) proves fast9_core matches refactor. (2)==(3) proves the VP6 strip
 * path is bit-exact with the PC reference. Together: VP6 == refactor, including
 * keypoint count, ORDER (global raster - strips emit row-by-row across the full
 * ROI width), every field (x,y,response,octave,angle,size), and the cap cut.
 * Covers whole-image and inset ROIs, multiple thresholds (7/20/40), multiple
 * strip heights {16,32,64}, small frames, non-multiple borders, random images
 * (corner-rich), and cap tests.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fast9_core.h"
#include "fast9_vp6.h"
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

enum { IMG_CHECKER, IMG_RANDOM, IMG_CONSTANT };
static const char *img_name(int k) {
    switch (k) { case IMG_CHECKER: return "checker"; case IMG_RANDOM: return "random"; default: return "constant"; }
}
static void fill_image(uint8_t *p, int w, int h, int kind) {
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        int g;
        switch (kind) {
        case IMG_CHECKER:  g = (((x / 8) + (y / 8)) & 1) ? 255 : 0; break;
        case IMG_RANDOM:   g = rand() & 255; break;
        default:           g = 128; break;
        }
        p[(size_t)y * w + x] = (uint8_t)g;
    }
}

/* ---- (1) verbatim oracle: refactor ngd_fast_score / ngd_fast_detect ---- */
static const int ref_circle[16][2] = {
    { 0,-3},{ 1,-3},{ 2,-2},{ 3,-1},{ 3, 0},{ 3, 1},{ 2, 2},{ 1, 3},
    { 0, 3},{-1, 3},{-2, 2},{-3, 1},{-3, 0},{-3,-1},{-2,-2},{-1,-3}
};
static int ref_score(const uint8_t *img, int w, int h, int stride, int x0, int y0, int t) {
    int p = img[y0 * stride + x0]; int v[16];
    for (int i = 0; i < 16; i++) { int x = x0 + ref_circle[i][0], y = y0 + ref_circle[i][1];
        if (x < 0 || x >= w || y < 0 || y >= h) return -1; v[i] = img[y * stride + x]; }
    int hi = 0, lo = 0; const int card[4] = {0, 4, 8, 12};
    for (int k = 0; k < 4; k++) { if (v[card[k]] > p + t) hi++; else if (v[card[k]] < p - t) lo++; }
    if (hi < 2 && lo < 2) return -1;
    int best_hi = -1, best_lo = -1;
    for (int s = 0; s < 16; s++) { int allhi = 1, alllo = 1, minhi = 255, maxlo = 0;
        for (int k = 0; k < 9; k++) { int val = v[(s + k) & 15];
            if (val > p + t) { if (val < minhi) minhi = val; } else allhi = 0;
            if (val < p - t) { if (val > maxlo) maxlo = val; } else alllo = 0; }
        if (allhi) { int m = minhi - p; if (m > best_hi) best_hi = m; }
        if (alllo) { int m = p - maxlo; if (m > best_lo) best_lo = m; } }
    int best = best_hi > best_lo ? best_hi : best_lo; if (best < 0) return -1;
    return best - 1;   /* cv::cornerScore<16> returns (max threshold) - 1 */
}
static void ref_detect(const uint8_t *img, int w, int h, int stride,
                       int iniX, int maxX, int iniY, int maxY, int t,
                       fast9_keypoint *out, int *n, int cap) {
    for (int y = iniY; y < maxY; y++) for (int x = iniX; x < maxX; x++) {
        int s = ref_score(img, w, h, stride, x, y, t); if (s < 0) continue;
        int is_max = 1;
        for (int dy = -1; dy <= 1 && is_max; dy++) for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue; int xn = x + dx, yn = y + dy;
            if (xn < iniX || xn >= maxX || yn < iniY || yn >= maxY) continue;
            int sn = ref_score(img, w, h, stride, xn, yn, t); if (sn > s) { is_max = 0; break; } }
        if (!is_max) continue; if (*n >= cap) return;
        fast9_keypoint *kp = &out[(*n)++]; kp->x = (float)x; kp->y = (float)y;
        kp->response = (float)s; kp->octave = 0; kp->angle = 0.0f; kp->size = 0.0f;
    }
}

static int kps_equal(const fast9_keypoint *a, const fast9_keypoint *b, int n) {
    for (int i = 0; i < n; i++)
        if (a[i].x != b[i].x || a[i].y != b[i].y || a[i].response != b[i].response ||
            a[i].octave != b[i].octave || a[i].angle != b[i].angle || a[i].size != b[i].size)
            return 0;
    return 1;
}

/* Returns 0 on full 3-way bit-exact match (count + order + fields), 1 otherwise. */
static int run_one(xvTileManager *tm, int w, int h,
                   int iniX, int maxX, int iniY, int maxY,
                   int t, int strip_h, int kind, int cap)
{
    uint8_t *src = (uint8_t *)malloc((size_t)w * h);
    fast9_keypoint *k_ref  = (fast9_keypoint *)malloc(sizeof(fast9_keypoint) * (size_t)cap);
    fast9_keypoint *k_full = (fast9_keypoint *)malloc(sizeof(fast9_keypoint) * (size_t)cap);
    fast9_keypoint *k_vp6  = (fast9_keypoint *)malloc(sizeof(fast9_keypoint) * (size_t)cap);
    if (!src || !k_ref || !k_full || !k_vp6) { fprintf(stderr, "oom\n"); free(src); free(k_ref); free(k_full); free(k_vp6); return 1; }
    fill_image(src, w, h, kind);

    int n_ref = 0, n_full = 0, n_vp6 = 0;
    ref_detect (src, w, h, w, iniX, maxX, iniY, maxY, t, k_ref,  &n_ref,  cap);
    fast9_full (src, w, h, w, iniX, maxX, iniY, maxY, t, k_full, &n_full, cap);

    xvFrame *src_frame = xvCreateFrame(tm, src, (uint32_t)w * h, w, h, w, 1, 1, FRAME_EDGE_PADDING, 0);
    if (!src_frame) { fprintf(stderr, "  frame create failed\n"); free(src); free(k_ref); free(k_full); free(k_vp6); return 1; }
    int roiW = maxX - iniX;
    Fast9Vp6Config cfg = { strip_h, roiW + 8, strip_h + 8 };
    int rc = fast9_vp6_process(tm, src_frame, w, h, iniX, maxX, iniY, maxY, t, k_vp6, &n_vp6, cap, &cfg);
    xvFreeFrame(tm, src_frame);

    int ok = 0;
    if (rc != 0) {
        fprintf(stderr, "  fast9_vp6_process failed: %d\n", rc);
    } else {
        int same_count = (n_ref == n_full && n_full == n_vp6);
        int same_full  = kps_equal(k_ref, k_full, n_ref);
        int same_vp6   = kps_equal(k_ref, k_vp6,  n_ref);
        ok = same_count && same_full && same_vp6;
        printf("  %-9s %dx%d ROI=[%d,%d)x[%d,%d) t=%2d strip=%-2d cap=%-6d : n=%-4d count=%s full=%s vp6=%s\n",
               img_name(kind), w, h, iniX, maxX, iniY, maxY, t, strip_h, cap, n_ref,
               same_count ? "ok" : "DIFF", same_full ? "ok" : "DIFF", same_vp6 ? "ok" : "DIFF");
        if (!ok)
            fprintf(stderr, "    MISMATCH: n_ref=%d n_full=%d n_vp6=%d (full=%d vp6=%d)\n",
                    n_ref, n_full, n_vp6, same_full, same_vp6);
    }

    free(src); free(k_ref); free(k_full); free(k_vp6);
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

    printf("=== VP6 strip FAST-9 vs PC (3-way: refactor-ref vs fast9_full vs fast9_vp6) ===\n");

    int fail = 0;
    int BIG = 1 << 20;  /* effectively unbounded cap for non-cap cases */
    struct { int w,h, ix,mx,iy,my, t, sh, kind, cap; } cs[] = {
        {128,96,  0,128, 0,96,  20, 64, IMG_CHECKER,  BIG},
        {128,96,  0,128, 0,96,  20, 32, IMG_CHECKER,  BIG},
        {128,96,  0,128, 0,96,  20, 16, IMG_CHECKER,  BIG},  /* many strips */
        {128,96, 10,118,10,86,  20, 32, IMG_CHECKER,  BIG},  /* inset ROI */
        {128,96,  0,128, 0,96,   7, 64, IMG_CHECKER,  BIG},  /* low threshold */
        {128,96,  0,128, 0,96,  40, 64, IMG_CHECKER,  BIG},  /* high threshold */
        { 30,30,  0, 30, 0,30,  20, 16, IMG_CHECKER,  BIG},  /* small frame */
        {128,96,  0,128, 0,96,  20, 64, IMG_RANDOM,   BIG},  /* corner-rich */
        {128,96,  0,128, 0,96,  20, 32, IMG_RANDOM,   BIG},
        { 64,64,  0, 64, 0,64,  20, 64, IMG_CONSTANT, BIG},  /* no corners */
        {128,96,  0,128, 0,96,  20, 64, IMG_RANDOM,    50},  /* cap test (hit) */
        {128,96,  0,128, 0,96,  20, 32, IMG_RANDOM,     5},  /* cap test (tight) */
        { 70,50,  0, 70, 0,50,  20, 32, IMG_RANDOM,   BIG},  /* non-multiple */
    };
    const int n = (int)(sizeof(cs) / sizeof(cs[0]));
    for (int i = 0; i < n; i++)
        if (run_one(&tm, cs[i].w, cs[i].h, cs[i].ix, cs[i].mx, cs[i].iy, cs[i].my,
                    cs[i].t, cs[i].sh, cs[i].kind, cs[i].cap) != 0)
            fail = 1;

    printf("=== %s ===\n", fail ? "FAIL" : "ALL PASS");
    return fail ? 1 : 0;
}
