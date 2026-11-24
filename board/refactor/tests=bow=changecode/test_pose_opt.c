/* test_pose_opt.c — synthetic validation of ngd_pose_optimization.
 *
 * Builds a known scene (3-D points + ground-truth pose), projects to get
 * observations, injects a few gross outliers, perturbs the pose, runs the
 * optimizer, and asserts:
 *   - the pose is recovered to the ground truth (translation+rotation),
 *   - inlier reprojection error collapses to ~0,
 *   - the gross outliers are flagged as outliers.
 * Exercises both the monocular and the stereo code paths.
 */
#include "ngd/so3.h"
#include "ngd/se3.h"
#include "ngd/pinhole.h"
#include "ngd/pose_opt.h"

#include <stdio.h>
#include <math.h>

static int fails = 0;
#define CHECK(cond, msg) do { if(!(cond)){ printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); fails++; } } while(0)
#define APPROX(a,b,eps) (fabsf((a)-(b)) <= (eps))

/* deterministic LCG so the test is reproducible across platforms */
static unsigned int rng = 2463534242u;
static float frand(void){ rng = rng*1664525u + 1013904223u; return (rng >> 8) * (1.0f/16777216.0f); } /* [0,1) */

static float pose_dist(ngd_se3 a, ngd_se3 b) {
    ngd_mat3 Ra = ngd_quat_to_matrix(a.q), Rb = ngd_quat_to_matrix(b.q);
    ngd_mat3 dR = ngd_m3_multiply(ngd_m3_transpose(Ra), Rb);
    ngd_vec3 w = ngd_so3_log(dR);
    ngd_vec3 dt = ngd_v3_sub(a.t, b.t);
    return ngd_v3_norm(w) + ngd_v3_norm(dt);
}

#define NPTS 40

/* Run one scenario: stereo flag selects mono (0) or stereo (1). */
static int run_scenario(int stereo)
{
    ngd_pinhole cam = ngd_pinhole_make(500.f, 500.f, 320.f, 240.f);
    float bf = 500.f * 0.075f;          /* baseline 0.075 m */

    ngd_vec3 Xw[NPTS];
    float obs[3*NPTS];
    int is_stereo[NPTS], octave[NPTS], gross[NPTS];
    int outlier[NPTS];
    float invS2[1] = { 1.0f };

    /* ground-truth pose */
    ngd_se3 Tgt = ngd_se3_identity();
    Tgt.q = ngd_so3_exp(ngd_v3_scale(ngd_v3_normalize(ngd_v3(1,2,3)), 0.12f));
    Tgt.t = ngd_v3(0.25f, -0.18f, 0.4f);

    for (int i = 0; i < NPTS; ++i) {
        Xw[i] = ngd_v3((frand()*2-1)*0.8f, (frand()*2-1)*0.6f, 2.5f + frand()*2.5f);
        ngd_vec3 Xc = ngd_se3_map(Tgt, Xw[i]);
        if (Xc.z <= 0.5f) { i--; continue; }     /* keep points well in front */

        float p[3];
        if (stereo) ngd_pinhole_project_stereo(cam, bf, Xc, p);
        else        ngd_pinhole_project(cam, Xc, p);
        obs[3*i+0]=p[0]; obs[3*i+1]=p[1]; obs[3*i+2]=p[2];
        is_stereo[i] = stereo;
        octave[i] = 0;
        gross[i] = 0;
    }

    /* inject 5 gross outliers */
    int ngross = 0;
    for (int k = 0; k < 5; ++k) {
        int idx = (int)(frand()*NPTS); if (idx>=NPTS) idx=NPTS-1;
        if (gross[idx]) { k--; continue; }
        gross[idx] = 1;
        obs[3*idx+0] += 45.f;          /* large pixel offset */
        obs[3*idx+1] += 33.f;
        if (stereo) obs[3*idx+2] += 50.f;
        ngross++;
    }

    /* perturbed initial pose (small, so LM converges) */
    float xi_pert[6] = { 0.018f, -0.014f, 0.016f, 0.06f, -0.05f, 0.04f };
    ngd_se3 T0 = ngd_se3_multiply(ngd_se3_exp(xi_pert), Tgt);

    ngd_pose_obs O;
    O.cam = cam; O.bf = bf; O.n = NPTS;
    O.Xw = Xw; O.obs = obs; O.is_stereo = is_stereo; O.octave = octave;
    O.invLevelSigma2 = invS2; O.nlevels = 1;

    int inliers = ngd_pose_optimization(&T0, &O, outlier);
    (void)inliers;

    /* 1) pose recovered to ground truth */
    float d = pose_dist(T0, Tgt);
    CHECK(d < 1e-3f, stereo ? "stereo pose recovered" : "mono pose recovered");

    /* 2) inlier reprojection RMS collapses */
    double sse = 0; int cnt = 0;
    for (int i = 0; i < NPTS; ++i) {
        if (gross[i]) continue;        /* skip injected outliers */
        ngd_vec3 Xc = ngd_se3_map(T0, Xw[i]);
        float p[3];
        if (stereo) ngd_pinhole_project_stereo(cam, bf, Xc, p);
        else        ngd_pinhole_project(cam, Xc, p);
        float du = obs[3*i+0]-p[0], dv = obs[3*i+1]-p[1];
        sse += du*du + dv*dv;
        if (stereo) { float dr=obs[3*i+2]-p[2]; sse += dr*dr; }
        cnt++;
    }
    float rms = (float)sqrt(sse/cnt);
    CHECK(rms < 0.5f, stereo ? "stereo inlier RMS small" : "mono inlier RMS small");

    /* 3) every gross outlier flagged */
    int gross_flagged = 0, inlier_flagged = 0;
    for (int i = 0; i < NPTS; ++i) {
        if (gross[i]) { if (outlier[i]) gross_flagged++; }
        else          { if (outlier[i]) inlier_flagged++; }
    }
    CHECK(gross_flagged == ngross, stereo ? "stereo all gross flagged" : "mono all gross flagged");
    CHECK(inlier_flagged <= 2, stereo ? "stereo few inliers misflagged" : "mono few inliers misflagged");

    printf("  [%s] pose_err=%.2e inlier_rms=%.4f gross=%d/%d flagged, inlier_misflag=%d, inliers=%d\n",
           stereo?"stereo":"mono", d, rms, gross_flagged, ngross, inlier_flagged, inliers);
    return fails;
}

int main(void)
{
    printf("test_pose_opt:\n");
    run_scenario(0);   /* monocular */
    run_scenario(1);   /* stereo    */

    if (fails == 0) { printf("test_pose_opt: PASS\n"); return 0; }
    printf("test_pose_opt: %d FAILURES\n", fails);
    return 1;
}
