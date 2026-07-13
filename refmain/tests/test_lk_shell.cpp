// test_lk_shell.cpp — verify the OpenCV LK callback (ngd_shell_lk_default):
//   (1) tracks a known integer shift on a textured image;
//   (2) drives the refactor core's mask chain (ngd_mask_predict_current) with
//       real OpenCV LK (the "光流 = 薄壳" binding), complementing refactor's
//       test_mask which injected a synthetic flow.
#include "ngd_shell/lk_shell.h"
#include "ngd/mask.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <cstdio>
#include <cmath>
#include <vector>
#include <random>

static int fails = 0;
#define CHECK(cond, msg) do { if(!(cond)){ std::printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); ++fails; } } while(0)

int main()
{
    const int W = 160, H = 120;

    // ---- (1) direct LK: track a known shift on random texture ----
    {
        cv::Mat prev(H, W, CV_8UC1), curr(H, W, CV_8UC1);
        std::mt19937 rng(1234);
        std::uniform_int_distribution<int> u(0, 255);
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                prev.at<uint8_t>(y, x) = (uint8_t)u(rng);

        const int dx = 5, dy = 3;   // curr = prev shifted by (dx, dy)
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) {
                int sx = x - dx, sy = y - dy;
                curr.at<uint8_t>(y, x) = (sx >= 0 && sx < W && sy >= 0 && sy < H)
                                         ? prev.at<uint8_t>(sy, sx) : 0;
            }

        // grid of interior points to track (avoid the shifted-out border band)
        std::vector<float> ptsIn, ptsOut;
        for (int y = 20; y < H - 20; y += 10)
            for (int x = 20; x < W - 20; x += 10) { ptsIn.push_back((float)x); ptsIn.push_back((float)y); }
        int n = (int)ptsIn.size() / 2;
        std::vector<uint8_t> status(n, 0);
        ptsOut.resize(2 * n);
        int rc = ngd_shell_lk_default(prev.data, curr.data, W, H, (int)prev.step,
                                      ptsIn.data(), n, ptsOut.data(), status.data(), nullptr);
        CHECK(rc == 0, "lk_default returns 0");

        // median tracked displacement should be ~ (dx, dy)
        std::vector<float> ddx, ddy;
        for (int i = 0; i < n; ++i) {
            if (!status[i]) continue;
            ddx.push_back(ptsOut[2 * i + 0] - ptsIn[2 * i + 0]);
            ddy.push_back(ptsOut[2 * i + 1] - ptsIn[2 * i + 1]);
        }
        CHECK(!ddx.empty(), "lk_default tracked at least one point");
        std::sort(ddx.begin(), ddx.end());
        std::sort(ddy.begin(), ddy.end());
        float mdx = ddx[ddx.size() / 2], mdy = ddy[ddy.size() / 2];
        std::printf("  [lk] median shift = (%.2f, %.2f), expected (%d, %d), tracked %zu/%d\n",
                    mdx, mdy, dx, dy, ddx.size(), n);
        CHECK(std::fabs(mdx - dx) < 0.5f && std::fabs(mdy - dy) < 0.5f, "lk recovers known shift");
    }

    // ---- (2) mask chain end-to-end with real OpenCV LK ----
    {
        // Random-texture grayscale + a large filled seed region (must cover
        // enough 15x15 ExtractDynaPoints cells to clear DBSCAN minPts=15); curr
        // = prev shifted.
        const int W2 = 200, H2 = 160;
        cv::Mat prev(H2, W2, CV_8UC1), curr(H2, W2, CV_8UC1);
        std::mt19937 rng(99);
        std::uniform_int_distribution<int> u(0, 255);
        for (int y = 0; y < H2; ++y)
            for (int x = 0; x < W2; ++x)
                prev.at<uint8_t>(y, x) = (uint8_t)u(rng);
        const int dx = 8, dy = 5;
        for (int y = 0; y < H2; ++y)
            for (int x = 0; x < W2; ++x) {
                int sx = x - dx, sy = y - dy;
                curr.at<uint8_t>(y, x) = (sx >= 0 && sx < W2 && sy >= 0 && sy < H2)
                                         ? prev.at<uint8_t>(sy, sx) : 0;
            }

        cv::Mat lastMask = cv::Mat::zeros(H2, W2, CV_8UC1);
        const int x0 = 50, x1 = 150, y0 = 40, y1 = 140;     // 100x100 box, centroid (100,90)
        for (int y = y0; y < y1; ++y)
            for (int x = x0; x < x1; ++x) lastMask.at<uint8_t>(y, x) = 1;
        cv::Mat depth(H2, W2, CV_32F, 1.0f);                // constant 1 m

        cv::Mat outMask = cv::Mat::zeros(H2, W2, CV_8UC1);
        ngd_mask_predict_current(prev.data, curr.data, lastMask.data,
                                 (const float *)depth.data, W2, H2, (int)prev.step,
                                 ngd_shell_lk_default, nullptr, outMask.data);

        int nz = cv::countNonZero(outMask);
        // centroid of predicted mask
        cv::Moments m = cv::moments(outMask, true);
        float cx = nz > 0 ? (float)(m.m10 / m.m00) : -1.0f;
        float cy = nz > 0 ? (float)(m.m01 / m.m00) : -1.0f;
        std::printf("  [mask-chain] OpenCV-LK predicted mask: %d px, centroid (%.1f, %.1f) "
                    "[seed centroid (100,90) + shift (%d,%d) -> expect ~(%d,%d)]\n",
                    nz, cx, cy, dx, dy, 100 + dx, 90 + dy);
        CHECK(nz > 0, "mask chain produced a non-empty mask with real OpenCV LK");
        // centroid should land near seed-centroid + shift (DBSCAN bbox is loose, ±20)
        CHECK(std::fabs(cx - (100 + dx)) < 22.0f && std::fabs(cy - (90 + dy)) < 22.0f,
              "predicted mask centroid near seed + shift");
    }

    if (fails == 0) { std::printf("test_lk_shell: PASS\n"); return 0; }
    std::printf("test_lk_shell: %d FAILURES\n", fails);
    return 1;
}
