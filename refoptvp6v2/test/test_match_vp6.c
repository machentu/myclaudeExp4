/*
 * test_match_vp6.c - end-to-end test for the VP6 batch descriptor matcher.
 *
 * Two checks:
 *  (A) Hamming: for random descriptor pairs, a VERBATUM copy of refactor's
 *      ngd_descriptor_distance vs match_hamming_full (must be identical).
 *  (B) Best-match (3-way): for Q queries x N train descriptors,
 *        (1) ref_best  - verbatim search_by_bow_core inner loop using
 *            ngd_descriptor_distance (the oracle),
 *        (2) match_best_kernel - the portable per-query kernel (match_hamming),
 *        (3) match_vp6 - the VP6 batch driver (queries in SRAM, train tiled).
 *      (1)==(2) proves match_core matches refactor; (2)==(3) proves the VP6
 *      batch path is bit-exact. Compare best1/idx1/best2 per query.
 *
 * Covers several Q/N (incl. 0 and 1) and random descriptors.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "match_core.h"
#include "match_vp6.h"
#include "tileManager.h"

#define POOL_BYTES      (256 * 1024)
#define DMA_DESCR_CNT   32
#define MAX_PIF         16
#ifndef MAX_BLOCK_8
#define MAX_BLOCK_8     8
#endif

#ifndef ALIGN64
#ifdef _MSC_VER
#define ALIGN64 __declspec(align(64))
#else
#define ALIGN64 __attribute__((aligned(64)))
#endif
#endif

static IDMA_BUFFER_DEFINE(idmaObjBuff, DMA_DESCR_CNT, IDMA_2D_DESC);
static ALIGN64 uint8_t bank0[POOL_BYTES];
static ALIGN64 uint8_t bank1[POOL_BYTES];
static void err_cb(idma_error_details_t *d) { (void)d; fprintf(stderr, "iDMA error\n"); }
static void intr_cb(void *p) { (void)p; }

/* ---- (1) verbatim oracle: refactor ngd_descriptor_distance + best loop ---- */
static int ref_descriptor_distance(const uint8_t *a, const uint8_t *b) {
    const uint32_t *pa = (const uint32_t *)(const void *)a;
    const uint32_t *pb = (const uint32_t *)(const void *)b;
    int dist = 0;
    for (int i = 0; i < 8; i++) {
        uint32_t v = pa[i] ^ pb[i];
        v = v - ((v >> 1) & 0x55555555u);
        v = (v & 0x33333333u) + ((v >> 2) & 0x33333333u);
        dist += (int)((((v + (v >> 4)) & 0x0F0F0F0Fu) * 0x01010101u) >> 24);
    }
    return dist;
}
static void ref_best(const uint8_t *query, const uint8_t *train, int n,
                     int *d1, int *i1, int *d2) {
    int bestDist1 = 256, bestIdx1 = -1, bestDist2 = 256;
    for (int b = 0; b < n; b++) {
        int dist = ref_descriptor_distance(query, train + (size_t)b * 32);
        if (dist < bestDist1) { bestDist2 = bestDist1; bestDist1 = dist; bestIdx1 = b; }
        else if (dist < bestDist2) { bestDist2 = dist; }
    }
    *d1 = bestDist1; *i1 = bestIdx1; *d2 = bestDist2;
}

/* Returns 0 on full 3-way bit-exact match (best1/idx1/best2 per query), 1 otherwise. */
static int run_one(xvTileManager *tm, int Q, int N)
{
    uint8_t *qry = (uint8_t *)malloc((size_t)Q * 32);
    uint8_t *trn = (uint8_t *)malloc((size_t)N * 32);
    int *r1 = (int *)malloc(sizeof(int) * (size_t)Q), *r2 = (int *)malloc(sizeof(int) * (size_t)Q), *ri = (int *)malloc(sizeof(int) * (size_t)Q);
    int *k1 = (int *)malloc(sizeof(int) * (size_t)Q), *k2 = (int *)malloc(sizeof(int) * (size_t)Q), *ki = (int *)malloc(sizeof(int) * (size_t)Q);
    int *v1 = (int *)malloc(sizeof(int) * (size_t)Q), *v2 = (int *)malloc(sizeof(int) * (size_t)Q), *vi = (int *)malloc(sizeof(int) * (size_t)Q);
    if (Q && (!qry || !r1 || !r2 || !ri || !k1 || !k2 || !ki || !v1 || !v2 || !vi)) { fprintf(stderr, "oom\n"); return 1; }
    if (N && !trn) { fprintf(stderr, "oom\n"); return 1; }
    for (int i = 0; i < Q * 32; i++) qry[i] = (uint8_t)(rand() & 255);
    for (int i = 0; i < N * 32; i++) trn[i] = (uint8_t)(rand() & 255);

    /* (1) naive ref + (2) portable kernel, per query */
    for (int q = 0; q < Q; q++) {
        ref_best(qry + (size_t)q * 32, trn, N, &r1[q], &ri[q], &r2[q]);
        match_best_kernel(qry + (size_t)q * 32, trn, N, &k1[q], &ki[q], &k2[q]);
    }

    /* (3) VP6 batch (driver handles Q==0 / N==0 by returning early) */
    xvFrame *qf = (Q > 0) ? xvCreateFrame(tm, qry, (uint32_t)Q * 32, 32, Q, 32, 1, 1, FRAME_EDGE_PADDING, 0) : NULL;
    xvFrame *tf = (N > 0) ? xvCreateFrame(tm, trn, (uint32_t)N * 32, 32, N, 32, 1, 1, FRAME_EDGE_PADDING, 0) : NULL;
    MatchVp6Config cfg = MATCH_VP6_DEFAULT_CONFIG;
    if (Q > cfg.max_queries) cfg.max_queries = Q;
    int rc = match_vp6_process(tm, qf, tf, Q, N, v1, vi, v2, &cfg);
    if (qf) xvFreeFrame(tm, qf);
    if (tf) xvFreeFrame(tm, tf);

    int ok = 0;
    if (rc != 0) {
        fprintf(stderr, "  match_vp6_process failed: %d (Q=%d N=%d)\n", rc, Q, N);
    } else {
        int mism_kernel = 0, mism_vp6 = 0, first_bad = -1;
        for (int q = 0; q < Q; q++) {
            if (r1[q] != k1[q] || ri[q] != ki[q] || r2[q] != k2[q]) mism_kernel++;
            if (r1[q] != v1[q] || ri[q] != vi[q] || r2[q] != v2[q]) { mism_vp6++; if (first_bad < 0) first_bad = q; }
        }
        ok = (mism_kernel == 0 && mism_vp6 == 0);
        printf("  Q=%-4d N=%-4d : kernel_mism=%d vp6_mism=%d\n", Q, N, mism_kernel, mism_vp6);
        if (!ok) {
            fprintf(stderr, "    MISMATCH: kernel=%d vp6=%d\n", mism_kernel, mism_vp6);
            if (first_bad >= 0) {
                int q = first_bad;
                fprintf(stderr, "    q[%d] ref(d1=%d,i1=%d,d2=%d) vp6(d1=%d,i1=%d,d2=%d)\n",
                        q, r1[q], ri[q], r2[q], v1[q], vi[q], v2[q]);
            }
        }
    }

    free(qry); free(trn); free(r1); free(r2); free(ri); free(k1); free(k2); free(ki); free(v1); free(v2); free(vi);
    return ok ? 0 : 1;
}

int main(void)
{
    srand(1234);
    xvTileManager tm;
    void   *banks[2] = { bank0, bank1 };
    int32_t sizes[2] = { POOL_BYTES, POOL_BYTES };
    idma_init(0, MAX_BLOCK_8, MAX_PIF, 0, 0, (idma_err_callback_fn)err_cb);
    uint32_t r = xvCreateTileManager(&tm, idmaObjBuff, 2, banks, sizes,
                                     (idma_err_callback_fn)err_cb,
                                     (idma_callback_fn)intr_cb, NULL,
                                     DMA_DESCR_CNT, MAX_BLOCK_8, MAX_PIF);
    if (r != XVTM_SUCCESS) { fprintf(stderr, "TM init failed: %u\n", r); return 1; }

    printf("=== VP6 descriptor match vs PC ===\n");

    int fail = 0;

    /* (A) Hamming: random pairs, verbatim ngd_descriptor_distance vs match_hamming_full. */
    int ham_mism = 0;
    for (int i = 0; i < 1000; i++) {
        uint8_t a[32], b[32];
        for (int k = 0; k < 32; k++) { a[k] = (uint8_t)(rand() & 255); b[k] = (uint8_t)(rand() & 255); }
        if (ref_descriptor_distance(a, b) != match_hamming_full(a, b)) ham_mism++;
    }
    printf("  Hamming: 1000 random pairs, mism=%d\n", ham_mism);
    if (ham_mism) fail = 1;

    /* (B) Best-match 3-way over several Q/N. */
    struct { int Q, N; } cs[] = {
        {50, 200}, {100, 100}, {10, 500}, {200, 50},
        {1, 100}, {50, 1}, {50, 0}, {0, 50}, {64, 64}, {150, 130},
    };
    const int n = (int)(sizeof(cs) / sizeof(cs[0]));
    for (int i = 0; i < n; i++)
        if (run_one(&tm, cs[i].Q, cs[i].N) != 0)
            fail = 1;

    printf("=== %s ===\n", fail ? "FAIL" : "ALL PASS");
    return fail ? 1 : 0;
}
