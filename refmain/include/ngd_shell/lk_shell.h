#ifndef NGD_SHELL_LK_H
#define NGD_SHELL_LK_H

/*
 * ngd_shell/lk_shell.h — OpenCV LK optical-flow callback for the pure-C core.
 *
 * The refactor core's dynamic-mask chain (refactor/include/ngd/mask.h,
 * ngd_mask_predict_current) is dependency-free and accepts the LK optical-flow
 * step as an injectable callback `ngd_lk_flow_fn`. This thin shell binds that
 * callback to cv::calcOpticalFlowPyrLK with the default arguments the original
 * NGD-SLAM used (Tracking.cc:4271 — all-default call: winSize 21×21, maxLevel 3,
 * TermCriteria COUNT+EPS, 30, 0.01).
 *
 * Passing ngd_shell_lk_default as the `lk` argument of ngd_mask_predict_current
 * connects the pure-C mask chain to real OpenCV LK (doc §6.7 "光流 = 薄壳").
 *
 * extern "C" so the C core / C demos can take its address as a ngd_lk_flow_fn.
 */

#include "ngd/mask.h"   /* ngd_lk_flow_fn typedef */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Default OpenCV LK optical-flow callback.
 *   prev/curr : uint8 grayscale, w×h, `stride` bytes/row (no copy; wrapped as cv::Mat).
 *   pts_in[n] : source (x,y) pairs (2*n floats).
 *   pts_out   : tracked (x,y) pairs (2*n floats, caller buffer).
 *   status    : per-point success flag (n bytes, 1=tracked).
 * Returns 0 on success, non-zero on error. `user` is ignored. */
int ngd_shell_lk_default(const uint8_t *prev, const uint8_t *curr,
                         int w, int h, int stride,
                         const float *pts_in, int n,
                         float *pts_out, uint8_t *status, void *user);

#ifdef __cplusplus
}
#endif
#endif /* NGD_SHELL_LK_H */
