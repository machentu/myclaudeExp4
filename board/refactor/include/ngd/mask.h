#ifndef NGD_MASK_H
#define NGD_MASK_H

/*
 * ngd/mask.h — NGD dynamic-mask prediction chain (pure C).
 *
 * Faithful port of the NGD-specific mask pipeline that propagates the previous
 * frame's dynamic mask into the current frame (src/Tracking.cc:4249-4587):
 *
 *   PredictCurrentMask (:4249)
 *     ├─ erode last mask            (11×11 rect, erosionSize=5)
 *     ├─ ExtractDynaPoints (:4355)  (15×15 grid, FAST-style 4-cross seed pick)
 *     ├─ calcOpticalFlowPyrLK       (OpenCV, all-default args)  ← thin shell
 *     ├─ depth filter               (NaN / bounds / depth >= 0.05)
 *     ├─ ClusterWithDBSCAN (:4402)  (depth pre-seg + 3D DBSCAN, eps=50 minPts=15)
 *     └─ CreateMaskFromClusters (:4486) (bbox+15, depth gate ±0.3, largest CC, dilate 7×7)
 *
 * The ONLY hard OpenCV dependency in the whole chain is the LK optical-flow
 * call. Everything else is reimplemented in pure C here, so the core stays
 * dependency-free and unit-testable; LK is injected as a callback
 * (ngd_lk_flow_fn). The thin-shell side binds cv::calcOpticalFlowPyrLK with its
 * default arguments (winSize 21×21, maxLevel 3, COUNT+EPS 30/0.01); tests inject
 * a synthetic flow. This mirrors the doc §6.7 "光流 = 薄壳" decision and the
 * established EPnP-for-MLPnP substitution pattern.
 *
 * Mask convention: uint8 binary, 1 = dynamic, 0 = static (same as the original
 * CV_8UC1 mask). Mask buffers are w*h contiguous; grayscale/depth take an
 * explicit row stride.
 *
 * Two latent bugs in the original are NOT propagated (faithful to intent):
 *   - ComputePixelPotential leaves isBrighter/isDarker uninitialized
 *     (Tracking.cc:4325) → initialized to 0 here.
 *   - its bounds check (potential=-1) is immediately overwritten by potential=0
 *     (Tracking.cc:4319/4322), a dead guard that reads out of bounds near the
 *     border → here any out-of-bounds 4-cross neighbour yields -1.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct { float x, y; } ngd_pt2f;
typedef struct { float x, y, z; } ngd_pt3f;

/*
 * LK pyramid optical-flow callback (thin shell binds cv::calcOpticalFlowPyrLK).
 *   prev/curr : uint8 grayscale, w×h, row-major with `stride` bytes/row.
 *   pts_in[n] : source points as (x,y) pairs (2*n floats).
 *   pts_out   : tracked points, 2*n floats (caller buffer).
 *   status    : per-point track flag, 1 = success (caller buffer, n bytes).
 * Returns 0 on success. `user` is passed through from the orchestrator caller.
 */
typedef int (*ngd_lk_flow_fn)(const uint8_t *prev, const uint8_t *curr,
                              int w, int h, int stride,
                              const float *pts_in, int n,
                              float *pts_out, uint8_t *status, void *user);

/* Binary morphology with a (2*half+1)×(2*half+1) rectangular kernel.
 * Border convention: out-of-bounds neighbours are ignored (binary erode = AND
 * of the in-bounds window; dilate = OR), matching OpenCV erode/dilate at the
 * border for a {0,1} source. `in` and `out` may alias. */
void ngd_mask_erode (const uint8_t *in, uint8_t *out, int w, int h, int half); /* half=5 → 11×11 */
void ngd_mask_dilate(const uint8_t *in, uint8_t *out, int w, int h, int half); /* half=3 → 7×7  */

/* FAST-style 4-cross corner potential (Tracking::ComputePixelPotential,
 * Tracking.cc:4308-4352). Offsets (-3,0)(0,3)(3,0)(0,-3). Returns:
 *   -1                      if any cross neighbour is out of bounds;
 *   200 + minBrightDiff     if >=3 neighbours are brighter than the centre;
 *   200 + minDarkDiff       if >=3 neighbours are darker than the centre;
 *   max(minBright, minDark) otherwise (minBright/minDark are -1 when absent).
 * (isBrighter/isDarker are initialized to 0, unlike the original.) */
int ngd_mask_pixel_potential(const uint8_t *gray, int w, int h, int stride, int x, int y);

/* Tracking::ExtractDynaPoints (Tracking.cc:4355-4399). Tiles the image in
 * cellSize×cellSize cells; inside each, over mask==1 pixels, keeps the pixel
 * with the largest ngd_mask_pixel_potential (early-break once it exceeds
 * `threshold`). Writes up to (w/cellSize)*(h/cellSize) seeds to `out`; the
 * count goes to *out_n. Empty/mismatched-size mask → 0 seeds. */
void ngd_mask_extract_dyna_points(const uint8_t *gray, int w, int h, int stride,
                                  const uint8_t *mask, int cellSize, int threshold,
                                  ngd_pt2f *out, int *out_n);

/* Tracking::ClusterWithDBSCAN (Tracking.cc:4402-4483). Two phases:
 *   (a) depth pre-segmentation — sort by z, split on gap > 0.1 or window > 0.5,
 *       keep sub-segments with > 20 points (their z rewritten to 200*level);
 *       if nothing survives, fall back to all original points;
 *   (b) 3D DBSCAN with `eps` / `minPts` on the segmented points.
 * Writes `labels[n]` indexed by ORIGINAL point: >=1 cluster id, -1 noise or
 * dropped by pre-segmentation. *out_nclusters receives the largest cluster id
 * (0 if every point is noise). */
void ngd_mask_cluster_dbscan(const ngd_pt3f *pts, int n, float eps, int minPts,
                             int *labels, int *out_nclusters);

/* 4-connectivity connected components of a binary mask (standalone; also used
 * internally by CreateMaskFromClusters in place of cv::connectedComponentsWithStats).
 * Writes `labels[w*h]` (0 = background, 1..k = components). Returns the label
 * count (k+1, including background). `labels` is a caller buffer of w*h ints. */
int ngd_mask_connected_components(const uint8_t *mask, int w, int h, int *labels);

/* Tracking::CreateMaskFromClusters (Tracking.cc:4486-4587). For each label in
 * 1..nclusters: bounding box of its points; skip if width<50 or height<50;
 * expand by 15 (clamped); fill a local rectangle; zero pixels whose depth
 * differs from the cluster median by > 0.3; keep the largest connected
 * component and write 1 into out_mask. Finally dilate out_mask with half=3.
 * `out_mask` (w×h) must be pre-zeroed by the caller. Depth is read from the
 * `depth` buffer (dw×dh, row stride `dstride`, float metres). */
void ngd_mask_create_from_clusters(const ngd_pt3f *pts, const int *labels, int n, int nclusters,
                                   const float *depth, int dw, int dh, int dstride,
                                   uint8_t *out_mask, int w, int h);

/* Tracking::PredictCurrentMask orchestrator (Tracking.cc:4249-4305). Erodes a
 * scratch copy of `last_mask` (half=5), extracts dyna seeds, runs LK (callback)
 * from last_gray to curr_gray, filters by depth (NaN/bounds/depth>=0.05),
 * clusters (DBSCAN eps=50 minPts=15), and builds the mask. Writes `out_mask`
 * (w×h, caller-zeroed). `last_mask` is NOT mutated (unlike the original, which
 * eroded it in place — a no-op persistence-wise since the caller overwrites it).
 * If `lk` is NULL or extracts no seeds, out_mask stays all-zero. */
void ngd_mask_predict_current(const uint8_t *last_gray, const uint8_t *curr_gray,
                              const uint8_t *last_mask, const float *depth,
                              int w, int h, int stride,
                              ngd_lk_flow_fn lk, void *lk_user,
                              uint8_t *out_mask);

#ifdef __cplusplus
}
#endif
#endif /* NGD_MASK_H */
