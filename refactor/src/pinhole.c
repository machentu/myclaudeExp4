/* ngd/pinhole.c — pinhole camera model (pure C), matches ORB_SLAM3::Pinhole. */
#include "ngd/pinhole.h"

#include <math.h>

ngd_pinhole ngd_pinhole_make(float fx, float fy, float cx, float cy) {
    ngd_pinhole c; c.fx=fx; c.fy=fy; c.cx=cx; c.cy=cy; return c;
}

void ngd_pinhole_project(ngd_pinhole c, ngd_vec3 Xc, float out[2]) {
    float invz = 1.0f / Xc.z;
    out[0] = c.fx * Xc.x * invz + c.cx;
    out[1] = c.fy * Xc.y * invz + c.cy;
}

void ngd_pinhole_project_jac(ngd_pinhole c, ngd_vec3 Xc, float J[6]) {
    /* row-major 2x3:  [[fx/Z, 0, -fx*X/Z²], [0, fy/Z, -fy*Y/Z²]] */
    float invz  = 1.0f / Xc.z;
    float invz2 = invz * invz;
    J[0] = c.fx * invz;       J[1] = 0.f;                J[2] = -c.fx * Xc.x * invz2;
    J[3] = 0.f;               J[4] = c.fy * invz;        J[5] = -c.fy * Xc.y * invz2;
}

void ngd_pinhole_project_stereo(ngd_pinhole c, float bf, ngd_vec3 Xc, float out[3]) {
    float invz = 1.0f / Xc.z;
    float u = c.fx * Xc.x * invz + c.cx;
    out[0] = u;
    out[1] = c.fy * Xc.y * invz + c.cy;
    out[2] = u - bf * invz;     /* uR */
}

void ngd_pinhole_project_jac_stereo(ngd_pinhole c, float bf, ngd_vec3 Xc, float J[9]) {
    /* row-major 3x3:
     *   [[fx/Z, 0, -fx*X/Z²],
     *    [0, fy/Z, -fy*Y/Z²],
     *    [fx/Z, 0, -fx*X/Z² + bf/Z²]]   <- d(uR)/dXc */
    float invz  = 1.0f / Xc.z;
    float invz2 = invz * invz;
    J[0] = c.fx * invz;       J[1] = 0.f;                J[2] = -c.fx * Xc.x * invz2;
    J[3] = 0.f;               J[4] = c.fy * invz;        J[5] = -c.fy * Xc.y * invz2;
    J[6] = c.fx * invz;       J[7] = 0.f;                J[8] = -c.fx * Xc.x * invz2 + bf * invz2;
}
