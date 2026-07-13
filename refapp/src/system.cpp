// ngd_app/system.cpp — SLAM system driver (NGD OF mode, single-threaded).
//
// GrabImageRGBD-equivalent (Tracking.cc:1532-1675), faithful NGD optical-flow mode:
//   cvtColor -> gray ; YoloDetector -> mask_yolo ; PredictCurrentMask -> mask
//   mbStartOpticalFlow = (mState!=NOT_INITIALIZED) && (frame_num>=3)   // cc:1567 RGBDInitialized=mFrameNum>3
//   if (mbStartOpticalFlow):
//       build no-ORB frame ; ngd_app_track_with_optical_flow(prev_lk, ...)  // OF sets pose + inlier keys/MPs
//       mbNeedKF = NeedNewKeyFrame()                                         // on the OF frame
//   else: mbNeedKF = true
//   if (mbNeedKF):
//       Tcw = cur.pose ; pick dual ORB ; rebuild cur = build_rgbd_mask(gray,depth,mask,ex)
//       if (mbStartOpticalFlow) cur.pose = Tcw                              // transfer OF pose (cc:1646)
//       tk.last = last_kf ; ngd_tracking_track(tk)                         // TWM keeps OF pose (guard), refines
//       last_kf = cur
//   record trajectory ; lm.run + loopclosing on KF created ; prev_lk = cur
#include "ngd_app/system.h"
#include "ngd_app/imio.h"
#include "ngd_app/optflow_track.h"
#include "ngd/optflow.h"
#include "ngd/mask.h"
#include "ngd_shell/YoloDetector.h"
#include "ngd_shell/lk_shell.h"   // mask-prediction LK: OpenCV ngd_shell_lk_default (pure-C optflow regressed tracking)

#include <opencv2/imgproc.hpp>
#include <opencv2/core.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <chrono>

/* ===== ngd_app runtime config (edit here + recompile; was env vars) ===== */
#ifndef NGD_VIEWER
#define NGD_VIEWER 1        /* 0 = off (headless/timing, default); 1 = on (see trajectory) */
#endif
#ifndef NGD_PHASE
#define NGD_PHASE 3         /* 3 = OF mode (default); 2 = masked-ORB; 1 = static (needs fake yolo_dir) */
#endif

/* ---------------- trajectory list ---------------- */
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

/* ---------------- owned-frame lifetime ---------------- */
static void own_frame(ngd_app_system *s, ngd_frame *f)
{
    if (s->nOwned == s->capOwned) {
        int nc = s->capOwned ? s->capOwned * 2 : 8;
        s->owned = (ngd_frame **)realloc(s->owned, (size_t)nc * sizeof(ngd_frame *));
        s->capOwned = nc;
    }
    s->owned[s->nOwned++] = f;
}
/* Free owned frames that are neither last_kf nor prev_lk (no longer referenced). */
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

int ngd_app_system_init(ngd_app_system *s, const char *vocab_path, const char *yaml_path,
                        const char *yolo_dir)
{
    memset(s, 0, sizeof(*s));

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
    s->frame_id = 0;
    s->frame_num = 0;
    s->vel_lk = ngd_se3_identity();   /* mVelocityLK starts as identity (no prior motion) */

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

    int ven = (NGD_VIEWER != 0);
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

int ngd_app_system_track_rgbd(ngd_app_system *s,
                                const cv::Mat &imRGB, const cv::Mat &imDepthMeters,
                                double timestamp)
{
    const int W = s->cam.imgW, H = s->cam.imgH, N = W * H;
    if (imRGB.cols != W || imRGB.rows != H || imDepthMeters.cols != W || imDepthMeters.rows != H) {
        fprintf(stderr, "system: image/depth size mismatch\n"); return 0;
    }
    const uint64_t fid = s->frame_id++;

    /* Reset per-frame: mbCreatedKF is only cleared at the start of Track()
     * (tracking.c:683), which doesn't run on OF-only frames. Without this reset,
     * a KF-branch frame that created a KF leaves mbCreatedKF=1 sticky -> every
     * subsequent OF-only frame spuriously runs LocalMapping+LoopClosing on the
     * same currentKF and inflates n_kf. Only surfaces once OF-only frames become
     * frequent (i.e. once c2 stops over-firing). */
    s->tk.mbCreatedKF = 0;

    cv::Mat grayMat;
    if (imRGB.channels() == 3) cv::cvtColor(imRGB, grayMat, cv::COLOR_BGR2GRAY);
    else                       grayMat = imRGB;
    if (grayMat.type() != CV_8U) grayMat.convertTo(grayMat, CV_8U);
    const uint8_t *gray = grayMat.ptr<uint8_t>();
    const float *depth = imDepthMeters.ptr<float>();

    /* ---- YOLO person mask ---- */
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

    /* ---- current mask: PredictCurrentMask (propagate last YOLO mask) or YOLO ---- */
    uint8_t *mask;
    if (s->frame_num > 0 && s->has_last) {
        memset(s->mask_pred, 0, (size_t)N);
        auto m_a = std::chrono::steady_clock::now();
        ngd_mask_predict_current(s->last_gray, gray, s->last_mask, depth,
                                 W, H, W, ngd_shell_lk_default, NULL, s->mask_pred);  // OpenCV LK: pure-C optflow regressed (89KF/54cm vs 61KF/10.7cm)
        auto m_b = std::chrono::steady_clock::now();
        s->t_mask += std::chrono::duration<double, std::milli>(m_b - m_a).count();
        s->n_mask++;
        mask = s->mask_pred;
    } else {
        mask = mask_yolo;
    }

    /* ---- mbStartOpticalFlow (cc:1566-1570): RGBDInitialized = mFrameNum>3 ----
     * NGD_PHASE env switch selects the benchmark phase without code edits:
     *   unset / "3" -> computed mbOF (phase 3 OF mode, faithful NGD)
     *   "2"         -> mbOF=0 (phase 2 masked-ORB variant; needs YOLO on)
     *   "1"         -> mbOF=0 (phase 1 static; YOLO off via a fake yolo_dir) */
    int force_no_of = (NGD_PHASE == 1 || NGD_PHASE == 2);
    int mbOF = !force_no_of && (s->tk.mState != NGD_STATE_NOT_INITIALIZED) && (s->frame_num >= 3);
    s->tk.mbStartOpticalFlow = mbOF;

    /* previous frame's pose (for the per-frame OF velocity mVelocityLK, cc:1661). */
    ngd_se3 prev_lk_pose = s->prev_lk ? s->prev_lk->pose : ngd_se3_identity();

    ngd_frame *cur = NULL;
    int mbNeedKF;

    if (mbOF) {
        /* ---- OF branch: no-ORB frame + TrackWithOpticalFlow (cc:1619-1625) ---- */
        cur = new_no_orb_frame(s, fid);
        own_frame(s, cur);
        s->tk.current = cur;
        auto of_a = std::chrono::steady_clock::now();
        ngd_app_track_with_optical_flow(s, s->prev_lk, s->last_gray, gray, mask);
        auto of_b = std::chrono::steady_clock::now();
        s->t_of += std::chrono::duration<double, std::milli>(of_b - of_a).count();
        s->n_of++;
        mbNeedKF = (s->tk.mnMatchesInliers > 0) ? ngd_tracking_need_new_keyframe(&s->tk) : 1;
    } else {
        mbNeedKF = 1;   /* first frames / not initialized (cc:1628) */
    }

    /* ---- KF branch (cc:1632-1649): rebuild with ORB+mask, transfer OF pose, Track ---- */
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

        if (mbOF) ngd_frame_set_pose(kf_frame, Tcw);   /* cc:1646 transfer OF pose */
        /* cur (no-ORB OF frame) is dropped (neither last_kf nor prev_lk after update). */
        cur = kf_frame;
        own_frame(s, cur);

        s->tk.current = cur;
        s->tk.last = s->last_kf;     /* mLastFrame = last KF-branch frame (NULL on first) */
        auto t_a = std::chrono::steady_clock::now();
        ngd_tracking_track(&s->tk);  /* TWM keeps OF pose via !mbStartOpticalFlow guard */
        auto t_b = std::chrono::steady_clock::now();
        s->t_track += std::chrono::duration<double, std::milli>(t_b - t_a).count();
        s->last_kf = cur;            /* mLastFrame for the next KF branch */
    }

    /* ---- record trajectory (cc:1652-1658) ---- */
    if (s->tk.mState == NGD_STATE_OK || s->tk.mState == NGD_STATE_RECENTLY_LOST) {
        ngd_se3 Tcr = ngd_se3_multiply(cur->pose, ngd_se3_inverse(s->tk.refKF->pose));
        push_rel(s, Tcr, s->tk.refKF, timestamp, s->tk.mState == NGD_STATE_LOST);
    }

    if (s->tk.mbCreatedKF) {
        s->n_kf++;
        auto lm_a = std::chrono::steady_clock::now();
        ngd_local_mapping_run(&s->lm);
        auto lm_b = std::chrono::steady_clock::now();
        s->t_lm += std::chrono::duration<double, std::milli>(lm_b - lm_a).count();
        s->n_lm++;

        auto lc_a = std::chrono::steady_clock::now();
        ngd_loopclosing_insert_kf(&s->lc, s->lm.currentKF);
        ngd_loopclosing_run(&s->lc);
        auto lc_b = std::chrono::steady_clock::now();
        s->t_lc += std::chrono::duration<double, std::milli>(lc_b - lc_a).count();
        s->n_lc++;
    }

    /* ---- update last frames + mask state (cc:1661-1668) ---- */
    s->vel_lk = ngd_se3_multiply(cur->pose, ngd_se3_inverse(prev_lk_pose));  /* mVelocityLK (cc:1661) */
    s->prev_lk = cur;                /* mLastFrameLK = last frame overall */
    sweep_owned(s);                  /* free frames no longer referenced */

    memcpy(s->last_gray, gray, (size_t)N);
    memcpy(s->last_mask, mask_yolo, (size_t)N);
    s->has_last = 1;
    s->frame_num++;

    if ((s->frame_num % 50) == 0)
        fprintf(stderr, "frame %llu  state=%d  KFs=%d MPs=%d  maskYolo=%d  OF=%d\n",
                (unsigned long long)s->frame_num, s->tk.mState, s->map.nKFs, s->map.nMPs,
                nonzero_count(mask_yolo, N), (int)mbOF);

    /* ---- live viewer (pure display; no SLAM-state mutation) ---- */
    auto v_a = std::chrono::steady_clock::now();
    ngd_app_viewer_draw(&s->viewer, &s->map, cur, imRGB, mask, W, H,
                        s->tk.mState, s->tk.mnMatchesInliers, s->frame_num);
    auto v_b = std::chrono::steady_clock::now();
    s->t_view += std::chrono::duration<double, std::milli>(v_b - v_a).count();

    return (s->tk.mState == NGD_STATE_OK || s->tk.mState == NGD_STATE_RECENTLY_LOST) ? 1 : 0;
}

/* find the init KeyFrame (mnId == map.mnInitKFid); never culled. */
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
    double total = s->t_yolo + s->t_mask + s->t_of + s->t_orb +
                   s->t_track + s->t_lm + s->t_lc + s->t_view;
    fprintf(stderr, "------- stage timing (frames=%d, KFs created=%d, sum=%.0f ms) -------\n",
            nFrames, s->n_kf, total);
    fprintf(stderr, "%-10s %12s %12s %10s %8s\n", "stage", "total(ms)", "per-frm(ms)", "calls", "share");
    auto row = [&](const char *name, double t, int n) {
        double pf = nFrames > 0 ? t / nFrames : 0.0;
        double sh = total > 0 ? 100.0 * t / total : 0.0;
        fprintf(stderr, "%-10s %12.1f %12.3f %10d %7.1f%%\n", name, t, pf, n, sh);
    };
    row("yolo",  s->t_yolo,  s->n_yolo);
    row("mask",  s->t_mask,  s->n_mask);
    row("of",    s->t_of,    s->n_of);
    row("orb",   s->t_orb,   s->n_orb);
    row("track", s->t_track, s->n_orb);   /* runs on KF-branch frames, same as orb */
    row("lm",    s->t_lm,    s->n_lm);
    row("lc",    s->t_lc,    s->n_lc);
    row("view",  s->t_view,  nFrames);
    fprintf(stderr, "%-10s %12.1f %12.3f\n", "sum", total, nFrames > 0 ? total / nFrames : 0.0);
    fprintf(stderr, "(per-frm = total/nFrames, amortized across all frames; "
                    "lm/lc run only on created-KF frames, track/orb on KF-branch frames)\n");
}

void ngd_app_system_shutdown(ngd_app_system *s)
{
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
    memset(s, 0, sizeof(*s));
}
