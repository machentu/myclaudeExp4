/*
 * ic_angle_core.h - portable ORB orientation (IC_Angle) kernel (shared PC + VP6).
 *
 * Extracted verbatim-arithmetic from refactor/src/orb.c::ngd_ic_angle /
 * ngd_pix_clamp. The intensity-centroid maths (m_10/m_01 over the radius-15
 * disk via the umax table, atan2f -> degrees) is UNCHANGED so the result is
 * bit-exact with the refactor C reference.
 *
 * This is the SLAM analogue of fast9_core.c: the same function the PC reference
 * (ic_angle_full) and the VP6 driver (ic_angle_vp6.c) call. The kernel is
 * tile-aware: it reads a source region DMA'd into SRAM (origin_x/origin_y =
 * region top-left in frame coords) and uses absolute frame coords for the
 * edge-clamp (ngd_pix_clamp clamps to the frame edge, not reflect-101), so the
 * source region is the keypoint's +/-15 patch clamped to the frame (EDGE=0).
 */
#ifndef IC_ANGLE_CORE_H
#define IC_ANGLE_CORE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IC_ANGLE_HALF_PATCH 15                      /* NGD_ORB_HALF_PATCH       */
#define IC_ANGLE_PATCH_SIZE (2 * IC_ANGLE_HALF_PATCH + 1)  /* 31              */
#define IC_ANGLE_UMAX_SIZE  (IC_ANGLE_HALF_PATCH + 1)     /* 16 entries      */
#define IC_ANGLE_MARGIN     IC_ANGLE_HALF_PATCH          /* source margin   */

/*
 * Compute the umax table (1:1 from refactor ngd_orb_init): per-row horizontal
 * extent of the radius-15 disk. Fills umax[0..15]. The same table refactor's
 * ngd_ic_angle uses, so the disk sampled is identical.
 */
void ic_angle_init_umax(int umax[IC_ANGLE_UMAX_SIZE]);

/*
 * Tile-aware IC angle (1:1 maths from refactor ngd_ic_angle).
 *   src_data/src_pitch : SRAM source region (DMA'd in by the VP6 driver), or
 *                        the full image for the PC reference path.
 *   origin_x/origin_y  : top-left of the source region in FRAME coords.
 *   frame_w/frame_h    : full image size (for the edge-clamp, same as
 *                        ngd_pix_clamp's w,h).
 *   cx/cy              : keypoint centre in FRAME coords (caller pre-rounds
 *                        the float keypoint with lroundf, matching ngd_ic_angle).
 *   umax               : the table from ic_angle_init_umax.
 * Returns the orientation in degrees (atan2f(m_01,m_10)*180/pi). Bit-exact with
 * refactor's ngd_ic_angle (the per-pixel arithmetic and atan2f are identical).
 */
float ic_angle_kernel(const uint8_t *src_data, int src_pitch,
                      int origin_x, int origin_y,
                      int frame_w, int frame_h,
                      int cx, int cy, const int *umax);

/*
 * Per-keypoint patch bbox: (cx,cy) +/- IC_ANGLE_MARGIN (15), clamped to the
 * frame. The source region DMA'd for one keypoint. Always valid.
 */
void ic_angle_patch_bbox(int cx, int cy, int frame_w, int frame_h,
                         int *min_x, int *min_y, int *max_x, int *max_y);

/*
 * PC reference: IC angle on the full image (origin 0,0). Bit-exact with
 * refactor's ngd_ic_angle by construction (ic_angle_kernel with origin 0,0 and
 * src=image == ngd_ic_angle).
 */
float ic_angle_full(const uint8_t *img, int w, int h, int stride,
                    int cx, int cy, const int *umax);

#ifdef __cplusplus
}
#endif
#endif /* IC_ANGLE_CORE_H */
