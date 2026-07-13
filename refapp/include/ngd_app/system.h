#ifndef NGD_APP_SYSTEM_H
#define NGD_APP_SYSTEM_H

/*
 * ngd_app/system.h — SLAM system driver (NGD dynamic-mask variant).
 *
 * Phase 2: NGD dynamic filtering. Per frame (GrabImageRGBD-equivalent):
 *   cvtColor(imRGB) -> gray
 *   YoloDetector.detect(imRGB, imDepthMeters) -> mask_yolo (person mask)
 *   if (frame>0) ngd_mask_predict_current(last_gray, gray, last_mask, depth,
 *                                         ngd_shell_lk_default) -> mask (propagated)
 *   extractor = (mask non-zero) ? ex_dyna(nFeatures*1.5) : ex_left
 *   ngd_frame_build_rgbd_mask(gray, depth, mask, extractor) -> cur
 *   ngd_tracking_track(tk)  (TWM/TrackRefKF/TrackLocalMap/KF insertion)
 *   record trajectory; lm.run + loopclosing on KF created
 *
 * This is the "masked-ORB" NGD variant: dynamic objects (person) are filtered
 * from every frame's ORB, so tracking/mapping excludes them. The OF tracking
 * mode (ngd_app_track_with_optical_flow, built+tested) is wired in a follow-up
 * for exact NGD-SLAM algorithm parity. mbStartOpticalFlow stays 0 here, so TWM
 * uses the velocity seed as in phase 1.
 *
 * Single-threaded. Trajectory output matches SaveTrajectoryTUM (Twc, qx qy qz qw,
 * normalized so KF0 is at the origin).
 */

#include "ngd_app/yaml.h"
#include "ngd_app/viewer.h"
#include "ngd/orb.h"
#include "ngd/calib.h"
#include "ngd/frame.h"
#include "ngd/keyframe.h"
#include "ngd/mappoint.h"
#include "ngd/map.h"
#include "ngd/kfdb.h"
#include "ngd/localmapping.h"
#include "ngd/loopclosing.h"
#include "ngd/tracking.h"
#include "ngd/bow.h"
#include "ngd/se3.h"
#include <opencv2/core.hpp>
#include <stdint.h>

class YoloDetector;   // forward decl (refmain/YoloDetector.h, global namespace)

struct ngd_app_system {
    ngd_bow_vocab     *vocab;     /* owned */
    ngd_orb_extractor  ex;        /* mpORBextractorLeft  (nFeatures)    */
    ngd_orb_extractor  ex_dyna;   /* mpORBextractorDyna  (nFeatures*1.5)*/
    ngd_cam_ctx        cam;
    ngd_app_config     cfg;

    ngd_map            map;
    ngd_kfdb           kfdb;
    ngd_local_mapping  lm;
    ngd_loopclosing    lc;
    ngd_tracking       tk;

    /* NGD dual last-frame (Tracking::mLastFrame vs mLastFrameLK):
     *  - last_kf : last KF-branch ORB frame (mLastFrame, TWM/TrackRefKF source).
     *              Updated only when the KF branch runs (ngd_tracking_track).
     *  - prev_lk : last frame overall (mLastFrameLK, OF LK source). Updated every
     *              frame (OF inlier frame or KF ORB frame). */
    ngd_frame         *last_kf;
    ngd_frame         *prev_lk;

    ngd_se3            vel_lk;    /* mVelocityLK: per-frame OF velocity (cur.pose * prev_lk.pose^-1);
                                   * fresh every frame, used as the PnP init for the NEXT OF frame.
                                   * (Distinct from tk.velocity, the TWM velocity updated only at KFs.) */

    /* owned raw frames (the OF no-ORB frame and the KF-branch ORB frame are
     * distinct allocations; last_kf/prev_lk may alias the same frame). A frame
     * is freed once it is neither last_kf nor prev_lk. */
    ngd_frame        **owned;
    int                nOwned, capOwned;

    /* trajectory (Tracking::mlRelativeFramePoses / mlpReferences / mlFrameTimes
     * / mlbLost): relative pose Tcr, reference KF, timestamp, lost flag. */
    ngd_se3           *relPoses;
    ngd_keyframe     **relRefs;
    double            *relTimes;
    int               *relLost;
    int                nRel, capRel;

    /* NGD mask state. */
    YoloDetector          *yolo;     /* owned; NULL if model unavailable */
    uint8_t           *gray_buf;       /* imgW*imgH, cvtColor output */
    uint8_t           *last_gray;      /* previous frame gray (for PredictCurrentMask LK) */
    uint8_t           *last_mask;      /* previous YOLO mask (mImMaskLastKey) */
    uint8_t           *mask_pred;      /* PredictCurrentMask output */
    uint8_t           *mask_curr;      /* mask applied to the current frame */
    int                has_last;       /* last_gray/last_mask valid? */
    uint64_t           frame_id;
    uint64_t           frame_num;      /* mFrameNum (0-based) */

    /* live OpenCV 3D map viewer (no Pangolin/VTK). NGD_VIEWER=0 disables. */
    ngd_app_viewer     viewer;

    /* per-stage timing accumulators (ms) + call counts. Filled by
     * ngd_app_system_track_rgbd, printed by ngd_app_system_print_timing.
     * memset-0'd in init. For honest numbers run with NGD_VIEWER=0.
     *   t_yolo/t_mask : every frame (phase 2/3)
     *   t_of          : OF-branch frames only (phase 3, ~93%)
     *   t_orb/t_track : KF-branch frames (phase 1/2 every frame; phase 3 ~7%)
     *   t_lm/t_lc     : created-KF frames only (mbCreatedKF) */
    double t_yolo, t_mask, t_of, t_orb, t_track, t_lm, t_lc, t_view;
    int    n_yolo, n_mask, n_of, n_orb, n_lm, n_lc, n_kf;
};

/* Load vocab + settings, build map/kfdb/lm/loopclosing/tracking + YoloDetector.
 * yolo_dir may be NULL/empty -> no YOLO (falls back to no-mask tracking). */
int  ngd_app_system_init(ngd_app_system *s, const char *vocab_path, const char *yaml_path,
                         const char *yolo_dir);

/* Track one RGB-D frame (NGD masked-ORB variant). imRGB: CV_8UC3 color.
 * imDepthMeters: CV_32F metres (TUM raw / DepthMapFactor). Returns 1 if OK. */
int  ngd_app_system_track_rgbd(ngd_app_system *s,
                                const cv::Mat &imRGB, const cv::Mat &imDepthMeters,
                                double timestamp);

int  ngd_app_system_save_trajectory_tum(ngd_app_system *s, const char *path);
int  ngd_app_system_save_keyframe_trajectory_tum(ngd_app_system *s, const char *path);
void ngd_app_system_shutdown(ngd_app_system *s);

/* Print the per-stage timing breakdown accumulated during tracking. nFrames is
 * the number of frames processed (for per-frame amortization). */
void ngd_app_system_print_timing(const ngd_app_system *s, int nFrames);

#endif /* NGD_APP_SYSTEM_H */
