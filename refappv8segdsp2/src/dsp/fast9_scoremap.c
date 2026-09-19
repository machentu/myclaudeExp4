/*
 * fast9_scoremap.c - single-scan FAST-9_16 score map + cached-NMS collection.
 *
 * Bit-exactness with fast9_detect_kernel (refappv8segdsp / refoptvp6v2)
 * ---------------------------------------------------------------
 *
 * 1. Threshold independence. fast9_score_kernel(x,y,t) evaluates, for every
 *    rotation s, the 9-pixel arc minimum minhi-p (bright) / p-maxlo (dark)
 *    over the arcs whose 9 ring pixels all exceed p+/-t, and returns the max
 *    over qualifying arcs, minus 1. If ANY arc qualifies at t then the
 *    globally best arc also qualifies (its min is >= the qualifying arc's
 *    min > t), so the returned value is
 *        S-1,   S = max over all 16 arcs of the arc-min contrast,
 *    whenever the pixel is a corner at t - i.e. the score S-1 does not depend
 *    on t, and "corner at t" <=> S > t <=> S-1 >= t. (Integer contrasts make
 *    this exact.) Building the map once with t_floor = min threshold and
 *    collecting with `map >= t` therefore reproduces both the detection set
 *    and the responses of per-cell scans at iniThFAST and minThFAST, and it
 *    lets both thresholds share one pass over the pixels.
 *
 * 2. NMS. fast9_detect_kernel suppresses a corner when a neighbour INSIDE the
 *    ROI has a strictly larger kernel score at the same t (non-corner
 *    neighbours return -1 and never suppress). In map terms a non-corner
 *    neighbour at t is either FAST9_MAP_NONE (skip, like -1) or holds S-1 < t
 *    while the centre holds S-1 >= t, so it can never exceed the centre - the
 *    same outcome. Comparisons between two corners at t compare S-1 == S-1.
 *
 * 3. Borders. The kernel returns -1 when any ring pixel leaves the frame, so
 *    pixels within 3 px of the border are never corners; the map stores
 *    FAST9_MAP_NONE there (and the SLAM ROI is inset 16 px anyway).
 *
 * Accelerations ported from try-slam31-RI/xsTileLib/slam/src/fast9NMS.c:
 *   - detect-while-scoring score map, NMS reads the cache instead of
 *     re-running the kernel 9x per corner;
 *   - shared-core min/max tree: the 16 arc scores share 8 8-wide cores
 *     (arcs 2k and 2k+1 differ only in their endpoints), so ~80 min/max ops
 *     replace the 16-rotation loop (fast9NMS.c:173-214). Branch-free and
 *     LUT-free, so it maps directly to SIMD lanes;
 *   - 7 row pointers + interior-only loop: no per-ring bounds checks;
 *   - cardinal 4-point early rejection kept from the original kernel.
 *
 * Build: plain C on PC, or as C++ under the Xtensa cstub (LANGUAGE CXX).
 */
#include "fast9_scoremap.h"

#include <string.h>

#define FS_MIN(a, b) (((a) < (b)) ? (a) : (b))
#define FS_MAX(a, b) (((a) > (b)) ? (a) : (b))

/*
 * max over the 16 nine-pixel arcs of the arc minimum, for one polarity
 * (d[i] = ring_i - centre for bright corners, centre - ring_i for dark).
 * Arcs 2k and 2k+1 share the 8-pixel core starting at 2k+1:
 *   min(arc_{2k})   = min(core_{2k+1}, d[2k])
 *   min(arc_{2k+1}) = min(core_{2k+1}, d[2k+9])
 *   max of both     = min(core_{2k+1}, max(d[2k], d[2k+9]))
 * so 8 cores (built as a 2/4/8-wide min tree over the ring duplicated once)
 * plus 8 (MIN,MAX) endpoint pairs and a 7-level max tree cover all 16 arcs.
 */
static int fast9_tree_score(const int d[16])
{
    int d2[24];
    for (int i = 0; i < 24; ++i) d2[i] = d[i & 15];

    int m2[22];
    for (int j = 0; j < 22; ++j) m2[j] = FS_MIN(d2[j], d2[j + 1]);
    int m4[20];
    for (int j = 0; j < 20; ++j) m4[j] = FS_MIN(m2[j], m2[j + 2]);
    int core[16];
    for (int j = 0; j < 16; ++j) core[j] = FS_MIN(m4[j], m4[j + 4]);

    int S = FS_MIN(core[1], FS_MAX(d2[0], d2[9]));
    for (int s = 2; s < 16; s += 2) {
        int pr = FS_MIN(core[s + 1], FS_MAX(d2[s], d2[s + 9]));
        if (pr > S) S = pr;
    }
    return S;
}

void fast9_build_scoremap(const uint8_t *img, int w, int h, int stride,
                          int t_floor,
                          uint8_t *map, int map_stride)
{
    memset(map, FAST9_MAP_NONE, (size_t)map_stride * (size_t)h);
    if (w < 7 || h < 7) return;   /* no pixel has a full in-frame ring */

    for (int y = 3; y <= h - 4; ++y) {
        const uint8_t *rm3 = img + (size_t)(y - 3) * stride;
        const uint8_t *rm2 = img + (size_t)(y - 2) * stride;
        const uint8_t *rm1 = img + (size_t)(y - 1) * stride;
        const uint8_t *r0  = img + (size_t)(y)     * stride;
        const uint8_t *rp1 = img + (size_t)(y + 1) * stride;
        const uint8_t *rp2 = img + (size_t)(y + 2) * stride;
        const uint8_t *rp3 = img + (size_t)(y + 3) * stride;
        uint8_t *mrow = map + (size_t)y * map_stride;

        for (int x = 3; x <= w - 4; ++x) {
            int p = r0[x];

            /* cardinal 4-point rejection (indices 0,4,8,12) - same test as
             * fast9_score_kernel at t_floor */
            int hi = 0, lo = 0;
            if (rm3[x] > p + t_floor) ++hi; else if (rm3[x] < p - t_floor) ++lo;
            if (r0[x + 3] > p + t_floor) ++hi; else if (r0[x + 3] < p - t_floor) ++lo;
            if (rp3[x] > p + t_floor) ++hi; else if (rp3[x] < p - t_floor) ++lo;
            if (r0[x - 3] > p + t_floor) ++hi; else if (r0[x - 3] < p - t_floor) ++lo;
            if (hi < 2 && lo < 2) continue;

            /* full 16-pixel ring in fast9_circle order */
            int d[16];
            d[0]  = (int)rm3[x]     - p;
            d[1]  = (int)rm3[x + 1] - p;
            d[2]  = (int)rm2[x + 2] - p;
            d[3]  = (int)rm1[x + 3] - p;
            d[4]  = (int)r0[x + 3]  - p;
            d[5]  = (int)rp1[x + 3] - p;
            d[6]  = (int)rp2[x + 2] - p;
            d[7]  = (int)rp3[x + 1] - p;
            d[8]  = (int)rp3[x]     - p;
            d[9]  = (int)rp3[x - 1] - p;
            d[10] = (int)rp2[x - 2] - p;
            d[11] = (int)rp1[x - 3] - p;
            d[12] = (int)r0[x - 3]  - p;
            d[13] = (int)rm1[x - 3] - p;
            d[14] = (int)rm2[x - 2] - p;
            d[15] = (int)rm3[x - 1] - p;

            int S = fast9_tree_score(d);           /* bright polarity */
            int e[16];
            for (int i = 0; i < 16; ++i) e[i] = -d[i];
            int Sd = fast9_tree_score(e);          /* dark polarity */
            if (Sd > S) S = Sd;

            mrow[x] = (S > t_floor) ? (uint8_t)(S - 1) : FAST9_MAP_NONE;
        }
    }
}

void fast9_collect_cell(const uint8_t *map, int map_stride,
                        int iniX, int maxX, int iniY, int maxY, int t,
                        fast9_keypoint *out, int *n_inout, int cap)
{
    for (int y = iniY; y < maxY; ++y) {
        const uint8_t *mrow = map + (size_t)y * map_stride;
        for (int x = iniX; x < maxX; ++x) {
            int s = mrow[x];
            if (s == FAST9_MAP_NONE || s < t) continue;

            int is_max = 1;
            for (int dy = -1; dy <= 1 && is_max; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dy == 0) continue;
                    int xn = x + dx, yn = y + dy;
                    if (xn < iniX || xn >= maxX || yn < iniY || yn >= maxY) continue;
                    int sn = map[(size_t)yn * map_stride + xn];
                    if (sn != FAST9_MAP_NONE && sn > s) { is_max = 0; break; }
                }
            if (!is_max) continue;

            if (*n_inout >= cap) return;
            fast9_keypoint *kp = &out[(*n_inout)++];
            kp->x = (float)x; kp->y = (float)y;
            kp->response = (float)s; kp->octave = 0; kp->angle = 0.0f; kp->size = 0.0f;
        }
    }
}
