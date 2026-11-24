#ifndef NGD_ESSGRAPH_H
#define NGD_ESSGRAPH_H

/*
 * ngd/essgraph.h — OptimizeEssentialGraph in pure C.
 *
 * Faithful (single-map, non-IMU, non-merge) port of
 * Optimizer::OptimizeEssentialGraph (src/Optimizer.cc:1501-1783): a Sim3
 * pose-graph over all KeyFrames that propagates a loop-closure correction
 * across the whole map.
 *
 * Vertices: one Sim3 per KeyFrame (Scw, world->cam). The init KeyFrame is
 * FIXED (gauge). Estimate = CorrectedSim3[kf] if supplied, else Sim3(Tcw, s=1).
 *
 * Edges (EdgeSim3, 7-D relative constraint err = log(meas * Siw * Sjw^{-1}),
 * information = I_7, NO robust kernel; g2o uses numeric Jacobians — so do we):
 *   - covisibility edges (weight >= minFeat=100, id_j < id_i, dedup), measured
 *     from NonCorrectedSim3 (pre-correction poses);
 *   - loop edges supplied by the caller (the new LoopConnections), measured from
 *     CorrectedSim3.
 * Spanning-tree parent edges and the KF mLoopEdges set are NOT modelled (the
 * refactor's KeyFrame does not track them); covisibility edges carry the
 * backbone.  LM (dense H, Nielsen lambda), 20 iterations.
 *
 * Recovery: each KF pose <- SE3(R, t/s) of the optimized Sim3.  Each MapPoint
 * world pos <- (optimized Twc of its refKF) * (pre-opt Tcw of refKF * Pw).
 */

#include "ngd/math.h"
#include "ngd/se3.h"
#include "ngd/sim3.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ngd_map;
struct ngd_keyframe;

/* KeyFrame -> Sim3 entry (the KeyFrameAndPose map). */
typedef struct { struct ngd_keyframe *kf; ngd_sim3 Scw; } ngd_kf_sim3;

/* A loop-closure connection a -> b (KeyFrame*, set<KeyFrame*> element). */
typedef struct { struct ngd_keyframe *a, *b; } ngd_kf_pair;

/* Optimize the pose graph of `map`.
 *   pLoopKF / pCurKF : the loop pair (used to force the pCurKF<->pLoopKF loop
 *                      edge even when its covisibility weight < minFeat).
 *   corrected[nCorr]      : CorrectedSim3 (vertex init + loop-edge measurement).
 *                           May be NULL (then each KF uses its current pose, s=1).
 *   nonCorrected[nNonCorr]: NonCorrectedSim3 (covisibility-edge measurement).
 *                           May be NULL (then edges measured from current pose).
 *   loopConns[nLoopConns] : extra loop edges to insert (LoopConnections).
 *                           May be NULL.
 *   bFixScale             : RGBD/stereo -> 1 (scale known, frozen at 1). */
void ngd_optimize_essential_graph(struct ngd_map *map,
                                  struct ngd_keyframe *pLoopKF, struct ngd_keyframe *pCurKF,
                                  const ngd_kf_sim3 *corrected, int nCorr,
                                  const ngd_kf_sim3 *nonCorrected, int nNonCorr,
                                  const ngd_kf_pair *loopConns, int nLoopConns,
                                  int bFixScale);

#ifdef __cplusplus
}
#endif
#endif /* NGD_ESSGRAPH_H */
