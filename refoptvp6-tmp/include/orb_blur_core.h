/*
 * orb_blur_core.h - portable ORB Gaussian-blur kernel (the shared per-tile maths).
 *
 * Extracted verbatim-arithmetic from refactor/src/orb.c::ngd_gaussian_blur
 * (7-tap separable, sigma=2, reflect-101 boundary, float accumulate,
 * (uint8_t)(s+0.5f) round). The float arithmetic is UNCHANGED so the kernel is
 * bit-exact with the refactor C reference.
 *
 * This is the SLAM analogue of ipm_core.c::ipm_remap_kernel: the same function
 * the PC reference (orb_blur_full) and the VP6 tile driver (orb_blur_vp6.c)
 * call, so the per-tile maths the VP6 runs is validated on PC (see
 * test_orb_blur_vp6.c). The kernel is tile-aware: it reads a source region that
 * has been DMA'd into SRAM (origin_x/origin_y = region top-left in frame coords)
 * and uses the absolute frame coords for reflect-101, so no edge padding is
 * needed (EDGE=0) - the +3 bilinear/neighbour region is real frame data.
 */
#ifndef ORB_BLUR_CORE_H
#define ORB_BLUR_CORE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 7-tap Gaussian, sigma=2 (matches refactor orb.c ngd_gauss7 / OpenCV
 * getGaussianKernel(7,2) up to fp). Copied verbatim. */
extern const float orb_blur_gauss7[7];

/*
 * Separable 7-tap Gaussian blur on one tile.
 *
 *   src_data/src_pitch : SRAM source region (DMA'd in by the VP6 driver), or
 *                        the full image for the PC reference path.
 *   origin_x/origin_y  : top-left of the source region in FRAME coords (so the
 *                        kernel can do reflect-101 against the frame, not the
 *                        region). 0,0 for the full-image reference.
 *   region_h           : number of source-region rows present in SRAM (>= the
 *                        rows the vertical pass reads = dst_h + up to 6).
 *   frame_w/frame_h    : full source image size (for reflect-101).
 *   tu0/tv0            : top-left of the OUTPUT tile in frame coords.
 *   dst_w/dst_h        : output tile size.
 *   dst/dst_pitch      : output tile buffer (SRAM for VP6, frame for PC ref).
 *   tmp/tmp_pitch      : scratch for the horizontal->vertical intermediate.
 *                        tmp_pitch >= dst_w; tmp holds region_h rows of width
 *                        dst_w, i.e. needs region_h * tmp_pitch bytes.
 *
 * Produces output identical (bit-exact) to refactor's ngd_gaussian_blur for the
 * covered output region, because the per-pixel arithmetic is the same.
 */
void orb_blur_kernel(const uint8_t *src_data, int src_pitch,
                     int origin_x, int origin_y, int region_h,
                     int frame_w, int frame_h,
                     int tu0, int tv0, int dst_w, int dst_h,
                     uint8_t *dst, int dst_pitch,
                     uint8_t *tmp, int tmp_pitch);

/*
 * Per-tile source bounding box for a 7-tap blur: the output tile [tu0,tv0]-
 * [tu0+tw,tv0+th] needs source cols/rows [tu0-3, tu0+tw+2] x [tv0-3, tv0+th+2].
 * reflect-101 maps any out-of-frame coord back into the frame, so the region is
 * clamped to the frame (no edge padding). Always returns 0 (a blur tile always
 * has a valid source region, unlike IPM where a tile can be all-invalid).
 */
int orb_blur_tile_source_bbox(int tu0, int tv0, int tw, int th,
                              int frame_w, int frame_h,
                              int *min_x, int *min_y, int *max_x, int *max_y);

/*
 * PC reference: full-image blur (origin 0,0, dst = full frame). Allocates its
 * own tmp. This is the oracle the VP6 tile path is compared against; it is
 * bit-exact with refactor's ngd_gaussian_blur by construction (same maths).
 * dst may == src.
 */
void orb_blur_full(const uint8_t *src, int w, int h, int stride, uint8_t *dst);

#ifdef __cplusplus
}
#endif
#endif /* ORB_BLUR_CORE_H */
