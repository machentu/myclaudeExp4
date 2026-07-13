#ifndef NGD_CALIB_H
#define NGD_CALIB_H

/*
 * ngd/calib.h — camera calibration + ORB scale-pyramid context (pure C).
 *
 * Holds the per-camera intrinsics (replacing ORB-SLAM3 Frame's static fx/fy/...
 * and the mbf/mb/thDepth members) and the ORB scale pyramid copied from the
 * extractor (Frame.cc:233-239 copies mvScaleFactor/mvLevelSigma2/etc into the
 * Frame). Shared by Frame, KeyFrame, and MapPoint so they don't duplicate it.
 *
 * For the validated TUM3 run: fx=535.4 fy=539.2 cx=320.1 cy=247.6,
 * b=0.0747 -> mbf~=40.0, thDepth=2.988, image 640x480, zero distortion.
 */

#include "ngd/pinhole.h"
#include "ngd/orb.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ngd_cam_ctx {
    ngd_pinhole cam;     /* fx,fy,cx,cy */
    float mbf;            /* baseline * fx (px*m) */
    float mb;             /* baseline (m) */
    float thDepth;        /* near/far threshold (m) */
    int   imgW, imgH;     /* image bounds (mnMaxX/mnMaxY; mnMinX/mnMinY = 0 for zero distortion) */

    int   nlevels;
    float mvScaleFactor[NGD_ORB_MAX_LEVELS];
    float mvInvScaleFactor[NGD_ORB_MAX_LEVELS];
    float mvLevelSigma2[NGD_ORB_MAX_LEVELS];
    float mvInvLevelSigma2[NGD_ORB_MAX_LEVELS];
    float mfLogScaleFactor;   /* log(scaleFactor); MapPoint::PredictScale uses level1 (Frame::mfLogScaleFactor) */
} ngd_cam_ctx;

/* Build a camera context from an initialised extractor (copies the scale
 * pyramid) plus intrinsics. imgW/imgH are the level-0 image dimensions. */
void ngd_cam_ctx_init(ngd_cam_ctx *c, const ngd_orb_extractor *ex,
                      float fx, float fy, float cx, float cy,
                      float baseline, float thDepth, int imgW, int imgH);

#ifdef __cplusplus
}
#endif
#endif /* NGD_CALIB_H */
