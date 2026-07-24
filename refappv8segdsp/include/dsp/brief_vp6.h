/*
 * brief_vp6.h - VP6 per-keypoint rotated-BRIEF descriptor (Cadence P6
 *              TileManager / xvTile API).
 *
 * For each keypoint the +/-19 source patch (clamped to the frame) is DMA'd from
 * DRAM into local SRAM, the shared brief_kernel() (validated on PC, bit-exact
 * with refactor's ngd_compute_descriptor) runs on the SRAM data, and the 32-byte
 * descriptor is written to desc[i*32] (by index, preserving keypoint order).
 * Ping/pong double-buffering overlaps the DMA-in of keypoint k+1 with the kernel
 * of keypoint k.
 *
 * Like IC angle, rBRIEF is a per-keypoint gather (256 rotated patch samples), so
 * the VP6 unit is one patch tile per keypoint. The patch is +/-19 (pattern coord
 * max 13 -> rotated offset <= ~18). Output is 32 bytes/keypoint (no dense DMA-out).
 * The xtensa ORBAngleBRIEF.c reference uses IVP gather + a sincos LUT for the
 * rotation; here the patch is DMA'd and the kernel uses cosf/sinf (portable,
 * auto-vectorisable) - the IVP path is a future hook.
 *
 * Mirrors ic_angle_vp6.h. See README.md.
 */
#ifndef BRIEF_VP6_H
#define BRIEF_VP6_H

#include "brief_core.h"
#include "fast9_core.h"     /* fast9_keypoint (keypoint input) */
#include "tileManager.h"    /* P6 TileManager (project-provided) */

#ifdef __cplusplus
extern "C" {
#endif

/* VP6 patch-tile config. max_src_{w,h} must be >= 39 (the +/-19 patch). */
typedef struct {
    int max_src_w;   /* max patch width  a tile buffer must hold (>= 39) */
    int max_src_h;   /* max patch height a tile buffer must hold (>= 39) */
} BriefVp6Config;

/* Defaults: 40x40 patch tiles (39 + 1 headroom). */
#define BRIEF_VP6_DEFAULT_CONFIG { 40, 40 }

/*
 * Run per-keypoint rotated-BRIEF on VP6.
 *   tm        : xvTileManager created with xvCreateTileManager().
 *   src_frame : xvFrame wrapping the source (blurred pyramid level) image in DRAM.
 *   src_w/h   : source image dimensions (must match src_frame).
 *   kps       : input keypoints (frame coords); kps[i].x/.y float (rounded with
 *               lroundf), kps[i].angle = orientation in degrees (from IC angle).
 *   desc      : output buffer, descriptor for kps[i] at desc[i*32 .. i*32+31].
 *   n_kps     : number of keypoints.
 *   cfg       : patch-tile config (NULL => BRIEF_VP6_DEFAULT_CONFIG).
 * Returns 0 on success, non-zero on error.
 *
 * desc[i*32] is bit-exact with brief_full (== refactor ngd_compute_descriptor)
 * for every keypoint: the per-keypoint kernel maths is the same brief_kernel()
 * validated by test_brief_vp6.c (3-way: verbatim refactor ref vs brief_full vs
 * brief_vp6).
 */
int brief_vp6_process(xvTileManager *tm, xvFrame *src_frame,
                      int src_w, int src_h,
                      const fast9_keypoint *kps, uint8_t *desc, int n_kps,
                      const BriefVp6Config *cfg);

#ifdef __cplusplus
}
#endif
#endif /* BRIEF_VP6_H */
