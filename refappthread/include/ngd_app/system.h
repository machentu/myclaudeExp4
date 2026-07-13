#ifndef NGD_APP_SYSTEM_H
#define NGD_APP_SYSTEM_H

/*
 * ngd_app/system.h — multi-threaded SLAM system driver (NGD dynamic-mask).
 *
 * Mirrors the original NGD-SLAM 4-thread structure (src/System.cc:200-256):
 *   - Tracking  : the main thread (main.cpp loop), GrabImageRGBD per frame.
 *   - LocalMapping   : background thread, consumes KFs from a queue.
 *   - LoopClosing    : background thread, consumes KFs from a queue.
 *   - Viewer         : background thread, renders from a per-frame snapshot.
 * YOLO stays synchronous in Tracking (as in refapp); the original's 5th YOLO
 * thread is not replicated (user asked for the classic four).
 *
 * Thread safety: the refactor pure-C core (ngd_core.lib) has NO synchronization
 * of its own. A single coarse global mutex `map_mutex` guards ALL access to the
 * shared map / kfdb / KeyFrame / MapPoint graph. Tracking acquires it around
 * the map-touching steps (OF call, need_new_keyframe, ngd_tracking_track,
 * trajectory record, snapshot build); LocalMapping/LoopClosing acquire it for
 * their whole run; the Viewer acquires it to read the live map while drawing.
 * mask predict / YOLO / ORB extraction stay lock-free (they do not touch the
 * map), so the heavy LBA in LocalMapping overlaps with the next frames' ORB/OF.
 *
 * Per-frame flow (GrabImageRGBD-equivalent, NGD OF mode):
 *   cvtColor -> gray ; YoloDetector -> mask_yolo ; PredictCurrentMask -> mask
 *   mbStartOpticalFlow = (mState!=NOT_INITIALIZED) && (frame_num>=3)
 *   if (mbStartOpticalFlow): no-ORB frame + ngd_app_track_with_optical_flow
 *                             mbNeedKF = NeedNewKeyFrame()
 *   else: mbNeedKF = true
 *   if (mbNeedKF): rebuild cur = build_rgbd_mask ; ngd_tracking_track(tk)
 *   record trajectory ; if (mbCreatedKF) push KF to the LocalMapping queue
 *   build viewer snapshot (if enabled) ; signal viewer
 *
 * Trajectory output matches SaveTrajectoryTUM (Twc, qx qy qz qw, normalized so
 * KF0 is at the origin). mbCreatedKF KFs are processed asynchronously by the
 * LocalMapping/LoopClosing threads; shutdown drains both queues before saving
 * so all LBA / loop corrections land in the KF poses first.
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

#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <deque>

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

    /* ---- thread control ----
     * map_mutex: coarse global lock over the map/kfdb/KF/MP graph (see header).
     * The LM/LC queues have their own mutexes+cv (the worker threads must block
     * WITHOUT holding map_mutex, else they'd deadlock the whole system). */
    std::mutex              map_mutex;

    std::deque<ngd_keyframe*> lm_q;
    std::mutex              lm_q_mutex;
    std::condition_variable lm_q_cv;

    std::deque<ngd_keyframe*> lc_q;
    std::mutex              lc_q_mutex;
    std::condition_variable lc_q_cv;

    /* viewer single-slot snapshot: view_snap is guarded by map_mutex (written by
     * Tracking under map_mutex, read by the Viewer under map_mutex); view_pending
     * + view_cv are guarded by view_mutex (so the Viewer can wait without holding
     * map_mutex). */
    ngd_app_viewer_snapshot view_snap;
    int                     view_pending;
    std::mutex              view_mutex;
    std::condition_variable view_cv;

    std::thread         th_lm;
    std::thread         th_lc;
    std::thread         th_view;
    int                 threads_started;

    std::atomic<bool>   stop_flag;   /* set by shutdown: stop accepting new work */
    std::atomic<bool>   draining;    /* set by shutdown: workers drain queues then exit */

    /* NGD dual last-frame (Tracking::mLastFrame vs mLastFrameLK):
     *  - last_kf : last KF-branch ORB frame (mLastFrame, TWM/TrackRefKF source).
     *  - prev_lk : last frame overall (mLastFrameLK, OF LK source). */
    ngd_frame         *last_kf;
    ngd_frame         *prev_lk;

    ngd_se3            vel_lk;    /* mVelocityLK (per-frame OF velocity) */

    /* owned raw frames (only the transient no-ORB OF frame and the KF-branch
     * frame are tracked here; both are Tracking-private, never enqueued to LM/LC
     * which only receive ngd_keyframe*). A frame is freed once it is neither
     * last_kf nor prev_lk. KeyFrames (map.kfs) and MapPoints (map.mps) are freed
     * only at shutdown. */
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
    uint8_t           *gray_buf;
    uint8_t           *last_gray;
    uint8_t           *last_mask;
    uint8_t           *mask_pred;
    uint8_t           *mask_curr;
    int                has_last;
    uint64_t           frame_id;
    uint64_t           frame_num;

    ngd_app_viewer     viewer;

    /* per-stage timing accumulators (ms) + call counts.
     *   Tracking-thread fields (plain): t_yolo/t_mask/t_of/t_orb/t_track
     *   Worker-thread fields (atomic):  t_lm/t_lc/t_view (written by LM/LC/Viewer)
     * memset-0 is NOT used (the struct now holds mutexes/threads); init zeroes
     * these explicitly. For honest numbers run with NGD_VIEWER=0. */
    double t_yolo, t_mask, t_of, t_orb, t_track;
    int    n_yolo, n_mask, n_of, n_orb, n_kf;
    std::atomic<double> t_lm;
    std::atomic<double> t_lc;
    std::atomic<double> t_view;
    std::atomic<int>    n_lm;
    std::atomic<int>    n_lc;
    std::atomic<int>    n_view;
    std::atomic<int>    max_lm_q_depth;   /* high-water mark of the LM queue */
};

/* Load vocab + settings, build map/kfdb/lm/loopclosing/tracking + YoloDetector.
 * Does NOT spawn threads — call ngd_app_system_start afterwards. yolo_dir may be
 * NULL/empty -> no YOLO. */
int  ngd_app_system_init(ngd_app_system *s, const char *vocab_path, const char *yaml_path,
                         const char *yolo_dir);

/* Spawn the LocalMapping / LoopClosing / Viewer background threads. */
void ngd_app_system_start(ngd_app_system *s);

/* Track one RGB-D frame (NGD masked-ORB / OF variant). imRGB: CV_8UC3 color.
 * imDepthMeters: CV_32F metres. Returns 1 if OK. On mbCreatedKF, pushes the KF
 * to the LocalMapping queue (async); does NOT wait for LBA. */
int  ngd_app_system_track_rgbd(ngd_app_system *s,
                                const cv::Mat &imRGB, const cv::Mat &imDepthMeters,
                                double timestamp);

int  ngd_app_system_save_trajectory_tum(ngd_app_system *s, const char *path);
int  ngd_app_system_save_keyframe_trajectory_tum(ngd_app_system *s, const char *path);

/* Stop the worker threads (drain LM/LC queues first), join them, then free all
 * resources. Saves nothing — call save_trajectory_* first. */
void ngd_app_system_shutdown(ngd_app_system *s);

/* Print the per-stage timing breakdown accumulated during tracking. nFrames is
 * the number of frames processed (for per-frame amortization). */
void ngd_app_system_print_timing(const ngd_app_system *s, int nFrames);

#endif /* NGD_APP_SYSTEM_H */
