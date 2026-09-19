/*
 * fast9_scoremap.h - single-scan FAST-9_16 score map + cached-NMS collection.
 *
 * Algorithmic restructure of the per-cell fast9_full() calls in
 * refappv8segdsp's dsp_orb.c, ported from try-slam31-RI/xsTileLib/slam/src/
 * fast9NMS.c (shared-core min/max arc scoring, detect-while-scoring score
 * map, O(candidates) NMS). Bit-exact with fast9_detect_kernel; the
 * equivalence argument is in fast9_scoremap.c.
 *
 * Build: plain C on PC, or as C++ under the Xtensa cstub (LANGUAGE CXX).
 */
#ifndef FAST9_SCOREMAP_H
#define FAST9_SCOREMAP_H

#include <stdint.h>

#include "fast9_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* score-map sentinel: border pixels (ring would leave the frame, where
 * fast9_score_kernel returns -1) and non-corners at the build threshold. */
#define FAST9_MAP_NONE 0xFF

/*
 * Build the threshold-independent FAST-9 score map for a whole image.
 *
 *   map[y*map_stride + x] = S-1 for pixels whose ring is fully in-frame,
 *                           where S = max over the 16 nine-pixel arcs of the
 *                           arc-min contrast (bright or dark) - the maximum
 *                           threshold at which the pixel is a corner, minus 1,
 *                           matching the value fast9_score_kernel returns.
 *                         = FAST9_MAP_NONE when the pixel is not a corner at
 *                           t_floor (S <= t_floor) or sits within 3 px of the
 *                           frame border (fast9_score_kernel returns -1 there).
 *
 * A pixel is a FAST corner at threshold t (t >= t_floor) iff
 *   map != FAST9_MAP_NONE && map >= t,
 * and its corner score equals map - exactly the (s >= 0, s) pair the old
 * per-pixel kernel produced at threshold t (see fast9_scoremap.c for why the
 * score does not depend on t).
 *
 * t_floor must not exceed any threshold later passed to
 * fast9_collect_cell (refappv8segdsp passes min(iniThFAST, minThFAST)).
 */
void fast9_build_scoremap(const uint8_t *img, int w, int h, int stride,
                          int t_floor,
                          uint8_t *map, int map_stride);

/*
 * Collect + 3x3-NMS keypoints inside one grid-cell ROI [iniX,maxX)x[iniY,maxY)
 * from a map built by fast9_build_scoremap. Drop-in replacement for
 * fast9_full() on the same ROI: identical raster order, identical responses,
 * identical cap semantics (append via *n_inout, return silently at cap).
 * Neighbours outside the ROI are skipped, exactly like fast9_detect_kernel.
 */
void fast9_collect_cell(const uint8_t *map, int map_stride,
                        int iniX, int maxX, int iniY, int maxY, int t,
                        fast9_keypoint *out, int *n_inout, int cap);

#ifdef __cplusplus
}
#endif
#endif /* FAST9_SCOREMAP_H */
