/*
 * brief_core.h - portable rotated-BRIEF descriptor kernel (shared PC + VP6).
 *
 * Extracted verbatim-arithmetic from refactor/src/orb.c::ngd_compute_descriptor
 * (and ngd_pix_clamp). The rBRIEF maths (rotate the 256 bit_pattern_31 pairs by
 * the keypoint angle via cosf/sinf + lroundf, sample the blurred patch with edge
 * clamp, (t0<t1) -> 256-bit / 32-byte descriptor) is UNCHANGED so the result is
 * bit-exact with the refactor C reference (and vocabulary-compatible).
 *
 * This is the SLAM analogue of ic_angle_core.c: the same function the PC
 * reference (brief_full) and the VP6 driver (brief_vp6.c) call. The kernel is
 * tile-aware: it reads a source region DMA'd into SRAM (origin_x/origin_y =
 * region top-left in frame coords) and uses absolute frame coords for the
 * edge-clamp (ngd_pix_clamp clamps to the frame edge, not reflect-101), so the
 * source region is the keypoint's +/-19 patch clamped to the frame (EDGE=0).
 */
#ifndef BRIEF_CORE_H
#define BRIEF_CORE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* bit_pattern_31[256][4]: the 256 calibrated BRIEF pairs (x1,y1,x2,y2), verbatim
 * from refactor orb.c (originally ORBextractor.cc). Any change breaks vocabulary
 * compatibility. Max pattern coord is 13 -> rotated offset <= 13*sqrt(2) ~ 18.4
 * -> lroundf 18, so a +/-19 patch covers every sample. */
extern const int brief_bit_pattern_31[256][4];

/* Source margin a BRIEF tile needs: covers the rotated pattern (+/-18) + headroom. */
#define BRIEF_MARGIN 19
#define BRIEF_PATCH_SIZE (2 * BRIEF_MARGIN + 1)   /* 39 */

/*
 * Tile-aware rotated-BRIEF (1:1 maths from refactor ngd_compute_descriptor).
 *   src_data/src_pitch : SRAM source region (DMA'd in by the VP6 driver), or
 *                        the full image for the PC reference path.
 *   origin_x/origin_y  : top-left of the source region in FRAME coords.
 *   frame_w/frame_h    : full image size (for the edge-clamp, same as
 *                        ngd_pix_clamp's w,h).
 *   cx/cy              : keypoint centre in FRAME coords (caller pre-rounds the
 *                        float keypoint with lroundf, matching ngd_compute_descriptor).
 *   angle              : keypoint orientation in DEGREES (kp->angle from IC angle).
 *   desc               : output 32-byte (256-bit) descriptor.
 * Bit-exact with refactor's ngd_compute_descriptor (rotation, sampling, bit
 * packing are identical).
 */
void brief_kernel(const uint8_t *src_data, int src_pitch,
                  int origin_x, int origin_y,
                  int frame_w, int frame_h,
                  int cx, int cy, float angle, uint8_t *desc);

/*
 * Per-keypoint patch bbox: (cx,cy) +/- BRIEF_MARGIN (19), clamped to the frame.
 * The source region DMA'd for one keypoint. Always valid.
 */
void brief_patch_bbox(int cx, int cy, int frame_w, int frame_h,
                      int *min_x, int *min_y, int *max_x, int *max_y);

/*
 * PC reference: rBRIEF on the full image (origin 0,0). Bit-exact with refactor's
 * ngd_compute_descriptor by construction (brief_kernel with origin 0,0 and
 * src=image == ngd_compute_descriptor).
 */
void brief_full(const uint8_t *img, int w, int h, int stride,
                int cx, int cy, float angle, uint8_t *desc);

#ifdef __cplusplus
}
#endif
#endif /* BRIEF_CORE_H */
