/* ngd/sim3opt.c — OptimizeSim3 (single Sim3 vertex, numeric Jacobians, LM+Huber).
 * Faithful to Optimizer::OptimizeSim3 (src/Optimizer.cc:2115-2381). */
#include "ngd/sim3opt.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>

/* ---- residual of one edge pair at Sim3 S12 ----
 * S12 maps cam2->cam1:  P_c1 = s12*(R12*P_c2) + t12 = ngd_sim3_map(S12, P_c2)
 * S21 = S12^{-1}        maps cam1->cam2.
 * edge12 residual: obs1 - project_cam1(S12 * X2c[i])
 * edge21 residual: obs2 - project_cam2(S21 * X1c[i])
 * Returns 0 if both projections valid (z>0), -1 otherwise (degenerate). */
static int edge_pair_residual(ngd_sim3 S12, ngd_sim3 S21, const ngd_sim3_obs* o, int i,
                              float r12[2], float r21[2],
                              float *chi2_12, float *chi2_21)
{
    ngd_vec3 p1c = ngd_sim3_map(S12, o->X2c[i]);   /* in cam1 frame */
    ngd_vec3 p2c = ngd_sim3_map(S21, o->X1c[i]);   /* in cam2 frame */
    if (p1c.z <= 1e-6f || p2c.z <= 1e-6f) return -1;
    float uv1[2], uv2[2];
    ngd_pinhole_project(o->cam1, p1c, uv1);
    ngd_pinhole_project(o->cam2, p2c, uv2);
    r12[0] = o->obs1[2*i+0] - uv1[0];
    r12[1] = o->obs1[2*i+1] - uv1[1];
    r21[0] = o->obs2[2*i+0] - uv2[0];
    r21[1] = o->obs2[2*i+1] - uv2[1];
    float c1 = r12[0]*r12[0] + r12[1]*r12[1];
    float c2 = r21[0]*r21[0] + r21[1]*r21[1];
    *chi2_12 = o->invS2_1[i] * c1;
    *chi2_21 = o->invS2_2[i] * c2;
    return 0;
}

static inline float huber_rho1(float chi2, float delta) {
    if (chi2 <= delta*delta) return 1.0f;
    return delta / sqrtf(chi2);
}

/* Sum of robustified cost over inlier edges. With kernel, each edge's chi2 is
 * replaced by Huber rho[0](chi2) = 2*delta*sqrt(chi2) - delta^2 (chi2>delta^2). */
static double active_chi2(ngd_sim3 S12, ngd_sim3 S21, const ngd_sim3_obs* o,
                          const int* outlier, int use_kernel, float delta)
{
    double sum = 0.0;
    float d2 = delta * delta;
    for (int i = 0; i < o->nCorr; ++i) {
        if (outlier[i]) continue;
        float r12[2], r21[2], c1, c2;
        if (edge_pair_residual(S12, S21, o, i, r12, r21, &c1, &c2) != 0) { sum += 1e30; continue; }
        double a = (double)c1, b = (double)c2;
        if (use_kernel) {
            if (c1 > d2) a = 2.0*delta*sqrt((double)c1) - (double)d2;
            if (c2 > d2) b = 2.0*delta*sqrt((double)c2) - (double)d2;
        }
        sum += a + b;
    }
    return sum;
}

/* numeric Jacobian of the inlier edge residuals w.r.t. the nDim left-perturbation.
 * r0 (in)  : current residuals, layout [4*nInlier]: for inlier k, edge12 at [4k+0,1], edge21 at [4k+2,3]
 * J  (out) : row-major [nDim][4*nInlier], J[col*4n + k] = d r_k / d xi_col
 * nInlier  : number of inlier correspondences; inlIdx[k] = original correspondence index.
 * nDim = 7 (or 6 if bFixScale; sigma column omitted). */
static void numeric_jacobian(ngd_sim3 S12, const ngd_sim3_obs* o,
                             const int* inlIdx, int nInlier, int nDim,
                             const float* r0, float* J)
{
    const float h = 1e-4f;   /* sqrt(float_eps) ~ 3e-4: avoids roundoff dominance */
    int R = 4*nInlier;
    for (int col = 0; col < nDim; ++col) {
        float xp[7] = {0,0,0,0,0,0,0};
        float xm[7] = {0,0,0,0,0,0,0};
        xp[col] =  h;  xm[col] = -h;
        ngd_sim3 Sp = ngd_sim3_multiply(ngd_sim3_exp(xp), S12);
        ngd_sim3 Sm = ngd_sim3_multiply(ngd_sim3_exp(xm), S12);
        ngd_sim3 S21p = ngd_sim3_inverse(Sp);
        ngd_sim3 S21m = ngd_sim3_inverse(Sm);
        for (int k = 0; k < nInlier; ++k) {
            int i = inlIdx[k];
            float rp12[2], rp21[2], cp1, cp2, rm12[2], rm21[2], cm1, cm2;
            edge_pair_residual(Sp, S21p, o, i, rp12, rp21, &cp1, &cp2);
            edge_pair_residual(Sm, S21m, o, i, rm12, rm21, &cm1, &cm2);
            J[col*R + 4*k+0] = (rp12[0]-rm12[0])/(2.0f*h);
            J[col*R + 4*k+1] = (rp12[1]-rm12[1])/(2.0f*h);
            J[col*R + 4*k+2] = (rp21[0]-rm21[0])/(2.0f*h);
            J[col*R + 4*k+3] = (rp21[1]-rm21[1])/(2.0f*h);
        }
    }
    (void)r0;
}

/* One LM round: up to max_steps accepted steps over inlier edges. */
static void lm_round(ngd_sim3* S12, const ngd_sim3_obs* o, const int* outlier,
                     int use_kernel, float delta, int max_steps)
{
    int nDim = o->bFixScale ? 6 : 7;
    int nCorr = o->nCorr;

    /* build inlier index list */
    int* inlIdx = (int*)malloc(sizeof(int)*nCorr);
    int nIn = 0;
    for (int i = 0; i < nCorr; ++i) if (!outlier[i]) inlIdx[nIn++] = i;
    int R = 4*nIn;
    float* r0 = (float*)malloc(sizeof(float)*R);
    float* J  = (float*)malloc(sizeof(float)*nDim*R);

    ngd_sim3 S21 = ngd_sim3_inverse(*S12);
    double currentChi = active_chi2(*S12, S21, o, outlier, use_kernel, delta);
    double lambda = -1.0, ni = 2.0;
    int nBad = 0;

    for (int step = 0; step < max_steps; ++step) {
        /* recompute residuals + chi2 at current pose */
        S21 = ngd_sim3_inverse(*S12);
        for (int k = 0; k < nIn; ++k) {
            int i = inlIdx[k];
            float c1, c2; float r12[2], r21[2];
            edge_pair_residual(*S12, S21, o, i, r12, r21, &c1, &c2);
            r0[4*k+0]=r12[0]; r0[4*k+1]=r12[1]; r0[4*k+2]=r21[0]; r0[4*k+3]=r21[1];
        }
        numeric_jacobian(*S12, o, inlIdx, nIn, nDim, r0, J);

        /* build H (nDim x nDim) and b (nDim) with per-edge IRLS weight + invS2.
         * Each correspondence contributes two 2-D edges (edge12, edge21);
         * edge12 uses invS2_1 + Huber weight w1, edge21 uses invS2_2 + w2. */
        float H[49] = {0}; float b[7] = {0};
        for (int k = 0; k < nIn; ++k) {
            int i = inlIdx[k];
            float c1 = o->invS2_1[i]*(r0[4*k+0]*r0[4*k+0]+r0[4*k+1]*r0[4*k+1]);
            float c2 = o->invS2_2[i]*(r0[4*k+2]*r0[4*k+2]+r0[4*k+3]*r0[4*k+3]);
            float w1 = use_kernel ? huber_rho1(c1, delta) : 1.0f;
            float w2 = use_kernel ? huber_rho1(c2, delta) : 1.0f;
            float ws[4] = { w1*o->invS2_1[i], w1*o->invS2_1[i], w2*o->invS2_2[i], w2*o->invS2_2[i] };
            for (int rr = 0; rr < nDim; ++rr) {
                for (int cc = 0; cc < nDim; ++cc) {
                    float s = 0;
                    for (int t = 0; t < 4; ++t) s += J[rr*R+4*k+t]*J[cc*R+4*k+t]*ws[t];
                    H[rr*nDim+cc] += s;
                }
                float s = 0;
                for (int t = 0; t < 4; ++t) s += J[rr*R+4*k+t]*r0[4*k+t]*ws[t];
                b[rr] -= s;   /* b = -g */
            }
        }

        if (step == 0) {
            float maxd = 0;
            for (int j = 0; j < nDim; ++j) { float v = fabsf(H[j*nDim+j]); if (v>maxd) maxd=v; }
            lambda = 1e-5 * (double)maxd; ni = 2.0;
            if (lambda <= 0.0) lambda = 1e-5;
        }

        double iniChi = currentChi;
        double rho = 0.0; int qmax = 0; float dx[7] = {0};
        do {
            float Hd[49];
            memcpy(Hd, H, sizeof(float)*nDim*nDim);
            for (int j = 0; j < nDim; ++j) Hd[j*nDim+j] += (float)lambda;
            float b6[7];
            for (int j = 0; j < nDim; ++j) b6[j] = b[j];
            if (ngd_solve_linear(Hd, b6, dx, nDim) != 0) {
                rho = -1.0;
            } else {
                float xi[7] = {0,0,0,0,0,0,0};
                for (int j = 0; j < nDim; ++j) xi[j] = dx[j];
                ngd_sim3 Sp = ngd_sim3_multiply(ngd_sim3_exp(xi), *S12);
                double tempChi = active_chi2(Sp, ngd_sim3_inverse(Sp), o, outlier, use_kernel, delta);
                if (!isfinite(tempChi)) tempChi = 1e30;
                double scale = 1e-3;
                for (int j = 0; j < nDim; ++j) scale += dx[j]*((float)lambda*dx[j] + b[j]);
                if (scale <= 0.0) scale = 1e-3;
                rho = (currentChi - tempChi) / scale;
                if (rho > 0.0) {
                    double alpha = 1.0 - pow(2.0*rho - 1.0, 3.0);
                    if (alpha < 1.0/3.0) alpha = 1.0/3.0;
                    if (alpha > 2.0/3.0) alpha = 2.0/3.0;
                    lambda *= alpha; ni = 2.0;
                    currentChi = tempChi; *S12 = Sp;
                } else {
                    lambda *= ni; ni *= 2.0;
                }
            }
            qmax++;
        } while (rho < 0.0 && qmax < 10);

        if (qmax == 10 || rho == 0.0) break;
        if ((iniChi - currentChi)*1e3 < iniChi) nBad++; else nBad = 0;
        if (nBad >= 3) break;
    }
    free(inlIdx); free(r0); free(J);
}

int ngd_sim3_optimize(ngd_sim3* S12, const ngd_sim3_obs* o, int* outliers)
{
    int nCorr = o->nCorr;
    for (int i = 0; i < nCorr; ++i) outliers[i] = 0;
    float delta = sqrtf(o->th2);

    /* Phase 1: optimize(5) with Huber kernel. */
    lm_round(S12, o, outliers, 1, delta, 5);

    /* Classify outliers by raw chi2 (either edge > th2).  S21 must be recomputed
     * from the updated *S12 (it was stale if cached from before Phase 1). */
    ngd_sim3 S21 = ngd_sim3_inverse(*S12);
    int nBad = 0;
    for (int i = 0; i < nCorr; ++i) {
        float r12[2], r21[2], c1, c2;
        int ok = edge_pair_residual(*S12, S21, o, i, r12, r21, &c1, &c2);
        if (ok != 0 || c1 > o->th2 || c2 > o->th2) { outliers[i] = 1; nBad++; }
        else outliers[i] = 0;
    }
    if (nCorr - nBad < 10) return 0;

    /* Phase 2: drop kernel, optimize 5 (or 10 if outliers found). */
    int nMore = (nBad > 0) ? 10 : 5;
    lm_round(S12, o, outliers, 0, delta, nMore);

    /* Final inlier count (no kernel, raw chi2). */
    int nIn = 0;
    for (int i = 0; i < nCorr; ++i) {
        float r12[2], r21[2], c1, c2;
        int ok = edge_pair_residual(*S12, ngd_sim3_inverse(*S12), o, i, r12, r21, &c1, &c2);
        if (ok != 0 || c1 > o->th2 || c2 > o->th2) outliers[i] = 1;
        else { outliers[i] = 0; nIn++; }
    }
    return nIn;
}
