/*
 * dsp_calib_vp6.h — one-shot on-chip access/instruction cost calibration
 *                   (v1005). See dsp_calib_vp6.c for motivation and method.
 *
 * Each result is cycles-per-op x100 (op = one byte load / one 64B vector
 * load / one MAC / one u16 store / one iDMA byte ...), measured on the
 * first workflow call. Zero on host builds / before the first call.
 */
#ifndef DSP_CALIB_VP6_H
#define DSP_CALIB_VP6_H

#include <stdint.h>

#include "tileManager.h"     /* xvTileManager */

#ifdef __cplusplus
extern "C" {
#endif

enum {
    DSP_CALIB_LD_HEAP = 0,   /* CPU u8 load, heap DDR (image region)      */
    DSP_CALIB_LD_BANK,       /* CPU u8 load, bank SRAM (COLOR_1)          */
    DSP_CALIB_LD_BANK1,      /* CPU u8 load, bank SRAM, base offset 1     */
    DSP_CALIB_LV_BANK,       /* IVP aligned 64B LV load, bank             */
    DSP_CALIB_LV_HEAP,       /* IVP aligned 64B LV load, heap DDR         */
    DSP_CALIB_LAV1,          /* 1 concurrent LAV unaligned stream, bank   */
    DSP_CALIB_LAV2,          /* 2 concurrent LAV streams                  */
    DSP_CALIB_LAV7,          /* 7 concurrent LAV streams (blur hpass)     */
    DSP_CALIB_GATH7,         /* 7-tap stride-1 GATHERAN/GATHERD pattern   */
    DSP_CALIB_MAC7,          /* MULUS + 6x MULUSPA chain, resident vector */
    DSP_CALIB_SV16,          /* IVP u16 vector store to bank (per u16)    */
    DSP_CALIB_FADD,          /* float mul+add pair (soft/hard float check)*/
    DSP_CALIB_MEMSET,        /* CPU memset heap (per byte written)        */
    DSP_CALIB_IDMA,          /* iDMA 64x64 heap->bank copy (per byte)     */
    DSP_CALIB_COUNT
};

/* results: cycles/op x100, indexed by the enum above */
extern unsigned long long dsp_calib_vp6_res[DSP_CALIB_COUNT];

/* ---- v1006 round 2: decompose the blur kernel anomaly (CAL1 unit prices
 *      say the hpass vector body should cost ~3.5M cycles/frame; board
 *      measures 178M) and the quadtree's DDR/malloc dependency. ---- */
enum {
    DSP_CALIB2_LD_STACK = 0, /* CPU u8 load from the C stack (spill cost)    */
    DSP_CALIB2_LD_C0,        /* CPU u8 load, bank COLOR_0 (ping — unmeasured)*/
    DSP_CALIB2_LV_C0,        /* IVP aligned 64B LV, COLOR_0                  */
    DSP_CALIB2_LAV7_C0,      /* 7 concurrent LAV streams, COLOR_0            */
    DSP_CALIB2_MALLOC,       /* malloc(64)+free pair (octtree node unit)     */
    DSP_CALIB2_HP_ROW_C1,    /* blur hpass composite per ROW: 7 primes + 1
                              * chunk (LAV7+MAC7+CVT2+SV2), src COLOR_1 ->
                              * tmp COLOR_1, no DMA                         */
    DSP_CALIB2_HP_ROW_C0,    /* same, src COLOR_0 -> tmp COLOR_0 (the tile
                              * driver's exact buffer colors)                */
    DSP_CALIB2_HP_ROW_DMA,   /* HP_ROW_C0 with heap->COLOR_1 iDMA submitted
                              * alongside (bank port contention)            */
    DSP_CALIB2_HP_PRIME,     /* 7x LA2NX8U_PP row primes only, per row       */
    DSP_CALIB2_VP_CHUNK_C0,  /* blur vpass composite per CHUNK: 14 LV tmp
                              * COLOR_0 + UNPK/CVT24/PACKL + 14 MAC + SA    */
    DSP_CALIB2_COUNT
};

/* results: cycles/row (HP_*), cycles/chunk (VP), cycles/op (rest) x100 */
extern unsigned long long dsp_calib2_vp6_res[DSP_CALIB2_COUNT];

/* ---- v1007 round 3: CAL2 said the standalone kernels are cheap (hp.row
 *      20.77, vp.chunk 83.4) while production tiles cost ~2.4M cycles —
 *      CAL3 times the REAL orb_blur_kernel_ivp / fast9_collect_cell in
 *      production shape to separate kernel cost from environment. ---- */
enum {
    DSP_CALIB3_KTILE = 0,    /* real kernel on interior 72x72 -> 64x64
                              * tiles, no DMA: cycles per TILE              */
    DSP_CALIB3_KTILE_DMA,    /* same + the driver's ping-pong DMA overlap
                              * (DMA-in of t+1 and DMA-out of t in flight
                              * during compute), cycles per TILE            */
    DSP_CALIB3_SCANBYTE,     /* real fast9_collect_cell scan loop on a bank
                              * band, scoremap-like mix: cycles per BYTE    */
    DSP_CALIB3_COUNT
};

extern unsigned long long dsp_calib3_vp6_res[DSP_CALIB3_COUNT];

/* ---- v1015 round 4: decompose the collect scan before vectorizing it.
 *      CAL3 says the real loop costs 39.5 cyc/byte on a synthetic mix;
 *      CAL4 separates the base loop from the candidate tail and prices the
 *      vector skip that would replace it. ---- */
enum {
    DSP_CALIB4_SCAN_ZERO = 0, /* real fast9_collect_cell on an all-NONE band
                               * (no candidates, no NMS): pure base-loop
                               * cycles per BYTE                              */
    DSP_CALIB4_SCAN_VEC,      /* vector threshold-skip prototype on the same
                               * synthetic mix (per 64B chunk: streaming
                               * load + skip-lane masks + select/scratch/
                               * scalar-OR all-skip test; hit chunks fall to
                               * the byte loop): cycles per BYTE           */
    DSP_CALIB4_COUNT
};

extern unsigned long long dsp_calib4_vp6_res[DSP_CALIB4_COUNT];

/* Run the calibration once (safe to call repeatedly: internally guarded).
 * Buffers come from the tile manager bank pool + malloc heap; everything
 * is released before returning. Non-Xtensa builds: no-op. */
void dsp_calib_vp6_run(xvTileManager *tm);

#ifdef __cplusplus
}
#endif
#endif /* DSP_CALIB_VP6_H */
