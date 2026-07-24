/*
 * fast9_core.c - portable FAST-9_16 detect + NMS kernel (shared PC + VP6 maths).
 *
 * The integer arithmetic is copied 1:1 from refactor/src/orb.c::ngd_fast_score /
 * ngd_fast_detect so the result is bit-exact with the refactor C reference. The
 * only structural change is that the kernel is tile-aware (origin_x/origin_y +
 * frame_w/h), so the VP6 driver can run it on a per-tile SRAM source region.
 *
 * Build: plain C on PC, or as C++ under the Xtensa cstub (see CMakeLists.txt).
 */
#include "fast9_core.h"

/* verbatim from refactor/src/orb.c (ngd_fast_circle) */
const int fast9_circle[16][2] = {
    { 0,-3},{ 1,-3},{ 2,-2},{ 3,-1},{ 3, 0},{ 3, 1},{ 2, 2},{ 1, 3},
    { 0, 3},{-1, 3},{-2, 2},{-3, 1},{-3, 0},{-3,-1},{-2,-2},{-1,-3}
};

/* 1:1 with ngd_fast_score, but indexes a tile source region (origin_x/origin_y)
 * and bounds-checks ring pixels against the full frame (frame_w/h). The centre
 * pixel is never bounds-checked, matching ngd_fast_score (the caller guarantees
 * the candidate is in-image). */
int fast9_score_kernel(const uint8_t *src_data, int src_pitch,
                       int origin_x, int origin_y,
                       int frame_w, int frame_h,
                       int x0, int y0, int t)
{
    int p = src_data[(y0 - origin_y) * src_pitch + (x0 - origin_x)];
    int v[16];
    for (int i = 0; i < 16; ++i) {
        int x = x0 + fast9_circle[i][0];
        int y = y0 + fast9_circle[i][1];
        if (x < 0 || x >= frame_w || y < 0 || y >= frame_h) return -1;
        v[i] = src_data[(y - origin_y) * src_pitch + (x - origin_x)];
    }
    /* speed test: cardinal pixels (indices 0,4,8,12 of the circle) */
    int hi = 0, lo = 0;
    const int card[4] = {0, 4, 8, 12};
    for (int k = 0; k < 4; ++k) {
        if (v[card[k]] > p + t) hi++;
        else if (v[card[k]] < p - t) lo++;
    }
    if (hi < 2 && lo < 2) return -1;   /* a 9-contiguous arc spans >=2 cardinals */

    int best_hi = -1, best_lo = -1;
    for (int s = 0; s < 16; ++s) {
        int allhi = 1, alllo = 1, minhi = 255, maxlo = 0;
        for (int k = 0; k < 9; ++k) {
            int val = v[(s + k) & 15];
            if (val > p + t) { if (val < minhi) minhi = val; } else allhi = 0;
            if (val < p - t) { if (val > maxlo) maxlo = val; } else alllo = 0;
        }
        if (allhi) { int m = minhi - p; if (m > best_hi) best_hi = m; }
        if (alllo) { int m = p - maxlo; if (m > best_lo) best_lo = m; }
    }
    int best = best_hi > best_lo ? best_hi : best_lo;
    if (best < 0) return -1;
    return best - 1;   /* cv::cornerScore<16> returns (max threshold) - 1; uniform shift,
                        * detection set unchanged, NMS/quadtree order preserved. */
}

/* 1:1 with ngd_fast_detect, but iterates a sub-tile [tx0,ty0)-[tx0+tw,ty0+th)
 * and scores via the tile-aware fast9_score_kernel. NMS skips neighbours
 * outside the ROI [iniX,maxX)x[iniY,maxY) (same as refactor) and recomputes
 * neighbour scores from the SRAM source. With the source region = sub-tile +/- 4,
 * every candidate's ring and NMS-neighbour ring are in SRAM. */
void fast9_detect_kernel(const uint8_t *src_data, int src_pitch,
                         int origin_x, int origin_y,
                         int frame_w, int frame_h,
                         int tx0, int ty0, int tw, int th,
                         int iniX, int maxX, int iniY, int maxY,
                         int t,
                         fast9_keypoint *out, int *n_inout, int cap)
{
    for (int y = ty0; y < ty0 + th; ++y) {
        for (int x = tx0; x < tx0 + tw; ++x) {
            int s = fast9_score_kernel(src_data, src_pitch, origin_x, origin_y,
                                       frame_w, frame_h, x, y, t);
            if (s < 0) continue;
            int is_max = 1;
            for (int dy = -1; dy <= 1 && is_max; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dy == 0) continue;
                    int xn = x + dx, yn = y + dy;
                    if (xn < iniX || xn >= maxX || yn < iniY || yn >= maxY) continue;
                    int sn = fast9_score_kernel(src_data, src_pitch, origin_x, origin_y,
                                                frame_w, frame_h, xn, yn, t);
                    if (sn > s) { is_max = 0; break; }
                }
            if (!is_max) continue;
            if (*n_inout >= cap) return;
            fast9_keypoint *kp = &out[(*n_inout)++];
            kp->x = (float)x; kp->y = (float)y;
            kp->response = (float)s; kp->octave = 0; kp->angle = 0.0f; kp->size = 0.0f;
        }
    }
}

int fast9_tile_source_bbox(int tx0, int ty0, int tw, int th,
                           int frame_w, int frame_h,
                           int *min_x, int *min_y, int *max_x, int *max_y)
{
    int mnx = tx0 - FAST9_MARGIN;                  /* tx0 - 4            */
    int mny = ty0 - FAST9_MARGIN;
    int mxx = tx0 + tw + FAST9_MARGIN - 1;         /* last px + 4        */
    int mxy = ty0 + th + FAST9_MARGIN - 1;
    if (mnx < 0) mnx = 0;
    if (mny < 0) mny = 0;
    if (mxx > frame_w - 1) mxx = frame_w - 1;
    if (mxy > frame_h - 1) mxy = frame_h - 1;
    *min_x = mnx; *min_y = mny; *max_x = mxx; *max_y = mxy;
    return 0;
}

void fast9_full(const uint8_t *img, int w, int h, int stride,
                int iniX, int maxX, int iniY, int maxY, int t,
                fast9_keypoint *out, int *n_inout, int cap)
{
    /* sub-tile == whole ROI, source == full image (origin 0,0): the kernel then
     * iterates [iniX,maxX)x[iniY,maxY) and reads img[] directly, exactly
     * ngd_fast_detect. */
    fast9_detect_kernel(img, stride, 0, 0, w, h,
                        iniX, iniY, maxX - iniX, maxY - iniY,
                        iniX, maxX, iniY, maxY, t, out, n_inout, cap);
}
