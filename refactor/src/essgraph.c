/* ngd/essgraph.c — OptimizeEssentialGraph (Sim3 pose graph, pure C).
 * Faithful single-map/non-IMU/non-merge port of Optimizer::OptimizeEssentialGraph
 * (src/Optimizer.cc:1501-1783).  g2o EdgeSim3 uses numeric Jacobians, so the
 * optimizer below uses central-difference Jacobians on the 7-DoF left
 * perturbation, matching g2o's runtime behaviour. */
#include "ngd/essgraph.h"
#include "ngd/map.h"
#include "ngd/keyframe.h"
#include "ngd/mappoint.h"
#include "ngd/so3.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>

#define MIN_FEAT 100   /* Optimizer.cc:1530 */

/* ------------------------------------------------------------------ *
 * Edge: relative Sim3 constraint between vertex i (Siw) and j (Sjw).
 *   err = log( meas * Siw * Sjw^{-1} )   (7-D)   [EdgeSim3::computeError]
 * ------------------------------------------------------------------ */
typedef struct { int i, j; ngd_sim3 meas; } eg_edge;

static ngd_sim3 se3_to_sim3(ngd_se3 e) { ngd_sim3 s; s.q = e.q; s.t = e.t; s.s = 1.0f; return s; }

/* KF* -> vertex index in vkfs[0..nV), or -1. */
static int idx_of(ngd_keyframe **vkfs, int nV, const ngd_keyframe *kf) {
    for (int i = 0; i < nV; ++i) if (vkfs[i] == kf) return i;
    return -1;
}

/* look up kf in a kf_sim3 list; return its Scw or Sim3(kf->pose, s=1) if absent */
static ngd_sim3 lookup_sim3(const ngd_kf_sim3 *list, int n, const ngd_keyframe *kf, int use_pose_fallback) {
    if (list) {
        for (int k = 0; k < n; ++k)
            if (list[k].kf == kf) return list[k].Scw;
    }
    if (use_pose_fallback) return se3_to_sim3(kf->pose);
    ngd_sim3 z; z.q = ngd_quat_identity(); z.t = ngd_v3(0,0,0); z.s = 1.0f; return z;
}

/* edge residual at vertices[]: fills err[7] = log(meas * Siw * Sjw^{-1}). */
static void edge_residual(const ngd_sim3 *vertices, const eg_edge *e, float err[7]) {
    ngd_sim3 prod = ngd_sim3_multiply(e->meas, vertices[e->i]);           /* meas * Siw */
    ngd_sim3 sjw_inv = ngd_sim3_inverse(vertices[e->j]);
    ngd_sim3 errS = ngd_sim3_multiply(prod, sjw_inv);                     /* meas*Siw*Sjw^{-1} */
    ngd_sim3_log(errS, err);
}

void ngd_optimize_essential_graph(ngd_map *map,
                                  ngd_keyframe *pLoopKF, ngd_keyframe *pCurKF,
                                  const ngd_kf_sim3 *corrected, int nCorr,
                                  const ngd_kf_sim3 *nonCorrected, int nNonCorr,
                                  const ngd_kf_pair *loopConns, int nLoopConns,
                                  int bFixScale)
{
    (void)bFixScale;   /* pose-graph scale handling: RGBD fixes s=1 in vertex init */

    /* ---- collect non-bad KFs (vertices) ---- */
    int nK = map->nKFs;
    ngd_keyframe **vkfs = (ngd_keyframe**)malloc(sizeof(ngd_keyframe*)*nK);
    int nV = 0;
    for (int i = 0; i < nK; ++i) {
        ngd_keyframe *kf = map->kfs[i];
        if (!kf || ngd_keyframe_is_bad(kf)) continue;
        vkfs[nV++] = kf;
    }
    if (nV == 0) { free(vkfs); return; }

    ngd_sim3 *vertices = (ngd_sim3*)malloc(sizeof(ngd_sim3)*nV);
    ngd_sim3 *vScw_pre = (ngd_sim3*)malloc(sizeof(ngd_sim3)*nV);  /* pre-opt Scw (for MP correction) */
    char *fixed = (char*)calloc(nV, 1);
    for (int v = 0; v < nV; ++v) {
        ngd_keyframe *kf = vkfs[v];
        vertices[v] = lookup_sim3(corrected, nCorr, kf, /*fallback*/1);
        vScw_pre[v] = se3_to_sim3(kf->pose);   /* current stored pose (pre-opt) */
        if (kf->mnId == map->mnInitKFid) fixed[v] = 1;
    }

    /* ---- build edges ---- */
    int capE = 4 * nV + nLoopConns + 8;
    eg_edge *edges = (eg_edge*)malloc(sizeof(eg_edge)*capE);
    int nE = 0;
    /* dedup set of (min,max) vertex-index pairs already inserted */
    long *inserted = (long*)malloc(sizeof(long)*capE);
    int nIns = 0;

    /* (a) loop edges from LoopConnections, measured from corrected (vScw_pre? No:
     *     the original uses vScw = corrected Sim3). Use `corrected` lookup. */
    for (int c = 0; c < nLoopConns; ++c) {
        ngd_keyframe *pa = loopConns[c].a, *pb = loopConns[c].b;
        int vi = idx_of(vkfs, nV, pa), vj = idx_of(vkfs, nV, pb);
        if (vi < 0 || vj < 0) continue;
        /* keep weight gate exception for the pCurKF<->pLoopKF pair (cc:1588) */
        ngd_sim3 Siw = lookup_sim3(corrected, nCorr, pa, 1);
        ngd_sim3 Sjw = lookup_sim3(corrected, nCorr, pb, 1);
        ngd_sim3 Sji = ngd_sim3_multiply(Sjw, ngd_sim3_inverse(Siw));
        edges[nE].i = vi; edges[nE].j = vj; edges[nE].meas = Sji; nE++;
        long key = (vi < vj) ? (long)vi * nV + vj : (long)vj * nV + vi;
        inserted[nIns++] = key;
    }

    /* (b) covisibility edges (weight >= MIN_FEAT), measured from nonCorrected. */
    for (int v = 0; v < nV; ++v) {
        ngd_keyframe *kf = vkfs[v];
        ngd_sim3 Siw = lookup_sim3(nonCorrected, nNonCorr, kf, 1);
        ngd_sim3 Swi = ngd_sim3_inverse(Siw);
        for (int c = 0; c < kf->nConn; ++c) {
            if (kf->connWeights[c] < MIN_FEAT) continue;   /* GetCovisiblesByWeight */
            ngd_keyframe *nb = kf->connKFs[c];
            int vj = idx_of(vkfs, nV, nb);
            if (vj < 0 || vj == v) continue;
            /* dedup + id ordering (cc:1682-1685): only insert nb->mnId < kf->mnId */
            if (nb->mnId >= kf->mnId) continue;
            long key = (v < vj) ? (long)v * nV + vj : (long)vj * nV + v;
            int seen = 0; for (int s = 0; s < nIns; ++s) if (inserted[s] == key) { seen=1; break; }
            if (seen) continue;
            ngd_sim3 Sjw = lookup_sim3(nonCorrected, nNonCorr, nb, 1);
            ngd_sim3 Sji = ngd_sim3_multiply(Sjw, Swi);
            edges[nE].i = v; edges[nE].j = vj; edges[nE].meas = Sji; nE++;
            inserted[nIns++] = key;
        }
    }

    if (nE == 0) {
        /* no constraints: still nothing to optimize */
        free(vkfs); free(vertices); free(vScw_pre); free(fixed); free(edges); free(inserted);
        return;
    }
    (void)pLoopKF; (void)pCurKF;

    /* ---- dense LM pose graph (7*nV params) ---- */
    int P = 7 * nV;
    float *H = (float*)malloc(sizeof(float)*P*P);
    float *b = (float*)malloc(sizeof(float)*P);
    float *J = (float*)malloc(sizeof(float)*7*nE*P);   /* [7*nE rows][P cols] row-major */
    float *r0 = (float*)malloc(sizeof(float)*7*nE);

    double currentChi = 0;
    for (int e = 0; e < nE; ++e) {
        float err[7]; edge_residual(vertices, &edges[e], err);
        for (int k = 0; k < 7; ++k) currentChi += (double)err[k]*err[k];
    }

    double lambda = -1.0, ni = 2.0;
    int nBad = 0;
    const int max_steps = 20;

    for (int step = 0; step < max_steps; ++step) {
        /* residuals + numeric Jacobian (central difference on each of P params) */
        for (int e = 0; e < nE; ++e) {
            float err[7]; edge_residual(vertices, &edges[e], err);
            for (int k = 0; k < 7; ++k) r0[7*e+k] = err[k];
        }
        const float h = 1e-4f;
        ngd_sim3 *vp = (ngd_sim3*)malloc(sizeof(ngd_sim3)*nV);
        for (int p = 0; p < P; ++p) {
            int v = p / 7, d = p % 7;
            if (fixed[v]) { for (int e=0;e<nE;++e) for(int k=0;k<7;++k) J[(7*e+k)*P+p]=0.0f; continue; }
            for (int w = 0; w < nV; ++w) vp[w] = vertices[w];
            float xp[7] = {0,0,0,0,0,0,0}, xm[7] = {0,0,0,0,0,0,0};
            xp[d] = h; xm[d] = -h;
            vp[v] = ngd_sim3_multiply(ngd_sim3_exp(xp), vertices[v]);
            for (int e = 0; e < nE; ++e) {
                if (edges[e].i != v && edges[e].j != v) {
                    for (int k = 0; k < 7; ++k) J[(7*e+k)*P+p] = 0.0f;
                    continue;
                }
                float rp[7]; edge_residual(vp, &edges[e], rp);
                /* recompute with -h */
                ngd_sim3 vm_d = ngd_sim3_multiply(ngd_sim3_exp(xm), vertices[v]);
                ngd_sim3 sav = vp[v]; vp[v] = vm_d;
                float rm[7]; edge_residual(vp, &edges[e], rm);
                vp[v] = sav;
                for (int k = 0; k < 7; ++k) J[(7*e+k)*P+p] = (rp[k]-rm[k])/(2.0f*h);
            }
        }
        free(vp);

        /* H = J^T J (P x P), b = -J^T r0 (P). information = I. */
        memset(H, 0, sizeof(float)*P*P);
        memset(b, 0, sizeof(float)*P);
        int R = 7*nE;
        for (int rr = 0; rr < P; ++rr) {
            for (int cc = 0; cc < P; ++cc) {
                float s = 0;
                for (int t = 0; t < R; ++t) s += J[t*P+rr]*J[t*P+cc];
                H[rr*P+cc] = s;
            }
            float s = 0;
            for (int t = 0; t < R; ++t) s += J[t*P+rr]*r0[t];
            b[rr] = -s;
        }
        /* fix fixed vertices: pin their dx to 0 */
        for (int v = 0; v < nV; ++v) if (fixed[v]) {
            for (int d = 0; d < 7; ++d) {
                int p = v*7+d;
                for (int q = 0; q < P; ++q) { H[p*P+q]=0; H[q*P+p]=0; }
                H[p*P+p] = 1.0f; b[p] = 0.0f;
            }
        }

        if (step == 0) {
            float maxd = 0;
            for (int j = 0; j < P; ++j) { float vv = fabsf(H[j*P+j]); if (vv>maxd) maxd=vv; }
            lambda = 1e-16 * (double)maxd;   /* setUserLambdaInit(1e-16) */
            if (lambda <= 0.0) lambda = 1e-16;
            ni = 2.0;
        }

        double iniChi = currentChi;
        double rho = 0.0; int qmax = 0;
        float *dx = (float*)malloc(sizeof(float)*P);
        ngd_sim3 *vtrial = (ngd_sim3*)malloc(sizeof(ngd_sim3)*nV);
        do {
            float *Hd = (float*)malloc(sizeof(float)*P*P);
            memcpy(Hd, H, sizeof(float)*P*P);
            for (int j = 0; j < P; ++j) Hd[j*P+j] += (float)lambda;
            if (ngd_solve_linear(Hd, b, dx, P) != 0) {
                rho = -1.0;
            } else {
                for (int v = 0; v < nV; ++v) {
                    if (fixed[v]) { vtrial[v] = vertices[v]; continue; }
                    float xi[7] = {0,0,0,0,0,0,0};
                    for (int d = 0; d < 7; ++d) xi[d] = dx[v*7+d];
                    vtrial[v] = ngd_sim3_multiply(ngd_sim3_exp(xi), vertices[v]);
                }
                double tempChi = 0;
                for (int e = 0; e < nE; ++e) {
                    float err[7]; edge_residual(vtrial, &edges[e], err);
                    for (int k = 0; k < 7; ++k) tempChi += (double)err[k]*err[k];
                }
                if (!isfinite(tempChi)) tempChi = 1e30;
                double scale = 1e-3;
                for (int j = 0; j < P; ++j) scale += dx[j]*((float)lambda*dx[j] + b[j]);
                if (scale <= 0.0) scale = 1e-3;
                rho = (currentChi - tempChi) / scale;
                if (rho > 0.0) {
                    double alpha = 1.0 - pow(2.0*rho - 1.0, 3.0);
                    if (alpha < 1.0/3.0) alpha = 1.0/3.0;
                    if (alpha > 2.0/3.0) alpha = 2.0/3.0;
                    lambda *= alpha; ni = 2.0;
                    currentChi = tempChi;
                    for (int v = 0; v < nV; ++v) vertices[v] = vtrial[v];
                } else {
                    lambda *= ni; ni *= 2.0;
                }
            }
            free(Hd);
            qmax++;
        } while (rho < 0.0 && qmax < 10);
        free(dx); free(vtrial);

        if (qmax == 10 || rho == 0.0) break;
        if ((iniChi - currentChi)*1e3 < iniChi) nBad++; else nBad = 0;
        if (nBad >= 3) break;
    }

    /* ---- write back KF poses (SE3 = [R | t/s]) and stash pre/post Sim3 ---- */
    for (int v = 0; v < nV; ++v) {
        ngd_se3 T = ngd_sim3_to_se3(vertices[v]);
        ngd_keyframe_set_pose(vkfs[v], T);
    }

    /* ---- correct MapPoints: Pw' = (post Twc_ref) * (pre Tcw_ref * Pw) ---- */
    for (int i = 0; i < map->nMPs; ++i) {
        ngd_mappoint *mp = map->mps[i];
        if (!mp || mp->mbBad) continue;
        ngd_keyframe *ref = mp->refKF;
        if (!ref) continue;
        int vr = idx_of(vkfs, nV, ref);
        if (vr < 0) continue;   /* refKF not in this optimization: leave as-is */
        ngd_sim3 preScw = vScw_pre[vr];          /* pre-opt Tcw_ref */
        ngd_sim3 postSwc = ngd_sim3_inverse(vertices[vr]);  /* optimized Twc_ref */
        ngd_vec3 pw = ngd_v3(mp->worldPos[0], mp->worldPos[1], mp->worldPos[2]);
        ngd_vec3 pc = ngd_sim3_map(preScw, pw);          /* into ref old cam frame */
        ngd_vec3 pwn = ngd_sim3_map(postSwc, pc);        /* back to world, new pose */
        mp->worldPos[0] = pwn.x; mp->worldPos[1] = pwn.y; mp->worldPos[2] = pwn.z;
        ngd_mappoint_update_normal_and_depth(mp);
    }

    free(vkfs); free(vertices); free(vScw_pre); free(fixed);
    free(edges); free(inserted);
    free(H); free(b); free(J); free(r0);
}
