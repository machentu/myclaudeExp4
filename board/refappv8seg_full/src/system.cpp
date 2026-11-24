// ngd_app/system.cpp -- SLAM system driver (zero-OpenCV variant).
//
// GrabImageRGBD-equivalent. All image data passed as raw uint8_t*/float* buffers.
//   bgr_to_gray | Yolov8SegDetector.detect mask (ONNX Runtime)
//   Pure-C OF + EPnP (NGD_OF_LK_C=1, NGD_PNP=0) -- no ngd_shell, no OpenCV.
#include "ngd_app/system.h"
#include "ngd_app/imio.h"
#include "ngd_app/optflow_track.h"
#include "ngd/optflow.h"
#include "ngd_app/yolov8_seg.h"
#include "ngd/pinhole.h"    /* ngd_pinhole_project_jac */
#include "ngd/so3.h"        /* ngd_so3_exp, ngd_so3_log */
#include "ngd/math.h"       /* ngd_solve_linear */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>       /* access, sleep — NGD_WAIT_KO 同步点 */
#include <chrono>
#include <string>

/* ---- pure-C utility externs ---- */
extern "C" {
void ngd_bgr_to_gray(const uint8_t *bgr, int w, int h, int stride, uint8_t *gray);
}

/* [①] lm 工作线程 (宏 NGD_LM_THREAD; 宏关时空头, 钩子亦不编译) */
#include "ngd_opt_lmthread.h"

/* ===== ngd_app runtime config ===== */
#ifndef NGD_VIEWER
#define NGD_VIEWER 0        /* 0 = headless (default); 1 = write viewer frames to disk */
#endif
#ifndef NGD_PHASE
#define NGD_PHASE 3         /* 3 = OF mode (default) */
#endif
#ifndef NGD_NEES_EMPIRICAL
#define NGD_NEES_EMPIRICAL 0
#endif
#ifndef NGD_ORB_DSP
#define NGD_ORB_DSP 0        /* 1 = VDSP firmware ORB extraction (xc01 board build) */
#endif

#if NGD_ORB_DSP
#include "ngd_app/orb_dsp.h"
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
    s->relCovValid[s->nRel] = 0;
    for (int k = 0; k < 36; ++k) s->relCov[(size_t)s->nRel * 36 + k] = 0.0f;
    s->nRel++;
}

static int nonzero_count(const uint8_t *m, int n) {
    int c = 0; for (int i = 0; i < n; ++i) if (m[i]) ++c; return c;
}

/* Compute 6x6 pose covariance (camera-frame, [rvec;tvec] perturbation of T_cw)
 * from inlier 3D-2D correspondences. Pure-C replacement for the OpenCV version:
 *   cv::Rodrigues → ngd_so3_log
 *   cv::projectPoints + Jacobian → ngd_pinhole_project_jac + ngd_se3_deriv
 *   cv::invert(DECOMP_SVD) → ngd_solve_linear (Gauss elimination on 6x6 SPD matrix) */
static void compute_pose_cov(const ngd_frame *cur, const ngd_cam_ctx *cam,
                             float *cov36, int *valid)
{
    *valid = 0;
    for (int k = 0; k < 36; ++k) cov36[k] = 0.0f;
    if (!cur || cur->N < 6 || !cur->keys || !cur->mvpMapPoints || !cam) return;

    /* collect valid inlier correspondences */
    int n = 0;
    for (int i = 0; i < cur->N; ++i) {
        ngd_mappoint *mp = cur->mvpMapPoints[i];
        if (!mp || mp->mbBad) continue;
        if (cur->mvbOutlier && cur->mvbOutlier[i]) continue;
        ++n;
    }
    if (n < 6) return;

    /* allocate working arrays */
    float *world3d = (float*)malloc((size_t)n * 3 * sizeof(float));
    float *img2d   = (float*)malloc((size_t)n * 2 * sizeof(float));
    float *invs2   = (float*)malloc((size_t)n * sizeof(float));
    int idx = 0;
    for (int i = 0; i < cur->N; ++i) {
        ngd_mappoint *mp = cur->mvpMapPoints[i];
        if (!mp || mp->mbBad) continue;
        if (cur->mvbOutlier && cur->mvbOutlier[i]) continue;
        world3d[3*idx]   = mp->worldPos[0];
        world3d[3*idx+1] = mp->worldPos[1];
        world3d[3*idx+2] = mp->worldPos[2];
        img2d[2*idx]     = cur->keys[i].x;
        img2d[2*idx+1]   = cur->keys[i].y;
        int o = cur->keys[i].octave;
        if (o < 0 || o >= cam->nlevels) o = 0;
        invs2[idx] = (cam->mvLevelSigma2[o] > 0.0f) ? (1.0f / cam->mvLevelSigma2[o]) : 1.0f;
        ++idx;
    }
    n = idx;
    if (n < 6) { free(world3d); free(img2d); free(invs2); return; }

    /* decompose cur->pose (T_cw) into rvec/tvec */
    /* T_cw = [R | t]; rvec = axis-angle from R via ngd_so3_log */
    // ngd_mat3 R = ngd_quat_to_matrix(cur->pose.q);
    // ngd_vec3 rvec = ngd_so3_log(R);  /* axis-angle */
    // ngd_vec3 tvec = cur->pose.t;

    /* Jacobian chain for each point:
     *   Xc = R * Xw + t   (camera-frame 3D)
     *   d(u,v)/d(rvec,tvec) = d(u,v)/d(Xc) * d(Xc)/d(rvec,tvec)
     *   d(u,v)/d(Xc) = ngd_pinhole_project_jac (2x3)
     *   d(Xc)/d(rvec,tvec) = [-skew(Xc) | I_3] = ngd_se3_deriv (3x6) */
    double H[36] = {0};  /* 6x6, row-major */

    for (int i = 0; i < n; ++i) {
        ngd_vec3 Xw = ngd_v3(world3d[3*i], world3d[3*i+1], world3d[3*i+2]);
        ngd_vec3 Xc = ngd_se3_map(cur->pose, Xw);  /* R*Xw + t */

        float Jcam[6];   /* 2x3: d(uv)/d(Xc) */
        ngd_pinhole_project_jac(cam->cam, Xc, Jcam);
        /* Jcam layout: row-major 2x3: [du/dX du/dY du/dZ; dv/dX dv/dY dv/dZ] */
        float Ju_cam[3] = {Jcam[0], Jcam[1], Jcam[2]};
        float Jv_cam[3] = {Jcam[3], Jcam[4], Jcam[5]};

        float Jse3[18];  /* 3x6: d(Xc)/d(rvec,tvec) = [-skew(Xc) | I] */
        ngd_se3_deriv(Xc, Jse3);

        /* Full 2x6 Jacobian: Jfull = Jcam(2x3) * Jse3(3x6) */
        float Ju[6], Jv[6];
        for (int a = 0; a < 6; ++a) {
            Ju[a] = Ju_cam[0]*Jse3[0*6+a] + Ju_cam[1]*Jse3[1*6+a] + Ju_cam[2]*Jse3[2*6+a];
            Jv[a] = Jv_cam[0]*Jse3[0*6+a] + Jv_cam[1]*Jse3[1*6+a] + Jv_cam[2]*Jse3[2*6+a];
        }

        double w = (double)invs2[i];
        for (int a = 0; a < 6; ++a)
            for (int b = 0; b <= a; ++b)
                H[a*6+b] += w * ( (double)Ju[a]*(double)Ju[b] + (double)Jv[a]*(double)Jv[b] );
    }
    /* mirror upper to lower */
    for (int a = 0; a < 6; ++a)
        for (int b = 0; b < a; ++b)
            H[b*6+a] = H[a*6+b];

    /* Sigma = H^-1 via Gauss elimination (solve H * Sigma_col_j = e_j, j=0..5).
     * ngd_solve_linear modifies A in-place, so copy H before each solve. */
    float Sf[36];
    for (int j = 0; j < 6; ++j) {
        float Hf[36];
        for (int k = 0; k < 36; ++k) Hf[k] = (float)H[k];
        float ej[6] = {0}; ej[j] = 1.0f;
        float col[6];
        if (!ngd_solve_linear(Hf, ej, col, 6)) {
            free(world3d); free(img2d); free(invs2); return;
        }
        for (int i = 0; i < 6; ++i) Sf[i*6+j] = col[i];
    }

    /* sanity check */
    for (int a = 0; a < 6; ++a)
        for (int b = 0; b < 6; ++b) {
            float v = Sf[a*6+b];
            if (v != v || fabsf(v) > 1e15f) {
                free(world3d); free(img2d); free(invs2); return;
            }
        }
    for (int k = 0; k < 36; ++k) cov36[k] = Sf[k];

#if NGD_NEES_EMPIRICAL
    /* Empirical chi-square scaling (compute reprojection residuals with pure-C projection) */
    double rOmega_r = 0.0;
    for (int i = 0; i < n; ++i) {
        ngd_vec3 Xw = ngd_v3(world3d[3*i], world3d[3*i+1], world3d[3*i+2]);
        ngd_vec3 Xc = ngd_se3_map(cur->pose, Xw);
        float uv[2];
        ngd_pinhole_project(cam->cam, Xc, uv);
        double du = (double)img2d[2*i] - (double)uv[0];
        double dv = (double)img2d[2*i+1] - (double)uv[1];
        rOmega_r += (double)invs2[i] * (du*du + dv*dv);
    }
    int dof = 2*n - 6;
    double emp_scale = (dof > 0) ? (rOmega_r / (double)dof) : 1.0;
    if (emp_scale != emp_scale || emp_scale > 1e12) {
        free(world3d); free(img2d); free(invs2); return;
    }
    for (int k = 0; k < 36; ++k) cov36[k] = (float)((double)cov36[k] * emp_scale);
#endif

    free(world3d); free(img2d); free(invs2);
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
static void sweep_owned(ngd_app_system *s)
{
    int w = 0;
    for (int i = 0; i < s->nOwned; ++i) {
        ngd_frame *f = s->owned[i];
        if (f == s->last_kf || f == s->prev_lk)
            s->owned[w++] = f;
        else { ngd_frame_free(f); free(f); }
    }
    s->nOwned = w;
}

int ngd_app_system_init(ngd_app_system *s, const char *vocab_path, const char *yaml_path,
                        const char *yolo_dir)
{
    printf("ngd_app_system_init\n");

    memset(s, 0, sizeof(*s));

#if NGD_BOW
    /* [gap-probe] 词袋装载计时 (gap 分解诊断, 2026-11-24) */
    const auto tv0 = std::chrono::steady_clock::now();
    s->vocab = ngd_bow_vocab_load_binary(vocab_path);
    if (!s->vocab) s->vocab = ngd_bow_vocab_load_text(vocab_path);
    if (!s->vocab) { fprintf(stderr, "system: failed to load vocabulary %s\n", vocab_path); return 0; }
    fprintf(stderr, "system: vocabulary loaded (%u nodes, %u words) in %lldms\n",
            s->vocab->n_nodes, s->vocab->n_words,
            (long long)std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - tv0).count());
    /* 板端 (256MB): tmpfs 副本在 SoA 载入后即无用, 立即释放省 ~44MB.
     * 保守限定 /tmp 前缀, PC/NFS 路径不受影响. */
    if (!strncmp(vocab_path, "/tmp/", 5)) remove(vocab_path);
    /* E20 延迟装 ko 协议: 词袋阶段(大堆+NFS 读)与模块驻留共存 = 词袋期内核死
     * (E13-E19 0/7; E18 无模块时词袋全程安全)。模块只需在 DSP/NPU init(词袋
     * 之后)就位: 词袋加载完原地等 /tmp/ko_ready, 由外部脚本此刻才开始 insmod。 */
    if (getenv("NGD_WAIT_KO")) {
        int i;
        fprintf(stderr, "system: waiting for /tmp/ko_ready (NGD_WAIT_KO)...\n");
        for (i = 0; i < 300 && access("/tmp/ko_ready", F_OK) != 0; ++i) sleep(1);
        fprintf(stderr, "system: %s after %ds\n",
                access("/tmp/ko_ready", F_OK) == 0 ? "ko_ready seen" : "ko_ready TIMEOUT",
                i);
    }
#else
    (void)vocab_path;
    fprintf(stderr, "system: NGD_BOW=0, vocabulary loading skipped\n");
#endif

    if (!ngd_app_load_yaml(yaml_path, &s->cfg)) { fprintf(stderr, "system: failed to load settings %s\n", yaml_path); return 0; }
    ngd_app_build(&s->cfg, &s->ex, &s->cam);
    ngd_orb_init(&s->ex_dyna, (int)(s->cfg.nFeatures * 1.5f + 0.5f),
                 s->cfg.scaleFactor, s->cfg.nLevels, s->cfg.iniThFAST, s->cfg.minThFAST);

#if NGD_ORB_DSP
    /* VDSP ORB backend (xc01): firmware params are fixed == TUM3 defaults, so
     * the ex/ex_dyna switch above makes no difference under this backend. */
    if (orb_dsp_init(s->cam.imgW, s->cam.imgH) != 0) {
        fprintf(stderr, "system: orb_dsp_init failed (check NGD_ORB_DSP_FW)\n");
        return 0;
    }
    ngd_orb_set_backend(orb_dsp_extract);
    fprintf(stderr, "system: ORB extraction on VDSP (NGD_ORB_DSP=1)\n");
#endif

    printf("ngd_app_system_init 1\n");

    ngd_map_init(&s->map);
#if NGD_BOW
    ngd_kfdb_init(&s->kfdb, s->vocab);
#endif
    ngd_local_mapping_init(&s->lm, &s->map, 0);
#if NGD_BOW
    ngd_local_mapping_set_kfdb(&s->lm, &s->kfdb);
    ngd_loopclosing_init(&s->lc, &s->map, &s->kfdb, s->vocab, 1);
#endif
#if defined(NGD_LM_THREAD) && NGD_LM_THREAD
    ngd_lmthread_init(&s->lm);   /* [①] lm 工作线程 (失败自动回退内联) */
#endif
    ngd_tracking_init(&s->tk);
#if NGD_BOW
    s->tk.vocab = s->vocab; s->tk.kfdb = &s->kfdb;
#endif
    s->tk.map = &s->map; s->tk.lm = &s->lm;
    s->tk.sensor = NGD_SENSOR_RGBD;
    s->tk.mState = NGD_STATE_NOT_INITIALIZED;
    s->tk.mMinFrames = 0;
    s->tk.mMaxFrames = (int)(s->cfg.fps + 0.5f);
    s->frame_id = 0;
    s->frame_num = 0;
    s->vel_lk = ngd_se3_identity();

    printf("ngd_app_system_init 3\n");

    const int npix = s->cam.imgW * s->cam.imgH;
    s->gray_buf  = (uint8_t *)calloc((size_t)npix, 1);
    s->last_gray = (uint8_t *)calloc((size_t)npix, 1);
    s->last_mask = (uint8_t *)calloc((size_t)npix, 1);
    s->mask_curr = (uint8_t *)calloc((size_t)npix, 1);

    printf("ngd_app_system_init 4\n");

    s->yolo = NULL;
    if (yolo_dir && yolo_dir[0]) {
        std::string d(yolo_dir);
        /* 探测文件名随网络分辨率走 (默认 320x448; npu3 构建传 256x480) */
#ifndef NGD_YOLO_TAG
#define NGD_YOLO_TAG "320x448"
#endif
        std::string onnx = d + "/yolov8n-seg-" NGD_YOLO_TAG ".onnx";
        FILE *fo = std::fopen(onnx.c_str(), "rb");
        if (fo) {
            std::fclose(fo);
            s->yolo = new Yolov8SegDetector();
            if (!s->yolo->init(onnx)) { delete s->yolo; s->yolo = NULL; fprintf(stderr, "system: Yolov8SegDetector init failed\n"); }
            else {
                fprintf(stderr, "system: Yolov8SegDetector loaded (%s)\n", onnx.c_str());
                /* 板端: MNN 读入建 session 后模型文件即无用; /tmp 副本立即释放 (~14MB). */
                if (onnx.rfind("/tmp/", 0) == 0) {
                    remove(onnx.c_str());
                    std::string mnn = onnx.substr(0, onnx.size() - 5) + ".mnn";
                    remove(mnn.c_str());
                }
            }
        } else fprintf(stderr, "system: YOLOv8-seg model not found in %s\n", yolo_dir);
    }

    printf("ngd_app_system_init 5\n");

    /* viewer: PNG frames to viewer_frames/ when NGD_VIEWER=1 */
    int ven = (NGD_VIEWER != 0);
    ngd_app_viewer_init(&s->viewer, ven,
                        s->cfg.viewpointX, s->cfg.viewpointY, s->cfg.viewpointZ, s->cfg.viewpointF);
    fprintf(stderr, "system: viewer %s (set NGD_VIEWER=1 to enable PNG output)\n", ven?"on":"off");

    printf("ngd_app_system_init 6\n");

    return 1;
}

static ngd_frame *new_no_orb_frame(ngd_app_system *s, uint64_t fid)
{
    ngd_frame *f = (ngd_frame *)calloc(1, sizeof(ngd_frame));
    f->cam = s->cam;
    f->mnId = fid;
    f->pose = ngd_se3_identity();
    return f;
}

#if defined(NGD_LM_THREAD) && NGD_LM_THREAD
/* [①] 排空 lm 完成事件: lc insert+run (WR) + retired 清扫 (帧注册表在
 * driver 层) + t_lm 计时。每帧 (KF 块 post 后 / 非 KF 帧) 与停机前调用。
 * 与原内联 KF 块语义一致, 仅 lc/清扫延迟到该事件完成时 (事件序 = 提交序)。 */
static void lm_pump(ngd_app_system *s)
{
    ngd_lm_done_ev ev;
    while (ngd_lmthread_pop_done(&ev)) {
#if NGD_BOW
        auto lc_a = std::chrono::steady_clock::now();
        ngd_lmthread_map_wrlock();
        ngd_loopclosing_insert_kf(&s->lc, ev.kf);
        ngd_loopclosing_run(&s->lc);
        ngd_lmthread_map_wrunlock();
        auto lc_b = std::chrono::steady_clock::now();
        s->t_lc += std::chrono::duration<double, std::milli>(lc_b - lc_a).count();
        s->n_lc++;
#endif
        /* bad-mp 回收 (SetBadFlag delete 语义的分层落地): KF 槽位已在
         * set_bad/replace 清; 这里清 owned 帧槽位后统一 free — 帧注册表
         * (s->owned) 只在 driver 层, mappoint/localmapping 层拿不到. */
        for (int r = 0; r < ev.nRetired; ++r) {
            ngd_mappoint *rmp = ev.retired[r];
            for (int fi = 0; fi < s->nOwned; ++fi) {
                ngd_frame *fr = s->owned[fi];
                if (!fr || !fr->mvpMapPoints) continue;
                for (int k = 0; k < fr->N; ++k)
                    if (fr->mvpMapPoints[k] == rmp) fr->mvpMapPoints[k] = NULL;
            }
            ngd_mappoint_free(rmp);
        }
        free(ev.retired);
        s->t_lm += ev.t_ms;
        s->n_lm++;
    }
}
#endif

int ngd_app_system_track_rgbd(ngd_app_system *s,
                                const uint8_t *imBGR, const float *imDepthMeters,
                                int w, int h, double timestamp)
{
    printf("ngd_app_system_track_rgbd\n");

    const int W = s->cam.imgW, H = s->cam.imgH, N = W * H;
    if (w != W || h != H) {
        fprintf(stderr, "system: image size mismatch (expected %dx%d, got %dx%d)\n", W, H, w, h);
        return 0;
    }
    const uint64_t fid = s->frame_id++;

    s->tk.mbCreatedKF = 0;

    printf("ngd_app_system_track_rgbd 1\n");

    /* BGR → gray (pure-C) */
    ngd_bgr_to_gray(imBGR, W, H, 3*W, s->gray_buf);
    const uint8_t *gray  = s->gray_buf;
    const float   *depth = imDepthMeters;

    printf("ngd_app_system_track_rgbd 2\n");

    /* ---- YOLOv8-seg person mask (ONNX Runtime, end-to-end) ---- */
    uint8_t *mask_yolo = s->mask_curr;
    memset(mask_yolo, 0, (size_t)N);

    printf("ngd_app_system_track_rgbd 3\n");
    if (s->yolo) {
        printf("ngd_app_system_track_rgbd 4\n");

        auto y_a = std::chrono::steady_clock::now();
        s->yolo->detect(imBGR, imDepthMeters, W, H, mask_yolo);
        auto y_b = std::chrono::steady_clock::now();
        s->t_yolo += std::chrono::duration<double, std::milli>(y_b - y_a).count();
        s->n_yolo++;
    }
    printf("ngd_app_system_track_rgbd 5\n");

    uint8_t *mask = mask_yolo;

    /* ---- mbStartOpticalFlow ---- */
    int force_no_of = (NGD_PHASE == 1 || NGD_PHASE == 2);
    int mbOF = !force_no_of && (s->tk.mState != NGD_STATE_NOT_INITIALIZED) && (s->frame_num >= 3);
    s->tk.mbStartOpticalFlow = mbOF;

    ngd_se3 prev_lk_pose = s->prev_lk ? s->prev_lk->pose : ngd_se3_identity();
    ngd_frame *cur = NULL;
    int mbNeedKF;

    printf("ngd_app_system_track_rgbd 6\n");

    if (mbOF) {
        cur = new_no_orb_frame(s, fid);
        own_frame(s, cur);
        s->tk.current = cur;
#if defined(NGD_LM_THREAD) && NGD_LM_THREAD
        ngd_lmthread_map_rdlock();   /* [①] OF 读 prev_lk 的 MP/世界坐标 */
#endif
        auto of_a = std::chrono::steady_clock::now();
        ngd_app_track_with_optical_flow(s, s->prev_lk, s->last_gray, gray, mask);
        auto of_b = std::chrono::steady_clock::now();
        s->t_of += std::chrono::duration<double, std::milli>(of_b - of_a).count();
        s->n_of++;
        mbNeedKF = (s->tk.mnMatchesInliers > 0) ? ngd_tracking_need_new_keyframe(&s->tk) : 1;
#if defined(NGD_LM_THREAD) && NGD_LM_THREAD
        ngd_lmthread_map_rdunlock();
#endif
    } else {
        mbNeedKF = 1;
    }

    printf("ngd_app_system_track_rgbd 7\n");

    if (mbNeedKF) {
        printf("ngd_app_system_track_rgbd 7a\n");

        ngd_se3 Tcw = cur ? cur->pose : ngd_se3_identity();

        printf("ngd_app_system_track_rgbd 7b\n");

        ngd_orb_extractor *ex = &s->ex;
        if (nonzero_count(mask, N) > 0 || nonzero_count(s->last_mask, N) > 0) ex = &s->ex_dyna;

        printf("ngd_app_system_track_rgbd 7c\n");

        const int MAXK = (int)(s->cfg.nFeatures * 1.5f + 0.5f) * 2;
        ngd_keypoint *kps = (ngd_keypoint *)malloc(sizeof(ngd_keypoint) * MAXK);
        if(!kps){printf("no kps mem\n");}
        uint8_t *desc = (uint8_t *)malloc((size_t)MAXK * 32);
        if(!desc){printf("no desc mem\n");}
        ngd_frame *kf_frame = (ngd_frame *)calloc(1, sizeof(ngd_frame));
        if(!kf_frame){printf("no kf_frame mem\n");}

        printf("ngd_app_system_track_rgbd 7d\n");
        if (!kps || !desc || !kf_frame) {
			printf("some mbNeedKF mem failed\n");
            free(kps); free(desc); free(kf_frame);
            return 0;
        }
		
        auto o_a = std::chrono::steady_clock::now();
        int Nfeat = ngd_frame_build_rgbd_mask(kf_frame, ex, &s->cam, gray, W, H, W, depth, W,
                                              fid, mask, kps, MAXK, desc);
        auto o_b = std::chrono::steady_clock::now();

        printf("ngd_app_system_track_rgbd 7e\n");

        s->t_orb += std::chrono::duration<double, std::milli>(o_b - o_a).count();
        s->n_orb++;
        free(kps); free(desc);
        if (Nfeat <= 0) { ngd_frame_free(kf_frame); free(kf_frame); return 0; }

        printf("ngd_app_system_track_rgbd 7f\n");

        if (mbOF) ngd_frame_set_pose(kf_frame, Tcw);

        printf("ngd_app_system_track_rgbd 7g\n");

        cur = kf_frame;
        own_frame(s, cur);

        printf("ngd_app_system_track_rgbd 7h\n");

        s->tk.current = cur;
        printf("ngd_app_system_track_rgbd 7i\n");

        s->tk.last = s->last_kf;
        printf("ngd_app_system_track_rgbd 7ia\n");
        auto t_a = std::chrono::steady_clock::now();
        printf("ngd_app_system_track_rgbd 7ib\n");
        printf("ngd_app_system_track_rgbd 7ic %p\n", s->tk);
#if defined(NGD_LM_THREAD) && NGD_LM_THREAD
        ngd_lmthread_map_rdlock();   /* [①] track 读地图 (create_kf 内部升级 WR) */
#endif
        ngd_tracking_track(&s->tk);
#if defined(NGD_LM_THREAD) && NGD_LM_THREAD
        ngd_lmthread_map_rdunlock();
#endif
        auto t_b = std::chrono::steady_clock::now();
        s->t_track += std::chrono::duration<double, std::milli>(t_b - t_a).count();
        s->last_kf = cur;

        printf("ngd_app_system_track_rgbd 7j\n");

    }

    printf("ngd_app_system_track_rgbd 8\n");

    if (s->tk.mState == NGD_STATE_OK || s->tk.mState == NGD_STATE_RECENTLY_LOST) {
#if defined(NGD_LM_THREAD) && NGD_LM_THREAD
        ngd_lmthread_map_rdlock();   /* [①] refKF->pose 是 BA writeback 目标 */
#endif
        ngd_se3 Tcr = ngd_se3_multiply(cur->pose, ngd_se3_inverse(s->tk.refKF->pose));
        push_rel(s, Tcr, s->tk.refKF, timestamp, s->tk.mState == NGD_STATE_LOST);
        compute_pose_cov(cur, &s->cam, s->relCov + (size_t)(s->nRel - 1) * 36,
                         &s->relCovValid[s->nRel - 1]);
#if defined(NGD_LM_THREAD) && NGD_LM_THREAD
        ngd_lmthread_map_rdunlock();
#endif
    }

    printf("ngd_app_system_track_rgbd 9\n");

    if (s->tk.mbCreatedKF) {
        s->n_kf++;
#if defined(NGD_LM_THREAD) && NGD_LM_THREAD
        if (ngd_lmthread_active()) {
            /* [①] 提交工作线程 (深度 2 背压): 满则先 pump 排空完成环再重试 —
             * worker 取件等完成环为空, 阻塞式 post 会与之成环死锁 (v42 实测) */
            int spins = 0;
            while (!ngd_lmthread_post(&s->lm)) {
                lm_pump(s);
                usleep(5000);
                ++spins;
            }
            if (spins) fprintf(stderr, "[lmthr] backpressure %d spins (~%dms) at kf\n",
                               spins, spins * 5);
            lm_pump(s);
        } else
#endif
        {
        auto lm_a = std::chrono::steady_clock::now();
        ngd_local_mapping_run(&s->lm);
        auto lm_b = std::chrono::steady_clock::now();
        s->t_lm += std::chrono::duration<double, std::milli>(lm_b - lm_a).count();
        s->n_lm++;

#if NGD_BOW
        auto lc_a = std::chrono::steady_clock::now();
        ngd_loopclosing_insert_kf(&s->lc, s->lm.currentKF);
        ngd_loopclosing_run(&s->lc);
        auto lc_b = std::chrono::steady_clock::now();
        s->t_lc += std::chrono::duration<double, std::milli>(lc_b - lc_a).count();
        s->n_lc++;
#endif

        /* bad-mp 回收 (SetBadFlag delete 语义的分层落地): KF 槽位已在
         * set_bad/replace 清; 这里清 owned 帧槽位后统一 free — 帧注册表
         * (s->owned) 只在 driver 层, mappoint/localmapping 层拿不到. */
        for (int r = 0; r < s->lm.nRetired; ++r) {
            ngd_mappoint *rmp = s->lm.retired_mps[r];
            for (int fi = 0; fi < s->nOwned; ++fi) {
                ngd_frame *fr = s->owned[fi];
                if (!fr || !fr->mvpMapPoints) continue;
                for (int k = 0; k < fr->N; ++k)
                    if (fr->mvpMapPoints[k] == rmp) fr->mvpMapPoints[k] = NULL;
            }
            ngd_mappoint_free(rmp);
        }
        s->lm.nRetired = 0;
        }
    }
#if defined(NGD_LM_THREAD) && NGD_LM_THREAD
    else lm_pump(s);   /* [①] 非 KF 帧也排空, 保 lc/清扫流动 */
#endif

    printf("ngd_app_system_track_rgbd 10\n");

    s->vel_lk = ngd_se3_multiply(cur->pose, ngd_se3_inverse(prev_lk_pose));
    s->prev_lk = cur;
    sweep_owned(s);

    printf("ngd_app_system_track_rgbd 11\n");

    memcpy(s->last_gray, gray, (size_t)N);
    memcpy(s->last_mask, mask_yolo, (size_t)N);
    s->has_last = 1;
    s->frame_num++;

    if ((s->frame_num % 50) == 0)
        fprintf(stderr, "frame %llu  state=%d  KFs=%d MPs=%d  maskYolo=%d  OF=%d\n",
                (unsigned long long)s->frame_num, s->tk.mState, s->map.nKFs, s->map.nMPs,
                nonzero_count(mask_yolo, N), (int)mbOF);

    /* viewer: PNG frames to viewer_frames/ when NGD_VIEWER=1 */
    auto v_a = std::chrono::steady_clock::now();
    ngd_app_viewer_draw(&s->viewer, &s->map, cur, imBGR, W, H, 3,
                        mask, W, H, s->tk.mState, s->tk.mnMatchesInliers, s->frame_num);
    auto v_b = std::chrono::steady_clock::now();
    s->t_view += std::chrono::duration<double, std::milli>(v_b - v_a).count();

    return (s->tk.mState == NGD_STATE_OK || s->tk.mState == NGD_STATE_RECENTLY_LOST) ? 1 : 0;
}

/* ---------------- save functions (unchanged) ---------------- */

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
    fprintf(f, "# NEES pose covariance (pure-C approximation)\n");
    fprintf(f, "# camera-frame [rvec(3);tvec(3)] perturbation of T_cw\n");
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
                   s->t_track + s->t_lm + s->t_view
#if NGD_BOW
                   + s->t_lc
#endif
                   ;
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
    row("track", s->t_track, s->n_orb);
    row("lm",    s->t_lm,    s->n_lm);
#if NGD_BOW
    row("lc",    s->t_lc,    s->n_lc);
#endif
    row("view",  s->t_view,  nFrames);
    fprintf(stderr, "%-10s %12.1f %12.3f\n", "sum", total, nFrames > 0 ? total / nFrames : 0.0);
    if (s->yolo) s->yolo->print_timing(nFrames);
}

void ngd_app_system_shutdown(ngd_app_system *s)
{
#if defined(NGD_LM_THREAD) && NGD_LM_THREAD
    /* [①] 先停工作线程并排空残余完成事件 (清扫 retired), 再释放任何地图对象 */
    ngd_lmthread_shutdown();
    lm_pump(s);
#endif
#if NGD_ORB_DSP
    ngd_orb_set_backend(NULL);
    orb_dsp_exit();
#endif
    for (int i = 0; i < s->nOwned; ++i) { ngd_frame_free(s->owned[i]); free(s->owned[i]); }
    free(s->owned);
    for (int i = 0; i < s->map.nMPs; ++i) ngd_mappoint_free(s->map.mps[i]);
    for (int i = 0; i < s->map.nKFs; ++i) ngd_keyframe_free(s->map.kfs[i]);
#if NGD_BOW
    ngd_loopclosing_free(&s->lc);
#endif
    ngd_local_mapping_free(&s->lm);
#if NGD_BOW
    ngd_kfdb_free(&s->kfdb);
#endif
    ngd_map_free(&s->map);
    ngd_tracking_destroy(&s->tk);
    free(s->relPoses); free(s->relRefs); free(s->relTimes); free(s->relLost);
    free(s->relCov); free(s->relCovValid);
    free(s->gray_buf); free(s->last_gray); free(s->last_mask); free(s->mask_curr);
    if (s->yolo) { delete s->yolo; s->yolo = NULL; }
#if NGD_BOW
    if (s->vocab) ngd_bow_vocab_free(s->vocab);
#endif
    ngd_app_viewer_free(&s->viewer);
    memset(s, 0, sizeof(*s));
}
