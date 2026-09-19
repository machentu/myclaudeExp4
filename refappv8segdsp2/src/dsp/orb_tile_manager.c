/*
 * orb_tile_manager.c - app-side TileManager init, 1:1 pattern of
 *                      sgbmwarpFace/WarpRGBA/TileMemMgr.cpp::InitTileMrg.
 *
 * The bank pools are STATIC arrays carrying the _LOCAL_DRAM0_/_LOCAL_DRAM1_
 * section attributes on Xtensa (see WarpRGBA/inc/commonDef.h), which the
 * fy01 linker script maps into the two local DRAM banks - this is what makes
 * every xvAllocateBuffer'd strip/tile buffer SRAM-local instead of system
 * DRAM. On the host/cstub build the attributes are empty and the pools are
 * ordinary aligned arrays, so the same file builds everywhere.
 */
#include "orb_tile_manager.h"

#include <stdio.h>
#include <stdint.h>

#ifdef __XTENSA__
#define ORB_LOCAL_DRAM0_ __attribute__((aligned(64), section(".dram0.data")))
#define ORB_LOCAL_DRAM1_ __attribute__((aligned(64), section(".dram1.data")))
#else
#define ORB_LOCAL_DRAM0_
#define ORB_LOCAL_DRAM1_
#endif

static IDMA_BUFFER_DEFINE(orb_idma_obj, ORB_TM_DESCR_CNT, IDMA_2D_DESC);

static uint8_t orb_bank0[ORB_TM_POOL_BYTES] ORB_LOCAL_DRAM0_;
static uint8_t orb_bank1[ORB_TM_POOL_BYTES] ORB_LOCAL_DRAM1_;

static void orb_idma_err_cb(idma_error_details_t *d)
{
    (void)d;
    fprintf(stderr, "iDMA error\n");
}

static void orb_idma_intr_cb(void *p)
{
    (void)p;
}

int orb_tile_manager_init(xvTileManager *tm)
{
    if (!tm) return -1;

    void *banks[2] = { orb_bank0, orb_bank1 };
    int32_t sizes[2] = { ORB_TM_POOL_BYTES, ORB_TM_POOL_BYTES };

    /* xvCreateTileManager = xvInitTileManager + xvInitMemAllocator(2 banks)
     * + xvInitIdma - the same sequence InitTileMrg performs piecewise on the
     * WarpRGBA TileManager_P6 variant of this library. */
    int32_t r = xvCreateTileManager(tm, orb_idma_obj, 2, banks, sizes,
                                    (idma_err_callback_fn)orb_idma_err_cb,
                                    (idma_callback_fn)orb_idma_intr_cb, NULL,
                                    ORB_TM_DESCR_CNT, MAX_BLOCK_8, MAX_PIF);
    if (r != XVTM_SUCCESS) {
        fprintf(stderr, "orb_tile_manager_init: xvCreateTileManager failed (%d)\n",
                (int)r);
        return -1;
    }
    return 0;
}
