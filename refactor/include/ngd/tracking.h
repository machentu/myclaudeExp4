#ifndef NGD_TRACKING_H
#define NGD_TRACKING_H

/*
 * ngd/tracking.h — tracking front-end (pure C), beginning with
 * StereoInitialization (P0). The reference-frame tracker (TrackReferenceKeyFrame)
 * and local-map tracking arrive in a later iteration.
 */

#include "ngd/frame.h"
#include "ngd/keyframe.h"
#include "ngd/mappoint.h"
#include "ngd/bow.h"
#include "ngd/kfdb.h"
#include "ngd/map.h"
#include "ngd/localmapping.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * StereoInitialization (RGB-D / stereo first frame), faithful to
 * src/Tracking.cc:2458-2492:
 *   - requires frame.N > 500
 *   - set frame pose to identity
 *   - create the first KeyFrame (owns the frame's keypoint arrays)
 *   - for each keypoint with depth>0: UnprojectStereo → world point (== camera
 *     point under identity pose), create a MapPoint, AddObservation(kf,i),
 *     kf.mvpMapPoints[i] = mp, ComputeDistinctiveDescriptors, UpdateNormalAndDepth
 *
 * On success returns the number of MapPoints created and writes *out_kf /
 * *out_mps (caller-owned arrays of mps; length = return value). Returns -1 if
 * the frame has too few features (<500).
 */
int ngd_stereo_initialization(ngd_frame *frame,
                              ngd_keyframe **out_kf,
                              ngd_mappoint ***out_mps, int *out_mps_cap);

/* ------------------------------------------------------------------ *
 * Tracking context (mirrors the relevant members of ORB_SLAM3::Tracking).
 * The frames / keyframes are NOT owned by the context (the caller owns
 * them); the context only holds pointers + the local-map and PoseOpt
 * scratch buffers (heap, per §7.3).                                          */

typedef enum { NGD_SENSOR_MONOCULAR, NGD_SENSOR_STEREO, NGD_SENSOR_RGBD } ngd_sensor;
typedef enum { NGD_STATE_NOT_INITIALIZED, NGD_STATE_OK, NGD_STATE_RECENTLY_LOST, NGD_STATE_LOST } ngd_state;

typedef struct {
    ngd_frame    *current;       /* mCurrentFrame (caller-owned) */
    ngd_frame    *last;          /* mLastFrame    (caller-owned) */
    ngd_keyframe *refKF;         /* mpReferenceKF */

    const ngd_bow_vocab *vocab;  /* ORB vocabulary (caller-owned; for TrackRefKF) */
    ngd_kfdb *kfdb;              /* KeyFrameDatabase (caller-owned; for Relocalization) */
    ngd_map *map;                /* Map (caller-owned; for CreateNewKeyFrame) */
    ngd_local_mapping *lm;       /* LocalMapping (caller-owned; KF insertion + culling) */

    ngd_se3    velocity;         /* mVelocity (Tcw_curr * Tcw_last^-1) */
    ngd_sensor sensor;
    ngd_state  mState;
    int   mbVO;                  /* VO flag (onlyTracking path, unused here) */
    int   mbVelocity;            /* Tracking::mbVelocity — motion model available */
    int   mbCreatedKF;           /* set by ngd_tracking_track when a new KF was created this frame */
    int   mnMatchesInliers;      /* TrackLocalMap inlier count */
    uint64_t mnLastRelocFrameId;
    uint64_t mnLastKeyFrameId;   /* Tracking::mnLastKeyFrameId (set by CreateNewKeyFrame) */
    int   mMinFrames;            /* Tracking::mMinFrames (default 0) */
    int   mbStartOpticalFlow;    /* NGD: optical-flow active flag (c5; inert until OF ported) */
    int   mMaxFrames;            /* reloc-recent window (default 60) */

    /* LocalMapping stub fields (config; bFarPoints off by default). */
    int   mbFarPoints;
    float mThFarPoints;

    /* local map buffers (heap, growable) */
    ngd_keyframe **localKFs; int nLocalKFs, capLocalKFs;   /* mvpLocalKeyFrames */
    int *kfVotes;                                         /* parallel to localKFs */
    ngd_mappoint **localMPs; int nLocalMPs, capLocalMPs;   /* mvpLocalMapPoints */

    /* PoseOptimization scratch (heap, grows to max frame N). */
    ngd_vec3 *po_Xw;
    float    *po_obs;        /* 3 * po_cap */
    int      *po_is_stereo, *po_octave, *po_frameidx, *po_outlier;
    int       po_cap;
} ngd_tracking;

void ngd_tracking_init(ngd_tracking *tk);     /* defaults: RGBD, OK, velocity=identity */
void ngd_tracking_destroy(ngd_tracking *tk); /* frees the heap buffers only */

/*
 * ngd_tracking_track_with_motion_model: faithful port of Tracking.cc:2901-2994
 * (non-IMU, non-onlyTracking). Applies the velocity model (Tcw_cur = V * Tcw_last),
 * SearchByProjection overload #2 (th=7 stereo / 15 else, retry 2*th if <20, fail
 * if still <20), PoseOptimization, then outlier culling. Returns 1 iff
 * nmatchesMap >= 10. State machine (Track() dispatcher) not ported: the caller
 * wires current/last frames + velocity into the context and calls this directly.
 */
int ngd_tracking_track_with_motion_model(ngd_tracking *tk);

/*
 * ngd_tracking_track_reference_keyframe: faithful port of Tracking.cc:2767-2826
 * (non-IMU). ComputeBoW(current) + SearchByBoW(refKF, current, ratio=0.7) ->
 * <15 fail -> SetPose(lastPose) -> PoseOptimization -> outlier culling. Returns
 * 1 iff nmatchesMap >= 10. Requires tk->vocab, tk->current, tk->last, tk->refKF
 * set by the caller. The refKF's BoW is computed lazily on first call.
 */
int ngd_tracking_track_reference_keyframe(ngd_tracking *tk);

/*
 * ngd_tracking_relocalization: faithful port of Tracking.cc:3757-3925 (non-IMU,
 * left branch). ComputeBoW(current) -> DetectRelocalizationCandidates -> for
 * each candidate: SearchByBoW(ratio=0.75, <15 discard) -> EPnP+RANSAC
 * (SetRansacParameters 0.99,10,300,6,0.5,5.991) -> SetPose -> fill inliers into
 * mvpMapPoints -> PoseOptimization -> nGood<10 continue, nGood>=50 success.
 * The nGood<50 refinement chain (cc:3872-3901, SearchByProjection ratio 0.9) is
 * applied as a single simplified pass over the candidate KF's MapPoints before
 * re-running PoseOptimization. Requires tk->kfdb, tk->vocab, tk->current set.
 * On success sets mnLastRelocFrameId = current->mnId and returns 1.
 */
int ngd_tracking_relocalization(ngd_tracking *tk);

/*
 * ngd_tracking_track_local_map: faithful port of Tracking.cc:3104-3204 (non-IMU).
 * UpdateLocalMap (covisibility-graph neighbour/spanning-tree expansion SIMPLIFIED
 * to "the KFs observing the current frame's matched MPs" — see WORKLOG) +
 * SearchLocalPoints (isInFrustum 0.5 + SearchByProjection #1, th=3 RGBD / 1 mono)
 * + PoseOptimization + mnMatchesInliers count. Returns 1 iff mnMatchesInliers >= 30
 * (non-IMU; reloc-recent needs >=50, RECENTLY_LOST needs >10).
 */
int ngd_tracking_track_local_map(ngd_tracking *tk);

/* Sub-steps, exposed for testing: */
void ngd_tracking_update_local_map(ngd_tracking *tk);   /* UpdateLocalKeyFrames(+UpdateLocalPoints) */
void ngd_tracking_search_local_points(ngd_tracking *tk); /* SearchLocalPoints */

/* LocalMapping is a separate thread in the original; the pure-C core is
 * single-threaded, so KF insertion is driven explicitly by the caller:
 *   if (ngd_tracking_need_new_keyframe(tk)) ngd_tracking_create_new_key_frame(tk);
 *
 * ngd_tracking_need_new_keyframe: faithful port of Tracking.cc:3220-3360 (non-IMU
 * core). c1a/c1b/c1c + c2 + NGD c5 (c5 inert while mbStartOpticalFlow==0).
 * Requires tk->current, tk->refKF, tk->map, tk->mnMatchesInliers set.
 */
int ngd_tracking_need_new_keyframe(ngd_tracking *tk);

/* ngd_tracking_create_new_key_frame: faithful port of Tracking.cc:3363-3489
 * (non-IMU, non-fisheye). Creates a KeyFrame from the current frame (deep
 * copy), creates new MapPoints from RGB-D depth for unmatched features
 * (maxPoint=100, mThDepth cut), computes BoW, adds to KFDB + map, builds the
 * covisibility graph (UpdateConnections), pushes new MPs to lm->recent_mps,
 * and sets refKF / mnLastKeyFrameId / lm->currentKF. Requires tk->current,
 * tk->vocab, tk->kfdb, tk->map, tk->lm set. */
int ngd_tracking_create_new_key_frame(ngd_tracking *tk);

/* ngd_tracking_initialize: RGB-D/stereo first-frame init — wraps
 * ngd_stereo_initialization plus the map/kfdb/lm bookkeeping the original does
 * inline in StereoInitialization (Tracking.cc:2458-2492): computes the KF BoW,
 * adds KF + MPs to map + kfdb, pushes MPs to lm->recent, sets lm->currentKF,
 * refKF, mnLastKeyFrameId, mbCreatedKF=1. Requires tk->current built, tk->vocab /
 * kfdb / map / lm set. Returns 1 on success (N>500), 0 on failure. */
int ngd_tracking_initialize(ngd_tracking *tk);

/* ngd_tracking_track: top-level Track() dispatcher (Tracking.cc:1910-2455,
 * non-IMU RGBD simplified). Dispatches on mState:
 *   NOT_INITIALIZED -> ngd_tracking_initialize -> OK
 *   OK              -> TrackWithMotionModel (or TrackRefKF when !mbVelocity or
 *                      within reloc-recent window); TWM fail -> TrackRefKF.
 *                      Then TrackLocalMap; on success NeedNewKeyFrame+CreateNewKeyFrame.
 *   RECENTLY_LOST/  -> Relocalization; on success TrackLocalMap.
 *   LOST
 * On success sets mState=OK + velocity (mbVelocity=1); sets mbCreatedKF when a KF
 * was created. Returns 1 if tracking OK this frame, 0 if lost/failed.
 *
 * NOT modelled (non-IMU single-map phase 1): IMU/preintegration, onlyTracking/
 * mbVO dual-pose path, CheckReplacedInLastFrame, Atlas/multi-map +
 * ResetActiveMap/CreateMapInAtlas, timestamp-based RECENTLY_LOST->LOST timeout
 * (frame-count heuristic used instead), temporal MapPoints. The caller wires
 * tk->current + tk->last before each call and records the trajectory afterwards
 * (Tcr = current.pose * refKF.pose^-1, refKF, timestamp, lost). */
int ngd_tracking_track(ngd_tracking *tk);

#ifdef __cplusplus
}
#endif
#endif /* NGD_TRACKING_H */
