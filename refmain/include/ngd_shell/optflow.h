#ifndef NGD_SHELL_OPTFLOW_H
#define NGD_SHELL_OPTFLOW_H

/*
 * ngd_shell/optflow.h — OpenCV optical-flow SLAM tracking (thin shell).
 *
 * Faithful port of the two OpenCV-dependent pieces of NGD-SLAM's optical-flow
 * tracking mode, which the pure-C core deliberately does NOT contain:
 *
 *   1. SearchByOpticalFlow (src/ORBmatcher.cc:2016-2082):
 *      propagate the last frame's keypoints (those with a stable MapPoint,
 *      Observations>=3) into the current frame via LK, dropping points that
 *      fall out of bounds or land on the dynamic mask.
 *
 *   2. TrackWithOpticalFlow's PnP (src/Tracking.cc:3025-3053):
 *      cv::solvePnPRansac on the (3D MapPoint, 2D tracked keypoint)
 *      correspondences to recover the current camera pose.
 *
 * Per the user's constraint, optical-flow tracking uses OpenCV directly
 * (calcOpticalFlowPyrLK + solvePnPRansac); the core's pure-C EPnP/MLPnP are
 * NOT used here. The API is flat arrays (C-linkage) so the C core or a C demo
 * can drive it; out_Tcw is row-major SE3 homogeneous [R|t;0 1] consistent with
 * the core's ngd_se3 convention.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* SearchByOpticalFlow (ORBmatcher.cc:2016-2082, left branch).
 *   last_gray/curr_gray : uint8 grayscale, w×h, `stride` bytes/row.
 *   mask                : uint8, w×h (may be NULL = no mask). 0 = keep, !=0 = dynamic (drop).
 *   last_keys[2*n_last] : (x,y) of last frame keypoints.
 *   last_mp_valid[n_last]: 1 if the keypoint has a stable MapPoint (Observations>=3), else 0 (skip).
 *   out_keys[2*n_last]  : tracked (x,y) in current frame (caller buffer).
 *   out_mp_idx[n_last]  : index into the last_keys array of each tracked point (caller buffer).
 * Returns the number of successfully tracked points (<= n_last). */
int ngd_shell_search_by_optical_flow(const uint8_t *last_gray, const uint8_t *curr_gray,
                                     const uint8_t *mask, int w, int h, int stride,
                                     const float *last_keys, const int *last_mp_valid, int n_last,
                                     float *out_keys, int *out_mp_idx);

/* cv::solvePnPRansac (Tracking.cc:3040, SOLVEPNP_ITERATIVE).
 *   mapPoints[3*n] : (X,Y,Z) world MapPoints.
 *   imgPoints[2*n] : (u,v) tracked keypoints.
 *   K[9]           : row-major intrinsics (fx,0,cx, 0,fy,cy, 0,0,1).
 *   dist[5]        : distortion (may be NULL = zero).
 *   init_Tcw[16]   : initial pose guess, row-major homogeneous (may be NULL = usePnP=false).
 *   iter/reprojErr/conf : RANSAC params (original: 100, 5.0, 0.99).
 *   inliers_out[n] : 1 = inlier (caller buffer, may be NULL).
 *   out_Tcw[16]    : solved pose, row-major homogeneous [R|t;0 1].
 * Returns the inlier count, or -1 on failure (n<4 or solvePnPRansac error). */
int ngd_shell_pnp_ransac(const float *mapPoints, const float *imgPoints, int n,
                         const float K[9], const float *dist,
                         const float *init_Tcw, int iter, float reprojErr, float conf,
                         int *inliers_out, float out_Tcw[16]);

#ifdef __cplusplus
}
#endif
#endif /* NGD_SHELL_OPTFLOW_H */
