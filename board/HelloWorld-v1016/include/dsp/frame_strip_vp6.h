/*
 * frame_strip_vp6.h - shared full-width strip sweep for the VP6 ORB drivers
 *                     (fast9_scoremap_vp6 / ic_angle_vp6 / brief_vp6).
 *
 * This is the ExtractImage.cpp / TileMemMgr.cpp (sgbmwarpFace/WarpRGBA)
 * runtime pattern, applied to ORB:
 *
 *   - ALL SRAM strip buffers are allocated ONCE (xvAllocateBuffer, ping in
 *     memory bank 0, pong in bank 1, 64B aligned) and reused across frames,
 *     pyramid levels and process() calls - no per-call xvCreateTile/
 *     xvFreeTile, no malloc.
 *   - At runtime each strip is moved with a raw 2D iDMA descriptor
 *     (xvAddIdmaRequest + xvWaitForiDMA) whose parameters are computed inline
 *     from the frame pointer/pitch - no tile-queue bookkeeping. The DMA of
 *     pass p+1 is scheduled before the kernel of pass p runs (ping/pong
 *     overlap), exactly like ResizeMain()'s pre-schedule loop.
 *
 * The sweep covers a frame of w x h pixels in passes: pass p is ASSIGNED the
 * frame rows [top, top+rows); the SRAM strip additionally carries `halo` rows
 * above and below (clamped to the frame), so any consumer whose reads extend
 * at most `halo` rows beyond an assigned row (FAST-9 ring: 3, IC angle disk:
 * 15, rBRIEF rotated patch: 19) finds every clamped read inside the strip.
 * Columns always span the full frame width [0, w).
 *
 * Per level the strip pitch is ALIGN64(w) (64B rows): smaller pyramid levels
 * get more SRAM rows per pass from the same buffer, so they finish in fewer
 * passes (a 320-wide level holds ~2x the rows of a 640-wide one).
 *
 * Bit-exactness: the strip contains a byte-identical copy of the frame rows
 * it covers (the iDMA is a plain 2D copy), and every kernel in the consumers
 * indexes its source purely as (clamped_coord - origin) * pitch with origin =
 * the strip's first frame row - the same memory cells the full-image path
 * reads. No kernel maths change.
 *
 * Single tile manager, single thread (same assumptions as the drivers). Each
 * driver owns its fs_bufs as a file-static; the buffers grow if a larger
 * frame arrives (free + re-allocate, still once per size class, not per call).
 */
#ifndef FRAME_STRIP_VP6_H
#define FRAME_STRIP_VP6_H

#include <stddef.h>
#include <stdint.h>

#include "tileManager.h"    /* xvAllocateBuffer/xvFreeBuffer/xvAddIdmaRequest/... */

#ifdef __cplusplus
extern "C" {
#endif

/* round up to a multiple of 64 */
#define FS_ALIGN64(n) (((n) + 63) & ~63)

/* Persistent ping/pong strip pair owned by ONE driver. */
typedef struct {
    xvTileManager *tm;      /* TM the buffers were allocated from       */
    uint8_t *buf[2];        /* ping: bank0, pong: bank1, 64B aligned    */
    int32_t  bytes;         /* allocated bytes per buffer               */
} fs_bufs;

/* Release the pair (safe when never allocated). Pass the TM used at alloc. */
static inline void fs_free(fs_bufs *b)
{
    if (b->tm) {
        if (b->buf[0]) xvFreeBuffer(b->tm, b->buf[0]);
        if (b->buf[1]) xvFreeBuffer(b->tm, b->buf[1]);
    }
    b->tm = NULL; b->buf[0] = NULL; b->buf[1] = NULL; b->bytes = 0;
}

/*
 * Allocate (or keep / grow) the ping/pong pair so each buffer holds `bytes`
 * bytes. Buffers are only re-allocated when the TM changes or the request
 * grows; a same-or-smaller request reuses what is already resident.
 * Returns 0 ok, -1 bad args, -2 allocation failed.
 */
static inline int fs_alloc(fs_bufs *b, xvTileManager *tm, int32_t bytes)
{
    if (!tm || bytes <= 0) return -1;
    if (b->tm == tm && b->bytes >= bytes) return 0;    /* resident, reuse  */
    fs_free(b);
    b->buf[0] = (uint8_t *)xvAllocateBuffer(tm, bytes, XV_MEM_BANK_COLOR_0, 64);
    b->buf[1] = (uint8_t *)xvAllocateBuffer(tm, bytes, XV_MEM_BANK_COLOR_1, 64);
    if (!b->buf[0] || !b->buf[1] || b->buf[0] == (void *)(intptr_t)XVTM_ERROR
        || b->buf[1] == (void *)(intptr_t)XVTM_ERROR) {
        fs_free(b);
        return -2;
    }
    b->tm = tm;
    b->bytes = bytes;
    return 0;
}

/* One pass's view of the SRAM strip. */
typedef struct {
    const uint8_t *data;    /* strip base = frame row srow0, column 0     */
    int pitch;              /* strip pitch in bytes (>= w, 64B multiple)  */
    int srow0;              /* frame row of data[0] (pass origin_y)       */
    int srows;              /* strip rows in SRAM (incl. halo)            */
    int top;                /* first frame row ASSIGNED to this pass      */
    int rows;               /* assigned rows [top, top+rows)              */
} fs_view;

/* Sweep cursor over one frame (one driver level). */
typedef struct {
    fs_bufs *b;
    uint8_t *fdata;         /* frame base (DRAM)                          */
    int fpitch;             /* frame pitch in bytes                       */
    int w, h, halo;
    int pitch;              /* strip pitch used for this frame            */
    int eff;                /* assigned rows per pass                     */
    int pass, n_pass;
    int32_t dma[2];         /* outstanding dmaIndex per buffer            */
    int pend[2];            /* dma[i] valid                                */
    /* strip geometry stashed at issue time, indexed by buffer (pass&1)    */
    int srow[2], srw[2], top_[2], rw[2];
} fs_sweep;

/* Submit the strip DMA for pass p into buffer (p&1). */
static inline void fs_issue(fs_sweep *s, int p)
{
    const int top  = p * s->eff;
    const int rows = (s->h - top < s->eff) ? (s->h - top) : s->eff;
    int srow0 = top - s->halo;              if (srow0 < 0) srow0 = 0;
    int srow1 = top + rows - 1 + s->halo;   if (srow1 > s->h - 1) srow1 = s->h - 1;
    const int srows = srow1 - srow0 + 1;
    const int i = p & 1;

    s->dma[i] = xvAddIdmaRequest(s->b->tm, s->b->buf[i],
                                 (void *)(s->fdata + (size_t)srow0 * s->fpitch),
                                 (size_t)s->w, srows, s->fpitch, s->pitch, 0);
    s->pend[i] = 1;
    s->srow[i] = srow0; s->srw[i] = srows; s->top_[i] = top; s->rw[i] = rows;
}

/*
 * Start a sweep of the frame: passes of up to (buffer_rows - 2*halo) assigned
 * rows each. The first strip DMA is scheduled immediately; call fs_sweep_next
 * until it returns 0. `b` must have been fs_alloc'ed with at least
 * FS_ALIGN64(w) * (2*halo + 1) bytes.
 */
static inline void fs_sweep_begin(fs_sweep *s, fs_bufs *b, const xvFrame *fr,
                                  int w, int h, int halo)
{
    s->b = b;
    s->fdata = (uint8_t *)XV_FRAME_GET_DATA_PTR((xvFrame *)fr);
    s->fpitch = XV_FRAME_GET_PITCH_IN_BYTES((xvFrame *)fr);
    s->w = w; s->h = h; s->halo = halo;
    s->pitch = FS_ALIGN64(w);
    int cap = (int)(b->bytes / (int32_t)s->pitch);   /* max SRAM rows      */
    int eff = cap - 2 * halo;                        /* assigned rows/pass */
    if (eff < 1) eff = 1;
    if (eff > h) eff = h;
    s->eff = eff;
    s->n_pass = (h + eff - 1) / eff;
    s->pass = 0;
    s->pend[0] = s->pend[1] = 0;
    if (s->n_pass > 0) fs_issue(s, 0);
}

/*
 * Advance to the next pass. Schedules the NEXT strip's DMA first (overlap),
 * then waits for the CURRENT strip, then fills *v. Returns 1 with a valid
 * view, 0 when the frame is done.
 */
static inline int fs_sweep_next(fs_sweep *s, fs_view *v)
{
    if (s->pass >= s->n_pass) return 0;
    const int cur = s->pass & 1;
    if (s->pass + 1 < s->n_pass) fs_issue(s, s->pass + 1);   /* prefetch   */
    if (s->pend[cur]) {
        if (s->dma[cur] < 0) {          /* xvAddIdmaRequest failed: stop */
            s->pass = s->n_pass;
            return 0;
        }
        xvWaitForiDMA(s->b->tm, s->dma[cur]);
        s->pend[cur] = 0;
    }
    v->data  = s->b->buf[cur];
    v->pitch = s->pitch;
    v->srow0 = s->srow[cur];
    v->srows = s->srw[cur];
    v->top   = s->top_[cur];
    v->rows  = s->rw[cur];
    ++s->pass;
    return 1;
}

#ifdef __cplusplus
}
#endif
#endif /* FRAME_STRIP_VP6_H */
