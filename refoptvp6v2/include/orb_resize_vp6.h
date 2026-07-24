/*
 * orb_resize_vp6.h - VP6 tile-based bilinear resize (Cadence P6 TileManager /
 *                    xvTile API).
 *
 * The output frame is split into square tiles. For each output tile the source
 * rows needed (2-row vertical kernel extent) are DMA'd from DRAM into local
 * SRAM, precomputed tables (xofs/ialpha/yofs/ibeta) are prepared, and the
 * shared orb_resize_kernel() runs on the SRAM data. Ping/pong double-buffering
 * overlaps DMA-in(N+1) with the kernel(N) and DMA-out(N-1).
 *
 * Mirrors orb_blur_vp6.h. Links orb_resize_core.c and the P6 TileManager.
 */
#ifndef ORB_RESIZE_VP6_H
#define ORB_RESIZE_VP6_H

#include "orb_resize_core.h"
#include "tileManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* VP6 tile configuration. */
typedef struct {
    int tile_w;          /* output tile width  (e.g. 64)                        */
    int tile_h;          /* output tile height (e.g. 64)                       */
    int max_src_tile_w;  /* max source-region width  a tile buffer must hold   */
    int max_src_tile_h;  /* max source-region height a tile buffer must hold   */
} OrbResizeVp6Config;

/* Defaults: 64x64 tiles, full-width source, generous row margin for scale factor. */
#define ORB_RESIZE_VP6_DEFAULT_CONFIG { 64, 64, 2048, 256 }

/*
 * Run tile-based bilinear resize on VP6.
 *   tm        : xvTileManager.
 *   src_frame : xvFrame wrapping source image in DRAM (U8).
 *   dst_frame : xvFrame wrapping output in DRAM (U8).
 *   sw/sh     : source image dimensions.
 *   dw/dh     : destination image dimensions.
 *   cfg       : tile configuration (NULL => default).
 * Returns 0 on success, non-zero on error.
 */
int orb_resize_vp6_process(xvTileManager *tm,
                           xvFrame *src_frame, xvFrame *dst_frame,
                           int sw, int sh, int dw, int dh,
                           const OrbResizeVp6Config *cfg);

#ifdef __cplusplus
}
#endif
#endif /* ORB_RESIZE_VP6_H */
