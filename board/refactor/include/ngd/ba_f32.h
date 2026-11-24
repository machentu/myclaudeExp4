#ifndef NGD_BA_F32_H
#define NGD_BA_F32_H

/*
 * ngd/ba_f32.h — mixed-precision (f32 linear algebra) local BA switch.
 *
 * The A7 (ARMv7-A) has no f64 NEON: every f64 FMA goes to the scalar VFP
 * (~18 cycles, non-pipelined) while f32 has 4-lane NEON. The dense fast
 * solver (ngd_ba_optimize_fast, A1-A7) is f64, so its H assembly / Schur /
 * blocked LU / back-substitution sit on that scalar wall. This header
 * routes the LOCAL BA call site (ba.c ngd_local_ba) to
 * refappv8seg_full/src/ngd_opt_lmf32.c instead:
 *
 *   - F1  all linear algebra in f32 (H buffers, Schur, blocked LU NB=32,
 *         back-substitution buffers) — peak memory BELOW the f64 fast path
 *   - F2  NEON kernels (6x6 block scatter BD*B^T with 4+2 lanes, LU tail
 *         row update, back-sub dots); plain scalar f32 on x86
 *   - F3  f64 closure: linearisation, chi2, Huber weights, lambda/Nielsen,
 *         se3 state updates, write-back, outlier classification
 *   - F4  pl_blk dual layout (6x3 Ba + 3x6 Bt same-value) so scatter and
 *         back-sub both stream contiguous 6-float rows
 *
 * Priority at the LBA call site: NGD_BA_F32 > NGD_BA_SPARSE >
 * NGD_BA_FAST > dense. The ngd_ba_optimize dispatcher (GBA path) is NOT
 * affected — it keeps the ba_sparse.h switch (dense reference by default).
 *
 * Switch (compile-time):
 *   #define NGD_BA_F32 1   <- default: ON (2026-11-24 板端四门全过定档)
 * Override with -DNGD_BA_F32=0 to restore the v46-exact f64 fast path
 * (empty translation unit, bit-identical to the pre-f64-flip build).
 *
 * 定档数据 (xc01 A7, m208, NNP 500MHz, TUM fr3_walking_xyz 全量 827,
 * _xc01npuW = build_arm_xc01_npu23.sh, vs v46/_xc01npuP):
 *   rc=0; wall 14.22min (-6.4%); lm 101.4s (-35%, 882ms/KF); pure BA
 *   815ms/call (-37%), late max 1129ms (-44%); KF=115 (same); ATE all
 *   statistics identical digits (1.49/1.35/1.23/0.64/5.31); orb/of/track/
 *   yolo call counts identical — ZERO decision flips anywhere. Quality
 *   band gates (rc + ATE<=1.55 + |KF-115|<=8 + lm down) all passed; the
 *   zero-flip record is the strongest equivalence evidence an f32-grade
 *   change can produce. -O3 stacking (_xc01npuX: 13.60min/lm 86.0s/113KF/
 *   1.52cm) also passes the band but flips the chain (GCC -O3 ulp, same
 *   mechanism as npuV) — kept as backup tier, NOT the default.
 *   详见 slampart/ba_sparse_ab_test.md §六。
 */

#include "ngd/ba.h"

#ifndef NGD_BA_F32
#define NGD_BA_F32 1
#endif

/* Mixed-precision core optimiser — same contract as ngd_ba_optimize_fast
 * (ba.h); defined in refappv8seg_full/src/ngd_opt_lmf32.c (empty TU when
 * NGD_BA_F32 == 0). */
double ngd_ba_optimize_f32(ngd_ba_problem *p, int max_iters);

#endif /* NGD_BA_F32_H */
