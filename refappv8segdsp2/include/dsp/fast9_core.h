/*
 * fast9_core.h - portable FAST-9_16 detect + 3x3 NMS kernel (shared PC + VP6 maths).
 *
 * Extracted verbatim-arithmetic from refactor/src/orb.c::ngd_fast_score /
 * ngd_fast_detect. The integer maths (16-point Bresenham circle, cardinal speed
 * test, 9-contiguous arc score, 3x3 strict-local-max NMS with score recomputation)
 * is UNCHANGED so the result is bit-exact with the refactor C reference.
 *
 * This is the SLAM analogue of orb_blur_core.c: the same functions the PC
 * reference (fast9_full) and the VP6 tile driver (fast9_vp6.c) call, so the
 * per-tile maths the VP6 runs is validated on PC (see test_fast9_vp6.c). The
 * kernel is tile-aware: it reads a source region DMA'd into SRAM
 * (origin_x/origin_y = region top-left in frame coords) and uses absolute frame
 * coords for the out-of-image -> -1 check, so no edge padding is needed (EDGE=0)
 * - the +/-4 neighbour region (ring 3 + NMS 1) is real frame data.
 */
#ifndef FAST9_CORE_H
#define FAST9_CORE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* FAST-9_16 16-point Bresenham circle (radius 3) - verbatim from refactor orb.c. */
extern const int fast9_circle[16][2];

/* Source margin a FAST-9 tile needs: ring(3) + NMS(1) = 4 each side. */
#define FAST9_MARGIN 4

/* Keypoint - layout-identical to refactor's ngd_keypoint
 * (x,y,angle,response,octave,size) so VP6 output is binary-compatible with the
 * ORB pipeline. ngd_fast_detect sets x,y,response=s,octave=0,angle=0,size=0. */
typedef struct {
    float x, y;
    float angle;
    float response;
    int   octave;
    float size;
} fast9_keypoint;

/*
 * Tile-aware FAST-9 score (1:1 maths from refactor ngd_fast_score).
 *   src_data/src_pitch : SRAM source region (DMA'd in by the VP6 driver), or
 *                        the full image for the PC reference path.
 *   origin_x/origin_y  : top-left of the source region in FRAME coords.
 *   frame_w/frame_h    : full image size (for the out-of-image -> -1 check,
 *                        same as ngd_fast_score's w,h).
 *   x0/y0              : candidate pixel in FRAME coords (always in-image).
 *   t                  : FAST threshold.
 * Returns the FAST score (>=0) if a FAST-9 corner, else -1. Bit-exact with
 * refactor's ngd_fast_score (the per-pixel arithmetic is identical).
 */
int fast9_score_kernel(const uint8_t *src_data, int src_pitch,
                       int origin_x, int origin_y,
                       int frame_w, int frame_h,
                       int x0, int y0, int t);

/*
 * Tile-aware FAST-9 detect + 3x3 NMS for the sub-tile [tx0,ty0]-[tx0+tw,ty0+th)
 * (frame coords). 1:1 with refactor ngd_fast_detect:
 *   - for each pixel in the sub-tile, score it;
 *   - if a corner, NMS against the 8 neighbours (skipping neighbours outside the
 *     ROI [iniX,maxX)x[iniY,maxY), recomputing their scores via fast9_score_kernel);
 *   - keep if strict local max (no neighbour score > s);
 *   - emit (x,y,response=s,octave=0,angle=0,size=0) to out[*n_inout] in raster
 *     order, respecting cap (returns at cap, matching refactor's early return).
 * The source region must cover the sub-tile +/- FAST9_MARGIN (4) so every
 * candidate's ring (+/-3) and NMS-neighbour ring (+/-4) are in SRAM. With
 * disjoint sub-tiles partitioning the ROI, each pixel is NMS'd exactly once with
 * all 8 in-ROI neighbours (read from the margin) -> bit-exact with full-ROI
 * ngd_fast_detect. See fast9_vp6.c.
 */
void fast9_detect_kernel(const uint8_t *src_data, int src_pitch,
                         int origin_x, int origin_y,
                         int frame_w, int frame_h,
                         int tx0, int ty0, int tw, int th,
                         int iniX, int maxX, int iniY, int maxY,
                         int t,
                         fast9_keypoint *out, int *n_inout, int cap);

/*
 * Per-sub-tile source bbox: sub-tile +/- FAST9_MARGIN, clamped to the frame.
 * Always valid (a FAST-9 tile always has a source region).
 */
int fast9_tile_source_bbox(int tx0, int ty0, int tw, int th,
                           int frame_w, int frame_h,
                           int *min_x, int *min_y, int *max_x, int *max_y);

/*
 * PC reference: detect over ROI [iniX,maxX)x[iniY,maxY) on the full image
 * (origin 0,0). Bit-exact with refactor's ngd_fast_detect by construction
 * (fast9_score_kernel with origin 0,0 and src=image == ngd_fast_score, and the
 * sub-tile == the whole ROI so NMS sees every in-ROI neighbour).
 */
void fast9_full(const uint8_t *img, int w, int h, int stride,
                int iniX, int maxX, int iniY, int maxY, int t,
                fast9_keypoint *out, int *n_inout, int cap);

#ifdef __cplusplus
}
#endif
#endif /* FAST9_CORE_H */
