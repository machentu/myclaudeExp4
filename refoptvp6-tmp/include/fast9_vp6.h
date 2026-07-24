/*
 * fast9_vp6.h - VP6 strip-based FAST-9 detect + NMS (Cadence P6 TileManager /
 *              xvTile API).
 *
 * The detection ROI [iniX,maxX)x[iniY,maxY) is split into HORIZONTAL STRIPS of
 * height `strip_h` (each strip spans the FULL ROI width). For each strip the
 * source region needed (strip +/- 4 = ring 3 + NMS 1, clamped to the frame) is
 * DMA'd from DRAM into local SRAM, the shared fast9_detect_kernel() runs on the
 * SRAM data, and detected keypoints are written to the DRAM output list.
 *
 * Why strips (not square sub-tiles): a strip spans the full ROI width, so the
 * kernel emits keypoints row-by-row across the whole width = the same raster
 * order as refactor's ngd_fast_detect. Strips top-to-bottom concatenate to
 * global raster, so the VP6 output is bit-exact with ngd_fast_detect including
 * keypoint ORDER and the cap cut (which a square-sub-tile traversal could not
 * match). The +/-4 vertical margin covers the FAST ring (3) and the NMS
 * neighbour ring (4) across strip boundaries, so each pixel is NMS'd exactly
 * once with all 8 in-ROI neighbours.
 *
 * Output is SPARSE (a keypoint list), so there is no DMA-out of a dense tile:
 * the kernel appends keypoints to the caller's DRAM buffer. Ping/pong
 * double-buffering overlaps the DMA-in of strip j+1 with the kernel of strip j.
 *
 * Mirrors orb_blur_vp6.h / vp6ref/vp6/ipm_vp6.h (minus the output DMA). See
 * README.md.
 */
#ifndef FAST9_VP6_H
#define FAST9_VP6_H

#include "fast9_core.h"
#include "tileManager.h"   /* P6 TileManager (project-provided) */

#ifdef __cplusplus
extern "C" {
#endif

/* VP6 strip configuration.
 *   max_src_w must be >= ROI width + 8 (caller sets it per ROI; the strip
 *     spans the full ROI width + a +/-4 horizontal margin).
 *   max_src_h must be >= strip_h + 8 (strip + +/-4 vertical margin). */
typedef struct {
    int strip_h;        /* vertical strip height (e.g. 64)                   */
    int max_src_w;      /* max source-region width  a tile buffer must hold  */
    int max_src_h;      /* max source-region height a tile buffer must hold  */
} Fast9Vp6Config;

/* Defaults: 64-tall strips, source regions up to 256 wide x 72 tall. The
 * caller raises max_src_w if the ROI width + 8 exceeds it. */
#define FAST9_VP6_DEFAULT_CONFIG { 64, 256, 72 }

/*
 * Run strip-based FAST-9 detect + NMS on VP6 over ROI [iniX,maxX)x[iniY,maxY).
 *   tm        : xvTileManager created with xvCreateTileManager().
 *   src_frame : xvFrame wrapping the source image in DRAM (U8).
 *   src_w/h   : source image dimensions (must match src_frame).
 *   iniX/maxX/iniY/maxY : detection ROI (0<=iniX<maxX<=src_w, same for Y).
 *   t         : FAST threshold.
 *   out       : keypoint output buffer in DRAM, capacity `cap`.
 *   n_inout   : in = initial count (usually 0), out = final count.
 *   cfg       : strip configuration (NULL => FAST9_VP6_DEFAULT_CONFIG).
 * Returns 0 on success, non-zero on error.
 *
 * Keypoints are emitted in global raster order (strips top-to-bottom, each
 * strip row-by-row across the full ROI width), bit-exact with fast9_full (==
 * refactor ngd_fast_detect) including the cap cut. The per-strip kernel maths
 * is the same fast9_detect_kernel() validated by test_fast9_vp6.c.
 */
int fast9_vp6_process(xvTileManager *tm, xvFrame *src_frame,
                      int src_w, int src_h,
                      int iniX, int maxX, int iniY, int maxY, int t,
                      fast9_keypoint *out, int *n_inout, int cap,
                      const Fast9Vp6Config *cfg);

#ifdef __cplusplus
}
#endif
#endif /* FAST9_VP6_H */
