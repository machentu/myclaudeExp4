/*
 * test_orb_blur_vp6.c - end-to-end test for the VP6 tile-based ORB Gaussian
 *                      blur, built via cstub.
 *
 * Builds the P6 TileManager + cstub, runs orb_blur_vp6_process() on synthetic
 * images, and compares the result to the portable orb_blur_full() reference
 * (which is bit-exact with refactor's ngd_gaussian_blur by construction - the
 * same float maths). Because the VP6 path uses the same orb_blur_kernel() on
 * per-tile source regions sized to the 7-tap neighbour extent (bit-exact
 * decomposition, reflect-101 by absolute frame coord), the two must match
 * exactly across image sizes, tile sizes, and border tiles.
 *
 * Build (PC / cstub): see README. Links orb_blur_core.c, orb_blur_vp6.c, the
 * P6 TileManager (tileManager.c, tmUtils.c, idmaEmulate.c) and the cstub lib.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "orb_blur_core.h"
#include "orb_blur_vp6.h"
#include "tileManager.h"

/* ---- tile manager / iDMA setup (mirrors test_ipm_vp6.c / TileManagerInit) ---- */
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
static void intr_cb(void *p) { (void)p; }   /* polling mode; no work needed */

/* image content flavours */
enum { IMG_GRADIENT, IMG_RANDOM, IMG_CONSTANT };

static const char *img_name(int kind)
{
    switch (kind) { case IMG_GRADIENT: return "gradient";
                    case IMG_RANDOM:   return "random";
                    default:           return "constant"; }
}

static void fill_image(uint8_t *p, int w, int h, int kind)
{
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int g;
            switch (kind) {
            case IMG_GRADIENT:
                g = ((x * 255) / (w - 1) + (y * 255) / (h - 1)) / 2;
                if ((x % 16 == 0) || (y % 16 == 0)) g = 255;
                break;
            case IMG_RANDOM:
                g = rand() & 255;
                break;
            default: /* constant */
                g = 128;
                break;
            }
            p[(size_t)y * w + x] = (uint8_t)g;
        }
}

/* Run one (w,h,tw,th,kind) case. Returns 0 on bit-exact match, 1 otherwise. */
static int run_one(xvTileManager *tm, int w, int h, int tw, int th, int kind)
{
    uint8_t *src     = (uint8_t *)malloc((size_t)w * h);
    uint8_t *dst_vp6 = (uint8_t *)calloc((size_t)w * h, 1);
    uint8_t *dst_ref = (uint8_t *)calloc((size_t)w * h, 1);
    if (!src || !dst_vp6 || !dst_ref) {
        fprintf(stderr, "oom (w=%d h=%d)\n", w, h);
        free(src); free(dst_vp6); free(dst_ref);
        return 1;
    }
    fill_image(src, w, h, kind);

    /* PC reference (oracle: bit-exact with refactor ngd_gaussian_blur) */
    orb_blur_full(src, w, h, w, dst_ref);

    /* frames wrapping the DRAM image buffers */
    xvFrame *src_frame = xvCreateFrame(tm, src, (uint32_t)w * h,
                                       w, h, w, 1, 1, FRAME_EDGE_PADDING, 0);
    xvFrame *dst_frame = xvCreateFrame(tm, dst_vp6, (uint32_t)w * h,
                                       w, h, w, 1, 1, FRAME_ZERO_PADDING, 0);
    if (!src_frame || !dst_frame) {
        fprintf(stderr, "  frame create failed (w=%d h=%d)\n", w, h);
        free(src); free(dst_vp6); free(dst_ref);
        return 1;
    }

    /* +8 margin over tile size covers the 7-tap (+6) source region with room */
    OrbBlurVp6Config cfg = { tw, th, tw + 8, th + 8 };
    int rc = orb_blur_vp6_process(tm, src_frame, dst_frame, w, h, &cfg);

    int ok = 0;
    if (rc != 0) {
        fprintf(stderr, "  orb_blur_vp6_process failed: %d (w=%d h=%d tw=%d th=%d)\n",
                rc, w, h, tw, th);
    } else {
        int max_d = 0, mism = 0;
        for (size_t k = 0; k < (size_t)w * h; k++) {
            int d = (int)dst_vp6[k] - (int)dst_ref[k];
            if (d < 0) d = -d;
            if (d > max_d) max_d = d;
            if (d != 0) mism++;
        }
        printf("  %-9s %dx%d tile=%2dx%-2d : max|diff|=%d  mismatched=%d / %d\n",
               img_name(kind), w, h, tw, th, max_d, mism, w * h);
        ok = (mism == 0);
    }

    xvFreeFrame(tm, src_frame);
    xvFreeFrame(tm, dst_frame);
    free(src); free(dst_vp6); free(dst_ref);
    return ok ? 0 : 1;
}

int main(void)
{
    srand(1234);

    /* ---- tile manager init (once, reused across cases) ---- */
    xvTileManager tm;
    void   *banks[2] = { bank0, bank1 };
    int32_t sizes[2] = { POOL_BYTES, POOL_BYTES };
    idma_init(0, MAX_BLOCK_8, MAX_PIF, 0, 0, (idma_err_callback_fn)err_cb);
    uint32_t r = xvCreateTileManager(&tm, idmaObjBuff, 2, banks, sizes,
                                     (idma_err_callback_fn)err_cb,
                                     (idma_callback_fn)intr_cb, NULL,
                                     DMA_DESCR_CNT, MAX_BLOCK_8, MAX_PIF);
    if (r != XVTM_SUCCESS) { fprintf(stderr, "TM init failed: %u\n", r); return 1; }

    printf("=== VP6 tile blur vs PC reference (orb_blur_full) ===\n");

    int fail = 0;
    /* (w, h, tile_w, tile_h, kind) - covers interior, single-tile, non-multiple
     * borders, small frames, and all three image flavours. */
    struct { int w, h, tw, th, kind; } cases[] = {
        { 128, 96, 64, 64, IMG_GRADIENT },
        {  64, 64, 64, 64, IMG_GRADIENT },  /* single tile, all-border */
        {  70, 50, 32, 32, IMG_GRADIENT },  /* non-multiple borders */
        {  30, 30, 16, 16, IMG_GRADIENT },  /* small, border-heavy */
        {  33, 17, 32, 32, IMG_GRADIENT },  /* tiny, borders < tile */
        { 128, 96, 64, 64, IMG_RANDOM   },
        {  64, 64, 64, 64, IMG_CONSTANT },
        {  65, 65, 32, 32, IMG_RANDOM   },  /* just-over-one-tile borders */
    };
    const int n = (int)(sizeof(cases) / sizeof(cases[0]));
    for (int i = 0; i < n; i++) {
        if (run_one(&tm, cases[i].w, cases[i].h, cases[i].tw, cases[i].th, cases[i].kind) != 0)
            fail = 1;
    }

    printf("=== %s ===\n", fail ? "FAIL" : "ALL PASS");
    return fail ? 1 : 0;
}
