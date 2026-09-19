/*
 * fast9_scoremap_vp6.h - VP6 strip driver + IVP kernel for the FAST-9 score map.
 *
 * fast9_build_scoremap with the whole frame streamed through two resident SRAM
 * strip buffers (P6 TileManager raw 2D iDMA, ping in bank0 / pong in bank1,
 * allocated once and reused - the ExtractImage.cpp / TileMemMgr.cpp pattern)
 * and the per-pixel shared-core min/max tree evaluated in 64-byte IVP lanes
 * (2Nx8U ops under the Xtensa cstub; on a real VP6 DSP the same intrinsics map
 * to FLIX-wide vector instructions).
 *
 * Strips of `strip_h` map rows span the FULL frame width, top-to-bottom; each
 * strip's source region is the strip +/-3 ring rows, moved by one raw iDMA
 * descriptor into one of the two buffers while the previous strip's kernel
 * runs (ping/pong overlap). Map bytes are written directly to DRAM (dense
 * output, raster order, identical to fast9_build_scoremap - see the
 * equivalence notes in fast9_scoremap_vp6.c).
 *
 * Build: C++ under the Xtensa cstub (LANGUAGE CXX, links p6_tilemanager).
 */
#ifndef FAST9_SCOREMAP_VP6_H
#define FAST9_SCOREMAP_VP6_H

#include <stdint.h>

#include "tileManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* vertical halo the driver DMAes around each strip (FAST-9 ring radius) */
#define FAST9_SCOREMAP_HALO 3

typedef struct {
    int strip_h;    /* emitted map rows per pass                        */
    int max_src_w;  /* strip buffer width  >= frame width (ALIGN64'd)   */
    int max_src_h;  /* strip buffer rows  >= strip_h + 2*HALO           */
} Fast9ScoremapVp6Config;

/* Defaults: 640-wide strips, 40 map rows per pass (46-row buffers = 29,440B
 * per bank); the four VP6 drivers' defaults together fit two 128KB banks
 * (see orb_tile_manager.h). */
#define FAST9_SCOREMAP_VP6_DEFAULT_CONFIG { 40, 640, 46 }

/*
 * Build the FAST-9 score map (same output as fast9_build_scoremap) by
 * streaming the frame through SRAM strips:
 *   map[y*map_stride + x] must be writable for y in [0,src_h), x in [0,src_w).
 *
 * t_floor must be in [0, 254] (the vector path compares it as a byte lane;
 * production uses min(iniThFAST, minThFAST) = 7).
 * Returns 0 on success, negative on error (bad args / tile alloc / bbox).
 */
int fast9_scoremap_vp6_build(xvTileManager *tm, xvFrame *src_frame,
                             int src_w, int src_h, int t_floor,
                             uint8_t *map, int map_stride,
                             const Fast9ScoremapVp6Config *cfg);

/* Release the persistent strip buffers (optional; they also grow on demand). */
void fast9_scoremap_vp6_release(void);

/*
 * IVP kernel for one strip. Runs on the strip's SRAM tile (src_data/src_pitch
 * = XV_TILE_GET_DATA_PTR/PITCH) whose origin within the frame is
 * (origin_x, origin_y); emits map rows [y0,y1) in FRAME coordinates
 * (clamped internally to the ring-legal [3, frame_h-4] band, so border rows
 * just keep the NONE the driver pre-filled). Ring loads stay inside the tile:
 * the driver guarantees the tile spans the full frame width and the strip
 * rows +/-3.
 */
void fast9_scoremap_kernel_ivp(const uint8_t *src_data, int src_pitch,
                               int origin_x, int origin_y,
                               int frame_w, int frame_h,
                               int y0, int y1, int t_floor,
                               uint8_t *map, int map_stride);

#ifdef __cplusplus
}
#endif
#endif /* FAST9_SCOREMAP_VP6_H */
