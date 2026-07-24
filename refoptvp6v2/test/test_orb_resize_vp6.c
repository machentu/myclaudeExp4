/*
 * test_orb_resize_vp6.c - end-to-end test for the VP6 tile-based bilinear
 *                         resize, built via cstub.
 *
 * Builds the P6 TileManager + cstub, runs orb_resize_vp6_process() on synthetic
 * images, and compares the result to the portable orb_resize_full() reference
 * (which is bit-exact with refactor's ngd_resize_bilinear). Because the VP6
 * path uses the same orb_resize_kernel() on per-tile source regions, the two
 * must match exactly.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "orb_resize_core.h"
#include "orb_resize_vp6.h"
#include "tileManager.h"

/* ---- tile manager / iDMA setup ---- */
#define POOL_BYTES      (512 * 1024)
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

static int run_one(xvTileManager *tm, int sw, int sh, int dw, int dh,
                   int tw, int th, int kind)
{
    uint8_t *src     = (uint8_t *)malloc((size_t)sw * sh);
    uint8_t *dst_vp6 = (uint8_t *)calloc((size_t)dw * dh, 1);
    uint8_t *dst_ref = (uint8_t *)calloc((size_t)dw * dh, 1);
    if (!src || !dst_vp6 || !dst_ref) {
        fprintf(stderr, "oom (sw=%d sh=%d)\n", sw, sh);
        free(src); free(dst_vp6); free(dst_ref);
        return 1;
    }
    fill_image(src, sw, sh, kind);

    /* PC reference (oracle: bit-exact with refactor ngd_resize_bilinear) */
    orb_resize_full(src, sw, sh, dst_ref, dw, dh);

    /* frames wrapping the DRAM image buffers */
    xvFrame *src_frame = xvCreateFrame(tm, src, (uint32_t)sw * sh,
                                       sw, sh, sw, 1, 1, FRAME_EDGE_PADDING, 0);
    xvFrame *dst_frame = xvCreateFrame(tm, dst_vp6, (uint32_t)dw * dh,
                                       dw, dh, dw, 1, 1, FRAME_ZERO_PADDING, 0);
    if (!src_frame || !dst_frame) {
        fprintf(stderr, "  frame create failed (sw=%d sh=%d)\n", sw, sh);
        free(src); free(dst_vp6); free(dst_ref);
        return 1;
    }

    /* max source rows needed for a tile of height th at this scale.
     * ceil(th * sh/dh) rows + 4 for safety margin. */
    int max_src_h = (int)ceil((double)th * (double)sh / (double)dh) + 4;
    OrbResizeVp6Config cfg = { tw, th, sw + 8, max_src_h };
    int rc = orb_resize_vp6_process(tm, src_frame, dst_frame, sw, sh, dw, dh, &cfg);

    int ok = 0;
    if (rc != 0) {
        fprintf(stderr, "  orb_resize_vp6_process failed: %d (sw=%d sh=%d dw=%d dh=%d)\n",
                rc, sw, sh, dw, dh);
    } else {
        int max_d = 0, mism = 0;
        for (size_t k = 0; k < (size_t)dw * dh; k++) {
            int d = (int)dst_vp6[k] - (int)dst_ref[k];
            if (d < 0) d = -d;
            if (d > max_d) max_d = d;
            if (d != 0) mism++;
        }
        printf("  %-9s %dx%d->%dx%d tile=%2dx%-2d : max|diff|=%d  mism=%d/%d\n",
               img_name(kind), sw, sh, dw, dh, tw, th, max_d, mism, dw * dh);
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

    /* tile manager init */
    xvTileManager tm;
    void   *banks[2] = { bank0, bank1 };
    int32_t sizes[2] = { POOL_BYTES, POOL_BYTES };
    idma_init(0, MAX_BLOCK_8, MAX_PIF, 0, 0, (idma_err_callback_fn)err_cb);
    uint32_t r = xvCreateTileManager(&tm, idmaObjBuff, 2, banks, sizes,
                                     (idma_err_callback_fn)err_cb,
                                     (idma_callback_fn)intr_cb, NULL,
                                     DMA_DESCR_CNT, MAX_BLOCK_8, MAX_PIF);
    if (r != XVTM_SUCCESS) { fprintf(stderr, "TM init failed: %u\n", r); return 1; }

    printf("=== VP6 tile resize vs PC reference (orb_resize_full) ===\n");

    int fail = 0;
    /* (sw, sh, dw, dh, tile_w, tile_h, kind) — typical ORB pyramid sizes */
    struct { int sw, sh, dw, dh, tw, th, kind; } cases[] = {
        { 640, 480, 533, 400, 64, 64, IMG_GRADIENT },  /* level 0->1, typical */
        { 533, 400, 444, 333, 64, 64, IMG_GRADIENT },  /* level 1->2 */
        { 128,  96, 106,  80, 64, 64, IMG_GRADIENT },
        {  64,  64,  53,  53, 32, 32, IMG_GRADIENT },  /* small */
        { 100,  80,  53,  40, 40, 40, IMG_RANDOM   },
        { 640, 480, 533, 400, 64, 64, IMG_RANDOM   },
        {  80,  60,  40,  30, 32, 32, IMG_CONSTANT },
        { 200, 150, 133, 100, 48, 48, IMG_GRADIENT },  /* non-multiple tiles */
    };
    const int n = (int)(sizeof(cases) / sizeof(cases[0]));
    for (int i = 0; i < n; i++) {
        if (run_one(&tm, cases[i].sw, cases[i].sh, cases[i].dw, cases[i].dh,
                    cases[i].tw, cases[i].th, cases[i].kind) != 0)
            fail = 1;
    }

    printf("=== %s ===\n", fail ? "FAIL" : "ALL PASS");
    return fail ? 1 : 0;
}
