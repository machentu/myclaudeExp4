/* test_sim3solver.c — Horn Sim3 + 3-point RANSAC, faithful to Sim3Solver.cc. */
#include "ngd/so3.h"
#include "ngd/sim3.h"
#include "ngd/sim3solver.h"
#include "ngd/pinhole.h"

#include <stdio.h>
#include <math.h>
#include <stdlib.h>

static int fails = 0;
#define CHECK(cond, msg) do { if(!(cond)){ printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); fails++; } } while(0)
#define APPROX(a,b,eps) (fabsf((a)-(b)) <= (eps))

static float rot_err_deg(ngd_mat3 R, ngd_mat3 Rgt) {
    ngd_mat3 dR = ngd_m3_multiply(ngd_m3_transpose(R), Rgt);
    ngd_vec3 w = ngd_so3_log(dR);
    return ngd_v3_norm(w) * 57.29578f;
}

int main(void)
{
    /* TUM3-ish pinhole (fx,fy,cx,cy) for both cameras. */
    ngd_pinhole cam = ngd_pinhole_make(535.4f, 539.2f, 320.1f, 247.6f);

    /* Ground-truth Sim3 (cam2 -> cam1): R12, t12, s12. */
    ngd_mat3 R12 = ngd_quat_to_matrix(ngd_so3_exp(ngd_v3(0.15f, -0.1f, 0.2f)));
    ngd_vec3 t12 = ngd_v3(0.3f, -0.2f, 0.5f);
    float s12 = 1.2f;
    ngd_sim3 S12; S12.q = ngd_quat_from_matrix(R12); S12.t = t12; S12.s = s12;
    ngd_sim3 S21 = ngd_sim3_inverse(S12);   /* cam1 -> cam2 */

    /* 60 random 3D points in cam1 frame (in front of cam1, Z in [1,4]). */
    int N = 60;
    ngd_vec3 X1[60], X2[60];
    float P1im[120], P2im[120];
    float maxErr1[60], maxErr2[60];
    srand(7);
    for (int i = 0; i < N; ++i) {
        ngd_vec3 p = ngd_v3(0.5f*((rand()%1000)/100.0f) - 2.5f,
                            0.5f*((rand()%1000)/100.0f) - 2.5f,
                            1.0f + ((rand()%3000)/1000.0f));
        X1[i] = p;
        X2[i] = ngd_sim3_map(S21, p);   /* X3Dc2 = S21·X3Dc1 so that X3Dc1 = S12·X3Dc2 */
        float uv1[2], uv2[2];
        ngd_pinhole_project(cam, X1[i], uv1);
        ngd_pinhole_project(cam, X2[i], uv2);
        P1im[2*i]=uv1[0]; P1im[2*i+1]=uv1[1];
        P2im[2*i]=uv2[0]; P2im[2*i+1]=uv2[1];
        maxErr1[i] = 9.210f;   /* sigma^2=1 (level 0) */
        maxErr2[i] = 9.210f;
    }

    /* ---- clean: recover S12 exactly ---- */
    {
        ngd_sim3_solver* s = ngd_sim3solver_create(X1, X2, P1im, P2im, maxErr1, maxErr2,
                                                   N, /*bFixScale=*/0, cam, cam, N, NULL, 12345);
        char inl[60]; int nin; float T[16];
        ngd_sim3solver_find(s, inl, &nin, T);
        /* extract R12,t12,s12 from T */
        ngd_mat3 RsR = {{T[0],T[1],T[2],T[4],T[5],T[6],T[8],T[9],T[10]}};
        float sc = sqrtf(RsR.m[0]*RsR.m[0]+RsR.m[3]*RsR.m[3]+RsR.m[6]*RsR.m[6]); /* norm of col0 */
        ngd_mat3 R = {{RsR.m[0]/sc,RsR.m[1]/sc,RsR.m[2]/sc,
                       RsR.m[3]/sc,RsR.m[4]/sc,RsR.m[5]/sc,
                       RsR.m[6]/sc,RsR.m[7]/sc,RsR.m[8]/sc}};
        ngd_vec3 t = ngd_v3(T[3],T[7],T[11]);
        CHECK(nin >= 50, "clean: enough inliers");
        CHECK(rot_err_deg(R, R12) < 0.5f, "clean: rotation recovery");
        CHECK(APPROX(t.x,t12.x,0.02f)&&APPROX(t.y,t12.y,0.02f)&&APPROX(t.z,t12.z,0.02f), "clean: translation recovery");
        CHECK(APPROX(sc, s12, 0.02f), "clean: scale recovery");
        ngd_sim3solver_destroy(s);
    }

    /* ---- 30% outliers: RANSAC recovers ---- */
    {
        /* corrupt 18 of 60 with random X2 (outliers). */
        ngd_vec3 X2n[60];
        for (int i = 0; i < N; ++i) X2n[i] = X2[i];
        for (int k = 0; k < 18; ++k) {
            int idx = k*3;  /* deterministic outlier set */
            X2n[idx] = ngd_v3(X2[idx].x+1.5f, X2[idx].y-1.0f, X2[idx].z+0.8f);
        }
        ngd_sim3_solver* s = ngd_sim3solver_create(X1, X2n, P1im, P2im, maxErr1, maxErr2,
                                                   N, 0, cam, cam, N, NULL, 12345);
        char inl[60]; int nin; float T[16];
        ngd_sim3solver_find(s, inl, &nin, T);
        ngd_mat3 RsR = {{T[0],T[1],T[2],T[4],T[5],T[6],T[8],T[9],T[10]}};
        float sc = sqrtf(RsR.m[0]*RsR.m[0]+RsR.m[3]*RsR.m[3]+RsR.m[6]*RsR.m[6]);
        ngd_mat3 R = {{RsR.m[0]/sc,RsR.m[1]/sc,RsR.m[2]/sc,
                       RsR.m[3]/sc,RsR.m[4]/sc,RsR.m[5]/sc,
                       RsR.m[6]/sc,RsR.m[7]/sc,RsR.m[8]/sc}};
        CHECK(nin >= 38, "ransac: inlier count (~42 expected)");
        CHECK(rot_err_deg(R, R12) < 1.0f, "ransac: rotation recovery");
        CHECK(APPROX(sc, s12, 0.05f), "ransac: scale recovery");
        ngd_sim3solver_destroy(s);
    }

    /* ---- bFixScale: scale forced to 1 ---- */
    {
        ngd_sim3_solver* s = ngd_sim3solver_create(X1, X2, P1im, P2im, maxErr1, maxErr2,
                                                   N, /*bFixScale=*/1, cam, cam, N, NULL, 12345);
        char inl[60]; int nin; float T[16];
        ngd_sim3solver_find(s, inl, &nin, T);
        ngd_mat3 RsR = {{T[0],T[1],T[2],T[4],T[5],T[6],T[8],T[9],T[10]}};
        float sc = sqrtf(RsR.m[0]*RsR.m[0]+RsR.m[3]*RsR.m[3]+RsR.m[6]*RsR.m[6]);
        CHECK(APPROX(sc, 1.0f, 1e-3f), "fixscale: scale==1");
        ngd_sim3solver_destroy(s);
    }

    if (fails == 0) { printf("test_sim3solver: PASS\n"); return 0; }
    printf("test_sim3solver: %d FAILURES\n", fails);
    return 1;
}
