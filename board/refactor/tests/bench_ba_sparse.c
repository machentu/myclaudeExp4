/* bench_ba_sparse.c — dense vs block-sparse BA pose solver benchmark.
 *
 * Not part of ctest (manual target). Builds chain problems at LBA / GBA
 * scale (banded covisibility like real SLAM), runs ngd_ba_optimize_dense
 * and ngd_ba_optimize_sparse on identical clones, and reports per-call wall
 * time plus multiply counts (the cost model that matters on soft-float,
 * no-FPU boards where cycles ~ mults).
 *
 * Build:  cmake --build myBuild --config Release --target bench_ba_sparse
 * Run:    ./Release/bench_ba_sparse [configs...]
 *         (no args = all; indices: 0=LBA-S 1=LBA-M 2=GBA-M 3=GBA-L)
 */
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
#include <time.h>

#define W 640
#define H 480

static unsigned int rng = 7u;
static float frand(void){ rng = rng*1664525u + 1013904223u; return (rng >> 8) * (1.0f/16777216.0f); }
static float frandn(void){ return (frand()+frand()+frand()+frand()-2.0f); }

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

static ngd_se3 perturb_pose(ngd_se3 T, float dt, float drot)
{
    ngd_vec3 t = ngd_v3(T.t.x + dt*frandn(), T.t.y + dt*frandn(), T.t.z + dt*frandn());
    ngd_quat q = ngd_quat_from_axis_angle(ngd_v3_normalize(ngd_v3(frandn(),frandn(),frandn())), drot*frandn());
    ngd_se3 dT = ngd_se3_from_rt(ngd_quat_to_matrix(q), ngd_v3(0,0,0));
    return ngd_se3_multiply(dT, ngd_se3_from_rt(ngd_quat_to_matrix(T.q), t));
}

/* chain scene: forward motion, points unprojected at an anchor KF, observed
 * in a +-vw window (banded covisibility). Mixed mono/stereo, mixed octaves. */
static void gen_problem(ngd_ba_problem *p, ngd_cam_ctx *cam,
                        int npos, int npts, int nfixed, int vw)
{
    ngd_se3 *gt=(ngd_se3*)malloc((size_t)npos*sizeof(ngd_se3));
    gt[0]=ngd_se3_identity();
    for (int i=1;i<npos;++i){
        ngd_vec3 t = ngd_v3(0.02f*frandn(), 0.02f*frandn(), 0.15f);
        ngd_quat q = ngd_quat_from_axis_angle(ngd_v3_normalize(ngd_v3(frandn(),frandn(),frandn())), 0.008f*frandn());
        gt[i] = ngd_se3_multiply(ngd_se3_from_rt(ngd_quat_to_matrix(q), t), gt[i-1]);
    }
    ngd_se3 *poses=(ngd_se3*)malloc((size_t)npos*sizeof(ngd_se3));
    int *fixed=(int*)calloc((size_t)npos,sizeof(int));
    for (int i=0;i<npos && i<nfixed;++i) fixed[i]=1;
    for (int i=0;i<npos;++i) poses[i] = fixed[i] ? gt[i] : perturb_pose(gt[i], 0.012f, 0.004f);

    ngd_vec3 *points=(ngd_vec3*)malloc((size_t)npts*sizeof(ngd_vec3));
    int capE = npos*npts + 8;
    ngd_ba_edge *edges=(ngd_ba_edge*)malloc((size_t)capE*sizeof(ngd_ba_edge));
    int nE=0;
    for (int j=0;j<npts;++j){
        int anchor = (int)(frand()*(float)npos); if(anchor>=npos) anchor=npos-1;
        float u = 40.f + frand()*(W-80.f), v = 40.f + frand()*(H-80.f), d = 1.6f + frand()*2.2f;
        ngd_vec3 Xc = ngd_v3((u-cam->cam.cx)/cam->cam.fx*d, (v-cam->cam.cy)/cam->cam.fy*d, d);
        ngd_se3 Twc = ngd_se3_inverse(gt[anchor]);
        ngd_vec3 Xw = ngd_se3_map(Twc, Xc);
        Xw.x+=0.008f*frandn(); Xw.y+=0.008f*frandn(); Xw.z+=0.008f*frandn();
        points[j]=Xw;
        for (int k=anchor-vw;k<=anchor+vw;++k){
            if (k<0 || k>=npos) continue;
            int st = (int)(frand()*2.f);
            float obs[3];
            if (!project_obs(cam, gt[k], &Xw.x, st, obs)) continue;
            ngd_ba_edge *E=&edges[nE];
            E->pose_idx=k; E->point_idx=j; E->stereo=st;
            E->octave=(int)(frand()*8.f); if(E->octave>7) E->octave=7;
            E->obs[0]=obs[0]; E->obs[1]=obs[1]; E->obs[2]=obs[2];
            nE++;
        }
    }
    free(gt);
    p->poses=poses; p->pose_fixed=fixed; p->n_poses=npos;
    p->points=points; p->n_points=npts;
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

typedef struct { const char *name; int npos, npts, nfixed, vw, reps_dense, reps_sparse; } cfg_t;
static const cfg_t CFGS[] = {
    { "LBA-S",  25,  3000, 2, 4, 3, 5 },
    { "LBA-M",  40,  6000, 3, 4, 3, 5 },
    { "GBA-M", 120, 12000, 1, 4, 1, 3 },
    { "GBA-L", 300, 30000, 1, 4, 1, 3 },
};
#define NCFG ((int)(sizeof(CFGS)/sizeof(CFGS[0])))

static double run_reps(const cfg_t *c, ngd_ba_problem *base, int sparse, int reps)
{
    double best = 1e30;
    int *out=(int*)calloc((size_t)base->n_edges,sizeof(int));
    for (int r=0;r<reps;++r){
        ngd_ba_problem p; clone_problem(base,&p,out);
        clock_t t0=clock();
        if (sparse) ngd_ba_optimize_sparse(&p,10);
        else        ngd_ba_optimize_dense(&p,10);
        clock_t t1=clock();
        double dt=(double)(t1-t0)/CLOCKS_PER_SEC;
        if (dt<best) best=dt;
        free_problem(&p);
        if (!sparse) printf(".");
    }
    free(out);
    return best;
}

int main(int argc, char **argv)
{
    int which[NCFG]; int nwhich=0;
    if (argc<=1) { for (int i=0;i<NCFG;++i) which[nwhich++]=i; }
    else for (int a=1;a<argc;++a){ int i=atoi(argv[a]); if(i>=0&&i<NCFG) which[nwhich++]=i; }

    ngd_cam_ctx cam = make_cam();
    printf("%-8s %5s %5s %7s %8s | %12s %12s %8s | %10s %10s %7s\n",
           "config","npos","nb","npts","nE","dense s/call","sparse s/call","speedup",
           "dense Mmul","sparse Mmul","mul-x");
    for (int w=0;w<nwhich;++w){
        const cfg_t *c=&CFGS[which[w]];
        rng = 7u + (unsigned)which[w];           /* deterministic per config */
        ngd_ba_problem base;
        gen_problem(&base,&cam,c->npos,c->npts,c->nfixed,c->vw);
        int nb = c->npos - (c->nfixed < c->npos ? c->nfixed : c->npos);

        double td = run_reps(c,&base,0,c->reps_dense);
        double ts = run_reps(c,&base,1,c->reps_sparse);
        ngd_ba_sparse_stats st; ngd_ba_sparse_last_stats(&st);
        long long sparse_mul = st.schur_mults + st.factor_mults + st.solve_mults;
        printf("\r%-8s %5d %5d %7d %8d | %12.4f %12.4f %7.2fx | %10.2f %10.2f %6.1fx\n",
               c->name, c->npos, st.nb>0?st.nb:nb, c->npts, base.n_edges,
               td, ts, td/ts,
               st.dense_lu_mults_equiv/1e6, sparse_mul/1e6,
               sparse_mul>0 ? (double)st.dense_lu_mults_equiv/(double)sparse_mul : 0.0);
        printf("         nnzA=%d/%d blocks (%.0f%% fill) nnzL=%d trials=%d factor_fails=%d\n",
               st.nnzA, st.nb*st.nb, st.nb>0 ? 100.0*st.nnzA/(st.nb*(double)st.nb) : 0.0,
               st.nnzL, st.trials, st.factor_fails);
        fflush(stdout);
        free_problem(&base);
    }
    printf("note: mults ratio ~ board speedup on soft-float (no FPU) targets; "
           "PC wall time is memory/cache bound.\n");
    return 0;
}
