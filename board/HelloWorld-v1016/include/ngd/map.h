#ifndef NGD_MAP_H
#define NGD_MAP_H

/*
 * ngd/map.h — Map container (pure C, minimal).
 *
 * Faithful subset of ORB_SLAM3::Map (include/Map.h, src/Map.cc:60-134) for the
 * LocalMapping path: a set of KeyFrames + a set of MapPoints, plus the KF-id
 * bookkeeping (mnInitKFid / mnMaxKFid / mnLastKeyFrameId) consumed by
 * NeedNewKeyFrame / CreateNewKeyFrame.
 *
 * The original uses std::set<KeyFrame*> / std::set<MapPoint*>; here we use a
 * dynamic array with linear dedup (P1-scale KF/MP counts make this acceptable).
 * Erase removes the pointer only (does not free the object, matching Map.cc).
 * Multi-map / Atlas, IMU flags, and map-update counters are not modelled.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ngd_keyframe;
struct ngd_mappoint;

typedef struct ngd_map {
    struct ngd_keyframe  **kfs;   /* mspKeyFrames */
    int  nKFs, capKFs;
    struct ngd_mappoint **mps;    /* mspMapPoints */
    int  nMPs, capMPs;

    uint64_t mnInitKFid;     /* id of the first KF in this map (Map::mnInitKFid) */
    uint64_t mnMaxKFid;      /* max KF id seen (Map::mnMaxKFid) */
    uint64_t mnLastKeyFrameId; /* set by CreateNewKeyFrame (Tracking::mnLastKeyFrameId) */
} ngd_map;

void ngd_map_init(ngd_map *m);
void ngd_map_free(ngd_map *m);   /* frees the arrays only, NOT the KF/MP objects */

/* AddKeyFrame (Map.cc:60-78): dedup-insert; first KF sets mnInitKFid; updates
 * mnMaxKFid. No-op (returns 0) if kf already in the set. Returns 1 on insert. */
int  ngd_map_add_keyframe(ngd_map *m, struct ngd_keyframe *kf);

/* AddMapPoint (Map.cc:80-84): dedup-insert. Returns 1 on insert. */
int  ngd_map_add_mappoint(ngd_map *m, struct ngd_mappoint *mp);

/* EraseMapPoint (Map.cc:109-116): removes the pointer; does not free the MP. */
void ngd_map_erase_mappoint(ngd_map *m, struct ngd_mappoint *mp);

/* EraseKeyFrame (Map.cc:118-134): removes the pointer; does not free the KF.
 * (Lower-id recomputation skipped — P1 does not rely on mpKFlowerID.) */
void ngd_map_erase_keyframe(ngd_map *m, struct ngd_keyframe *kf);

#ifdef __cplusplus
}
#endif
#endif /* NGD_MAP_H */
