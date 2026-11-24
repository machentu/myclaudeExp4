/*
 * orb_tile_manager.h - app-side TileManager init for the ORB VP6 drivers,
 *                      mirroring sgbmwarpFace/WarpRGBA/TileMemMgr.cpp::
 *                      InitTileMrg (the reference the DSP-side integration
 *                      uses).
 *
 * Two static bank pools placed in the DSP's two local DRAM bank regions
 * (128KB each) via .dram0/.dram1 linker sections on Xtensa (plain aligned
 * arrays on the host/cstub build), one iDMA descriptor buffer, one
 * xvCreateTileManager call. The pools default to 126KB - slightly under the
 * region - because the sections also carry other project data (see the
 * ORB_TM_POOL_BYTES note below). The four VP6 drivers' DEFAULT strip/tile
 * configs are sized to fit these two pools together:
 *
 *   orb_blur (64x64 tiles)      ~27KB     fast9_scoremap (strip 40)   ~59KB
 *   ic_angle  (strip 24 + halo) ~69KB     brief         (strip 24 + halo) ~79KB
 *
 * Raising any driver's config requires enlarging ORB_TM_POOL_BYTES (and the
 * DSP-side region) or shrinking another driver's.
 */
#ifndef ORB_TILE_MANAGER_H
#define ORB_TILE_MANAGER_H

#include "tileManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Per-bank pool size. The .dram0/.dram1 linker regions are 128KB each
 * (WarpRGBA POOL_SIZE0/POOL_SIZE1), but the pools must NOT claim the whole
 * region: the sections also carry other project content (iDMA descriptor
 * storage etc. - the fy01 HelloWorld link had ~1.2KB extra in .dram1.data,
 * overflowing a full-128KB pool by exactly that much). Default 126KB leaves
 * that headroom while still covering the four drivers' defaults (bank0 peak
 * 121,920B). Override at build time with -DORB_TM_POOL_BYTES=... if your
 * project places more in those sections.
 */
#ifndef ORB_TM_POOL_BYTES
#define ORB_TM_POOL_BYTES   (126 * 1024)
#endif
#define ORB_TM_DESCR_CNT    32

#ifndef MAX_BLOCK_8
#define MAX_BLOCK_8         8
#endif
#ifndef MAX_PIF
#define MAX_PIF             16
#endif

/*
 * Initialize *tm with the two static 128KB bank pools and the shared iDMA
 * object. Returns 0 on success, -1 on failure (error printed to stderr).
 * Call once before the first *_vp6_process(); the pools stay resident for
 * the program's lifetime (nothing to release).
 */
int orb_tile_manager_init(xvTileManager *tm);

#ifdef __cplusplus
}
#endif
#endif /* ORB_TILE_MANAGER_H */
