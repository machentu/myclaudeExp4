/* test_loopclosing.c — LoopClosing::CorrectLoop orchestration (single-map).
 *
 * 3 KFs (kf0=matched/anchor at origin, kf1, kf2=current) sharing MapPoints
 * (covisible chain). A consistent rigid drift D is injected into kf1/kf2 stored
 * poses. With loopScw = true currentKF pose injected (the detection result),
 * CorrectLoop must: set currentKF pose from loopScw, propagate the correction
 * along the covisibility neighbourhood (kf1 -> true), correct MapPoints, fuse,
 * run OptimizeEssentialGraph + GBA — ending with kf1/kf2 near their true poses.
 */
#include "ngd/orb.h"
#include "ngd/calib.h"
#include "ngd/se3.h"
#include "ngd/sim3.h"
#include "ngd/keyframe.h"
#include "ngd/mappoint.h"
#include "ngd/map.h"
#include "ngd/loopclosing.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { if(!(cond)){ printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); fails++; } } while(0)
#define APPROX(a,b,eps) (fabsf((a)-(b)) <= (eps))

static ngd_cam_ctx make_cam(void) {
    ngd_orb_extractor ex; ngd_orb_init(&ex, 1000, 1.2f, 8, 20, 7);
    ngd_cam_ctx cam; ngd_cam_ctx_init(&cam, &ex, 535.4f, 539.2f, 320.1f, 247.6f, 0.0747f, 2.988f, 640, 480);
    return cam;
}

static float tdist(ngd_se3 a, ngd_se3 b) { return ngd_v3_norm(ngd_v3_sub(a.t, b.t)); }

/* project world pt into KF image; return 1 if visible. fills (u,v,z). */
static int proj(ngd_cam_ctx *cam, ngd_se3 Tcw, const float Xw[3], float *u, float *v, float *z) {
    ngd_vec3 Xc = ngd_se3_map(Tcw, ngd_v3(Xw[0],Xw[1],Xw[2]));
    if (Xc.z <= 0.1f) return 0;
    float uu = cam->cam.fx*Xc.x/Xc.z + cam->cam.cx;
    float vv = cam->cam.fy*Xc.y/Xc.z + cam->cam.cy;
    if (uu<8||uu>632||vv<8||vv>472) return 0;
    *u=uu; *v=vv; *z=Xc.z; return 1;
}

/* build a KF whose keypoints are the projections of pts[] (only visible ones). */
static ngd_keyframe *build_kf(uint64_t id, ngd_se3 pose, ngd_cam_ctx *cam,
                              const float pts[][3], int npts, int *wpidx, int *n_out)
{
    ngd_keypoint *kp = (ngd_keypoint*)calloc((size_t)npts, sizeof(ngd_keypoint));
    uint8_t *desc = (uint8_t*)calloc((size_t)npts*32, 1);
    float *ur = (float*)malloc(sizeof(float)*npts);
    float *dep = (float*)malloc(sizeof(float)*npts);
    int n=0;
    for (int j=0;j<npts;++j){
        float u,v,z;
        if (!proj(cam, pose, pts[j], &u,&v,&z)) continue;
        kp[n].x=u; kp[n].y=v; kp[n].octave=0; kp[n].angle=0; kp[n].response=1; kp[n].size=31;
        ur[n]=-1; dep[n]=z;
        memset(desc+(size_t)n*32, (uint8_t)(j&0xFF), 32);
        wpidx[n]=j;
        n++;
    }
    ngd_keyframe *kf = ngd_keyframe_new(id, pose, cam, n, kp, desc, ur, dep);
    ngd_keyframe_assign_grid(kf);
    free(kp); free(desc); free(ur); free(dep);
    *n_out = n;
    return kf;
}

int main(void)
{
    ngd_cam_ctx cam = make_cam();
    int NPTS = 120;
    static float pts[120][3];
    srand(3);
    for (int j=0;j<NPTS;++j){
        pts[j][0] = (rand()%1000)/1000.0f*3.0f - 1.5f;  /* x in [-1.5,1.5] (seen by all 3 KFs) */
        pts[j][1] = (rand()%1000)/1000.0f*0.6f - 0.3f;
        pts[j][2] = 1.8f + (rand()%1000)/1000.0f*0.4f;
    }

    ngd_se3 T0 = ngd_se3_identity();
    ngd_se3 T1 = ngd_se3_from_rt(ngd_m3_identity(), ngd_v3(0.5f,0,0));
    ngd_se3 T2 = ngd_se3_from_rt(ngd_m3_identity(), ngd_v3(1.0f,0,0));

    int wp0[120],wp1[120],wp2[120], n0,n1,n2;
    ngd_keyframe *kf0 = build_kf(0, T0, &cam, pts, NPTS, wp0, &n0);
    ngd_keyframe *kf1 = build_kf(1, T1, &cam, pts, NPTS, wp1, &n1);
    ngd_keyframe *kf2 = build_kf(2, T2, &cam, pts, NPTS, wp2, &n2);

    /* attach shared MPs (each visible point -> one MP observed by all KFs that see it) */
    ngd_map map; ngd_map_init(&map);
    ngd_mappoint **mps = (ngd_mappoint**)malloc(sizeof(ngd_mappoint*)*NPTS);
    for (int j=0;j<NPTS;++j) mps[j]=NULL;
    for (int j=0;j<NPTS;++j){
        /* find each KF's keypoint for point j */
        int k0=-1,k1=-1,k2=-1;
        for (int k=0;k<n0;++k) if (wp0[k]==j) k0=k;
        for (int k=0;k<n1;++k) if (wp1[k]==j) k1=k;
        for (int k=0;k<n2;++k) if (wp2[k]==j) k2=k;
        if (k0<0&&k1<0&&k2<0) continue;
        float pos[3]={pts[j][0],pts[j][1],pts[j][2]};
        ngd_mappoint *mp = ngd_mappoint_new(pos, kf0);
        memset(mp->descriptor, (uint8_t)(j&0xFF), 32);
        if (k0>=0){ ngd_mappoint_add_observation(mp,kf0,k0,-1); kf0->mvpMapPoints[k0]=mp; }
        if (k1>=0){ ngd_mappoint_add_observation(mp,kf1,k1,-1); kf1->mvpMapPoints[k1]=mp; }
        if (k2>=0){ ngd_mappoint_add_observation(mp,kf2,k2,-1); kf2->mvpMapPoints[k2]=mp; }
        ngd_mappoint_update_normal_and_depth(mp);
        mps[j]=mp; ngd_map_add_mappoint(&map, mp);
    }
    ngd_map_add_keyframe(&map, kf0);
    ngd_map_add_keyframe(&map, kf1);
    ngd_map_add_keyframe(&map, kf2);
    ngd_keyframe_update_connections(kf0);
    ngd_keyframe_update_connections(kf1);
    ngd_keyframe_update_connections(kf2);
    CHECK(kf2->nConn >= 1, "kf2 covisible (shares MPs)");

    /* ---- inject a consistent rigid drift D into ALL three KFs ----
     * (so covisibility edges remain locally consistent; correct_loop propagates
     *  the loopScw correction along the covisibility neighbourhood, and the
     *  init/matched KF kf0 is corrected back to its true anchor pose T0). */
    ngd_se3 D = ngd_se3_from_rt(ngd_m3_identity(), ngd_v3(0.15f, 0.05f, 0.0f));
    ngd_se3 T0d = ngd_se3_multiply(D, T0);
    ngd_se3 T1d = ngd_se3_multiply(D, T1);
    ngd_se3 T2d = ngd_se3_multiply(D, T2);
    ngd_keyframe_set_pose(kf0, T0d);
    ngd_keyframe_set_pose(kf1, T1d);
    ngd_keyframe_set_pose(kf2, T2d);

    /* ---- set up loopclosing context + inject detection result ---- */
    ngd_loopclosing lc;
    ngd_loopclosing_init(&lc, &map, NULL, NULL, /*bFixScale=*/1);
    lc.currentKF = kf2;
    lc.loopMatchedKF = kf0;
    ngd_sim3 loopScw; loopScw.q = T2.q; loopScw.t = T2.t; loopScw.s = 1.0f;  /* true currentKF pose */
    lc.loopScw = loopScw;
    lc.loopMatchedMPs = (ngd_mappoint**)calloc((size_t)kf2->N, sizeof(ngd_mappoint*));
    lc.nCurN = kf2->N;

    printf("  before: kf1.t=(%.3f,%.3f) kf2.t=(%.3f,%.3f)\n",
           kf1->pose.t.x, kf1->pose.t.y, kf2->pose.t.x, kf2->pose.t.y);

    ngd_loopclosing_correct_loop(&lc);

    printf("  after:  kf1.t=(%.3f,%.3f) [true (0.5,0)] kf2.t=(%.3f,%.3f) [true (1,0)]\n",
           kf1->pose.t.x, kf1->pose.t.y, kf2->pose.t.x, kf2->pose.t.y);

    /* currentKF pose set from loopScw (== true T2, up to essgraph/gba refinement) */
    CHECK(tdist(kf2->pose, T2) < 0.1f, "kf2 (current) near true T2");
    /* kf1 propagated toward true T1 */
    CHECK(tdist(kf1->pose, T1) < 0.1f, "kf1 propagated near true T1");
    /* kf0 (anchor) still ~origin */
    CHECK(tdist(kf0->pose, T0) < 0.1f, "kf0 anchor near origin");
    /* MPs finite */
    int nok=0; for (int j=0;j<NPTS;++j) if (mps[j] && isfinite(mps[j]->worldPos[0])) nok++;
    CHECK(nok > 0, "MapPoints corrected to finite positions");

    /* cleanup */
    for (int j=0;j<NPTS;++j) if (mps[j]) ngd_mappoint_free(mps[j]);
    ngd_keyframe_free(kf0); ngd_keyframe_free(kf1); ngd_keyframe_free(kf2);
    ngd_map_free(&map);
    ngd_loopclosing_free(&lc);
    free(mps);

    if (fails) { printf("test_loopclosing: %d FAIL\n", fails); return 1; }
    printf("test_loopclosing: PASS\n");
    return 0;
}
