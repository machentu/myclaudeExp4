#ifndef NGD_MAPPOINT_H
#define NGD_MAPPOINT_H

/*
 * ngd/mappoint.h — MapPoint (pure C, minimal).
 *
 * Minimal subset of ORB_SLAM3::MapPoint (include/MapPoint.h) sufficient for
 * StereoInitialization and the next iteration's TrackReferenceKeyFrame:
 * world position, representative descriptor, mean viewing normal, observation
 * count, and the visible/found counters used by tracking/culling.
 *
 * The full observation map (map<KeyFrame*, tuple<left,right>>) is represented
 * here by a growable array of {keyframe, leftIdx, rightIdx} records — enough
 * to support AddObservation / Observations() / ComputeDistinctiveDescriptors /
 * UpdateNormalAndDepth as the original does. The covisibility-graph plumbing
 * lives on KeyFrame (next iteration).
 */

#include "ngd/math.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ngd_keyframe;
struct ngd_cam_ctx;

typedef struct {
    struct ngd_keyframe *kf;
    int leftIdx;
    int rightIdx;   /* -1 if absent (monocular obs) */
} ngd_observation;

typedef struct ngd_mappoint {
    uint64_t mnId;
    float worldPos[3];       /* mWorldPos (Eigen::Vector3f) */
    float normal[3];          /* mNormalVector */
    uint8_t descriptor[32];   /* mDescriptor (8 x uint32) */
    struct ngd_keyframe *refKF;   /* mpRefKF */

    ngd_observation *obs;
    int nObsRec;             /* number of observation records stored in obs[] */
    int nObs, capObs;          /* nObs mirrors MapPoint::nObs (stereo counts 2) */

    int mnVisible, mnFound;
    int mbBad;
    uint64_t mnFirstKFid;    /* mnFirstKFid (MapPoint ctor sets from refKF) — MapPointCulling age */
    float mfMinDistance, mfMaxDistance;

    /* ---- LocalBundleAdjustment scratch (MapPoint.h). Stamp = mnId of the KF
     * that triggered the current LBA; 0 = none. Dedup of lLocalMapPoints. */
    uint64_t mnBALocalForKF;

    /* ---- Fuse scratch (MapPoint.h). Stamp = mnId of the KF running
     * SearchInNeighbors; 0 = none. Dedup of vpFuseCandidates. */
    uint64_t mnFuseCandidateForKF;

    /* ---- track-projection cache, set by Frame::isInFrustum (Frame.cc:651) ----
     * Mono / RGBD path only (Nleft == -1); the fisheye right-camera members
     * (mbTrackInViewR / mTrackProjYR / mnTrackScaleLevelR / mTrackViewCosR) are
     * not modelled here. Consumed by ORBmatcher::SearchByProjection. */
    int    mbTrackInView;        /* visible in this frame's left camera */
    float  mTrackProjX, mTrackProjY, mTrackProjZ;   /* projected pixel + cam-z */
    float  mTrackDepth;          /* |Pc| (camera-centre distance) */
    float  mTrackProjXR;         /* uv.x - mbf/PcZ (stereo right-x for left-branch check) */
    int    mnTrackScaleLevel;    /* PredictScale result */
    float  mTrackViewCos;        /* PO . normal / dist */
    uint64_t mnLastFrameSeen;       /* SearchLocalPoints skip + TWM outlier stamp */
    uint64_t mnTrackReferenceForFrame; /* UpdateLocalPoints dedup stamp */
} ngd_mappoint;

/* Allocate + initialise (mirrors MapPoint(Pos, pRefKF, pMap) ctor at MapPoint.cc:39). */
ngd_mappoint *ngd_mappoint_new(const float pos[3], struct ngd_keyframe *refKF);
void          ngd_mappoint_free(ngd_mappoint *mp);

void ngd_mappoint_set_world_pos(ngd_mappoint *mp, const float pos[3]);
void ngd_mappoint_add_observation(ngd_mappoint *mp, struct ngd_keyframe *kf, int leftIdx, int rightIdx);
int  ngd_mappoint_observations(const ngd_mappoint *mp);   /* == nObs */

/* Pick the representative descriptor (median Hamming). With a single obs this
 * is that observation's descriptor (matches ComputeDistinctiveDescriptors). */
void ngd_mappoint_compute_distinctive_descriptors(ngd_mappoint *mp);

/* Mean viewing normal + scale invariance bounds. For a single observing KF
 * at camera centre Owi, normal = normalize(Pos - Owi). (MapPoint.cc:434+)
 * Scale bounds use the refKF observation's octave (MapPoint.cc:476-499):
 *   mfMaxDistance = dist * mvScaleFactor[level]
 *   mfMinDistance = mfMaxDistance / mvScaleFactor[nlevels-1]   */
void ngd_mappoint_update_normal_and_depth(ngd_mappoint *mp);

/* Scale-invariant distance bands consumed by isInFrustum (MapPoint.cc:510-519):
 *   GetMaxDistanceInvariance = 1.2 * mfMaxDistance
 *   GetMinDistanceInvariance = 0.8 * mfMinDistance   */
float ngd_mappoint_get_max_distance_invariance(const ngd_mappoint *mp);
float ngd_mappoint_get_min_distance_invariance(const ngd_mappoint *mp);

/* Predict the pyramid level a point at `currentDist` should appear at
 * (MapPoint.cc:539-554, Frame overload): ratio = mfMaxDistance / currentDist;
 * nScale = ceil(log(ratio) / mfLogScaleFactor), clamped to [0, nlevels-1]. */
int ngd_mappoint_predict_scale(const ngd_mappoint *mp, float currentDist,
                               const struct ngd_cam_ctx *cam);

void ngd_mappoint_increase_visible(ngd_mappoint *mp);   /* mnVisible++ */
void ngd_mappoint_increase_found(ngd_mappoint *mp);     /* mnFound++   */

/* GetFoundRatio (MapPoint.cc): mnFound / mnVisible (0 if mnVisible==0). */
float ngd_mappoint_get_found_ratio(const ngd_mappoint *mp);

/* Simplified SetBadFlag (MapPoint.cc:521+): mark mbBad and release the
 * observation array. Does NOT clean covisibility-graph back-references or
 * erase from KF mvpMapPoints (P1 scope; full cleanup deferred to P2). */
void ngd_mappoint_set_bad(ngd_mappoint *mp);

/* IsInKeyFrame (MapPoint.cc:428): true if `mp` has an observation in `kf`. */
int ngd_mappoint_is_in_keyframe(const ngd_mappoint *mp, struct ngd_keyframe *kf);

/* Replace (MapPoint.cc:248-300): replace `mp` with `pMP` across all its
 * observations. For each observing KF: if pMP is not yet in that KF, transfer
 * the observation (KF mvpMapPoints slot + pMP AddObservation); else just clear
 * mp's slot. pMP inherits mp's mnVisible/mnFound, recomputes descriptor/normal.
 * mp is marked bad and its obs array released (consistent with set_bad: does
 * NOT erase from the map — caller may ngd_map_erase_mappoint if desired). */
void ngd_mappoint_replace(ngd_mappoint *mp, ngd_mappoint *pMP);

/* EraseObservation (MapPoint.cc:168-201): remove the obs record for `kf`,
 * decrement nObs (stereo left w/ rightIdx>=0 -> 2, else 1), clear the KF's
 * mvpMapPoints[leftIdx] slot (EraseMapPointMatch), re-pick refKF if it was
 * `kf`, and SetBadFlag the MP when nObs<=2 ("only 2 observations or less"). */
void ngd_mappoint_erase_observation(ngd_mappoint *mp, struct ngd_keyframe *kf);

#ifdef __cplusplus
}
#endif
#endif /* NGD_MAPPOINT_H */
