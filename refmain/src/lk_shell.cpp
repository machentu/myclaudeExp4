// ngd_shell/lk_shell.cpp — OpenCV LK optical-flow callback for the pure-C core.
// Wraps cv::calcOpticalFlowPyrLK with default args (Tracking.cc:4271) so the
// refactor core's mask chain (ngd_mask_predict_current) can use real OpenCV LK.
#include "ngd_shell/lk_shell.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/video/tracking.hpp>

#include <vector>

extern "C" int ngd_shell_lk_default(const uint8_t *prev, const uint8_t *curr,
                                    int w, int h, int stride,
                                    const float *pts_in, int n,
                                    float *pts_out, uint8_t *status, void *user)
{
    (void)user;
    if (n <= 0 || !prev || !curr || !pts_in || !pts_out || !status) return 1;
    if (w <= 0 || h <= 0 || stride < w) return 1;

    // Wrap the caller's grayscale buffers as cv::Mat WITHOUT copying.
    cv::Mat prevImg(h, w, CV_8UC1, const_cast<uint8_t *>(prev), stride);
    cv::Mat currImg(h, w, CV_8UC1, const_cast<uint8_t *>(curr), stride);

    std::vector<cv::Point2f> prevPts((size_t)n), currPts;
    for (int i = 0; i < n; ++i) {
        prevPts[i].x = pts_in[2 * i + 0];
        prevPts[i].y = pts_in[2 * i + 1];
    }

    std::vector<uchar> st;
    std::vector<float> err;
    // Default arguments match Tracking.cc:4271 (winSize 21×21, maxLevel 3,
    // TermCriteria COUNT+EPS 30/0.01). cv::calcOpticalFlowPyrLK fills currPts/st.
    cv::calcOpticalFlowPyrLK(prevImg, currImg, prevPts, currPts, st, err);

    if ((int)currPts.size() != n || (int)st.size() != n) return 2;
    for (int i = 0; i < n; ++i) {
        pts_out[2 * i + 0] = currPts[i].x;
        pts_out[2 * i + 1] = currPts[i].y;
        status[i] = st[i] ? 1 : 0;
    }
    return 0;
}
