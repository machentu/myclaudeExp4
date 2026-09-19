/*
 * test_orb_blur_vp6.c - end-to-end test for the VP6 tile-based ORB Gaussian
 *                      blur with the IVP intrinsic kernel, built via cstub.
 *
 * Runs orb_blur_vp6_process() (tile ping/pong DMA + orb_blur_kernel_ivp:
 * 7-stream LA/LAV loads, 24-lane MULUS/MULUSPA MACs, byte-plane vertical
 * pass, PACKVRU rounding) on synthetic images, and compares the result to
 * the portable orb_blur_full() reference (bit-exact with refactor's
 * ngd_gaussian_blur by construction). Any divergence of the IVP formulation
 * from the integer reference fails here.
 *
 * Covers interior/border tiles, single-tile frames, non-multiple borders,
 * small frames, and gradient (max-sum), random, and constant content.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "orb_blur_core.h"
#include "orb_blur_vp6.h"
#include "tileManager.h"

/* ---- tile manager / iDMA setup: shared InitTileMgr-style helper ---- */
#include "orb_tile_manager.h"

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
    if (orb_tile_manager_init(&tm) != 0) return 1;

    printf("=== VP6+IVP tile blur vs PC reference (orb_blur_full) ===\n");

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
        { 320,240, 64, 64, IMG_RANDOM   },  /* bigger frame, many tiles */
        {  71, 50, 32, 32, IMG_RANDOM   },  /* 1-chunk + 7-column tail */
    };
    const int n = (int)(sizeof(cases) / sizeof(cases[0]));
    for (int i = 0; i < n; i++) {
        if (run_one(&tm, cases[i].w, cases[i].h, cases[i].tw, cases[i].th, cases[i].kind) != 0)
            fail = 1;
    }

    printf("=== %s ===\n", fail ? "FAIL" : "ALL PASS");
    return fail ? 1 : 0;
}
