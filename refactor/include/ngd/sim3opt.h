#ifndef NGD_SIM3OPT_H
#define NGD_SIM3OPT_H

/*
 * ngd/sim3opt.h — OptimizeSim3 in pure C.
 *
 * Faithful to ORB-SLAM3 Optimizer::OptimizeSim3 (src/Optimizer.cc:2115-2381):
 * a single Sim3 vertex (S12, 7-DoF) is refined against bidirectional
 * reprojection edges
 *   edge12:  x1 = project_cam1( S12 * X2c )      (KF1 keypoint vs MP2-in-cam2)
 *   edge21:  x2 = project_cam2( S21 * X1c )      (KF2 keypoint vs MP1-in-cam1)
 * with point vertices FIXED.  g2o's EdgeSim3ProjectXYZ::linearizeOplus is
 * commented out, so g2o uses NUMERICAL Jacobians at runtime — this port
 * replicates that (central-difference on the 7-DoF left perturbation
 * S_new = exp(xi) * S_old), avoiding a hand-transcribed 2x7 analytic Jacobian
 * that g2o itself does not use.
 *
 * Robustification: Huber kernel (delta = sqrt(th2)), IRLS weight
 * rho[1] = min(1, delta/sqrt(chi2)). LM (H + lambda*I, Nielsen lambda) like
 * pose_opt.c.  Flow matches Optimizer.cc: optimize(5) with kernel -> reject
 * chi2>th2 -> if survivors < 10 abort, else drop kernel + optimize(5 or 10)
 * -> final inlier count.
 */

#include "ngd/math.h"
#include "ngd/sim3.h"
#include "ngd/pinhole.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int nCorr;                       /* number of correspondences */
    const float *obs1;               /* [2*nCorr] KF1 keypoint (u,v) for MP1 */
    const float *obs2;               /* [2*nCorr] KF2 keypoint (u,v) for MP2 */
    const ngd_vec3 *X1c;             /* [nCorr] MP1 in KF1 camera frame */
    const ngd_vec3 *X2c;             /* [nCorr] MP2 in KF2 camera frame */
    const float *invS2_1;            /* [nCorr] KF1 invLevelSigma2 */
    const float *invS2_2;            /* [nCorr] KF2 invLevelSigma2 */
    ngd_pinhole cam1, cam2;
    float th2;                       /* chi2 threshold (mono 5.991 / stereo 7.815) */
    int bFixScale;                   /* if true, scale (sigma) DoF held fixed */
} ngd_sim3_obs;

/* Refine S12 (inout).  outliers[nCorr] set (1=outlier).  Returns nInliers. */
int ngd_sim3_optimize(ngd_sim3* S12, const ngd_sim3_obs* o, int* outliers);

#ifdef __cplusplus
}
#endif
#endif /* NGD_SIM3OPT_H */
