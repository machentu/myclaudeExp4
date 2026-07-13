/* test_keyframeculling.c — KeyFrameCulling unit test.
 *
 * Validates ngd_local_mapping_keyframe_culling (LocalMapping.cc:910-1062,
 * non-inertial RGBD):
 *   5 KFs (kf0 init + kf1/kf2/kf3 observers + kfR redundant). 20 close points
 *   are observed by all 5 KFs (kfR's redundant MPs: 4 other observers >3 at the
 *   same scale). kfR also owns 1 exclusive MP. Each observer owns 3 unique MPs
 *   so its redundancy ratio is < 90% (not culled). kfR's ratio (20/21 ≈ 0.95)
 *   exceeds 90% -> SetBadFlag: kfR culled, its exclusive MP (nObs<=2 after
 *   erase) becomes bad, shared MPs survive (still 4 observers), and kfR is
 *   pruned from the covisibility lists of the survivors.
 *
 * No BoW required. Pure asserts. */
#include "ngd/orb.h"
#include "ngd/calib.h"
#include "ngd/se3.h"
#include "ngd/pinhole.h"
#include "ngd/keyframe.h"
#include "ngd/mappoint.h"
#include "ngd/map.h"
#include "ngd/localmapping.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { if(!(cond)){ printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); fails++; } } while(0)

static unsigned int rng = 7u;
static float frand(void){ rng = rng*1664525u + 1013904223u; return (rng >> 8) * (1.0f/16777216.0f); }

#define W 640
#define H 480
#define NSHARED 20          /* points 0..19: observed by all 5 KFs */
#define NUNIQ 3             /* points per observer: 20..22 (kf1), 23..25 (kf2), 26..28 (kf3) */
#define NPTS 30             /* + point 29: kfR exclusive */
#define POINT_KFR_EXCL 29

static ngd_cam_ctx make_cam(void)
{
    ngd_orb_extractor ex; ngd_orb_init(&ex, 1000, 1.2f, 8, 20, 7);
    ngd_cam_ctx cam; ngd_cam_ctx_init(&cam, &ex, 535.4f, 539.2f, 320.1f, 247.6f, 0.0747f, 2.988f, W, H);
    return cam;
}

static int project_point(ngd_cam_ctx *cam, ngd_se3 Tcw, const float Xw[3],
                         float *ux, float *uy, float *ur, float *zout)
{
    ngd_vec3 Xc = ngd_se3_map(Tcw, ngd_v3(Xw[0],Xw[1],Xw[2]));
    if (Xc.z <= 0.1f) return 0;
    float u = cam->cam.fx*Xc.x/Xc.z + cam->cam.cx;
    float v = cam->cam.fy*Xc.y/Xc.z + cam->cam.cy;
    if (u<4||u>W-4||v<4||v>H-4) return 0;
    *ux=u; *uy=v; *ur = u - cam->mbf/Xc.z; *zout = Xc.z;
    return 1;
}

static void make_desc(int point_idx, uint8_t desc[32]) {
    memset(desc, 0, 32);
    desc[0] = (uint8_t)point_idx;
}

static ngd_keyframe *build_kf(uint64_t id, ngd_se3 pose, ngd_cam_ctx *cam,
                              const float pts[][3], int npts, int *idxmap)
{
    ngd_keypoint *kps = (ngd_keypoint*)malloc((size_t)npts*sizeof(ngd_keypoint));
    uint8_t *desc = (uint8_t*)calloc((size_t)npts*32, 1);
    float *ur = (float*)malloc((size_t)npts*sizeof(float));
    float *dep = (float*)malloc((size_t)npts*sizeof(float));
    int n=0;
    for (int j=0;j<npts;++j){
        float ux,uy,r,z;
        if (!project_point(cam, pose, pts[j], &ux,&uy,&r,&z)) { idxmap[j]=-1; continue; }
        kps[n].x=ux; kps[n].y=uy; kps[n].octave=0; kps[n].angle=0; kps[n].response=1; kps[n].size=31;
        ur[n]=r; dep[n]=z;
        make_desc(j, desc + (size_t)n*32);
        idxmap[j] = n;
        n++;
    }
    ngd_keyframe *kf = ngd_keyframe_new(id, pose, cam, n, kps, desc, ur, dep);
    free(kps); free(desc); free(ur); free(dep);
    return kf;
}

/* create a MP at pts[j] observed (stereo) by the listed KFs; assign to each
 * KF's mvpMapPoints at its keypoint index for point j. */
static ngd_mappoint *make_shared_mp(const float pts[][3], int j,
                                    ngd_keyframe **kfs, const int *kf_ids,
                                    int nk, ngd_map *map)
{
    float pos[3]={pts[j][0],pts[j][1],pts[j][2]};
    ngd_mappoint *mp = ngd_mappoint_new(pos, kfs[0]);
    for (int k=0;k<nk;++k){
        ngd_keyframe *kf = kfs[k];
        int idx = kf_ids[k];       /* keypoint index in this KF for point j */
        if (idx < 0) continue;
        ngd_mappoint_add_observation(mp, kf, idx, idx);   /* stereo -> nObs+=2 */
        kf->mvpMapPoints[idx] = mp;
    }
    ngd_mappoint_compute_distinctive_descriptors(mp);
    ngd_mappoint_update_normal_and_depth(mp);
    ngd_map_add_mappoint(map, mp);
    return mp;
}

int main(void)
{
    rng = 7u;
    ngd_cam_ctx cam = make_cam();

    static float pts[NPTS][3];
    for (int j=0;j<NPTS;++j){
        pts[j][0] = (frand()-0.5f)*1.0f;
        pts[j][1] = (frand()-0.5f)*0.7f;
        pts[j][2] = 1.6f + frand()*0.4f;     /* < thDepth=2.988 -> "close" */
    }

    /* 5 KFs within ~0.05m of each other -> all see all 30 points. */
    ngd_se3 P0  = ngd_se3_identity();
    ngd_se3 P1  = ngd_se3_from_rt(ngd_quat_to_matrix(ngd_quat_identity()), ngd_v3(0.05f,0,0));
    ngd_se3 P2  = ngd_se3_from_rt(ngd_quat_to_matrix(ngd_quat_identity()), ngd_v3(0,0.05f,0));
    ngd_se3 P3  = ngd_se3_from_rt(ngd_quat_to_matrix(ngd_quat_identity()), ngd_v3(0.05f,0.05f,0));
    ngd_se3 PR  = ngd_se3_from_rt(ngd_quat_to_matrix(ngd_quat_identity()), ngd_v3(0.025f,0.025f,0));

    int idx0[NPTS], idx1[NPTS], idx2[NPTS], idx3[NPTS], idxR[NPTS];
    ngd_keyframe *kf0 = build_kf(0, P0, &cam, pts, NPTS, idx0);
    ngd_keyframe *kf1 = build_kf(1, P1, &cam, pts, NPTS, idx1);
    ngd_keyframe *kf2 = build_kf(2, P2, &cam, pts, NPTS, idx2);
    ngd_keyframe *kf3 = build_kf(3, P3, &cam, pts, NPTS, idx3);
    ngd_keyframe *kfR = build_kf(4, PR, &cam, pts, NPTS, idxR);
    ngd_keyframe *obs[5] = { kf0, kf1, kf2, kf3, kfR };

    ngd_map map; ngd_map_init(&map);
    ngd_local_mapping lm; ngd_local_mapping_init(&lm, &map, /*mbMonocular=*/0);

    /* 20 shared MPs (points 0..19) observed by all 5 KFs. */
    ngd_mappoint *shared[NSHARED];
    for (int j=0;j<NSHARED;++j){
        int ids[5] = { idx0[j], idx1[j], idx2[j], idx3[j], idxR[j] };
        shared[j] = make_shared_mp(pts, j, obs, ids, 5, &map);
    }
    /* kfR exclusive MP (point 29). */
    int excl_ids[1] = { idxR[POINT_KFR_EXCL] };
    ngd_mappoint *mp_excl = make_shared_mp(pts, POINT_KFR_EXCL, &kfR, excl_ids, 1, &map);

    /* each observer (kf1,kf2,kf3) gets 3 unique MPs (observed by only that KF),
     * inflating its nMPs so its redundancy ratio < 0.9. */
    int uniq_start[3] = { 20, 23, 26 };
    ngd_keyframe *uniq_kf[3] = { kf1, kf2, kf3 };
    int *uniq_idx[3] = { idx1, idx2, idx3 };
    for (int o=0;o<3;++o){
        for (int k=0;k<NUNIQ;++k){
            int j = uniq_start[o] + k;
            int ids[1] = { uniq_idx[o][j] };
            make_shared_mp(pts, j, &uniq_kf[o], ids, 1, &map);
        }
    }

    for (int i=0;i<5;++i) ngd_map_add_keyframe(&map, obs[i]);
    for (int i=0;i<5;++i) ngd_keyframe_update_connections(obs[i]);

    CHECK(kf3->nConn >= 1, "kf3 has covisibility (shares MPs with kfR et al.)");
    int nKFs_before = map.nKFs;
    printf("  scene: %d KFs, %d MPs, kf3.nConn=%d\n", nKFs_before, map.nMPs, kf3->nConn);

    /* kf3 is currentKF; its covisibles include kf0(init),kf1,kf2,kfR. */
    lm.currentKF = kf3;
    ngd_local_mapping_keyframe_culling(&lm);

    /* ---- assertions ---- */
    CHECK(kfR->mbBad == 1, "redundant kfR culled (SetBadFlag)");
    CHECK(kf0->mbBad == 0, "init kf0 NOT culled");
    CHECK(kf1->mbBad == 0, "non-redundant kf1 NOT culled");
    CHECK(kf2->mbBad == 0, "non-redundant kf2 NOT culled");
    CHECK(kf3->mbBad == 0, "current kf3 NOT culled");
    CHECK(map.nKFs == nKFs_before - 1, "map.nKFs decreased by 1 (kfR erased)");

    /* kfR's exclusive MP: only observer was kfR -> nObs 2->0 <=2 -> bad */
    CHECK(mp_excl->mbBad == 1, "kfR exclusive MP set bad (nObs<=2 after erase)");

    /* shared MPs survive: still observed by the 4 non-culled observers */
    int shared_survive = 0, shared_4obs = 0;
    for (int j=0;j<NSHARED;++j){
        if (!shared[j]->mbBad){
            shared_survive++;
            int nkf=0;
            for (int o=0;o<shared[j]->nObsRec;++o){
                ngd_keyframe *k = shared[j]->obs[o].kf;
                if (k==kf0||k==kf1||k==kf2||k==kf3) nkf++;
            }
            if (nkf==4) shared_4obs++;
        }
    }
    printf("  shared MPs: %d/%d survive, %d observed by all 4 survivors\n",
           shared_survive, NSHARED, shared_4obs);
    CHECK(shared_survive == NSHARED, "all shared MPs survive kfR culling");
    CHECK(shared_4obs == NSHARED, "shared MPs still observed by 4 surviving KFs");

    /* kfR pruned from survivors' covisibility lists */
    int kfR_in_kf3 = 0;
    for (int i=0;i<kf3->nConn;++i) if (kf3->connKFs[i]==kfR) kfR_in_kf3++;
    CHECK(kfR_in_kf3 == 0, "kfR pruned from kf3 covisibility list");

    /* cleanup. mp_excl is bad but still in map.mps (KeyFrameCulling does not
     * sweep bad MPs — only SearchInNeighbors does), so the map.mps loop frees
     * it. kfR was erased from map.kfs by SetBadFlag, so free it separately. */
    for (int i=0;i<map.nMPs;++i) ngd_mappoint_free(map.mps[i]);
    for (int i=0;i<map.nKFs;++i) ngd_keyframe_free(map.kfs[i]);
    ngd_keyframe_free(kfR);
    ngd_local_mapping_free(&lm);
    ngd_map_free(&map);

    if (fails) { printf("test_keyframeculling: %d FAIL\n", fails); return 1; }
    printf("test_keyframeculling: PASS\n");
    return 0;
}
