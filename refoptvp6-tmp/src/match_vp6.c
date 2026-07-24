/*
 * match_vp6.c - VP6 batch descriptor matching driver (P6 TileManager).
 *
 * Pipeline:
 *   DMA all Q queries into SRAM (qtile, once).
 *   for each train tile t (32xB, ping/pong):
 *       prefetch tile t+1                 ──┐  (overlap: DMA-in of next tile)
 *       xvWaitForTile(in[cur])            ──┤
 *       match_tile_kernel(qtile, in[cur]) ──┤  Hamming + best1/best2/idx1 update
 *                                         ──┘  (no DMA-out: 3 ints/query to DRAM)
 *
 * Each train descriptor is DMA'd once; the Q queries once. The kernel reads
 * both from SRAM and updates the per-query best1/best2/idx1 in DRAM (the query
 * best arrays persist across train tiles). Bit-exact with the per-query
 * match_best_kernel (== search_by_bow_core inner loop).
 *
 * Mirrors orb_blur_vp6.c / fast9_vp6.c (the "tiles" are 32xB descriptor slices
 * of a 32xN "image", not image regions).
 */
#include "match_vp6.h"

#include <stdlib.h>

#define MATCH_MIN(a, b) (((a) < (b)) ? (a) : (b))

int match_vp6_process(xvTileManager *tm,
                      xvFrame *query_frame, xvFrame *train_frame,
                      int Q, int N,
                      int *out_dist1, int *out_idx1, int *out_dist2,
                      const MatchVp6Config *cfg)
{
    MatchVp6Config def = MATCH_VP6_DEFAULT_CONFIG;
    MatchVp6Config c = cfg ? *cfg : def;

    const int MAXQ = c.max_queries;
    const int B    = c.train_tile_b;

    if (!tm || !out_dist1 || !out_idx1 || !out_dist2) return -1;
    if (MAXQ <= 0 || B <= 0) return -1;
    if (Q < 0 || N < 0) return -1;
    if (Q > MAXQ) return -1;
    if (Q == 0 || N == 0) {
        for (int q = 0; q < Q; q++) { out_dist1[q] = 256; out_dist2[q] = 256; out_idx1[q] = -1; }
        return 0;
    }

    /* init outputs (256/256/-1 = no match yet) */
    for (int q = 0; q < Q; q++) { out_dist1[q] = 256; out_dist2[q] = 256; out_idx1[q] = -1; }

    int ret = 0;

    /* qtile: DMA all Q queries (32xQ) into SRAM, once. */
    xvTile *qtile = xvCreateTile(tm, MATCH_DESC_SIZE * MAXQ, MATCH_DESC_SIZE, MAXQ,
                                 MATCH_DESC_SIZE, 0, 0, XV_MEM_BANK_COLOR_0,
                                 query_frame, XV_TILE_U8, DATA_ALIGNED_32);
    if (!qtile) return -2;
    XV_TILE_UPDATE_DIMENSIONS(qtile, 0, 0, MATCH_DESC_SIZE, Q, MATCH_DESC_SIZE);
    xvReqTileTransferIn(tm, qtile, NULL, 0);
    xvWaitForTile(tm, qtile);

    /* train tiles: 32xB slices of the 32xN train image, ping/pong. */
    const int n_tiles = (N + B - 1) / B;
    xvTile *in[2] = { NULL, NULL };
    in[0] = xvCreateTile(tm, MATCH_DESC_SIZE * B, MATCH_DESC_SIZE, B,
                         MATCH_DESC_SIZE, 0, 0, XV_MEM_BANK_COLOR_0,
                         train_frame, XV_TILE_U8, DATA_ALIGNED_32);
    in[1] = xvCreateTile(tm, MATCH_DESC_SIZE * B, MATCH_DESC_SIZE, B,
                         MATCH_DESC_SIZE, 0, 0, XV_MEM_BANK_COLOR_1,
                         train_frame, XV_TILE_U8, DATA_ALIGNED_32);
    if (!in[0] || !in[1]) { xvFreeTile(tm, qtile); return -2; }

    const uint8_t *qdata = (const uint8_t *)XV_TILE_GET_DATA_PTR(qtile);

    /* prefetch train tile 0 (rows 0..b0-1) */
    {
        int b0 = MATCH_MIN(B, N);
        XV_TILE_UPDATE_DIMENSIONS(in[0], 0, 0, MATCH_DESC_SIZE, b0, MATCH_DESC_SIZE);
        xvReqTileTransferIn(tm, in[0], NULL, 0);
    }

    for (int t = 0; t < n_tiles; t++) {
        const int cur = t & 1;
        const int nxt = 1 - cur;
        int b_cur = MATCH_MIN(B, N - t * B);

        /* prefetch tile t+1 */
        if (t + 1 < n_tiles) {
            int b_next = MATCH_MIN(B, N - (t + 1) * B);
            XV_TILE_UPDATE_DIMENSIONS(in[nxt], 0, (t + 1) * B, MATCH_DESC_SIZE, b_next, MATCH_DESC_SIZE);
            xvReqTileTransferIn(tm, in[nxt], NULL, 0);
        }

        xvWaitForTile(tm, in[cur]);

        /* update per-query best1/best2/idx1 over this train tile (global idx
         * t*B + j). Reads qtile (SRAM) + train tile (SRAM), writes DRAM. */
        match_tile_kernel(qdata, Q,
                          (const uint8_t *)XV_TILE_GET_DATA_PTR(in[cur]), b_cur, t * B,
                          out_dist1, out_dist2, out_idx1);
    }

    xvFreeTile(tm, in[0]);  xvFreeTile(tm, in[1]);
    xvFreeTile(tm, qtile);
    return ret;
}
