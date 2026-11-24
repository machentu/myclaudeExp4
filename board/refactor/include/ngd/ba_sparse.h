#ifndef NGD_BA_SPARSE_H
#define NGD_BA_SPARSE_H

/*
 * ngd/ba_sparse.h — block-sparse pose solver for the Schur-complement BA
 * (ngd/ba.c). ba.c marginalises the points with per-point 3x3 Schur blocks
 * but stores and solves the POSE system densely (dim_p x dim_p Hpp/Hschur,
 * memcpy+malloc per LM trial, dense LU O(dim_p^3)). g2o's BlockSolver_6_3
 * exploits the block sparsity instead: a 6x6 pose-pair block is nonzero only
 * when the two poses co-observe a map point. This header switches
 * ngd_ba_optimize to such a sparse implementation:
 *
 *   - 6x6 block-sparse upper-triangular CSR for the pose Hessian (pattern
 *     fixed per problem, built once from all edges),
 *   - Schur complement accumulated directly into the sparse blocks; each
 *     unordered pose pair contributes S = B_a D B_b^T once (the (b,a) entry
 *     is its implicit transpose),
 *   - up-looking block LDL^T (sparse block Cholesky, natural pose order) +
 *     forward/back substitution for dxp; per trial only the nonzero block
 *     values are copied (no dense dim_p^2 traffic, no per-trial malloc),
 *   - trial-chi2 evaluation skips the Jacobians (chi2 identical).
 *
 * The LM schedule (lambda init/update, Nielsen gain ratio, max 10 inner
 * trials, Huber IRLS weights, outlier classification) is line-for-line the
 * same as the dense path, so both converge to the same optimum
 * (tests/test_ba_sparse.c checks dense vs sparse equivalence).
 *
 * Switch (compile-time):
 *   #define NGD_BA_SPARSE 0   <- default: OFF (2026-11-23 板端 A/B 两轮定案)
 * Override with -DNGD_BA_SPARSE=1 to enable. Both entry points below stay
 * linkable either way, so A/B tests and benchmarks can drive them side by
 * side.
 *
 * 定案数据 (xc01 A7, m208, TUM fr3_walking_xyz 全量 827, vs v46 dense fast):
 *   r1 (npuS): lm 173.5s/106KF=1637ms/call, ATE 1.72cm  (v46: 155.5s/115KF
 *   =1352ms/call, 1.49cm); r2 (npuT, 热路径优化后): 169.8s=1602ms/call 只赚
 *   2.1% — 帽24 规模 (nb≈60/dim_p≲500) 下 6x6 块散射不敌 dense 分块流式。
 *   sparse 主场 = GBA 级/解帽大窗口 (PC GBA-L 2.2x wall / 11x mults)。
 *   详见 slampart/ba_sparse_ab_test.md。LBA 分发优先级: NGD_BA_SPARSE >
 *   NGD_BA_FAST > dense 参考 (ba.c ngd_local_ba 调用点)。
 */

#include "ngd/ba.h"

#ifndef NGD_BA_SPARSE
#define NGD_BA_SPARSE 0
#endif

/* ------------------------------------------------------------------ *
 * Diagnostics from the last ngd_ba_optimize_sparse call (single-threaded
 * core; the struct is file-static in ba_sparse.c). Multiply counts are the
 * dominant cost term on soft-float (no FPU) boards, where cycles ~ mults.
 * Zeroed at entry; filled even if the optimiser exits early.
 * ------------------------------------------------------------------ */
typedef struct {
    int nb;                  /* free pose blocks (n_free) */
    int dim_p;               /* 6*nb */
    int nnzA;                /* stored 6x6 blocks of the pose Hessian (upper) */
    int nnzL;                /* 6x6 blocks of L incl. implicit unit diagonals */
    int trials;              /* LM inner trials executed */
    int factor_fails;        /* LDL^T factorisations that failed (lambda up) */
    long long schur_mults;   /* multiply ops in per-point Schur accumulation */
    long long factor_mults;  /* multiply ops in block LDL^T factorisations */
    long long solve_mults;   /* multiply ops in forward/back substitution */
    long long dense_lu_mults_equiv; /* mults the dense path would need for the
                                       same trials: LU + ordered-pair Schur */
} ngd_ba_sparse_stats;

/* Copy the stats of the last ngd_ba_optimize_sparse call into *out. */
void ngd_ba_sparse_last_stats(ngd_ba_sparse_stats *out);

/* Sparse core optimiser — same contract as ngd_ba_optimize (ba.h). */
double ngd_ba_optimize_sparse(ngd_ba_problem *p, int max_iters);

/* Dense reference — the original ngd_ba_optimize body, kept linkable for
 * A/B equivalence tests and benchmarks (defined in ba.c). */
double ngd_ba_optimize_dense(ngd_ba_problem *p, int max_iters);

#endif /* NGD_BA_SPARSE_H */
