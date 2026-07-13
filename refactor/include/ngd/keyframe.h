#ifndef NGD_KEYFRAME_H
#define NGD_KEYFRAME_H

/*
 * ngd/keyframe.h — KeyFrame (pure C, minimal).
 *
 * Minimal subset of ORB_SLAM3::KeyFrame (include/KeyFrame.h) sufficient for
 * StereoInitialization and the next iteration's TrackReferenceKeyFrame /
 * TrackLocalMap: id, pose (+ camera centre), the per-keypoint arrays
 * (undistorted keys, descriptors, uRight, depth, mvpMapPoints), the ORB scale
 * pyramid, and the 64x48 feature grid (heap-allocated, like the original's
 * Frame.h:266 fix).
 *
 * Covisibility graph (mConnectedKeyFrameWeights / mvpOrderedConnectedKeyFrames
 * + GetBestCovisibilityKeyFrames) and the reloc scratch fields are included
 * for relocalization (Stage 2). The spanning-tree parent/children are NOT
 * modelled (reloc + UpdateLocalKeyFrames only need the covisibility graph). */

#include "ngd/math.h"
#include "ngd/se3.h"
#include "ngd/calib.h"
#include "ngd/orb.h"
#include "ngd/bow.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NGD_KF_GRID_COLS 64
#define NGD_KF_GRID_ROWS 48

struct ngd_mappoint;
struct ngd_map;
struct ngd_kfdb;

typedef struct {
    int *idx;     /* keypoint indices in this cell */
    int  n, cap;
} ngd_grid_cell;

typedef struct ngd_keyframe {
    uint64_t mnId;
    ngd_se3  pose;            /* mTcw */
    ngd_vec3 camCenter;       /* mOw (= -R^T t) */

    int  N;
    ngd_keypoint *keys;       /* mvKeysUn */
    uint8_t *descriptors;     /* N * 32 */
    float *uRight;            /* mvuRight (-1 = none) */
    float *depth;             /* mvDepth  (-1 = none) */
    struct ngd_mappoint **mvpMapPoints;  /* N */

    ngd_cam_ctx cam;

    /* 64x48 feature grid (heap). grid[col*ROWS + row]. */
    ngd_grid_cell *grid;
    float gridInvW, gridInvH;   /* mfGridElementWidthInv / HeightInv */

    /* BoW cache (KeyFrame::ComputeBoW, KeyFrame.cc:98-107). NULL until computed. */
    ngd_bowvec *bow;
    ngd_featvec *feat;

    /* ---- Covisibility graph (KeyFrame.cc:189-251) ----
     * connKFs[i]/connWeights[i] are parallel arrays kept sorted by weight
     * DESCENDING (== mvpOrderedConnectedKeyFrames / mvOrderedWeights). */
    struct ngd_keyframe **connKFs;
    int *connWeights;
    int  nConn, capConn;

    /* ---- Relocalization scratch (set by KeyFrameDatabase::DetectRelocalization
     * Candidates, KeyFrameDatabase.cc:733-845; fields declared in KeyFrame.h). */
    uint64_t mnRelocQuery;   /* mnId of the frame currently querying this KF (0 = none) */
    int      mnRelocWords;   /* #words shared with the querying frame */
    float    mRelocScore;    /* L1 score vs the querying frame */

    /* ---- LocalBundleAdjustment scratch (KeyFrame.h:338-340). Stamp = mnId of
     * the KF that triggered the current LBA; 0 = none. Dedup of lLocal/lFixed. */
    uint64_t mnBALocalForKF;
    uint64_t mnBAFixedForKF;

    /* ---- Fuse scratch (KeyFrame.h). Stamp = mnId of the KF running
     * SearchInNeighbors; 0 = none. Dedup of vpTargetKFs (incl. 2nd-order). */
    uint64_t mnFuseTargetForKF;

    int mbBad;   /* KeyFrame::mbBad — set by SetBadFlag (KeyFrameCulling). */
} ngd_keyframe;

/* Create a KeyFrame that owns copies of the given keypoint arrays (deep copy,
 * so the source buffers may be freed). Used by StereoInitialization. */
ngd_keyframe *ngd_keyframe_new(uint64_t id, ngd_se3 pose, const ngd_cam_ctx *cam,
                                int N, const ngd_keypoint *keys,
                                const uint8_t *descriptors,
                                const float *uRight, const float *depth);
void ngd_keyframe_free(ngd_keyframe *kf);

void ngd_keyframe_set_pose(ngd_keyframe *kf, ngd_se3 pose);
ngd_vec3 ngd_keyframe_get_camera_center(const ngd_keyframe *kf);

/* UnprojectStereo (Frame.cc:1043+): back-project keypoint i (with kf->depth>0)
 * to a world-space 3D point. Returns (0,0,0) if depth<=0. Used by
 * CreateNewMapPoints' low-parallax stereo branch. */
ngd_vec3 ngd_keyframe_unproject_stereo(const ngd_keyframe *kf, int i);

/* Copy keypoint i's 32-byte descriptor into out[32]. */
void ngd_keyframe_get_descriptor(const ngd_keyframe *kf, int i, uint8_t out[32]);

/* Grid: bin all keys (AssignFeaturesToGrid) and windowed retrieval
 * (GetFeaturesInArea). Mirrors Frame.cc:518-555 / 799-865. */
void ngd_keyframe_assign_grid(ngd_keyframe *kf);
int  ngd_keyframe_pos_in_grid(const ngd_keyframe *kf, float x, float y, int *col, int *row);
int  ngd_keyframe_get_features_in_area(const ngd_keyframe *kf, float x, float y, float r,
                                       int minLevel, int maxLevel, int *out, int max_out);

/* Compute (lazily) the BoW vector + feature vector from this KF's descriptors
 * (KeyFrame::ComputeBoW, levelsup=4). No-op if already computed. */
void ngd_keyframe_compute_bow(ngd_keyframe *kf, const ngd_bow_vocab *vocab);

/* ---- Covisibility graph (KeyFrame.cc:189-251, 379-475) ---- */

/* AddConnection (KeyFrame.cc:189-202): record/replace the weight to `nb` and
 * re-sort the ordered list descending. No-op if the weight is unchanged. */
void ngd_keyframe_add_connection(ngd_keyframe *kf, ngd_keyframe *nb, int weight);

/* UpdateConnections (KeyFrame.cc:379-475): rebuild the covisibility list from
 * scratch by scanning this KF's MapPoints' observations. Threshold = 15 shared
 * points; if none meet it, the single max-weight neighbour is kept. Calls
 * AddConnection on each retained neighbour (symmetric update). Spanning-tree
 * parent/children are NOT modelled. */
void ngd_keyframe_update_connections(ngd_keyframe *kf);

/* GetBestCovisibilityKeyFrames (KeyFrame.cc:243-251): fill out[] with up to N
 * highest-weight neighbours. Returns the number written. */
int  ngd_keyframe_get_best_covisibility_keyframes(const ngd_keyframe *kf, int N,
                                                  ngd_keyframe **out);

/* GetConnectedKeyFrames weight accessor: returns the cached weight to `nb`, or
 * 0 if absent (matches the mConnectedKeyFrameWeights map lookup). */
int  ngd_keyframe_get_weight(const ngd_keyframe *kf, const ngd_keyframe *nb);

/* TrackedMapPoints (KeyFrame.cc): count of this KF's non-bad MapPoints with
 * Observations() >= nMinObs. Used by Tracking::NeedNewKeyFrame (nRefMatches). */
int  ngd_keyframe_tracked_map_points(const ngd_keyframe *kf, int nMinObs);

/* isBad (KeyFrame.cc:681). */
int  ngd_keyframe_is_bad(const ngd_keyframe *kf);

/* EraseConnection (KeyFrame.cc:687-701): remove `target` from this KF's
 * covisibility list (shift-delete, preserves descending sort), nConn--. */
void ngd_keyframe_erase_connection(ngd_keyframe *kf, ngd_keyframe *target);

/* SetBadFlag (KeyFrame.cc:573-679), non-inertial simplified:
 *  - guard init KF (mnId == map->mnInitKFid) -> return
 *  - for each covisibility neighbour: EraseConnection(this) (symmetric trim)
 *  - for each mvpMapPoints[i]: EraseObservation(this) (may set the MP bad)
 *  - clear own covisibility list, set mbBad, erase from map (+ kfdb if given)
 * Spanning-tree parent/children reassignment is NOT modelled. `kfdb` may be
 * NULL (then the inverted-index erase is skipped). */
void ngd_keyframe_set_bad(ngd_keyframe *kf, struct ngd_map *map,
                           struct ngd_kfdb *kfdb);

#ifdef __cplusplus
}
#endif
#endif /* NGD_KEYFRAME_H */
