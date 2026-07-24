/*
 * brief_core.c - portable rotated-BRIEF descriptor kernel (shared PC + VP6).
 *
 * The arithmetic is copied 1:1 from refactor/src/orb.c::ngd_compute_descriptor /
 * ngd_pix_clamp (and the bit_pattern_31 table verbatim) so the result is bit-exact
 * with the refactor C reference. The only structural change is that the kernel is
 * tile-aware (origin_x/origin_y + frame_w/h), so the VP6 driver can run it on a
 * per-keypoint SRAM source patch.
 *
 * Build: plain C on PC, or as C++ under the Xtensa cstub (see CMakeLists.txt).
 */
#include "brief_core.h"

#include <math.h>

#ifndef BRIEF_PI
#define BRIEF_PI 3.14159265358979323846
#endif

/* verbatim from refactor/src/orb.c (ngd_bit_pattern_31). 256 pairs (x1,y1,x2,y2). */
const int brief_bit_pattern_31[256][4] = {
    {8,-3, 9,5},{4,2, 7,-12},{-11,9, -8,2},{7,-12, 12,-13},{2,-13, 2,12},
    {1,-7, 1,6},{-2,-10, -2,-4},{-13,-13, -11,-8},{-13,-3, -12,-9},{10,4, 11,9},
    {-13,-8, -8,-9},{-11,7, -9,12},{7,7, 12,6},{-4,-5, -3,0},{-13,2, -12,-3},{-9,0, -7,5},
    {12,-6, 12,-1},{-3,6, -2,12},{-6,-13, -4,-8},{11,-13, 12,-8},{4,7, 5,1},{5,-3, 10,-3},
    {3,-7, 6,12},{-8,-7, -6,-2},{-2,11, -1,-10},{-13,12, -8,10},{-7,3, -5,-3},{-4,2, -3,7},
    {-10,-12, -6,11},{5,-12, 6,-7},{5,-6, 7,-1},{1,0, 4,-5},{9,11, 11,-13},{4,7, 4,12},
    {2,-1, 4,4},{-4,-12, -2,7},{-8,-5, -7,-10},{4,11, 9,12},{0,-8, 1,-13},{-13,-2, -8,2},
    {-3,-2, -2,3},{-6,9, -4,-9},{8,12, 10,7},{0,9, 1,3},{7,-5, 11,-10},{-13,-6, -11,0},
    {10,7, 12,1},{-6,-3, -6,12},{10,-9, 12,-4},{-13,8, -8,-12},{-13,0, -8,-4},{3,3, 7,8},
    {5,7, 10,-7},{-1,7, 1,-12},{3,-10, 5,6},{2,-4, 3,-10},{-13,0, -13,5},{-13,-7, -12,12},
    {-13,3, -11,8},{-7,12, -4,7},{6,-10, 12,8},{-9,-1, -7,-6},{-2,-5, 0,12},{-12,5, -7,5},
    {3,-10, 8,-13},{-7,-7, -4,5},{-3,-2, -1,-7},{2,9, 5,-11},{-11,-13, -5,-13},{-1,6, 0,-1},
    {5,-3, 5,2},{-4,-13, -4,12},{-9,-6, -9,6},{-12,-10, -8,-4},{10,2, 12,-3},{7,12, 12,12},
    {-7,-13, -6,5},{-4,9, -3,4},{7,-1, 12,2},{-7,6, -5,1},{-13,11, -12,5},{-3,7, -2,-6},
    {7,-8, 12,-7},{-13,-7, -11,-12},{1,-3, 12,12},{2,-6, 3,0},{-4,3, -2,-13},{-1,-13, 1,9},
    {7,1, 8,-6},{1,-1, 3,12},{9,1, 12,6},{-1,-9, -1,3},{-13,-13, -10,5},{7,7, 10,12},
    {12,-5, 12,9},{6,3, 7,11},{5,-13, 6,10},{2,-12, 2,3},{3,8, 4,-6},{2,6, 12,-13},
    {9,-12, 10,3},{-8,4, -7,9},{-11,12, -4,-6},{1,12, 2,-8},{6,-9, 7,-4},{2,3, 3,-2},
    {6,3, 11,0},{3,-3, 8,-8},{7,8, 9,3},{-11,-5, -6,-4},{-10,11, -5,10},{-5,-8, -3,12},
    {-10,5, -9,0},{8,-1, 12,-6},{4,-6, 6,-11},{-10,12, -8,7},{4,-2, 6,7},{-2,0, -2,12},
    {-5,-8, -5,2},{7,-6, 10,12},{-9,-13, -8,-8},{-5,-13, -5,-2},{8,-8, 9,-13},{-9,-11, -9,0},
    {1,-8, 1,-2},{7,-4, 9,1},{-2,1, -1,-4},{11,-6, 12,-11},{-12,-9, -6,4},{3,7, 7,12},
    {5,5, 10,8},{0,-4, 2,8},{-9,12, -5,-13},{0,7, 2,12},{-1,2, 1,7},{5,11, 7,-9},{3,5, 6,-8},
    {-13,-4, -8,9},{-5,9, -3,-3},{-4,-7, -3,-12},{6,5, 8,0},{-7,6, -6,12},{-13,6, -5,-2},
    {1,-10, 3,10},{4,1, 8,-4},{-2,-2, 2,-13},{2,-12, 12,12},{-2,-13, 0,-6},{4,1, 9,3},
    {-6,-10, -3,-5},{-3,-13, -1,1},{7,5, 12,-11},{4,-2, 5,-7},{-13,9, -9,-5},{7,1, 8,6},
    {7,-8, 7,6},{-7,-4, -7,1},{-8,11, -7,-8},{-13,6, -12,-8},{2,4, 3,9},{10,-5, 12,3},
    {-6,-5, -6,7},{8,-3, 9,-8},{2,-12, 2,8},{-11,-2, -10,3},{-12,-13, -7,-9},{-11,0, -10,-5},
    {5,-3, 11,8},{-2,-13, -1,12},{-1,-8, 0,9},{-13,-11, -12,-5},{-10,-2, -10,11},{-3,9, -2,-13},
    {2,-3, 3,2},{-9,-13, -4,0},{-4,6, -3,-10},{-4,12, -2,-7},{-6,-11, -4,9},{6,-3, 6,11},
    {-13,11, -5,5},{11,11, 12,6},{7,-5, 12,-2},{-1,12, 0,7},{-4,-8, -3,-2},{-7,1, -6,7},
    {-13,-12, -8,-13},{-7,-2, -6,-8},{-8,5, -6,-9},{-5,-1, -4,5},{-13,7, -8,10},{1,5, 5,-13},
    {1,0, 10,-13},{9,12, 10,-1},{5,-8, 10,-9},{-1,11, 1,-13},{-9,-3, -6,2},{-1,-10, 1,12},
    {-13,1, -8,-10},{8,-11, 10,-6},{2,-13, 3,-6},{7,-13, 12,-9},{-10,-10, -5,-7},{-10,-8, -8,-13},
    {4,-6, 8,5},{3,12, 8,-13},{-4,2, -3,-3},{5,-13, 10,-12},{4,-13, 5,-1},{-9,9, -4,3},
    {0,3, 3,-9},{-12,1, -6,1},{3,2, 4,-8},{-10,-10, -10,9},{8,-13, 12,12},{-8,-12, -6,-5},
    {2,2, 3,7},{10,6, 11,-8},{6,8, 8,-12},{-7,10, -6,5},{-3,-9, -3,9},{-1,-13, -1,5},
    {-3,-7, -3,4},{-8,-2, -8,3},{4,2, 12,12},{2,-5, 3,11},{6,-9, 11,-13},{3,-1, 7,12},
    {11,-1, 12,4},{-3,0, -3,6},{4,-11, 4,12},{2,-4, 2,1},{-10,-6, -8,1},{-13,7, -11,1},
    {-13,12, -11,-13},{6,0, 11,-13},{0,-1, 1,4},{-13,3, -9,-2},{-9,8, -6,-3},{-13,-6, -8,-2},
    {5,-9, 8,10},{2,7, 3,-9},{-1,-6, -1,-1},{9,5, 11,-2},{11,-3, 12,-8},{3,0, 3,5},
    {-1,4, 0,10},{3,-6, 4,5},{-13,0, -10,5},{5,8, 12,11},{8,9, 9,-6},{7,-4, 8,-12},
    {-10,4, -10,9},{7,3, 12,4},{9,-7, 10,-2},{7,0, 12,-2},{-1,-6, 0,-11}
};

/* 1:1 with ngd_pix_clamp, but indexes a tile source region (origin_x/origin_y)
 * and clamps against the full frame (frame_w/h). The clamped coord is always in
 * the source region (the patch +/-19 clamped to the frame). */
static inline uint8_t brief_pix_clamp(const uint8_t *src, int pitch,
                                      int origin_x, int origin_y,
                                      int frame_w, int frame_h,
                                      int x, int y)
{
    if (x < 0) x = 0; else if (x >= frame_w) x = frame_w - 1;
    if (y < 0) y = 0; else if (y >= frame_h) y = frame_h - 1;
    return src[(y - origin_y) * pitch + (x - origin_x)];
}

/* 1:1 with ngd_compute_descriptor, but samples via the tile-aware brief_pix_clamp. */
void brief_kernel(const uint8_t *src_data, int src_pitch,
                  int origin_x, int origin_y,
                  int frame_w, int frame_h,
                  int cx, int cy, float angle, uint8_t *desc)
{
    float ar = angle * (float)(BRIEF_PI / 180.0);
    float a = cosf(ar), b = sinf(ar);

    for (int i = 0; i < 32; ++i) {
        int val = 0;
        for (int bit = 0; bit < 8; ++bit) {
            int pair = i * 8 + bit;
            int p1x = brief_bit_pattern_31[pair][0], p1y = brief_bit_pattern_31[pair][1];
            int p2x = brief_bit_pattern_31[pair][2], p2y = brief_bit_pattern_31[pair][3];
            /* GET_VALUE: row_off=round(px*b+py*a), col_off=round(px*a-py*b) */
            int r1 = (int)lroundf(p1x * b + p1y * a), c1 = (int)lroundf(p1x * a - p1y * b);
            int r2 = (int)lroundf(p2x * b + p2y * a), c2 = (int)lroundf(p2x * a - p2y * b);
            int t0 = brief_pix_clamp(src_data, src_pitch, origin_x, origin_y,
                                     frame_w, frame_h, cx + c1, cy + r1);
            int t1 = brief_pix_clamp(src_data, src_pitch, origin_x, origin_y,
                                     frame_w, frame_h, cx + c2, cy + r2);
            val |= (t0 < t1) << bit;
        }
        desc[i] = (uint8_t)val;
    }
}

void brief_patch_bbox(int cx, int cy, int frame_w, int frame_h,
                      int *min_x, int *min_y, int *max_x, int *max_y)
{
    int mnx = cx - BRIEF_MARGIN;
    int mny = cy - BRIEF_MARGIN;
    int mxx = cx + BRIEF_MARGIN;   /* cx + 19 */
    int mxy = cy + BRIEF_MARGIN;
    if (mnx < 0) mnx = 0;
    if (mny < 0) mny = 0;
    if (mxx > frame_w - 1) mxx = frame_w - 1;
    if (mxy > frame_h - 1) mxy = frame_h - 1;
    *min_x = mnx; *min_y = mny; *max_x = mxx; *max_y = mxy;
}

void brief_full(const uint8_t *img, int w, int h, int stride,
                int cx, int cy, float angle, uint8_t *desc)
{
    /* origin 0,0, source == full image: the kernel reads img[] directly, exactly
     * ngd_compute_descriptor. */
    brief_kernel(img, stride, 0, 0, w, h, cx, cy, angle, desc);
}
