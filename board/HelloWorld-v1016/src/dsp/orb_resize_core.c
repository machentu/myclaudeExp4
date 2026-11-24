/*
 * orb_resize_core.c - portable bilinear resize kernel (shared PC + VP6 maths).
 *
 * The integer arithmetic is copied 1:1 from refactor/src/orb.c::ngd_resize_bilinear
 * (INTER_RESIZE_COEF_BITS=11, scale_x=sw/dw in double, HResize int accumulator,
 * VResize ((b0*(S0>>4))>>16 + (b1*(S1>>4))>>16 + 2)>>2). Bit-exact with
 * cv::resize INTER_LINEAR 8u on all verified sizes.
 *
 * Build: plain C on PC, or as C++ under the Xtensa cstub (see CMakeLists.txt).
 */
#include "orb_resize_core.h"

#include <math.h>
#include <stdlib.h>

void orb_resize_precompute_h(const uint8_t *src, int sw, int sh,
                             int dw, int dh,
                             int dx0, int n,
                             int *xofs, short *ialpha)
{
    double scale_x = (double)sw / (double)dw;
    (void)src; (void)sh; (void)dh; /* unused */
    for (int dx = 0; dx < n; ++dx) {
        int dxi = dx0 + dx;
        float fx = (float)(((double)dxi + 0.5) * scale_x - 0.5);
        int sx = (int)floor((double)fx);          /* cvFloor */
        fx -= (float)sx;
        if (sx < 0)     { sx = 0; fx = 0.f; }      /* clamp left */
        if (sx >= sw-1) { sx = sw-1; fx = 0.f; }   /* clamp right */
        xofs[dx] = sx;
        float c0 = 1.0f - fx, c1 = fx;
        ialpha[2*dx+0] = (short)rint((double)(c0 * (float)ORB_RESIZE_COEF_SCALE));
        ialpha[2*dx+1] = (short)rint((double)(c1 * (float)ORB_RESIZE_COEF_SCALE));
    }
}

void orb_resize_precompute_v(const uint8_t *src, int sw, int sh,
                             int dw, int dh,
                             int dy0, int n,
                             int *yofs, short *ibeta)
{
    double scale_y = (double)sh / (double)dh;
    (void)src; (void)sw; (void)dw; /* unused */
    for (int dy = 0; dy < n; ++dy) {
        int dyi = dy0 + dy;
        float fy = (float)(((double)dyi + 0.5) * scale_y - 0.5);
        int sy = (int)floor((double)fy);
        fy -= (float)sy;
        if (sy < 0)     { sy = 0; fy = 0.f; }
        if (sy >= sh-1) { sy = sh-1; fy = 0.f; }
        yofs[dy] = sy;
        float c0 = 1.0f - fy, c1 = fy;
        ibeta[2*dy+0] = (short)rint((double)(c0 * (float)ORB_RESIZE_COEF_SCALE));
        ibeta[2*dy+1] = (short)rint((double)(c1 * (float)ORB_RESIZE_COEF_SCALE));
    }
}

int orb_resize_tile_source_bbox(int tu0, int tv0, int tw, int th,
                                int sw, int sh, int dw, int dh,
                                int *min_y, int *max_y)
{
    /* compute yofs for the tile rows to find source row range */
    int *yofs = (int *)malloc((size_t)th * sizeof(int));
    short *ibeta = (short *)malloc((size_t)th * 2 * sizeof(short));
    if (!yofs || !ibeta) { free(yofs); free(ibeta); *min_y = 0; *max_y = sh-1; return -1; }
    orb_resize_precompute_v(NULL, sw, sh, dw, dh, tv0, th, yofs, ibeta);

    int sy_min = sh, sy_max = -1;
    for (int i = 0; i < th; ++i) {
        int sy = yofs[i];
        if (sy < sy_min) sy_min = sy;
        if (sy > sy_max) sy_max = sy;
        int sy1 = sy + 1;  /* vertical 2-tap: sy and sy+1 */
        if (sy1 < sh) { if (sy1 < sy_min) sy_min = sy1; if (sy1 > sy_max) sy_max = sy1; }
    }
    free(yofs); free(ibeta);

    if (sy_min < 0) sy_min = 0;
    if (sy_max >= sh) sy_max = sh - 1;
    *min_y = sy_min;
    *max_y = sy_max;
    return 0;
}

void orb_resize_kernel(const uint8_t *src, int src_pitch,
                       int origin_x, int origin_y, int region_h,
                       int sw, int sh, int dw, int dh,
                       int tu0, int tv0, int tw, int th,
                       uint8_t *dst, int dst_pitch,
                       const int *xofs, const short *ialpha,
                       const int *yofs, const short *ibeta)
{
    (void)sw; (void)sh; (void)dw; (void)dh; (void)region_h; /* unused in kernel — caller ensures valid */
    for (int dy = 0; dy < th; ++dy) {
        int y0 = yofs[dy], y1 = (y0 + 1 < sh) ? y0 + 1 : sh - 1;
        short b0 = ibeta[2*dy+0], b1 = ibeta[2*dy+1];
        const uint8_t *S0 = src + (size_t)(y0 - origin_y) * src_pitch;
        const uint8_t *S1 = src + (size_t)(y1 - origin_y) * src_pitch;
        uint8_t *D = dst + (size_t)dy * dst_pitch;
        for (int dx = 0; dx < tw; ++dx) {
            int x0 = xofs[dx], x1 = (x0 + 1 < sw) ? x0 + 1 : sw - 1;
            short a0 = ialpha[2*dx+0], a1 = ialpha[2*dx+1];
            int s0 = (int)S0[x0 - origin_x] * a0 + (int)S0[x1 - origin_x] * a1;   /* HResize -> int */
            int s1 = (int)S1[x0 - origin_x] * a0 + (int)S1[x1 - origin_x] * a1;
            int v = ((b0 * (s0 >> 4)) >> 16) + ((b1 * (s1 >> 4)) >> 16) + 2;  /* VResize 8u cast */
            D[dx] = (uint8_t)(v >> 2);
        }
    }
}

void orb_resize_full(const uint8_t *src, int sw, int sh,
                     uint8_t *dst, int dw, int dh)
{
    int   *xofs   = (int*)malloc(sizeof(int)*dw);
    short *ialpha = (short*)malloc(sizeof(short)*dw*2);
    int   *yofs   = (int*)malloc(sizeof(int)*dh);
    short *ibeta  = (short*)malloc(sizeof(short)*dh*2);
    if (!xofs || !ialpha || !yofs || !ibeta) { free(xofs); free(ialpha); free(yofs); free(ibeta); return; }
    orb_resize_precompute_h(NULL, sw, sh, dw, dh, 0, dw, xofs, ialpha);
    orb_resize_precompute_v(NULL, sw, sh, dw, dh, 0, dh, yofs, ibeta);
    orb_resize_kernel(src, sw,
                      0, 0, sh,
                      sw, sh, dw, dh,
                      0, 0, dw, dh,
                      dst, dw,
                      xofs, ialpha, yofs, ibeta);
    free(xofs); free(ialpha); free(yofs); free(ibeta);
}
