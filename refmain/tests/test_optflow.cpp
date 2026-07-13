// test_optflow.cpp — verify the optical-flow SLAM tracking shell:
//   SearchByOpticalFlow (LK propagation) + cv::solvePnPRansac (PnP) recover a
//   known camera pose from synthetic textured patches projected into two views.
#include "ngd_shell/optflow.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <cstdio>
#include <cmath>
#include <vector>
#include <random>

static int fails = 0;
#define CHECK(cond, msg) do { if(!(cond)){ std::printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); ++fails; } } while(0)

// Project a world point with a row-major 4x4 Tcw + K (fx,fy,cx,cy). Returns false if z<=0.
static bool project(const float Xw[3], const float Tcw[16], float fx, float fy, float cx, float cy, float uv[2])
{
    float Xc[3];
    for (int r = 0; r < 3; ++r)
        Xc[r] = Tcw[r * 4 + 0] * Xw[0] + Tcw[r * 4 + 1] * Xw[1] + Tcw[r * 4 + 2] * Xw[2] + Tcw[r * 4 + 3];
    if (Xc[2] <= 1e-3f) return false;
    uv[0] = fx * Xc[0] / Xc[2] + cx;
    uv[1] = fy * Xc[1] / Xc[2] + cy;
    return true;
}

static void rot_err_deg(const float T[16], const float Tgt[16], float &deg, float &dt)
{
    // rotation angle between R and Rgt, and translation difference
    float RtTgt[9];   // R^T * Rgt
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            float s = 0;
            for (int k = 0; k < 3; ++k) s += T[k * 4 + i] * Tgt[k * 4 + j];   // R^T row i * Rgt col j
            RtTgt[i * 3 + j] = s;
        }
    float tr = RtTgt[0] + RtTgt[4] + RtTgt[8];
    if (tr > 1.0f) tr = 1.0f;
    if (tr < -1.0f) tr = -1.0f;
    deg = std::acos(tr) * 57.29578f;
    dt = 0;
    for (int r = 0; r < 3; ++r) dt += std::fabs(T[r * 4 + 3] - Tgt[r * 4 + 3]);
}

int main()
{
    const int W = 640, H = 480;
    const float fx = 535.4f, fy = 539.2f, cx = 320.1f, cy = 247.6f;
    float K[9] = { fx, 0, cx, 0, fy, cy, 0, 0, 1 };

    // camera 0 = identity; camera 1 = a small motion (translate + rotate a touch)
    float T0[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
    // modest yaw + translation in X/Z (well-conditioned, non-degenerate)
    float ang = 0.05f, c = std::cos(ang), s = std::sin(ang);
    float T1[16] = { c,0,-s,0.10f,
                     0,1, 0,0.00f,
                     s,0, c,0.05f,
                     0,0, 0,1 };

    // 3D points spread in front of the camera; keep projections well inside the image.
    std::mt19937 rng(2024);
    std::uniform_real_distribution<float> u(-0.6f, 0.6f), uz(2.0f, 4.0f);
    const int N = 40;
    std::vector<float> world(3 * N), p0(2 * N), p1(2 * N);
    int nvis = 0;
    std::vector<int> visIdx;
    for (int i = 0; i < N; ++i) {
        float Xw[3] = { u(rng), u(rng) * 0.7f, uz(rng) };
        float a[2], b[2];
        if (!project(Xw, T0, fx, fy, cx, cy, a)) continue;
        if (!project(Xw, T1, fx, fy, cx, cy, b)) continue;
        if (a[0] < 20 || a[0] > W - 20 || a[1] < 20 || a[1] > H - 20) continue;
        if (b[0] < 20 || b[0] > W - 20 || b[1] < 20 || b[1] > H - 20) continue;
        world[3 * nvis] = Xw[0]; world[3 * nvis + 1] = Xw[1]; world[3 * nvis + 2] = Xw[2];
        p0[2 * nvis] = a[0]; p0[2 * nvis + 1] = a[1];
        p1[2 * nvis] = b[0]; p1[2 * nvis + 1] = b[1];
        visIdx.push_back(nvis);
        ++nvis;
    }
    CHECK(nvis >= 20, "enough visible, well-distributed points");

    // Stamp a unique deterministic-random-texture patch at each point in BOTH
    // views (same patch per point) so LK can lock onto and track each feature.
    cv::Mat lastImg = cv::Mat::zeros(H, W, CV_8UC1);
    cv::Mat currImg = cv::Mat::zeros(H, W, CV_8UC1);
    const int PR = 6;   // patch radius (13x13)
    for (int i = 0; i < nvis; ++i) {
        int ax = (int)p0[2 * i], ay = (int)p0[2 * i + 1];
        int bx = (int)p1[2 * i], by = (int)p1[2 * i + 1];
        for (int dy = -PR; dy <= PR; ++dy)
            for (int dx = -PR; dx <= PR; ++dx) {
                uint8_t v = (uint8_t)(rng() & 0xFF);
                if (ax + dx >= 0 && ax + dx < W && ay + dy >= 0 && ay + dy < H) lastImg.at<uint8_t>(ay + dy, ax + dx) = v;
                if (bx + dx >= 0 && bx + dx < W && by + dy >= 0 && by + dy < H) currImg.at<uint8_t>(by + dy, bx + dx) = v;
            }
    }

    // last frame: every visible keypoint has a stable MapPoint.
    std::vector<int> valid(nvis, 1);
    std::vector<float> outKeys(2 * nvis), outKeysByLast(2 * nvis);
    std::vector<int> outIdx(nvis);
    int nTracked = ngd_shell_search_by_optical_flow(lastImg.data, currImg.data, nullptr,
                                                    W, H, (int)lastImg.step,
                                                    p0.data(), valid.data(), nvis,
                                                    outKeys.data(), outIdx.data());
    std::printf("  [optflow] tracked %d / %d points\n", nTracked, nvis);
    CHECK(nTracked >= 15, "SearchByOpticalFlow tracked enough points");

    // Build (world MP, current keypoint) correspondences for PnP.
    std::vector<float> mps(3 * nTracked), ipts(2 * nTracked);
    for (int k = 0; k < nTracked; ++k) {
        int li = outIdx[k];   // index into the visible-point arrays
        mps[3 * k + 0] = world[3 * li + 0];
        mps[3 * k + 1] = world[3 * li + 1];
        mps[3 * k + 2] = world[3 * li + 2];
        ipts[2 * k + 0] = outKeys[2 * k + 0];
        ipts[2 * k + 1] = outKeys[2 * k + 1];
    }

    float Tcw[16];
    std::vector<int> inliers(nTracked, 0);
    int nin = ngd_shell_pnp_ransac(mps.data(), ipts.data(), nTracked, K, nullptr,
                                   nullptr, 100, 5.0f, 0.99f, inliers.data(), Tcw);
    std::printf("  [optflow] PnP inliers %d / %d\n", nin, nTracked);
    CHECK(nin >= 10, "PnP found enough inliers");

    float deg, dt;
    rot_err_deg(Tcw, T1, deg, dt);
    std::printf("  [optflow] recovered pose: rot_err = %.3f deg, |dt| = %.4f (truth T1)\n", deg, dt);
    CHECK(deg < 2.0f, "PnP recovers rotation (<2 deg)");
    CHECK(dt < 0.10f, "PnP recovers translation (|dt|<0.1)");

    if (fails == 0) { std::printf("test_optflow: PASS\n"); return 0; }
    std::printf("test_optflow: %d FAILURES\n", fails);
    return 1;
}
