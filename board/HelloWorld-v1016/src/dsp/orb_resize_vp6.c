/*
 * orb_resize_vp6.c - VP6 tile-based bilinear resize driver (P6 TileManager).
 *
 * Pipeline (per output tile, raster order, ping/pong double-buffered):
 *
 *   precompute tables for tile t+1 ──┐
 *   prepare in[nxt] for tile t+1  ──┤
 *   xvWaitForTile(in[cur])        ──┤ wait for tile t's source DMA
 *   wait out[cur] free (if reused) ─┤
 *   orb_resize_kernel(in[cur]->out)─┤ run validated kernel on SRAM data
 *   xvReqTileTransferOut(out[cur]) ─┘
 *
 * The source region DMA'd per tile is the 2-row vertical kernel extent
 * (the rows referenced by yofs for this tile's output rows, clamped to the
 * frame). Edge clamp is used (no reflect-101 needed for bilinear resize).
 *
 * Mirrors orb_blur_vp6.c. The per-tile maths is the same orb_resize_kernel()
 * the PC reference (orb_resize_full) calls.
 */
#include "orb_resize_vp6.h"

#include <stdlib.h>

#define ORB_RESIZE_MIN(a, b) (((a) < (b)) ? (a) : (b))

/* Translate a linear tile index to (tu0, tv0) dest pixel coords. */
static void orb_resize_tile_xy(int t, int n_tiles_x, int tile_w, int tile_h,
                               int *tu0, int *tv0)
{
    *tv0 = (t / n_tiles_x) * tile_h;
    *tu0 = (t % n_tiles_x) * tile_w;
}

/* Prepare an input tile for the source region [mn_y, mx_y] rows of the
 * source frame. Returns 0 ok, -1 if the region exceeds the tile buffer. */
static int orb_resize_prepare_input(xvTile *t, int mn_y, int mx_y,
                                    int sw, int max_w, int max_h, int pitch)
{
    int h = mx_y - mn_y + 1;
    if (sw > max_w || h > max_h) return -1;
    XV_TILE_UPDATE_DIMENSIONS(t, 0, mn_y, sw, h, pitch);
    return 0;
}

int orb_resize_vp6_process(xvTileManager *tm,
                           xvFrame *src_frame, xvFrame *dst_frame,
                           int sw, int sh, int dw, int dh,
                           const OrbResizeVp6Config *cfg)
{
    OrbResizeVp6Config def = ORB_RESIZE_VP6_DEFAULT_CONFIG;
    OrbResizeVp6Config c = cfg ? *cfg : def;

    const int TW  = c.tile_w;
    const int TH  = c.tile_h;
    const int MSW = c.max_src_tile_w;
    const int MSH = c.max_src_tile_h;

    if (!tm || !src_frame || !dst_frame) return -1;
    if (TW <= 0 || TH <= 0 || MSW <= 0 || MSH <= 0) return -1;
    if (MSW < sw) return -1;  /* source tile must hold full image width */

    const int n_tiles_x = (dw + TW - 1) / TW;
    const int n_tiles_y = (dh + TH - 1) / TH;
    const int N         = n_tiles_x * n_tiles_y;

    /* allocate ping/pong input and output tiles */
    const int in_buf  = MSW * MSH;
    const int out_buf = TW  * TH;
    xvTile *in[2]  = { NULL, NULL };
    xvTile *out[2] = { NULL, NULL };
    in[0]  = xvCreateTile(tm, in_buf,  MSW, MSH, MSW, 0, 0, XV_MEM_BANK_COLOR_0, src_frame, XV_TILE_U8, DATA_ALIGNED_32);
    in[1]  = xvCreateTile(tm, in_buf,  MSW, MSH, MSW, 0, 0, XV_MEM_BANK_COLOR_1, src_frame, XV_TILE_U8, DATA_ALIGNED_32);
    out[0] = xvCreateTile(tm, out_buf, TW,  TH,  TW,  0, 0, XV_MEM_BANK_COLOR_0, dst_frame, XV_TILE_U8, DATA_ALIGNED_32);
    out[1] = xvCreateTile(tm, out_buf, TW,  TH,  TW,  0, 0, XV_MEM_BANK_COLOR_1, dst_frame, XV_TILE_U8, DATA_ALIGNED_32);
    if (!in[0] || !in[1] || !out[0] || !out[1]) return -2;

    /* precomputed tables: horizontal for full dest width (shared across tiles in
     * a column), vertical for one tile's rows (recomputed per tile row). */
    int   *xofs   = (int*)malloc(sizeof(int) * dw);
    short *ialpha = (short*)malloc(sizeof(short) * dw * 2);
    int   *yofs   = (int*)malloc(sizeof(int) * TH);
    short *ibeta  = (short*)malloc(sizeof(short) * TH * 2);
    if (!xofs || !ialpha || !yofs || !ibeta) {
        xvFreeTile(tm, in[0]); xvFreeTile(tm, in[1]);
        xvFreeTile(tm, out[0]); xvFreeTile(tm, out[1]);
        free(xofs); free(ialpha); free(yofs); free(ibeta);
        return -2;
    }

    int ret = 0;

    #define RESIZE_BBOX(tv, th2, mny, mxy) \
        orb_resize_tile_source_bbox(0, (tv), (int)(sw), (int)(th2), (int)(sw), (int)(sh), (int)(dw), (int)(dh), (mny), (mxy))

    /* prefetch tile 0 source rows */
    {
        int tu, tv; orb_resize_tile_xy(0, n_tiles_x, TW, TH, &tu, &tv);
        int th2 = ORB_RESIZE_MIN(TH, dh - tv);
        int mny, mxy;
        RESIZE_BBOX(tv, th2, &mny, &mxy);
        if (orb_resize_prepare_input(in[0], mny, mxy, sw, MSW, MSH, MSW) != 0) { ret = -3; goto done; }
        xvReqTileTransferIn(tm, in[0], NULL, 0);
    }

    /* precompute horizontal tables once (same for all tiles in a column) */
    orb_resize_precompute_h(NULL, sw, sh, dw, dh, 0, dw, xofs, ialpha);

    for (int t = 0; t < N; t++) {
        const int cur = t & 1;
        const int nxt = 1 - cur;

        int tu, tv; orb_resize_tile_xy(t, n_tiles_x, TW, TH, &tu, &tv);
        int tw2 = ORB_RESIZE_MIN(TW, dw - tu), th2 = ORB_RESIZE_MIN(TH, dh - tv);
        int mny, mxy;
        RESIZE_BBOX(tv, th2, &mny, &mxy);

        /* precompute vertical tables for this tile's rows */
        orb_resize_precompute_v(NULL, sw, sh, dw, dh, tv, th2, yofs, ibeta);

        /* prefetch tile t+1 source rows */
        if (t + 1 < N) {
            int tu1, tv1; orb_resize_tile_xy(t + 1, n_tiles_x, TW, TH, &tu1, &tv1);
            int th3 = ORB_RESIZE_MIN(TH, dh - tv1);
            int n_mny, n_mxy;
            RESIZE_BBOX(tv1, th3, &n_mny, &n_mxy);
            if (orb_resize_prepare_input(in[nxt], n_mny, n_mxy, sw, MSW, MSH, MSW) != 0) { ret = -3; goto done; }
            xvReqTileTransferIn(tm, in[nxt], NULL, 0);
        }

        xvWaitForTile(tm, in[cur]);

        if (t >= 2) xvWaitForTile(tm, out[cur]);

        int region_h = mxy - mny + 1;
        XV_TILE_UPDATE_DIMENSIONS(out[cur], tu, tv, tw2, th2, TW);
        orb_resize_kernel((const uint8_t *)XV_TILE_GET_DATA_PTR(in[cur]), XV_TILE_GET_PITCH(in[cur]),
                          0, mny, region_h,
                          sw, sh, dw, dh,
                          tu, tv, tw2, th2,
                          (uint8_t *)XV_TILE_GET_DATA_PTR(out[cur]), XV_TILE_GET_PITCH(out[cur]),
                          xofs + tu, ialpha + tu * 2,
                          yofs, ibeta);
        xvReqTileTransferOut(tm, out[cur], 0);
    }

    /* drain in-flight DMA-out */
    xvWaitForTile(tm, out[(N - 1) & 1]);
    if (N >= 2) xvWaitForTile(tm, out[(N - 2) & 1]);

done:
    free(xofs); free(ialpha); free(yofs); free(ibeta);
    xvFreeTile(tm, in[0]);  xvFreeTile(tm, in[1]);
    xvFreeTile(tm, out[0]); xvFreeTile(tm, out[1]);
    return ret;
    #undef RESIZE_BBOX
}
