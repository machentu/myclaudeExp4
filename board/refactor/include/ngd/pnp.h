#ifndef NGD_PNP_H
#define NGD_PNP_H

/*
 * ngd/pnp.h — EPnP pose solver + RANSAC (pure C).
 *
 * The relocalization path (Tracking::Relocalization, Tracking.cc:3757-3925)
 * originally uses MLPnPsolver (Maximum-Likelihood PnP, bearing-vector +
 * nullspace + sparse Gauss-Newton, depends on Eigen). That is heavy to port.
 * Per the approved Stage-2 plan we instead implement **EPnP** (Lepetit et
 * al., IJCV 2009), a self-contained linear PnP, faithfully ported from
 * OpenCV's modules/calib3d/src/epnp.cpp. PoseOptimization (already ported)
 * refines the seed pose afterward, so EPnP only needs to supply a seed +
 * RANSAC inlier classification.
 *
 * No external dependencies (only <math.h>). All internal math is double for
 * EPnP's conditioning; the result is narrowed to float ngd_se3.
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
    int    maxIterations; /* 300   (hard cap on RANSAC iterations          */
    int    minSet;        /* 6     (sample size per EPnP run)              */
    float  epsilon;       /* 0.5   (expected outlier ratio)                */
    float  th2;           /* 5.991 (chi-square, 2 DoF, 0.05 significance)  */
} ngd_pnp_ransac_params;

/* Defaults matching MLPnPsolver::SetRansacParameters(0.99,10,300,6,0.5,5.991)
 * used by Tracking::Relocalization (Tracking.cc:3805). */
ngd_pnp_ransac_params ngd_pnp_ransac_defaults(void);

/* EPnP on all N correspondences (no RANSAC). p2d is length 2N (u,v interleaved).
 * Returns 1 on success, 0 on failure (e.g. N < minSet, singular config).
 * Faithful port of OpenCV epnp.cpp::compute_pose. */
int ngd_epnp_solve(const ngd_vec3 *p3d, const float *p2d, int N,
                   const ngd_cam_ctx *cam, ngd_se3 *outTcw);

/* EPnP + RANSAC. sigma2 is length N (per-point image-space variance, ==
 * mvLevelSigma2[octave]); outInliers is length N (set to 1/0). Returns 1 on
 * success. RANSAC samples minSet points, runs EPnP, classifies all N by
 * reprojection chi2 (err^2 < sigma2*th2), keeps the best inlier set, then
 * re-runs EPnP on all best inliers for a refined pose. Uses a fixed srand
 * seed for determinism. */
int ngd_pnp_solve_ransac(const ngd_vec3 *p3d, const float *p2d, const float *sigma2,
                         int N, const ngd_cam_ctx *cam,
                         const ngd_pnp_ransac_params *p,
                         ngd_se3 *outTcw, int *outInliers, int *outNInliers);

#ifdef __cplusplus
}
#endif
#endif /* NGD_PNP_H */
