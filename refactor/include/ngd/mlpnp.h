#ifndef NGD_MLPNP_H
#define NGD_MLPNP_H

/*
 * ngd/mlpnp.h — Maximum-Likelihood PnP (MLPnP) + RANSAC (pure C).
 *
 * Faithful port of NGD-SLAM's MLPnPsolver (src/MLPnPsolver.cpp, Penate-Sanchez
 * et al. IJCV 2013), which Tracking::Relocalization (Tracking.cc:3757-3925)
 * originally uses for relocalization. The fifth stage shipped EPnP instead
 * (OpenCV epnp.cpp port, src/pnp.c) because MLPnP depends on Eigen; this module
 * adds MLPnP back as a second, independent PnP solver. The two coexist in
 * ngd_core; which to use is decided later by the caller.
 *
 * Algorithm (computePose, MLPnPsolver.cpp:356-658):
 *   A. per-point bearing nullspace (3x2)           :367-375
 *   B. planar test (rank of points3*points3^T)     :377-399   (planar -> 9 cols)
 *   C. design matrix A (2N x 12 / 2N x 9)          :425-512
 *   D. least squares: smallest eigenvector of A^T A:514-524
 *   E. R/t recovery + sign disambiguation          :526-637
 *   F. Gauss-Newton refinement (Rodrigues, 5 iters):639-658, 694-758
 * wrapped by RANSAC (iterate, :100-223): sample 6 -> computePose -> inliers
 * (reproject chi2) -> keep best -> Refine on all best inliers.
 *
 * Faithfulness notes (see plan / WORKLOG 第十一期):
 *   - The ML covariance path is DEAD in the original: iterate/Refine always pass
 *     covs(size=1), so the guard covMats.size()==N (N>5) is never true, the
 *     weight matrix P stays identity, and AtPA = A^T A (no sparse linalg). This
 *     port drops that path entirely (faithful to actual behavior). sigma2 is
 *     used ONLY for the RANSAC inlier threshold, exactly as in the original.
 *   - Bearings are pinhole unproject divided by z: ((u-cx)/fx, (v-cy)/fy, 1)
 *     (NOT unit), matching the original's (x/z,y/z,1) form that the sign-
 *     disambiguation score (1 - v.f) depends on. Pinhole only (refactor scope;
 *     Kannala-Brandt8 not ported).
 *   - Output is ngd_se3 (quat + t), consistent with EPnP; the original outputs
 *     Eigen::Matrix4f. R -> quat via ngd_quat_from_matrix (handles slight
 *     non-orthogonality).
 *   - RANSAC is a batch run-to-completion (up to maxIts, keep best, Refine,
 *     return best on exhaustion), net-equivalent to the original incremental
 *     iterate(5,...) loop whose OR-condition collapses to "run until maxIts or
 *     success". Deterministic srand(12345) (refactor convention).
 *
 * No external dependencies (only <math.h>). Internal math is double for
 * conditioning (eigenvalues of a 12x12 span ~1e8..1e10); result narrowed to
 * float ngd_se3.
 */

#include "ngd/math.h"
#include "ngd/se3.h"
#include "ngd/calib.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    double prob;          /* 0.99  (RANSAC success probability)            */
    int    minInliers;    /* 10    (minimum inliers to accept a model)     */
    int    maxIterations; /* 300   (hard cap on RANSAC iterations)         */
    int    minSet;        /* 6     (sample size per computePose run)       */
    float  epsilon;       /* 0.5   (expected outlier ratio)                */
    float  th2;           /* 5.991 (chi-square, 2 DoF, 0.05 significance)  */
} ngd_mlpnp_ransac_params;

/* Defaults = Tracking.cc:3805 call-site values (0.99,10,300,6,0.5,5.991),
 * matching ngd_pnp_ransac_defaults. */
ngd_mlpnp_ransac_params ngd_mlpnp_ransac_defaults(void);

/* MLPnP on all N correspondences (no RANSAC). p2d is length 2N (u,v interleaved,
 * pixels). Bearings are derived internally via pinhole unproject. N >= 6.
 * Faithful port of MLPnPsolver::computePose. Returns 1 on success, 0 on failure
 * (N < 6, singular config). */
int ngd_mlpnp_solve(const ngd_vec3 *p3d, const float *p2d, int N,
                    const ngd_cam_ctx *cam, ngd_se3 *outTcw);

/* MLPnP + RANSAC. sigma2 is length N (per-point image-space variance, ==
 * mvLevelSigma2[octave]); outInliers is length N (set to 1/0). Returns 1 on
 * success. RANSAC samples minSet points, runs computePose, classifies all N by
 * reprojection chi2 (err^2 < sigma2*th2), keeps the best inlier set, then
 * re-runs computePose on all best inliers (Refine). Uses a fixed srand seed for
 * determinism. */
int ngd_mlpnp_solve_ransac(const ngd_vec3 *p3d, const float *p2d, const float *sigma2,
                           int N, const ngd_cam_ctx *cam,
                           const ngd_mlpnp_ransac_params *p,
                           ngd_se3 *outTcw, int *outInliers, int *outNInliers);

#ifdef __cplusplus
}
#endif
#endif /* NGD_MLPNP_H */
