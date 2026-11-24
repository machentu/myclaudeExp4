#ifndef NGD_LOOPCLOSING_H
#define NGD_LOOPCLOSING_H

/*
 * ngd/loopclosing.h — LoopClosing orchestration (pure C, single-map / non-IMU
 * / non-merge).
 *
 * Faithful (simplified) port of ORB_SLAM3::LoopClosing (src/LoopClosing.cc),
 * reduced to the single-map loop-closing path the RGBD/mono main line uses:
 *
 *   run() :  pop a KeyFrame from the queue
 *            detect_common_regions()  -> BoW candidate -> Sim3 -> SearchBySim3
 *                                        -> OptimizeSim3 -> consistency
 *            if mbLoopDetected: correct_loop()
 *
 *   correct_loop() (LoopClosing.cc:972-1226): propagate the corrective Sim3
 *     along the current KF's covisibility neighbourhood, correct MapPoints,
 *     fuse duplicates, build the loop connections, run OptimizeEssentialGraph,
 *     then a GlobalBundleAdjustment.
 *
 * Multi-map merge, IMU, spanning-tree parent edges, and the multi-KF
 * SearchByProjection variant are NOT modelled (single-map main line).  The
 * detect pipeline uses ngd_kfdb candidates + KF-KF SearchByBoW + Sim3Solver +
 * SearchBySim3 + OptimizeSim3 (all ported in earlier phases).
 */

#include "ngd/math.h"
#include "ngd/se3.h"
#include "ngd/sim3.h"
#include "ngd/kfdb.h"   /* brings ngd_kfdb + (via bow.h) ngd_bow_vocab typedefs */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ngd_map;
struct ngd_keyframe;
struct ngd_mappoint;

typedef struct ngd_loopclosing {
    struct ngd_map       *map;
    struct ngd_kfdb      *kfdb;
    const ngd_bow_vocab *vocab;
    int bFixScale;            /* RGBD/stereo -> 1 (metric scale) */

    /* KeyFrame queue (mlpLoopKFQueue). */
    struct ngd_keyframe **kfQueue;
    int nQ, capQ;

    /* ---- detection result, consumed by correct_loop ---- */
    struct ngd_keyframe  *currentKF;
    struct ngd_keyframe  *loopMatchedKF;
    ngd_sim3              loopScw;        /* corrective Sim3 of currentKF (mg2oLoopScw) */
    struct ngd_mappoint  **loopMatchedMPs; /* size currentKF->N: cur keypoint idx -> matched MP (NULL=none) */
    int                   nCurN;          /* allocated size of loopMatchedMPs */

    int mbLoopDetected;
    uint64_t mnLastLoopKFid;
} ngd_loopclosing;

void ngd_loopclosing_init(ngd_loopclosing *lc, struct ngd_map *map,
                          struct ngd_kfdb *kfdb, const ngd_bow_vocab *vocab,
                          int bFixScale);
void ngd_loopclosing_free(ngd_loopclosing *lc);

/* InsertKeyFrame (LoopClosing.cc:314). */
void ngd_loopclosing_insert_kf(ngd_loopclosing *lc, struct ngd_keyframe *kf);

/* NewDetectCommonRegions (LoopClosing.cc:327-513, single-map): query kfdb for
 * candidates of currentKF, then per candidate: KF-KF SearchByBoW (>=20) ->
 * Sim3Solver RANSAC (>=15) -> SearchBySim3 (>=50) -> OptimizeSim3 (>=20),
 * keeping the best. Sets currentKF/loopMatchedKF/loopScw/loopMatchedMPs and
 * mbLoopDetected.  Returns 1 if a loop was detected. */
int ngd_loopclosing_detect_common_regions(ngd_loopclosing *lc, struct ngd_keyframe *currentKF);

/* CorrectLoop (LoopClosing.cc:972-1226, single-map/non-IMU): propagate the
 * corrective Sim3, correct MapPoints, fuse, build loop connections, run
 * OptimizeEssentialGraph + GlobalBA.  Consumes currentKF/loopMatchedKF/
 * loopScw/loopMatchedMPs. */
void ngd_loopclosing_correct_loop(ngd_loopclosing *lc);

/* Run one iteration: pop the next KF, detect, and correct if detected.
 * Returns 1 if a loop was corrected this call, 0 otherwise. */
int ngd_loopclosing_run(ngd_loopclosing *lc);

#ifdef __cplusplus
}
#endif
#endif /* NGD_LOOPCLOSING_H */
