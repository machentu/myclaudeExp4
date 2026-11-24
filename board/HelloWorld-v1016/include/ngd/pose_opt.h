#ifndef NGD_POSE_OPT_H
#define NGD_POSE_OPT_H

/*
 * ngd/pose_opt.h — single-frame pose-only bundle adjustment (P0 optimizer).
 *
 * Faithful C reimplementation of ORB-SLAM3::Optimizer::PoseOptimization
 * (src/Optimizer.cc:814-1114) for the pure-C core. It is the most-called
 * optimizer in the tracking loop and — per NGD_SLAM_TECHNICAL_DOC.md §6.3(1)/§6.8 —
 * the recommended first port target.
 *
 * What is replicated exactly (verified against the g2o/ORB-SLAM3 sources):
 *   - one SE3 vertex, left-perturbation oplus T_new = exp(xi)*T  (g2o SE3Quat::exp)
 *   - reprojection residual  r = obs - project(map(T, Xw))
 *   - Jacobian  A = -projectJac(Xc) * SE3deriv(Xc)   (OptimizableTypes.cpp:49-63)
 *   - information  Omega = invSigma2(octave) * I
 *   - Huber robust kernel, delta = sqrt(5.991) mono / sqrt(7.815) stereo
 *   - IRLS weight w = rho[1] = min(1, delta/sqrt(chi2))  (g2o robustInformation)
 *   - 4 outer rounds; per round up to 10 Levenberg steps (H+lambda*I, lambda
 *     init 1e-5*maxdiag, Nielsen gain ratio, alpha clamped [1/3,2/3]); rounds
 *     0-2 use the Huber kernel, round 3 drops it (kernel removed at it==2)
 *   - chi2 inlier/outlier reclassification each round (5.991 / 7.815),
 *     previously-outlier edges reconsidered; early stop if <10 edges
 *   - returns (#observations - #outliers); writes optimized pose back in place
 *
 * No OpenCV, no g2o, no Eigen — pure C on ngd_core.
 */

#include "ngd/math.h"
#include "ngd/se3.h"
#include "ngd/pinhole.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Observation bundle. Arrays are caller-owned and must outlive the call.
 *   Xw[n]            map-point world positions
 *   obs[3*n]         flat: obs[3*i+0]=u, +1=v, +2=uR  (uR used iff is_stereo[i])
 *   is_stereo[n]     1 => stereo (3-D obs), 0 => mono (2-D obs)
 *   octave[n]        pyramid level, indexes invLevelSigma2[]
 *   invLevelSigma2[nlevels]  per-level inverse variance (octave 0 -> 1.0)
 */
typedef struct {
    ngd_pinhole cam;
    float bf;                    /* baseline*fx, used for stereo obs */
    int   n;
    const ngd_vec3 *Xw;
    const float    *obs;         /* length 3*n */
    const int      *is_stereo;   /* length n */
    const int      *octave;      /* length n */
    const float    *invLevelSigma2;
    int   nlevels;
} ngd_pose_obs;

/*
 * Optimize *pose against the observations in `o`.
 *   outlier[n]  in/out: entry state is ignored (all reset to inlier first),
 *               on return outlier[i] = 1 if classified outlier, else 0.
 * Returns the inlier count (== o->n - #outliers), or 0 if o->n < 3.
 */
int ngd_pose_optimization(ngd_se3 *pose, const ngd_pose_obs *o, int *outlier);

#ifdef __cplusplus
}
#endif
#endif /* NGD_POSE_OPT_H */
