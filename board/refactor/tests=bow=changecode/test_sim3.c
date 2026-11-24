/* test_sim3.c — unit tests for ngd/sim3.h (g2o::Sim3 faithfulness). */
#include "ngd/so3.h"
#include "ngd/se3.h"
#include "ngd/sim3.h"

#include <stdio.h>
#include <math.h>

static int fails = 0;
#define CHECK(cond, msg) do { if(!(cond)){ printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); fails++; } } while(0)
#define APPROX(a,b,eps) (fabsf((a)-(b)) <= (eps))

static float sim3_dist(ngd_sim3 a, ngd_sim3 b) {
    ngd_mat3 Ra = ngd_quat_to_matrix(a.q), Rb = ngd_quat_to_matrix(b.q);
    ngd_mat3 dR = ngd_m3_multiply(ngd_m3_transpose(Ra), Rb);
    ngd_vec3 w = ngd_so3_log(dR);
    ngd_vec3 dt = ngd_v3_sub(a.t, b.t);
    return ngd_v3_norm(w) + ngd_v3_norm(dt) + fabsf(a.s - b.s);
}

int main(void)
{
    /* ---- map: s*(R*p)+t ---- */
    ngd_sim3 S = ngd_sim3_identity();
    S.q = ngd_so3_exp(ngd_v3(0.2f, -0.1f, 0.3f));
    S.t = ngd_v3(0.5f, -0.3f, 0.8f);
    S.s = 1.3f;
    ngd_vec3 p = ngd_v3(1.0f, 2.0f, -1.0f);
    ngd_vec3 mp = ngd_sim3_map(S, p);
    ngd_mat3 R = ngd_quat_to_matrix(S.q);
    ngd_vec3 expect = ngd_v3_add(ngd_v3_scale(ngd_m3_transform(R, p), S.s), S.t);
    CHECK(APPROX(mp.x,expect.x,1e-5f)&&APPROX(mp.y,expect.y,1e-5f)&&APPROX(mp.z,expect.z,1e-5f),
          "sim3 map = s*(R*p)+t");

    /* ---- inverse * self == identity ---- */
    ngd_sim3 Sinv = ngd_sim3_inverse(S);
    ngd_sim3 I = ngd_sim3_multiply(Sinv, S);
    CHECK(fabsf(I.s - 1.0f) < 1e-4f, "sim3 inverse*scale=1");
    CHECK(ngd_v3_norm(I.t) < 1e-4f, "sim3 inverse translation");
    ngd_mat3 Rident = ngd_quat_to_matrix(I.q);
    CHECK(APPROX(Rident.m[0],1,1e-4f)&&APPROX(Rident.m[4],1,1e-4f)&&APPROX(Rident.m[8],1,1e-4f),
          "sim3 inverse rotation");

    /* ---- inverse(map(p)) == p ---- */
    ngd_vec3 pback = ngd_sim3_map(Sinv, ngd_sim3_map(S, p));
    CHECK(APPROX(pback.x,p.x,1e-4f)&&APPROX(pback.y,p.y,1e-4f)&&APPROX(pback.z,p.z,1e-4f),
          "sim3 inverse(map(p))==p");

    /* ---- exp/log roundtrip (general, nonzero sigma & theta) ---- */
    float xi[7] = {0.15f, -0.22f, 0.31f, 0.4f, -0.25f, 0.6f, 0.262f};  /* sigma=ln(1.3) */
    ngd_sim3 Se = ngd_sim3_exp(xi);
    float xo[7];
    ngd_sim3_log(Se, xo);
    ngd_sim3 Se2 = ngd_sim3_exp(xo);
    CHECK(sim3_dist(Se, Se2) < 1e-4f, "sim3 exp/log roundtrip (general)");

    /* ---- exp/log roundtrip: theta~0 (omega small), sigma nonzero ---- */
    float xi2[7] = {1e-4f, -1e-4f, 2e-4f, 0.3f, -0.2f, 0.1f, 0.182f};
    ngd_sim3 S2 = ngd_sim3_exp(xi2);
    float xo2[7]; ngd_sim3_log(S2, xo2);
    ngd_sim3 S2b = ngd_sim3_exp(xo2);
    CHECK(sim3_dist(S2, S2b) < 1e-4f, "sim3 exp/log roundtrip (theta~0)");

    /* ---- exp/log roundtrip: sigma~0 (s~1), theta nonzero ---- */
    float xi3[7] = {0.2f, -0.15f, 0.25f, 0.4f, -0.25f, 0.6f, 1e-4f};
    ngd_sim3 S3 = ngd_sim3_exp(xi3);
    float xo3[7]; ngd_sim3_log(S3, xo3);
    ngd_sim3 S3b = ngd_sim3_exp(xo3);
    CHECK(sim3_dist(S3, S3b) < 1e-4f, "sim3 exp/log roundtrip (sigma~0)");

    /* ---- exp/log roundtrip: both ~0 ---- */
    float xi4[7] = {1e-4f, -1e-4f, 1e-4f, 1e-4f, -1e-4f, 1e-4f, 1e-4f};
    ngd_sim3 S4 = ngd_sim3_exp(xi4);
    float xo4[7]; ngd_sim3_log(S4, xo4);
    ngd_sim3 S4b = ngd_sim3_exp(xo4);
    CHECK(sim3_dist(S4, S4b) < 1e-4f, "sim3 exp/log roundtrip (both~0)");

    /* ---- s=1 reduces to SE3: to_se3 matches SE3 map ---- */
    ngd_sim3 Ss1 = ngd_sim3_identity();
    Ss1.q = ngd_so3_exp(ngd_v3(0.1f, 0.2f, -0.15f));
    Ss1.t = ngd_v3(0.3f, -0.4f, 0.5f);
    Ss1.s = 1.0f;
    ngd_se3 E = ngd_sim3_to_se3(Ss1);
    ngd_vec3 m1 = ngd_sim3_map(Ss1, p);
    ngd_vec3 m2 = ngd_se3_map(E, p);
    CHECK(APPROX(m1.x,m2.x,1e-5f)&&APPROX(m1.y,m2.y,1e-5f)&&APPROX(m1.z,m2.z,1e-5f),
          "sim3 s=1 matches SE3 map");

    /* ---- to_se3: t/s recovery ---- */
    ngd_se3 E2 = ngd_sim3_to_se3(S);
    CHECK(APPROX(E2.t.x, S.t.x/S.s, 1e-5f) &&
          APPROX(E2.t.y, S.t.y/S.s, 1e-5f) &&
          APPROX(E2.t.z, S.t.z/S.s, 1e-5f), "sim3 to_se3 t/s");

    /* ---- compose associativity: (A*B)*C == A*(B*C) ---- */
    ngd_sim3 A = ngd_sim3_exp((float[7]){0.1f,0.05f,-0.08f, 0.2f,-0.1f,0.3f, 0.1f});
    ngd_sim3 B = ngd_sim3_exp((float[7]){-0.12f,0.2f,0.05f, -0.15f,0.25f,0.1f, 0.15f});
    ngd_sim3 C = ngd_sim3_exp((float[7]){0.07f,-0.13f,0.2f, 0.3f,0.1f,-0.2f, -0.1f});
    ngd_sim3 l = ngd_sim3_multiply(ngd_sim3_multiply(A,B), C);
    ngd_sim3 r = ngd_sim3_multiply(A, ngd_sim3_multiply(B,C));
    CHECK(sim3_dist(l,r) < 1e-4f, "sim3 compose associativity");

    if (fails == 0) { printf("test_sim3: PASS\n"); return 0; }
    printf("test_sim3: %d FAILURES\n", fails);
    return 1;
}
