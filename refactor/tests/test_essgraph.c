/* test_essgraph.c — OptimizeEssentialGraph (Sim3 pose graph).
 *
 * 3 KFs in a chain (kf0 fixed at origin, kf1, kf2), each consecutive pair
 * covisible (shares >= MIN_FEAT=100 MapPoints). Vertices are INITIALIZED at
 * DRIFTED poses; covisibility edges are measured from the TRUE poses (supplied
 * via `nonCorrected`). After optimization kf1/kf2 must converge back to their
 * true poses, and the shared MapPoints are corrected along with the refKF.
 */
#include "ngd/orb.h"
#include "ngd/calib.h"
#include "ngd/se3.h"
#include "ngd/sim3.h"
#include "ngd/keyframe.h"
#include "ngd/mappoint.h"
#include "ngd/map.h"
#include "ngd/essgraph.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { if(!(cond)){ printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); fails++; } } while(0)
#define APPROX(a,b,eps) (fabsf((a)-(b)) <= (eps))

static ngd_cam_ctx make_cam(void)
{
    ngd_orb_extractor ex; ngd_orb_init(&ex, 1000, 1.2f, 8, 20, 7);
    ngd_cam_ctx cam; ngd_cam_ctx_init(&cam, &ex, 535.4f, 539.2f, 320.1f, 247.6f, 0.0747f, 2.988f, 640, 480);
    return cam;
}

static float se3_tdist(ngd_se3 a, ngd_se3 b) { return ngd_v3_norm(ngd_v3_sub(a.t, b.t)); }
static float se3_rdist(ngd_se3 a, ngd_se3 b) {
    ngd_mat3 Ra = ngd_quat_to_matrix(a.q), Rb = ngd_quat_to_matrix(b.q);
    ngd_mat3 dR = ngd_m3_multiply(ngd_m3_transpose(Ra), Rb);
    return ngd_v3_norm(ngd_so3_log(dR));
}

/* a minimal "frame" with 1 keypoint per point, enough to host MPs */
static ngd_keyframe *make_kf(uint64_t id, ngd_se3 pose, ngd_cam_ctx *cam, int N) {
    ngd_keypoint *kp = (ngd_keypoint*)calloc((size_t)N, sizeof(ngd_keypoint));
    uint8_t *desc = (uint8_t*)calloc((size_t)N*32, 1);
    float *ur = (float*)malloc(sizeof(float)*N);
    float *dep = (float*)malloc(sizeof(float)*N);
    for (int i = 0; i < N; ++i) {
        kp[i].x = 100.f + i; kp[i].y = 100.f; kp[i].octave = 0; kp[i].angle = 0; kp[i].response = 1; kp[i].size = 31;
        ur[i] = -1.f; dep[i] = 2.f;
    }
    ngd_keyframe *kf = ngd_keyframe_new(id, pose, cam, N, kp, desc, ur, dep);
    free(kp); free(desc); free(ur); free(dep);
    return kf;
}

int main(void)
{
    ngd_cam_ctx cam = make_cam();

    /* true poses: a straight-line translation along +X. */
    ngd_se3 T0 = ngd_se3_identity();
    ngd_se3 T1 = ngd_se3_from_rt(ngd_m3_identity(), ngd_v3(0.5f, 0.0f, 0.0f));
    ngd_se3 T2 = ngd_se3_from_rt(ngd_m3_identity(), ngd_v3(1.0f, 0.0f, 0.0f));

    int NMP = 120;   /* >= MIN_FEAT shared between consecutive KFs */
    ngd_keyframe *kf0 = make_kf(0, T0, &cam, NMP);
    ngd_keyframe *kf1 = make_kf(1, T1, &cam, NMP);
    ngd_keyframe *kf2 = make_kf(2, T2, &cam, NMP);

    /* shared world points near the origin; observed (formally) by each KF.
     * Each MP's refKF = kf0 for simplicity. */
    ngd_map map; ngd_map_init(&map);
    ngd_mappoint **mps = (ngd_mappoint**)malloc(sizeof(ngd_mappoint*)*NMP);
    for (int j = 0; j < NMP; ++j) {
        float pos[3] = { 0.2f*((j%10)-5)*0.1f, 0.1f*((j/10)-5)*0.1f, 1.5f };
        ngd_mappoint *mp = ngd_mappoint_new(pos, kf0);
        /* observe in all three KFs to build covisibility weights = NMP */
        ngd_mappoint_add_observation(mp, kf0, j, -1);
        ngd_mappoint_add_observation(mp, kf1, j, -1);
        ngd_mappoint_add_observation(mp, kf2, j, -1);
        kf0->mvpMapPoints[j] = mp;
        kf1->mvpMapPoints[j] = mp;
        kf2->mvpMapPoints[j] = mp;
        memset(mp->descriptor, (uint8_t)(j & 0xFF), 32);
        ngd_mappoint_update_normal_and_depth(mp);
        mps[j] = mp;
        ngd_map_add_mappoint(&map, mp);
    }
    ngd_map_add_keyframe(&map, kf0);
    ngd_map_add_keyframe(&map, kf1);
    ngd_map_add_keyframe(&map, kf2);
    ngd_keyframe_update_connections(kf0);
    ngd_keyframe_update_connections(kf1);
    ngd_keyframe_update_connections(kf2);
    CHECK(kf0->nConn >= 1 && kf1->nConn >= 1, "covisibility graph built");
    /* kf0<->kf1 and kf1<->kf2 share NMP points (>=100) */
    CHECK(ngd_keyframe_get_weight(kf0, kf1) >= 100, "kf0-kf1 weight>=100");
    CHECK(ngd_keyframe_get_weight(kf1, kf2) >= 100, "kf1-kf2 weight>=100");

    /* ---- inject drift into kf1, kf2 stored poses ---- */
    ngd_se3 T1d = ngd_se3_from_rt(ngd_m3_identity(), ngd_v3(0.65f, 0.05f, 0.0f));  /* +0.15, +0.05 drift */
    ngd_se3 T2d = ngd_se3_from_rt(ngd_m3_identity(), ngd_v3(1.35f, 0.10f, 0.0f));  /* +0.35, +0.10 drift */
    ngd_keyframe_set_pose(kf1, T1d);
    ngd_keyframe_set_pose(kf2, T2d);

    /* nonCorrected = TRUE poses (so covisibility edges encode correct relatives) */
    ngd_kf_sim3 nc[3] = { {kf0, {.q=T0.q, .t=T0.t, .s=1.0f}},
                          {kf1, {.q=T1.q, .t=T1.t, .s=1.0f}},
                          {kf2, {.q=T2.q, .t=T2.t, .s=1.0f}} };

    ngd_optimize_essential_graph(&map, /*pLoopKF=*/kf0, /*pCurKF=*/kf2,
                                 /*corrected=*/NULL, 0,
                                 /*nonCorrected=*/nc, 3,
                                 /*loopConns=*/NULL, 0, /*bFixScale=*/1);

    /* kf1, kf2 should converge back toward true T1, T2 (kf0 fixed at T0). */
    float e1t = se3_tdist(kf1->pose, T1), e1r = se3_rdist(kf1->pose, T1);
    float e2t = se3_tdist(kf2->pose, T2), e2r = se3_rdist(kf2->pose, T2);
    printf("  kf1: t_err=%.4f r_err=%.4f | kf2: t_err=%.4f r_err=%.4f\n", e1t, e1r, e2t, e2r);
    CHECK(APPROX(kf0->pose.t.x, 0.0f, 1e-4f), "kf0 fixed at origin");
    CHECK(e1t < 1e-2f, "kf1 translation recovered");
    CHECK(e2t < 1e-2f, "kf2 translation recovered");
    CHECK(e1r < 1e-3f && e2r < 1e-3f, "rotations recovered");

    /* MapPoints corrected along refKF (kf0, fixed) -> unchanged (kf0 didn't move) */
    /* sanity: MP world positions still finite & near original */
    int nok = 0;
    for (int j = 0; j < NMP; ++j) {
        if (isfinite(mps[j]->worldPos[0]) && isfinite(mps[j]->worldPos[2])) nok++;
    }
    CHECK(nok == NMP, "all MapPoints corrected to finite positions");

    /* cleanup */
    for (int j = 0; j < NMP; ++j) ngd_mappoint_free(mps[j]);
    ngd_keyframe_free(kf0); ngd_keyframe_free(kf1); ngd_keyframe_free(kf2);
    ngd_map_free(&map);
    free(mps);

    if (fails) { printf("test_essgraph: %d FAIL\n", fails); return 1; }
    printf("test_essgraph: PASS\n");
    return 0;
}
