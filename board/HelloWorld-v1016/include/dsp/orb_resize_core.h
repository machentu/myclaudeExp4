/*
 * orb_resize_core.h - portable bilinear resize kernel (shared PC + VP6 maths).
 *
 * Extracted verbatim-arithmetic from refactor/src/orb.c::ngd_resize_bilinear
 * (bilinear interpolation, INTER_RESIZE_COEF_BITS=11, scale_x=sw/dw in double).
 * Bit-exact with cv::resize INTER_LINEAR 8u (OpenCV 4.12 linear_tab[CV_8U] path).
 *
 * The kernel is tile-aware: it takes a source region that has been DMA'd into
 * SRAM and computes the bilinear downscale for a rectangular output tile.
 */
#ifndef ORB_RESIZE_CORE_H
#define ORB_RESIZE_CORE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ORB_RESIZE_COEF_SCALE 2048  /* 1 << INTER_RESIZE_COEF_BITS (BITS=11) */

/*
 * Bilinear resize on one output tile.
 *
 *   src/src_pitch     : SRAM source region (DMA'd in), or full image for PC ref.
 *   origin_x/origin_y : top-left of source region in frame coords (0,0 for full).
 *   region_h          : number of source rows present in SRAM.
 *   sw/sh             : source image dimensions (full frame).
 *   dw/dh             : destination image dimensions (full frame).
 *   tu0/tv0           : top-left of the OUTPUT tile in dest coords.
 *   tw/th             : output tile size.
 *   dst/dst_pitch     : output tile buffer.
 *   xofs/ialpha       : precomputed horizontal offset/coefficient tables for
 *                       columns [tu0, tu0+tw); each has tw entries. Caller owns.
 *   yofs/ibeta        : precomputed vertical offset/coefficient tables for
 *                       rows [tv0, tv0+th); each has th*2 entries. Caller owns.
 *
 * ialpha layout: ialpha[2*dx+0]=a0, ialpha[2*dx+1]=a1.
 * ibeta layout:  ibeta[2*dy+0]=b0,  ibeta[2*dy+1]=b1.
 */
void orb_resize_kernel(const uint8_t *src, int src_pitch,
                       int origin_x, int origin_y, int region_h,
                       int sw, int sh, int dw, int dh,
                       int tu0, int tv0, int tw, int th,
                       uint8_t *dst, int dst_pitch,
                       const int *xofs, const short *ialpha,
                       const int *yofs, const short *ibeta);

/*
 * Precompute horizontal tables for destination columns [dx0, dx0+n).
 *   xofs:  n ints (source column index)
 *   ialpha: 2*n shorts (coefficients for the 2 horizontal taps)
 * Caller allocates.
 */
void orb_resize_precompute_h(const uint8_t *src, int sw, int sh,
                             int dw, int dh,
                             int dx0, int n,
                             int *xofs, short *ialpha);

/*
 * Precompute vertical tables for destination rows [dy0, dy0+n).
 *   yofs:  n ints (source row index)
 *   ibeta: 2*n shorts (coefficients for the 2 vertical taps)
 * Caller allocates.
 */
void orb_resize_precompute_v(const uint8_t *src, int sw, int sh,
                             int dw, int dh,
                             int dy0, int n,
                             int *yofs, short *ibeta);

/*
 * Per-tile source bounding box: the output tile [tu0,tv0]-[tu0+tw,tv0+th]
 * needs source rows [sy0, sy1] where sy0/sy1 are the min/max of yofs[dy]
 * for dy in [tv0, tv0+th-1], clamped to [0, sh-1]. Source columns [0, sw-1]
 * are needed (the xofs table covers all columns). Returns the row range.
 */
int orb_resize_tile_source_bbox(int tu0, int tv0, int tw, int th,
                                int sw, int sh, int dw, int dh,
                                int *min_y, int *max_y);

/*
 * PC reference: full-image bilinear resize. Allocates its own tables.
 * Bit-exact with refactor's ngd_resize_bilinear (same maths).
 */
void orb_resize_full(const uint8_t *src, int sw, int sh,
                     uint8_t *dst, int dw, int dh);

#ifdef __cplusplus
}
#endif
#endif /* ORB_RESIZE_CORE_H */
