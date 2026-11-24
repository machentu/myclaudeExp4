/* test_sim3opt.c — OptimizeSim3 (numeric-Jac LM + Huber), faithful to Optimizer.cc:2115. */
#include "ngd/so3.h"
#include "ngd/sim3.h"
#include "ngd/sim3opt.h"
#include "ngd/pinhole.h"

#include <stdio.h>
#include <math.h>
#include <stdlib.h>

static int fails = 0;
#define CHECK(cond, msg) do { if(!(cond)){ printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); fails++; } } while(0)
#define APPROX(a,b,eps) (fabsf((a)-(b)) <= (eps))

static float sim3_err(ngd_sim3 a, ngd_sim3 b) {
    ngd_mat3 Ra = ngd_quat_to_matrix(a.q), Rb = ngd_quat_to_matrix(b.q);
    ngd_mat3 dR = ngd_m3_multiply(ngd_m3_transpose(Ra), Rb);
    ngd_vec3 w = ngd_so3_log(dR);
    ngd_vec3 dt = ngd_v3_sub(a.t, b.t);
    return ngd_v3_norm(w) + ngd_v3_norm(dt) + fabsf(a.s - b.s);
}

int main(void)
{
    ngd_pinhole cam = ngd_pinhole_make(535.4f, 539.2f, 320.1f, 247.6f);

    /* GT Sim3 (cam2->cam1), s=1.2. */
    ngd_sim3 Sgt; Sgt.q = ngd_so3_exp(ngd_v3(0.15f,-0.1f,0.2f)); Sgt.t = ngd_v3(0.3f,-0.2f,0.5f); Sgt.s = 1.2f;

    int N = 60;
    static ngd_vec3 X2c[60], X1c[60];
    static float obs1[120], obs2[120], inv1[60], inv2[60];
    srand(11);
    for (int i = 0; i < N; ++i) {
        ngd_vec3 p2 = ngd_v3(0.6f*((rand()%1000)/100.0f)-3.0f,
                             0.6f*((rand()%1000)/100.0f)-3.0f,
                             1.5f + ((rand()%3000)/1000.0f));
        X2c[i] = p2;
        X1c[i] = ngd_sim3_map(Sgt, p2);   /* X1c = S12 * X2c */
        float u1[2], u2[2];
        ngd_pinhole_project(cam, X1c[i], u1);
        ngd_pinhole_project(cam, X2c[i], u2);
        obs1[2*i]=u1[0]; obs1[2*i+1]=u1[1];
        obs2[2*i]=u2[0]; obs2[2*i+1]=u2[1];
        inv1[i] = 1.0f; inv2[i] = 1.0f;
    }

    ngd_sim3_obs obs = { N, obs1, obs2, X1c, X2c, inv1, inv2, cam, cam, 5.991f, /*bFixScale=*/0 };
    int out[60];

    /* ---- perturbed init recovers GT ---- */
    {
        float dx[7] = {0.05f,-0.04f,0.03f, 0.1f,-0.08f,0.06f, 0.04f};
        ngd_sim3 S0 = ngd_sim3_multiply(ngd_sim3_exp(dx), Sgt);
        ngd_sim3 S = S0;
        int nIn = ngd_sim3_optimize(&S, &obs, out);
        CHECK(nIn >= 55, "mono: inliers >= 55");
        CHECK(sim3_err(S, Sgt) < 1e-3f, "mono: recover GT (rot/t/s < 1e-3)");
    }

    /* ---- bFixScale: s held at 1, rot/trans recover ---- */
    {
        ngd_sim3 Sgt1; Sgt1.q = ngd_so3_exp(ngd_v3(0.12f,0.08f,-0.15f)); Sgt1.t = ngd_v3(0.2f,0.1f,-0.3f); Sgt1.s = 1.0f;
        static ngd_vec3 Xb2[60], Xb1[60]; static float ob1[120], ob2[120], i1[60], i2[60];
        for (int i = 0; i < N; ++i) {
            ngd_vec3 p2 = ngd_v3(0.6f*((rand()%1000)/100.0f)-3.0f,
                                 0.6f*((rand()%1000)/100.0f)-3.0f,
                                 1.5f + ((rand()%3000)/1000.0f));
            Xb2[i] = p2; Xb1[i] = ngd_sim3_map(Sgt1, p2);
            float u1[2], u2[2];
            ngd_pinhole_project(cam, Xb1[i], u1); ngd_pinhole_project(cam, Xb2[i], u2);
            ob1[2*i]=u1[0]; ob1[2*i+1]=u1[1]; ob2[2*i]=u2[0]; ob2[2*i+1]=u2[1];
            i1[i]=1; i2[i]=1;
        }
        ngd_sim3_obs ob = { N, ob1, ob2, Xb1, Xb2, i1, i2, cam, cam, 5.991f, /*bFixScale=*/1 };
        float dx[7] = {0.04f,-0.03f,0.05f, 0.08f,-0.06f,0.04f, 0.0f};  /* sigma=0 -> s stays 1 */
        ngd_sim3 S = ngd_sim3_multiply(ngd_sim3_exp(dx), Sgt1);
        ngd_sim3_optimize(&S, &ob, out);
        CHECK(APPROX(S.s, 1.0f, 1e-3f), "fixscale: s stays 1");
        /* rotation/translation should still recover; scale dim frozen but others optimize */
        ngd_mat3 Ra = ngd_quat_to_matrix(S.q), Rgt = ngd_quat_to_matrix(Sgt1.q);
        ngd_mat3 dR = ngd_m3_multiply(ngd_m3_transpose(Ra), Rgt);
        CHECK(ngd_v3_norm(ngd_so3_log(dR)) < 1e-3f, "fixscale: rotation recovers");
        CHECK(ngd_v3_norm(ngd_v3_sub(S.t, Sgt1.t)) < 1e-3f, "fixscale: translation recovers");
    }

    /* ---- outliers flagged ---- */
    {
        static float ob1n[120];
        for (int i = 0; i < 120; ++i) ob1n[i] = obs1[i];
        /* corrupt 6 of 60 obs1 by +30px (gross outliers) */
        for (int k = 0; k < 6; ++k) { int idx=k*10; ob1n[2*idx] += 30.0f; }
        ngd_sim3_obs obn = { N, ob1n, obs2, X1c, X2c, inv1, inv2, cam, cam, 5.991f, 0 };
        float dx[7] = {0.02f,-0.02f,0.02f, 0.03f,-0.03f,0.02f, 0.02f};
        ngd_sim3 S = ngd_sim3_multiply(ngd_sim3_exp(dx), Sgt);
        int nIn = ngd_sim3_optimize(&S, &obn, out);
        int nOut = 0;
        for (int i = 0; i < N; ++i) if (out[i]) nOut++;
        CHECK(nOut >= 5, "outliers: gross outliers flagged");
        CHECK(nIn <= N - 5, "outliers: inliers reduced");
        CHECK(sim3_err(S, Sgt) < 1e-2f, "outliers: GT still recovered");
    }

    if (fails == 0) { printf("test_sim3opt: PASS\n"); return 0; }
    printf("test_sim3opt: %d FAILURES\n", fails);
    return 1;
}
