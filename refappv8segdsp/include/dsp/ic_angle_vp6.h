/*
 * ic_angle_vp6.h - VP6 per-keypoint IC angle (Cadence P6 TileManager / xvTile API).
 *
 * For each keypoint the +/-15 source patch (clamped to the frame) is DMA'd from
 * DRAM into local SRAM, the shared ic_angle_kernel() (validated on PC, bit-exact
 * with refactor's ngd_ic_angle) runs on the SRAM data, and the angle is written
 * to angles[i] (by index, preserving keypoint order). Ping/pong double-buffering
 * overlaps the DMA-in of keypoint k+1 with the kernel of keypoint k.
 *
 * IC angle is a per-keypoint gather (radius-15 disk), so the natural VP6 unit is
 * one 31x31 patch tile per keypoint (not a whole-image strip). The xtensa
 * ORBAngleBRIEF.c reference uses IVP gather for the same disk; here the patch is
 * DMA'd (the disk pixels the kernel reads are a subset of the patch). Output is
 * one float per keypoint (no dense DMA-out).
 *
 * Mirrors orb_blur_vp6.h / fast9_vp6.h (minus the dense output). See README.md.
 */
#ifndef IC_ANGLE_VP6_H
#define IC_ANGLE_VP6_H

#include "ic_angle_core.h"
#include "fast9_core.h"     /* fast9_keypoint (keypoint input) */
#include "tileManager.h"    /* P6 TileManager (project-provided) */

#ifdef __cplusplus
extern "C" {
#endif

/* VP6 patch-tile config. max_src_{w,h} must be >= 31 (the +/-15 patch). */
typedef struct {
    int max_src_w;   /* max patch width  a tile buffer must hold (>= 31) */
    int max_src_h;   /* max patch height a tile buffer must hold (>= 31) */
} IcAngleVp6Config;

/* Defaults: 32x32 patch tiles (31 + 1 margin for alignment headroom). */
#define IC_ANGLE_VP6_DEFAULT_CONFIG { 32, 32 }

/*
 * Run per-keypoint IC angle on VP6.
 *   tm        : xvTileManager created with xvCreateTileManager().
 *   src_frame : xvFrame wrapping the source (raw pyramid level) image in DRAM.
 *   src_w/h   : source image dimensions (must match src_frame).
 *   kps       : input keypoints (frame coords); kps[i].x/.y are float (rounded
 *               with lroundf, matching ngd_ic_angle).
 *   angles    : output buffer, angles[i] = orientation of kps[i] (degrees).
 *   n_kps     : number of keypoints.
 *   umax      : the table from ic_angle_init_umax.
 *   cfg       : patch-tile config (NULL => IC_ANGLE_VP6_DEFAULT_CONFIG).
 * Returns 0 on success, non-zero on error.
 *
 * angles[i] is bit-exact with ic_angle_full (== refactor ngd_ic_angle) for every
 * keypoint: the per-keypoint kernel maths is the same ic_angle_kernel()
 * validated by test_ic_angle_vp6.c (3-way: verbatim refactor ref vs ic_angle_full
 * vs ic_angle_vp6).
 */
int ic_angle_vp6_process(xvTileManager *tm, xvFrame *src_frame,
                         int src_w, int src_h,
                         const fast9_keypoint *kps, float *angles, int n_kps,
                         const int *umax, const IcAngleVp6Config *cfg);

#ifdef __cplusplus
}
#endif
#endif /* IC_ANGLE_VP6_H */
