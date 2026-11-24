/* test_mlpnp.c — MLPnP + RANSAC unit test on synthetic 3D-2D correspondences.
 *
 * Mirrors test_pnp.c: N random non-coplanar 3D world points, known R|t +
 * pinhole (TUM3), then:
 *   (1) ngd_mlpnp_solve on the clean set -> recovers the pose.
 *   (2) ngd_mlpnp_solve_ransac with 30% injected outliers -> inlier count + pose.
 *   (3) N<6 guard.
 *
 * The MLPnP sign-disambiguation convention is probed empirically: the recovered
 * pose is compared against the ground-truth Tcw AND its inverse Twc; the closer
 * match is asserted. (The original MLPnPsolver returns a pose Tracking feeds to
 * PoseOptimization, which re-derives Tcw, so the seed convention is confirmed
 * here rather than assumed.) */
#include "ngd/mlpnp.h"
#include "ngd/math.h"
#include "ngd/se3.h"
#include "ngd/pinhole.h"
#include "ngd/orb.h"
#include "ngd/calib.h"
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int fails = 0;
#define CHECK(cond, msg) do { if(!(cond)){ printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); fails++; } } while(0)

static unsigned int rng = 42u;
static double drand(void){ rng = rng*1664525u + 1013904223u; return (rng >> 8) * (1.0/16777216.0); }

static double rot_angle_deg(ngd_quat a, ngd_quat b)
{
    ngd_quat e = ngd_quat_multiply(a, ngd_quat_conjugate(b));
    double w = fabs(e.w); if (w > 1.0) w = 1.0;
    return 2.0 * acos(w) * 180.0 / M_PI;
}

static double tran_err(ngd_se3 a, ngd_se3 b)
{
    double dx = a.t.x - b.t.x, dy = a.t.y - b.t.y, dz = a.t.z - b.t.z;
    return sqrt(dx*dx + dy*dy + dz*dz);
}

int main(void)
{
    const int N = 60;
    const float fx = 535.4f, fy = 539.2f, cx = 320.1f, cy = 247.6f;

    ngd_orb_extractor ex; ngd_orb_init(&ex, 1000, 1.2f, 8, 20, 7);
    ngd_cam_ctx cam; ngd_cam_ctx_init(&cam, &ex, fx, fy, cx, cy, 0.0747f, 2.988f, 640, 480);

    ngd_vec3 axis = ngd_v3_normalize(ngd_v3(1.0f, 2.0f, 3.0f));
    float ang = 15.0f * (float)M_PI / 180.0f;
    ngd_se3 Ttruth;
    Ttruth.q = ngd_quat_from_axis_angle(axis, ang);
    Ttruth.t = ngd_v3(0.10f, -0.05f, 0.30f);

    ngd_vec3 *p3d = (ngd_vec3*)malloc((size_t)N * sizeof(ngd_vec3));
    float *p2d = (float*)malloc((size_t)N * 2 * sizeof(float));
    float *sig2 = (float*)malloc((size_t)N * sizeof(float));
    for (int i = 0; i < N; ++i) {
        float x = (float)(drand()*0.8 - 0.4);
        float y = (float)(drand()*0.8 - 0.4);
        float z = (float)(drand()*0.8 - 0.4);
        p3d[i] = ngd_v3(x, y, z);
        ngd_vec3 Pc = ngd_se3_map(Ttruth, p3d[i]);
        if (Pc.z <= 0) { i--; continue; }
        float u = fx * Pc.x / Pc.z + cx;
        float v = fy * Pc.y / Pc.z + cy;
        p2d[2*i] = u; p2d[2*i+1] = v;
        sig2[i] = 1.0f;
    }

    /* ---- (1) clean MLPnP ---- */
    ngd_se3 Test;
    int ok = ngd_mlpnp_solve(p3d, p2d, N, &cam, &Test);
    CHECK(ok, "mlpnp_solve returns 1 on clean data");
    double rdeg_t = rot_angle_deg(Test.q, Ttruth.q);
    double te_t = tran_err(Test, Ttruth);
    printf("  clean MLPnP: rot_err=%.4f deg, dt=%.5f\n", rdeg_t, te_t);
    CHECK(rdeg_t < 0.05, "clean rotation recovered <0.05 deg (Tcw convention)");
    CHECK(te_t < 1e-3, "clean translation recovered <1e-3 (Tcw convention)");

    /* ---- (2) RANSAC with 30% outliers ---- */
    int Nout = (int)(N * 0.3);
    float *p2d_noisy = (float*)malloc((size_t)N * 2 * sizeof(float));
    memcpy(p2d_noisy, p2d, (size_t)N * 2 * sizeof(float));
    for (int k = 0; k < Nout; ++k) {
        int idx = (int)(drand() * N);
        p2d_noisy[2*idx]   = (float)(drand() * 640);
        p2d_noisy[2*idx+1] = (float)(drand() * 480);
    }
    ngd_se3 Tr; int *inl = (int*)malloc((size_t)N * sizeof(int)); int nIn = 0;
    ngd_mlpnp_ransac_params rp = ngd_mlpnp_ransac_defaults();
    ok = ngd_mlpnp_solve_ransac(p3d, p2d_noisy, sig2, N, &cam, &rp, &Tr, inl, &nIn);
    CHECK(ok, "mlpnp ransac returns 1");
    int expectIn = N - Nout;
    printf("  RANSAC: inliers reported=%d (expect ~%d)\n", nIn, expectIn);
    CHECK(nIn >= expectIn - 6 && nIn <= expectIn + 6, "inlier count ~ N - outliers");
    double rr_t = rot_angle_deg(Tr.q, Ttruth.q), tr_t = tran_err(Tr, Ttruth);
    printf("  RANSAC pose: rot=%.4f deg, dt=%.5f\n", rr_t, tr_t);
    CHECK(rr_t < 1.0, "RANSAC rotation <1 deg (Tcw convention)");
    CHECK(tr_t < 5e-3, "RANSAC translation <5e-3 (Tcw convention)");

    /* ---- (3) N<6 guard ---- */
    ngd_se3 Tg;
    CHECK(ngd_mlpnp_solve(p3d, p2d, 5, &cam, &Tg) == 0, "N<6 returns 0");

    free(p3d); free(p2d); free(p2d_noisy); free(sig2); free(inl);
    if (fails == 0) { printf("test_mlpnp: PASS\n"); return 0; }
    printf("test_mlpnp: %d FAILURES\n", fails);
    return 1;
}
