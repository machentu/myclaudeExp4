/* test_ba_sparse.c — block-sparse pose solver (ba_sparse.c) tests.
 *
 * 1. noise-free problem does not drift (through the sparse dispatcher)
 * 2. dense vs sparse equivalence on a battery of random problems
 *    (mono/stereo/mixed octaves/fixed poses/outliers/degenerate behind-camera
 *    edge): same final chi2, poses, points and outlier flags
 * 3. sparse recovery from a perturbation vs ground truth
 * 4. all-poses-fixed (dim_p == 0) equivalence
 * 5. larger chain problem (nb~28, banded covisibility): equivalence +
 *    sparsity sanity (nnzA well below dense, factor mults below dense LU)
 * 6. dispatcher (NGD_BA_SPARSE) routes to the sparse optimiser
 *
 * No external deps. Pure asserts. */
#include "ngd/orb.h"
#include "ngd/calib.h"
#include "ngd/se3.h"
#include "ngd/pinhole.h"
#include "ngd/ba.h"
#include "ngd/ba_sparse.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { if(!(cond)){ printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); fails++; } } while(0)

static unsigned int rng = 42u;
static float frand(void){ rng = rng*1664525u + 1013904223u; return (rng >> 8) * (1.0f/16777216.0f); }
static float frandn(void){ return (frand()+frand()+frand()+frand()-2.0f); }

#define W 640
#define H 480
#define MAXPOS 64

static ngd_cam_ctx make_cam(void)
{
    ngd_orb_extractor ex; ngd_orb_init(&ex, 1000, 1.2f, 8, 20, 7);
    ngd_cam_ctx cam; ngd_cam_ctx_init(&cam, &ex, 535.4f, 539.2f, 320.1f, 247.6f, 0.0747f, 2.988f, W, H);
    return cam;
}

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

static double pose_rot_err(ngd_se3 a, ngd_se3 b)
{
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

static ngd_se3 perturb_pose(ngd_se3 T, float dt, float drot)
{
    ngd_vec3 t = ngd_v3(T.t.x + dt*frandn(), T.t.y + dt*frandn(), T.t.z + dt*frandn());
    ngd_quat q = ngd_quat_from_axis_angle(ngd_v3_normalize(ngd_v3(frandn(),frandn(),frandn())), drot*frandn());
    ngd_se3 dT = ngd_se3_from_rt(ngd_quat_to_matrix(q), ngd_v3(0,0,0));
    return ngd_se3_multiply(dT, ngd_se3_from_rt(ngd_quat_to_matrix(T.q), t));
}

/* ------------------------------------------------------------------ *
 * Random chain problem: npos poses (pose 0 identity, motion along +z with
 * small drift), points sampled by unprojecting a pixel in an anchor KF at
 * random depth, observed by KFs in a +-3 window (banded block structure
 * like real covisibility). stereo_mode: 0 mono / 1 stereo / 2 mixed.
 * Observations always come from the GT chain; the initial estimate in the
 * problem is optionally perturbed. gt_out (may be NULL) receives the GT.
 * ------------------------------------------------------------------ */
static void gen_chain_poses(ngd_se3 *poses, int npos)
{
    poses[0]=ngd_se3_identity();
    for (int i=1;i<npos;++i){
        ngd_vec3 t = ngd_v3(0.02f*frandn(), 0.02f*frandn(), 0.12f);
        ngd_quat q = ngd_quat_from_axis_angle(ngd_v3_normalize(ngd_v3(frandn(),frandn(),frandn())), 0.01f*frandn());
        poses[i] = ngd_se3_multiply(ngd_se3_from_rt(ngd_quat_to_matrix(q), t), poses[i-1]);
    }
}

static void gen_problem(ngd_ba_problem *p, ngd_cam_ctx *cam,
                        int npos, int npts, int stereo_mode, int nfixed,
                        int perturb, int inject_outliers, int degenerate,
                        ngd_se3 *gt_out)
{
    ngd_se3 gt[MAXPOS];
    gen_chain_poses(gt, npos);
    if (gt_out) for (int i=0;i<npos;++i) gt_out[i]=gt[i];

    ngd_se3 *poses=(ngd_se3*)malloc((size_t)npos*sizeof(ngd_se3));
    int *fixed=(int*)calloc((size_t)npos,sizeof(int));
    for (int i=0;i<npos && i<nfixed;++i) fixed[i]=1;
    for (int i=0;i<npos;++i) poses[i] = (perturb && !fixed[i]) ? perturb_pose(gt[i], 0.012f, 0.004f) : gt[i];

    const int ntot = npts + (degenerate?1:0);
    ngd_vec3 *points=(ngd_vec3*)malloc((size_t)ntot*sizeof(ngd_vec3));
    int capE = npos*npts + 8;
    ngd_ba_edge *edges=(ngd_ba_edge*)malloc((size_t)capE*sizeof(ngd_ba_edge));
    int nE=0;
    for (int j=0;j<npts;++j){
        int anchor = (int)(frand()*(float)npos); if(anchor>=npos) anchor=npos-1;
        float u = 40.f + frand()*(W-80.f), v = 40.f + frand()*(H-80.f), d = 1.6f + frand()*2.2f;
        ngd_vec3 Xc = ngd_v3((u-cam->cam.cx)/cam->cam.fx*d, (v-cam->cam.cy)/cam->cam.fy*d, d);
        ngd_se3 Twc = ngd_se3_inverse(gt[anchor]);
        ngd_vec3 Xw = ngd_se3_map(Twc, Xc);
        if (perturb){ Xw.x+=0.008f*frandn(); Xw.y+=0.008f*frandn(); Xw.z+=0.008f*frandn(); }
        points[j]=Xw;
        for (int k=anchor-3;k<=anchor+3;++k){
            if (k<0 || k>=npos) continue;
            int st = (stereo_mode==2) ? (int)(frand()*2.f) : stereo_mode;
            float obs[3];
            if (!project_obs(cam, gt[k], &Xw.x, st, obs)) continue;
            ngd_ba_edge *E=&edges[nE];
            E->pose_idx=k; E->point_idx=j; E->stereo=st;
            E->octave=(int)(frand()*8.f); if(E->octave>7) E->octave=7;
            E->obs[0]=obs[0]; E->obs[1]=obs[1]; E->obs[2]=obs[2];
            nE++;
        }
    }
    if (degenerate) {
        int j=ntot-1;                       /* behind every camera (chain looks +z) */
        points[j]=ngd_v3(0.05f, 0.05f, -2.0f);
        ngd_ba_edge *E=&edges[nE];
        E->pose_idx=npos/2; E->point_idx=j; E->stereo=0; E->octave=0;
        E->obs[0]=320.f; E->obs[1]=240.f; E->obs[2]=0.f;
        nE++;
    }
    if (inject_outliers)
        for (int e=0;e<4 && e<nE;++e){ edges[e].obs[0]+=60.0f; edges[e].obs[1]+=55.0f; if(edges[e].stereo) edges[e].obs[2]+=50.0f; }

    p->poses=poses; p->pose_fixed=fixed; p->n_poses=npos;
    p->points=points; p->n_points=ntot;
    p->edges=edges; p->n_edges=nE;
    p->cam=&cam->cam; p->bf=cam->mbf; p->calib=cam;
}

static void free_problem(ngd_ba_problem *p)
{
    free(p->poses); free(p->pose_fixed); free(p->points); free(p->edges);
}

static void clone_problem(const ngd_ba_problem *src, ngd_ba_problem *dst, int *outlier)
{
    memset(dst,0,sizeof(*dst));
    dst->poses=(ngd_se3*)malloc((size_t)src->n_poses*sizeof(ngd_se3));
    dst->pose_fixed=(int*)malloc((size_t)src->n_poses*sizeof(int));
    dst->points=(ngd_vec3*)malloc((size_t)src->n_points*sizeof(ngd_vec3));
    dst->edges=(ngd_ba_edge*)malloc((size_t)src->n_edges*sizeof(ngd_ba_edge));
    memcpy(dst->poses,src->poses,(size_t)src->n_poses*sizeof(ngd_se3));
    memcpy(dst->pose_fixed,src->pose_fixed,(size_t)src->n_poses*sizeof(int));
    memcpy(dst->points,src->points,(size_t)src->n_points*sizeof(ngd_vec3));
    memcpy(dst->edges,src->edges,(size_t)src->n_edges*sizeof(ngd_ba_edge));
    dst->n_poses=src->n_poses; dst->n_points=src->n_points; dst->n_edges=src->n_edges;
    dst->cam=src->cam; dst->bf=src->bf; dst->calib=src->calib;
    dst->outlier=outlier;
}

/* compare two solved problems; 0 within tolerance */
static int compare_solved(const ngd_ba_problem *a, const ngd_ba_problem *b,
                          double chi_a, double chi_b,
                          double *maxq, double *maxt, double *maxp, int *odiff)
{
    double mq=0, mt=0, mp=0; int od=0;
    for (int i=0;i<a->n_poses;++i){
        double dq = fabs((double)a->poses[i].q.w-b->poses[i].q.w);
        dq=fmax(dq,fabs((double)a->poses[i].q.x-b->poses[i].q.x));
        dq=fmax(dq,fabs((double)a->poses[i].q.y-b->poses[i].q.y));
        dq=fmax(dq,fabs((double)a->poses[i].q.z-b->poses[i].q.z));
        if(dq>mq)mq=dq;
        double et=pose_t_err(a->poses[i],b->poses[i]); if(et>mt)mt=et;
    }
    for (int j=0;j<a->n_points;++j){
        double dx=a->points[j].x-b->points[j].x, dy=a->points[j].y-b->points[j].y, dz=a->points[j].z-b->points[j].z;
        double d=sqrt(dx*dx+dy*dy+dz*dz); if(d>mp)mp=d;
    }
    for (int e=0;e<a->n_edges;++e) if ((a->outlier[e]!=0)!=(b->outlier[e]!=0)) od++;
    double denom = fabs(chi_a)>1e-12 ? fabs(chi_a) : 1.0;
    double crel = fabs(chi_a-chi_b)/denom;
    *maxq=mq; *maxt=mt; *maxp=mp; *odiff=od;
    if (mq>1e-5 || mt>1e-5 || mp>1e-4 || od>2 || crel>1e-8) return 1;
    return 0;
}

/* run dense + sparse on identical copies, compare */
static void ab_case(ngd_cam_ctx *cam, const char *tag,
                    int npos,int npts,int stereo_mode,int nfixed,
                    int perturb,int inj,int degen)
{
    ngd_ba_problem base;
    int capE = npos*npts + 8;
    int *out1=(int*)calloc((size_t)capE,sizeof(int));
    gen_problem(&base,cam,npos,npts,stereo_mode,nfixed,perturb,inj,degen,NULL);
    ngd_ba_problem pd, psb; int *out2=(int*)calloc((size_t)base.n_edges,sizeof(int));
    clone_problem(&base,&pd,out1);
    clone_problem(&base,&psb,out2);
    free_problem(&base);
    double chi_d = ngd_ba_optimize_dense(&pd,10);
    double chi_s = ngd_ba_optimize_sparse(&psb,10);
    double mq,mt,mp; int od;
    int bad = compare_solved(&pd,&psb,chi_d,chi_s,&mq,&mt,&mp,&od);
    printf("  %-24s nE=%5d chi_d=%.6g chi_s=%.6g dq=%.1e dt=%.1e dp=%.1e odiff=%d %s\n",
           tag, pd.n_edges, chi_d, chi_s, mq, mt, mp, od, bad?"MISMATCH":"ok");
    CHECK(!bad, tag);
    free_problem(&pd); free_problem(&psb); free(out1); free(out2);
}

int main(void)
{
    rng = 42u;
    ngd_cam_ctx cam = make_cam();

    /* ---- T1: noise-free no drift (through the dispatcher) ---- */
    {
        ngd_ba_problem p; int outlier[4096]; ngd_se3 gt[MAXPOS];
        gen_problem(&p,&cam,4,50,/*stereo=*/0,/*nfixed=*/2,/*perturb=*/0,0,0,gt);
        p.outlier=outlier;
        ngd_vec3 *p0=(ngd_vec3*)malloc((size_t)p.n_points*sizeof(ngd_vec3));
        memcpy(p0,p.points,(size_t)p.n_points*sizeof(ngd_vec3));
        double chi = ngd_ba_optimize(&p,10); (void)chi;
        double maxr=0, maxt=0, maxp=0;
        for (int i=0;i<p.n_poses;++i){ double e=pose_rot_err(p.poses[i],gt[i]); if(e>maxr)maxr=e;
                                       double et=pose_t_err(p.poses[i],gt[i]); if(et>maxt)maxt=et; }
        for (int j=0;j<p.n_points;++j){
            double dx=p.points[j].x-p0[j].x, dy=p.points[j].y-p0[j].y, dz=p.points[j].z-p0[j].z;
            double d=sqrt(dx*dx+dy*dy+dz*dz); if(d>maxp)maxp=d;
        }
        printf("  T1 noise-free: max rot=%.2e max t=%.2e max pt=%.2e (%d edges)\n",
               maxr, maxt, maxp, p.n_edges);
        CHECK(maxr < 1e-5, "noise-free poses do not drift");
        CHECK(maxt < 1e-4, "noise-free translations do not drift");
        CHECK(maxp < 1e-4, "noise-free points do not drift");
        free(p0); free_problem(&p);
    }

    /* ---- T2: dense vs sparse equivalence battery ---- */
    printf("  T2 dense/sparse equivalence battery:\n");
    ab_case(&cam,"mono small",        5, 60, 0, 2, 1, 0, 0);
    ab_case(&cam,"stereo small",      5, 60, 1, 1, 1, 0, 0);
    ab_case(&cam,"mix+outlier+degen", 6, 80, 2, 2, 1, 1, 1);
    ab_case(&cam,"noise-free mix",    3, 40, 2, 2, 0, 0, 0);
    ab_case(&cam,"8pose mix",         8,120, 2, 3, 1, 1, 1);
    ab_case(&cam,"all-fixed",         4, 40, 2, 4, 1, 0, 0);

    /* ---- T3: sparse recovery vs ground truth ---- */
    {
        ngd_ba_problem p; int outlier[4096]; ngd_se3 gt[MAXPOS];
        gen_problem(&p,&cam,5,60,/*stereo=*/1,/*nfixed=*/1,/*perturb=*/1,0,0,gt);
        p.outlier=outlier;
        ngd_ba_optimize_sparse(&p,10);
        double maxr=0, maxt=0; int nin=0;
        for (int e=0;e<p.n_edges;++e) if(!outlier[e]) nin++;
        for (int i=0;i<p.n_poses;++i){ double e=pose_rot_err(p.poses[i],gt[i]); if(e>maxr)maxr=e;
                                       double et=pose_t_err(p.poses[i],gt[i]); if(et>maxt)maxt=et; }
        printf("  T3 stereo recovery: rot=%.2e t=%.2e inliers=%d/%d\n", maxr, maxt, nin, p.n_edges);
        CHECK(maxr < 1e-3, "sparse stereo poses recovered");
        CHECK(maxt < 5e-3, "sparse stereo translation recovered");
        CHECK(nin == p.n_edges, "all edges inlier (clean stereo)");
        free_problem(&p);
    }

    /* ---- T4: chain problem (nb~28): equivalence + sparsity stats ---- */
    {
        ngd_ba_problem base; int capE = 30*400+8;
        int *out1=(int*)calloc((size_t)capE,sizeof(int));
        gen_problem(&base,&cam,30,400,/*mix=*/2,/*nfixed=*/2,/*perturb=*/1,/*inj=*/1,/*degen=*/1,NULL);
        ngd_ba_problem pd, psb; int *out2=(int*)calloc((size_t)base.n_edges,sizeof(int));
        clone_problem(&base,&pd,out1);
        clone_problem(&base,&psb,out2);
        free_problem(&base);
        double chi_d = ngd_ba_optimize_dense(&pd,10);
        double chi_s = ngd_ba_optimize_sparse(&psb,10);
        ngd_ba_sparse_stats st;
        ngd_ba_sparse_last_stats(&st);
        double mq,mt,mp; int od;
        int bad = compare_solved(&pd,&psb,chi_d,chi_s,&mq,&mt,&mp,&od);
        long long sparse_total = st.schur_mults + st.factor_mults + st.solve_mults;
        printf("  T4 chain: nE=%d chi_d=%.6g chi_s=%.6g dq=%.1e odiff=%d\n",
               pd.n_edges, chi_d, chi_s, mq, od);
        printf("     stats: nb=%d nnzA=%d (<nb^2/2=%d) nnzL=%d trials=%d fails=%d\n",
               st.nb, st.nnzA, st.nb*st.nb/2, st.nnzL, st.trials, st.factor_fails);
        printf("     mults: schur=%lld factor+solve=%lld (sparse total %lld) vs dense-equiv %lld  (%.1fx)\n",
               st.schur_mults, st.factor_mults+st.solve_mults, sparse_total,
               st.dense_lu_mults_equiv,
               st.dense_lu_mults_equiv>0 ? (double)st.dense_lu_mults_equiv/(double)sparse_total : 0.0);
        CHECK(!bad, "chain dense/sparse equivalence");
        CHECK(st.nb == 28, "chain nb == 28 free poses");
        CHECK(st.nnzA > 0 && st.nnzA < st.nb*st.nb/2, "pose Hessian meaningfully sparse");
        CHECK(st.factor_fails == 0, "no factorisation failures on the chain");
        CHECK(sparse_total < st.dense_lu_mults_equiv, "sparse mults below dense equivalent");
        free_problem(&pd); free_problem(&psb); free(out1); free(out2);
    }

    /* ---- T5: dispatcher routes to sparse ---- */
    {
        ngd_ba_problem base; int capE = 6*80+8;
        int *out1=(int*)calloc((size_t)capE,sizeof(int));
        gen_problem(&base,&cam,6,80,2,2,1,0,0,NULL);
        ngd_ba_problem p1, p2; int *out2=(int*)calloc((size_t)base.n_edges,sizeof(int));
        clone_problem(&base,&p1,out1);
        clone_problem(&base,&p2,out2);
        free_problem(&base);
#if NGD_BA_SPARSE
        double chi1 = ngd_ba_optimize(&p1,10);
        double chi2 = ngd_ba_optimize_sparse(&p2,10);
        CHECK(chi1 == chi2, "dispatcher chi2 equals sparse chi2");
        printf("  T5 dispatcher: chi=%.6g (NGD_BA_SPARSE=%d)\n", chi1, NGD_BA_SPARSE);
#else
        printf("  T5 dispatcher: NGD_BA_SPARSE=0 (dense build), skip\n");
#endif
        free_problem(&p1); free_problem(&p2); free(out1); free(out2);
    }

    if (fails == 0) { printf("test_ba_sparse: PASS\n"); return 0; }
    printf("test_ba_sparse: %d FAILURES\n", fails);
    return 1;
}
