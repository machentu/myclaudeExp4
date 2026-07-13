#ifndef NGD_OPTFLOW_H
#define NGD_OPTFLOW_H

/*
 * ngd/optflow.h — Pure-C pyramidal Lucas-Kanade optical flow.
 *
 * Replaces the two OpenCV-dependent LK call-sites in the NGD-SLAM pipeline:
 *
 *   1. SearchByOpticalFlow (ORBmatcher.cc:2044):
 *        cv::calcOpticalFlowPyrLK(LastImg, CurrImg, lastPts, currPts, st, err,
 *                                 winSize, maxLevel, termCrit)
 *      → ngd_optflow_pyr_lk() or the convenience wrapper ngd_optflow_search().
 *
 *   2. Dynamic-mask LK callback (Tracking.cc:4271, mask.c):
 *        cv::calcOpticalFlowPyrLK(mImGrayLastKey, mImGray, lastDynaPoints,
 *                                  currDynaPoints, status, error)
 *      → ngd_optflow_lk_callback() (matches ngd_lk_flow_fn typedef in mask.h).
 *
 * The PnP half of the OF mode (cv::solvePnPRansac) already has a pure-C
 * replacement: ngd_mlpnp_solve_ransac() in mlpnp.h — no further work needed.
 *
 * Algorithm: iterative forward-additive Lucas-Kanade on a Gaussian image
 * pyramid. Each level propagates its displacement to the next finer level
 * (×2). At each level the 2×2 normal equations are solved per iteration;
 * ill-conditioned points (det ≈ 0) are marked as lost. Sub-pixel
 * interpolation uses bilinear sampling over the current image.
 *
 * Pyramid construction follows OpenCV's pyrDown: separable 5×5 Gaussian
 * [1 4 6 4 1]/16, border reflection, keep even rows/cols.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- LK parameters ---- */
typedef struct {
    int   win_size;   /* window width in pixels (typical: 9 for tracking, 21 for mask) */
    int   max_level;  /* pyramid levels (0 = single level, typical: 3) */
    int   max_iter;   /* iterations per level (typical: 30–50) */
    float epsilon;    /* convergence threshold in pixels (typical: 0.001–0.01) */
    float fb_thresh;  /* forward-backward round-trip px threshold (0=disabled, 1.0 typical) */
    float min_eig;    /* min eigenvalue per pixel of gradient normal matrix (0=disabled).
                        * OpenCV default=1e-4; discards low-texture points before LK.
                        * Set to 0 to disable (tracks all points). */
} ngd_lk_params;

/* Default parameter sets matching the two original call-sites. */
#define NGD_LK_DEFAULT_TRACKING { 9, 3, 50, 0.001f, 0.0f, 0.0f}  /* tighter convergence for best ATE */
#define NGD_LK_DEFAULT_MASK      {21, 3, 30, 0.01f,  0.0f, 0.0f}

/* ---- Core: pyramidal Lucas-Kanade ---- */

/**
 * Track points from prev_img to curr_img using pyramidal LK.
 *
 * Two backends are available:
 *   - Default (float pyramid): builds a float Gaussian pyramid for prev (better
 *     gradient precision at coarse levels) + uint8 pyramid for curr (bilinear
 *     sampling) + Gaussian window weighting.  Good sub-pixel convergence.
 *   - U8  (uint8 pyramid): both pyramids are uint8, flat window weights.
 *     Faster, uses less memory, and in practice more stable on noisy real data
 *     (fr3_walk ATE 11.81cm vs 14.58cm for float).
 *
 * Select backend at runtime via NGD_LK_BACKEND env: unset = float, "u8" = uint8.
 *
 * @param prev_img  Previous grayscale image (uint8, w×h, `stride` bytes/row).
 * @param curr_img  Current  grayscale image (same layout).
 * @param w, h      Image dimensions.
 * @param stride    Row stride in bytes (≥ w).
 * @param pts_in    Source (x,y) pairs: [x0,y0, x1,y1, ...] — n_pts × 2 floats.
 * @param n_pts     Number of points to track.
 * @param pts_out   Output (x,y) pairs (caller buffer, n_pts × 2 floats).
 * @param status    Per-point success (1 = tracked, 0 = lost). Caller buffer, n_pts bytes.
 * @param params    LK parameters (may be NULL → NGD_LK_DEFAULT_TRACKING).
 * @return 0 on success, non-zero if the input is invalid.
 *
 * Points that drift outside the image, land on an ill-conditioned patch
 * (det < 1e-6), or fail to converge within max_iter get status=0.
 * pts_out entries for failed points are set to the last valid position
 * (or the input position if they never moved).
 */
int ngd_optflow_pyr_lk(const uint8_t *prev_img, const uint8_t *curr_img,
                       int w, int h, int stride,
                       const float *pts_in, int n_pts,
                       float *pts_out, uint8_t *status,
                       const ngd_lk_params *params);

/* ---- Convenience: LK callback (mask chain) ---- */

/**
 * Callback matching the ngd_lk_flow_fn typedef (mask.h).
 *
 * Uses NGD_LK_DEFAULT_MASK (winSize=21, maxLevel=3, 30 iters, eps=0.01).
 *
 * @param user  Ignored (reserved for future parameter passing).
 */
int ngd_optflow_lk_callback(const uint8_t *prev, const uint8_t *curr,
                            int w, int h, int stride,
                            const float *pts_in, int n,
                            float *pts_out, uint8_t *status, void *user);

/* ---- Convenience: SearchByOpticalFlow wrapper ---- */

/**
 * Track last-frame keypoints into the current frame, filtered by a
 * dynamic-object mask.
 *
 * Faithful to the original SearchByOpticalFlow logic (ORBmatcher.cc:2016):
 *   1. LK-propagate last_keys → out_keys (only points with last_mp_valid[i] ≠ 0).
 *   2. If params->fb_thresh > 0: run backward LK and drop points whose round-trip
 *      error exceeds fb_thresh (key lever for OF pose stability, benchmark §8).
 *   3. Drop points that fall outside the image or land on a non-zero mask pixel.
 *
 * @param last_gray, curr_gray  Grayscale images (w×h, stride bytes/row).
 * @param mask  Dynamic mask (w×h, stride==w). 0 = keep, ≠ 0 = drop. May be NULL.
 * @param last_keys      (x,y) of last-frame keypoints [2 × n_last].
 * @param last_mp_valid  Per-point flag: 1 = has stable MapPoint, 0 = skip.
 * @param n_last         Number of input points.
 * @param out_keys       Tracked (x,y) in current frame (caller buffer, 2 × n_last).
 * @param out_mp_idx     Index into last_keys of each tracked point (caller buffer, n_last).
 * @param params         LK parameters (may be NULL → NGD_LK_DEFAULT_TRACKING).
 * @return Number of successfully tracked points (≤ n_last).
 */
int ngd_optflow_search(const uint8_t *last_gray, const uint8_t *curr_gray,
                       const uint8_t *mask, int w, int h, int stride,
                       const float *last_keys, const int *last_mp_valid, int n_last,
                       float *out_keys, int *out_mp_idx,
                       const ngd_lk_params *params);

/* ---- U8 backend: uint8 pyramids + flat window weights ---- */

/**
 * uint8-pyramid LK (ngd_optflow_pyr_lk variant).
 * Both prev and curr pyramids are uint8. Flat (uniform) window weights.
 * Faster, less memory, and empirically more stable on real noise than the
 * float+Gaussian default.  Use via ngd_optflow_search_u8() below.
 */
int ngd_optflow_pyr_lk_u8(const uint8_t *prev_img, const uint8_t *curr_img,
                          int w, int h, int stride,
                          const float *pts_in, int n_pts,
                          float *pts_out, uint8_t *status,
                          const ngd_lk_params *params);

/**
 * SearchByOpticalFlow wrapper (u8 backend).  Same semantics as
 * ngd_optflow_search() but uses ngd_optflow_pyr_lk_u8 internally.
 */
int ngd_optflow_search_u8(const uint8_t *last_gray, const uint8_t *curr_gray,
                          const uint8_t *mask, int w, int h, int stride,
                          const float *last_keys, const int *last_mp_valid, int n_last,
                          float *out_keys, int *out_mp_idx,
                          const ngd_lk_params *params);

#ifdef __cplusplus
}
#endif
#endif /* NGD_OPTFLOW_H */
