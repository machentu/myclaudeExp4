/*
 * orb_blur_core.c - portable ORB Gaussian-blur kernel (shared PC + VP6 maths).
 *
 * The fixed-point arithmetic is copied 1:1 from refactor/src/orb.c::ngd_gaussian_blur
 * (OpenCV GaussianBlurFixedPoint path, ufixedpoint16 kernel, shift8) so the result
 * is bit-exact with both cv::GaussianBlur and the refactor C reference. The only
 * structural change is that the kernel is tile-aware (origin_x/origin_y + frame_w/h
 * for reflect-101), so the VP6 driver can run it on a per-tile SRAM source region.
 *
 * Build: plain C on PC, or as C++ under the Xtensa cstub (the cstub TIE stubs
 * are C++ and the IVP vector types only match in C++ mode - see CMakeLists.txt,
 * which compiles all .c as CXX, mirroring vp6ref/vp6/CMakeLists.txt).
 */
#include "orb_blur_core.h"

#include <stdlib.h>
#include <string.h>

/* verbatim from refactor/src/orb.c (ngd_gauss7): ufixedpoint16 shift8, sum=256.
 * Extracted from OpenCV 4.12 getGaussianKernelFixedPoint_ED(7,2,8). */
const uint16_t orb_blur_gauss7[7] = { 18, 34, 48, 56, 48, 34, 18 };

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
                     uint16_t *tmp, int tmp_pitch)
{
    /* horizontal pass: for each source-region row, blur the output columns.
     * tmp gets region_h rows of width dst_w (the horizontal blur at every
     * source row the vertical pass might read - including the +3/-3 neighbours
     * the vertical taps reach outside the output rows).
     * Output: uint16_t raw sum (<= 255*256 = 65280), no rounding. */
    for (int ry = 0; ry < region_h; ++ry) {
        const uint8_t *row = src_data + (size_t)ry * src_pitch;
        uint16_t *trow = tmp + (size_t)ry * tmp_pitch;
        for (int x = 0; x < dst_w; ++x) {
            int ax = tu0 + x;
            uint32_t s = 0;
            for (int k = 0; k < 7; ++k)
                s += (uint32_t)orb_blur_gauss7[k] * (uint32_t)row[orb_blur_reflect(ax - 3 + k, frame_w) - origin_x];
            trow[x] = (uint16_t)s;   /* <= 65280, fits uint16 */
        }
    }
    /* vertical pass: for each output row, blur over tmp rows (reflect-101 by
     * absolute frame row, indexing tmp by (abs_row - origin_y)).
     * Output: sat_u8((sum_k GK[k]*tmp[row] + 32768) >> 16). */
    for (int y = 0; y < dst_h; ++y) {
        int ay = tv0 + y;
        uint8_t *drow = dst + (size_t)y * dst_pitch;
        for (int x = 0; x < dst_w; ++x) {
            uint32_t s = 0;
            for (int k = 0; k < 7; ++k) {
                int yy = orb_blur_reflect(ay - 3 + k, frame_h);
                s += (uint32_t)orb_blur_gauss7[k] * (uint32_t)tmp[(size_t)(yy - origin_y) * tmp_pitch + x];
            }
            uint32_t v = (s + 32768u) >> 16;
            drow[x] = v > 255u ? 255u : (uint8_t)v;
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
    /* tmp holds uint16_t raw horizontal sums, matching refactor ngd_gaussian_blur. */
    uint16_t *tmp = (uint16_t *)malloc((size_t)w * h * sizeof(uint16_t));
    if (!tmp) { if (dst != src) memcpy(dst, src, (size_t)stride * h); return; }
    orb_blur_kernel(src, stride,
                    0, 0, h,
                    w, h,
                    0, 0, w, h,
                    dst, w,
                    tmp, w);
    free(tmp);
}

/* horizontal-pass border pixel (window crosses the frame edge): the original
 * direct 7-tap + reflect-101 evaluation, verbatim (origin_x = 0). */
static void orb_blur_hborder(const uint8_t *row, int w, int x, uint16_t *trow)
{
    uint32_t s = 0;
    for (int k = 0; k < 7; ++k)
        s += (uint32_t)orb_blur_gauss7[k] * (uint32_t)row[orb_blur_reflect(x - 3 + k, w)];
    trow[x] = (uint16_t)s;
}

void orb_blur_full_opt(const uint8_t *src, int w, int h, int stride, uint8_t *dst)
{
    const uint16_t *g = orb_blur_gauss7;
    uint16_t *tmp = (uint16_t *)malloc((size_t)w * h * sizeof(uint16_t));
    if (!tmp) { if (dst != src) memcpy(dst, src, (size_t)stride * h); return; }

    /* ---- horizontal pass. Interior: symmetric-pair form
     *   S = g0*(v[x-3]+v[x+3]) + g1*(v[x-2]+v[x+2]) + g2*(v[x-1]+v[x+1]) + g3*v[x]
     * (g symmetric) - same exact integer sum as the per-tap loop (addition is
     * associative), 4 multiplies instead of 7, no reflect branch. Border cols
     * (window crosses the frame edge) keep the direct reflect-101 path. ---- */
    for (int y = 0; y < h; ++y) {
        const uint8_t *row = src + (size_t)y * stride;
        uint16_t *trow = tmp + (size_t)y * w;

        for (int x = 0; x < 3 && x < w; ++x) orb_blur_hborder(row, w, x, trow);
        for (int x = (w - 3 > 3) ? w - 3 : 3; x < w; ++x) orb_blur_hborder(row, w, x, trow);

        for (int x = 3; x <= w - 4; ++x) {
            uint32_t s = (uint32_t)g[0] * (uint32_t)(row[x - 3] + row[x + 3])
                       + (uint32_t)g[1] * (uint32_t)(row[x - 2] + row[x + 2])
                       + (uint32_t)g[2] * (uint32_t)(row[x - 1] + row[x + 1])
                       + (uint32_t)g[3] * (uint32_t)row[x];
            trow[x] = (uint16_t)s;
        }
    }

    /* ---- vertical pass. Border rows: direct reflect-101 (unchanged). Interior
     * rows: 7 rolling row pointers into tmp, symmetric-pair form, no reflect
     * and no per-tap index arithmetic. ---- */
    for (int y = 0; y < h; ++y) {
        if (!(y < 3 || y >= h - 3)) continue;
        uint8_t *drow = dst + (size_t)y * w;
        for (int x = 0; x < w; ++x) {
            uint32_t s = 0;
            for (int k = 0; k < 7; ++k)
                s += (uint32_t)g[k] * (uint32_t)tmp[(size_t)orb_blur_reflect(y - 3 + k, h) * w + x];
            uint32_t v = (s + 32768u) >> 16;
            drow[x] = v > 255u ? 255u : (uint8_t)v;
        }
    }
    for (int y = 3; y <= h - 4; ++y) {
        const uint16_t *r0 = tmp + (size_t)(y - 3) * w;
        const uint16_t *r1 = r0 + w;
        const uint16_t *r2 = r1 + w;
        const uint16_t *r3 = r2 + w;
        const uint16_t *r4 = r3 + w;
        const uint16_t *r5 = r4 + w;
        const uint16_t *r6 = r5 + w;
        uint8_t *drow = dst + (size_t)y * w;
        for (int x = 0; x < w; ++x) {
            uint32_t s = (uint32_t)g[0] * (uint32_t)(r0[x] + r6[x])
                       + (uint32_t)g[1] * (uint32_t)(r1[x] + r5[x])
                       + (uint32_t)g[2] * (uint32_t)(r2[x] + r4[x])
                       + (uint32_t)g[3] * (uint32_t)r3[x];
            uint32_t v = (s + 32768u) >> 16;
            drow[x] = v > 255u ? 255u : (uint8_t)v;
        }
    }

    free(tmp);
}
