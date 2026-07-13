/* ngd/so3.c — SO(3) Lie-group primitives (pure C). */
#include "ngd/so3.h"

#include <math.h>

ngd_mat3 ngd_so3_hat(ngd_vec3 v) {
    ngd_mat3 m;
    m.m[0]=0.f;    m.m[1]=-v.z; m.m[2]= v.y;
    m.m[3]= v.z;   m.m[4]=0.f;  m.m[5]=-v.x;
    m.m[6]=-v.y;   m.m[7]= v.x; m.m[8]=0.f;
    return m;
}

ngd_quat ngd_so3_exp(ngd_vec3 omega) {
    /* Rodrigues into a unit quaternion. For ||omega||->0 returns identity. */
    float theta2 = ngd_v3_dot(omega, omega);
    if (theta2 < 1e-20f) return ngd_quat_identity();
    float theta = sqrtf(theta2);
    float h = 0.5f * theta;
    float s = sinf(h) / theta;
    ngd_quat q = { cosf(h), omega.x*s, omega.y*s, omega.z*s };
    return ngd_quat_normalize(q);
}

ngd_vec3 ngd_so3_log(ngd_mat3 R) {
    /* trace = 1 + 2*cos(theta)  =>  cos(theta) = (trace-1)/2 */
    float cos_theta = 0.5f * (R.m[0] + R.m[4] + R.m[8] - 1.0f);
    if (cos_theta > 1.0f)  cos_theta = 1.0f;
    if (cos_theta < -1.0f) cos_theta = -1.0f;
    float theta = acosf(cos_theta);
    if (theta < 1e-12f) return ngd_v3(0.f,0.f,0.f);
    /* axis = (1/(2 sin theta)) * [R32-R23 ; R13-R31 ; R21-R12] */
    float s = 1.0f / (2.0f * sinf(theta));
    ngd_vec3 axis = ngd_v3(s*(R.m[7]-R.m[5]),
                           s*(R.m[2]-R.m[6]),
                           s*(R.m[3]-R.m[1]));
    return ngd_v3_scale(axis, theta);
}
