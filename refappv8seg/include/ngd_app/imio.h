#ifndef NGD_APP_IMIO_H
#define NGD_APP_IMIO_H

/*
 * ngd_app/imio.h — image + depth I/O thin shell (OpenCV).
 *
 * Loads TUM RGB-D frames into the flat row-major buffers the pure-C core
 * consumes (ngd_frame_build_rgbd takes uint8 gray + float metres depth,
 * stride == width). Faithful to NGD-SLAM GrabImageRGBD (Frame.cc:1126-1147):
 *   - RGB -> grayscale (TUM3 zero distortion, no undistort here; the core's
 *     Frame build treats mvKeys==mvKeysUn for zero distortion)
 *   - depth uint16 PNG -> float metres = raw / DepthMapFactor (raw 0 = no depth,
 *     stays 0.0; frame.c guards d<=0 -> depth=-1/uRight=-1, no div-by-zero)
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Load `path` as 8-bit grayscale, contiguous row-major (stride == w).
 * On success returns 1, sets *out (caller free()) / *w / *h. 0 on failure. */
int ngd_app_load_gray(const char *path, uint8_t **out, int *w, int *h);

/* Load a TUM uint16 depth PNG -> float metres (raw / depthMapFactor).
 * Raw-0 pixels stay 0.0 (no depth). CV_32F inputs are copied as-is (assume
 * already metres). On success returns 1, sets *out (caller free()) / *w / *h. */
int ngd_app_load_depth_meters(const char *path, float depthMapFactor,
                              float **out, int *w, int *h);

#ifdef __cplusplus
}
#endif
#endif /* NGD_APP_IMIO_H */
