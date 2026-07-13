/* test_fuse.c — SearchInNeighbors/Fuse unit test.
 *
 * Validates ngd_local_mapping_search_in_neighbors (LocalMapping.cc:722-831)
 * + ngd_fuse (ORBmatcher.cc:1152-1342) end-to-end:
 *   2 KFs observe 30 points. Points 0..9 carry one shared MP each (covisibility).
 *   Points 10..29 are DUPLICATED: kf0 owns mpA, kf1 owns mpB at the same world
 *   position, same descriptor. search_in_neighbors fuses mpA into mpB (or vice
 *   versa): one survives observed by both KFs, the other is Replace()'d (bad)
 *   and swept from the map.
 *
 * No BoW required (Fuse uses Hamming distance, not the vocabulary). Pure asserts. */
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

static unsigned int rng = 42u;
static float frand(void){ rng = rng*1664525u + 1013904223u; return (rng >> 8) * (1.0f/16777216.0f); }

#define W 640
#define H 480
#define NPTS 30
#define NSHARED 10

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
                              const float pts[][3], int npts, int *n_out)
{
    ngd_keypoint *kps = (ngd_keypoint*)malloc((size_t)npts*sizeof(ngd_keypoint));
    uint8_t *desc = (uint8_t*)calloc((size_t)npts*32, 1);
    float *ur = (float*)malloc((size_t)npts*sizeof(float));
    float *dep = (float*)malloc((size_t)npts*sizeof(float));
    int n=0;
    for (int j=0;j<npts;++j){
        float ux,uy,r,z;
        if (!project_point(cam, pose, pts[j], &ux,&uy,&r,&z)) continue;
        kps[n].x=ux; kps[n].y=uy; kps[n].octave=0; kps[n].angle=0; kps[n].response=1; kps[n].size=31;
        ur[n]=r; dep[n]=z;
        make_desc(j, desc + (size_t)n*32);
        n++;
    }
    ngd_keyframe *kf = ngd_keyframe_new(id, pose, cam, n, kps, desc, ur, dep);
    free(kps); free(desc); free(ur); free(dep);
    *n_out = n;
    return kf;
}

static void build_index_map(ngd_cam_ctx *cam, ngd_se3 pose, const float pts[][3], int npts, int *idxmap) {
    int n=0;
    for (int j=0;j<npts;++j){
        float ux,uy,r,z;
        if (project_point(cam, pose, pts[j], &ux,&uy,&r,&z)) idxmap[n++]=j;
    }
}

int main(void)
{
    rng = 42u;
    ngd_cam_ctx cam = make_cam();

    static float pts[NPTS][3];
    for (int j=0;j<NPTS;++j){
        pts[j][0] = (frand()-0.5f)*1.0f;     /* keep well inside the frustum */
        pts[j][1] = (frand()-0.5f)*0.7f;
        pts[j][2] = 1.6f + frand()*0.4f;
    }

    ngd_se3 P0 = ngd_se3_identity();
    ngd_se3 P1 = ngd_se3_from_rt(ngd_quat_to_matrix(ngd_quat_identity()), ngd_v3(0.15f, 0.0f, 0.0f));
    int n0, n1;
    ngd_keyframe *kf0 = build_kf(0, P0, &cam, pts, NPTS, &n0);
    ngd_keyframe *kf1 = build_kf(1, P1, &cam, pts, NPTS, &n1);

    int map0[NPTS], map1[NPTS];
    build_index_map(&cam, P0, pts, NPTS, map0);
    build_index_map(&cam, P1, pts, NPTS, map1);
    int inv0[NPTS], inv1[NPTS];
    for (int j=0;j<NPTS;++j){ inv0[j]=-1; inv1[j]=-1; }
    for (int k=0;k<n0;++k) inv0[map0[k]] = k;
    for (int k=0;k<n1;++k) inv1[map1[k]] = k;

    ngd_map map; ngd_map_init(&map);
    ngd_local_mapping lm; ngd_local_mapping_init(&lm, &map, /*mbMonocular=*/0);

    /* shared MPs (0..NSHARED-1) for covisibility */
    for (int j=0;j<NSHARED;++j){
        if (inv0[j]<0 || inv1[j]<0) continue;
        float pos[3]={pts[j][0],pts[j][1],pts[j][2]};
        ngd_mappoint *mp = ngd_mappoint_new(pos, kf0);
        ngd_mappoint_add_observation(mp, kf0, inv0[j], inv0[j]);
        ngd_mappoint_add_observation(mp, kf1, inv1[j], inv1[j]);
        kf0->mvpMapPoints[inv0[j]] = mp;
        kf1->mvpMapPoints[inv1[j]] = mp;
        ngd_mappoint_compute_distinctive_descriptors(mp);
        ngd_mappoint_update_normal_and_depth(mp);
        ngd_map_add_mappoint(&map, mp);
    }
    /* duplicate MPs (NSHARED..NPTS-1): kf0 owns mpA, kf1 owns mpB at same pos */
    ngd_mappoint *mpA[NPTS], *mpB[NPTS];
    int ndup = 0;
    for (int j=NSHARED;j<NPTS;++j){
        mpA[j]=NULL; mpB[j]=NULL;
        if (inv0[j]<0 || inv1[j]<0) continue;
        float pos[3]={pts[j][0],pts[j][1],pts[j][2]};
        ngd_mappoint *a = ngd_mappoint_new(pos, kf0);
        ngd_mappoint_add_observation(a, kf0, inv0[j], inv0[j]);
        kf0->mvpMapPoints[inv0[j]] = a;
        ngd_mappoint_compute_distinctive_descriptors(a);
        ngd_mappoint_update_normal_and_depth(a);
        ngd_map_add_mappoint(&map, a);
        mpA[j] = a;
        ngd_mappoint *b = ngd_mappoint_new(pos, kf1);
        ngd_mappoint_add_observation(b, kf1, inv1[j], inv1[j]);
        kf1->mvpMapPoints[inv1[j]] = b;
        ngd_mappoint_compute_distinctive_descriptors(b);
        ngd_mappoint_update_normal_and_depth(b);
        ngd_map_add_mappoint(&map, b);
        mpB[j] = b;
        ndup++;
    }
    ngd_map_add_keyframe(&map, kf0);
    ngd_map_add_keyframe(&map, kf1);
    ngd_keyframe_update_connections(kf0);
    ngd_keyframe_update_connections(kf1);
    CHECK(kf1->nConn > 0, "kf1 covisible with kf0 via shared MPs");
    int mps_before = map.nMPs;
    printf("  scene: %d MPs before fuse (%d duplicates), kf1.nConn=%d\n",
           mps_before, ndup, kf1->nConn);

    lm.currentKF = kf1;
    ngd_local_mapping_search_in_neighbors(&lm);

    int nA_bad = 0, nB_alive = 0, nB_both = 0, n_slot0 = 0, n_slot1 = 0;
    for (int j=NSHARED;j<NPTS;++j){
        if (!mpA[j] || !mpB[j]) continue;
        if (mpA[j]->mbBad) nA_bad++;
        if (!mpB[j]->mbBad){
            nB_alive++;
            int o0=0,o1=0;
            for (int o=0;o<mpB[j]->nObsRec;++o){
                if (mpB[j]->obs[o].kf==kf0) o0=1;
                if (mpB[j]->obs[o].kf==kf1) o1=1;
            }
            if (o0 && o1) nB_both++;
            if (kf0->mvpMapPoints[inv0[j]] == mpB[j]) n_slot0++;
            if (kf1->mvpMapPoints[inv1[j]] == mpB[j]) n_slot1++;
        }
    }
    printf("  %d duplicates: A_bad=%d, B_alive=%d, B_observed_by_both=%d, slot0->B=%d, slot1->B=%d\n",
           ndup, nA_bad, nB_alive, nB_both, n_slot0, n_slot1);

    CHECK(nA_bad == ndup, "every mpA replaced (bad)");
    CHECK(nB_alive == ndup, "every mpB survives");
    CHECK(nB_both == ndup, "every surviving mpB observed by both kf0 and kf1");
    CHECK(n_slot0 == ndup, "kf0 slot transferred to surviving mpB");
    CHECK(n_slot1 == ndup, "kf1 slot still points to mpB");

    /* bad (replaced) MPs swept from the map */
    int n_bad_in_map = 0;
    for (int i=0;i<map.nMPs;++i)
        if (map.mps[i] && map.mps[i]->mbBad) n_bad_in_map++;
    CHECK(n_bad_in_map == 0, "no bad MPs left in map (sweep)");
    CHECK(map.nMPs == mps_before - ndup, "map.nMPs decreased by #duplicates (mpA swept)");

    /* shared MPs untouched (still observed by both, not bad) */
    int shared_ok = 0;
    for (int j=0;j<NSHARED;++j){
        if (inv0[j]<0 || inv1[j]<0) continue;
        ngd_mappoint *mp = kf0->mvpMapPoints[inv0[j]];
        if (mp && !mp->mbBad && kf1->mvpMapPoints[inv1[j]]==mp) shared_ok++;
    }
    CHECK(shared_ok == NSHARED || shared_ok > 0, "shared covisibility MPs untouched");

    /* cleanup */
    for (int j=NSHARED;j<NPTS;++j) if (mpA[j]) ngd_mappoint_free(mpA[j]);   /* swept (bad) MPs */
    for (int i=0;i<map.nMPs;++i) ngd_mappoint_free(map.mps[i]);
    for (int i=0;i<map.nKFs;++i) ngd_keyframe_free(map.kfs[i]);
    ngd_local_mapping_free(&lm);
    ngd_map_free(&map);

    if (fails) { printf("test_fuse: %d FAIL\n", fails); return 1; }
    printf("test_fuse: PASS\n");
    return 0;
}
