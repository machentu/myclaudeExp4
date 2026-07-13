#ifndef NGD_FRAME_H
#define NGD_FRAME_H

/*
 * ngd/frame.h — Frame (pure C), RGB-D construction.
 *
 * Faithful port of the RGBD Frame constructor path (src/Frame.cc:222-309):
 *   ExtractORB → UndistortKeyPoints (TUM3 zero distortion → no-op) →
 *   ComputeStereoFromRGBD → mvpMapPoints/mvbOutlier → AssignFeaturesToGrid.
 *
 * The NGD dynamic-mask Frame variant (Frame.cc:312-419) and the
 * optical-flow no-ORB Frame(bool) are deferred. The grid is heap-allocated
 * (the documented Windows fix: Frame.h:262-266).
 */

#include "ngd/math.h"
#include "ngd/se3.h"
#include "ngd/calib.h"
#include "ngd/orb.h"
#include "ngd/keyframe.h"   /* ngd_grid_cell, grid constants */
#include "ngd/bow.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ngd_mappoint;

typedef struct ngd_frame {
    uint64_t mnId;
    ngd_se3  pose;
    int  N;
    ngd_keypoint *keys;        /* mvKeysUn (== mvKeys for zero distortion) */
    uint8_t *descriptors;     /* N*32 */
    float *uRight;             /* mvuRight (-1 none) */
    float *depth;              /* mvDepth  (-1 none) */
    struct ngd_mappoint **mvpMapPoints;  /* N */
    int    *mvbOutlier;        /* N (0/1) */

    ngd_cam_ctx cam;
    ngd_grid_cell *grid;       /* 64x48, heap */
    float gridInvW, gridInvH;

    /* BoW cache (Frame::ComputeBoW, Frame.cc:880-887). NULL until computed. */
    ngd_bowvec *bow;
    ngd_featvec *feat;
} ngd_frame;

/*
 * Build an RGBD frame: run ORB extraction on `gray`, compute stereo-from-depth
 * from `depth_meters` (CV_32F metres, already scaled by DepthMapFactor), and
 * bin features into the grid. Pose is set to identity (StereoInitialization
 * may keep it; the tracker overwrites it). Returns N, or -1 on failure.
 *   keybuf/descbuf are caller scratch with capacity max_kps / max_kps*32.
 */
int ngd_frame_build_rgbd(ngd_frame *f, const ngd_orb_extractor *ex, const ngd_cam_ctx *cam,
                         const uint8_t *gray, int w, int h, int stride,
                         const float *depth_meters, int dstride,
                         uint64_t id,
                         ngd_keypoint *keybuf, int max_kps, uint8_t *descbuf);

/* NGD dynamic-mask Frame variant (Frame.cc:312-419): same as build_rgbd but
 * drops keypoints whose mask pixel != 0 (0 = static/keep, !=0 = dynamic/drop).
 * `mask` is uint8 w×h, stride == w; NULL = keep all (== build_rgbd). */
int ngd_frame_build_rgbd_mask(ngd_frame *f, const ngd_orb_extractor *ex, const ngd_cam_ctx *cam,
                              const uint8_t *gray, int w, int h, int stride,
                              const float *depth_meters, int dstride,
                              uint64_t id, const uint8_t *mask,
                              ngd_keypoint *keybuf, int max_kps, uint8_t *descbuf);
void ngd_frame_free(ngd_frame *f);

void ngd_frame_set_pose(ngd_frame *f, ngd_se3 pose);
ngd_se3 ngd_frame_get_pose(const ngd_frame *f);
ngd_vec3 ngd_frame_get_camera_center(const ngd_frame *f);   /* Ow = -R^T t */

/* Back-project keypoint i using its depth → world point (applies frame pose:
 * Xw = Twc * Xc; identity pose ⇒ Xw = Xc). Returns z in .z; z<=0 if no depth. */
ngd_vec3 ngd_frame_unproject_stereo(const ngd_frame *f, int i);

void ngd_frame_assign_grid(ngd_frame *f);
int  ngd_frame_get_features_in_area(const ngd_frame *f, float x, float y, float r,
                                    int minLevel, int maxLevel, int *out, int max_out);

/* Frustum test (Frame::isInFrustum, Frame.cc:651-716, Nleft==-1 mono/RGBD path).
 * Projects mp into the frame, checks z>0 / image bounds / scale-invariant
 * distance band / viewing-cosine limit, and on success fills mp's track-projection
 * cache (mTrackProjX/Y/Z, mTrackDepth, mTrackProjXR, mnTrackScaleLevel,
 * mTrackViewCos, mbTrackInView=1) for the SearchByProjection overload #1.
 * Does NOT touch mnVisible (IncreaseVisible is the caller's job, SearchLocalPoints).
 * Returns 1 if visible, 0 otherwise. */
int ngd_frame_is_in_frustum(ngd_frame *f, struct ngd_mappoint *mp, float viewingCosLimit);

/* Compute (lazily) the BoW vector + feature vector from this frame's
 * descriptors (Frame::ComputeBoW, levelsup=4). No-op if already computed. */
void ngd_frame_compute_bow(ngd_frame *f, const ngd_bow_vocab *vocab);

#ifdef __cplusplus
}
#endif
#endif /* NGD_FRAME_H */
