/*
 * orb_blur_vp6.h - VP6 tile-based ORB Gaussian blur (Cadence P6 TileManager /
 *                  xvTile API).
 *
 * The output frame is split into square tiles. For each output tile the source
 * region needed (output tile +/- 3 for the 7-tap neighbour, clamped to the
 * frame) is DMA'd from DRAM into local SRAM, the shared orb_blur_kernel()
 * (validated on PC, bit-exact with refactor's ngd_gaussian_blur) runs on the
 * SRAM data via a scratch tmp, and the output tile is DMA'd back to the frame.
 * Ping/pong double-buffering overlaps DMA-in(N+1) with the kernel(N) and
 * DMA-out(N-1), hiding DRAM latency - the dominant cost on VP6.
 *
 * Mirrors vp6ref/vp6/ipm_vp6.h. Links orb_blur_core.c (for orb_blur_kernel /
 * orb_blur_tile_source_bbox) and the P6 TileManager (TileManager_P6) plus the
 * cstub lib on PC. See README.md.
 */
#ifndef ORB_BLUR_VP6_H
#define ORB_BLUR_VP6_H

#include "orb_blur_core.h"
#include "tileManager.h"   /* P6 TileManager (project-provided) */

#ifdef __cplusplus
extern "C" {
#endif

/* VP6 tile configuration. max_src_tile_{w,h} must be >= tile_{w,h} + 6 (the
 * 7-tap source region for an interior tile). */
typedef struct {
    int tile_w;          /* output tile width  (multiple of 8, e.g. 64)        */
    int tile_h;          /* output tile height (e.g. 64)                       */
    int max_src_tile_w;  /* max source-region width  a tile buffer must hold   */
    int max_src_tile_h;  /* max source-region height a tile buffer must hold   */
} OrbBlurVp6Config;

/* Defaults: 64x64 tiles, source regions up to 72x72 (64+6+margin). */
#define ORB_BLUR_VP6_DEFAULT_CONFIG { 64, 64, 72, 72 }

/*
 * Run tile-based ORB Gaussian blur on VP6.
 *   tm        : xvTileManager created with xvCreateTileManager().
 *   src_frame : xvFrame wrapping the source image in DRAM (U8, edge pad >=3
 *               recommended, though EDGE=0 is used per-tile because the bbox
 *               already covers the +3 neighbour via reflect-101).
 *   dst_frame : xvFrame wrapping the output in DRAM (same size as src).
 *   src_w/h   : source image dimensions (must match src_frame).
 *   cfg       : tile configuration (NULL => ORB_BLUR_VP6_DEFAULT_CONFIG).
 * Returns 0 on success, non-zero on error.
 *
 * cstub (PC): the per-tile kernel maths is the same orb_blur_kernel() validated
 * by test_orb_blur_vp6.c (bit-exact with the PC full-image reference). DMA
 * plumbing is exercised via the TileManager iDMA emulator. On DSP the same code
 * runs with real iDMA.
 */
int orb_blur_vp6_process(xvTileManager *tm,
                         xvFrame *src_frame, xvFrame *dst_frame,
                         int src_w, int src_h,
                         const OrbBlurVp6Config *cfg);

#ifdef __cplusplus
}
#endif
#endif /* ORB_BLUR_VP6_H */
