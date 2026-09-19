/*
 * orb_resize_strip_vp6.h - IVP strip driver for the ORB pyramid resize
 *                          (bit-exact replacement for orb_resize_full).
 *
 * Moves the whole bilinear downscale through the TileManager pattern used by
 * the other VP6 ORB drivers: source rows stream into a ping/pong SRAM strip
 * pair via raw 2D iDMA (overlap: pass p+1's DMA is scheduled before pass p's
 * kernel runs), the horizontal 2-tap runs as a 64-lane gather + per-lane
 * weighted MAC, the vertical 2-tap + rounding runs as scalar-weighted 24-bit
 * MAC chains over two byte planes - no per-pixel DRAM reads, no scalar MAC
 * loop. All SRAM buffers come from memory bank 1 (bank 0 is nearly full with
 * the other four drivers' strips) and persist across calls (grow-reuse).
 *
 * Bit-exactness: the output is byte-identical to orb_resize_kernel for every
 * input (verified by test_orb_resize_vp6.c). The integer identities that make
 * the IVP decomposition exact are documented in orb_resize_strip_vp6.c.
 */
#ifndef ORB_RESIZE_STRIP_VP6_H
#define ORB_RESIZE_STRIP_VP6_H

#include <stdint.h>

#include "tileManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Resize src (sw x sh, pitch = sw, tight) into dst (dw x dh, pitch = dw).
 * Returns 0 on success; -1 bad args; -2 SRAM/DMA resource failure (caller
 * should fall back to orb_resize_full - the outputs are interchangeable).
 * Buffers persist across calls (grow-reuse); orb_resize_strip_vp6_release()
 * frees them. dsp_orb_vp6 releases after each pyramid so the four pixel
 * drivers own bank 1 during extraction.
 */
int orb_resize_strip_vp6(xvTileManager *tm, const uint8_t *src, int sw, int sh,
                         uint8_t *dst, int dw, int dh);

/* Release the persistent bank-1 buffers (safe when never allocated). */
void orb_resize_strip_vp6_release(void);

#ifdef __cplusplus
}
#endif
#endif /* ORB_RESIZE_STRIP_VP6_H */
