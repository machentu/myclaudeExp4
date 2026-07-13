#ifndef NGD_APP_VIEWER_H
#define NGD_APP_VIEWER_H

/*
 * ngd_app/viewer.h — live OpenCV 3D map viewer (no Pangolin / no VTK), threaded.
 *
 * Multi-threaded variant of refapp/viewer.h: the Tracking thread no longer
 * draws. Instead it builds a lightweight SNAPSHOT of the current frame's draw
 * inputs (under the global map_mutex) and hands it to the Viewer thread, which
 * renders from the snapshot + a live read of the map (under map_mutex). This
 * decouples drawing from Tracking and avoids use-after-free on the per-frame
 * `ngd_frame` (which Tracking may sweep once it is neither last_kf nor prev_lk).
 *
 * The snapshot copies only what draw needs from the transient frame:
 *   - cur pose / camera centre / keypoints / per-key outlier & mp-present flags
 *   - imRGB (refcounted cv::Mat clone) + dynamic mask (byte copy)
 * The map (point cloud, keyframes, covisibility) is still read LIVE by the
 * Viewer thread while it holds map_mutex — map.kfs / map.mps entries are not
 * freed until shutdown, so live reads under the lock are safe.
 *
 * NGD_VIEWER=0: Tracking never builds a snapshot; the Viewer thread idles on
 * its condition variable until shutdown (the 4-thread structure is preserved,
 * but draw is a no-op so timing runs are uncontended).
 *
 * Pure display: NEVER mutates SLAM state. Enabling it has no effect on the
 * trajectory (ATE is identical viewer-on vs viewer-off), modulo the unavoidable
 * scheduling differences of the threaded driver.
 */

#include <opencv2/core.hpp>
#include "ngd/math.h"
#include "ngd/se3.h"
#include "ngd/frame.h"
#include "ngd/keyframe.h"
#include "ngd/mappoint.h"
#include "ngd/map.h"
#include <stdint.h>
#include <vector>

/* Snapshot of one frame's draw inputs, produced by Tracking, consumed by Viewer.
 * POD-ish (holds cv::Mat + std::vector); built under map_mutex. */
typedef struct ngd_app_viewer_snapshot {
    int   valid;            /* 1 once a frame has been snapshotted */
    int   state;            /* tk.mState */
    int   nInliers;         /* tk.mnMatchesInliers */
    uint64_t frame_num;

    cv::Mat imRGB;          /* refcounted clone (BGR8 or gray) */
    std::vector<uint8_t> mask;  /* dynamic mask, row-major, maskW*maskH */
    int   maskW, maskH;

    /* current frame (copied so the frame object may be freed afterwards). */
    ngd_se3 cur_pose;       /* Tcw */
    ngd_vec3 cur_center;    /* camera centre in world (Twc.t) */
    int   cur_N;            /* cur->N */
    std::vector<ngd_keypoint> cur_keys;       /* copy of cur->keys[0..N) */
    std::vector<uint8_t>     cur_outlier;     /* size N: cur->mvbOutlier[i] ? 1:0 */
    std::vector<uint8_t>     cur_mp_present;  /* size N: mvpMapPoints[i] non-NULL ? 1:0 */
} ngd_app_viewer_snapshot;

typedef struct ngd_app_viewer {
    int   enabled;
    int   W3d, H3d;            /* 3D map canvas size (px) */

    /* Fixed viewpoint replicating the ORB-SLAM3 Pangolin Map viewer
     * (Viewer.cc: ModelViewLookAt(X,Y,Z, 0,0,0, 0,-1,0), Projection F). */
    float eye[3];
    float eye_home[3];
    float target[3];
    float up[3];
    float focal;

    float kf_size, cam_size;

    int   drag_last_x, drag_last_y, dragging;
    int   win_created;

    /* live camera trajectory: camera centres in the map/world frame. */
    ngd_vec3 *traj;
    int   n_traj, cap_traj;
} ngd_app_viewer;

void ngd_app_viewer_init(ngd_app_viewer *v, int enabled,
                         float vpX, float vpY, float vpZ, float vpF);
void ngd_app_viewer_free(ngd_app_viewer *v);

/* Build a snapshot from the live frame + buffers. Caller MUST hold map_mutex
 * (reads cur->keys / mvpMapPoints / mvbOutlier / pose, which LM/LC may mutate).
 * No-op (snap->valid=0) when v is disabled. */
void ngd_app_viewer_snapshot_build(ngd_app_viewer *v, ngd_app_viewer_snapshot *snap,
                                   const ngd_frame *cur,
                                   const cv::Mat &imRGB,
                                   const uint8_t *mask, int maskW, int maskH,
                                   int state, int nInliers, uint64_t frame_num);

/* Draw one frame from a snapshot + a live map read. Caller MUST hold map_mutex
 * (reads map.mps / map.kfs / connKFs, which LM/LC mutate). No-op when disabled
 * or snap not valid. */
void ngd_app_viewer_draw(ngd_app_viewer *v,
                         const ngd_app_viewer_snapshot *snap,
                         const ngd_map *map);

#endif /* NGD_APP_VIEWER_H */
