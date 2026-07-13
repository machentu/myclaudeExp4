#ifndef NGD_LOCALMAPPING_H
#define NGD_LOCALMAPPING_H

/*
 * ngd/localmapping.h — LocalMapping context (pure C, minimal).
 *
 * Faithful subset of ORB_SLAM3::LocalMapping (src/LocalMapping.cc) for the P1
 * scope: KeyFrame insertion plumbing + MapPointCulling. The thread loop is
 * collapsed to a single inline run() (the pure-C core is single-threaded).
 *
 * Stage 6 scope (KF insertion + MapPointCulling):
 *   - mlpRecentAddedMapPoints: recently-created MapPoints pending culling
 *   - mpCurrentKeyFrame: the KF being processed
 *   - map_point_culling (LocalMapping.cc:354-393)
 *   - run(): culling + stubs for CreateNewMapPoints / SearchInNeighbors /
 *     KeyFrameCulling / LocalBundleAdjustment (deferred to P2)
 *
 * KeyFrame creation itself (ProcessNewKeyFrame + depth-point MP creation) lives
 * in tracking.c::ngd_tracking_create_new_key_frame; this module owns the
 * per-frame culling pass and the recent-MP list.
 */

#include "ngd/map.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ngd_mappoint;
struct ngd_keyframe;
struct ngd_kfdb;

typedef struct {
    ngd_map *map;                 /* caller-owned */
    struct ngd_keyframe *currentKF;   /* mpCurrentKeyFrame */

    /* mlpRecentAddedMapPoints: growable list of recently-created MapPoints. */
    struct ngd_mappoint **recent_mps;
    int nRecent, capRecent;

    int mbMonocular;              /* cnThObs = mbMonocular ? 2 : 3 */

    /* KeyFrameDatabase (caller-owned, may be NULL) — used by KeyFrameCulling
     * to erase culled KFs from the inverted index (KeyFrame::SetBadFlag). */
    struct ngd_kfdb *kfdb;
} ngd_local_mapping;

void ngd_local_mapping_init(ngd_local_mapping *lm, ngd_map *map, int mbMonocular);
void ngd_local_mapping_free(ngd_local_mapping *lm);

/* Append a newly-created MapPoint to the recent list (called by
 * CreateNewKeyFrame / StereoInit for every new MP). */
void ngd_local_mapping_push_recent(ngd_local_mapping *lm, struct ngd_mappoint *mp);

/* MapPointCulling (LocalMapping.cc:354-393). Culls recent MPs by:
 *   - already bad           -> drop from list
 *   - found ratio < 0.25    -> SetBadFlag + drop
 *   - age>=2 & Observations()<=cnThObs -> SetBadFlag + drop
 *   - age>=3                -> drop (matured, leave the MP alive)
 * age = currentKF->mnId - mp->mnFirstKFid. cnThObs = mbMonocular ? 2 : 3. */
void ngd_local_mapping_map_point_culling(ngd_local_mapping *lm);

/* CreateNewMapPoints (LocalMapping.cc:396-720). For each of up to nn=10 (RGBD)
 * covisible neighbours: baseline check, SearchForTriangulation, per-match
 * parallax/Triangulate-or-UnprojectStereo/front-camera/reprojection/scale
 * checks; on success creates a MapPoint, adds observations to both KFs, adds
 * to the map and to the recent-MP list. Requires pKF + neighbours' BoW
 * computed (ComputeBoW, called defensively inside). */
void ngd_local_mapping_create_new_map_points(ngd_local_mapping *lm);

/* Set the KeyFrameDatabase used by KeyFrameCulling (optional; NULL skips the
 * inverted-index erase on culled KFs). */
void ngd_local_mapping_set_kfdb(ngd_local_mapping *lm, struct ngd_kfdb *kfdb);

/* SearchInNeighbors (LocalMapping.cc:722-831). Fuse the current KF's MapPoints
 * into its 1st/2nd-order covisibility neighbours and vice-versa (ORBmatcher::
 * Fuse, th=3). Recompute descriptors/normal on the current KF's surviving MPs,
 * rebuild its covisibility, and sweep bad (replaced) MPs from the map. */
void ngd_local_mapping_search_in_neighbors(ngd_local_mapping *lm);

/* KeyFrameCulling (LocalMapping.cc:910-1062), non-inertial RGBD. Cull a
 * covisible KF when >90% of its close stereo MapPoints are seen in >3 other
 * KFs at the same or finer scale (scaleLeveli <= scaleLevel+1). Culled KFs
 * are SetBadFlag'd (erased from map + covisibility + kfdb). */
void ngd_local_mapping_keyframe_culling(ngd_local_mapping *lm);

/* Run one LocalMapping pass (LocalMapping.cc:64-290, single-threaded): culling
 * -> CreateNewMapPoints -> SearchInNeighbors/Fuse -> KeyFrameCulling ->
 * LocalBundleAdjustment. Sets currentKF via the map's last KF if NULL. */
void ngd_local_mapping_run(ngd_local_mapping *lm);

#ifdef __cplusplus
}
#endif
#endif /* NGD_LOCALMAPPING_H */
