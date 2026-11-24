/* test_createmps.c — CreateNewMapPoints triangulation unit tests.
 *
 * Validates ngd_local_mapping_create_new_map_points (LocalMapping.cc:396-720)
 * + ngd_search_for_triangulation + ngd_triangulate end-to-end:
 *   T1: 2 KFs share 20 tracked MPs (covisibility) + 20 untracked features each
 *       with identical cross-KF descriptors; create_new_map_points triangulates
 *       ~20 new MPs at ground-truth positions, observed by both KFs, in map+recent.
 *   T2: baseline < mb -> neighbour skipped, 0 new MPs.
 *   T3: one untracked feature's kf1 descriptor corrupted -> not triangulated.
 *
 * Requires the real ORB vocabulary (SKIPPED if not found) for the FeatureVector
 * used by SearchForTriangulation. Pure asserts. */
#include "ngd/orb.h"
#include "ngd/calib.h"
#include "ngd/se3.h"
#include "ngd/pinhole.h"
#include "ngd/keyframe.h"
#include "ngd/mappoint.h"
#include "ngd/map.h"
#include "ngd/localmapping.h"
#include "ngd/bow.h"

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
#define NPTS 40
#define NTRACK 20      /* first NTRACK points have a shared MP (covisibility) */

static ngd_bow_vocab *load_vocab(void)
{
    const char *bin_cands[] = {
        "../../../Vocabulary/ORBvoc.bin", "../../Vocabulary/ORBvoc.bin",
        "../../../../Vocabulary/ORBvoc.bin", "../Vocabulary/ORBvoc.bin",
        "Vocabulary/ORBvoc.bin", NULL };
    for (int i = 0; bin_cands[i]; ++i) {
        FILE *tf = fopen(bin_cands[i], "rb");
        if (tf) { fclose(tf); return ngd_bow_vocab_load_binary(bin_cands[i]); }
    }
    const char *txt_cands[] = {
        "../../../Vocabulary/ORBvoc.txt", "../../Vocabulary/ORBvoc.txt",
        "../../../../Vocabulary/ORBvoc.txt", "Vocabulary/ORBvoc.txt", NULL };
    for (int i = 0; txt_cands[i]; ++i) {
        FILE *tf = fopen(txt_cands[i], "r");
        if (tf) { fclose(tf); return ngd_bow_vocab_load_text(txt_cands[i]); }
    }
    return NULL;
}

static ngd_cam_ctx make_cam(void)
{
    ngd_orb_extractor ex; ngd_orb_init(&ex, 1000, 1.2f, 8, 20, 7);
    ngd_cam_ctx cam; ngd_cam_ctx_init(&cam, &ex, 535.4f, 539.2f, 320.1f, 247.6f, 0.0747f, 2.988f, W, H);
    return cam;
}

/* project world point through Tcw, fill kp.x/kp.y, uRight, depth; return 0 if not visible */
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

/* distinct per-point descriptor: byte0 = point index, rest 0 (legal ORB bytes,
 * identical across KFs for the same point -> same vocab node -> Hamming 0). */
static void make_desc(int point_idx, uint8_t desc[32]) {
    memset(desc, 0, 32);
    desc[0] = (uint8_t)point_idx;
}

/* Build a KF that observes all visible points among `pts` (in order). */
static ngd_keyframe *build_kf(uint64_t id, ngd_se3 pose, ngd_cam_ctx *cam,
                              const float pts[][3], int npts,
                              int *n_out)
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

/* map KF keypoint index -> original point index (first visible point is idx 0) */
static void build_index_map(ngd_cam_ctx *cam, ngd_se3 pose, const float pts[][3], int npts, int *idxmap) {
    int n=0;
    for (int j=0;j<npts;++j){
        float ux,uy,r,z;
        if (project_point(cam, pose, pts[j], &ux,&uy,&r,&z)) { idxmap[n++]=j; }
    }
}

int main(void)
{
    rng = 42u;
    ngd_bow_vocab *voc = load_vocab();
    if (!voc) { printf("test_createmps: SKIP (ORB vocabulary not found)\n"); return 0; }

    ngd_cam_ctx cam = make_cam();

    /* ---- scene: 40 points in front of both cameras ---- */
    static float pts[NPTS][3];
    for (int j=0;j<NPTS;++j){
        pts[j][0] = (frand()-0.5f)*1.4f;
        pts[j][1] = (frand()-0.5f)*1.0f;
        pts[j][2] = 1.8f + frand()*1.5f;
    }

    /* kf0 at origin (init), kf1 translated 0.15m in +x (baseline > mb=0.0747). */
    ngd_se3 P0 = ngd_se3_identity();
    ngd_se3 P1 = ngd_se3_from_rt(ngd_quat_to_matrix(ngd_quat_identity()), ngd_v3(0.15f, 0.0f, 0.0f));

    int n0, n1;
    ngd_keyframe *kf0 = build_kf(0, P0, &cam, pts, NPTS, &n0);
    ngd_keyframe *kf1 = build_kf(1, P1, &cam, pts, NPTS, &n1);
    ngd_keyframe_compute_bow(kf0, voc);
    ngd_keyframe_compute_bow(kf1, voc);

    /* index maps: KF keypoint idx -> original point idx */
    int map0[NPTS], map1[NPTS];
    build_index_map(&cam, P0, pts, NPTS, map0);
    build_index_map(&cam, P1, pts, NPTS, map1);
    /* inverse: point idx -> KF keypoint idx */
    int inv0[NPTS], inv1[NPTS];
    for (int j=0;j<NPTS;++j){ inv0[j]=-1; inv1[j]=-1; }
    for (int k=0;k<n0;++k) inv0[map0[k]] = k;
    for (int k=0;k<n1;++k) inv1[map1[k]] = k;

    /* seed NTRACK shared MPs (points 0..NTRACK-1) for covisibility */
    ngd_map map; ngd_map_init(&map);
    ngd_local_mapping lm; ngd_local_mapping_init(&lm, &map, /*mbMonocular=*/0);
    ngd_mappoint *tracked[NTRACK]; int ntracked=0;
    for (int j=0;j<NTRACK;++j){
        if (inv0[j]<0 || inv1[j]<0) continue;
        float pos[3]={pts[j][0],pts[j][1],pts[j][2]};
        ngd_mappoint *mp = ngd_mappoint_new(pos, kf0);
        ngd_mappoint_add_observation(mp, kf0, inv0[j], inv0[j]);   /* stereo -> nObs+=2 */
        ngd_mappoint_add_observation(mp, kf1, inv1[j], inv1[j]);
        kf0->mvpMapPoints[inv0[j]] = mp;
        kf1->mvpMapPoints[inv1[j]] = mp;
        ngd_mappoint_compute_distinctive_descriptors(mp);
        ngd_mappoint_update_normal_and_depth(mp);
        ngd_map_add_mappoint(&map, mp);
        tracked[ntracked++] = mp;
    }
    ngd_map_add_keyframe(&map, kf0);
    ngd_map_add_keyframe(&map, kf1);
    ngd_keyframe_update_connections(kf0);
    ngd_keyframe_update_connections(kf1);
    CHECK(kf1->nConn > 0, "kf1 has covisibility to kf0 via tracked MPs");
    printf("  scene: %d/%d KF0 pts, %d/%d KF1 pts, %d tracked MPs, kf1.nConn=%d\n",
           n0, NPTS, n1, NPTS, ntracked, kf1->nConn);

    /* ---- T1: create_new_map_points from kf1 ---- */
    lm.currentKF = kf1;
    int map_mps_before = map.nMPs;
    int recent_before = lm.nRecent;
    ngd_local_mapping_create_new_map_points(&lm);

    int n_new = map.nMPs - map_mps_before;
    int n_recent_new = lm.nRecent - recent_before;
    printf("  T1: new MPs = %d (recent +%d)\n", n_new, n_recent_new);
    CHECK(n_new >= 15, ">=15 new MPs triangulated (of 20 untracked)");
    CHECK(n_new <= 20, "<=20 new MPs (no over-triangulation)");
    CHECK(n_recent_new == n_new, "all new MPs pushed to recent list");

    /* verify each untracked point that got a MP is near GT, observed by both KFs */
    double max_err = 0; int n_verified = 0;
    for (int j=NTRACK; j<NPTS; ++j) {
        if (inv0[j]<0 || inv1[j]<0) continue;
        ngd_mappoint *mp0 = kf0->mvpMapPoints[inv0[j]];
        ngd_mappoint *mp1 = kf1->mvpMapPoints[inv1[j]];
        if (!mp0 || mp0 != mp1) continue;            /* only fully-triangulated ones */
        double dx=mp0->worldPos[0]-pts[j][0], dy=mp0->worldPos[1]-pts[j][1], dz=mp0->worldPos[2]-pts[j][2];
        double d=sqrt(dx*dx+dy*dy+dz*dz); if (d>max_err) max_err=d;
        /* observed by both KFs */
        int obs0=0,obs1=0;
        for (int o=0;o<mp0->nObsRec;++o){ if(mp0->obs[o].kf==kf0)obs0=1; if(mp0->obs[o].kf==kf1)obs1=1; }
        CHECK(obs0 && obs1, "new MP observed by both kf0 and kf1");
        n_verified++;
    }
    printf("  T1: verified %d new MPs vs GT, max pos err = %.4f m\n", n_verified, max_err);
    CHECK(n_verified >= 15, ">=15 new MPs verified against GT");
    CHECK(max_err < 0.02, "triangulated positions within 2cm of GT");

    /* tracked MPs untouched (still the original objects) */
    int tracked_intact = 0;
    for (int t=0;t<ntracked;++t) if (tracked[t]->mbBad==0) tracked_intact++;
    CHECK(tracked_intact == ntracked, "tracked MPs not corrupted");

    /* ---- T2: baseline too short -> 0 new ---- */
    {
        /* kf2 at 0.02m from kf0 (baseline < mb), sharing tracked MPs for covisibility */
        ngd_se3 P2 = ngd_se3_from_rt(ngd_quat_to_matrix(ngd_quat_identity()), ngd_v3(0.02f,0,0));
        int n2; ngd_keyframe *kf2 = build_kf(2, P2, &cam, pts, NPTS, &n2);
        ngd_keyframe_compute_bow(kf2, voc);
        for (int t=0;t<ntracked;++t){
            /* find the point index for this tracked MP */
            for (int j=0;j<NTRACK;++j){
                if (inv0[j]>=0 && kf0->mvpMapPoints[inv0[j]] == tracked[t]) {
                    /* kf2's keypoint for point j: recompute index */
                    float ux,uy,r,z;
                    if (project_point(&cam, P2, pts[j], &ux,&uy,&r,&z)) {
                        /* locate in kf2 by pixel */
                        for (int k=0;k<kf2->N;++k) if (fabsf(kf2->keys[k].x-ux)<0.5f && fabsf(kf2->keys[k].y-uy)<0.5f) {
                            kf2->mvpMapPoints[k] = tracked[t];
                            break;
                        }
                    }
                    break;
                }
            }
        }
        ngd_map_add_keyframe(&map, kf2);
        ngd_keyframe_update_connections(kf2);
        ngd_local_mapping lm2; ngd_local_mapping_init(&lm2, &map, 0);
        lm2.currentKF = kf2;
        int before = map.nMPs;
        ngd_local_mapping_create_new_map_points(&lm2);
        int created = map.nMPs - before;
        printf("  T2: baseline<mb -> created %d new MPs\n", created);
        CHECK(created == 0, "short baseline produces 0 new MPs");
        /* cleanup kf2 (MPs freed via map) */
        /* (kf2 freed at end via map.kfs) */
        ngd_local_mapping_free(&lm2);
    }

    /* ---- T3: descriptor corruption -> that point not triangulated ---- */
    {
        /* rebuild a fresh pair with point NTRACK's kf1 descriptor corrupted */
        static float pts2[NPTS][3];
        memcpy(pts2, pts, sizeof(pts));
        ngd_keyframe *a0 = build_kf(10, P0, &cam, pts2, NPTS, &n0);
        ngd_keyframe *a1 = build_kf(11, P1, &cam, pts2, NPTS, &n1);
        /* corrupt a1's keypoint for point NTRACK (idx = inv1[NTRACK]) */
        if (inv1[NTRACK] >= 0) {
            uint8_t *d = a1->descriptors + (size_t)inv1[NTRACK]*32;
            memset(d, 0xFF, 32);   /* maximally distant from a0's byte0=NTRACK */
        }
        ngd_keyframe_compute_bow(a0, voc);
        ngd_keyframe_compute_bow(a1, voc);
        /* minimal covisibility: seed a few shared MPs among points 0..NTRACK-1 */
        ngd_map m2; ngd_map_init(&m2);
        ngd_local_mapping lm3; ngd_local_mapping_init(&lm3, &m2, 0);
        int im0[NPTS], im1[NPTS]; build_index_map(&cam,P0,pts2,NPTS,im0); build_index_map(&cam,P1,pts2,NPTS,im1);
        int iim0[NPTS], iim1[NPTS]; for(int j=0;j<NPTS;++j){iim0[j]=-1;iim1[j]=-1;}
        for(int k=0;k<n0;++k) iim0[im0[k]]=k;
        for(int k=0;k<n1;++k) iim1[im1[k]]=k;
        for (int j=0;j<NTRACK;++j){
            if (iim0[j]<0||iim1[j]<0) continue;
            float pos[3]={pts2[j][0],pts2[j][1],pts2[j][2]};
            ngd_mappoint *mp = ngd_mappoint_new(pos, a0);
            ngd_mappoint_add_observation(mp, a0, iim0[j], iim0[j]);
            ngd_mappoint_add_observation(mp, a1, iim1[j], iim1[j]);
            a0->mvpMapPoints[iim0[j]] = mp; a1->mvpMapPoints[iim1[j]] = mp;
            ngd_mappoint_compute_distinctive_descriptors(mp);
            ngd_mappoint_update_normal_and_depth(mp);
            ngd_map_add_mappoint(&m2, mp);
        }
        ngd_map_add_keyframe(&m2, a0); ngd_map_add_keyframe(&m2, a1);
        ngd_keyframe_update_connections(a0); ngd_keyframe_update_connections(a1);
        lm3.currentKF = a1;
        ngd_local_mapping_create_new_map_points(&lm3);
        /* point NTRACK should NOT have a MP in a0 (corrupted match rejected) */
        int corrupted_has_mp = (iim0[NTRACK]>=0 && a0->mvpMapPoints[iim0[NTRACK]] != NULL) ? 1 : 0;
        printf("  T3: corrupted point has MP? %d (expect 0)\n", corrupted_has_mp);
        CHECK(corrupted_has_mp == 0, "descriptor-corrupted point not triangulated");
        /* but other untracked points still triangulated */
        int other=0;
        for (int j=NTRACK+1;j<NPTS;++j) if (iim0[j]>=0 && a0->mvpMapPoints[iim0[j]]!=NULL) other++;
        CHECK(other >= 10, "other untracked points still triangulated");
        for (int j=0;j<m2.nMPs;++j) ngd_mappoint_free(m2.mps[j]);
        for (int i=0;i<m2.nKFs;++i) ngd_keyframe_free(m2.kfs[i]);
        ngd_local_mapping_free(&lm3);
        ngd_map_free(&m2);
    }

    /* cleanup main */
    for (int j=0;j<map.nMPs;++j) ngd_mappoint_free(map.mps[j]);
    for (int i=0;i<map.nKFs;++i) ngd_keyframe_free(map.kfs[i]);
    ngd_local_mapping_free(&lm);
    ngd_map_free(&map);
    ngd_bow_vocab_free(voc);

    if (fails == 0) { printf("test_createmps: PASS\n"); return 0; }
    printf("test_createmps: %d FAILURES\n", fails);
    return 1;
}
