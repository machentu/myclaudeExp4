/* replay_pose_opt.c - Experiment A: feed the ORIGINAL's seed pose + correspondences
 * to the refactor's ngd_pose_optimization, compare the output pose to the original's.
 *
 * Pure numerics test: identical inputs (seed + edges) -> is the refactor's optimized
 * pose the same as the original's (g2o, double)? Small delta => float precision is fine
 * (divergence is upstream matching); large delta => pose_opt.c float (H/b/dx) is the cause.
 *
 * Build (standalone, no CMakeLists): from a VS x64 prompt with refactor/myBuild built:
 *   cl /Fe:replay_pose_opt.exe replay_pose_opt.c /I ..\include /I . /link ..\myBuild\Release\ngd_core.lib
 * Run: replay_pose_opt.exe <golden_poseopt.txt> [fid1 fid2 ...]   (fids to highlight, e.g. 143 330)
 */
#include "ngd/math.h"
#include "ngd/se3.h"
#include "ngd/pinhole.h"
#include "ngd/orb.h"
#include "ngd/calib.h"
#include "ngd/pose_opt.h"
#include "ngd_replay.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

static double quat_angle_deg(const double a[4], const double b[4]) {
    /* a,b = (w,x,y,z); angle between rotations = 2*acos(|a.b|) */
    double dot = a[0]*b[0]+a[1]*b[1]+a[2]*b[2]+a[3]*b[3];
    if (dot < 0) dot = -dot;
    if (dot > 1.0) dot = 1.0;
    return 2.0 * acos(dot) * 180.0 / 3.14159265358979323846;
}

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <golden_poseopt.txt> [fid...]\n", argv[0]); return 1; }
    ngd_golden_call* calls = NULL;
    int n = ngd_replay_load_poseopt(argv[1], &calls);
    if (n < 0) return 1;
    fprintf(stderr, "loaded %d PoseOpt calls\n", n);

    /* TUM3 camera context (fx fy cx cy baseline thDepth 640x480, nlevels 8, sf 1.2) */
    ngd_orb_extractor ex; ngd_orb_init(&ex, 1000, 1.2f, 8, 20, 7);
    ngd_cam_ctx cam; ngd_cam_ctx_init(&cam, &ex, 535.4f, 539.2f, 320.1f, 247.6f, 0.0747f, 2.988f, 640, 480);

    /* highlight set */
    int hl[16]; int nhl = 0;
    for (int i = 2; i < argc && nhl < 16; ++i) hl[nhl++] = atoi(argv[i]);

    double max_rot = 0, max_tr = 0, sum_tr = 0;
    int n_reported = 0;
    printf("call   fid    n_edges  ninl_orig  rot_err_deg  trans_err_mm   (A: same inputs)\n");
    for (int c = 0; c < n; ++c) {
        ngd_golden_call* g = &calls[c];
        int ne = g->n_edges;
        if (ne < 3) continue;

        /* build ngd_pose_obs from golden edges */
        ngd_vec3* Xw = (ngd_vec3*)malloc(ne*sizeof(ngd_vec3));
        float* obs = (float*)malloc(3*ne*sizeof(float));
        int* ist = (int*)malloc(ne*sizeof(int));
        int* oct = (int*)malloc(ne*sizeof(int));
        int* outl = (int*)malloc(ne*sizeof(int));
        for (int i = 0; i < ne; ++i) {
            Xw[i].x = (float)g->edges[i].Xw[0]; Xw[i].y = (float)g->edges[i].Xw[1]; Xw[i].z = (float)g->edges[i].Xw[2];
            obs[3*i+0] = (float)g->edges[i].u;
            obs[3*i+1] = (float)g->edges[i].v;
            obs[3*i+2] = (float)g->edges[i].uR;
            ist[i] = g->edges[i].st;
            oct[i] = g->edges[i].oct;
        }
        ngd_pose_obs O;
        O.cam = cam.cam; O.bf = cam.mbf; O.n = ne;
        O.Xw = Xw; O.obs = obs; O.is_stereo = ist; O.octave = oct;
        O.invLevelSigma2 = cam.mvInvLevelSigma2; O.nlevels = cam.nlevels;

        /* seed pose = ORIGINAL's pre-optimization pose (Experiment A) */
        ngd_se3 pose;
        pose.q.w = (float)g->sq[0]; pose.q.x = (float)g->sq[1]; pose.q.y = (float)g->sq[2]; pose.q.z = (float)g->sq[3];
        pose.t.x = (float)g->st_[0]; pose.t.y = (float)g->st_[1]; pose.t.z = (float)g->st_[2];

        int inl = ngd_pose_optimization(&pose, &O, outl);

        /* compare refactor output pose to original's END pose */
        double rq[4] = {pose.q.w, pose.q.x, pose.q.y, pose.q.z};
        double rt[3] = {pose.t.x, pose.t.y, pose.t.z};
        double rot = quat_angle_deg(rq, g->oq);
        double tr = sqrt((rt[0]-g->ot_[0])*(rt[0]-g->ot_[0]) + (rt[1]-g->ot_[1])*(rt[1]-g->ot_[1]) + (rt[2]-g->ot_[2])*(rt[2]-g->ot_[2])) * 1000.0; /* mm */
        if (rot > max_rot) max_rot = rot;
        if (tr > max_tr) max_tr = tr;
        sum_tr += tr; n_reported++;

        int highlight = 0;
        for (int h = 0; h < nhl; ++h) if ((int)g->fid == hl[h]) highlight = 1;
        if (highlight || rot > 0.05 || tr > 1.0)   /* print divergent or highlighted */
            printf("%5d  %5llu  %5d  %6d   %10.6f  %10.4f%s\n", c, g->fid, ne, g->ninl, rot, tr, highlight?"  <<<":"");
        else if (n_reported <= 3)
            printf("%5d  %5llu  %5d  %6d   %10.6f  %10.4f\n", c, g->fid, ne, g->ninl, rot, tr);

        free(Xw); free(obs); free(ist); free(oct); free(outl);
    }
    printf("\n=== Experiment A (pure numerics, same inputs) ===\n");
    printf("calls=%d  max_rot_err=%.6f deg  max_trans_err=%.4f mm  mean_trans_err=%.4f mm\n",
        n_reported, max_rot, max_tr, n_reported? sum_tr/n_reported:0);
    if (max_tr < 1.0) printf("=> A SMALL: float precision EXONERATED. Divergence is upstream (ORB/matching).\n");
    else              printf("=> A LARGE: pose_opt.c float (H/b/dx) contributes. Consider porting to double.\n");
    return 0;
}
