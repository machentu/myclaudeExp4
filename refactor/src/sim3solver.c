/* ngd/sim3solver.c — Horn 1987 Sim3 + 3-point RANSAC, faithful to Sim3Solver.cc. */
#include "ngd/sim3solver.h"
#include "ngd/so3.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>

/* ---- self-contained cyclic Jacobi for symmetric eigendecomposition (n<=16).
 * Sorts eigenpairs ASCENDING.  Identical convention to pnp.c/mlpnp.c
 * (tan(2θ)=2·apq/(app−aqq) under J=[[c,-s],[s,c]]). */
static void jacobi_sym(double *A, int n, double *V, double *w)
{
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            V[i * n + j] = (i == j) ? 1.0 : 0.0;

    for (int sweep = 0; sweep < 60; ++sweep) {
        double off = 0.0;
        for (int p = 0; p < n; ++p)
            for (int q = p + 1; q < n; ++q)
                off += A[p * n + q] * A[p * n + q];
        if (off < 1e-24) break;

        for (int p = 0; p < n; ++p) {
            for (int q = p + 1; q < n; ++q) {
                double apq = A[p * n + q];
                if (fabs(apq) < 1e-18) continue;
                double app = A[p * n + p], aqq = A[q * n + q];
                double phi = 0.5 * atan2(2.0 * apq, app - aqq);
                double c = cos(phi), s = sin(phi);
                for (int i = 0; i < n; ++i) {
                    double aip = A[i * n + p], aiq = A[i * n + q];
                    A[i * n + p] = c * aip + s * aiq;
                    A[i * n + q] = -s * aip + c * aiq;
                }
                for (int i = 0; i < n; ++i) {
                    double api = A[p * n + i], aqi = A[q * n + i];
                    A[p * n + i] = c * api + s * aqi;
                    A[q * n + i] = -s * api + c * aqi;
                }
                for (int i = 0; i < n; ++i) {
                    double vip = V[i * n + p], viq = V[i * n + q];
                    V[i * n + p] = c * vip + s * viq;
                    V[i * n + q] = -s * vip + c * viq;
                }
            }
        }
    }
    for (int i = 0; i < n; ++i) w[i] = A[i * n + i];
    for (int i = 0; i < n - 1; ++i) {
        int mn = i;
        for (int j = i + 1; j < n; ++j) if (w[j] < w[mn]) mn = j;
        if (mn != i) {
            double tw = w[i]; w[i] = w[mn]; w[mn] = tw;
            for (int r = 0; r < n; ++r) {
                double tv = V[r * n + i];
                V[r * n + i] = V[r * n + mn];
                V[r * n + mn] = tv;
            }
        }
    }
}

/* deterministic LCG (stand-in for DUtils::Random::RandomInt) */
static int rng_randint(unsigned int *state, int hi) {
    *state = (*state * 1103515245u + 12345u) & 0x7fffffffu;
    return (int)(*state % (unsigned)(hi + 1));
}

/* -------------------------------------------------------------------- create */
ngd_sim3_solver* ngd_sim3solver_create(
    const ngd_vec3* X3Dc1, const ngd_vec3* X3Dc2,
    const float* P1im1, const float* P2im2,
    const float* maxErr1, const float* maxErr2,
    int N, int bFixScale,
    ngd_pinhole cam1, ngd_pinhole cam2,
    int nOrig, const int* origIdx, unsigned int seed)
{
    ngd_sim3_solver* s = (ngd_sim3_solver*)calloc(1, sizeof(*s));
    if (!s || N <= 0) { free(s); return NULL; }
    s->N = N;
    s->X3Dc1 = (ngd_vec3*)malloc(sizeof(ngd_vec3)*N);
    s->X3Dc2 = (ngd_vec3*)malloc(sizeof(ngd_vec3)*N);
    s->P1im1 = (float*)malloc(sizeof(float)*2*N);
    s->P2im2 = (float*)malloc(sizeof(float)*2*N);
    s->maxErr1 = (float*)malloc(sizeof(float)*N);
    s->maxErr2 = (float*)malloc(sizeof(float)*N);
    s->mvbInliersi  = (char*)malloc(N);
    s->mvbBestInliers = (char*)malloc(N);
    s->mvAllIndices = (int*)malloc(sizeof(int)*N);
    if (!s->X3Dc1 || !s->X3Dc2 || !s->P1im1 || !s->P2im2 ||
        !s->maxErr1 || !s->maxErr2 || !s->mvbInliersi || !s->mvbBestInliers || !s->mvAllIndices) {
        ngd_sim3solver_destroy(s); return NULL;
    }
    memcpy(s->X3Dc1, X3Dc1, sizeof(ngd_vec3)*N);
    memcpy(s->X3Dc2, X3Dc2, sizeof(ngd_vec3)*N);
    memcpy(s->P1im1, P1im1, sizeof(float)*2*N);
    memcpy(s->P2im2, P2im2, sizeof(float)*2*N);
    memcpy(s->maxErr1, maxErr1, sizeof(float)*N);
    memcpy(s->maxErr2, maxErr2, sizeof(float)*N);
    for (int i = 0; i < N; ++i) s->mvAllIndices[i] = i;
    s->cam1 = cam1; s->cam2 = cam2;
    s->bFixScale = bFixScale;
    s->nOrig = (nOrig > 0) ? nOrig : N;
    s->origIdx = origIdx;   /* not owned; caller must keep alive */
    s->rng = (seed == 0) ? 1 : seed;
    s->mBestScale = 1.0f;
    s->mBestRotation = ngd_m3_identity();
    s->haveBest = 0;
    /* default RANSAC params (Sim3Solver::SetRansacParameters defaults) */
    ngd_sim3solver_set_params(s, 0.99, 20, 300);
    return s;
}

void ngd_sim3solver_set_params(ngd_sim3_solver* s, double prob, int minInliers, int maxIts) {
    s->prob = prob;
    s->minInliers = minInliers;
    s->maxIts = maxIts;
    s->mnIterations = 0;
    s->mnBestInliers = 0;
    /* Adjust maxIts according to prob/epsilon (SetRansacParameters). */
    if (s->N > 0 && minInliers < s->N && minInliers != s->N) {
        double eps = (double)minInliers / (double)s->N;
        double nIters = ceil(log(1.0 - s->prob) / log(1.0 - pow(eps, 3.0)));
        int m = (int)nIters;
        if (m < 1) m = 1;
        if (m < maxIts) maxIts = m;
    }
    if (maxIts < 1) maxIts = 1;
    s->maxIts = maxIts;
}

/* ---------------------------------------------------- Horn 1987 ComputeSim3 */
/* P1, P2: 3 points each (column j = point j), as 9 doubles row-major
 * (P[i*3+j] = component i of point j). Fills s->mR12i/mt12i/ms12i. */
static void compute_sim3(ngd_sim3_solver* s, const double P1[9], const double P2[9])
{
    /* Step 1: centroid & relative coords */
    double O1[3] = {0,0,0}, O2[3] = {0,0,0};
    for (int j = 0; j < 3; ++j) { O1[0]+=P1[0*3+j]; O1[1]+=P1[1*3+j]; O1[2]+=P1[2*3+j];
                                  O2[0]+=P2[0*3+j]; O2[1]+=P2[1*3+j]; O2[2]+=P2[2*3+j]; }
    for (int k = 0; k < 3; ++k) { O1[k]/=3.0; O2[k]/=3.0; }
    double Pr1[9], Pr2[9];
    for (int j = 0; j < 3; ++j) for (int i = 0; i < 3; ++i) {
        Pr1[i*3+j] = P1[i*3+j] - O1[i];
        Pr2[i*3+j] = P2[i*3+j] - O2[i];
    }

    /* Step 2: M = Pr2 * Pr1^T  (3x3). M(i,k) = sum_j Pr2[i*3+j]*Pr1[k*3+j]. */
    double M[9];
    for (int i = 0; i < 3; ++i)
        for (int k = 0; k < 3; ++k) {
            double acc = 0;
            for (int j = 0; j < 3; ++j) acc += Pr2[i*3+j]*Pr1[k*3+j];
            M[i*3+k] = acc;
        }

    /* Step 3: N 4x4 (Horn) */
    double N11=M[0]+M[4]+M[8];
    double N12=M[1*3+2]-M[2*3+1];
    double N13=M[2*3+0]-M[0*3+2];
    double N14=M[0*3+1]-M[1*3+0];
    double N22=M[0]-M[4]-M[8];
    double N23=M[0*3+1]+M[1*3+0];
    double N24=M[2*3+0]+M[0*3+2];
    double N33=-M[0]+M[4]-M[8];
    double N34=M[1*3+2]+M[2*3+1];
    double N44=-M[0]-M[4]+M[8];
    double N[16] = {
        N11,N12,N13,N14,
        N12,N22,N23,N24,
        N13,N23,N33,N34,
        N14,N24,N34,N44
    };

    /* Step 4: eigenvector of highest eigenvalue (jacobi ascending -> last col). */
    double V[16], w[4];
    jacobi_sym(N, 4, V, w);
    int maxIdx = 3;   /* ascending sort -> largest eigenvalue is index 3 */
    double evec0 = V[0*4+maxIdx];        /* real part (cos) */
    double vec0 = V[1*4+maxIdx];
    double vec1 = V[2*4+maxIdx];
    double vec2 = V[3*4+maxIdx];
    double vecn = sqrt(vec0*vec0 + vec1*vec1 + vec2*vec2);
    double ang = atan2(vecn, evec0);
    ngd_vec3 ax;
    if (vecn < 1e-12) {
        ax = ngd_v3(0,0,0);
    } else {
        double k = 2.0*ang/vecn;   /* axis-angle (quaternion angle is the half) */
        ax = ngd_v3((float)(k*vec0), (float)(k*vec1), (float)(k*vec2));
    }
    ngd_mat3 R12 = ngd_quat_to_matrix(ngd_so3_exp(ax));

    /* Step 5-6: scale. nom = sum(Pr1 .* (R12*Pr2)); den = sum((R12*Pr2).^2). */
    double P3[9];   /* R12*Pr2 */
    for (int j = 0; j < 3; ++j) {
        double x=Pr2[0*3+j], y=Pr2[1*3+j], z=Pr2[2*3+j];
        P3[0*3+j] = R12.m[0]*x+R12.m[1]*y+R12.m[2]*z;
        P3[1*3+j] = R12.m[3]*x+R12.m[4]*y+R12.m[5]*z;
        P3[2*3+j] = R12.m[6]*x+R12.m[7]*y+R12.m[8]*z;
    }
    double ms12;
    if (!s->bFixScale) {
        double nom = 0, den = 0;
        for (int i = 0; i < 9; ++i) { nom += Pr1[i]*P3[i]; den += P3[i]*P3[i]; }
        ms12 = (fabs(den) < 1e-20) ? 1.0 : nom/den;
    } else {
        ms12 = 1.0;
    }

    /* Step 7: translation: t12 = O1 - s*R12*O2 */
    double sx = ms12*(R12.m[0]*O2[0]+R12.m[1]*O2[1]+R12.m[2]*O2[2]);
    double sy = ms12*(R12.m[3]*O2[0]+R12.m[4]*O2[1]+R12.m[5]*O2[2]);
    double sz = ms12*(R12.m[6]*O2[0]+R12.m[7]*O2[1]+R12.m[8]*O2[2]);

    s->mBestRotation = R12;   /* reused as per-iteration slot below; overwritten by caller */
    /* store into per-iteration fields via the struct's "current" slots:
     * we reuse mBest* temporarily; check_inliers reads them. */
    s->mBestTranslation = ngd_v3((float)(O1[0]-sx),(float)(O1[1]-sy),(float)(O1[2]-sz));
    s->mBestScale = (float)ms12;
}

/* Since compute_sim3 writes into the "best" slots as a per-iteration scratch,
 * keep separate per-iteration accessors.  To stay faithful to Sim3Solver.cc
 * (which has mR12i/mt12i/ms12i distinct from mBest*), we mirror that by
 * storing the iteration values in the same fields and copying to best on
 * improvement.  check_inliers reads the current iteration values. */
#define CUR_R(s)   ((s)->mBestRotation)
#define CUR_T(s)   ((s)->mBestTranslation)
#define CUR_S(s)   ((s)->mBestScale)

static void check_inliers(ngd_sim3_solver* s) {
    ngd_mat3 R12 = CUR_R(s);
    ngd_vec3 t12 = CUR_T(s);
    float sc = CUR_S(s);
    /* sR12 = sc*R12 ; sR21 = (1/sc)*R12^T ; t21 = -sR21*t12 */
    ngd_mat3 sR12 = {{ sc*R12.m[0],sc*R12.m[1],sc*R12.m[2],
                       sc*R12.m[3],sc*R12.m[4],sc*R12.m[5],
                       sc*R12.m[6],sc*R12.m[7],sc*R12.m[8] }};
    float invs = (fabsf(sc) > 1e-20f) ? 1.0f/sc : 0.0f;
    ngd_mat3 sR21 = {{ invs*R12.m[0], invs*R12.m[3], invs*R12.m[6],
                       invs*R12.m[1], invs*R12.m[4], invs*R12.m[7],
                       invs*R12.m[2], invs*R12.m[5], invs*R12.m[8] }};
    ngd_vec3 t21 = ngd_v3_scale(ngd_m3_transform(sR21, t12), -1.0f);

    int nin = 0;
    for (int i = 0; i < s->N; ++i) {
        /* vP2im1 = project cam1(sR12*X3Dc2 + t12); vP1im2 = project cam2(sR21*X3Dc1 + t21) */
        ngd_vec3 p2 = ngd_v3_add(ngd_m3_transform(sR12, s->X3Dc2[i]), t12);
        ngd_vec3 p1 = ngd_v3_add(ngd_m3_transform(sR21, s->X3Dc1[i]), t21);
        float uv2im1[2], uv1im2[2];
        ngd_pinhole_project(s->cam1, p2, uv2im1);
        ngd_pinhole_project(s->cam2, p1, uv1im2);
        float d1x = s->P1im1[2*i+0]-uv2im1[0], d1y = s->P1im1[2*i+1]-uv2im1[1];
        float d2x = uv1im2[0]-s->P2im2[2*i+0], d2y = uv1im2[1]-s->P2im2[2*i+1];
        float err1 = d1x*d1x+d1y*d1y;
        float err2 = d2x*d2x+d2y*d2y;
        if (err1 < s->maxErr1[i] && err2 < s->maxErr2[i]) {
            s->mvbInliersi[i] = 1; nin++;
        } else {
            s->mvbInliersi[i] = 0;
        }
    }
    s->mnInliersi = nin;
}

static void store_best(ngd_sim3_solver* s) {
    s->haveBest = 1;
    s->mnBestInliers = s->mnInliersi;
    memcpy(s->mvbBestInliers, s->mvbInliersi, s->N);
    /* best = current (already in mBest* slots) */
}

static void to_T12(ngd_sim3_solver* s, float outT12[16]) {
    ngd_mat3 R = s->mBestRotation; float sc = s->mBestScale; ngd_vec3 t = s->mBestTranslation;
    outT12[0]=sc*R.m[0]; outT12[1]=sc*R.m[1]; outT12[2]=sc*R.m[2];  outT12[3]=t.x;
    outT12[4]=sc*R.m[3]; outT12[5]=sc*R.m[4]; outT12[6]=sc*R.m[5];  outT12[7]=t.y;
    outT12[8]=sc*R.m[6]; outT12[9]=sc*R.m[7]; outT12[10]=sc*R.m[8]; outT12[11]=t.z;
    outT12[12]=0; outT12[13]=0; outT12[14]=0; outT12[15]=1;
}

static void identity_T(float outT12[16]) {
    for (int i = 0; i < 16; ++i) outT12[i] = (i%5==0)?1.0f:0.0f;
}

/* ----------------------------------------------------------------- iterate */
void ngd_sim3solver_iterate(ngd_sim3_solver* s, int nIterations, int* bNoMore,
                            char* vbInliers, int* nInliers, float outT12[16])
{
    *bNoMore = 0;
    if (vbInliers) for (int i = 0; i < s->nOrig; ++i) vbInliers[i] = 0;
    *nInliers = 0;

    if (s->N < s->minInliers) { *bNoMore = 1; identity_T(outT12); return; }

    int nCurrent = 0;
    while (s->mnIterations < s->maxIts && nCurrent < nIterations) {
        nCurrent++; s->mnIterations++;

        /* sample 3 distinct indices */
        int avail[256];
        int navail = s->N;
        if (s->N <= 256) { for (int i = 0; i < s->N; ++i) avail[i] = i; }
        double P1[9], P2[9];
        for (short i = 0; i < 3; ++i) {
            int randi = rng_randint(&s->rng, navail-1);
            int idx;
            if (s->N <= 256) {
                idx = avail[randi];
                avail[randi] = avail[navail-1]; navail--;
            } else {
                /* fall back to direct index sampling for large N */
                idx = rng_randint(&s->rng, s->N-1);
            }
            ngd_vec3 a = s->X3Dc1[idx], b = s->X3Dc2[idx];
            P1[0*3+i]=a.x; P1[1*3+i]=a.y; P1[2*3+i]=a.z;
            P2[0*3+i]=b.x; P2[1*3+i]=b.y; P2[2*3+i]=b.z;
        }

        compute_sim3(s, P1, P2);
        check_inliers(s);

        if (s->mnInliersi >= s->mnBestInliers) {
            store_best(s);
            if (s->mnInliersi > s->minInliers) {
                *nInliers = s->mnInliersi;
                if (vbInliers) {
                    for (int i = 0; i < s->N; ++i) {
                        if (s->mvbInliersi[i]) {
                            int oi = s->origIdx ? s->origIdx[i] : i;
                            if (oi >= 0 && oi < s->nOrig) vbInliers[oi] = 1;
                        }
                    }
                }
                to_T12(s, outT12);
                return;
            }
        }
    }
    if (s->mnIterations >= s->maxIts) *bNoMore = 1;
    identity_T(outT12);
}

void ngd_sim3solver_find(ngd_sim3_solver* s, char* vbInliers, int* nInliers, float outT12[16]) {
    int bNoMore;
    ngd_sim3solver_iterate(s, s->maxIts, &bNoMore, vbInliers, nInliers, outT12);
}

void ngd_sim3solver_destroy(ngd_sim3_solver* s) {
    if (!s) return;
    free(s->X3Dc1); free(s->X3Dc2); free(s->P1im1); free(s->P2im2);
    free(s->maxErr1); free(s->maxErr2);
    free(s->mvbInliersi); free(s->mvbBestInliers); free(s->mvAllIndices);
    free(s);
}
