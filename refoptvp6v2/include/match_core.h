/*
 * match_core.h - portable ORB descriptor matching kernel (shared PC + VP6).
 *
 * Extracted verbatim-arithmetic from refactor/src/matcher.c::ngd_descriptor_distance
 * (Hamming via bit-parallel popcount) and the best1/best2/idx1 inner loop of
 * search_by_bow_core (ORBmatcher.cc:269-302). The Hamming and the best-match
 * tracking are UNCHANGED so the result is bit-exact with the refactor C
 * reference.
 *
 * This is the matching analogue of the ORB cores: the same functions the PC
 * reference (match_best_kernel / match_hamming_full) and the VP6 batch driver
 * (match_vp6.c) call. The kernel is tile-aware for the VP6 path (a train-tile
 * slice in SRAM); the per-query best1/best2/idx1 logic is identical to
 * search_by_bow_core's inner loop (minus the BoW/geometric candidate filtering,
 * which is the matcher integration's job - the VP6 kernel scans a dense train
 * set; the caller packs filtered candidates into it).
 */
#ifndef MATCH_CORE_H
#define MATCH_CORE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MATCH_DESC_SIZE 32   /* ORB descriptor = 256 bits / 32 bytes */

/*
 * Hamming distance between two 32-byte ORB descriptors (bit-parallel popcount,
 * 8 x uint32). 1:1 with refactor's ngd_descriptor_distance (the arithmetic is
 * identical). `a` and `b` must be 4-byte aligned (descriptors in tile buffers
 * with pitch 32 are 32-aligned, so this holds).
 */
int match_hamming(const uint8_t *a, const uint8_t *b);

/*
 * Per-query best-match over a DENSE train descriptor array (1:1 with the
 * search_by_bow_core inner loop, ORBmatcher.cc:269-302).
 *   query       : one 32-byte descriptor.
 *   train        : n_train descriptors, back-to-back (train[i*32 .. i*32+31]).
 *   n_train      : number of train descriptors.
 *   out_dist1    : best (smallest) Hamming distance.
 *   out_idx1     : index of the best train descriptor (0..n_train-1).
 *   out_dist2    : second-best Hamming distance.
 * Tracks best1/best2 exactly as refactor: on a new best, best2 <- old best1;
 * on a new second-best, best2 <- dist. Returns nothing; the caller applies the
 * TH_LOW/ratio accept (search_by_bow_core: best1<=TH_LOW && best1<ratio*best2).
 * If n_train==0, leaves *out_dist1=*out_dist2=256, *out_idx1=-1.
 */
void match_best_kernel(const uint8_t *query, const uint8_t *train, int n_train,
                       int *out_dist1, int *out_idx1, int *out_dist2);

/*
 * Per-tile best-match update: for each query q in [0,Q) and each train
 * descriptor j in [0,B) (at SRAM tile `train_tile`), compute Hamming vs the
 * SRAM query tile `query_tile` and update the per-query best1/best2/idx1 in
 * DRAM. `t_base` is the global train index of tile row 0 (so the recorded idx
 * is t_base+j). 1:1 with running match_best_kernel over the tile slice. Used by
 * the VP6 driver; also callable from PC.
 */
void match_tile_kernel(const uint8_t *query_tile, int Q,
                       const uint8_t *train_tile, int B, int t_base,
                       int *best1, int *best2, int *idx1);

/*
 * PC reference: Hamming on two descriptors (== ngd_descriptor_distance).
 */
int match_hamming_full(const uint8_t *a, const uint8_t *b);

#ifdef __cplusplus
}
#endif
#endif /* MATCH_CORE_H */
