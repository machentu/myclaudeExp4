/* test_ba.c — LocalBundleAdjustment (Schur LM BA) unit tests.
 *
 * Validates the pure-C ngd_ba_optimize core and the ngd_local_ba wrapper:
 *   1. noise-free problem does not drift (self-consistency of linearisation)
 *   2. mono BA recovers ground-truth poses + points from a perturbation
 *   3. stereo BA recovers ground-truth
 *   4. gross-outlier edges are flagged and inliers still converge
 *   5. ngd_local_ba end-to-end on a small synthetic map (KF/MP/covisibility)
 *
 * No external deps (no ORB vocabulary needed). Pure asserts. */
#include "ngd/orb.h"
#include "ngd/calib.h"
#include "ngd/se3.h"
#include "ngd/pinhole.h"
#include "ngd/ba.h"
#include "ngd/keyframe.h"
#include "ngd/mappoint.h"
#include "ngd/map.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { if(!(cond)){ printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); fails++; } } while(0)

static unsigned int rng = 42u;
static float frand(void){ rng = rng*1664525u + 1013904223u; return (rng >> 8) * (1.0f/16777216.0f); }
static float frandn(void){ return (frand()+frand()+frand()+frand()-2.0f); } /* ~N(0,1/√12*...) rough */

#define W 640
#define H 480

/* ---- shared camera ---- */
static ngd_cam_ctx make_cam(void)
{
    ngd_orb_extractor ex; ngd_orb_init(&ex, 1000, 1.2f, 8, 20, 7);
    ngd_cam_ctx cam; ngd_cam_ctx_init(&cam, &ex, 535.4f, 539.2f, 320.1f, 247.6f, 0.0747f, 2.988f, W, H);
    return cam;
}

/* project a 3D world point through Tcw -> (u,v[,uR]); returns 0 if behind cam */
static int project_obs(ngd_cam_ctx *cam, ngd_se3 Tcw, const float Xw[3], int stereo, float obs[3])
{
    ngd_vec3 Xc = ngd_se3_map(Tcw, ngd_v3(Xw[0],Xw[1],Xw[2]));
    if (Xc.z <= 0.1f) return 0;
    if (stereo) {
        float o[3]; ngd_pinhole_project_stereo(cam->cam, cam->mbf, Xc, o);
        obs[0]=o[0]; obs[1]=o[1]; obs[2]=o[2];
    } else {
        float o[2]; ngd_pinhole_project(cam->cam, Xc, o);
        obs[0]=o[0]; obs[1]=o[1]; obs[2]=0.f;
    }
    if (obs[0]<4||obs[0]>W-4||obs[1]<4||obs[1]>H-4) return 0;
    return 1;
}

/* build a synthetic scene: NPTS points, NPOSES poses (pose 0 = identity). */
typedef struct {
    ngd_se3 poses[8];
    float   pts[256][3];
    int npos, npts;
} scene;

static scene make_scene(int npos, int npts)
{
    scene s; s.npos=npos; s.npts=npts;
    s.poses[0]=ngd_se3_identity();
    for (int i=1;i<npos;++i){
        /* small lateral/forward motion */
        ngd_vec3 t = ngd_v3(0.05f*(float)i + 0.01f*frandn(), 0.02f*frandn(), 0.03f*(float)i);
        ngd_quat q = ngd_quat_from_axis_angle(ngd_v3_normalize(ngd_v3(frandn(),frandn(),frandn())), 0.003f*frandn());
        s.poses[i] = ngd_se3_from_rt(ngd_quat_to_matrix(q), t);
    }
    for (int j=0;j<npts;++j){
        s.pts[j][0] = (frand()-0.5f)*1.6f;
        s.pts[j][1] = (frand()-0.5f)*1.2f;
        s.pts[j][2] = 1.5f + frand()*2.0f;
    }
    return s;
}

/* perturb a pose by (dt, drot) */
static ngd_se3 perturb_pose(ngd_se3 T, float dt, float drot)
{
    ngd_vec3 t = ngd_v3(T.t.x + dt*frandn(), T.t.y + dt*frandn(), T.t.z + dt*frandn());
    ngd_quat q = ngd_quat_from_axis_angle(ngd_v3_normalize(ngd_v3(frandn(),frandn(),frandn())), drot*frandn());
    ngd_se3 dT = ngd_se3_from_rt(ngd_quat_to_matrix(q), ngd_v3(0,0,0));
    return ngd_se3_multiply(dT, ngd_se3_from_rt(ngd_quat_to_matrix(T.q), t));
}

static double pose_rot_err(ngd_se3 a, ngd_se3 b)
{
    /* relative rotation angle in radians */
    ngd_se3 r = ngd_se3_multiply(ngd_se3_inverse(a), b);
    ngd_mat3 R = ngd_quat_to_matrix(r.q);
    double tr = R.m[0]+R.m[4]+R.m[8];
    if (tr>1) tr=1; if (tr<-1) tr=-1;
    return acos(tr);
}
static double pose_t_err(ngd_se3 a, ngd_se3 b)
{
    double dx=a.t.x-b.t.x, dy=a.t.y-b.t.y, dz=a.t.z-b.t.z;
    return sqrt(dx*dx+dy*dy+dz*dz);
}

/* ------------------------------------------------------------------ *
 * Build a flat ngd_ba_problem from a scene (obs = project(GT)). pose 0 fixed.
 * stereo: 0 mono / 1 stereo.  Returns #edges. perturb: apply to non-fixed
 * poses + points before optimisation (so the optimiser starts off GT).
 * ------------------------------------------------------------------ */
static int build_problem(ngd_ba_problem *p, scene *s, ngd_cam_ctx *cam,
                         int stereo, int nfixed, int perturb, int inject_outliers,
                         ngd_se3 *pose_out, ngd_vec3 *pts_out, int *outlier)
{
    int npos=s->npos, npts=s->npts;
    ngd_se3 *poses = (ngd_se3*)malloc((size_t)npos*sizeof(ngd_se3));
    int *pose_fixed = (int*)calloc((size_t)npos,sizeof(int));
    for (int i=0;i<npos && i<nfixed;++i) pose_fixed[i]=1;
    for (int i=0;i<npos;++i) poses[i]=s->poses[i];
    if (perturb) for (int i=0;i<npos;++i) if(!pose_fixed[i]) poses[i]=perturb_pose(poses[i], 0.015f, 0.004f);

    ngd_vec3 *points = (ngd_vec3*)malloc((size_t)npts*sizeof(ngd_vec3));
    for (int j=0;j<npts;++j){
        points[j]=ngd_v3(s->pts[j][0],s->pts[j][1],s->pts[j][2]);
        if (perturb){ points[j].x+=0.01f*frandn(); points[j].y+=0.01f*frandn(); points[j].z+=0.01f*frandn(); }
    }

    /* count edges */
    int capE=npos*npts;
    ngd_ba_edge *edges = (ngd_ba_edge*)malloc((size_t)capE*sizeof(ngd_ba_edge));
    int nE=0;
    for (int i=0;i<npos;++i) for (int j=0;j<npts;++j){
        float obs[3];
        if (!project_obs(cam, s->poses[i], s->pts[j], stereo, obs)) continue; /* GT visibility */
        ngd_ba_edge *E=&edges[nE];
        E->pose_idx=i; E->point_idx=j; E->stereo=stereo; E->octave=0;
        E->obs[0]=obs[0]; E->obs[1]=obs[1]; E->obs[2]=obs[2];
        nE++;
    }
    if (inject_outliers) {
        /* corrupt the first 3 edges by a large pixel offset */
        for (int e=0;e<3 && e<nE;++e){ edges[e].obs[0]+=60.0f; edges[e].obs[1]+=55.0f; if(stereo) edges[e].obs[2]+=50.0f; }
    }

    p->poses=poses; p->pose_fixed=pose_fixed; p->n_poses=npos;
    p->points=points; p->n_points=npts;
    p->edges=edges; p->n_edges=nE;
    p->cam=&cam->cam; p->bf=cam->mbf; p->calib=cam;
    p->outlier=outlier;
    if (pose_out) for (int i=0;i<npos;++i) pose_out[i]=poses[i];
    if (pts_out) for (int j=0;j<npts;++j) pts_out[j]=points[j];
    return nE;
}

static void free_problem(ngd_ba_problem *p)
{
    free(p->poses); free(p->pose_fixed); free(p->points); free(p->edges);
}

/* ------------------------------------------------------------------ */
int main(void)
{
    rng = 42u;
    ngd_cam_ctx cam = make_cam();

    /* ---- Test 1: noise-free no drift ---- */
    {
        scene s = make_scene(3, 40);
        ngd_ba_problem pb; int outlier[1024];
        int nE = build_problem(&pb, &s, &cam, /*stereo=*/0, /*nfixed=*/2, /*perturb=*/0, 0, NULL, NULL, outlier);
        double chi = ngd_ba_optimize(&pb, 10); (void)chi;
        double maxp=0, maxpt=0;
        for (int i=0;i<s.npos;++i){ double e=pose_rot_err(pb.poses[i], s.poses[i]); if(e>maxp)maxp=e;
                                    double et=pose_t_err(pb.poses[i], s.poses[i]); if(et>maxpt)maxpt=et; }
        for (int j=0;j<s.npts;++j){ double dx=pb.points[j].x-s.pts[j][0], dy=pb.points[j].y-s.pts[j][1], dz=pb.points[j].z-s.pts[j][2];
                                    double d=sqrt(dx*dx+dy*dy+dz*dz); if(d>maxpt)maxpt=d; }
        printf("  T1 noise-free: max rot=%.2e max t/pt=%.2e (%d edges)\n", maxp, maxpt, nE);
        CHECK(maxp < 1e-5, "noise-free poses do not drift");
        CHECK(maxpt < 1e-4, "noise-free points do not drift");
        free_problem(&pb);
    }

    /* ---- Test 2: mono BA recovery ---- */
    {
        rng = 7u;
        scene s = make_scene(3, 50);
        ngd_se3 pose_init[8]; ngd_vec3 pts_init[256]; int outlier[2048];
        ngd_ba_problem pb;
        int nE = build_problem(&pb, &s, &cam, /*stereo=*/0, /*nfixed=*/2, /*perturb=*/1, 0, pose_init, pts_init, outlier);
        ngd_ba_optimize(&pb, 10);
        double maxre=0, maxte=0, maxpe=0; int nin=0;
        for (int e=0;e<nE;++e) if(!outlier[e]) nin++;
        for (int i=0;i<s.npos;++i){ double e=pose_rot_err(pb.poses[i], s.poses[i]); if(e>maxre)maxre=e;
                                    double et=pose_t_err(pb.poses[i], s.poses[i]); if(et>maxte)maxte=et; }
        for (int j=0;j<s.npts;++j){ double dx=pb.points[j].x-s.pts[j][0], dy=pb.points[j].y-s.pts[j][1], dz=pb.points[j].z-s.pts[j][2];
                                    double d=sqrt(dx*dx+dy*dy+dz*dz); if(d>maxpe)maxpe=d; }
        printf("  T2 mono recovery: rot=%.2e t=%.2e pt=%.2e inliers=%d/%d\n", maxre, maxte, maxpe, nin, nE);
        CHECK(maxre < 1e-3, "mono poses recovered");
        CHECK(maxte < 5e-3, "mono translation recovered");
        CHECK(maxpe < 5e-3, "mono points recovered");
        CHECK(nin == nE, "all edges inlier (clean mono)");
        free_problem(&pb);
    }

    /* ---- Test 3: stereo BA recovery ---- */
    {
        rng = 19u;
        scene s = make_scene(3, 50);
        ngd_se3 pose_init[8]; ngd_vec3 pts_init[256]; int outlier[2048];
        ngd_ba_problem pb;
        int nE = build_problem(&pb, &s, &cam, /*stereo=*/1, /*nfixed=*/1, /*perturb=*/1, 0, pose_init, pts_init, outlier);
        ngd_ba_optimize(&pb, 10);
        double maxre=0, maxte=0, maxpe=0; int nin=0;
        for (int e=0;e<nE;++e) if(!outlier[e]) nin++;
        for (int i=0;i<s.npos;++i){ double e=pose_rot_err(pb.poses[i], s.poses[i]); if(e>maxre)maxre=e;
                                    double et=pose_t_err(pb.poses[i], s.poses[i]); if(et>maxte)maxte=et; }
        for (int j=0;j<s.npts;++j){ double dx=pb.points[j].x-s.pts[j][0], dy=pb.points[j].y-s.pts[j][1], dz=pb.points[j].z-s.pts[j][2];
                                    double d=sqrt(dx*dx+dy*dy+dz*dz); if(d>maxpe)maxpe=d; }
        printf("  T3 stereo recovery: rot=%.2e t=%.2e pt=%.2e inliers=%d/%d\n", maxre, maxte, maxpe, nin, nE);
        CHECK(maxre < 1e-3, "stereo poses recovered");
        CHECK(maxte < 5e-3, "stereo translation recovered");
        CHECK(maxpe < 5e-3, "stereo points recovered");
        CHECK(nin == nE, "all edges inlier (clean stereo)");
        free_problem(&pb);
    }

    /* ---- Test 4: outlier rejection ---- */
    {
        rng = 31u;
        scene s = make_scene(3, 50);
        ngd_se3 pose_init[8]; ngd_vec3 pts_init[256]; int outlier[2048];
        ngd_ba_problem pb;
        int nE = build_problem(&pb, &s, &cam, /*stereo=*/0, /*nfixed=*/2, /*perturb=*/1, /*inject=*/1, pose_init, pts_init, outlier);
        ngd_ba_optimize(&pb, 10);
        int nout=0; for (int e=0;e<nE;++e) if(outlier[e]) nout++;
        /* inliers should still recover GT reasonably */
        double maxre=0, maxte=0;
        for (int i=0;i<s.npos;++i){ double e=pose_rot_err(pb.poses[i], s.poses[i]); if(e>maxre)maxre=e;
                                    double et=pose_t_err(pb.poses[i], s.poses[i]); if(et>maxte)maxte=et; }
        printf("  T4 outliers: flagged=%d  rot=%.2e t=%.2e\n", nout, maxre, maxte);
        CHECK(nout >= 3, "injected gross outliers flagged");
        CHECK(nout <= 6, "not too many false outliers");
        CHECK(maxre < 5e-3, "inlier poses still recovered with outliers");
        free_problem(&pb);
    }

    /* ---- Test 5: ngd_local_ba end-to-end on a small synthetic map ---- */
    {
        rng = 99u;
        scene s = make_scene(3, 24);
        ngd_cam_ctx cam2 = make_cam();

        /* build 3 KFs (kf0 init/fixed, kf1, kf2) sharing all points as MPs */
        ngd_keyframe *kfs[3];
        int NKP = s.npts;
        for (int i=0;i<3;++i){
            ngd_keypoint *kps = (ngd_keypoint*)malloc((size_t)NKP*sizeof(ngd_keypoint));
            uint8_t *desc = (uint8_t*)calloc((size_t)NKP*32,1);
            float *ur = (float*)malloc((size_t)NKP*sizeof(float));
            float *dep = (float*)malloc((size_t)NKP*sizeof(float));
            int n=0;
            for (int j=0;j<NKP;++j){
                float obs[3];
                if (!project_obs(&cam2, s.poses[i], s.pts[j], /*stereo=*/1, obs)) continue;
                kps[n].x=obs[0]; kps[n].y=obs[1]; kps[n].octave=0; kps[n].angle=0; kps[n].response=1; kps[n].size=31;
                float Z = ngd_se3_map(s.poses[i], ngd_v3(s.pts[j][0],s.pts[j][1],s.pts[j][2])).z;
                ur[n]=obs[2]; dep[n]=Z;
                n++;
            }
            ngd_se3 Tp = s.poses[i];
            if (i>0) Tp = perturb_pose(Tp, 0.02f, 0.005f);   /* start off GT */
            kfs[i] = ngd_keyframe_new((uint64_t)i, Tp, &cam2, n, kps, desc, ur, dep);
            free(kps); free(desc); free(ur); free(dep);
        }
        /* shared MPs: each MP observed by all 3 KFs (at its index in each KF) */
        ngd_mappoint **mps = (ngd_mappoint**)malloc((size_t)NKP*sizeof(ngd_mappoint*));
        int nmp=0;
        /* For each original point j, find its leftIdx in each KF (the order we
         * added is preserved by project_obs visibility, but some points drop out;
         * re-derive index by matching projected pixel). */
        for (int j=0;j<NKP;++j){
            /* locate this point's index in each KF by matching uRight + pixel */
            int idx[3]={-1,-1,-1};
            for (int i=0;i<3;++i){
                for (int k=0;k<kfs[i]->N;++k){
                    float obs[3];
                    if (!project_obs(&cam2, s.poses[i], s.pts[j], 1, obs)) break;
                    if (fabsf(kfs[i]->uRight[k]-obs[2])<0.5f && fabsf(kfs[i]->keys[k].x-obs[0])<0.5f
                        && fabsf(kfs[i]->keys[k].y-obs[1])<0.5f){ idx[i]=k; break; }
                }
            }
            if (idx[0]<0||idx[1]<0||idx[2]<0) continue;
            float pos[3]={s.pts[j][0],s.pts[j][1],s.pts[j][2]};
            pos[0]+=0.01f*frandn(); pos[1]+=0.01f*frandn(); pos[2]+=0.01f*frandn(); /* perturb */
            ngd_mappoint *mp = ngd_mappoint_new(pos, kfs[0]);
            for (int i=0;i<3;++i){
                ngd_mappoint_add_observation(mp, kfs[i], idx[i], /*rightIdx=*/-1);
                kfs[i]->mvpMapPoints[idx[i]] = mp;
            }
            ngd_mappoint_compute_distinctive_descriptors(mp);
            ngd_mappoint_update_normal_and_depth(mp);
            mps[nmp++]=mp;
        }

        ngd_map map; ngd_map_init(&map);
        for (int i=0;i<3;++i) ngd_map_add_keyframe(&map, kfs[i]);
        for (int j=0;j<nmp;++j) ngd_map_add_mappoint(&map, mps[j]);
        for (int i=0;i<3;++i) ngd_keyframe_update_connections(kfs[i]);

        /* record pre-BA pose of kf1 to confirm it changes */
        ngd_se3 kf1_pre = kfs[1]->pose;
        ngd_se3 kf0_pre = kfs[0]->pose;

        int rc = ngd_local_ba(kfs[2], &map, NULL);
        CHECK(rc == 0, "ngd_local_ba returns 0 (not aborted)");

        double d1 = pose_t_err(kfs[1]->pose, kf1_pre);
        double d0 = pose_t_err(kfs[0]->pose, kf0_pre);
        double kf1_gt = pose_t_err(kfs[1]->pose, s.poses[1]);
        double kf2_gt = pose_t_err(kfs[2]->pose, s.poses[2]);
        printf("  T5 wrapper: rc=%d kf1 moved=%.4f kf0 moved=%.4f kf1_gt_err=%.4f kf2_gt_err=%.4f nMP=%d\n",
               rc, d1, d0, kf1_gt, kf2_gt, nmp);
        CHECK(d0 < 1e-6, "fixed kf0 pose unchanged");
        CHECK(d1 > 1e-4, "non-fixed kf1 pose updated");
        CHECK(kf1_gt < 0.02, "kf1 converges toward GT");
        CHECK(kf2_gt < 0.02, "kf2 converges toward GT");

        for (int j=0;j<nmp;++j) ngd_mappoint_free(mps[j]);
        for (int i=0;i<3;++i) ngd_keyframe_free(kfs[i]);
        free(mps);
        ngd_map_free(&map);
    }

    if (fails == 0) { printf("test_ba: PASS\n"); return 0; }
    printf("test_ba: %d FAILURES\n", fails);
    return 1;
}
