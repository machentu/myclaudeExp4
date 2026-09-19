/*
 * ic_angle_vp6.h - VP6 whole-frame strip IC angle (Cadence P6 TileManager /
 *                  iDMA).
 *
 * The frame streams through two resident SRAM strip buffers (ping in bank0,
 * pong in bank1, allocated once and reused across frames/levels - the
 * ExtractImage.cpp / TileMemMgr.cpp pattern). Each sweep pass covers
 * `strip_rows` frame rows plus a +/-15 halo; every keypoint whose clamped
 * centre row falls in the pass has its IC angle computed by the shared
 * ic_angle_kernel() (validated on PC, bit-exact with refactor's ngd_ic_angle)
 * gathering straight from the SRAM strip - no per-keypoint DMA. angles[i] is
 * written by index, preserving keypoint order.
 *
 * The kernel is the IVP intrinsic form (IVP gather pixel access + per-column
 * accumulators + scalar moment fold); the clamp/offset precompute stays
 * scalar. Define IC_ANGLE_NO_IVP when building ic_angle_vp6.c to use the
 * portable kernel.
 *
 * Buffers persist until ic_angle_vp6_release() (or process is called with a
 * larger config). Default sizing: 640-wide strips x (24 + 30) rows = 34,560
 * bytes per bank - the four VP6 drivers' defaults together fit two 128KB
 * banks (see orb_tile_manager.h).
 */
#ifndef IC_ANGLE_VP6_H
#define IC_ANGLE_VP6_H

#include "ic_angle_core.h"
#include "fast9_core.h"     /* fast9_keypoint (keypoint input) */
#include "tileManager.h"    /* P6 TileManager (project-provided) */

#ifdef __cplusplus
extern "C" {
#endif

/* VP6 strip config: strip buffers sized ALIGN64(max_src_w) x max_src_h. */
typedef struct {
    int strip_rows;  /* frame rows assigned per sweep pass (>= 1)         */
    int max_src_w;   /* strip pitch basis: >= largest frame width         */
    int max_src_h;   /* strip buffer rows = strip_rows + 2*IC_ANGLE_MARGIN*/
} IcAngleVp6Config;

/* Defaults: 640-wide strips, 24 assigned rows/pass (54-row buffers). */
#define IC_ANGLE_VP6_DEFAULT_CONFIG { 24, 640, 54 }

/*
 * Run IC angle for all keypoints on VP6 (whole-frame strip sweep).
 *   tm        : xvTileManager created with xvCreateTileManager().
 *   src_frame : xvFrame wrapping the source image in DRAM (the UNBLURRED
 *               pyramid level in the ORB pipeline — orientation is computed
 *               on the raw level, unlike BRIEF which uses the blurred one).
 *   src_w/h   : source image dimensions (must match src_frame).
 *   kps       : input keypoints (frame coords); kps[i].x/.y are float (rounded
 *               with lroundf, matching ngd_ic_angle).
 *   angles    : output buffer, angles[i] = orientation of kps[i] (degrees).
 *   n_kps     : number of keypoints.
 *   umax      : the table from ic_angle_init_umax.
 *   cfg       : strip config (NULL => IC_ANGLE_VP6_DEFAULT_CONFIG).
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

/* Release the persistent strip buffers (optional; they also grow on demand). */
void ic_angle_vp6_release(void);

#ifdef __cplusplus
}
#endif
#endif /* IC_ANGLE_VP6_H */
