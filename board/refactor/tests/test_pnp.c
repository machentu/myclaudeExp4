/* test_pnp.c — EPnP + RANSAC unit test on synthetic 3D-2D correspondences.
 *
 * Generates N random non-coplanar 3D world points, applies a known R|t +
 * pinhole intrinsics (TUM3) to get 2D pixels, then:
 *   (1) ngd_epnp_solve on the clean set -> recovers the pose to ~1e-4.
 *   (2) ngd_pnp_solve_ransac with 30% injected outliers -> correct inlier
 *       count and pose still near the truth. */
#include "ngd/pnp.h"
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

/* rotation angle between two unit quats (degrees) */
static double rot_angle_deg(ngd_quat a, ngd_quat b)
{
    ngd_quat e = ngd_quat_multiply(a, ngd_quat_conjugate(b));
    double w = fabs(e.w); if (w > 1.0) w = 1.0;
    return 2.0 * acos(w) * 180.0 / M_PI;
}

int main(void)
{
    const int N = 60;
    const float fx = 535.4f, fy = 539.2f, cx = 320.1f, cy = 247.6f;

    ngd_orb_extractor ex; ngd_orb_init(&ex, 1000, 1.2f, 8, 20, 7);
    ngd_cam_ctx cam; ngd_cam_ctx_init(&cam, &ex, fx, fy, cx, cy, 0.0747f, 2.988f, 640, 480);

    /* known pose: 15 deg about (1,2,3) + small translation */
    ngd_vec3 axis = ngd_v3_normalize(ngd_v3(1.0f, 2.0f, 3.0f));
    float ang = 15.0f * (float)M_PI / 180.0f;
    ngd_se3 Ttruth;
    Ttruth.q = ngd_quat_from_axis_angle(axis, ang);
    Ttruth.t = ngd_v3(0.10f, -0.05f, 0.30f);

    /* generate 3D world points in front of camera: project world->camera = R*P + t */
    ngd_vec3 *p3d = (ngd_vec3*)malloc((size_t)N * sizeof(ngd_vec3));
    float *p2d = (float*)malloc((size_t)N * 2 * sizeof(float));
    float *sig2 = (float*)malloc((size_t)N * sizeof(float));
    for (int i = 0; i < N; ++i) {
        /* world points around origin, box ~[-0.4,0.4]^3 (non-coplanar) */
        float x = (float)(drand()*0.8 - 0.4);
        float y = (float)(drand()*0.8 - 0.4);
        float z = (float)(drand()*0.8 - 0.4);
        p3d[i] = ngd_v3(x, y, z);
        ngd_vec3 Pc = ngd_se3_map(Ttruth, p3d[i]);
        if (Pc.z <= 0) { i--; continue; }   /* ensure in front */
        float u = fx * Pc.x / Pc.z + cx;
        float v = fy * Pc.y / Pc.z + cy;
        p2d[2*i] = u; p2d[2*i+1] = v;
        sig2[i] = 1.0f;   /* level-0 sigma2 ~1 */
    }

    /* ---- (1) clean EPnP ---- */
    ngd_se3 Test;
    int ok = ngd_epnp_solve(p3d, p2d, N, &cam, &Test);
    CHECK(ok, "epnp_solve returns 1 on clean data");
    double rdeg = rot_angle_deg(Test.q, Ttruth.q);
    double dtx = Test.t.x - Ttruth.t.x, dty = Test.t.y - Ttruth.t.y, dtz = Test.t.z - Ttruth.t.z;
    printf("  clean EPnP: rot_err=%.4f deg, dt=(%.5f,%.5f,%.5f)\n", rdeg, dtx, dty, dtz);
    CHECK(rdeg < 0.05, "clean rotation recovered <0.05 deg");
    CHECK(fabs(dtx) < 1e-3 && fabs(dty) < 1e-3 && fabs(dtz) < 1e-3, "clean translation recovered <1e-3");

    /* ---- (2) RANSAC with 30% outliers ---- */
    int Nout = (int)(N * 0.3);
    float *p2d_noisy = (float*)malloc((size_t)N * 2 * sizeof(float));
    memcpy(p2d_noisy, p2d, (size_t)N * 2 * sizeof(float));
    /* corrupt Nout random indices with random 2D */
    for (int k = 0; k < Nout; ++k) {
        int idx = (int)(drand() * N);
        p2d_noisy[2*idx]   = (float)(drand() * 640);
        p2d_noisy[2*idx+1] = (float)(drand() * 480);
    }

    ngd_se3 Tr; int *inl = (int*)malloc((size_t)N * sizeof(int)); int nIn = 0;
    ngd_pnp_ransac_params rp = ngd_pnp_ransac_defaults();
    ok = ngd_pnp_solve_ransac(p3d, p2d_noisy, sig2, N, &cam, &rp, &Tr, inl, &nIn);
    CHECK(ok, "ransac returns 1");
    int expectIn = N - Nout;
    printf("  RANSAC: inliers=%d (expect ~%d), nIn reported=%d\n", expectIn, expectIn, nIn);
    CHECK(nIn >= expectIn - 4 && nIn <= expectIn + 4, "inlier count ~ N - outliers");

    rdeg = rot_angle_deg(Tr.q, Ttruth.q);
    double dtxr = Tr.t.x - Ttruth.t.x, dtyr = Tr.t.y - Ttruth.t.y, dtzr = Tr.t.z - Ttruth.t.z;
    printf("  RANSAC pose: rot_err=%.4f deg, dt=(%.5f,%.5f,%.5f)\n", rdeg, dtxr, dtyr, dtzr);
    CHECK(rdeg < 1.0, "RANSAC rotation <1 deg");
    CHECK(fabs(dtxr) < 5e-3 && fabs(dtyr) < 5e-3 && fabs(dtzr) < 5e-3, "RANSAC translation <5e-3");

    free(p3d); free(p2d); free(p2d_noisy); free(sig2); free(inl);
    if (fails == 0) { printf("test_pnp: PASS\n"); return 0; }
    printf("test_pnp: %d FAILURES\n", fails);
    return 1;
}
