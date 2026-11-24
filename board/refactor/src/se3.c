/* ngd/se3.c — SE(3) Lie group (pure C), faithful to g2o::SE3Quat. */
#include "ngd/se3.h"

#include <math.h>

ngd_se3 ngd_se3_identity(void) {
    ngd_se3 s; s.q = ngd_quat_identity(); s.t = ngd_v3(0.f,0.f,0.f); return s;
}

ngd_se3 ngd_se3_from_rt(ngd_mat3 R, ngd_vec3 t) {
    ngd_se3 s; s.q = ngd_quat_from_matrix(R); s.t = t; return s;
}

ngd_se3 ngd_se3_from_matrix4(const float m[16]) {
    ngd_mat3 R = {{ m[0],m[1],m[2], m[4],m[5],m[6], m[8],m[9],m[10] }};
    ngd_vec3 t = ngd_v3(m[3], m[7], m[11]);
    return ngd_se3_from_rt(R, t);
}

void ngd_se3_to_matrix4(ngd_se3 s, float m[16]) {
    ngd_mat3 R = ngd_quat_to_matrix(s.q);
    m[0]=R.m[0]; m[1]=R.m[1]; m[2]=R.m[2];  m[3]=s.t.x;
    m[4]=R.m[3]; m[5]=R.m[4]; m[6]=R.m[5];  m[7]=s.t.y;
    m[8]=R.m[6]; m[9]=R.m[7]; m[10]=R.m[8]; m[11]=s.t.z;
    m[12]=0.f;   m[13]=0.f;   m[14]=0.f;    m[15]=1.f;
}

ngd_se3 ngd_se3_multiply(ngd_se3 a, ngd_se3 b) {
    /* g2o: t = tA + R_A*tB ; q = qA*qB (normalised). */
    ngd_mat3 Ra = ngd_quat_to_matrix(a.q);
    ngd_se3 r;
    r.t = ngd_v3_add(a.t, ngd_m3_transform(Ra, b.t));
    r.q = ngd_quat_normalize(ngd_quat_multiply(a.q, b.q));
    return r;
}

ngd_se3 ngd_se3_inverse(ngd_se3 s) {
    /* g2o: q' = conj(q); t' = R' * (-t) = -R^T t */
    ngd_se3 r;
    r.q = ngd_quat_conjugate(s.q);
    ngd_mat3 Rt = ngd_quat_to_matrix(r.q);   /* = R^T since r.q = conj */
    r.t = ngd_v3_scale(ngd_m3_transform(Rt, s.t), -1.0f);
    return r;
}

ngd_vec3 ngd_se3_map(ngd_se3 s, ngd_vec3 p) {
    ngd_mat3 R = ngd_quat_to_matrix(s.q);
    return ngd_v3_add(ngd_m3_transform(R, p), s.t);
}

ngd_se3 ngd_se3_exp(const float xi[6]) {
    ngd_vec3 omega   = ngd_v3(xi[0], xi[1], xi[2]);
    ngd_vec3 upsilon = ngd_v3(xi[3], xi[4], xi[5]);
    float theta = ngd_v3_norm(omega);
    ngd_mat3 Omega = ngd_so3_hat(omega);
    ngd_mat3 Omega2 = ngd_m3_multiply(Omega, Omega);

    ngd_mat3 R, V;
    if (theta < 1e-5f) {
        /* g2o small-angle branch: R = I + Ω + Ω²,  V = R. */
        R = ngd_m3_identity();
        R = ngd_m3_add(R, Omega);
        R = ngd_m3_add(R, Omega2);
        V = R;
    } else {
        float s = sinf(theta) / theta;
        float c = (1.0f - cosf(theta)) / (theta * theta);
        float v3 = (theta - sinf(theta)) / (theta * theta * theta);
        R = ngd_m3_identity();
        R = ngd_m3_add_scaled(R, Omega, s);
        R = ngd_m3_add_scaled(R, Omega2, c);
        V = ngd_m3_identity();
        V = ngd_m3_add_scaled(V, Omega, c);
        V = ngd_m3_add_scaled(V, Omega2, v3);
    }
    ngd_se3 out;
    out.q = ngd_quat_from_matrix(R);
    out.t = ngd_m3_transform(V, upsilon);
    return out;
}

void ngd_se3_log(ngd_se3 s, float xi[6]) {
    ngd_mat3 R = ngd_quat_to_matrix(s.q);
    float d = 0.5f * (R.m[0] + R.m[4] + R.m[8] - 1.0f);   /* cos(theta) */
    if (d > 1.0f) d = 1.0f;
    if (d < -1.0f) d = -1.0f;

    /* deltaR(R) = [R21-R12, R02-R20, R10-R01]  (g2o se3_ops) */
    ngd_vec3 dR = ngd_v3(R.m[7] - R.m[5],
                         R.m[2] - R.m[6],
                         R.m[3] - R.m[1]);
    ngd_vec3 omega;
    ngd_mat3 Omega, Vinv;
    if (d > 0.99999f) {
        omega = ngd_v3_scale(dR, 0.5f);
        Omega = ngd_so3_hat(omega);
        Vinv = ngd_m3_identity();
        Vinv = ngd_m3_add_scaled(Vinv, Omega, -0.5f);
        ngd_mat3 O2 = ngd_m3_multiply(Omega, Omega);
        Vinv = ngd_m3_add_scaled(Vinv, O2, 1.0f/12.0f);
    } else {
        float theta = acosf(d);
        float inv = theta / (2.0f * sqrtf(1.0f - d*d));
        omega = ngd_v3_scale(dR, inv);
        Omega = ngd_so3_hat(omega);
        ngd_mat3 O2 = ngd_m3_multiply(Omega, Omega);
        float k = (1.0f - theta / (2.0f * tanf(theta/2.0f))) / (theta * theta);
        Vinv = ngd_m3_identity();
        Vinv = ngd_m3_add_scaled(Vinv, Omega, -0.5f);
        Vinv = ngd_m3_add_scaled(Vinv, O2, k);
    }
    ngd_vec3 upsilon = ngd_m3_transform(Vinv, s.t);
    xi[0]=omega.x; xi[1]=omega.y; xi[2]=omega.z;
    xi[3]=upsilon.x; xi[4]=upsilon.y; xi[5]=upsilon.z;
}

void ngd_se3_deriv(ngd_vec3 Xc, float out[18]) {
    /* [ -skew(Xc) | I_3 ], 3x6 row-major. */
    /* -skew(Xc) = [[0, z, -y],[-z, 0, x],[y, -x, 0]]  (matches OptimizableTypes) */
    out[0]=0.f;    out[1]=Xc.z;  out[2]=-Xc.y;  out[3]=1.f; out[4]=0.f; out[5]=0.f;
    out[6]=-Xc.z;  out[7]=0.f;   out[8]=Xc.x;   out[9]=0.f; out[10]=1.f;out[11]=0.f;
    out[12]=Xc.y;  out[13]=-Xc.x;out[14]=0.f;   out[15]=0.f;out[16]=0.f;out[17]=1.f;
}
