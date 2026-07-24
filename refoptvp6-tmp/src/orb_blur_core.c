/*
 * orb_blur_core.c - portable ORB Gaussian-blur kernel (shared PC + VP6 maths).
 *
 * The float arithmetic is copied 1:1 from refactor/src/orb.c::ngd_gaussian_blur
 * so the result is bit-exact with the refactor C reference. The only structural
 * change is that the kernel is tile-aware (origin_x/origin_y + frame_w/h for
 * reflect-101), so the VP6 driver can run it on a per-tile SRAM source region.
 *
 * Build: plain C on PC, or as C++ under the Xtensa cstub (the cstub TIE stubs
 * are C++ and the IVP vector types only match in C++ mode - see CMakeLists.txt,
 * which compiles all .c as CXX, mirroring vp6ref/vp6/CMakeLists.txt).
 */
#include "orb_blur_core.h"

#include <stdlib.h>
#include <string.h>

/* verbatim from refactor/src/orb.c (ngd_gauss7) */
const float orb_blur_gauss7[7] = {
    0.070159f, 0.131072f, 0.190723f, 0.216092f, 0.190723f, 0.131072f, 0.070159f
};

/* reflect-101 with the SAME two-if form as refactor (so extreme negatives
 * double-reflect identically). n >= 1. */
static inline int orb_blur_reflect(int x, int n)
{
    if (x < 0) x = -x;
    if (x >= n) x = 2 * (n - 1) - x;
    return x;
}

void orb_blur_kernel(const uint8_t *src_data, int src_pitch,
                     int origin_x, int origin_y, int region_h,
                     int frame_w, int frame_h,
                     int tu0, int tv0, int dst_w, int dst_h,
                     uint8_t *dst, int dst_pitch,
                     uint8_t *tmp, int tmp_pitch)
{
    /* horizontal pass: for each source-region row, blur the output columns.
     * tmp gets region_h rows of width dst_w (the horizontal blur at every
     * source row the vertical pass might read - including the +3/-3 neighbours
     * the vertical taps reach outside the output rows). */
    for (int ry = 0; ry < region_h; ++ry) {
        const uint8_t *row = src_data + (size_t)ry * src_pitch;
        uint8_t *trow = tmp + (size_t)ry * tmp_pitch;
        for (int x = 0; x < dst_w; ++x) {
            int ax = tu0 + x;
            float s = 0;
            for (int k = -3; k <= 3; ++k) {
                int xx = ax + k;
                xx = orb_blur_reflect(xx, frame_w);
                s += orb_blur_gauss7[k + 3] * (float)row[xx - origin_x];
            }
            trow[x] = (uint8_t)(s + 0.5f);
        }
    }
    /* vertical pass: for each output row, blur over tmp rows (reflect-101 by
     * absolute frame row, indexing tmp by (abs_row - origin_y)). */
    for (int y = 0; y < dst_h; ++y) {
        int ay = tv0 + y;
        uint8_t *drow = dst + (size_t)y * dst_pitch;
        for (int x = 0; x < dst_w; ++x) {
            float s = 0;
            for (int k = -3; k <= 3; ++k) {
                int yy = ay + k;
                yy = orb_blur_reflect(yy, frame_h);
                s += orb_blur_gauss7[k + 3] * (float)tmp[(size_t)(yy - origin_y) * tmp_pitch + x];
            }
            drow[x] = (uint8_t)(s + 0.5f);
        }
    }
}

int orb_blur_tile_source_bbox(int tu0, int tv0, int tw, int th,
                              int frame_w, int frame_h,
                              int *min_x, int *min_y, int *max_x, int *max_y)
{
    int mnx = tu0 - 3;
    int mny = tv0 - 3;
    int mxx = tu0 + tw + 2;   /* (tu0+tw-1) + 3 */
    int mxy = tv0 + th + 2;
    if (mnx < 0) mnx = 0;
    if (mny < 0) mny = 0;
    if (mxx > frame_w - 1) mxx = frame_w - 1;
    if (mxy > frame_h - 1) mxy = frame_h - 1;
    *min_x = mnx; *min_y = mny; *max_x = mxx; *max_y = mxy;
    return 0;
}

void orb_blur_full(const uint8_t *src, int w, int h, int stride, uint8_t *dst)
{
    /* tmp could alias dst if dst==src is allowed; use a separate scratch to
     * match refactor (which mallocs tmp). region_h = h (full image). */
    uint8_t *tmp = (uint8_t *)malloc((size_t)w * h);
    if (!tmp) { if (dst != src) memcpy(dst, src, (size_t)stride * h); return; }
    orb_blur_kernel(src, stride,
                    0, 0, h,
                    w, h,
                    0, 0, w, h,
                    dst, w,
                    tmp, w);
    free(tmp);
}
