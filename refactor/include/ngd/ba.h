#ifndef NGD_BA_H
#define NGD_BA_H

/*
 * ngd/ba.h — LocalBundleAdjustment (pure C, Schur complement).
 *
 * Faithful port of ORB_SLAM3::Optimizer::LocalBundleAdjustment
 * (src/Optimizer.cc:1116-1500) and the g2o machinery it sits on:
 *   - VertexSE3Expmap (6-DOF pose, left perturbation, fixed flag)
 *   - VertexSBAPointXYZ (3-DOF point, marginalized via Schur)
 *   - EdgeSE3ProjectXYZ / EdgeStereoSE3ProjectXYZ (reprojection, Huber)
 *   - BlockSolver_6_3 Schur complement + OptimizationAlgorithmLevenberg (LM)
 *
 * Two-layer API (mirrors pose_opt.h):
 *   1. ngd_ba_optimize  — core optimizer on flat arrays (unit-testable).
 *   2. ngd_local_ba     — gathering wrapper over ngd_map / ngd_keyframe /
 *      ngd_mappoint, faithful to Optimizer::LocalBundleAdjustment.
 *
 * Internal math is double (g2o uses double; pnp.c precedent). Poses/points are
 * float at the boundary, promoted/demoted inside.
 *
 * Conventions (identical to pose_opt.h / se3.h):
 *   - residual r = obs - project(Tcw * Xw)
 *   - J_pose = -Pjac * SE3deriv(Xc),  J_point = -Pjac * R   (leading minus from r)
 *   - oplus: T_new = exp(dx) * T_old (left),  p_new = p_old + dx
 *   - b = -J^T Omega r  (carries the minus), dx = (H+lambda*I)^-1 b applied as-is
 *   - Huber weight rho1 = min(1, delta/sqrt(chi2)); H += rho1*ws*J^T J, b -= rho1*ws*J^T r
 */

#include "ngd/math.h"
#include "ngd/se3.h"
#include "ngd/pinhole.h"
#include "ngd/calib.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ngd_keyframe;
struct ngd_map;

/* ------------------------------------------------------------------ *
 * Core optimizer (flat arrays).
 * ------------------------------------------------------------------ */

/* One reprojection edge: point `point_idx` seen from pose `pose_idx`. */
typedef struct {
    int   pose_idx;    /* 0..n_poses-1 */
    int   point_idx;   /* 0..n_points-1 */
    int   stereo;      /* 0 = mono (2D obs), 1 = stereo (3D obs) */
    int   octave;      /* pyramid level -> invLevelSigma2[octave] */
    float obs[3];      /* (u,v) mono, or (u,v,uR) stereo */
} ngd_ba_edge;

typedef struct {
    /* Poses (SE3). pose_fixed[i]!=0 -> constant (not optimised, not in the
     * reduced pose system, but its edges still contribute to point blocks). */
    ngd_se3 *poses;
    int     *pose_fixed;
    int      n_poses;

    /* Points (3D world). */
    ngd_vec3 *points;
    int       n_points;

    /* Edges. */
    ngd_ba_edge *edges;
    int           n_edges;

    /* Shared camera + scale pyramid (RGBD single camera). */
    const ngd_pinhole *cam;
    float              bf;
    const ngd_cam_ctx *calib;   /* supplies mvInvLevelSigma2[] / nlevels */

    /* Out: 1 for edges classified as outliers (chi2 > threshold or depth<=0). */
    int *outlier;              /* length n_edges, caller-allocated */
} ngd_ba_problem;

/* Run LM BA with Schur complement + Huber.
 *   delta mono = sqrt(5.991), stereo = sqrt(7.815)
 *   up to `max_iters` outer LM iterations (g2o optimize(10))
 * Updates poses[] (non-fixed) and points[] in place; fills outlier[].
 * Returns the final robust chi2 (sum of rho0 over inlier edges). */
double ngd_ba_optimize(ngd_ba_problem *p, int max_iters);

/* ------------------------------------------------------------------ *
 * Gathering wrapper — faithful LocalBundleAdjustment(pKF, pbStopFlag, pMap).
 * ------------------------------------------------------------------ */

/* Gathers local KFs (pKF + covisible neighbours), local MPs (observed in local
 * KFs), fixed KFs (observe local MPs but not local); aborts if 0 fixed; runs
 * Schur LM BA (10 iters); culls outlier edges (chi2>5.991/7.815 or depth<=0) by
 * erasing the MP<->KF observation; writes back optimised poses (non-fixed local
 * KFs) + MP positions. stop_flag may be NULL (single-threaded core).
 * Returns 0 on success, 1 if aborted (0 fixed KFs). */
int ngd_local_ba(struct ngd_keyframe *pKF, struct ngd_map *pmap, const int *stop_flag);

/* GlobalBundleAdjustment (Optimizer::GlobalBundleAdjustment, called by
 * LoopClosing::RunGlobalBundleAdjustment): all non-bad KFs (init KF fixed) +
 * all non-bad MPs + all observations; up to max_iters LM rounds.  `cam` may be
 * NULL (then allKFs[0]->cam is used).  Returns 0 on success, 1 if <2 KFs or 0
 * MPs. */
int ngd_global_ba(struct ngd_map *pmap, int max_iters, const ngd_cam_ctx *cam);

#ifdef __cplusplus
}
#endif
#endif /* NGD_BA_H */
