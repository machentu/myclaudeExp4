#ifndef NGD_APP_SYSTEM_H
#define NGD_APP_SYSTEM_H

// ngd_app/system.h -- SLAM system driver (zero-OpenCV variant).
//
// Phase v8seg_full: all image data passed as raw uint8_t*/float* buffers.
// Per frame: bgr_to_gray(imRGB), Yolov8SegDetector.detect (ONNX Runtime),
// masked-ORB extraction, ngd_tracking_track, record trajectory.
// OF tracking with pure-C LK + EPnP (no ngd_shell, no OpenCV).

#include "ngd_app/yaml.h"
#include "ngd_app/viewer.h"
#include "ngd/orb.h"
#include "ngd/calib.h"
#include "ngd/frame.h"
#include "ngd/keyframe.h"
#include "ngd/mappoint.h"
#include "ngd/map.h"
#if NGD_BOW
#include "ngd/kfdb.h"
#include "ngd/loopclosing.h"
#endif
#include "ngd/localmapping.h"
#include "ngd/tracking.h"
#include "ngd/bow.h"
#include "ngd/se3.h"
#include <stdint.h>

class Yolov8SegDetector;   // forward decl (ngd_app/yolov8_seg.h, global namespace)

struct ngd_app_system {
#if NGD_BOW
    ngd_bow_vocab     *vocab;     /* owned */
#endif
    ngd_orb_extractor  ex;        /* mpORBextractorLeft  (nFeatures)    */
    ngd_orb_extractor  ex_dyna;   /* mpORBextractorDyna  (nFeatures*1.5)*/
    ngd_cam_ctx        cam;
    ngd_app_config     cfg;

    ngd_map            map;
#if NGD_BOW
    ngd_kfdb           kfdb;
#endif
    ngd_local_mapping  lm;
#if NGD_BOW
    ngd_loopclosing    lc;
#endif
    ngd_tracking       tk;

    ngd_frame         *last_kf;
    ngd_frame         *prev_lk;

    ngd_se3            vel_lk;

    ngd_frame        **owned;
    int                nOwned, capOwned;

    ngd_se3           *relPoses;
    ngd_keyframe     **relRefs;
    double            *relTimes;
    int               *relLost;
    int                nRel, capRel;

    float             *relCov;
    int               *relCovValid;

    /* NGD mask state (end-to-end YOLOv8-seg, no LK propagation). */
    Yolov8SegDetector     *yolo;     /* owned; NULL if model unavailable */
    uint8_t           *gray_buf;       /* imgW*imgH, bgr_to_gray output */
    uint8_t           *last_gray;      /* previous frame gray (OF-pose LK source) */
    uint8_t           *last_mask;      /* previous frame mask (KF-branch dyna-extractor gate) */
    uint8_t           *mask_curr;      /* YOLOv8-seg mask applied to the current frame */
    int                has_last;
    uint64_t           frame_id;
    uint64_t           frame_num;

    /* stub viewer (trajectory record only, no OpenCV windows). */
    ngd_app_viewer     viewer;

    /* per-stage timing accumulators (ms) + call counts. */
    double t_yolo, t_mask, t_of, t_orb, t_track, t_lm, t_view;
    int    n_yolo, n_mask, n_of, n_orb, n_lm, n_kf;
#if NGD_BOW
    double t_lc;
    int    n_lc;
#endif
};

/* Load vocab + settings, build map/kfdb/lm/loopclosing/tracking + Yolov8SegDetector. */
int  ngd_app_system_init(ngd_app_system *s, const char *vocab_path, const char *yaml_path,
                         const char *yolo_dir);

/* Track one RGB-D frame (NGD masked-ORB variant).
 * imBGR: W*H*3 uint8 BGR interleaved (stride = 3*W).
 * imDepthMeters: W*H float metres (TUM raw / DepthMapFactor).
 * Returns 1 if OK. */
int  ngd_app_system_track_rgbd(ngd_app_system *s,
                                const uint8_t *imBGR, const float *imDepthMeters,
                                int w, int h, double timestamp);

int  ngd_app_system_save_trajectory_tum(ngd_app_system *s, const char *path);
int  ngd_app_system_save_keyframe_trajectory_tum(ngd_app_system *s, const char *path);
int  ngd_app_system_save_pose_covariance(ngd_app_system *s, const char *path);
void ngd_app_system_shutdown(ngd_app_system *s);
void ngd_app_system_print_timing(const ngd_app_system *s, int nFrames);

#endif /* NGD_APP_SYSTEM_H */
