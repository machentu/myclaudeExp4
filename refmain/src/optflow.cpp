// ngd_shell/optflow.cpp — OpenCV optical-flow SLAM tracking (thin shell).
// SearchByOpticalFlow (ORBmatcher.cc:2016) + cv::solvePnPRansac (Tracking.cc:3040).
#include "ngd_shell/optflow.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/calib3d.hpp>
#include <opencv2/video/tracking.hpp>

#include <vector>
#include <cstring>
#include <cstdlib>

/* ===== ngd_shell LK/PnP config (edit here + recompile; was env vars) ===== */
#ifndef NGD_LK_WINSIZE
#define NGD_LK_WINSIZE 9        /* LK window size (5..63) */
#endif
#ifndef NGD_LK_TERM_ORIG
#define NGD_LK_TERM_ORIG 0      /* 1 = OpenCV default termCrit (30/0.01); 0 = tightened (50/0.001) */
#endif
#ifndef NGD_OF_FB
#define NGD_OF_FB 0             /* OpenCV-shell FB consistency; 1 = on (FB_THRESH=1px), 0 = off */
#endif
#ifndef NGD_PNP_REFINE
#define NGD_PNP_REFINE 0        /* 1 = LM-refine solvePnPRansac result on all inliers; 0 = off */
#endif

extern "C" int ngd_shell_search_by_optical_flow(const uint8_t *last_gray, const uint8_t *curr_gray,
                                                const uint8_t *mask, int w, int h, int stride,
                                                const float *last_keys, const int *last_mp_valid, int n_last,
                                                float *out_keys, int *out_mp_idx)
{
    if (n_last <= 0 || !last_gray || !curr_gray || !last_keys || !last_mp_valid) return 0;
    if (w <= 0 || h <= 0 || stride < w) return 0;

    cv::Mat lastImg(h, w, CV_8UC1, const_cast<uint8_t *>(last_gray), stride);
    cv::Mat currImg(h, w, CV_8UC1, const_cast<uint8_t *>(curr_gray), stride);

    // Collect last-frame keypoints that carry a stable MapPoint (Observations>=3).
    // (ORBmatcher.cc:2028-2035)
    std::vector<cv::Point2f> lastPts;
    std::vector<int> srcIdx;
    lastPts.reserve(n_last);
    srcIdx.reserve(n_last);
    for (int i = 0; i < n_last; ++i) {
        if (!last_mp_valid[i]) continue;
        lastPts.push_back(cv::Point2f(last_keys[2 * i + 0], last_keys[2 * i + 1]));
        srcIdx.push_back(i);
    }
    if (lastPts.empty()) return 0;

    std::vector<cv::Point2f> currPts;
    std::vector<uchar> st;
    std::vector<float> err;
    // Explicit LK params + tighter termination (default is COUNT+EPS, 30, 0.01):
    // more iterations + a 10x tighter epsilon for sub-pixel precision (reduces
    // per-frame pose jitter). winSize 9 / maxLevel 3: a tighter LK window gives
    // markedly more precise sub-pixel localization -> far less OF pose jitter
    // (phase-3 ATE on fr3_walk: ws31 10.91, ws21 5.98, ws15 1.84, ws9 1.36 cm).
    // winSize is tunable at runtime via NGD_LK_WINSIZE.
    int ws = NGD_LK_WINSIZE;
#if NGD_LK_TERM_ORIG
    int tcIter = 30; float tcEps = 0.01f;   /* OpenCV default = 原版 */
#else
    int tcIter = 50; float tcEps = 0.001f;  /* tightened */
#endif
    cv::TermCriteria termCrit(cv::TermCriteria::COUNT + cv::TermCriteria::EPS, tcIter, tcEps);
    cv::Size winSize(ws, ws);
    int maxLevel = 3;
    cv::calcOpticalFlowPyrLK(lastImg, currImg, lastPts, currPts, st, err,
                             winSize, maxLevel, termCrit);  // forward

    // Forward-backward consistency + error gate. DEFAULT OFF (faithful to the
    // original SearchByOpticalFlow, which only checks LK status + bounds + mask
    // and lets solvePnPRansac's RANSAC reject bad tracks in-PnP). FB was the
    // phase-3 default with MLPnP, but it drops ~20-30% of points -> lowers
    // mnMatchesInliers -> c2 over-fires -> ORB extracted ~every frame (99%).
    // With solvePnPRansac (now the default PnP) RANSAC handles outlier rejection,
    // so FB is unnecessary and harmful to the ORB-skip rate. Set NGD_OF_FB=1 to
    // re-enable (e.g. for A/B with MLPnP).
    const bool useFB = (NGD_OF_FB != 0);
    std::vector<cv::Point2f> backPts;
    std::vector<uchar> stBack;
    std::vector<float> errBack;
    if (useFB)
        cv::calcOpticalFlowPyrLK(currImg, lastImg, currPts, backPts, stBack, errBack,
                                 winSize, maxLevel, termCrit);
    const float FB_THRESH = 1.0f;    // px round-trip tolerance
    const float ERR_THRESH = 10.0f;  // forward LK error (SSD-like) tolerance

    const bool hasMask = (mask != nullptr);
    int n = 0;
    for (size_t i = 0; i < currPts.size(); ++i) {
        if (!st[i]) continue;                                       // cc:2058
        if (useFB) {
            if (!stBack[i]) continue;                               // FB: backward track must succeed
            float dx = backPts[i].x - lastPts[i].x;
            float dy = backPts[i].y - lastPts[i].y;
            if (dx * dx + dy * dy > FB_THRESH * FB_THRESH) continue;   // FB: round-trip must close
            if (err[i] > ERR_THRESH) continue;                      // forward error must be small
        }
        const float px = currPts[i].x;
        const float py = currPts[i].y;
        if (px < 0.0f || py < 0.0f || px >= (float)w || py >= (float)h) continue;  // cc:2065 bounds
        if (hasMask && mask[(int)py * stride + (int)px] != 0) continue;            // cc:2069 dynamic
        out_keys[2 * n + 0] = px;
        out_keys[2 * n + 1] = py;
        out_mp_idx[n] = srcIdx[i];
        ++n;
    }
    return n;
}

extern "C" int ngd_shell_pnp_ransac(const float *mapPoints, const float *imgPoints, int n,
                                    const float K[9], const float *dist,
                                    const float *init_Tcw, int iter, float reprojErr, float conf,
                                    int *inliers_out, float out_Tcw[16])
{
    if (n < 4 || !mapPoints || !imgPoints || !K || !out_Tcw) return -1;

    cv::Mat objPts(n, 1, CV_32FC3);
    cv::Mat imgPts(n, 1, CV_32FC2);
    for (int i = 0; i < n; ++i) {
        objPts.at<cv::Vec3f>(i) = cv::Vec3f(mapPoints[3 * i + 0], mapPoints[3 * i + 1], mapPoints[3 * i + 2]);
        imgPts.at<cv::Vec2f>(i) = cv::Vec2f(imgPoints[2 * i + 0], imgPoints[2 * i + 1]);
    }
    cv::Mat camK(3, 3, CV_32F);
    std::memcpy(camK.data, K, 9 * sizeof(float));
    cv::Mat distCoef = cv::Mat::zeros(4, 1, CV_32F);   // TUM3 zero distortion; original passes mDistCoef
    if (dist) {
        cv::Mat d(1, 5, CV_32F);
        std::memcpy(d.data, dist, 5 * sizeof(float));
        d.copyTo(distCoef);
    }

    cv::Mat rvec, tvec;
    bool useExtrinsic = false;
    if (init_Tcw) {
        // Decompose init_Tcw ([R|t;0 1] row-major) -> rvec/tvec seed.
        cv::Mat R(3, 3, CV_32F);
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 3; ++c)
                R.at<float>(r, c) = init_Tcw[r * 4 + c];
        cv::Rodrigues(R, rvec);
        tvec = cv::Mat(3, 1, CV_32F);
        for (int r = 0; r < 3; ++r) tvec.at<float>(r) = init_Tcw[r * 4 + 3];
        useExtrinsic = true;
    }

    cv::Mat inliers;
    bool ok = cv::solvePnPRansac(objPts, imgPts, camK, distCoef, rvec, tvec, useExtrinsic,
                                 iter, reprojErr, conf, inliers, cv::SOLVEPNP_ITERATIVE);
    if (!ok) return -1;

    // solvePnPRansac may output rvec/tvec as CV_64F; convert to CV_32F for a
    // type-stable read (avoids reading a double as float -> garbage translation).
    cv::Mat rvec32, tvec32;
    rvec.convertTo(rvec32, CV_32F);
    tvec.convertTo(tvec32, CV_32F);
    if (rvec32.empty() || tvec32.empty()) return -1;

    /* NGD_PNP_REFINE: after RANSAC finds inliers, re-solve on ALL inliers with
     * cv::solvePnP (LM refine, init = RANSAC result). Isolates RANSAC best-sample
     * sensitivity (A, default) from deterministic least-squares over the inlier
     * set (B). Default off (A = exact original Tracking.cc:3040). */
#if NGD_PNP_REFINE
    if (!inliers.empty()) {
        cv::Mat inlObj, inlImg;
        for (int i = 0; i < inliers.rows; ++i) {
            int idx = inliers.at<int>(i);
            if (idx >= 0 && idx < n) { inlObj.push_back(objPts.row(idx)); inlImg.push_back(imgPts.row(idx)); }
        }
        if (inlObj.rows >= 4) {
            cv::Mat r = rvec32.clone(), t = tvec32.clone();
            cv::solvePnP(inlObj, inlImg, camK, distCoef, r, t, true, cv::SOLVEPNP_ITERATIVE);
            rvec32 = r; tvec32 = t;
        }
    }
#endif

    cv::Mat R;
    cv::Rodrigues(rvec32, R);   // R is CV_32F

    // out_Tcw = [R|t; 0 1], row-major homogeneous.
    for (int i = 0; i < 16; ++i) out_Tcw[i] = 0.0f;
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            out_Tcw[r * 4 + c] = R.at<float>(r, c);
    for (int r = 0; r < 3; ++r) out_Tcw[r * 4 + 3] = tvec32.at<float>(r);
    out_Tcw[15] = 1.0f;

    int nin = 0;
    if (inliers_out) {
        for (int i = 0; i < n; ++i) inliers_out[i] = 0;
        if (!inliers.empty()) {
            for (int i = 0; i < inliers.rows; ++i) {
                int idx = inliers.at<int>(i);
                if (idx >= 0 && idx < n) { inliers_out[idx] = 1; ++nin; }
            }
        }
    } else {
        nin = inliers.empty() ? 0 : inliers.rows;
    }
    return nin;
}
