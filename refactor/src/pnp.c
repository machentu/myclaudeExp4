/* ngd/pnp.c — EPnP + RANSAC (pure C), ported from OpenCV epnp.cpp.
 * See ngd/pnp.h for scope. Internal math in double for conditioning. */
#include "ngd/pnp.h"
#include "ngd/math.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

ngd_pnp_ransac_params ngd_pnp_ransac_defaults(void)
{
    ngd_pnp_ransac_params p;
    p.prob = 0.99;
    p.minInliers = 10;
    p.maxIterations = 300;
    p.minSet = 6;
    p.epsilon = 0.5f;
    p.th2 = 5.991f;
    return p;
}

/* ============================ linear algebra ============================ */

/* Solve A x = b (n×n, n<=16) via Gauss elimination with partial pivoting,
 * double precision. Returns 0 on success, -1 if singular. */
static int solve_linear_d(double *A, const double *b, double *x, int n)
{
    double Ab[16 * 17];
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) Ab[i * (n + 1) + j] = A[i * n + j];
        Ab[i * (n + 1) + n] = b[i];
    }
    for (int k = 0; k < n; ++k) {
        /* partial pivot */
        int piv = k;
        double best = fabs(Ab[k * (n + 1) + k]);
        for (int i = k + 1; i < n; ++i) {
            double v = fabs(Ab[i * (n + 1) + k]);
            if (v > best) { best = v; piv = i; }
        }
        if (best < 1e-12) return -1;
        if (piv != k) {
            for (int j = 0; j < n + 1; ++j) {
                double t = Ab[k * (n + 1) + j];
                Ab[k * (n + 1) + j] = Ab[piv * (n + 1) + j];
                Ab[piv * (n + 1) + j] = t;
            }
        }
        double inv = 1.0 / Ab[k * (n + 1) + k];
        for (int i = k + 1; i < n; ++i) {
            double f = Ab[i * (n + 1) + k] * inv;
            if (f == 0.0) continue;
            for (int j = k; j < n + 1; ++j)
                Ab[i * (n + 1) + j] -= f * Ab[k * (n + 1) + j];
        }
    }
    for (int i = n - 1; i >= 0; --i) {
        double s = Ab[i * (n + 1) + n];
        for (int j = i + 1; j < n; ++j) s -= Ab[i * (n + 1) + j] * x[j];
        x[i] = s / Ab[i * (n + 1) + i];
    }
    return 0;
}

/* Least-squares solve of A(m×k) x = b, m>=k, via normal equations
 * (A^T A) x = A^T b. Returns 0 on success. */
static int lstsq_d(const double *A, const double *b, int m, int k, double *x)
{
    double AtA[16 * 16], Atb[16];
    for (int i = 0; i < k; ++i) {
        for (int j = 0; j < k; ++j) {
            double s = 0.0;
            for (int r = 0; r < m; ++r) s += A[r * k + i] * A[r * k + j];
            AtA[i * k + j] = s;
        }
        double s = 0.0;
        for (int r = 0; r < m; ++r) s += A[r * k + i] * b[r];
        Atb[i] = s;
    }
    return solve_linear_d(AtA, Atb, x, k);
}

/* Cyclic Jacobi eigendecomposition of a symmetric n×n matrix (n<=12).
 * A (row-major, modified in place), V (eigenvectors as columns, n×n), w
 * (eigenvalues). On return, columns of V are eigenvectors, w[i] matches
 * V[:,i]. Sorts ascending by eigenvalue. */
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
                /* zero A[p][q]: tan(2θ) = 2·apq/(app−aqq) under J=[[c,-s],[s,c]] */
                double phi = 0.5 * atan2(2.0 * apq, app - aqq);
                double c = cos(phi), s = sin(phi);
                /* rotate A: rows/cols p,q */
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
                /* rotate V */
                for (int i = 0; i < n; ++i) {
                    double vip = V[i * n + p], viq = V[i * n + q];
                    V[i * n + p] = c * vip + s * viq;
                    V[i * n + q] = -s * vip + c * viq;
                }
            }
        }
    }
    for (int i = 0; i < n; ++i) w[i] = A[i * n + i];

    /* sort ascending (selection sort on eigenpairs) */
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

/* SVD of a general 3×3 A = U diag(S) V^T (S descending). U, V are column-major
 * here (U[:,k]=U[0..2][k] stored as U[3k+0..2]). Used by Kabsch. */
static void svd3(const double A[9], double U[9], double S[3], double V[9])
{
    /* ATA = A^T A (symmetric 3×3) */
    double ATA[9];
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            double s = 0.0;
            for (int k = 0; k < 3; ++k) s += A[k * 3 + i] * A[k * 3 + j];
            ATA[i * 3 + j] = s;
        }
    double Vm[9], w[3];
    jacobi_sym(ATA, 3, Vm, w);   /* ascending; V columns are right singular vectors */

    /* sort descending into S/V */
    int idx[3] = {0, 1, 2};
    for (int i = 0; i < 2; ++i)
        for (int j = i + 1; j < 3; ++j)
            if (w[idx[j]] > w[idx[i]]) { int t = idx[i]; idx[i] = idx[j]; idx[j] = t; }
    for (int k = 0; k < 3; ++k) {
        S[k] = (w[idx[k]] > 0.0) ? sqrt(w[idx[k]]) : 0.0;
        for (int r = 0; r < 3; ++r) V[r * 3 + k] = Vm[r * 3 + idx[k]];
    }
    /* U[:,k] = A V[:,k] / S[k] */
    for (int k = 0; k < 3; ++k) {
        double av[3];
        for (int r = 0; r < 3; ++r) {
            double s = 0.0;
            for (int c = 0; c < 3; ++c) s += A[r * 3 + c] * V[c * 3 + k];
            av[r] = s;
        }
        if (S[k] > 1e-9) {
            for (int r = 0; r < 3; ++r) U[r * 3 + k] = av[r] / S[k];
        } else {
            /* degenerate: pick a vector orthogonal to other U columns */
            double u[3] = {0, 0, 0};
            /* cross of U[:,other1] and U[:,other2] */
            int a = (k + 1) % 3, b = (k + 2) % 3;
            if (S[a] > 1e-9 && S[b] > 1e-9) {
                u[0] = U[3 * a + 1] * U[3 * b + 2] - U[3 * a + 2] * U[3 * b + 1];
                u[1] = U[3 * a + 2] * U[3 * b + 0] - U[3 * a + 0] * U[3 * b + 2];
                u[2] = U[3 * a + 0] * U[3 * b + 1] - U[3 * a + 1] * U[3 * b + 0];
            } else {
                u[k % 3] = 1.0;
            }
            double n = sqrt(u[0]*u[0]+u[1]*u[1]+u[2]*u[2]);
            if (n < 1e-12) n = 1.0;
            for (int r = 0; r < 3; ++r) U[r * 3 + k] = u[r] / n;
        }
    }
}

/* ============================ EPnP core ============================ */

typedef struct {
    int N;
    const ngd_vec3 *pws;     /* world points */
    const double *p2d;       /* length 2N: (u,v) doubles */
    double fu, fv, uc, vc;   /* intrinsics */
    double cws[4][3];        /* world control points (cc, c1..c3) */
    double ccs[4][3];        /* camera control points */
    double *alphas;          /* barycentric coords, N*4 (heap; caller sets N first) */
    double *pcs;             /* camera-frame 3D points, N×3 (scratch) */
    double ut[12 * 12];      /* 12 eigenvectors of MtM (rows) */
    double l_1, l_2, l_3, l_4; /* four smallest eigenvalues */
    double L6x10[6 * 10];
    double rho[6];
} epnp_ctx;

static void choose_control_points(epnp_ctx *e)
{
    /* cws[0] = centroid */
    for (int j = 0; j < 3; ++j) e->cws[0][j] = 0.0;
    for (int i = 0; i < e->N; ++i) {
        e->cws[0][0] += e->pws[i].x;
        e->cws[0][1] += e->pws[i].y;
        e->cws[0][2] += e->pws[i].z;
    }
    for (int j = 0; j < 3; ++j) e->cws[0][j] /= e->N;

    /* PCA via PW0^T PW0 (3×3 symmetric) */
    double PW0tPW0[9] = {0,0,0,0,0,0,0,0,0};
    for (int i = 0; i < e->N; ++i) {
        double dx = e->pws[i].x - e->cws[0][0];
        double dy = e->pws[i].y - e->cws[0][1];
        double dz = e->pws[i].z - e->cws[0][2];
        PW0tPW0[0] += dx*dx; PW0tPW0[1] += dx*dy; PW0tPW0[2] += dx*dz;
        PW0tPW0[3] += dy*dx; PW0tPW0[4] += dy*dy; PW0tPW0[5] += dy*dz;
        PW0tPW0[6] += dz*dx; PW0tPW0[7] += dz*dy; PW0tPW0[8] += dz*dz;
    }
    double Vm[9], w[3];
    jacobi_sym(PW0tPW0, 3, Vm, w);   /* ascending */
    /* take 3 largest eigenvectors (last 3 after ascending sort) */
    for (int i = 0; i < 3; ++i) {
        int src = 2 - i;   /* largest first */
        double k = (w[src] > 0.0) ? sqrt(w[src] / e->N) : 0.0;
        for (int j = 0; j < 3; ++j)
            e->cws[i + 1][j] = e->cws[0][j] + k * Vm[j * 3 + src];
    }
}

static void compute_barycentric(epnp_ctx *e)
{
    /* CC = [c1-c0, c2-c0, c3-c0] (3×3, columns) */
    double CC[9];
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            CC[i * 3 + j] = e->cws[j + 1][i] - e->cws[0][i];
    /* invert CC (3×3 closed form) */
    double a = CC[0], b = CC[1], c = CC[2];
    double d = CC[3], ee = CC[4], f = CC[5];
    double g = CC[6], h = CC[7], ii = CC[8];
    double det = a*(ee*ii - f*h) - b*(d*ii - f*g) + c*(d*h - ee*g);
    double inv[9];
    if (fabs(det) < 1e-12) {
        /* fallback: identity-ish so alphas don't blow up */
        memset(inv, 0, sizeof(inv));
        inv[0] = inv[4] = inv[8] = 1.0;
        det = 1.0;
    }
    double invd = 1.0 / det;
    inv[0] = (ee*ii - f*h) * invd;
    inv[1] = (c*h - b*ii) * invd;
    inv[2] = (b*f - c*ee) * invd;
    inv[3] = (f*g - d*ii) * invd;
    inv[4] = (a*ii - c*g) * invd;
    inv[5] = (c*d - a*f) * invd;
    inv[6] = (d*h - ee*g) * invd;
    inv[7] = (b*g - a*h) * invd;
    inv[8] = (a*ee - b*d) * invd;

    for (int i = 0; i < e->N; ++i) {
        double dx = e->pws[i].x - e->cws[0][0];
        double dy = e->pws[i].y - e->cws[0][1];
        double dz = e->pws[i].z - e->cws[0][2];
        double a1 = inv[0]*dx + inv[1]*dy + inv[2]*dz;
        double a2 = inv[3]*dx + inv[4]*dy + inv[5]*dz;
        double a3 = inv[6]*dx + inv[7]*dy + inv[8]*dz;
        e->alphas[i * 4 + 0] = 1.0 - a1 - a2 - a3;
        e->alphas[i * 4 + 1] = a1;
        e->alphas[i * 4 + 2] = a2;
        e->alphas[i * 4 + 3] = a3;
    }
}

static void fill_M(const epnp_ctx *e, double *M)  /* M: 2N×12 */
{
    for (int i = 0; i < e->N; ++i) {
        double u = e->p2d[2 * i], v = e->p2d[2 * i + 1];
        double *M1 = M + (2 * i) * 12;
        double *M2 = M + (2 * i + 1) * 12;
        for (int j = 0; j < 4; ++j) {
            double a = e->alphas[i * 4 + j];
            M1[3 * j + 0] = a * e->fu;
            M1[3 * j + 1] = 0.0;
            M1[3 * j + 2] = a * (e->uc - u);
            M2[3 * j + 0] = 0.0;
            M2[3 * j + 1] = a * e->fv;
            M2[3 * j + 2] = a * (e->vc - v);
        }
    }
}

static void compute_ccs(epnp_ctx *e, const double betas[4])
{
    /* ccs[j] = sum_i betas[i] * v_i[j], where v_i = eigenvector (11-i) of Ut
     * (the 4 smallest). jacobi_sym sorted ascending, so the 4 smallest are
     * rows 0..3 of ut. OpenCV indexes v[0..3] = eigvecs 11..8; ascending sort
     * gives smallest first → v[0]=row0, etc. */
    for (int j = 0; j < 4; ++j)
        for (int k = 0; k < 3; ++k)
            e->ccs[j][k] = 0.0;
    for (int i = 0; i < 4; ++i) {
        const double *v = e->ut + i * 12;   /* i-th smallest eigenvector */
        for (int j = 0; j < 4; ++j)
            for (int k = 0; k < 3; ++k)
                e->ccs[j][k] += betas[i] * v[3 * j + k];
    }
}

static void compute_pcs(epnp_ctx *e)
{
    for (int i = 0; i < e->N; ++i) {
        for (int k = 0; k < 3; ++k)
            e->pcs[i * 3 + k] =
                e->alphas[i * 4 + 0] * e->ccs[0][k] +
                e->alphas[i * 4 + 1] * e->ccs[1][k] +
                e->alphas[i * 4 + 2] * e->ccs[2][k] +
                e->alphas[i * 4 + 3] * e->ccs[3][k];
    }
}

static void solve_for_sign(epnp_ctx *e)
{
    if (e->N > 0 && e->pcs[2] < 0.0) {
        for (int j = 0; j < 4; ++j)
            for (int k = 0; k < 3; ++k) e->ccs[j][k] = -e->ccs[j][k];
        for (int i = 0; i < e->N * 3; ++i) e->pcs[i] = -e->pcs[i];
    }
}

/* Build L6x10 (6×10) and rho (6) from the 4 smallest eigenvectors and world
 * control-point distances (OpenCV compute_L_6x10 / compute_rho). */
static void compute_L6x10_and_rho(epnp_ctx *e)
{
    /* dv[k][i] = ccs-from-eigvec-k difference for pair i (6 pairs) */
    double dv[4][6][3];
    /* pairs: (0,1),(0,2),(0,3),(1,2),(1,3),(2,3) */
    int idx0[6] = {0,0,0,1,1,2}, idx1[6] = {1,2,3,2,3,3};
    for (int k = 0; k < 4; ++k) {
        const double *v = e->ut + k * 12;
        for (int p = 0; p < 6; ++p) {
            int a = idx0[p], b = idx1[p];
            for (int d = 0; d < 3; ++d)
                dv[k][p][d] = v[3 * a + d] - v[3 * b + d];
        }
    }
    for (int p = 0; p < 6; ++p) {
        double *row = e->L6x10 + p * 10;
        double d00 = dv[0][p][0]*dv[0][p][0] + dv[0][p][1]*dv[0][p][1] + dv[0][p][2]*dv[0][p][2];
        double d01 = 2*(dv[0][p][0]*dv[1][p][0] + dv[0][p][1]*dv[1][p][1] + dv[0][p][2]*dv[1][p][2]);
        double d11 = dv[1][p][0]*dv[1][p][0] + dv[1][p][1]*dv[1][p][1] + dv[1][p][2]*dv[1][p][2];
        double d02 = 2*(dv[0][p][0]*dv[2][p][0] + dv[0][p][1]*dv[2][p][1] + dv[0][p][2]*dv[2][p][2]);
        double d12 = 2*(dv[1][p][0]*dv[2][p][0] + dv[1][p][1]*dv[2][p][1] + dv[1][p][2]*dv[2][p][2]);
        double d22 = dv[2][p][0]*dv[2][p][0] + dv[2][p][1]*dv[2][p][1] + dv[2][p][2]*dv[2][p][2];
        double d03 = 2*(dv[0][p][0]*dv[3][p][0] + dv[0][p][1]*dv[3][p][1] + dv[0][p][2]*dv[3][p][2]);
        double d13 = 2*(dv[1][p][0]*dv[3][p][0] + dv[1][p][1]*dv[3][p][1] + dv[1][p][2]*dv[3][p][2]);
        double d23 = 2*(dv[2][p][0]*dv[3][p][0] + dv[2][p][1]*dv[3][p][1] + dv[2][p][2]*dv[3][p][2]);
        double d33 = dv[3][p][0]*dv[3][p][0] + dv[3][p][1]*dv[3][p][1] + dv[3][p][2]*dv[3][p][2];
        row[0]=d00; row[1]=d01; row[2]=d11; row[3]=d02; row[4]=d12;
        row[5]=d22; row[6]=d03; row[7]=d13; row[8]=d23; row[9]=d33;
    }
    for (int p = 0; p < 6; ++p) {
        int a = idx0[p], b = idx1[p];
        double dx = e->cws[a][0]-e->cws[b][0];
        double dy = e->cws[a][1]-e->cws[b][1];
        double dz = e->cws[a][2]-e->cws[b][2];
        e->rho[p] = dx*dx + dy*dy + dz*dz;
    }
}

static void compute_betas_approx_1(const epnp_ctx *e, double betas[4])
{
    /* cols 0,1,3,6 → 6×4 system L4 x = rho */
    int cols[4] = {0, 1, 3, 6};
    double L4[6 * 4];
    for (int r = 0; r < 6; ++r)
        for (int c = 0; c < 4; ++c) L4[r * 4 + c] = e->L6x10[r * 10 + cols[c]];
    double x[4] = {0,0,0,0};
    if (lstsq_d(L4, e->rho, 6, 4, x) != 0) { for(int i=0;i<4;++i) betas[i]=0; return; }
    if (x[0] < 0) {
        betas[0] = sqrt(-x[0]);
        betas[1] = -x[1] / betas[0];
        betas[2] = -x[2] / betas[0];
        betas[3] = -x[3] / betas[0];
    } else {
        betas[0] = sqrt(x[0]);
        betas[1] = x[1] / betas[0];
        betas[2] = x[2] / betas[0];
        betas[3] = x[3] / betas[0];
    }
}

static void compute_betas_approx_2(const epnp_ctx *e, double betas[4])
{
    int cols[3] = {0, 1, 2};
    double L3[6 * 3];
    for (int r = 0; r < 6; ++r)
        for (int c = 0; c < 3; ++c) L3[r * 3 + c] = e->L6x10[r * 10 + cols[c]];
    double x[3] = {0,0,0};
    if (lstsq_d(L3, e->rho, 6, 3, x) != 0) { for(int i=0;i<4;++i) betas[i]=0; return; }
    betas[0] = betas[1] = betas[2] = betas[3] = 0.0;
    if (x[0] < 0) {
        betas[0] = sqrt(-x[0]);
        betas[1] = (x[2] < 0) ? sqrt(-x[2]) : 0.0;
    } else {
        betas[0] = sqrt(x[0]);
        betas[1] = (x[2] > 0) ? sqrt(x[2]) : 0.0;
    }
    if (x[1] < 0) betas[0] = -betas[0];
}

static void compute_betas_approx_3(const epnp_ctx *e, double betas[4])
{
    int cols[5] = {0, 1, 2, 3, 4};
    double L5[6 * 5];
    for (int r = 0; r < 6; ++r)
        for (int c = 0; c < 5; ++c) L5[r * 5 + c] = e->L6x10[r * 10 + cols[c]];
    double x[5] = {0,0,0,0,0};
    if (lstsq_d(L5, e->rho, 6, 5, x) != 0) { for(int i=0;i<4;++i) betas[i]=0; return; }
    betas[0] = betas[1] = betas[2] = betas[3] = 0.0;
    if (x[0] < 0) {
        betas[0] = sqrt(-x[0]);
        betas[1] = (x[2] < 0) ? sqrt(-x[2]) : 0.0;
    } else {
        betas[0] = sqrt(x[0]);
        betas[1] = (x[2] > 0) ? sqrt(x[2]) : 0.0;
    }
    if (x[1] < 0) betas[0] = -betas[0];
    betas[2] = x[3] / betas[0];
    betas[3] = 0.0;
}

/* Gauss-Newton refinement of betas against L6x10·βproducts = rho (5 iters). */
static void gauss_newton(epnp_ctx *e, double betas[4])
{
    for (int iter = 0; iter < 5; ++iter) {
        double A[6 * 4], b[6];
        for (int i = 0; i < 6; ++i) {
            const double *L = e->L6x10 + i * 10;
            A[i * 4 + 0] = 2*L[0]*betas[0] + L[1]*betas[1] + L[3]*betas[2] + L[6]*betas[3];
            A[i * 4 + 1] = L[1]*betas[0] + 2*L[2]*betas[1] + L[4]*betas[2] + L[7]*betas[3];
            A[i * 4 + 2] = L[3]*betas[0] + L[4]*betas[1] + 2*L[5]*betas[2] + L[8]*betas[3];
            A[i * 4 + 3] = L[6]*betas[0] + L[7]*betas[1] + L[8]*betas[2] + 2*L[9]*betas[3];
            double cur =
                L[0]*betas[0]*betas[0] + 2*L[1]*betas[0]*betas[1] + L[2]*betas[1]*betas[1] +
                2*L[3]*betas[0]*betas[2] + 2*L[4]*betas[1]*betas[2] + L[5]*betas[2]*betas[2] +
                2*L[6]*betas[0]*betas[3] + 2*L[7]*betas[1]*betas[3] + 2*L[8]*betas[2]*betas[3] +
                L[9]*betas[3]*betas[3];
            b[i] = e->rho[i] - cur;
        }
        double dx[4] = {0,0,0,0};
        if (lstsq_d(A, b, 6, 4, dx) != 0) break;
        for (int k = 0; k < 4; ++k) betas[k] += dx[k];
    }
}

/* Kabsch: align world points pw to camera points pc → R, t (pw in world frame,
 * pc in camera frame; we want R,t s.t. R*pw + t ≈ pc). */
static void estimate_R_and_t(epnp_ctx *e, double R[9], double t[3])
{
    double pc0[3] = {0,0,0}, pw0[3] = {0,0,0};
    for (int i = 0; i < e->N; ++i) {
        pc0[0] += e->pcs[i*3+0]; pc0[1] += e->pcs[i*3+1]; pc0[2] += e->pcs[i*3+2];
        pw0[0] += e->pws[i].x;   pw0[1] += e->pws[i].y;   pw0[2] += e->pws[i].z;
    }
    for (int k = 0; k < 3; ++k) { pc0[k] /= e->N; pw0[k] /= e->N; }
    /* ABt = Σ (pc-pc0)(pw-pw0)^T  (3×3) */
    double ABt[9] = {0,0,0,0,0,0,0,0,0};
    for (int i = 0; i < e->N; ++i) {
        double pcx = e->pcs[i*3+0]-pc0[0], pcy = e->pcs[i*3+1]-pc0[1], pcz = e->pcs[i*3+2]-pc0[2];
        double pwx = e->pws[i].x-pw0[0], pwy = e->pws[i].y-pw0[1], pwz = e->pws[i].z-pw0[2];
        ABt[0]+=pcx*pwx; ABt[1]+=pcx*pwy; ABt[2]+=pcx*pwz;
        ABt[3]+=pcy*pwx; ABt[4]+=pcy*pwy; ABt[5]+=pcy*pwz;
        ABt[6]+=pcz*pwx; ABt[7]+=pcz*pwy; ABt[8]+=pcz*pwz;
    }
    double U[9], S[3], V[9];
    svd3(ABt, U, S, V);
    /* R = U V^T */
    double Vt[9];
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            Vt[i*3+j] = V[j*3+i];
    double dR[9];
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            double s = 0.0;
            for (int k = 0; k < 3; ++k) s += U[i*3+k]*Vt[k*3+j];
            dR[i*3+j] = s;
        }
    /* det fix: if det<0, flip third row */
    double det = dR[0]*(dR[4]*dR[8]-dR[5]*dR[7]) - dR[1]*(dR[3]*dR[8]-dR[5]*dR[6]) + dR[2]*(dR[3]*dR[7]-dR[4]*dR[6]);
    if (det < 0) { dR[6]=-dR[6]; dR[7]=-dR[7]; dR[8]=-dR[8]; }
    for (int i = 0; i < 9; ++i) R[i] = dR[i];
    t[0] = pc0[0] - (R[0]*pw0[0]+R[1]*pw0[1]+R[2]*pw0[2]);
    t[1] = pc0[1] - (R[3]*pw0[0]+R[4]*pw0[1]+R[5]*pw0[2]);
    t[2] = pc0[2] - (R[6]*pw0[0]+R[7]*pw0[1]+R[8]*pw0[2]);
}

/* Reprojection error (sum of squared pixel residuals) for current R,t. */
static double repro_error(const epnp_ctx *e, const double R[9], const double t[3])
{
    double err = 0.0;
    for (int i = 0; i < e->N; ++i) {
        double X = e->pws[i].x, Y = e->pws[i].y, Z = e->pws[i].z;
        double Xc = R[0]*X+R[1]*Y+R[2]*Z + t[0];
        double Yc = R[3]*X+R[4]*Y+R[5]*Z + t[1];
        double Zc = R[6]*X+R[7]*Y+R[8]*Z + t[2];
        if (Zc <= 0) { err += 1e6; continue; }
        double u = e->fu * Xc / Zc + e->uc;
        double v = e->fv * Yc / Zc + e->vc;
        double du = u - e->p2d[2*i], dv = v - e->p2d[2*i+1];
        err += du*du + dv*dv;
    }
    return err;
}

/* Run EPnP on the ctx's current correspondences. Fills *outR, *outT.
 * Returns 1 on success. */
static int epnp_compute_pose(epnp_ctx *e, double R[9], double t[3])
{
    double *M = (double*)malloc((size_t)(2 * e->N) * 12 * sizeof(double));
    e->alphas = (double*)malloc((size_t)e->N * 4 * sizeof(double));
    if (!M || !e->alphas) { free(M); free(e->alphas); e->alphas = NULL; return 0; }

    choose_control_points(e);
    compute_barycentric(e);
    fill_M(e, M);

    /* MtM = M^T M (12×12) */
    double MtM[144];
    for (int i = 0; i < 12; ++i)
        for (int j = 0; j < 12; ++j) {
            double s = 0.0;
            for (int r = 0; r < 2*e->N; ++r) s += M[r*12+i] * M[r*12+j];
            MtM[i*12+j] = s;
        }
    free(M);

    double V[144], w[12];
    jacobi_sym(MtM, 12, V, w);   /* ascending → 4 smallest = rows 0..3 */
    /* store eigenvectors as rows in ut (ut[i*12 + k] = V[k*12 + i]) */
    for (int i = 0; i < 12; ++i)
        for (int k = 0; k < 12; ++k)
            e->ut[i*12+k] = V[k*12+i];
    e->l_1 = w[0]; e->l_2 = w[1]; e->l_3 = w[2]; e->l_4 = w[3];

    compute_L6x10_and_rho(e);

    double bestErr = 1e18;
    double bestR[9], bestT[3];
    int found = 0;
    /* try the 3 approx methods (+GN), keep best repro */
    for (int method = 1; method <= 3; ++method) {
        double betas[4];
        if (method == 1) compute_betas_approx_1(e, betas);
        else if (method == 2) compute_betas_approx_2(e, betas);
        else compute_betas_approx_3(e, betas);
        gauss_newton(e, betas);
        compute_ccs(e, betas);
        compute_pcs(e);
        solve_for_sign(e);
        double Rk[9], tk[3];
        estimate_R_and_t(e, Rk, tk);
        double err = repro_error(e, Rk, tk);
        if (err < bestErr) {
            bestErr = err;
            memcpy(bestR, Rk, sizeof(bestR));
            memcpy(bestT, tk, sizeof(bestT));
            found = 1;
        }
    }
    if (!found) { free(e->alphas); e->alphas = NULL; return 0; }
    memcpy(R, bestR, 9*sizeof(double));
    memcpy(t, bestT, 3*sizeof(double));
    free(e->alphas); e->alphas = NULL;
    return 1;
}

/* ============================ public API ============================ */

int ngd_epnp_solve(const ngd_vec3 *p3d, const float *p2d, int N,
                   const ngd_cam_ctx *cam, ngd_se3 *outTcw)
{
    if (N < 6 || !p3d || !p2d || !cam || !outTcw) return 0;
    epnp_ctx e;
    memset(&e, 0, sizeof(e));
    e.N = N;
    e.pws = p3d;
    double *p2dd = (double*)malloc((size_t)N * 2 * sizeof(double));
    e.pcs = (double*)malloc((size_t)N * 3 * sizeof(double));
    if (!p2dd || !e.pcs) { free(p2dd); free(e.pcs); return 0; }
    for (int i = 0; i < 2*N; ++i) p2dd[i] = p2d[i];
    e.p2d = p2dd;
    e.fu = cam->cam.fx; e.fv = cam->cam.fy;
    e.uc = cam->cam.cx; e.vc = cam->cam.cy;

    double R[9], t[3];
    int ok = epnp_compute_pose(&e, R, t);
    free(p2dd); free(e.pcs);
    if (!ok) return 0;

    ngd_mat3 Rm;
    for (int i = 0; i < 9; ++i) Rm.m[i] = (float)R[i];
    /* nearest proper rotation (guard against slight non-orthogonality) */
    ngd_quat q = ngd_quat_from_matrix(Rm);
    outTcw->q = q;
    outTcw->t = ngd_v3((float)t[0], (float)t[1], (float)t[2]);
    return 1;
}

/* classify inliers given R,t; returns count */
static int classify_inliers(const epnp_ctx *e, const double R[9], const double t[3],
                            const float *sigma2, float th2, int *inliers)
{
    int cnt = 0;
    for (int i = 0; i < e->N; ++i) {
        double X = e->pws[i].x, Y = e->pws[i].y, Z = e->pws[i].z;
        double Xc = R[0]*X+R[1]*Y+R[2]*Z + t[0];
        double Yc = R[3]*X+R[4]*Y+R[5]*Z + t[1];
        double Zc = R[6]*X+R[7]*Y+R[8]*Z + t[2];
        inliers[i] = 0;
        if (Zc <= 0) continue;
        double u = e->fu * Xc / Zc + e->uc;
        double v = e->fv * Yc / Zc + e->vc;
        double du = u - e->p2d[2*i], dv = v - e->p2d[2*i+1];
        double chi2 = du*du + dv*dv;
        double th = sigma2[i] * th2;
        if (chi2 < th) { inliers[i] = 1; ++cnt; }
    }
    return cnt;
}

int ngd_pnp_solve_ransac(const ngd_vec3 *p3d, const float *p2d, const float *sigma2,
                         int N, const ngd_cam_ctx *cam,
                         const ngd_pnp_ransac_params *p,
                         ngd_se3 *outTcw, int *outInliers, int *outNInliers)
{
    ngd_pnp_ransac_params def;
    if (!p) { def = ngd_pnp_ransac_defaults(); p = &def; }
    if (N < p->minSet || !p3d || !p2d || !sigma2 || !cam || !outTcw) return 0;

    epnp_ctx e;
    memset(&e, 0, sizeof(e));
    e.N = N;
    e.pws = p3d;
    e.pcs = (double*)malloc((size_t)N * 3 * sizeof(double));
    double *p2dd = (double*)malloc((size_t)N * 2 * sizeof(double));
    int *sample = (int*)malloc((size_t)p->minSet * sizeof(int));
    int *inliers = (int*)malloc((size_t)N * sizeof(int));
    int *bestInliers = (int*)malloc((size_t)N * sizeof(int));
    if (!e.pcs || !p2dd || !sample || !inliers || !bestInliers) {
        free(e.pcs); free(p2dd); free(sample); free(inliers); free(bestInliers);
        return 0;
    }
    for (int i = 0; i < 2*N; ++i) p2dd[i] = p2d[i];
    e.p2d = p2dd;
    e.fu = cam->cam.fx; e.fv = cam->cam.fy;
    e.uc = cam->cam.cx; e.vc = cam->cam.cy;

    /* RANSAC iteration count (MLPnPsolver.cpp:253) */
    double prob = p->prob; float eps = p->epsilon;
    double denom = log(1.0 - pow((double)eps, p->minSet));
    int nIters = (denom < 0) ? (int)ceil(log(1.0 - prob) / denom) : p->maxIterations;
    if (nIters > p->maxIterations) nIters = p->maxIterations;
    if (nIters < 1) nIters = 1;

    srand(12345);   /* deterministic */

    int bestInlierCount = 0;
    double bestR[9] = {1,0,0,0,1,0,0,0,1}, bestT[3] = {0,0,0};

    /* RANSAC needs ≥ minSet distinct indices; build a per-iteration sample of
     * the full correspondence set (EPnP uses the ctx's N points, so we swap the
     * ctx to point at a scratch subset array). */
    ngd_vec3 *sub = (ngd_vec3*)malloc((size_t)p->minSet * sizeof(ngd_vec3));
    double *subp2d = (double*)malloc((size_t)p->minSet * 2 * sizeof(double));
    if (!sub || !subp2d) {
        free(e.pcs); free(p2dd); free(sample); free(inliers); free(bestInliers);
        free(sub); free(subp2d);
        return 0;
    }

    for (int it = 0; it < nIters && bestInlierCount < p->minInliers * 3; ++it) {
        /* sample minSet distinct indices */
        for (int k = 0; k < p->minSet; ++k) {
            int idx;
            int tries = 0;
            do {
                idx = rand() % N;
                int dup = 0;
                for (int j = 0; j < k; ++j) if (sample[j] == idx) { dup = 1; break; }
                if (!dup) break;
            } while (++tries < 100);
            sample[k] = idx;
            sub[k] = p3d[idx];
            subp2d[2*k] = p2dd[2*idx];
            subp2d[2*k+1] = p2dd[2*idx+1];
        }
        /* run EPnP on the sample */
        epnp_ctx es;
        memset(&es, 0, sizeof(es));
        es.N = p->minSet;
        es.pws = sub;
        es.p2d = subp2d;
        es.pcs = (double*)malloc((size_t)p->minSet * 3 * sizeof(double));
        if (!es.pcs) continue;
        es.fu = e.fu; es.fv = e.fv; es.uc = e.uc; es.vc = e.vc;

        double R[9], t[3];
        if (!epnp_compute_pose(&es, R, t)) { free(es.pcs); continue; }
        free(es.pcs);

        /* classify against the FULL set */
        int cnt = classify_inliers(&e, R, t, sigma2, p->th2, inliers);
        if (cnt > bestInlierCount) {
            bestInlierCount = cnt;
            memcpy(bestR, R, sizeof(bestR));
            memcpy(bestT, t, sizeof(bestT));
            memcpy(bestInliers, inliers, (size_t)N * sizeof(int));
        }
    }

    free(sub); free(subp2d);

    int ok = 0;
    if (bestInlierCount >= p->minInliers) {
        /* refine: EPnP on all best inliers */
        ngd_vec3 *in3d = (ngd_vec3*)malloc((size_t)bestInlierCount * sizeof(ngd_vec3));
        double *in2d = (double*)malloc((size_t)bestInlierCount * 2 * sizeof(double));
        if (in3d && in2d) {
            int m = 0;
            for (int i = 0; i < N; ++i) if (bestInliers[i]) {
                in3d[m] = p3d[i];
                in2d[2*m] = p2dd[2*i];
                in2d[2*m+1] = p2dd[2*i+1];
                ++m;
            }
            epnp_ctx er;
            memset(&er, 0, sizeof(er));
            er.N = m;
            er.pws = in3d;
            er.p2d = in2d;
            er.pcs = (double*)malloc((size_t)m * 3 * sizeof(double));
            if (er.pcs) {
                er.fu = e.fu; er.fv = e.fv; er.uc = e.uc; er.vc = e.vc;
                double R[9], t[3];
                if (epnp_compute_pose(&er, R, t)) {
                    memcpy(bestR, R, sizeof(bestR));
                    memcpy(bestT, t, sizeof(bestT));
                    /* reclassify final */
                    bestInlierCount = classify_inliers(&e, R, t, sigma2, p->th2, bestInliers);
                    ok = 1;
                }
                free(er.pcs);
            }
        }
        free(in3d); free(in2d);
    }

    if (outInliers) memcpy(outInliers, bestInliers, (size_t)N * sizeof(int));
    if (outNInliers) *outNInliers = bestInlierCount;

    if (ok || bestInlierCount >= p->minInliers) {
        ngd_mat3 Rm;
        for (int i = 0; i < 9; ++i) Rm.m[i] = (float)bestR[i];
        outTcw->q = ngd_quat_from_matrix(Rm);
        outTcw->t = ngd_v3((float)bestT[0], (float)bestT[1], (float)bestT[2]);
        ok = 1;
    }

    free(e.pcs); free(p2dd); free(sample); free(inliers); free(bestInliers);
    return ok ? 1 : 0;
}
