/*
 * match_core.c - portable ORB descriptor matching kernel (shared PC + VP6).
 *
 * The Hamming popcount is copied 1:1 from refactor/src/matcher.c::
 * ngd_descriptor_distance, and the best1/best2/idx1 tracking is copied 1:1 from
 * search_by_bow_core's inner loop (ORBmatcher.cc:269-302), so the result is
 * bit-exact with the refactor C reference.
 *
 * Build: plain C on PC, or as C++ under the Xtensa cstub (see CMakeLists.txt).
 */
#include "match_core.h"

/* 1:1 with ngd_descriptor_distance (8 x uint32 bit-parallel popcount). */
int match_hamming(const uint8_t *a, const uint8_t *b)
{
    const uint32_t *pa = (const uint32_t *)(const void *)a;
    const uint32_t *pb = (const uint32_t *)(const void *)b;
    int dist = 0;
    for (int i = 0; i < 8; ++i) {
        uint32_t v = pa[i] ^ pb[i];
        v = v - ((v >> 1) & 0x55555555u);
        v = (v & 0x33333333u) + ((v >> 2) & 0x33333333u);
        dist += (int)((((v + (v >> 4)) & 0x0F0F0F0Fu) * 0x01010101u) >> 24);
    }
    return dist;
}

/* 1:1 with search_by_bow_core's best1/best2/idx1 loop (cc:269-302). */
void match_best_kernel(const uint8_t *query, const uint8_t *train, int n_train,
                       int *out_dist1, int *out_idx1, int *out_dist2)
{
    int bestDist1 = 256, bestIdx1 = -1, bestDist2 = 256;   /* cc:269-271 */
    for (int b = 0; b < n_train; ++b) {
        int dist = match_hamming(query, train + (size_t)b * MATCH_DESC_SIZE);
        if (dist < bestDist1) {                            /* cc:289 */
            bestDist2 = bestDist1;
            bestDist1 = dist;
            bestIdx1 = b;
        } else if (dist < bestDist2) {                     /* cc:295 */
            bestDist2 = dist;
        }
    }
    *out_dist1 = bestDist1;
    *out_idx1 = bestIdx1;
    *out_dist2 = bestDist2;
}

/* Per-tile update: same best1/best2/idx1 logic, but the global train index is
 * t_base + j (the tile's offset into the full train set). */
void match_tile_kernel(const uint8_t *query_tile, int Q,
                       const uint8_t *train_tile, int B, int t_base,
                       int *best1, int *best2, int *idx1)
{
    for (int q = 0; q < Q; ++q) {
        const uint8_t *dq = query_tile + (size_t)q * MATCH_DESC_SIZE;
        int b1 = best1[q], b2 = best2[q], i1 = idx1[q];
        for (int j = 0; j < B; ++j) {
            int dist = match_hamming(dq, train_tile + (size_t)j * MATCH_DESC_SIZE);
            if (dist < b1) {
                b2 = b1; b1 = dist; i1 = t_base + j;
            } else if (dist < b2) {
                b2 = dist;
            }
        }
        best1[q] = b1; best2[q] = b2; idx1[q] = i1;
    }
}

int match_hamming_full(const uint8_t *a, const uint8_t *b)
{
    return match_hamming(a, b);
}
