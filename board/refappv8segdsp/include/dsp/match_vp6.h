/*
 * match_vp6.h - VP6 batch ORB descriptor matching (Cadence P6 TileManager /
 *               xvTile API).
 *
 * For Q query descriptors and N train descriptors (each 32 bytes), finds the
 * best (min Hamming) and second-best train match per query. The Q queries are
 * DMA'd once into SRAM (a 32xQ tile); the N train descriptors are sliced into
 * 32xB tiles (ping/pong DMA). For each train tile, match_tile_kernel() runs
 * over all Q queries, computing Hamming (1:1 with ngd_descriptor_distance) and
 * updating each query's best1/best2/idx1 (1:1 with search_by_bow_core's inner
 * loop). Each train descriptor is DMA'd exactly once; each query once.
 *
 * This is the matching inner loop all refactor matcher functions (SearchByBoW,
 * SearchByProjection, SearchForTriangulation, Fuse, SearchBySim3) share. The
 * BoW/geometric candidate filtering those do reduces N per query; the caller
 * packs the filtered candidates into a dense train array and calls this. The
 * xtensa ORBforwardKeypointMatching_U16 vectorises the same Hamming (IVP
 * POPC2NX8) + min-reduction - the IVP fast-path hook is future work; the
 * portable popcount here is bit-exact and auto-vectorisable.
 *
 * Mirrors orb_blur_vp6.h / fast9_vp6.h. See README.md.
 */
#ifndef MATCH_VP6_H
#define MATCH_VP6_H

#include "match_core.h"
#include "tileManager.h"   /* P6 TileManager (project-provided) */

#ifdef __cplusplus
extern "C" {
#endif

/* VP6 match config.
 *   max_queries   : max Q the query SRAM tile must hold (>= actual Q).
 *   train_tile_b  : train descriptors per DMA tile (e.g. 64). */
typedef struct {
    int max_queries;
    int train_tile_b;
} MatchVp6Config;

#define MATCH_VP6_DEFAULT_CONFIG { 256, 64 }

/*
 * Run batch descriptor matching on VP6.
 *   tm           : xvTileManager created with xvCreateTileManager().
 *   query_frame  : xvFrame wrapping Q*32 query descriptors in DRAM (32 wide x
 *                  Q tall, pitch 32, U8). Row q = query q's 32 bytes.
 *   train_frame  : xvFrame wrapping N*32 train descriptors in DRAM (32 x N).
 *   Q            : number of query descriptors.
 *   N            : number of train descriptors.
 *   out_dist1    : output, length Q: best Hamming distance per query.
 *   out_idx1     : output, length Q: index of best train descriptor (0..N-1).
 *   out_dist2    : output, length Q: second-best Hamming distance per query.
 *   cfg          : config (NULL => MATCH_VP6_DEFAULT_CONFIG).
 * Returns 0 on success, non-zero on error. The driver initialises the outputs
 * (256/256/-1) and updates them; a query with no train has 256/256/-1.
 *
 * Bit-exact with calling match_best_kernel per query (== refactor
 * search_by_bow_core inner loop, using ngd_descriptor_distance): the Hamming
 * and best1/best2/idx1 tracking are identical. Validated by test_match_vp6.c
 * (3-way: naive ref using ngd_descriptor_distance vs match_best_kernel vs
 * match_vp6).
 */
int match_vp6_process(xvTileManager *tm,
                      xvFrame *query_frame, xvFrame *train_frame,
                      int Q, int N,
                      int *out_dist1, int *out_idx1, int *out_dist2,
                      const MatchVp6Config *cfg);

#ifdef __cplusplus
}
#endif
#endif /* MATCH_VP6_H */
