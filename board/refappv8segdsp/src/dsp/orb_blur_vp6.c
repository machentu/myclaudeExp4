/*
 * orb_blur_vp6.c - VP6 tile-based ORB Gaussian blur driver (P6 TileManager / xvTile API).
 *
 * Pipeline (per output tile, raster order, ping/pong double-buffered):
 *
 *   prepare in[1-cur] for tile t+1  ──┐  (overlap: DMA-in of next tile)
 *   xvWaitForTile(in[cur])          ──┤  wait for tile t's source DMA
 *   wait out[cur] free (if reused) ──┤  drain tile t-2's DMA-out
 *   orb_blur_kernel(in[cur]->out)  ──┤  run validated kernel on SRAM data
 *   xvReqTileTransferOut(out[cur]) ──┘  (overlap: DMA-out of this tile)
 *
 * The source region DMA'd per tile is the 7-tap neighbour extent
 * (output tile +/- 3, clamped to the frame, returned by
 * orb_blur_tile_source_bbox). reflect-101 maps any out-of-frame tap back into
 * the frame, so the region is clamped to the frame (EDGE=0) and the +/-3
 * neighbour is real frame data - bit-exact with the PC full-image transform
 * (test_orb_blur_vp6.c). A blur tile always has a valid source region (no
 * all-invalid case, unlike IPM), so there is no zero-fill branch.
 *
 * Mirrors vp6ref/vp6/ipm_vp6.c. The per-tile maths is the same
 * orb_blur_kernel() the PC reference (orb_blur_full) calls, so the result is
 * bit-exact with refactor's ngd_gaussian_blur by construction.
 *
 * Build: plain C on PC, or as C++ under the Xtensa cstub (the cstub TIE stubs
 * are C++ and the IVP vector types only match in C++ mode - see CMakeLists.txt,
 * which compiles all .c as CXX, mirroring vp6ref/vp6/CMakeLists.txt).
 */
#include "orb_blur_vp6.h"

#include <stdlib.h>

/* ---- IVP intrinsic kernel (optional). Falls back to the validated portable
 *      orb_blur_kernel from orb_blur_core.c when ORB_BLUR_USE_IVP_INTRIN is
 *      not set. The IVP path is NOT built by default; it is a hook for a
 *      hand-tuned xt_ivpn.h implementation validated separately against
 *      orb_blur_kernel (the test comparison will flag any divergence). ---- */
#ifdef ORB_BLUR_USE_IVP_INTRIN
extern void orb_blur_kernel_ivp(const uint8_t *src_data, int src_pitch,
                                int origin_x, int origin_y, int region_h,
                                int frame_w, int frame_h,
                                int tu0, int tv0, int dst_w, int dst_h,
                                uint8_t *dst, int dst_pitch,
                                uint8_t *tmp, int tmp_pitch);
#define ORB_BLUR_KERNEL orb_blur_kernel_ivp
#else
#define ORB_BLUR_KERNEL orb_blur_kernel
#endif

#define ORB_BLUR_MIN(a, b) (((a) < (b)) ? (a) : (b))

/* Translate a linear tile index to (tu0, tv0) frame pixel coords (top-left). */
static void orb_blur_tile_xy(int t, int n_tiles_x, int tile_w, int tile_h,
                             int *tu0, int *tv0)
{
    *tv0 = (t / n_tiles_x) * tile_h;
    *tu0 = (t % n_tiles_x) * tile_w;
}

/* Prepare an input tile for the source region [mn_x,mn_y]-[mx_x,mx_y] of the
 * source frame. Returns 0 ok, -1 if the region exceeds the tile buffer. */
static int orb_blur_prepare_input(xvTile *t, int mn_x, int mn_y, int mx_x, int mx_y,
                                  int max_w, int max_h, int pitch)
{
    int w = mx_x - mn_x + 1;
    int h = mx_y - mn_y + 1;
    if (w > max_w || h > max_h) return -1;
    XV_TILE_UPDATE_DIMENSIONS(t, mn_x, mn_y, w, h, pitch);
    return 0;
}

int orb_blur_vp6_process(xvTileManager *tm,
                         xvFrame *src_frame, xvFrame *dst_frame,
                         int src_w, int src_h,
                         const OrbBlurVp6Config *cfg)
{
    OrbBlurVp6Config def = ORB_BLUR_VP6_DEFAULT_CONFIG;
    OrbBlurVp6Config c = cfg ? *cfg : def;

    const int TW  = c.tile_w;
    const int TH  = c.tile_h;
    const int MSW = c.max_src_tile_w;
    const int MSH = c.max_src_tile_h;

    if (!tm || !src_frame || !dst_frame) return -1;
    if (TW <= 0 || TH <= 0 || MSW <= 0 || MSH <= 0) return -1;
    /* an interior tile's 7-tap source region is (TW+6) x (TH+6); the per-tile
     * buffer must hold that. */
    if (MSW < TW + 6 || MSH < TH + 6) return -1;

    const int n_tiles_x = (src_w + TW - 1) / TW;
    const int n_tiles_y = (src_h + TH - 1) / TH;
    const int N         = n_tiles_x * n_tiles_y;

    /* allocate ping/pong input (source region) and output tiles */
    const int in_buf  = MSW * MSH;
    const int out_buf = TW  * TH;
    xvTile *in[2]  = { NULL, NULL };
    xvTile *out[2] = { NULL, NULL };
    in[0]  = xvCreateTile(tm, in_buf,  MSW, MSH, MSW, 0, 0, XV_MEM_BANK_COLOR_0, src_frame, XV_TILE_U8, DATA_ALIGNED_32);
    in[1]  = xvCreateTile(tm, in_buf,  MSW, MSH, MSW, 0, 0, XV_MEM_BANK_COLOR_1, src_frame, XV_TILE_U8, DATA_ALIGNED_32);
    out[0] = xvCreateTile(tm, out_buf, TW,  TH,  TW,  0, 0, XV_MEM_BANK_COLOR_0, dst_frame, XV_TILE_U8, DATA_ALIGNED_32);
    out[1] = xvCreateTile(tm, out_buf, TW,  TH,  TW,  0, 0, XV_MEM_BANK_COLOR_1, dst_frame, XV_TILE_U8, DATA_ALIGNED_32);
    if (!in[0] || !in[1] || !out[0] || !out[1]) return -2;

    /* scratch for the horizontal->vertical intermediate (portable path only).
     * Holds region_h (<= MSH) rows of width TW as uint16_t raw horizontal sums.
     * On the DSP this is local SRAM; under cstub it is host memory. */
    uint16_t *tmp = (uint16_t *)malloc((size_t)MSH * TW * sizeof(uint16_t));
    if (!tmp) {
        xvFreeTile(tm, in[0]); xvFreeTile(tm, in[1]);
        xvFreeTile(tm, out[0]); xvFreeTile(tm, out[1]);
        return -2;
    }

    int ret = 0;

    /* helper to compute a tile's source bbox (always valid for a blur tile) */
    #define BLUR_BBOX(tu, tv, tw, th, mnx, mny, mxx, mxy) \
        orb_blur_tile_source_bbox((tu), (tv), (tw), (th), src_w, src_h, (mnx), (mny), (mxx), (mxy))

    /* prefetch tile 0 source region */
    {
        int tu, tv; orb_blur_tile_xy(0, n_tiles_x, TW, TH, &tu, &tv);
        int tw = ORB_BLUR_MIN(TW, src_w - tu), th = ORB_BLUR_MIN(TH, src_h - tv);
        int mnx, mny, mxx, mxy;
        BLUR_BBOX(tu, tv, tw, th, &mnx, &mny, &mxx, &mxy);
        if (orb_blur_prepare_input(in[0], mnx, mny, mxx, mxy, MSW, MSH, MSW) != 0) { ret = -3; goto done; }
        xvReqTileTransferIn(tm, in[0], NULL, 0);
    }

    for (int t = 0; t < N; t++) {
        const int cur = t & 1;          /* ping/pong index for the current tile */
        const int nxt = 1 - cur;        /* the other buffer, used to prefetch t+1 */

        int tu, tv; orb_blur_tile_xy(t, n_tiles_x, TW, TH, &tu, &tv);
        int tw = ORB_BLUR_MIN(TW, src_w - tu), th = ORB_BLUR_MIN(TH, src_h - tv);
        int mnx, mny, mxx, mxy;
        BLUR_BBOX(tu, tv, tw, th, &mnx, &mny, &mxx, &mxy);
        int region_h = mxy - mny + 1;   /* source rows present in SRAM */

        /* prefetch tile t+1 source region into the other input buffer (overlap) */
        if (t + 1 < N) {
            int tu1, tv1; orb_blur_tile_xy(t + 1, n_tiles_x, TW, TH, &tu1, &tv1);
            int tw1 = ORB_BLUR_MIN(TW, src_w - tu1), th1 = ORB_BLUR_MIN(TH, src_h - tv1);
            int n_mnx, n_mny, n_mxx, n_mxy;
            BLUR_BBOX(tu1, tv1, tw1, th1, &n_mnx, &n_mny, &n_mxx, &n_mxy);
            if (orb_blur_prepare_input(in[nxt], n_mnx, n_mny, n_mxx, n_mxy, MSW, MSH, MSW) != 0) { ret = -3; goto done; }
            /* pPrevTile=NULL: no cross-tile row reuse (safe; enable for
             * vertically-adjacent tiles once DMA ordering is validated). */
            xvReqTileTransferIn(tm, in[nxt], NULL, 0);
        }

        xvWaitForTile(tm, in[cur]);          /* wait for this tile's source DMA */

        /* reuse safety: ensure out[cur]'s previous DMA-out (from t-2) is done */
        if (t >= 2) xvWaitForTile(tm, out[cur]);

        XV_TILE_UPDATE_DIMENSIONS(out[cur], tu, tv, tw, th, TW);
        ORB_BLUR_KERNEL((const uint8_t *)XV_TILE_GET_DATA_PTR(in[cur]), XV_TILE_GET_PITCH(in[cur]),
                        mnx, mny, region_h,
                        src_w, src_h,
                        tu, tv, tw, th,
                        (uint8_t *)XV_TILE_GET_DATA_PTR(out[cur]), XV_TILE_GET_PITCH(out[cur]),
                        tmp, TW);
        xvReqTileTransferOut(tm, out[cur], 0);
    }

    /* drain in-flight DMA-out: at most two output buffers can be outstanding
     * (out[cur] from the last tile and out[nxt] from the tile before it). */
    xvWaitForTile(tm, out[(N - 1) & 1]);
    if (N >= 2) xvWaitForTile(tm, out[(N - 2) & 1]);

done:
    free(tmp);
    xvFreeTile(tm, in[0]);  xvFreeTile(tm, in[1]);
    xvFreeTile(tm, out[0]); xvFreeTile(tm, out[1]);
    return ret;
    #undef BLUR_BBOX
}
