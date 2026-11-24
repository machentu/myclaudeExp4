#ifndef NGD_APP_OPTFLOW_TRACK_H
#define NGD_APP_OPTFLOW_TRACK_H

/*
 * ngd_app/optflow_track.h — TrackWithOpticalFlow orchestration (NGD optical-
 * flow tracking mode). Faithful to Tracking.cc:2996-3102.
 *
 * LK-propagate the last frame's stable MapPoints (Observations>=3) into the
 * current frame, mask-filter, cv::solvePnPRansac with init = velocity*last.pose,
 * velocity-magnitude sanity reject (currVelo > 3*lastVelo && >0.05 -> reject,
 * fall back to the init guess). Sets s->tk.current->pose and s->tk.mnMatchesInliers.
 *
 * The two OpenCV primitives (ngd_shell_search_by_optical_flow +
 * ngd_shell_pnp_ransac) live in refmain/ngd_shell; this function is the SLAM
 * glue (MP collection + sanity + pose write-back) and lives in the driver
 * (links ngd_core + ngd_shell). Per the user's constraint, optical flow uses
 * OpenCV directly (LK + solvePnPRansac); the core's pure-C EPnP/MLPnP are NOT
 * used here.
 *
 * mask convention: uint8 w×h stride==w, 0=static/keep, !=0=dynamic/drop (same
 * as ngd_frame_build_rgbd_mask and ngd_shell optflow). May be NULL.
 *
 * Returns 1 iff mnMatchesInliers >= 20 (Tracking.cc:3101).
 */

#include "ngd_app/system.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int ngd_app_track_with_optical_flow(ngd_app_system *s, ngd_frame *last_lk,
                                     const uint8_t *last_gray, const uint8_t *curr_gray,
                                     const uint8_t *mask);

#ifdef __cplusplus
}
#endif
#endif /* NGD_APP_OPTFLOW_TRACK_H */
