// ngd_app/system.cpp — SLAM system driver (NGD OF mode, single-threaded).
//
// GrabImageRGBD-equivalent (Tracking.cc:1532-1675), faithful NGD optical-flow mode:
//   cvtColor -> gray ; Yolov8SegDetector -> mask (end-to-end, NO PredictCurrentMask LK)
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
#include "ngd_app/yolov8_seg.h"   // end-to-end YOLOv8-seg mask (replaces YoloDetector + PredictCurrentMask LK)

//#define USE_DEMO_ORB 1
//#include "ngd_app/demo_orb.h"     // USE_DEMO_ORB: alternative ORB from demo_feature_extraction.c
#ifdef USE_DSP_ORB
#include "ngd_app/dsp_orb.h"       // USE_DSP_ORB: VP6/DSP ORB kernels (bit-exact with refactor)
#endif

#include <opencv2/imgproc.hpp>
#include <opencv2/core.hpp>
#include <opencv2/calib3d.hpp>   /* cv::projectPoints, Rodrigues (pose covariance for NEES) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <vector>
#include <chrono>

/* ===== ngd_app runtime config (edit here + recompile; was env vars) ===== */
#ifndef NGD_VIEWER
#define NGD_VIEWER 1        /* 0 = headless benchmark (default); 1 = see trajectory/mask */
#endif
#ifndef NGD_PHASE
#define NGD_PHASE 3         /* 3 = OF mode (default); 2 = masked-ORB; 1 = static (needs fake yolo_dir) */
#endif
/* NEES covariance absolute-scale mode (compute_pose_cov):
 *   0 = model (1px Gaussian, Omega = 1/mvLevelSigma2[0])              -> A
 *   1 = empirical chi-square scaling, Sigma' = Sigma * (r^T Omega r)/(2n-6) -> B
 * A uses the 1px noise model; B calibrates the scale with actual reprojection
 * residuals of the PnP inliers. */
#ifndef NGD_NEES_EMPIRICAL
#define NGD_NEES_EMPIRICAL 0
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
        s->relCov      = (float *)realloc(s->relCov,      (size_t)nc * 36 * sizeof(float));
        s->relCovValid = (int   *)realloc(s->relCovValid, (size_t)nc * sizeof(int));
        s->capRel = nc;
    }
    s->relPoses[s->nRel] = Tcr;
    s->relRefs[s->nRel]  = ref;
    s->relTimes[s->nRel] = ts;
    s->relLost[s->nRel]  = lost;
    /* default invalid until compute_pose_cov fills it (caller, right after). */
    s->relCovValid[s->nRel] = 0;
    for (int k = 0; k < 36; ++k) s->relCov[(size_t)s->nRel * 36 + k] = 0.0f;
    s->nRel++;
}

static int nonzero_count(const uint8_t *m, int n) {
    int c = 0; for (int i = 0; i < n; ++i) if (m[i]) ++c; return c;
}

/* Compute 6x6 pose covariance (camera-frame, [rvec;tvec] perturbation of T_cw)
 * from the inlier 3D-2D correspondences in `cur` and its pose, as the PnP
 * information matrix inverse:  Sigma = (J^T Omega J)^-1, where J is the
 * cv::projectPoints reprojection jacobian (2N x 6, d(u,v)/d(rvec,tvec)) and
 * Omega = diag(1/mvLevelSigma2[octave]). This is an approximation (PnP residual
 * information, not a post-optimization marginal covariance) but covers every
 * frame (OF and KF branch) since `cur` always carries its PnP/Track inliers.
 *
 *   cov36[36]   out: row-major 6x6 Sigma (zeroed if invalid)
 *   *valid      out: 1 if >=6 inliers and Sigma finite & invertible, else 0
 * Pure post-hoc evaluation; does NOT mutate cur->pose or tracking state. */
static void compute_pose_cov(const ngd_frame *cur, const ngd_cam_ctx *cam,
                             float *cov36, int *valid)
{
    *valid = 0;
    for (int k = 0; k < 36; ++k) cov36[k] = 0.0f;
    if (!cur || cur->N < 6 || !cur->keys || !cur->mvpMapPoints || !cam) return;

    std::vector<cv::Point3f> obj;
    std::vector<cv::Point2f> img;
    std::vector<float> invs2;
    obj.reserve(cur->N); img.reserve(cur->N); invs2.reserve(cur->N);
    for (int i = 0; i < cur->N; ++i) {
        ngd_mappoint *mp = cur->mvpMapPoints[i];
        if (!mp || mp->mbBad) continue;
        if (cur->mvbOutlier && cur->mvbOutlier[i]) continue;
        obj.push_back(cv::Point3f(mp->worldPos[0], mp->worldPos[1], mp->worldPos[2]));
        img.push_back(cv::Point2f(cur->keys[i].x, cur->keys[i].y));
        int o = cur->keys[i].octave;
        if (o < 0 || o >= cam->nlevels) o = 0;
        float s2 = cam->mvLevelSigma2[o];
        invs2.push_back((s2 > 0.0f) ? (1.0f / s2) : 1.0f);
    }
    const int n = (int)obj.size();
    if (n < 6) return;

    /* cur->pose is T_cw (world->camera). Decompose to rvec/tvec for projectPoints. */
    float T16[16];
    ngd_se3_to_matrix4(cur->pose, T16);   /* row-major [R|t;0 1] */
    cv::Mat R(3, 3, CV_32F);
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            R.at<float>(r, c) = T16[r * 4 + c];
    cv::Mat rvec; cv::Rodrigues(R, rvec);
    cv::Mat tvec(3, 1, CV_32F);
    for (int r = 0; r < 3; ++r) tvec.at<float>(r) = T16[r * 4 + 3];

    cv::Mat K = (cv::Mat_<float>(3, 3) << cam->cam.fx, 0.0f, cam->cam.cx,
                                            0.0f, cam->cam.fy, cam->cam.cy,
                                            0.0f, 0.0f, 1.0f);
    cv::Mat imgProj, jac;
    cv::projectPoints(obj, rvec, tvec, K, cv::Mat(), imgProj, jac);
    /* jac: 2N x (3+3+...); first 6 cols = d(u,v)/d(rvec(3),tvec(3)). */
    if (jac.rows != 2 * n || jac.cols < 6) return;
    cv::Mat J = jac.colRange(0, 6);   /* 2N x 6, CV_32F (projectPoints outputs CV_64F? check) */

    /* H = J^T Omega J  (Omega = diag(invs2 per u,v)). Symmetric, build upper then mirror. */
    cv::Mat H = cv::Mat::zeros(6, 6, CV_64F);
    const bool jdbl = (J.type() == CV_64F);
    for (int i = 0; i < n; ++i) {
        const double w = (double)invs2[i];
        double Ju[6], Jv[6];
        if (jdbl) {
            for (int a = 0; a < 6; ++a) { Ju[a] = J.at<double>(2*i, a); Jv[a] = J.at<double>(2*i+1, a); }
        } else {
            for (int a = 0; a < 6; ++a) { Ju[a] = (double)J.at<float>(2*i, a); Jv[a] = (double)J.at<float>(2*i+1, a); }
        }
        for (int a = 0; a < 6; ++a) {
            for (int b = 0; b <= a; ++b) {
                H.at<double>(a, b) += w * (Ju[a] * Ju[b] + Jv[a] * Jv[b]);
            }
        }
    }
    for (int a = 0; a < 6; ++a)
        for (int b = 0; b < a; ++b)
            H.at<double>(b, a) = H.at<double>(a, b);

    cv::Mat S;
    if (!cv::invert(H, S, cv::DECOMP_SVD)) return;
    /* finite + sanity check (NaN/inf/absurd -> invalid). */
    for (int a = 0; a < 6; ++a)
        for (int b = 0; b < 6; ++b) {
            double v = S.at<double>(a, b);
            if (v != v || fabs(v) > 1e15) return;   /* NaN or non-finite */
        }
    for (int a = 0; a < 6; ++a)
        for (int b = 0; b < 6; ++b)
            cov36[a * 6 + b] = (float)S.at<double>(a, b);

#if NGD_NEES_EMPIRICAL
    /* Empirical chi-square scaling: Sigma' = Sigma * (r^T Omega r) / (2n - 6),
     * using actual reprojection residuals (obs - proj) of the PnP inliers
     * instead of the 1px model assumption. Calibrates the covariance's absolute
     * scale so a consistent estimator's mean NEES -> 3.
     * NOTE: RANSAC inliers are truncated at the 5px gate, so r^T Omega r is
     * biased low -> this is a lower-bound scale; see A/B comparison. */
    double rOmega_r = 0.0;
    for (int i = 0; i < n; ++i) {
        const float *pp = imgProj.ptr<float>(i);
        double du = (double)img[i].x - (double)pp[0];
        double dv = (double)img[i].y - (double)pp[1];
        rOmega_r += (double)invs2[i] * (du * du + dv * dv);
    }
    int dof = 2 * n - 6;
    double emp_scale = (dof > 0) ? (rOmega_r / (double)dof) : 1.0;
    if (emp_scale != emp_scale || emp_scale > 1e12) return;   /* NaN/garbage -> invalid */
    for (int a = 0; a < 6; ++a)
        for (int b = 0; b < 6; ++b)
            cov36[a * 6 + b] = (float)((double)cov36[a * 6 + b] * emp_scale);
#endif

    *valid = 1;
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
    s->mask_curr = (uint8_t *)calloc((size_t)npix, 1);

    s->yolo = NULL;
    if (yolo_dir && yolo_dir[0]) {
        std::string d(yolo_dir);
        std::string onnx = d + "/yolov8n-seg-320x448.onnx";
        FILE *fo = std::fopen(onnx.c_str(), "rb");
        if (fo) {
            std::fclose(fo);
            s->yolo = new Yolov8SegDetector();
            if (!s->yolo->init(onnx)) { delete s->yolo; s->yolo = NULL; fprintf(stderr, "system: Yolov8SegDetector init failed\n"); }
            else fprintf(stderr, "system: Yolov8SegDetector loaded (%s)\n", onnx.c_str());
        } else fprintf(stderr, "system: YOLOv8-seg model not found in %s (expected yolov8n-seg-320x448.onnx)\n", yolo_dir);
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

    /* ---- YOLOv8-seg person mask (end-to-end, every frame; NO LK propagation) ---- */
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
    uint8_t *mask = mask_yolo;   /* refappv8seg: mask comes straight from YOLOv8-seg */

    /* ---- mbStartOpticalFlow (cc:1566-1570): RGBDInitialized = mFrameNum>3 ----
     * NGD_PHASE define selects the benchmark phase (see top of system.cpp):
     *   3 (default) -> computed mbOF (phase 3 OF mode, faithful NGD)
     *   2           -> mbOF=0 (phase 2 masked-ORB variant; needs YOLO on)
     *   1           -> mbOF=0 (phase 1 static; YOLO off via a fake yolo_dir) */
    int force_no_of = (NGD_PHASE == 1 || NGD_PHASE == 2);
    int mbOF = !force_no_of && (s->tk.mState != NGD_STATE_NOT_INITIALIZED) && (s->frame_num >= 3);
    s->tk.mbStartOpticalFlow = mbOF;
    //fprintf(stderr, "--- frame %llu state=%d OF=%d ---\n",
    //        (unsigned long long)s->frame_num, s->tk.mState, (int)mbOF);

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
        //fprintf(stderr, "  DIAG OF: state=%d matches=%d needKF=%d\n",
        //        s->tk.mState, s->tk.mnMatchesInliers, (int)mbNeedKF);
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
        int Nfeat;
#ifdef USE_DSP_ORB
        {
            /* DSP ORB path: multi-scale VP6 kernels (resize/blur/FAST9/IC_Angle/rBRIEF)
             * + quadtree distribution. Mask-filter + frame finalize done here
             * (same pattern as USE_DEMO_ORB). */
            int Nraw = dsp_orb_extract(ex, gray, W, H, W, kps, MAXK, desc);
            if (mask && Nraw > 0) {
                int n = 0;
                for (int i = 0; i < Nraw; ++i) {
                    int u = (int)lroundf(kps[i].x), v = (int)lroundf(kps[i].y);
                    if (u < 0) u = 0; if (u >= W) u = W-1;
                    if (v < 0) v = 0; if (v >= H) v = H-1;
                    if (mask[v * W + u] != 0) continue;
                    if (n != i) { kps[n] = kps[i]; memcpy(desc + (size_t)n * 32, desc + (size_t)i * 32, 32); }
                    ++n;
                }
                Nraw = n;
            }
            memset(kf_frame, 0, sizeof(*kf_frame));
            kf_frame->mnId = fid;
            kf_frame->cam = s->cam;
            kf_frame->pose = ngd_se3_identity();
            if (Nraw > 0) {
                kf_frame->N = Nraw;
                kf_frame->keys = (ngd_keypoint *)malloc(sizeof(ngd_keypoint) * (size_t)Nraw);
                memcpy(kf_frame->keys, kps, sizeof(ngd_keypoint) * (size_t)Nraw);
                kf_frame->descriptors = (uint8_t *)malloc((size_t)Nraw * 32);
                memcpy(kf_frame->descriptors, desc, (size_t)Nraw * 32);
                kf_frame->uRight = (float *)malloc(sizeof(float) * (size_t)Nraw);
                kf_frame->depth  = (float *)malloc(sizeof(float) * (size_t)Nraw);
                kf_frame->mvpMapPoints = (struct ngd_mappoint **)calloc((size_t)Nraw, sizeof(void *));
                kf_frame->mvbOutlier   = (int *)calloc((size_t)Nraw, sizeof(int));
                float mbf = s->cam.mbf;
                for (int i = 0; i < Nraw; ++i) {
                    int u = (int)lroundf(kps[i].x), v = (int)lroundf(kps[i].y);
                    if (u < 0) u = 0; if (u >= W) u = W-1;
                    if (v < 0) v = 0; if (v >= H) v = H-1;
                    float d = depth[v * W + u];
                    if (d > 0) { kf_frame->depth[i] = d; kf_frame->uRight[i] = kps[i].x - mbf / d; }
                    else       { kf_frame->depth[i] = -1.0f; kf_frame->uRight[i] = -1.0f; }
                }
                kf_frame->gridInvW = (float)NGD_KF_GRID_COLS / (float)W;
                kf_frame->gridInvH = (float)NGD_KF_GRID_ROWS / (float)H;
                ngd_frame_assign_grid(kf_frame);
            }
            Nfeat = Nraw;
        }
#elif defined(USE_DEMO_ORB)
        {
            /* Demo ORB path: single-scale FAST9+NMS+TopN+ORB angle+rBRIEF from
             * demo_feature_extraction.c. Mask-filter + frame finalize done here
             * (replicates ngd_frame_build_rgbd_mask + finalize_rgbd). */
            int Nraw = demo_orb_extract(ex, gray, W, H, W, kps, MAXK, desc);
            if (mask && Nraw > 0) {
                int n = 0;
                for (int i = 0; i < Nraw; ++i) {
                    int u = (int)lroundf(kps[i].x), v = (int)lroundf(kps[i].y);
                    if (u < 0) u = 0; if (u >= W) u = W-1;
                    if (v < 0) v = 0; if (v >= H) v = H-1;
                    if (mask[v * W + u] != 0) continue;
                    if (n != i) { kps[n] = kps[i]; memcpy(desc + (size_t)n * 32, desc + (size_t)i * 32, 32); }
                    ++n;
                }
                Nraw = n;
            }
            memset(kf_frame, 0, sizeof(*kf_frame));
            kf_frame->mnId = fid;
            kf_frame->cam = s->cam;
            kf_frame->pose = ngd_se3_identity();
            if (Nraw > 0) {
                kf_frame->N = Nraw;
                kf_frame->keys = (ngd_keypoint *)malloc(sizeof(ngd_keypoint) * (size_t)Nraw);
                memcpy(kf_frame->keys, kps, sizeof(ngd_keypoint) * (size_t)Nraw);
                kf_frame->descriptors = (uint8_t *)malloc((size_t)Nraw * 32);
                memcpy(kf_frame->descriptors, desc, (size_t)Nraw * 32);
                kf_frame->uRight = (float *)malloc(sizeof(float) * (size_t)Nraw);
                kf_frame->depth  = (float *)malloc(sizeof(float) * (size_t)Nraw);
                kf_frame->mvpMapPoints = (struct ngd_mappoint **)calloc((size_t)Nraw, sizeof(void *));
                kf_frame->mvbOutlier   = (int *)calloc((size_t)Nraw, sizeof(int));
                float mbf = s->cam.mbf;
                for (int i = 0; i < Nraw; ++i) {
                    int u = (int)lroundf(kps[i].x), v = (int)lroundf(kps[i].y);
                    if (u < 0) u = 0; if (u >= W) u = W-1;
                    if (v < 0) v = 0; if (v >= H) v = H-1;
                    float d = depth[v * W + u];
                    if (d > 0) { kf_frame->depth[i] = d; kf_frame->uRight[i] = kps[i].x - mbf / d; }
                    else       { kf_frame->depth[i] = -1.0f; kf_frame->uRight[i] = -1.0f; }
                }
                kf_frame->gridInvW = (float)NGD_KF_GRID_COLS / (float)W;
                kf_frame->gridInvH = (float)NGD_KF_GRID_ROWS / (float)H;
                ngd_frame_assign_grid(kf_frame);
            }
            Nfeat = Nraw;
        }
#else
        Nfeat = ngd_frame_build_rgbd_mask(kf_frame, ex, &s->cam, gray, W, H, W, depth, W,
                                          fid, mask, kps, MAXK, desc);
#endif
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

        /* ---- DIAG: KF branch tracking result ---- */
        //fprintf(stderr, "  DIAG KF: state=%d matches=%d createdKF=%d Nfeat=%d\n",
        //        s->tk.mState, s->tk.mnMatchesInliers, (int)s->tk.mbCreatedKF, Nfeat);
    }

    /* ---- record trajectory (cc:1652-1658) ---- */
    if (s->tk.mState == NGD_STATE_OK || s->tk.mState == NGD_STATE_RECENTLY_LOST) {
        ngd_se3 Tcr = ngd_se3_multiply(cur->pose, ngd_se3_inverse(s->tk.refKF->pose));
        push_rel(s, Tcr, s->tk.refKF, timestamp, s->tk.mState == NGD_STATE_LOST);
        /* PnP-covariance approximation for NEES (post-hoc; does not affect pose/tracking). */
        compute_pose_cov(cur, &s->cam, s->relCov + (size_t)(s->nRel - 1) * 36,
                         &s->relCovValid[s->nRel - 1]);
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

int ngd_app_system_save_pose_covariance(ngd_app_system *s, const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) { fprintf(stderr, "system: cannot write %s\n", path); return 0; }
    fprintf(f, "# NEES pose covariance (PnP approximation)\n");
    fprintf(f, "# camera-frame [rvec(3);tvec(3)] perturbation of T_cw, Sigma=(J^T Omega J)^-1\n");
    fprintf(f, "# columns: ts valid c00 c01 ... c55 (row-major 6x6)\n");
    int nvalid = 0;
    for (int i = 0; i < s->nRel; ++i) {
        fprintf(f, "%.6f %d", s->relTimes[i], s->relCovValid[i]);
        const float *c = s->relCov + (size_t)i * 36;
        for (int k = 0; k < 36; ++k) fprintf(f, " %.9e", c[k]);
        fprintf(f, "\n");
        if (s->relCovValid[i]) ++nvalid;
    }
    fclose(f);
    fprintf(stderr, "system: pose covariance saved to %s (%d/%d frames valid)\n",
            path, nvalid, s->nRel);
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
    if (s->yolo) s->yolo->print_timing(nFrames);
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
    free(s->relCov); free(s->relCovValid);
    free(s->gray_buf); free(s->last_gray); free(s->last_mask); free(s->mask_curr);
    if (s->yolo) { delete s->yolo; s->yolo = NULL; }
    if (s->vocab) ngd_bow_vocab_free(s->vocab);
    ngd_app_viewer_free(&s->viewer);
    memset(s, 0, sizeof(*s));
}
