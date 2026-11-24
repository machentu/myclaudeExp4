#ifndef NGD_PINHOLE_H
#define NGD_PINHOLE_H

/*
 * ngd/pinhole.h — pinhole camera model for the pure-C core.
 *
 * Replaces ORB_SLAM3::Pinhole::project / projectJac for the PoseOptimization
 * path. Parameters vector is [fx, fy, cx, cy] exactly like the original
 * (see include/CameraModels/Pinhole.h:93 and src/CameraModels/Pinhole.cpp:30-81).
 *
 * Stereo extends the mono projection with a right-pixel coordinate
 *   uR = fx*X/Z + cx - bf/Z          (bf = baseline*fx, g2o EdgeStereo...)
 * consistent with g2o's EdgeStereoSE3ProjectXYZOnlyPose::cam_projection.
 */

#include "ngd/math.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct { float fx, fy, cx, cy; } ngd_pinhole;

ngd_pinhole ngd_pinhole_make(float fx, float fy, float cx, float cy);

/* Mono projection: (fx*X/Z + cx, fy*Y/Z + cy). out = [u, v]. */
void ngd_pinhole_project(ngd_pinhole c, ngd_vec3 Xc, float out[2]);

/* Mono projection Jacobian (2x3 row-major), = Pinhole::projectJac. */
void ngd_pinhole_project_jac(ngd_pinhole c, ngd_vec3 Xc, float J[6]);

/* Stereo projection: (u, v, uR = u - bf/Z). out = [u, v, uR]. */
void ngd_pinhole_project_stereo(ngd_pinhole c, float bf, ngd_vec3 Xc, float out[3]);

/* Stereo projection Jacobian (3x3 row-major). */
void ngd_pinhole_project_jac_stereo(ngd_pinhole c, float bf, ngd_vec3 Xc, float J[9]);

#ifdef __cplusplus
}
#endif
#endif /* NGD_PINHOLE_H */
