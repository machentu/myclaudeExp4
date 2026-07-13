// ngd_app/imio.cpp — image + depth I/O thin shell (OpenCV).
#include "ngd_app/imio.h"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/core.hpp>
#include <cstring>
#include <cstdlib>

extern "C" int ngd_app_load_gray(const char *path, uint8_t **out, int *w, int *h)
{
    if (!path || !out) return 0;
    cv::Mat img = cv::imread(path, cv::IMREAD_GRAYSCALE);
    if (img.empty()) return 0;
    if (img.type() != CV_8U) img.convertTo(img, CV_8U);

    const int W = img.cols, H = img.rows;
    uint8_t *buf = (uint8_t *)std::malloc((size_t)W * (size_t)H);
    if (!buf) return 0;
    // Make contiguous row-major with stride == W (cv::Mat step may include padding).
    for (int y = 0; y < H; ++y)
        std::memcpy(buf + (size_t)y * W, img.ptr<uint8_t>(y), (size_t)W);

    *out = buf; *w = W; *h = H;
    return 1;
}

extern "C" int ngd_app_load_depth_meters(const char *path, float depthMapFactor,
                                         float **out, int *w, int *h)
{
    if (!path || !out) return 0;
    cv::Mat raw = cv::imread(path, cv::IMREAD_UNCHANGED);
    if (raw.empty()) return 0;

    const int W = raw.cols, H = raw.rows;
    float *buf = (float *)std::malloc(sizeof(float) * (size_t)W * (size_t)H);
    if (!buf) return 0;

    if (raw.type() == CV_32F) {
        // Already float: copy as-is (assume metres; factor ignored).
        for (int y = 0; y < H; ++y)
            std::memcpy(buf + (size_t)y * W, raw.ptr<float>(y), (size_t)W * sizeof(float));
    } else {
        // TUM uint16 PNG -> metres = raw / DepthMapFactor.
        cv::Mat u16;
        if (raw.type() != CV_16U) raw.convertTo(u16, CV_16U);
        else u16 = raw;
        const float inv = (depthMapFactor > 0.0f) ? (1.0f / depthMapFactor) : 1.0f;
        for (int y = 0; y < H; ++y) {
            const uint16_t *s = u16.ptr<uint16_t>(y);
            float *d = buf + (size_t)y * W;
            for (int x = 0; x < W; ++x) {
                uint16_t v = s[x];
                d[x] = v ? (float)v * inv : 0.0f;
            }
        }
    }

    *out = buf; *w = W; *h = H;
    return 1;
}
