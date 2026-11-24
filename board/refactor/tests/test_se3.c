/* test_se3.c — unit tests for ngd/so3.h + ngd/se3.h. */
#include "ngd/so3.h"
#include "ngd/se3.h"

#include <stdio.h>
#include <math.h>

static int fails = 0;
#define CHECK(cond, msg) do { if(!(cond)){ printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); fails++; } } while(0)
#define APPROX(a,b,eps) (fabsf((a)-(b)) <= (eps))

static float pose_dist(ngd_se3 a, ngd_se3 b) {
    /* translation + rotation-angle difference */
    ngd_mat3 Ra = ngd_quat_to_matrix(a.q), Rb = ngd_quat_to_matrix(b.q);
    ngd_mat3 dR = ngd_m3_multiply(ngd_m3_transpose(Ra), Rb);
    ngd_vec3 w = ngd_so3_log(dR);
    ngd_vec3 dt = ngd_v3_sub(a.t, b.t);
    return ngd_v3_norm(w) + ngd_v3_norm(dt);
}

int main(void)
{
    /* ---- so3 exp/log roundtrip ---- */
    ngd_vec3 w = ngd_v3(0.13f, -0.21f, 0.37f);
    ngd_mat3 R = ngd_quat_to_matrix(ngd_so3_exp(w));
    ngd_vec3 w2 = ngd_so3_log(R);
    CHECK(APPROX(w2.x,w.x,1e-4f)&&APPROX(w2.y,w.y,1e-4f)&&APPROX(w2.z,w.z,1e-4f), "so3 exp/log roundtrip");

    /* ---- se3 exp/log roundtrip ---- */
    ngd_se3 T = ngd_se3_identity();
    T.q = ngd_so3_exp(ngd_v3(0.2f,-0.1f,0.3f));
    T.t = ngd_v3(0.5f,-0.3f,0.8f);
    float xi[6];
    ngd_se3_log(T, xi);
    ngd_se3 T2 = ngd_se3_exp(xi);
    CHECK(pose_dist(T,T2) < 1e-4f, "se3 exp/log roundtrip");

    /* ---- inverse * self == identity ---- */
    ngd_se3 Tinv = ngd_se3_inverse(T);
    ngd_se3 I = ngd_se3_multiply(Tinv, T);
    CHECK(ngd_v3_norm(I.t) < 1e-5f, "se3 inverse translation");
    ngd_mat3 Rident = ngd_quat_to_matrix(I.q);
    CHECK(APPROX(Rident.m[0],1,1e-4f)&&APPROX(Rident.m[4],1,1e-4f)&&APPROX(Rident.m[8],1,1e-4f),
          "se3 inverse rotation");

    /* ---- map: Xc = R*Xw + t ---- */
    ngd_vec3 Xw = ngd_v3(1,2,3);
    ngd_vec3 Xc = ngd_se3_map(T, Xw);
    ngd_mat3 Rm = ngd_quat_to_matrix(T.q);
    ngd_vec3 expect = ngd_v3_add(ngd_m3_transform(Rm, Xw), T.t);
    CHECK(APPROX(Xc.x,expect.x,1e-5f)&&APPROX(Xc.y,expect.y,1e-5f)&&APPROX(Xc.z,expect.z,1e-5f),
          "se3 map");

    /* ---- left-perturbation is the optimizer's oplus: T_new = exp(xi)*T ----
     * Assert the property the optimizer actually relies on: for a perturbation
     * dx applied to a base pose Tb, map(exp(dx)*Tb, p) == map(exp(dx), map(Tb, p)).
     * (exp(xi)*T == exp(xi)*T is trivially true; the meaningful invariant is
     *  that left-composition with an exponential is a left group action.) */
    ngd_vec3 p = ngd_v3(1.0f, -0.5f, 2.0f);
    float dx[6] = {0.02f, -0.03f, 0.01f, 0.05f, -0.04f, 0.06f};
    ngd_se3 Tx = ngd_se3_exp(dx);
    ngd_vec3 lhs = ngd_se3_map(ngd_se3_multiply(Tx, T), p);
    ngd_vec3 rhs = ngd_se3_map(Tx, ngd_se3_map(T, p));
    CHECK(APPROX(lhs.x,rhs.x,1e-5f) && APPROX(lhs.y,rhs.y,1e-5f) && APPROX(lhs.z,rhs.z,1e-5f),
          "se3 left-action map(exp*dx*T,p)==map(exp*dx,map(T,p))");

    if (fails == 0) { printf("test_se3: PASS\n"); return 0; }
    printf("test_se3: %d FAILURES\n", fails);
    return 1;
}
