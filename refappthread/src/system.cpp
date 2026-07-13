// ngd_app/system.cpp — multi-threaded SLAM system driver (NGD OF mode).
//
// 4-thread structure mirroring the original NGD-SLAM (src/System.cc:200-256):
//   Tracking (main) + LocalMapping + LoopClosing + Viewer threads.
//
// GrabImageRGBD-equivalent (Tracking.cc:1532-1675), faithful NGD optical-flow
// mode. The single change vs the single-threaded refapp driver: on mbCreatedKF
// the KeyFrame is pushed to the LocalMapping queue (async) instead of running
// lm.run + loopclosing inline. A coarse global map_mutex guards all map/kfdb/KF/
// MP access (the pure-C core has no synchronization of its own). mask predict /
// YOLO / ORB extraction stay lock-free so LM's LBA overlaps with later frames.
#include "ngd_app/system.h"
#include "ngd_app/imio.h"
#include "ngd_app/optflow_track.h"
#include "ngd_shell/lk_shell.h"
#include "ngd_shell/YoloDetector.h"

#include <opencv2/imgproc.hpp>
#include <opencv2/core.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <chrono>

/* ---------------- trajectory list (Tracking-only) ---------------- */
static void push_rel(ngd_app_system *s, ngd_se3 Tcr, ngd_keyframe *ref,
                     double ts, int lost)
{
    if (s->nRel == s->capRel) {
        int nc = s->capRel ? s->capRel * 2 : 256;
        s->relPoses = (ngd_se3 *)realloc(s->relPoses, (size_t)nc * sizeof(ngd_se3));
        s->relRefs  = (ngd_keyframe **)realloc(s->relRefs, (size_t)nc * sizeof(ngd_keyframe *));
        s->relTimes = (double *)realloc(s->relTimes, (size_t)nc * sizeof(double));
        s->relLost  = (int *)realloc(s->relLost, (size_t)nc * sizeof(int));
        s->capRel = nc;
    }
    s->relPoses[s->nRel] = Tcr;
    s->relRefs[s->nRel]  = ref;
    s->relTimes[s->nRel] = ts;
    s->relLost[s->nRel]  = lost;
    s->nRel++;
}

static int nonzero_count(const uint8_t *m, int n) {
    int c = 0; for (int i = 0; i < n; ++i) if (m[i]) ++c; return c;
}

/* ---------------- owned-frame lifetime (Tracking-only) ----------------
 * Frames are Tracking-private (LM/LC only ever receive ngd_keyframe*). A frame
 * is freed once it is neither last_kf nor prev_lk. KeyFrames (map.kfs) and
 * MapPoints (map.mps) are freed only at shutdown. */
static void own_frame(ngd_app_system *s, ngd_frame *f)
{
    if (s->nOwned == s->capOwned) {
        int nc = s->capOwned ? s->capOwned * 2 : 8;
        s->owned = (ngd_frame **)realloc(s->owned, (size_t)nc * sizeof(ngd_frame *));
        s->capOwned = nc;
    }
    s->owned[s->nOwned++] = f;
}
static void sweep_owned(ngd_app_system *s)
{
    int w = 0;
    for (int i = 0; i < s->nOwned; ++i) {
        ngd_frame *f = s->owned[i];
        if (f == s->last_kf || f == s->prev_lk) {
            s->owned[w++] = f;
        } else {
            ngd_frame_free(f); free(f);
        }
    }
    s->nOwned = w;
}

/* ---------------- init (no memset: struct holds mutexes/threads/atomics) ---- */
int ngd_app_system_init(ngd_app_system *s, const char *vocab_path, const char *yaml_path,
                        const char *yolo_dir)
{
    /* Zero the POD fields explicitly; the C++ members (mutex/cv/thread/atomic/
     * deque/snapshot) are already default-constructed by `ngd_app_system SLAM;`. */
    s->vocab = NULL;
    s->last_kf = NULL; s->prev_lk = NULL;
    s->vel_lk = ngd_se3_identity();
    s->owned = NULL; s->nOwned = 0; s->capOwned = 0;
    s->relPoses = NULL; s->relRefs = NULL; s->relTimes = NULL; s->relLost = NULL;
    s->nRel = 0; s->capRel = 0;
    s->yolo = NULL;
    s->gray_buf = NULL; s->last_gray = NULL; s->last_mask = NULL;
    s->mask_pred = NULL; s->mask_curr = NULL;
    s->has_last = 0; s->frame_id = 0; s->frame_num = 0;
    s->view_pending = 0; s->threads_started = 0;
    s->t_yolo = s->t_mask = s->t_of = s->t_orb = s->t_track = 0.0;
    s->n_yolo = s->n_mask = s->n_of = s->n_orb = s->n_kf = 0;
    s->t_lm.store(0.0); s->t_lc.store(0.0); s->t_view.store(0.0);
    s->n_lm.store(0); s->n_lc.store(0); s->n_view.store(0);
    s->max_lm_q_depth.store(0);
    s->stop_flag.store(false); s->draining.store(false);
    s->view_snap.valid = 0;

    s->vocab = ngd_bow_vocab_load_binary(vocab_path);
    if (!s->vocab) s->vocab = ngd_bow_vocab_load_text(vocab_path);
    if (!s->vocab) { fprintf(stderr, "system: failed to load vocabulary %s\n", vocab_path); return 0; }
    fprintf(stderr, "system: vocabulary loaded (%u nodes, %u words)\n",
            s->vocab->n_nodes, s->vocab->n_words);

    if (!ngd_app_load_yaml(yaml_path, &s->cfg)) { fprintf(stderr, "system: failed to load settings %s\n", yaml_path); return 0; }
    ngd_app_build(&s->cfg, &s->ex, &s->cam);
    ngd_orb_init(&s->ex_dyna, (int)(s->cfg.nFeatures * 1.5f + 0.5f),
                 s->cfg.scaleFactor, s->cfg.nLevels, s->cfg.iniThFAST, s->cfg.minThFAST);

    ngd_map_init(&s->map);
    ngd_kfdb_init(&s->kfdb, s->vocab);
    ngd_local_mapping_init(&s->lm, &s->map, /*mbMonocular=*/0);
    ngd_local_mapping_set_kfdb(&s->lm, &s->kfdb);
    ngd_loopclosing_init(&s->lc, &s->map, &s->kfdb, s->vocab, /*bFixScale=*/1);
    ngd_tracking_init(&s->tk);
    s->tk.vocab = s->vocab; s->tk.kfdb = &s->kfdb; s->tk.map = &s->map; s->tk.lm = &s->lm;
    s->tk.sensor = NGD_SENSOR_RGBD;
    s->tk.mState = NGD_STATE_NOT_INITIALIZED;
    s->tk.mMinFrames = 0;
    s->tk.mMaxFrames = (int)(s->cfg.fps + 0.5f);

    const int npix = s->cam.imgW * s->cam.imgH;
    s->gray_buf  = (uint8_t *)calloc((size_t)npix, 1);
    s->last_gray = (uint8_t *)calloc((size_t)npix, 1);
    s->last_mask = (uint8_t *)calloc((size_t)npix, 1);
    s->mask_pred = (uint8_t *)calloc((size_t)npix, 1);
    s->mask_curr = (uint8_t *)calloc((size_t)npix, 1);

    s->yolo = NULL;
    if (yolo_dir && yolo_dir[0]) {
        std::string d(yolo_dir);
        std::string cfg = d + "/yolo-fastest-xl.cfg";
        std::string w   = d + "/yolo-fastest-xl.weights";
        std::string nm  = d + "/coco.names";
        FILE *fc = std::fopen(cfg.c_str(), "rb"), *fw = std::fopen(w.c_str(), "rb"), *fn = std::fopen(nm.c_str(), "rb");
        if (fc) std::fclose(fc); if (fw) std::fclose(fw); if (fn) std::fclose(fn);
        if (fc && fw && fn) {
            YoloDetector::Params p;
            p.classesFile = nm; p.modelCfg = cfg; p.modelWeights = w;
            p.maxBoxRatio = 0.0f;
            s->yolo = new YoloDetector(p);
            if (!s->yolo->initialized()) { delete s->yolo; s->yolo = NULL; fprintf(stderr, "system: YoloDetector init failed\n"); }
            else fprintf(stderr, "system: YoloDetector loaded (%s)\n", cfg.c_str());
        } else fprintf(stderr, "system: YOLO model not found in %s\n", yolo_dir);
    }

    const char *vw = getenv("NGD_VIEWER");
    int ven = (vw && vw[0]=='0') ? 0 : 1;
    ngd_app_viewer_init(&s->viewer, ven,
                        s->cfg.viewpointX, s->cfg.viewpointY, s->cfg.viewpointZ, s->cfg.viewpointF);
    fprintf(stderr, "system: viewer %s (NGD_VIEWER=0 to disable)\n", ven?"on":"off");
    return 1;
}

/* Build a no-ORB frame shell (cam + id + identity pose) for the OF branch. */
static ngd_frame *new_no_orb_frame(ngd_app_system *s, uint64_t fid)
{
    ngd_frame *f = (ngd_frame *)calloc(1, sizeof(ngd_frame));
    f->cam = s->cam;
    f->mnId = fid;
    f->pose = ngd_se3_identity();
    return f;
}

/* ---------------- worker threads ---------------- */

static void lm_thread_fn(ngd_app_system *s)
{
    while (true) {
        ngd_keyframe *kf = NULL;
        {
            std::unique_lock<std::mutex> qlk(s->lm_q_mutex);
            s->lm_q_cv.wait(qlk, [s]{ return !s->lm_q.empty() || s->stop_flag.load(); });
            if (!s->lm_q.empty()) { kf = s->lm_q.front(); s->lm_q.pop_front(); }
            else break;   /* empty + stop: drain complete */
        }
        auto t0 = std::chrono::steady_clock::now();
        {
            std::lock_guard<std::mutex> mlk(s->map_mutex);
            s->lm.currentKF = kf;
            ngd_local_mapping_run(&s->lm);
        }
        /* hand the processed KF to LoopClosing */
        {
            std::lock_guard<std::mutex> qlk(s->lc_q_mutex);
            s->lc_q.push_back(kf);
        }
        s->lc_q_cv.notify_one();
        auto t1 = std::chrono::steady_clock::now();
        s->t_lm.store(s->t_lm.load() + std::chrono::duration<double, std::milli>(t1 - t0).count());
        s->n_lm.store(s->n_lm.load() + 1);
    }
}

static void lc_thread_fn(ngd_app_system *s)
{
    while (true) {
        ngd_keyframe *kf = NULL;
        {
            std::unique_lock<std::mutex> qlk(s->lc_q_mutex);
            s->lc_q_cv.wait(qlk, [s]{ return !s->lc_q.empty() || s->stop_flag.load(); });
            if (!s->lc_q.empty()) { kf = s->lc_q.front(); s->lc_q.pop_front(); }
            else break;
        }
        auto t0 = std::chrono::steady_clock::now();
        {
            std::lock_guard<std::mutex> mlk(s->map_mutex);
            /* ngd_loopclosing_run = pop + detect + correct; we bypass its internal
             * kfQueue (not thread-safe) and call detect/correct directly on the
             * KF we popped from our own queue. */
            if (ngd_loopclosing_detect_common_regions(&s->lc, kf))
                ngd_loopclosing_correct_loop(&s->lc);
        }
        auto t1 = std::chrono::steady_clock::now();
        s->t_lc.store(s->t_lc.load() + std::chrono::duration<double, std::milli>(t1 - t0).count());
        s->n_lc.store(s->n_lc.load() + 1);
    }
}

static void viewer_thread_fn(ngd_app_system *s)
{
    while (true) {
        {
            std::unique_lock<std::mutex> vlk(s->view_mutex);
            s->view_cv.wait(vlk, [s]{ return s->view_pending || s->stop_flag.load(); });
            if (!s->view_pending && s->stop_flag.load()) break;
        }
        auto t0 = std::chrono::steady_clock::now();
        {
            /* draw reads view_snap (written by Tracking under map_mutex) + the
             * live map; both guarded by map_mutex. cv::waitKey(1) is ~1ms; only
             * runs when the viewer is enabled (NGD_VIEWER!=0). */
            std::lock_guard<std::mutex> mlk(s->map_mutex);
            ngd_app_viewer_draw(&s->viewer, &s->view_snap, &s->map);
        }
        auto t1 = std::chrono::steady_clock::now();
        {
            std::lock_guard<std::mutex> vlk(s->view_mutex);
            s->view_pending = 0;   /* drop-if-stale: a newer frame may overwrite */
        }
        s->t_view.store(s->t_view.load() + std::chrono::duration<double, std::milli>(t1 - t0).count());
        s->n_view.store(s->n_view.load() + 1);
    }
}

void ngd_app_system_start(ngd_app_system *s)
{
    if (s->threads_started) return;
    s->th_lm = std::thread(lm_thread_fn, s);
    s->th_lc = std::thread(lc_thread_fn, s);
    s->th_view = std::thread(viewer_thread_fn, s);
    s->threads_started = 1;
    fprintf(stderr, "system: started LocalMapping / LoopClosing / Viewer threads\n");
}

/* ---------------- per-frame tracking (main thread) ---------------- */
int ngd_app_system_track_rgbd(ngd_app_system *s,
                                const cv::Mat &imRGB, const cv::Mat &imDepthMeters,
                                double timestamp)
{
    const int W = s->cam.imgW, H = s->cam.imgH, N = W * H;
    if (imRGB.cols != W || imRGB.rows != H || imDepthMeters.cols != W || imDepthMeters.rows != H) {
        fprintf(stderr, "system: image/depth size mismatch\n"); return 0;
    }
    const uint64_t fid = s->frame_id++;

    s->tk.mbCreatedKF = 0;   /* clear (OF-only frames skip Track()'s reset) */

    cv::Mat grayMat;
    if (imRGB.channels() == 3) cv::cvtColor(imRGB, grayMat, cv::COLOR_BGR2GRAY);
    else                       grayMat = imRGB;
    if (grayMat.type() != CV_8U) grayMat.convertTo(grayMat, CV_8U);
    const uint8_t *gray = grayMat.ptr<uint8_t>();
    const float *depth = imDepthMeters.ptr<float>();

    /* ---- YOLO person mask (lock-free: no map access) ---- */
    uint8_t *mask_yolo = s->mask_curr;
    memset(mask_yolo, 0, (size_t)N);
    if (s->yolo) {
        auto y_a = std::chrono::steady_clock::now();
        cv::Mat m = s->yolo->detect(imRGB, imDepthMeters);
        auto y_b = std::chrono::steady_clock::now();
        s->t_yolo += std::chrono::duration<double, std::milli>(y_b - y_a).count();
        s->n_yolo++;
        if (!m.empty() && m.rows == H && m.cols == W)
            for (int y = 0; y < H; ++y) memcpy(mask_yolo + y * W, m.ptr<uint8_t>(y), W);
    }

    /* ---- current mask: PredictCurrentMask or YOLO (lock-free) ---- */
    uint8_t *mask;
    if (s->frame_num > 0 && s->has_last) {
        memset(s->mask_pred, 0, (size_t)N);
        auto m_a = std::chrono::steady_clock::now();
        ngd_mask_predict_current(s->last_gray, gray, s->last_mask, depth,
                                 W, H, W, ngd_shell_lk_default, NULL, s->mask_pred);
        auto m_b = std::chrono::steady_clock::now();
        s->t_mask += std::chrono::duration<double, std::milli>(m_b - m_a).count();
        s->n_mask++;
        mask = s->mask_pred;
    } else {
        mask = mask_yolo;
    }

    const char *phs = getenv("NGD_PHASE");
    int force_no_of = (phs && (phs[0]=='1' || phs[0]=='2'));
    int mbOF = !force_no_of && (s->tk.mState != NGD_STATE_NOT_INITIALIZED) && (s->frame_num >= 3);
    s->tk.mbStartOpticalFlow = mbOF;

    ngd_se3 prev_lk_pose = s->prev_lk ? s->prev_lk->pose : ngd_se3_identity();

    ngd_frame *cur = NULL;
    int mbNeedKF;

    if (mbOF) {
        /* ---- OF branch: no-ORB frame + TrackWithOpticalFlow ----
         * Both the OF call and NeedNewKeyFrame touch the map (prev_lk MPs,
         * refKF tracked MPs, map.nKFs) -> hold map_mutex. LK+PnP compute runs
         * inside ngd_app_track_with_optical_flow; LM/LC are blocked for its
         * ~10ms but the mask+YOLO(~28ms) of the NEXT frames overlap with LM. */
        cur = new_no_orb_frame(s, fid);
        own_frame(s, cur);
        s->tk.current = cur;
        auto of_a = std::chrono::steady_clock::now();
        {
            std::lock_guard<std::mutex> mlk(s->map_mutex);
            ngd_app_track_with_optical_flow(s, s->prev_lk, s->last_gray, gray, mask);
            mbNeedKF = (s->tk.mnMatchesInliers > 0) ? ngd_tracking_need_new_keyframe(&s->tk) : 1;
        }
        auto of_b = std::chrono::steady_clock::now();
        s->t_of += std::chrono::duration<double, std::milli>(of_b - of_a).count();
        s->n_of++;
    } else {
        mbNeedKF = 1;
    }

    /* ---- KF branch: ORB extract (lock-free) + Track (under map_mutex) ---- */
    if (mbNeedKF) {
        ngd_se3 Tcw = cur ? cur->pose : ngd_se3_identity();

        ngd_orb_extractor *ex = &s->ex;
        if (nonzero_count(mask, N) > 0 || nonzero_count(s->last_mask, N) > 0) ex = &s->ex_dyna;

        const int MAXK = (int)(s->cfg.nFeatures * 1.5f + 0.5f) * 2;
        ngd_keypoint *kps = (ngd_keypoint *)malloc(sizeof(ngd_keypoint) * MAXK);
        uint8_t *desc = (uint8_t *)malloc((size_t)MAXK * 32);
        ngd_frame *kf_frame = (ngd_frame *)calloc(1, sizeof(ngd_frame));
        auto o_a = std::chrono::steady_clock::now();
        int Nfeat = ngd_frame_build_rgbd_mask(kf_frame, ex, &s->cam, gray, W, H, W, depth, W,
                                              fid, mask, kps, MAXK, desc);
        auto o_b = std::chrono::steady_clock::now();
        s->t_orb += std::chrono::duration<double, std::milli>(o_b - o_a).count();
        s->n_orb++;
        free(kps); free(desc);
        if (Nfeat <= 0) { ngd_frame_free(kf_frame); free(kf_frame); return 0; }

        if (mbOF) ngd_frame_set_pose(kf_frame, Tcw);
        cur = kf_frame;
        own_frame(s, cur);

        auto t_a = std::chrono::steady_clock::now();
        {
            /* ngd_tracking_track touches the map (TrackLocalMap + CreateNewKeyFrame
             * which adds KF/MPs to map/kfdb + pushes recent MPs to lm). */
            std::lock_guard<std::mutex> mlk(s->map_mutex);
            s->tk.current = cur;
            s->tk.last = s->last_kf;
            ngd_tracking_track(&s->tk);
            s->last_kf = cur;
        }
        auto t_b = std::chrono::steady_clock::now();
        s->t_track += std::chrono::duration<double, std::milli>(t_b - t_a).count();
    }

    /* ---- record trajectory + capture created KF (under map_mutex) ---- */
    ngd_keyframe *newKF = NULL;
    {
        std::lock_guard<std::mutex> mlk(s->map_mutex);
        if (s->tk.mState == NGD_STATE_OK || s->tk.mState == NGD_STATE_RECENTLY_LOST) {
            ngd_se3 Tcr = ngd_se3_multiply(cur->pose, ngd_se3_inverse(s->tk.refKF->pose));
            push_rel(s, Tcr, s->tk.refKF, timestamp, s->tk.mState == NGD_STATE_LOST);
        }
        if (s->tk.mbCreatedKF) {
            s->n_kf++;
            newKF = s->lm.currentKF;   /* set by create_new_key_frame under the lock */
        }
    }

    /* ---- hand the KF to LocalMapping (async; does NOT wait for LBA) ---- */
    if (newKF) {
        {
            std::lock_guard<std::mutex> qlk(s->lm_q_mutex);
            s->lm_q.push_back(newKF);
            int d = (int)s->lm_q.size();
            if (d > s->max_lm_q_depth.load()) s->max_lm_q_depth.store(d);
        }
        s->lm_q_cv.notify_one();
    }

    /* ---- update last frames + mask state (Tracking-private) ---- */
    s->vel_lk = ngd_se3_multiply(cur->pose, ngd_se3_inverse(prev_lk_pose));
    s->prev_lk = cur;
    sweep_owned(s);

    memcpy(s->last_gray, gray, (size_t)N);
    memcpy(s->last_mask, mask_yolo, (size_t)N);
    s->has_last = 1;
    s->frame_num++;

    if ((s->frame_num % 50) == 0)
        fprintf(stderr, "frame %llu  state=%d  KFs=%d MPs=%d  maskYolo=%d  OF=%d  lmQ=%d\n",
                (unsigned long long)s->frame_num, s->tk.mState, s->map.nKFs, s->map.nMPs,
                nonzero_count(mask_yolo, N), (int)mbOF, (int)s->lm_q.size());

    /* ---- viewer snapshot (under map_mutex) + signal viewer thread ---- */
    if (s->viewer.enabled) {
        {
            std::lock_guard<std::mutex> mlk(s->map_mutex);
            ngd_app_viewer_snapshot_build(&s->viewer, &s->view_snap, cur, imRGB, mask, W, H,
                                          s->tk.mState, s->tk.mnMatchesInliers, s->frame_num);
        }
        {
            std::lock_guard<std::mutex> vlk(s->view_mutex);
            s->view_pending = 1;
        }
        s->view_cv.notify_one();
    }

    return (s->tk.mState == NGD_STATE_OK || s->tk.mState == NGD_STATE_RECENTLY_LOST) ? 1 : 0;
}

/* ---------------- trajectory save (after shutdown drain) ---------------- */
static ngd_keyframe *find_kf0(ngd_app_system *s)
{
    for (int i = 0; i < s->map.nKFs; ++i)
        if (s->map.kfs[i]->mnId == s->map.mnInitKFid) return s->map.kfs[i];
    return NULL;
}

int ngd_app_system_save_trajectory_tum(ngd_app_system *s, const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) { fprintf(stderr, "system: cannot write %s\n", path); return 0; }
    ngd_keyframe *kf0 = find_kf0(s);
    ngd_se3 Two = kf0 ? ngd_se3_inverse(kf0->pose) : ngd_se3_identity();
    for (int i = 0; i < s->nRel; ++i) {
        ngd_keyframe *ref = s->relRefs[i];
        if (!ref) continue;
        ngd_se3 Trw = ngd_se3_multiply(ref->pose, Two);
        ngd_se3 Tcw = ngd_se3_multiply(s->relPoses[i], Trw);
        ngd_se3 Twc = ngd_se3_inverse(Tcw);
        fprintf(f, "%.6f %.9f %.9f %.9f %.9f %.9f %.9f %.9f\n",
                s->relTimes[i], Twc.t.x, Twc.t.y, Twc.t.z,
                Twc.q.x, Twc.q.y, Twc.q.z, Twc.q.w);
    }
    fclose(f);
    fprintf(stderr, "system: trajectory saved to %s (%d frames)\n", path, s->nRel);
    return 1;
}

static int cmp_kf_id(const void *a, const void *b)
{
    const ngd_keyframe *ka = *(const ngd_keyframe **)a;
    const ngd_keyframe *kb = *(const ngd_keyframe **)b;
    if (ka->mnId < kb->mnId) return -1;
    if (ka->mnId > kb->mnId) return 1;
    return 0;
}

int ngd_app_system_save_keyframe_trajectory_tum(ngd_app_system *s, const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) { fprintf(stderr, "system: cannot write %s\n", path); return 0; }
    ngd_keyframe **kfs = (ngd_keyframe **)malloc(sizeof(ngd_keyframe *) * (size_t)s->map.nKFs);
    for (int i = 0; i < s->map.nKFs; ++i) kfs[i] = s->map.kfs[i];
    qsort(kfs, (size_t)s->map.nKFs, sizeof(ngd_keyframe *), cmp_kf_id);
    for (int i = 0; i < s->map.nKFs; ++i) {
        ngd_keyframe *kf = kfs[i];
        if (ngd_keyframe_is_bad(kf)) continue;
        ngd_se3 Twc = ngd_se3_inverse(kf->pose);
        fprintf(f, "%llu %.9f %.9f %.9f %.9f %.9f %.9f %.9f\n",
                (unsigned long long)kf->mnId, Twc.t.x, Twc.t.y, Twc.t.z,
                Twc.q.x, Twc.q.y, Twc.q.z, Twc.q.w);
    }
    free(kfs);
    fclose(f);
    fprintf(stderr, "system: keyframe trajectory saved to %s (%d KFs)\n", path, s->map.nKFs);
    return 1;
}

void ngd_app_system_print_timing(const ngd_app_system *s, int nFrames)
{
    double tlm = s->t_lm.load(), tlc = s->t_lc.load(), tvw = s->t_view.load();
    int    nlm = s->n_lm.load(), nlc = s->n_lc.load(), nvw = s->n_view.load();
    int    maxq = s->max_lm_q_depth.load();
    double track_total = s->t_yolo + s->t_mask + s->t_of + s->t_orb + s->t_track;
    double all_total   = track_total + tlm + tlc + tvw;
    fprintf(stderr, "------- stage timing (frames=%d, KFs created=%d, LM runs=%d, LC runs=%d, "
                    "max lmQ=%d) -------\n",
            nFrames, s->n_kf, nlm, nlc, maxq);
    fprintf(stderr, "%-10s %12s %12s %10s %8s\n", "stage", "total(ms)", "per-frm(ms)", "calls", "share");
    auto row = [&](const char *name, double t, int n) {
        double pf = nFrames > 0 ? t / nFrames : 0.0;
        double sh = all_total > 0 ? 100.0 * t / all_total : 0.0;
        fprintf(stderr, "%-10s %12.1f %12.3f %10d %7.1f%%\n", name, t, pf, n, sh);
    };
    row("yolo",  s->t_yolo,  s->n_yolo);
    row("mask",  s->t_mask,  s->n_mask);
    row("of",    s->t_of,    s->n_of);
    row("orb",   s->t_orb,   s->n_orb);
    row("track", s->t_track, s->n_orb);
    row("lm(async)",  tlm, nlm);
    row("lc(async)",  tlc, nlc);
    row("view(async)",tvw, nvw);
    fprintf(stderr, "%-10s %12.1f %12.3f\n", "track-sum", track_total,
            nFrames > 0 ? track_total / nFrames : 0.0);
    fprintf(stderr, "%-10s %12.1f %12.3f\n", "all-sum",   all_total,
            nFrames > 0 ? all_total / nFrames : 0.0);
    fprintf(stderr, "(track-* = main-thread work charged to ttrack; lm/lc/view run async in "
                    "background threads and overlap with tracking. all-sum > nFrames*mean ttrack "
                    "indicates real overlap; track-sum is what the main thread actually spent.)\n");
}

void ngd_app_system_shutdown(ngd_app_system *s)
{
    /* Two-phase drain: stop LM first (it feeds LC). LM drains lm_q (pushing every
     * KF to lc_q) and exits; LC is still running and consumes lc_q concurrently.
     * Then stop LC (drains any remaining lc_q). Finally stop the Viewer. */
    if (s->threads_started) {
        s->stop_flag.store(true);
        s->draining.store(true);

        s->lm_q_cv.notify_all();
        if (s->th_lm.joinable()) s->th_lm.join();

        s->lc_q_cv.notify_all();
        if (s->th_lc.joinable()) s->th_lc.join();

        s->view_cv.notify_all();
        if (s->th_view.joinable()) s->th_view.join();

        s->threads_started = 0;
    }

    for (int i = 0; i < s->nOwned; ++i) { ngd_frame_free(s->owned[i]); free(s->owned[i]); }
    free(s->owned);
    for (int i = 0; i < s->map.nMPs; ++i) ngd_mappoint_free(s->map.mps[i]);
    for (int i = 0; i < s->map.nKFs; ++i) ngd_keyframe_free(s->map.kfs[i]);
    ngd_loopclosing_free(&s->lc);
    ngd_local_mapping_free(&s->lm);
    ngd_kfdb_free(&s->kfdb);
    ngd_map_free(&s->map);
    ngd_tracking_destroy(&s->tk);
    free(s->relPoses); free(s->relRefs); free(s->relTimes); free(s->relLost);
    free(s->gray_buf); free(s->last_gray); free(s->last_mask); free(s->mask_pred); free(s->mask_curr);
    if (s->yolo) { delete s->yolo; s->yolo = NULL; }
    if (s->vocab) ngd_bow_vocab_free(s->vocab);
    ngd_app_viewer_free(&s->viewer);
    /* Do NOT memset: the struct holds std::mutex/thread/atomic members. */
}
