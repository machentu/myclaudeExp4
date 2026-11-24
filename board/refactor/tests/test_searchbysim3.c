/* test_searchbysim3.c — SearchBySim3 (ORBmatcher.cc:1461-1678).
 *
 * 2 KFs observe the same N world points; both have a MapPoint at every visible
 * keypoint, each point's descriptor keyed by its world index (so the kf1 MP and
 * kf2 MP for the same point share a descriptor -> Hamming 0). vpMatches12 starts
 * empty; ngd_search_by_sim3 with the exact relative Sim3 S12 = T1w*T2w^{-1}
 * should recover the cross-KF correspondences by projection + agreement.
 */
#include "ngd/orb.h"
#include "ngd/calib.h"
#include "ngd/se3.h"
#include "ngd/sim3.h"
#include "ngd/pinhole.h"
#include "ngd/keyframe.h"
#include "ngd/mappoint.h"
#include "ngd/matcher.h"

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
#define NPTS 40

static ngd_cam_ctx make_cam(void)
{
    ngd_orb_extractor ex; ngd_orb_init(&ex, 1000, 1.2f, 8, 20, 7);
    ngd_cam_ctx cam; ngd_cam_ctx_init(&cam, &ex, 535.4f, 539.2f, 320.1f, 247.6f, 0.0747f, 2.988f, W, H);
    return cam;
}

static int project_point(ngd_cam_ctx *cam, ngd_se3 Tcw, const float Xw[3],
                         float *ux, float *uy, float *zout)
{
    ngd_vec3 Xc = ngd_se3_map(Tcw, ngd_v3(Xw[0],Xw[1],Xw[2]));
    if (Xc.z <= 0.1f) return 0;
    float u = cam->cam.fx*Xc.x/Xc.z + cam->cam.cx;
    float v = cam->cam.fy*Xc.y/Xc.z + cam->cam.cy;
    if (u<8||u>W-8||v<8||v>H-8) return 0;
    *ux=u; *uy=v; *zout = Xc.z;
    return 1;
}

static void make_desc(int point_idx, uint8_t desc[32]) {
    memset(desc, 0, 32);
    desc[0] = (uint8_t)point_idx;   /* unique per world point */
}

/* Build a KF with a keypoint per visible point; record the world-index of each
 * kept keypoint in wpidx_out. */
static ngd_keyframe *build_kf(uint64_t id, ngd_se3 pose, ngd_cam_ctx *cam,
                              const float pts[][3], int npts, int *wpidx_out, int *n_out)
{
    ngd_keypoint *kps = (ngd_keypoint*)malloc((size_t)npts*sizeof(ngd_keypoint));
    uint8_t *desc = (uint8_t*)calloc((size_t)npts*32, 1);
    float *ur = (float*)malloc((size_t)npts*sizeof(float));
    float *dep = (float*)malloc((size_t)npts*sizeof(float));
    int n=0;
    for (int j=0;j<npts;++j){
        float ux,uy,z;
        if (!project_point(cam, pose, pts[j], &ux,&uy,&z)) continue;
        kps[n].x=ux; kps[n].y=uy; kps[n].octave=0; kps[n].angle=0; kps[n].response=1; kps[n].size=31;
        ur[n]=-1; dep[n]=z;
        make_desc(j, desc + (size_t)n*32);
        wpidx_out[n]=j;
        n++;
    }
    ngd_keyframe *kf = ngd_keyframe_new(id, pose, cam, n, kps, desc, ur, dep);
    ngd_keyframe_assign_grid(kf);
    free(kps); free(desc); free(ur); free(dep);
    *n_out = n;
    return kf;
}

/* attach an MP at every keypoint of kf, descriptor keyed by wpidx[] */
static void attach_mps(ngd_keyframe *kf, const float pts[][3], const int *wpidx) {
    for (int k=0;k<kf->N;++k){
        int j = wpidx[k];
        float pos[3]={pts[j][0],pts[j][1],pts[j][2]};
        ngd_mappoint *mp = ngd_mappoint_new(pos, kf);
        ngd_mappoint_add_observation(mp, kf, k, -1);
        kf->mvpMapPoints[k] = mp;
        make_desc(j, mp->descriptor);
        ngd_mappoint_update_normal_and_depth(mp);
    }
}

int main(void)
{
    rng = 7u;
    ngd_cam_ctx cam = make_cam();

    static float pts[NPTS][3];
    for (int j=0;j<NPTS;++j){
        pts[j][0] = (frand()-0.5f)*1.2f;
        pts[j][1] = (frand()-0.5f)*0.8f;
        pts[j][2] = 1.6f + frand()*0.6f;
    }

    ngd_se3 P1 = ngd_se3_identity();
    ngd_se3 P2 = ngd_se3_from_rt(ngd_quat_to_matrix(ngd_quat_identity()), ngd_v3(0.2f, 0.0f, 0.0f));
    int wp1[NPTS], wp2[NPTS], n1, n2;
    ngd_keyframe *kf1 = build_kf(1, P1, &cam, pts, NPTS, wp1, &n1);
    ngd_keyframe *kf2 = build_kf(2, P2, &cam, pts, NPTS, wp2, &n2);
    attach_mps(kf1, pts, wp1);
    attach_mps(kf2, pts, wp2);

    /* relative Sim3 S12 = T1w * T2w^{-1}  (cam2 -> cam1), scale 1 (RGBD). */
    ngd_se3 e21 = ngd_se3_inverse(P2);
    ngd_sim3 S12; S12.q = e21.q; S12.t = e21.t; S12.s = 1.0f;

    /* vpMatches12[kf1 keypoint] -> kf2 MP (NULL initially). */
    ngd_mappoint **vm = (ngd_mappoint**)calloc((size_t)n1, sizeof(ngd_mappoint*));

    int nFound = ngd_search_by_sim3(kf1, kf2, vm, S12, 3.0f);
    printf("  nFound=%d (kf1.N=%d)\n", nFound, n1);

    /* verify: every recovered match links kf1 keypoint k to the kf2 MP of the
     * SAME world point (descriptor byte0 equal). */
    int nCorrect = 0;
    for (int k=0;k<n1;++k){
        ngd_mappoint *m = vm[k];
        if (!m) continue;
        /* m is a kf2 MP; find which kf2 keypoint it belongs to */
        int k2 = -1;
        for (int o=0;o<m->nObsRec;++o) if (m->obs[o].kf==kf2) { k2=m->obs[o].leftIdx; break; }
        if (k2 < 0) continue;
        if (wp1[k] == wp2[k2]) nCorrect++;
    }
    CHECK(nFound >= 20, "enough matches found");
    CHECK(nCorrect == nFound, "all matches link same world point");

    /* ---- corrupt one kf2 descriptor: that point must NOT be matched ---- */
    {
        /* pick kf2 keypoint 0, corrupt its MP descriptor */
        ngd_mappoint *bad = kf2->mvpMapPoints[0];
        memset(bad->descriptor, 0xFF, 32);
        int jbad = wp2[0];
        /* rerun with fresh vm */
        for (int k=0;k<n1;++k) vm[k]=NULL;
        int nf = ngd_search_by_sim3(kf1, kf2, vm, S12, 3.0f);
        /* the kf1 keypoint whose world point == jbad must now be unmatched */
        int still_matched = 0;
        for (int k=0;k<n1;++k){
            if (wp1[k]==jbad && vm[k]!=NULL) still_matched++;
        }
        CHECK(still_matched == 0, "corrupted-descriptor point not matched");
        CHECK(nf >= 18, "still finds the rest after corruption");
    }

    /* cleanup */
    for (int k=0;k<kf1->N;++k) ngd_mappoint_free(kf1->mvpMapPoints[k]);
    for (int k=0;k<kf2->N;++k) ngd_mappoint_free(kf2->mvpMapPoints[k]);
    ngd_keyframe_free(kf1); ngd_keyframe_free(kf2);
    free(vm);

    if (fails) { printf("test_searchbysim3: %d FAIL\n", fails); return 1; }
    printf("test_searchbysim3: PASS\n");
    return 0;
}
