/* ngd/pose_opt.c — PoseOptimization in pure C.
 * Faithful to ORB-SLAM3 Optimizer::PoseOptimization (src/Optimizer.cc:814-1114)
 * and g2o's Levenberg solver / Huber robust kernel. See include/ngd/pose_opt.h.
 */
#include "ngd/pose_opt.h"

#include <math.h>
#include <string.h>

/* g2o constants (src/Optimizer.cc:852-853, 1001-1002). */
#define NGD_DELTA_MONO_SQ   5.991f   /* chi2 threshold & Huber delta^2, mono  */
#define NGD_DELTA_STEREO_SQ 7.815f   /* chi2 threshold & Huber delta^2, stereo */

/* ------------------------------------------------------------------ *
 * Per-edge residual at pose T.
 *   r_out[0..dim-1] = obs - project(map(T, Xw))
 *   *dim = 2 (mono) or 3 (stereo)
 *   *chi2 = r^T Omega r = invSigma2 * (r . r)
 * Returns 0 on success, -1 if the point is behind/near the camera (degenerate).
 * ------------------------------------------------------------------ */
static int edge_residual(ngd_se3 T, const ngd_pose_obs *o, int i,
                         float r_out[3], int *dim, float *chi2)
{
    ngd_vec3 Xc = ngd_se3_map(T, o->Xw[i]);
    if (Xc.z <= 1e-6f) return -1;     /* depth not positive: skip */

    int stereo = o->is_stereo[i];
    float proj[3];
    int d;
    if (stereo) {
        ngd_pinhole_project_stereo(o->cam, o->bf, Xc, proj);
        d = 3;
    } else {
        ngd_pinhole_project(o->cam, Xc, proj);
        d = 2;
    }
    const float *obs = &o->obs[3*i];
    r_out[0] = obs[0] - proj[0];
    r_out[1] = obs[1] - proj[1];
    if (stereo) r_out[2] = obs[2] - proj[2];

    int lvl = o->octave[i];
    if (lvl < 0 || lvl >= o->nlevels) lvl = 0;
    float invS2 = o->invLevelSigma2[lvl];

    float rr = r_out[0]*r_out[0] + r_out[1]*r_out[1];
    if (stereo) rr += r_out[2]*r_out[2];
    *dim = d;
    *chi2 = invS2 * rr;
    return 0;
}

static inline float huber_rho0(float chi2, float delta) {
    /* rho[0]: robustified cost. */
    if (chi2 <= delta*delta) return chi2;
    float s = sqrtf(chi2);
    return 2.0f*delta*s - delta*delta;
}
static inline float huber_rho1(float chi2, float delta) {
    /* rho[1]: IRLS weight (g2o robustInformation returns rho[1]*Omega). */
    if (chi2 <= delta*delta) return 1.0f;
    return delta / sqrtf(chi2);
}

/* Sum of robustified cost over inlier (outlier==0) edges. +1e30 per degenerate. */
static double active_robust_chi2(ngd_se3 T, const ngd_pose_obs *o,
                                 const int *outlier, int use_kernel)
{
    double sum = 0.0;
    for (int i = 0; i < o->n; ++i) {
        if (outlier[i]) continue;
        float r[3]; int dim; float chi2;
        if (edge_residual(T, o, i, r, &dim, &chi2) != 0) { sum += 1e30; continue; }
        float delta = o->is_stereo[i] ? sqrtf(NGD_DELTA_STEREO_SQ)
                                      : sqrtf(NGD_DELTA_MONO_SQ);
        sum += use_kernel ? (double)huber_rho0(chi2, delta) : (double)chi2;
    }
    return sum;
}

/* ------------------------------------------------------------------ *
 * One Levenberg round: up to `max_steps` accepted steps (g2o solve()).
 * Linearises once per step at the current pose over inlier edges.
 * ------------------------------------------------------------------ */
static void lm_round(ngd_se3 *pose, const ngd_pose_obs *o,
                     const int *outlier, int use_kernel, int max_steps)
{
    double currentChi = active_robust_chi2(*pose, o, outlier, use_kernel);
    double lambda = -1.0;
    double ni = 2.0;
    int nBad = 0;

    for (int step = 0; step < max_steps; ++step) {
        /* ---- build H (6x6) and b (= -gradient) over inlier edges ---- */
        float H[36] = {0};   /* row-major 6x6 */
        float b[6]  = {0};   /* g2o's b = -g */

        for (int i = 0; i < o->n; ++i) {
            if (outlier[i]) continue;
            float r[3]; int dim; float chi2;
            if (edge_residual(*pose, o, i, r, &dim, &chi2) != 0) continue;

            int lvl = o->octave[i]; if (lvl<0||lvl>=o->nlevels) lvl=0;
            float invS2 = o->invLevelSigma2[lvl];
            float delta = o->is_stereo[i] ? sqrtf(NGD_DELTA_STEREO_SQ)
                                          : sqrtf(NGD_DELTA_MONO_SQ);
            float w = use_kernel ? huber_rho1(chi2, delta) : 1.0f;

            /* A = -Pjac * SE3deriv  (dim x 6) */
            ngd_vec3 Xc = ngd_se3_map(*pose, o->Xw[i]);
            float pjac[9], sd[18], A[18];
            if (o->is_stereo[i])
                ngd_pinhole_project_jac_stereo(o->cam, o->bf, Xc, pjac);
            else
                ngd_pinhole_project_jac(o->cam, Xc, pjac);   /* fills first 6 */
            ngd_se3_deriv(Xc, sd);
            /* A(dim x 6) = -pjac(dim x 3) * sd(3 x 6) */
            for (int rr = 0; rr < dim; ++rr)
                for (int cc = 0; cc < 6; ++cc) {
                    float s = 0.f;
                    for (int k = 0; k < 3; ++k) s += pjac[rr*3+k] * sd[k*6+cc];
                    A[rr*6+cc] = -s;
                }

            float ws = w * invS2;     /* Omega = invS2 * I, so A^T Omega A = invS2 * A^T A */
            /* H += ws * A^T A ;  b -= ws * A^T r   (g2o: b -= rho1 * A^T Omega r) */
            for (int rr = 0; rr < 6; ++rr) {
                for (int cc = 0; cc < 6; ++cc) {
                    float s = 0.f;
                    for (int k = 0; k < dim; ++k) s += A[k*6+rr] * A[k*6+cc];
                    H[rr*6+cc] += ws * s;
                }
                float s = 0.f;
                for (int k = 0; k < dim; ++k) s += A[k*6+rr] * r[k];
                b[rr] -= ws * s;        /* b = -g */
            }
        }

        /* ---- lambda init on first step ---- */
        if (step == 0) {
            float maxd = 0.f;
            for (int j = 0; j < 6; ++j) { float v = fabsf(H[j*6+j]); if (v>maxd) maxd=v; }
            lambda = 1e-5 * (double)maxd;
            ni = 2.0;
            if (lambda <= 0.0) lambda = 1e-5;
        }

        double iniChi = currentChi;
        double rho = 0.0;
        int qmax = 0;
        float dx[6];

        do {
            /* Hd = H + lambda*I */
            float Hd[36];
            memcpy(Hd, H, sizeof(H));
            for (int j = 0; j < 6; ++j) Hd[j*6+j] += (float)lambda;

            if (ngd_solve_linear(Hd, b, dx, 6) != 0) {
                /* singular: treat as bad step */
                rho = -1.0;
            } else {
                float xi[6] = {dx[0],dx[1],dx[2],dx[3],dx[4],dx[5]};
                ngd_se3 Ttrial = ngd_se3_multiply(ngd_se3_exp(xi), *pose);
                double tempChi = active_robust_chi2(Ttrial, o, outlier, use_kernel);
                if (!isfinite(tempChi)) tempChi = 1e30;

                /* Nielsen gain ratio: scale = sum dx*(lambda*dx + b) */
                double scale = 1e-3;
                for (int j = 0; j < 6; ++j) scale += dx[j]*((float)lambda*dx[j] + b[j]);
                if (scale <= 0.0) scale = 1e-3;
                rho = (currentChi - tempChi) / scale;

                if (rho > 0.0) {
                    /* accept */
                    double alpha = 1.0 - pow(2.0*rho - 1.0, 3.0);
                    if (alpha < 1.0/3.0) alpha = 1.0/3.0;
                    if (alpha > 2.0/3.0) alpha = 2.0/3.0;
                    lambda *= alpha;
                    ni = 2.0;
                    currentChi = tempChi;
                    *pose = Ttrial;
                } else {
                    /* reject */
                    lambda *= ni; ni *= 2.0;
                }
            }
            qmax++;
        } while (rho < 0.0 && qmax < 10);

        if (qmax == 10 || rho == 0.0) break;   /* Terminate this round */

        /* g2o stop criterion: consecutive negligible-improvement steps */
        if ((iniChi - currentChi)*1e3 < iniChi) nBad++;
        else nBad = 0;
        if (nBad >= 3) break;
    }
}

int ngd_pose_optimization(ngd_se3 *pose, const ngd_pose_obs *o, int *outlier)
{
    int nInit = o->n;
    if (nInit < 3) {
        for (int i = 0; i < nInit; ++i) outlier[i] = 0;
        return 0;
    }
    for (int i = 0; i < nInit; ++i) outlier[i] = 0;   /* all inlier to start */

    const int its[4] = {10,10,10,10};

    int nBad = 0;
    for (int it = 0; it < 4; ++it) {
        /* rounds 0,1,2 use the Huber kernel; round 3 drops it (kernel removed at it==2) */
        int use_kernel = (it < 3);
        lm_round(pose, o, outlier, use_kernel, its[it]);

        /* classify inliers/outliers by raw chi2 at the final pose */
        nBad = 0;
        for (int i = 0; i < nInit; ++i) {
            float r[3]; int dim; float chi2;
            int ok = edge_residual(*pose, o, i, r, &dim, &chi2);
            float thr = o->is_stereo[i] ? NGD_DELTA_STEREO_SQ : NGD_DELTA_MONO_SQ;
            if (ok != 0 || chi2 > thr) { outlier[i] = 1; nBad++; }
            else                         outlier[i] = 0;
        }

        if (nInit < 10) break;   /* optimizer.edges().size() < 10 */
    }
    return nInit - nBad;
}
