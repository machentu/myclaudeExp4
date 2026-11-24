/*
 * orb_blur_vp6.h - VP6 tile-based ORB Gaussian blur (Cadence P6 TileManager /
 *                  raw 2D iDMA).
 *
 * The output frame is split into square tiles. For each output tile the source
 * region needed (output tile +/- 3 for the 7-tap neighbour, clamped to the
 * frame) is moved from DRAM into local SRAM by one raw iDMA descriptor, the
 * shared orb_blur_kernel() (validated on PC, bit-exact with refactor's
 * ngd_gaussian_blur) runs on the SRAM data via a scratch tmp, and the output
 * tile is moved back to the frame by another descriptor.
 *
 * ALL SRAM buffers (ping/pong input pairs in bank0/bank1, ping/pong output
 * pairs, and the u16 tmp) are allocated ONCE via xvAllocateBuffer and reused
 * across frames and levels - the ExtractImage.cpp / TileMemMgr.cpp pattern
 * (see orb_tile_manager.h for the two-bank budget). Ping/pong
 * double-buffering overlaps DMA-in(N+1) with the kernel(N) and DMA-out(N-1),
 * hiding DRAM latency - the dominant cost on VP6.
 *
 * Links orb_blur_core.c (for orb_blur_kernel / orb_blur_tile_source_bbox) and
 * the P6 TileManager (TileManager_P6) plus the cstub lib on PC. See README.md.
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

/* Release the persistent SRAM buffers (optional; they also grow on demand). */
void orb_blur_vp6_release(void);

/* ---- sub-stage cycle profiling inside orb_blur_vp6_process (board v1003+;
 * accumulate like dsp_orb_vp6_cycles — zero them between measurements) ---- */
enum {
    ORB_BLUR_VP6_S_SUBMIT = 0,  /* xvAddIdmaRequest (descriptor submit) */
    ORB_BLUR_VP6_S_WAIT,        /* xvWaitForiDMA                      */
    ORB_BLUR_VP6_S_KERNEL,      /* per-tile IVP kernel                */
    ORB_BLUR_VP6_S_HPASS,       /* v1004: kernel horizontal pass      */
    ORB_BLUR_VP6_S_VPASS,       /* v1004: kernel vertical pass        */
    ORB_BLUR_VP6_S_COUNT
};
extern unsigned long long orb_blur_vp6_sub[ORB_BLUR_VP6_S_COUNT];

/* v1006 kernel-structure counters (counts, not cycles): how much of the
 * kernel actually runs the 64-lane IVP loops vs the scalar border paths —
 * decides whether the board hpass anomaly lives in the vector body at all.
 * Accumulate like the cycle counters. */
extern unsigned long long orb_blur_vp6_vec;    /* 64-lane chunks executed   */
extern unsigned long long orb_blur_vp6_scol;   /* scalar border column ops  */

/* The IVP kernel itself (v1007: exported for CAL3 on-chip calibration, which
 * times the production loop shape on a synthetic 72x72 -> 64x64 tile; see
 * dsp_calib_vp6.c usage before changing the signature). */
void orb_blur_kernel_ivp(const uint8_t *src_data, int src_pitch,
                         int origin_x, int origin_y, int region_h,
                         int frame_w, int frame_h,
                         int tu0, int tv0, int dst_w, int dst_h,
                         uint8_t *dst, int dst_pitch,
                         uint16_t *tmp, int tmp_pitch);

#ifdef __cplusplus
}
#endif
#endif /* ORB_BLUR_VP6_H */
