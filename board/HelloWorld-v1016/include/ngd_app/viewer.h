#ifndef NGD_APP_VIEWER_H
#define NGD_APP_VIEWER_H

/*
 * ngd_app/viewer.h — live OpenCV 3D map viewer (no Pangolin / no VTK).
 *
 * The original NGD-SLAM Viewer (src/Viewer.cc) uses Pangolin only for the 3D
 * map window (camera + keyframes + point cloud); its 2D frame/mask/LK windows
 * are already cv::imshow. This module replaces that 3D map window with a pure
 * OpenCV implementation: 3D->2D projection is done with cv::projectPoints
 * (calib3d), rendered onto a cv::Mat canvas and shown via cv::imshow.
 *
 * The camera/keyframe frustum drawing mirrors src/MapDrawer.cc (DrawCurrentCamera
 * / DrawKeyFrames / DrawMapPoints). Full line-referenced correspondence in
 * refapp/BENCHMARK.md appendix A.
 *
 * Two windows, refreshed once per tracked frame:
 *   "NGD refapp: 3D Map" — point cloud (coloured by height) + keyframe camera
 *                          frustums (blue; init KF red) + covisibility graph
 *                          (green) + live camera trajectory + current-camera
 *                          frustum (green). Controls: drag = orbit, wheel =
 *                          zoom, 'r' = reset to the YAML viewpoint.
 *   "NGD refapp: Frame"  — current RGB frame + masked-ORB keypoints
 *                          (green = inlier match, red = outlier, grey =
 *                          unmatched) + dynamic-mask overlay (red tint).
 *
 * Viewpoint: the default view replicates the original ORB-SLAM3 Pangolin Map
 * viewer (Viewer.cc) — eye at (Viewer.ViewpointX/Y/Z), looking at the origin,
 * up=(0,-1,0), focal=Viewer.ViewpointF. This matches the original's on-screen
 * scale (px/m) exactly, so camera motion is as prominent as in the original.
 * Drag orbits the eye around the target; wheel dollies; 'r' restores the YAML
 * default. The earlier auto-fit-to-scene-bbox zoomed out to fit the whole room
 * and made the ~1 m camera trajectory look tiny — that is gone.
 *
 * Pure display: NEVER mutates SLAM state. ngd_app_viewer is POD (memset-safe),
 * matching the rest of ngd_app_system. The viewer reads map.mps / map.kfs /
 * cur->keys / cur->pose only. Enabling it has no effect on the trajectory
 * (ATE is identical viewer-on vs viewer-off).
 */

#include <opencv2/core.hpp>
#include "ngd/math.h"
#include "ngd/se3.h"
#include "ngd/frame.h"
#include "ngd/keyframe.h"
#include "ngd/mappoint.h"
#include "ngd/map.h"
#include <stdint.h>

typedef struct ngd_app_viewer {
    int   enabled;
    int   W3d, H3d;            /* 3D map canvas size (px) */

    /* Fixed viewpoint replicating the ORB-SLAM3 Pangolin Map viewer
     * (Viewer.cc: ModelViewLookAt(X,Y,Z, 0,0,0, 0,-1,0), Projection F).
     * target/up/focal are fixed; eye is live (modified by drag/wheel) and
     * reset to eye_home on 'r'. */
    float eye[3];              /* live camera eye position (world) */
    float eye_home[3];         /* YAML default eye (Viewer.ViewpointX/Y/Z) */
    float target[3];           /* look-at point (origin, like Pangolin) */
    float up[3];               /* up hint ((0,-1,0), like Pangolin) */
    float focal;               /* Viewer.ViewpointF (px) */

    /* camera-frustum sizes (Viewer.KeyFrameSize / Viewer.CameraSize in TUM3.yaml).
     * Pangolin MapDrawer builds the frustum with h=w*0.75, z=w*0.6. */
    float kf_size, cam_size;

    /* mouse / window state */
    int   drag_last_x, drag_last_y, dragging;
    int   win_created;

    /* live camera trajectory: camera centres in the map/world frame. */
    ngd_vec3 *traj;
    int   n_traj, cap_traj;
} ngd_app_viewer;

/* enabled=0 makes every draw a no-op (for timing runs). vpX/Y/Z/F are the
 * Viewer.Viewpoint* values from the settings yaml (the original's default
 * camera); they set the initial eye and the 'r' reset target. */
void ngd_app_viewer_init(ngd_app_viewer *v, int enabled,
                         float vpX, float vpY, float vpZ, float vpF);
void ngd_app_viewer_free(ngd_app_viewer *v);

/* Draw one frame. map/cur may be NULL (then only what exists is drawn).
 * mask is the dynamic mask applied this frame (uint8 maskW*maskH), may be NULL.
 * state = tk.mState, nInliers = tk.mnMatchesInliers. */
void ngd_app_viewer_draw(ngd_app_viewer *v,
                         const ngd_map *map,
                         const ngd_frame *cur,
                         const cv::Mat &imRGB,
                         const uint8_t *mask, int maskW, int maskH,
                         int state, int nInliers, uint64_t frame_num);

#endif /* NGD_APP_VIEWER_H */
