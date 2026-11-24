/* ngd/sim3.c — Sim(3) Lie group (pure C), faithful to g2o::Sim3. */
#include "ngd/sim3.h"

#include <math.h>

ngd_sim3 ngd_sim3_identity(void) {
    ngd_sim3 S; S.q = ngd_quat_identity(); S.t = ngd_v3(0.f,0.f,0.f); S.s = 1.0f; return S;
}

ngd_sim3 ngd_sim3_from_rt(ngd_mat3 R, ngd_vec3 t, float s) {
    ngd_sim3 S; S.q = ngd_quat_from_matrix(R); S.t = t; S.s = s; return S;
}

ngd_sim3 ngd_sim3_from_matrix4(const float m[16]) {
    /* Reads a homogeneous [R | t; 0 1] with scale 1 (used where a 4x4 SE3 is
     * promoted to Sim3). 4x4 row-major: m[row*4+col]. */
    ngd_mat3 R = {{ m[0],m[1],m[2], m[4],m[5],m[6], m[8],m[9],m[10] }};
    ngd_vec3 t = ngd_v3(m[3], m[7], m[11]);
    return ngd_sim3_from_rt(R, t, 1.0f);
}

void ngd_sim3_to_matrix4(ngd_sim3 S, float m[16]) {
    /* [sR | t; 0 1], row-major. */
    ngd_mat3 R = ngd_quat_to_matrix(S.q);
    m[0]=S.s*R.m[0]; m[1]=S.s*R.m[1]; m[2]=S.s*R.m[2];  m[3]=S.t.x;
    m[4]=S.s*R.m[3]; m[5]=S.s*R.m[4]; m[6]=S.s*R.m[5];  m[7]=S.t.y;
    m[8]=S.s*R.m[6]; m[9]=S.s*R.m[7]; m[10]=S.s*R.m[8]; m[11]=S.t.z;
    m[12]=0.f;       m[13]=0.f;       m[14]=0.f;        m[15]=1.f;
}

ngd_sim3 ngd_sim3_multiply(ngd_sim3 a, ngd_sim3 b) {
    /* S_a(S_b(p)) = s_a*(R_a*(s_b*R_b*p + s_b*t_b... wait map_b(p)=s_b*(R_b*p)+t_b)
     * = s_a*(R_a*(s_b*(R_b*p)+t_b)) + t_a = s_a*s_b*(R_a*R_b*p) + s_a*(R_a*t_b) + t_a.
     * So { q_a*q_b, s_a*(R_a*t_b)+t_a, s_a*s_b }. */
    ngd_mat3 Ra = ngd_quat_to_matrix(a.q);
    ngd_sim3 r;
    r.t = ngd_v3_add(ngd_v3_scale(ngd_m3_transform(Ra, b.t), a.s), a.t);
    r.q = ngd_quat_normalize(ngd_quat_multiply(a.q, b.q));
    r.s = a.s * b.s;
    return r;
}

ngd_sim3 ngd_sim3_inverse(ngd_sim3 S) {
    /* g2o: { conj(q), conj(q)*((-1/s)*t), 1/s }. */
    ngd_sim3 r;
    r.q = ngd_quat_conjugate(S.q);
    r.s = 1.0f / S.s;
    ngd_mat3 Rt = ngd_quat_to_matrix(r.q);   /* R^T (since r.q = conj) */
    ngd_vec3 tinv = ngd_v3_scale(S.t, -1.0f / S.s);
    r.t = ngd_m3_transform(Rt, tinv);
    return r;
}

ngd_vec3 ngd_sim3_map(ngd_sim3 S, ngd_vec3 p) {
    ngd_mat3 R = ngd_quat_to_matrix(S.q);
    return ngd_v3_add(ngd_v3_scale(ngd_m3_transform(R, p), S.s), S.t);
}

ngd_sim3 ngd_sim3_exp(const float xi[7]) {
    /* Verbatim from g2o::Sim3::exp (sim3.h:70-142). xi = [omega(3)|upsilon(3)|sigma(1)]. */
    ngd_vec3 omega   = ngd_v3(xi[0], xi[1], xi[2]);
    ngd_vec3 upsilon = ngd_v3(xi[3], xi[4], xi[5]);
    float sigma = xi[6];
    float theta = ngd_v3_norm(omega);
    ngd_mat3 Omega  = ngd_so3_hat(omega);
    ngd_mat3 Omega2 = ngd_m3_multiply(Omega, Omega);
    float s = expf(sigma);

    const float eps = 1e-5f;
    float A, B, C;
    ngd_mat3 R;

    if (fabsf(sigma) < eps) {
        C = 1.0f;
        if (theta < eps) {
            A = 1.0f/2.0f;
            B = 1.0f/6.0f;
            R = ngd_m3_identity();
            R = ngd_m3_add(R, Omega);
            R = ngd_m3_add(R, Omega2);
        } else {
            float theta2 = theta*theta;
            A = (1.0f - cosf(theta)) / theta2;
            B = (theta - sinf(theta)) / (theta2*theta);
            R = ngd_m3_identity();
            R = ngd_m3_add_scaled(R, Omega, sinf(theta)/theta);
            R = ngd_m3_add_scaled(R, Omega2, (1.0f-cosf(theta))/(theta*theta));
        }
    } else {
        C = (s - 1.0f) / sigma;
        if (theta < eps) {
            float sigma2 = sigma*sigma;
            A = ((sigma - 1.0f)*s + 1.0f) / sigma2;
            B = ((0.5f*sigma2 - sigma + 1.0f)*s) / (sigma2*sigma);
            R = ngd_m3_identity();
            R = ngd_m3_add(R, Omega);
            R = ngd_m3_add(R, Omega2);
        } else {
            R = ngd_m3_identity();
            R = ngd_m3_add_scaled(R, Omega, sinf(theta)/theta);
            R = ngd_m3_add_scaled(R, Omega2, (1.0f-cosf(theta))/(theta*theta));
            float a = s*sinf(theta);
            float b = s*cosf(theta);
            float theta2 = theta*theta;
            float sigma2 = sigma*sigma;
            float c = theta2 + sigma2;
            A = (a*sigma + (1.0f - b)*theta) / (theta*c);
            B = (C - ((b - 1.0f)*sigma + a*theta)/c) * (1.0f/theta2);
        }
    }

    ngd_sim3 out;
    out.q = ngd_quat_from_matrix(R);
    out.s = s;
    /* W = A*Omega + B*Omega^2 + C*I ; t = W*upsilon */
    ngd_mat3 W = ngd_m3_identity();
    W = ngd_m3_add_scaled(W, Omega, A);
    W = ngd_m3_add_scaled(W, Omega2, B);
    W = ngd_m3_add_scaled(W, ngd_m3_identity(), C - 1.0f);   /* +C*I folded onto the identity base */
    out.t = ngd_m3_transform(W, upsilon);
    return out;
}

void ngd_sim3_log(ngd_sim3 S, float xi[7]) {
    /* Verbatim from g2o::Sim3::log (sim3.h:148-229). */
    float sigma = logf(S.s);
    ngd_mat3 R = ngd_quat_to_matrix(S.q);
    float d = 0.5f * (R.m[0] + R.m[4] + R.m[8] - 1.0f);   /* cos(theta) */
    if (d > 1.0f) d = 1.0f;
    if (d < -1.0f) d = -1.0f;

    /* deltaR(R) = [R21-R12, R02-R20, R10-R01]  (g2o se3_ops, same as SE3 log) */
    ngd_vec3 dR = ngd_v3(R.m[7] - R.m[5],
                         R.m[2] - R.m[6],
                         R.m[3] - R.m[1]);

    const float eps = 1e-5f;
    float A, B, C;
    ngd_vec3 omega;
    ngd_mat3 Omega;

    if (fabsf(sigma) < eps) {
        C = 1.0f;
        if (d > 1.0f - eps) {
            omega = ngd_v3_scale(dR, 0.5f);
            Omega = ngd_so3_hat(omega);
            A = 1.0f/2.0f;
            B = 1.0f/6.0f;
        } else {
            float theta = acosf(d);
            float theta2 = theta*theta;
            omega = ngd_v3_scale(dR, theta / (2.0f*sqrtf(1.0f - d*d)));
            Omega = ngd_so3_hat(omega);
            A = (1.0f - cosf(theta)) / theta2;
            B = (theta - sinf(theta)) / (theta2*theta);
        }
    } else {
        C = (S.s - 1.0f) / sigma;
        if (d > 1.0f - eps) {
            float sigma2 = sigma*sigma;
            omega = ngd_v3_scale(dR, 0.5f);
            Omega = ngd_so3_hat(omega);
            A = ((sigma - 1.0f)*S.s + 1.0f) / sigma2;
            B = ((0.5f*sigma2 - sigma + 1.0f)*S.s) / (sigma2*sigma);
        } else {
            float theta = acosf(d);
            omega = ngd_v3_scale(dR, theta / (2.0f*sqrtf(1.0f - d*d)));
            Omega = ngd_so3_hat(omega);
            float theta2 = theta*theta;
            float a = S.s*sinf(theta);
            float b = S.s*cosf(theta);
            float c = theta2 + sigma*sigma;
            A = (a*sigma + (1.0f - b)*theta) / (theta*c);
            B = (C - ((b - 1.0f)*sigma + a*theta)/c) * (1.0f/theta2);
        }
    }

    /* W = A*Omega + B*Omega*Omega + C*I ; upsilon = W^{-1} * t */
    ngd_mat3 O2 = ngd_m3_multiply(Omega, Omega);
    ngd_mat3 W = ngd_m3_identity();
    W = ngd_m3_add_scaled(W, Omega, A);
    W = ngd_m3_add_scaled(W, O2, B);
    W = ngd_m3_add_scaled(W, ngd_m3_identity(), C - 1.0f);

    /* Solve W * upsilon = t  (g2o uses LU solve). */
    float W3[9];
    for (int i = 0; i < 9; i++) W3[i] = W.m[i];
    float up[3] = {0.f, 0.f, 0.f};
    ngd_solve_linear(W3, (const float*)&S.t, up, 3);

    xi[0]=omega.x; xi[1]=omega.y; xi[2]=omega.z;
    xi[3]=up[0];   xi[4]=up[1];   xi[5]=up[2];
    xi[6]=sigma;
}

ngd_se3 ngd_sim3_to_se3(ngd_sim3 S) {
    /* [R | t/s]  (OptimizeEssentialGraph SE3 recovery). */
    ngd_se3 e;
    e.q = S.q;
    e.t = ngd_v3_scale(S.t, 1.0f / S.s);
    return e;
}
