/* ngd/ba_sparse.c — block-sparse pose solver for the Schur BA (see ba_sparse.h).
 *
 * Replaces ba.c's dense pose system (Hpp/Hschur dim_p x dim_p + dense LU) with:
 *   - 6x6 block-sparse upper-triangular CSR pose Hessian; block (i,j) is
 *     nonzero only if free poses i,j co-observe some point. Pattern built
 *     ONCE from all edges (superset of any per-iteration assembly, since
 *     degenerate edges can re-enter after an accept).
 *   - Schur complement accumulated straight into the sparse blocks; each
 *     UNORDERED pose pair {a,b} contributes S = B_a D B_b^T once (the (b,a)
 *     entry is S^T implicitly, halving the pair products vs the dense path).
 *   - Up-looking block Cholesky (A = Lc Lc^T, 6x6 diagonal blocks factored
 *     with chol6, natural pose order) + forward/back substitution for dxp.
 *     Per LM trial only the nnzA block values are copied — no dense dim_p^2
 *     memset/memcpy/malloc, no O(dim_p^3) LU.
 *
 * Everything else (LM schedule, Nielsen ratio, Huber IRLS, lambda init,
 * re-linearisation points, outlier classification, float<->double boundary)
 * is line-for-line ngd_ba_optimize in ba.c. Helpers quat_to_R_d/se3_exp_d/
 * se3_mul_d/se3_deriv_d/inv3_d/edge_linearize are local mirrors of ba.c so
 * this file stays self-contained (precedent: ba.c mirrors se3.c).
 *
 * Solver-failure semantics: a non-PD 6x6 pivot or non-finite value aborts the
 * factorisation; the caller bumps lambda and retries — the same rescue the
 * dense path gets from solve_dense_d returning -1.
 */
#include "ngd/ba_sparse.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define NGD_BA_DELTA_MONO_SQ   5.991
#define NGD_BA_DELTA_STEREO_SQ 7.815
#define NGD_BA_TAU             1e-5
#define NGD_BA_MAX_TRIALS      10

/* ------------------------------------------------------------------ *
 * Double-precision SE3 / camera helpers (mirrors of ba.c).
 * ------------------------------------------------------------------ */

static void quat_to_R_d(const double q[4], double R[9]) {
    double w=q[0], x=q[1], y=q[2], z=q[3];
    R[0]=1-2*(y*y+z*z); R[1]=2*(x*y-z*w);   R[2]=2*(x*z+y*w);
    R[3]=2*(x*y+z*w);   R[4]=1-2*(x*x+z*z); R[5]=2*(y*z-x*w);
    R[6]=2*(x*z-y*w);   R[7]=2*(y*z+x*w);   R[8]=1-2*(x*x+y*y);
}

static void se3_exp_d(const double xi[6], double q[4], double t[3]) {
    double ox=xi[0], oy=xi[1], oz=xi[2];
    double ux=xi[3], uy=xi[4], uz=xi[5];
    double theta = sqrt(ox*ox+oy*oy+oz*oz);
    double Omega[9] = {0,-oz,oy, oz,0,-ox, -oy,ox,0};
    double O2[9];
    for (int i=0;i<3;++i) for (int j=0;j<3;++j) {
        double s=0; for (int k=0;k<3;++k) s += Omega[i*3+k]*Omega[k*3+j]; O2[i*3+j]=s;
    }
    double R[9], V[9];
    if (theta < 1e-9) {
        for (int i=0;i<9;++i){ R[i]=(i%4==0)?1.0:0.0; R[i]+=Omega[i]+O2[i]; V[i]=R[i]; }
    } else {
        double s=sin(theta)/theta, c=(1-cos(theta))/(theta*theta), v=(theta-sin(theta))/(theta*theta*theta);
        for (int i=0;i<9;++i){ R[i]=(i%4==0)?1.0:0.0; R[i]+=s*Omega[i]+c*O2[i]; }
        for (int i=0;i<9;++i){ V[i]=(i%4==0)?1.0:0.0; V[i]+=c*Omega[i]+v*O2[i]; }
    }
    double tr = R[0]+R[4]+R[8];
    if (tr > 0) {
        double s = 0.5/sqrt(tr+1.0);
        q[0]=0.25/s; q[1]=(R[7]-R[5])*s; q[2]=(R[2]-R[6])*s; q[3]=(R[3]-R[1])*s;
    } else if (R[0]>R[4] && R[0]>R[8]) {
        double s = 2.0*sqrt(1.0+R[0]-R[4]-R[8]);
        q[0]=(R[7]-R[5])/s; q[1]=0.25*s; q[2]=(R[1]+R[3])/s; q[3]=(R[2]+R[6])/s;
    } else if (R[4]>R[8]) {
        double s = 2.0*sqrt(1.0+R[4]-R[0]-R[8]);
        q[0]=(R[2]-R[6])/s; q[1]=(R[1]+R[3])/s; q[2]=0.25*s; q[3]=(R[5]+R[7])/s;
    } else {
        double s = 2.0*sqrt(1.0+R[8]-R[0]-R[4]);
        q[0]=(R[3]-R[1])/s; q[1]=(R[2]+R[6])/s; q[2]=(R[5]+R[7])/s; q[3]=0.25*s;
    }
    double n=sqrt(q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3]);
    if (n>0) for (int i=0;i<4;++i) q[i]/=n;
    t[0]=V[0]*ux+V[1]*uy+V[2]*uz;
    t[1]=V[3]*ux+V[4]*uy+V[5]*uz;
    t[2]=V[6]*ux+V[7]*uy+V[8]*uz;
}

static void se3_mul_d(const double qa[4], const double ta[3],
                      const double qb[4], const double tb[3],
                      double qo[4], double to[3]) {
    double Ra[9]; quat_to_R_d(qa, Ra);
    to[0]=ta[0]+Ra[0]*tb[0]+Ra[1]*tb[1]+Ra[2]*tb[2];
    to[1]=ta[1]+Ra[3]*tb[0]+Ra[4]*tb[1]+Ra[5]*tb[2];
    to[2]=ta[2]+Ra[6]*tb[0]+Ra[7]*tb[1]+Ra[8]*tb[2];
    double aw=qa[0],ax=qa[1],ay=qa[2],az=qa[3];
    double bw=qb[0],bx=qb[1],by=qb[2],bz=qb[3];
    qo[0]=aw*bw-ax*bx-ay*by-az*bz;
    qo[1]=aw*bx+ax*bw+ay*bz-az*by;
    qo[2]=aw*by-ax*bz+ay*bw+az*bx;
    qo[3]=aw*bz+ax*by-ay*bx+az*bw;
    double n=sqrt(qo[0]*qo[0]+qo[1]*qo[1]+qo[2]*qo[2]+qo[3]*qo[3]);
    if (n>0) for (int i=0;i<4;++i) qo[i]/=n;
}

static void se3_deriv_d(const double Xc[3], double out[18]) {
    out[0]=0;     out[1]=Xc[2];  out[2]=-Xc[1];  out[3]=1; out[4]=0; out[5]=0;
    out[6]=-Xc[2];out[7]=0;      out[8]=Xc[0];   out[9]=0; out[10]=1;out[11]=0;
    out[12]=Xc[1];out[13]=-Xc[0];out[14]=0;      out[15]=0;out[16]=0;out[17]=1;
}

static int inv3_d(const double A[9], double Ai[9]) {
    double det = A[0]*(A[4]*A[8]-A[5]*A[7])
               - A[1]*(A[3]*A[8]-A[5]*A[6])
               + A[2]*(A[3]*A[7]-A[4]*A[6]);
    if (fabs(det) < 1e-18) return -1;
    double inv = 1.0/det;
    Ai[0]=( A[4]*A[8]-A[5]*A[7])*inv;
    Ai[1]=(-A[1]*A[8]+A[2]*A[7])*inv;
    Ai[2]=( A[1]*A[5]-A[2]*A[4])*inv;
    Ai[3]=(-A[3]*A[8]+A[5]*A[6])*inv;
    Ai[4]=( A[0]*A[8]-A[2]*A[6])*inv;
    Ai[5]=(-A[0]*A[5]+A[2]*A[3])*inv;
    Ai[6]=( A[3]*A[7]-A[4]*A[6])*inv;
    Ai[7]=(-A[0]*A[7]+A[1]*A[6])*inv;
    Ai[8]=( A[0]*A[4]-A[1]*A[3])*inv;
    return 0;
}

/* ------------------------------------------------------------------ *
 * Per-edge linearisation (mirror of ba.c) + residual-only variant used
 * for trial-chi2 / outlier classification (chi2 identical; Jacobians
 * skipped — trial states never need them, an accept re-linearises fresh).
 * ------------------------------------------------------------------ */
static int edge_linearize(const ngd_ba_problem *p,
                          const double *poseq, const double *poset,
                          const double *Xw,
                          int stereo, int octave,
                          const float obs[3],
                          double r[3], double *chi2, double Jp[18], double Jl[9])
{
    double R[9]; quat_to_R_d(poseq, R);
    double Xc[3] = { R[0]*Xw[0]+R[1]*Xw[1]+R[2]*Xw[2]+poset[0],
                     R[3]*Xw[0]+R[4]*Xw[1]+R[5]*Xw[2]+poset[1],
                     R[6]*Xw[0]+R[7]*Xw[1]+R[8]*Xw[2]+poset[2] };
    if (Xc[2] <= 1e-9) return 0;
    double fx=p->cam->fx, fy=p->cam->fy, cx=p->cam->cx, cy=p->cam->cy;
    double invz=1.0/Xc[2], invz2=invz*invz;
    int dim = stereo ? 3 : 2;

    double Pjac[9];
    Pjac[0]=fx*invz;  Pjac[1]=0;           Pjac[2]=-fx*Xc[0]*invz2;
    Pjac[3]=0;        Pjac[4]=fy*invz;     Pjac[5]=-fy*Xc[1]*invz2;
    if (stereo) { Pjac[6]=fx*invz; Pjac[7]=0; Pjac[8]=-fx*Xc[0]*invz2 + p->bf*invz2; }

    double o0=obs[0], o1=obs[1], o2=obs[2];
    double u = fx*Xc[0]*invz + cx;
    double v = fy*Xc[1]*invz + cy;
    r[0]=o0-u; r[1]=o1-v;
    if (stereo) r[2]=o2-(u - p->bf*invz);

    int lvl = octave; if (lvl<0||lvl>=p->calib->nlevels) lvl=0;
    double invS2 = (double)p->calib->mvInvLevelSigma2[lvl];
    double rr = r[0]*r[0]+r[1]*r[1]; if (stereo) rr += r[2]*r[2];
    *chi2 = invS2*rr;

    for (int rr2=0; rr2<dim; ++rr2)
        for (int cc=0; cc<3; ++cc) {
            double s=0; for (int k=0;k<3;++k) s += Pjac[rr2*3+k]*R[k*3+cc];
            Jl[rr2*3+cc] = -s;
        }
    double sd[18]; se3_deriv_d(Xc, sd);
    for (int rr2=0; rr2<dim; ++rr2)
        for (int cc=0; cc<6; ++cc) {
            double s=0; for (int k=0;k<3;++k) s += Pjac[rr2*3+k]*sd[k*6+cc];
            Jp[rr2*6+cc] = -s;
        }
    return dim;
}

static int edge_residual(const ngd_ba_problem *p,
                         const double *poseq, const double *poset,
                         const double *Xw,
                         int stereo, int octave,
                         const float obs[3],
                         double *chi2)
{
    double R[9]; quat_to_R_d(poseq, R);
    double Xc[3] = { R[0]*Xw[0]+R[1]*Xw[1]+R[2]*Xw[2]+poset[0],
                     R[3]*Xw[0]+R[4]*Xw[1]+R[5]*Xw[2]+poset[1],
                     R[6]*Xw[0]+R[7]*Xw[1]+R[8]*Xw[2]+poset[2] };
    if (Xc[2] <= 1e-9) return 0;
    double fx=p->cam->fx, fy=p->cam->fy, cx=p->cam->cx, cy=p->cam->cy;
    double invz=1.0/Xc[2];
    double u = fx*Xc[0]*invz + cx;
    double v = fy*Xc[1]*invz + cy;
    double r0=obs[0]-u, r1=obs[1]-v, r2=0.0;
    if (stereo) r2=obs[2]-(u - p->bf*invz);
    int lvl = octave; if (lvl<0||lvl>=p->calib->nlevels) lvl=0;
    double invS2 = (double)p->calib->mvInvLevelSigma2[lvl];
    double rr = r0*r0+r1*r1; if (stereo) rr += r2*r2;
    *chi2 = invS2*rr;
    return stereo ? 3 : 2;
}

static double huber_rho0(double chi2, double delta) {
    if (chi2 <= delta*delta) return chi2;
    return 2.0*delta*sqrt(chi2) - delta*delta;
}
static double huber_rho1(double chi2, double delta) {
    if (chi2 <= delta*delta) return 1.0;
    return delta/sqrt(chi2);
}

/* ------------------------------------------------------------------ *
 * 6x6 block helpers (row-major). g_mults counts multiplies for stats.
 * ------------------------------------------------------------------ */
static long long g_mults;

static void mulT6_sub(double *A, const double *B, const double *C) {
    /* A -= B * C^T */
    for (int r=0;r<6;++r) for (int c=0;c<6;++c) {
        double s=0; for (int k=0;k<6;++k) s += B[r*6+k]*C[c*6+k];
        A[r*6+c]-=s;
    }
    g_mults += 216;
}
static void mul6v_sub(double *y, const double *A, const double *v) {
    /* y -= A * v */
    for (int r=0;r<6;++r) y[r] -= A[r*6+0]*v[0]+A[r*6+1]*v[1]+A[r*6+2]*v[2]
                              + A[r*6+3]*v[3]+A[r*6+4]*v[4]+A[r*6+5]*v[5];
    g_mults += 36;
}
static void mulT6v_sub(double *y, const double *A, const double *v) {
    /* y -= A^T * v */
    for (int c=0;c<6;++c) y[c] -= A[0*6+c]*v[0]+A[1*6+c]*v[1]+A[2*6+c]*v[2]
                               + A[3*6+c]*v[3]+A[4*6+c]*v[4]+A[5*6+c]*v[5];
    g_mults += 36;
}
static void mul63(const double *A, const double *B, double *C) {
    /* C = A(6x3) * B(3x3) */
    for (int r=0;r<6;++r) for (int c=0;c<3;++c) {
        double s=0; for (int k=0;k<3;++k) s += A[r*3+k]*B[k*3+c];
        C[r*3+c]=s;
    }
}
static void mul636T(const double *A, const double *B, double *C) {
    /* C = A(6x3) * B(6x3)^T */
    for (int r=0;r<6;++r) for (int c=0;c<6;++c) {
        double s=0; for (int k=0;k<3;++k) s += A[r*3+k]*B[c*3+k];
        C[r*6+c]=s;
    }
}

/* Cholesky of a symmetric 6x6. C lower. 0 ok, -1 non-PD / non-finite. */
static int chol6(const double *A, double *C) {
    for (int j=0;j<6;++j) {
        double s = A[j*6+j];
        for (int k=0;k<j;++k) s -= C[j*6+k]*C[j*6+k];
        if (!(s > 0.0) || !isfinite(s)) return -1;
        double d = sqrt(s);
        C[j*6+j]=d;
        for (int i=j+1;i<6;++i) {
            double v = A[i*6+j];
            for (int k=0;k<j;++k) v -= C[i*6+k]*C[j*6+k];
            C[i*6+j]=v/d;
            C[j*6+i]=0.0;
        }
    }
    g_mults += 60;
    return 0;
}
static void chol6_vfwd(const double *C, double *b) {
    /* solve C y = b in place (C lower) */
    for (int i=0;i<6;++i) {
        double s=b[i];
        for (int k=0;k<i;++k) s -= C[i*6+k]*b[k];
        b[i]=s/C[i*6+i];
    }
    g_mults += 21;
}
static void chol6_vbwd(const double *C, double *b) {
    /* solve C^T x = b in place */
    for (int i=5;i>=0;--i) {
        double s=b[i];
        for (int k=i+1;k<6;++k) s -= C[k*6+i]*b[k];
        b[i]=s/C[i*6+i];
    }
    g_mults += 21;
}
/* out[r][:] = solve C v = X[r][:]  (out = X * C^-T) */
static void chol6_mrsolve(const double *C, const double *X, double *out) {
    for (int r=0;r<6;++r)
        for (int i=0;i<6;++i) {
            double s = X[r*6+i];
            for (int k=0;k<i;++k) s -= C[i*6+k]*out[r*6+k];
            out[r*6+i]=s/C[i*6+i];
        }
    g_mults += 126;
}

/* ------------------------------------------------------------------ *
 * Block-sparse pose system.
 * ------------------------------------------------------------------ */
typedef struct {
    int nb;             /* free pose blocks */
    int nnzA, nnzL;
    /* Upper-triangular pose Hessian, CSR by row; row i cols sorted ascending
     * with the diagonal (i,i) FIRST. h_val = undamped assembled Hessian
     * (per outer iter), a_val = damped per-trial working copy. */
    int    *a_ptr, *a_col;
    double *h_val, *a_val;
    /* Lower adjacency of the pattern: row i -> entries k<i with their slots. */
    int *t_ptr, *t_k, *t_slot;
    /* Lc factor, row-major CSR (cols ascending). */
    int    *l_ptr, *l_col;
    double *l_val;
    double *c_val;      /* nb 6x6 Cholesky factors of the diagonal blocks */
    /* Column CSR of Lc: (row m ascending, l_val slot) per column. */
    int *c_ptr, *c_row, *c_off;
    double *work;       /* nb*36 row workspace */
} ba_psys;

static void psys_free(ba_psys *ps) {
    if (!ps) return;
    free(ps->a_ptr); free(ps->a_col); free(ps->h_val); free(ps->a_val);
    free(ps->t_ptr); free(ps->t_k); free(ps->t_slot);
    free(ps->l_ptr); free(ps->l_col); free(ps->l_val); free(ps->c_val);
    free(ps->c_ptr); free(ps->c_row); free(ps->c_off); free(ps->work);
    free(ps);
}

/* binary search for col b in row a (a<=b); slot index or -1 */
static int psys_slot(const ba_psys *ps, int a, int b) {
    int lo=ps->a_ptr[a], hi=ps->a_ptr[a+1]-1;
    while (lo<=hi) {
        int mid=lo+(hi-lo)/2;
        int v=ps->a_col[mid];
        if (v==b) return mid;
        if (v<b) lo=mid+1; else hi=mid-1;
    }
    return -1;
}

static int cmp_ll(const void *pa, const void *pb) {
    long long a=*(const long long*)pa, b=*(const long long*)pb;
    return (a<b) ? -1 : (a>b) ? 1 : 0;
}

/* Build pattern + symbolic factorisation from ALL edges of the problem.
 * Returns NULL on allocation failure (caller bails like the dense path). */
static ba_psys *psys_build(const ngd_ba_problem *p, const int *free_idx, int n_free)
{
    const int NM=p->n_points, NE=p->n_edges, nb=n_free;
    ba_psys *ps = (ba_psys*)calloc(1,sizeof(ba_psys));
    if (!ps) return NULL;
    ps->nb=nb;

    /* ---- edges grouped by point ---- */
    int *eoff=(int*)calloc((size_t)NM+1,sizeof(int));
    int *eedge=(int*)malloc((size_t)NE*sizeof(int));
    if (!eoff||!eedge) { free(eoff); free(eedge); psys_free(ps); return NULL; }
    for (int e=0;e<NE;++e) eoff[p->edges[e].point_idx+1]++;
    for (int j=0;j<NM;++j) eoff[j+1]+=eoff[j];
    {
        int *cur=(int*)malloc((size_t)NM*sizeof(int));
        if (!cur) { free(eoff); free(eedge); psys_free(ps); return NULL; }
        for (int j=0;j<NM;++j) cur[j]=eoff[j];
        for (int e=0;e<NE;++e) eedge[cur[p->edges[e].point_idx]++]=e;
        free(cur);
    }

    /* ---- every (a<=b) free-pose pair per point + all diagonals ---- */
    long long kcap=(long long)NE+nb+16, nk=0;
    long long *keys=(long long*)malloc((size_t)kcap*sizeof(long long));
    int *ulist=(int*)malloc((size_t)64*sizeof(int));
    if (!keys||!ulist) { free(keys); free(ulist); free(eoff); free(eedge); psys_free(ps); return NULL; }
    for (int i=0;i<nb;++i) keys[nk++]=(long long)i*nb+i;
    for (int j=0;j<NM;++j) {
        int nu=0, ucap=64;
        for (int t=eoff[j]; t<eoff[j+1]; ++t) {
            int fi=free_idx[p->edges[eedge[t]].pose_idx];
            if (fi<0) continue;
            int seen=0; for (int u=0;u<nu;++u) if (ulist[u]==fi){seen=1;break;}
            if (seen) continue;
            if (nu==ucap) {
                ucap*=2;
                int *tmp=(int*)realloc(ulist,(size_t)ucap*sizeof(int));
                if (!tmp) goto oom;
                ulist=tmp;
            }
            ulist[nu++]=fi;
        }
        for (int a=0;a<nu;++a)
            for (int b=a;b<nu;++b) {
                if (nk==kcap) {
                    kcap*=2;
                    long long *tmp=(long long*)realloc(keys,(size_t)kcap*sizeof(long long));
                    if (!tmp) goto oom;
                    keys=tmp;
                }
                keys[nk++]=(long long)ulist[a]*nb+ulist[b];
            }
    }
    free(ulist); ulist=NULL;
    free(eoff); eoff=NULL; free(eedge); eedge=NULL;
    qsort(keys,(size_t)nk,sizeof(long long),cmp_ll);

    /* ---- unique keys -> upper CSR (diagonal first per row) ---- */
    int nnzA=0;
    for (long long t=0;t<nk;++t) if (t==0||keys[t]!=keys[t-1]) keys[nnzA++]=keys[t];
    ps->a_ptr=(int*)malloc((size_t)(nb+1)*sizeof(int));
    ps->a_col=(int*)malloc((size_t)nnzA*sizeof(int));
    ps->h_val=(double*)calloc((size_t)nnzA*36,sizeof(double));
    ps->a_val=(double*)malloc((size_t)nnzA*36*sizeof(double));
    if (!ps->a_ptr||!ps->a_col||!ps->h_val||!ps->a_val) goto oom;
    {
        int row=0; ps->a_ptr[0]=0;
        for (int s=0;s<nnzA;++s) {
            int a=(int)(keys[s]/nb);
            while (row<a) ps->a_ptr[++row]=s;
            ps->a_col[s]=(int)(keys[s]%nb);
        }
        while (row<nb) ps->a_ptr[++row]=nnzA;
    }
    ps->nnzA=nnzA;

    /* ---- lower adjacency with slots (transpose of the off-diagonal) ---- */
    ps->t_ptr=(int*)calloc((size_t)nb+1,sizeof(int));
    ps->t_k=(int*)malloc((size_t)nnzA*sizeof(int));
    ps->t_slot=(int*)malloc((size_t)nnzA*sizeof(int));
    if (!ps->t_ptr||!ps->t_k||!ps->t_slot) goto oom;
    for (int s=0;s<nnzA;++s) {
        int a=(int)(keys[s]/nb);
        if (ps->a_col[s]!=a) ps->t_ptr[ps->a_col[s]+1]++;
    }
    for (int i=0;i<nb;++i) ps->t_ptr[i+1]+=ps->t_ptr[i];
    {
        int *fill=(int*)malloc((size_t)nb*sizeof(int));
        if (!fill) goto oom;
        for (int i=0;i<nb;++i) fill[i]=ps->t_ptr[i];
        for (int i=0;i<nb;++i)
            for (int s=ps->a_ptr[i]; s<ps->a_ptr[i+1]; ++s) {
                int j=ps->a_col[s];
                if (j==i) continue;
                ps->t_k[fill[j]]=i;
                ps->t_slot[fill[j]++]=s;
            }
        free(fill);
    }
    free(keys); keys=NULL;

    /* ---- symbolic up-looking pass: L row patterns.
     * Row i marks start from its lower adjacency; scanning k<i ascending,
     * each marked k merges the rows of column k of L (linked lists appended
     * by earlier rows, hence all < i) — the minimal fill-closed pattern.
     * Worst-case storage nb*(nb+1)/2 entries. ---- */
    const long long capmax=(long long)nb*(nb+1)/2 + 1;
    if (capmax > (1LL<<30)) { psys_free(ps); return NULL; }
    int *lcol=(int*)malloc((size_t)capmax*sizeof(int));
    int *lrow=(int*)malloc((size_t)capmax*sizeof(int));
    int *lnext=(int*)malloc((size_t)capmax*sizeof(int));
    int *head=(int*)malloc((size_t)nb*sizeof(int));
    int *l_ptr=(int*)malloc((size_t)(nb+1)*sizeof(int));
    unsigned char *mark=(unsigned char*)malloc((size_t)nb);
    if (!lcol||!lrow||!lnext||!head||!l_ptr||!mark) {
        free(lcol); free(lrow); free(lnext); free(head); free(l_ptr); free(mark);
        psys_free(ps); return NULL;
    }
    for (int i=0;i<nb;++i) head[i]=-1;
    int lcnt=0, appends=0;
    l_ptr[0]=0;
    for (int i=0;i<nb;++i) {
        memset(mark,0,(size_t)i);
        for (int t=ps->t_ptr[i]; t<ps->t_ptr[i+1]; ++t) mark[ps->t_k[t]]=1;
        int start=lcnt;
        for (int k=0;k<i;++k) {
            if (!mark[k]) continue;
            lcol[lcnt++]=k;
            for (int q=head[k]; q!=-1; q=lnext[q]) mark[lrow[q]]=1;
        }
        l_ptr[i+1]=lcnt;
        for (int t=start;t<lcnt;++t) {           /* append i to each col k in P_i */
            int k=lcol[t];
            lrow[appends]=i; lnext[appends]=head[k]; head[k]=appends++;
        }
    }
    ps->nnzL=lcnt;

    /* ---- L column CSR (rows ascending; offsets = l_val slots) ---- */
    ps->l_ptr=l_ptr;
    ps->l_col=(int*)malloc((size_t)lcnt*sizeof(int));
    ps->l_val=(double*)malloc((size_t)lcnt*36*sizeof(double));
    ps->c_val=(double*)malloc((size_t)nb*36*sizeof(double));
    ps->c_ptr=(int*)calloc((size_t)nb+1,sizeof(int));
    ps->c_row=(int*)malloc((size_t)lcnt*sizeof(int));
    ps->c_off=(int*)malloc((size_t)lcnt*sizeof(int));
    ps->work=(double*)malloc((size_t)nb*36*sizeof(double));
    if (!ps->l_col||!ps->l_val||!ps->c_val||!ps->c_ptr||!ps->c_row||!ps->c_off||!ps->work) {
        free(lcol); free(lrow); free(lnext); free(head); free(mark);
        psys_free(ps); return NULL;
    }
    memcpy(ps->l_col,lcol,(size_t)lcnt*sizeof(int));
    for (int t=0;t<lcnt;++t) ps->c_ptr[ps->l_col[t]+1]++;
    for (int i=0;i<nb;++i) ps->c_ptr[i+1]+=ps->c_ptr[i];
    {
        int *fill=(int*)malloc((size_t)nb*sizeof(int));
        if (!fill) { free(lcol); free(lrow); free(lnext); free(head); free(mark); psys_free(ps); return NULL; }
        for (int i=0;i<nb;++i) fill[i]=ps->c_ptr[i];
        for (int i=0;i<nb;++i)
            for (int t=ps->l_ptr[i]; t<ps->l_ptr[i+1]; ++t) {
                int k=ps->l_col[t];
                ps->c_row[fill[k]]=i;
                ps->c_off[fill[k]++]=t;
            }
        free(fill);
    }
    free(lcol); free(lrow); free(lnext); free(head); free(mark);
    return ps;

oom:
    free(keys); free(ulist); free(eoff); free(eedge);
    psys_free(ps);
    return NULL;
}

/* Numeric up-looking block Cholesky of the damped a_val. 0 ok, -1 non-PD. */
static int psys_factor(ba_psys *ps)
{
    const int nb=ps->nb;
    double *x=ps->work;
    for (int i=0;i<nb;++i) {
        /* clear touched blocks */
        memset(&x[i*36],0,36*sizeof(double));
        for (int t=ps->l_ptr[i]; t<ps->l_ptr[i+1]; ++t)
            memset(&x[ps->l_col[t]*36],0,36*sizeof(double));
        /* gather: x[i] = A(i,i) ; x[k] = A(k,i)^T (upper storage) */
        memcpy(&x[i*36],&ps->a_val[36*ps->a_ptr[i]],36*sizeof(double));
        for (int t=ps->t_ptr[i]; t<ps->t_ptr[i+1]; ++t) {
            int k=ps->t_k[t];
            const double *B=&ps->a_val[36*ps->t_slot[t]];
            double *X=&x[k*36];
            for (int r=0;r<6;++r) {
                X[r*6+r]=B[r*6+r];
                for (int c=r+1;c<6;++c) { double tmp=B[r*6+c]; X[r*6+c]=B[c*6+r]; X[c*6+r]=tmp; }
            }
        }
        /* eliminate: L(i,k) = x[k] Ck^-T ; x[m] -= L(i,k) L(m,k)^T (m<=i) */
        for (int t=ps->l_ptr[i]; t<ps->l_ptr[i+1]; ++t) {
            int k=ps->l_col[t];
            double *Lik=&ps->l_val[36*t];
            chol6_mrsolve(&ps->c_val[36*k],&x[k*36],Lik);
            for (int q=ps->c_ptr[k]; q<ps->c_ptr[k+1]; ++q) {
                int m=ps->c_row[q];
                if (m>i) break;                             /* rows ascending */
                mulT6_sub(&x[m*36],Lik,&ps->l_val[36*ps->c_off[q]]);
            }
        }
        if (chol6(&x[i*36],&ps->c_val[36*i])!=0) return -1;
    }
    return 0;
}

/* Solve Lc Lc^T dx = b in place (b has 6*nb doubles, becomes dx). */
static void psys_solve(const ba_psys *ps, double *bx)
{
    const int nb=ps->nb;
    for (int i=0;i<nb;++i) {                       /* forward: Lc y = b */
        double *yi=&bx[6*i];
        for (int t=ps->l_ptr[i]; t<ps->l_ptr[i+1]; ++t)
            mul6v_sub(yi,&ps->l_val[36*t],&bx[6*ps->l_col[t]]);
        chol6_vfwd(&ps->c_val[36*i],yi);
    }
    for (int i=nb-1;i>=0;--i) {                    /* backward: Lc^T x = y */
        double *xi=&bx[6*i];
        for (int q=ps->c_ptr[i]; q<ps->c_ptr[i+1]; ++q)
            mulT6v_sub(xi,&ps->l_val[36*ps->c_off[q]],&bx[6*ps->c_row[q]]);
        chol6_vbwd(&ps->c_val[36*i],xi);
    }
}

/* ------------------------------------------------------------------ *
 * Stats (single-threaded core).
 * ------------------------------------------------------------------ */
static ngd_ba_sparse_stats g_stats;
void ngd_ba_sparse_last_stats(ngd_ba_sparse_stats *out)
{
    if (out) *out=g_stats;
}

/* ------------------------------------------------------------------ *
 * Core optimiser — sparse pose system, LM schedule identical to ba.c.
 * ------------------------------------------------------------------ */
double ngd_ba_optimize_sparse(ngd_ba_problem *p, int max_iters)
{
    const int NP = p->n_poses, NM = p->n_points, NE = p->n_edges;
    memset(&g_stats,0,sizeof(g_stats));
    if (NP==0 || NM==0 || NE==0) return 0.0;
    for (int e=0; e<NE; ++e) p->outlier[e]=0;

    /* ---- packed free-pose index ---- */
    int n_free=0;
    int *free_idx = (int*)malloc((size_t)NP*sizeof(int));
    if (!free_idx) return 0.0;
    for (int i=0;i<NP;++i){ free_idx[i] = p->pose_fixed[i] ? -1 : n_free; n_free += p->pose_fixed[i]?0:1; }
    const int nb = n_free, dim_p = 6*n_free;
    g_stats.nb=nb; g_stats.dim_p=dim_p;

    /* ---- double working copies ---- */
    double *pq = (double*)malloc((size_t)NP*4*sizeof(double));
    double *pt = (double*)malloc((size_t)NP*3*sizeof(double));
    double *X  = (double*)malloc((size_t)NM*3*sizeof(double));
    double *tq = (double*)malloc((size_t)NP*4*sizeof(double));
    double *tt = (double*)malloc((size_t)NP*3*sizeof(double));
    double *tX = (double*)malloc((size_t)NM*3*sizeof(double));
    if (pq&&pt&&X) {
        for (int i=0;i<NP;++i){ pq[i*4+0]=p->poses[i].q.w; pq[i*4+1]=p->poses[i].q.x;
                                pq[i*4+2]=p->poses[i].q.y; pq[i*4+3]=p->poses[i].q.z;
                                pt[i*3+0]=p->poses[i].t.x; pt[i*3+1]=p->poses[i].t.y; pt[i*3+2]=p->poses[i].t.z; }
        for (int j=0;j<NM;++j){ X[j*3+0]=p->points[j].x; X[j*3+1]=p->points[j].y; X[j*3+2]=p->points[j].z; }
    }

    /* per-edge linearisation storage */
    double *eJp = (double*)malloc((size_t)NE*18*sizeof(double));
    double *eJl = (double*)malloc((size_t)NE*9*sizeof(double));
    double *er  = (double*)malloc((size_t)NE*3*sizeof(double));
    double *ew  = (double*)malloc((size_t)NE*sizeof(double));
    int    *edim= (int*)malloc((size_t)NE*sizeof(int));
    double *einvS2=(double*)malloc((size_t)NE*sizeof(double));
    int    *efi = (int*)malloc((size_t)NE*sizeof(int));

    /* point-side blocks (same as dense) */
    double *Hll = (double*)calloc((size_t)NM*9,sizeof(double));
    double *bl  = (double*)calloc((size_t)NM*3,sizeof(double));
    int *plen = (int*)calloc((size_t)NM, sizeof(int));

    /* pose system */
    ba_psys *ps = dim_p>0 ? psys_build(p, free_idx, nb) : NULL;
    double *bp     = dim_p>0 ? (double*)calloc((size_t)dim_p,sizeof(double)) : NULL;
    double *bschur = dim_p>0 ? (double*)malloc((size_t)dim_p*sizeof(double)) : NULL;
    double *dxp    = dim_p>0 ? (double*)malloc((size_t)dim_p*sizeof(double)) : NULL;
    double *dxl    = (double*)malloc((size_t)NM*3*sizeof(double));
    double *Dinv   = (double*)malloc((size_t)NM*9*sizeof(double));

    /* declared before the allocation guard so every path into cleanup sees
     * initialised values (the guard's goto skips nothing) */
    double currentChi = 0.0;
    double lambda = -1.0;
    int   *poff = NULL, *pl_pose = NULL, *pcurs = NULL;
    double *pl_blk = NULL, *BDscr = NULL;
    /* hoisted per-call Schur scaffolding (2026-11-23): superset Hpl CSR over
     * free-pose edges + point pair cache — replaces the per-iteration
     * free/malloc of poff/pl_pose/pl_blk and the per-trial psys_slot binary
     * searches. Slots of currently-excluded (dim==0) edges stay zero-filled
     * each iteration -> identical arithmetic order, bit-exact vs the
     * per-iteration compact rebuild. */
    int   *eslot = NULL, *pair_ptr = NULL, *pair_ab = NULL, *pair_s = NULL;
    int    total_slots = 0;

    if (!free_idx || !pq || !pt || !X || !tq || !tt || !tX ||
        !eJp || !eJl || !er || !ew || !edim || !einvS2 || !efi ||
        (dim_p>0 && (!ps || !bp || !bschur || !dxp)) || !dxl || !Dinv)
        goto cleanup;
    if (ps) { g_stats.nnzA=ps->nnzA; g_stats.nnzL=ps->nnzL; }

    /* ---- one-time Schur scaffolding (superset pattern; static per call) ---- */
    poff = (int*)malloc((size_t)(NM+1)*sizeof(int));
    pair_ptr = (int*)calloc((size_t)NM+1,sizeof(int));
    if (!poff || !pair_ptr) goto cleanup;
    if (!ps) { for (int j=0;j<=NM;++j) poff[j]=0; }
    if (ps) {
        memset(plen, 0, (size_t)NM*sizeof(int));
        for (int e=0;e<NE;++e)
            if (free_idx[p->edges[e].pose_idx] >= 0) plen[p->edges[e].point_idx]++;
        poff[0]=0; for (int j=0;j<NM;++j) poff[j+1]=poff[j]+plen[j];
        total_slots = poff[NM];
        pl_pose  = total_slots>0 ? (int*)malloc((size_t)total_slots*sizeof(int)) : NULL;
        pl_blk   = total_slots>0 ? (double*)malloc((size_t)total_slots*18*sizeof(double)) : NULL;
        pcurs    = (int*)malloc((size_t)NM*sizeof(int));
        eslot    = (int*)malloc((size_t)NE*sizeof(int));
        if (!pcurs || !eslot || (total_slots>0 && (!pl_pose || !pl_blk))) goto cleanup;
        for (int j=0;j<NM;++j) pcurs[j]=poff[j];
        for (int e=0;e<NE;++e){
            int fi = free_idx[p->edges[e].pose_idx];
            if (fi < 0) { eslot[e]=-1; continue; }
            int slot = pcurs[p->edges[e].point_idx]++;
            eslot[e]=slot; pl_pose[slot]=fi;
        }
        {
            int maxobs=0;
            for (int j=0;j<NM;++j){ int o=poff[j+1]-poff[j]; if(o>maxobs) maxobs=o; }
            BDscr = maxobs>0 ? (double*)malloc((size_t)maxobs*18*sizeof(double)) : NULL;
            if (maxobs>0 && !BDscr) goto cleanup;
        }
        /* pair cache: per point, (slotA,slotB,psys block) for a<b in slot order,
         * same skips as the original inner loop (duplicate pose / no block) */
        {
            int npair=0;
            for (int j=0;j<NM;++j){
                const int o0=poff[j], o1=poff[j+1];
                for (int a=o0;a<o1;++a)
                    for (int b=a+1;b<o1;++b){
                        int i1=pl_pose[a], i2=pl_pose[b];
                        if (i1==i2) continue;
                        int s = (i1<i2) ? psys_slot(ps,i1,i2) : psys_slot(ps,i2,i1);
                        if (s<0) continue;
                        npair++;
                    }
            }
            pair_ab = npair>0 ? (int*)malloc((size_t)npair*2*sizeof(int)) : NULL;
            pair_s  = npair>0 ? (int*)malloc((size_t)npair*sizeof(int)) : NULL;
            if (npair>0 && (!pair_ab || !pair_s)) goto cleanup;
            int w=0;
            for (int j=0;j<NM;++j){
                const int o0=poff[j], o1=poff[j+1];
                for (int a=o0;a<o1;++a)
                    for (int b=a+1;b<o1;++b){
                        int i1=pl_pose[a], i2=pl_pose[b];
                        if (i1==i2) continue;
                        int s = (i1<i2) ? psys_slot(ps,i1,i2) : psys_slot(ps,i2,i1);
                        if (s<0) continue;
                        pair_ab[2*w]=a; pair_ab[2*w+1]=b; pair_s[w]=s; w++;
                    }
                pair_ptr[j+1]=w;
            }
        }
    }

    /* per-iteration CSR scratch (freed in cleanup, NULL-initialised above) */

    /* initial chi2 + linearisation (stores per-edge J, r, w) */
    for (int e=0;e<NE;++e){
        const ngd_ba_edge *E=&p->edges[e];
        double chi2; int dim=edge_linearize(p,&pq[E->pose_idx*4],&pt[E->pose_idx*3],
                                            &X[E->point_idx*3],E->stereo,E->octave,E->obs,
                                            &er[e*3],&chi2,&eJp[e*18],&eJl[e*9]);
        edim[e]=dim;
        if (dim==0){ einvS2[e]=0; ew[e]=0; efi[e]=-1; continue; }
        int lvl=E->octave; if(lvl<0||lvl>=p->calib->nlevels) lvl=0;
        einvS2[e]=(double)p->calib->mvInvLevelSigma2[lvl];
        double delta = E->stereo ? sqrt(NGD_BA_DELTA_STEREO_SQ) : sqrt(NGD_BA_DELTA_MONO_SQ);
        ew[e] = huber_rho1(chi2, delta)*einvS2[e];
        efi[e] = free_idx[E->pose_idx];
        currentChi += huber_rho0(chi2, delta);
    }

    for (int it=0; it<max_iters; ++it) {
        /* ---- assemble undamped pose system (sparse blocks) + Hll/bl ---- */
        if (ps) memset(ps->h_val, 0, (size_t)ps->nnzA*36*sizeof(double));
        memset(Hll, 0, (size_t)NM*9*sizeof(double));
        if (dim_p>0) memset(bp, 0, (size_t)dim_p*sizeof(double));
        memset(bl, 0, (size_t)NM*3*sizeof(double));
        memset(plen, 0, (size_t)NM*sizeof(int));
        /* Hpl CSR scaffolding is hoisted (superset pattern, built once before
         * the loop); zero-fill so currently-excluded slots contribute exact
         * zeros in the same arithmetic positions */
        if (total_slots>0) memset(pl_blk, 0, (size_t)total_slots*18*sizeof(double));

        for (int e=0;e<NE;++e){
            if (edim[e]==0) continue;
            const ngd_ba_edge *E=&p->edges[e];
            int dim=edim[e];
            int fi=efi[e];
            int j=E->point_idx;
            const double *Jp=&eJp[e*18];
            const double *Jl=&eJl[e*9];
            const double *r =&er[e*3];
            double w=ew[e];

            for (int a=0;a<3;++a) for (int b=0;b<3;++b){
                double s=0; for (int k=0;k<dim;++k) s+=Jl[k*3+a]*Jl[k*3+b]; Hll[j*9+a*3+b]+=w*s;
            }
            for (int a=0;a<3;++a){ double s=0; for (int k=0;k<dim;++k) s+=Jl[k*3+a]*r[k]; bl[j*3+a]-=w*s; }

            if (fi>=0) {
                double *diag=&ps->h_val[36*ps->a_ptr[fi]];   /* diagonal block */
                for (int a=0;a<6;++a) for (int b=0;b<6;++b){
                    double s=0; for (int k=0;k<dim;++k) s+=Jp[k*6+a]*Jp[k*6+b];
                    diag[a*6+b]+=w*s;
                }
                for (int a=0;a<6;++a){ double s=0; for (int k=0;k<dim;++k) s+=Jp[k*6+a]*r[k]; bp[6*fi+a]-=w*s; }
                int slot=eslot[e];
                double *blk=&pl_blk[slot*18];
                for (int a=0;a<6;++a) for (int b=0;b<3;++b){
                    double s=0; for (int k=0;k<dim;++k) s+=Jp[k*6+a]*Jl[k*3+b]; blk[a*3+b]=w*s;
                }
            }
        }

        /* ---- lambda init (iter 0): max diag over pose blocks + Hll ---- */
        if (it==0) {
            double maxd=0;
            if (ps) for (int i=0;i<nb;++i){
                const double *d=&ps->h_val[36*ps->a_ptr[i]];
                for (int a=0;a<6;++a){ double v=fabs(d[a*6+a]); if(v>maxd)maxd=v; }
            }
            for (int j=0;j<NM;++j) for (int a=0;a<3;++a){ double v=fabs(Hll[j*9+a*3+a]); if(v>maxd)maxd=v; }
            lambda = NGD_BA_TAU*maxd;
            if (lambda<=0) lambda=1e-6;
        }

        /* ---- inner LM trial loop ---- */
        double ni=2.0;
        int qmax=0, accepted=0;
        do {
            g_stats.trials++;
            /* a_val = h_val + lambda*I (diag blocks) ; bschur = bp */
            if (ps) {
                memcpy(ps->a_val, ps->h_val, (size_t)ps->nnzA*36*sizeof(double));
                for (int i=0;i<nb;++i){
                    double *d=&ps->a_val[36*ps->a_ptr[i]];
                    d[0]+=lambda; d[7]+=lambda; d[14]+=lambda; d[21]+=lambda; d[28]+=lambda; d[35]+=lambda;
                }
            }
            if (dim_p>0) memcpy(bschur, bp, (size_t)dim_p*sizeof(double));

            /* Dinv_j = (Hll_jj + lambda*I)^-1 ; Schur corrections into a_val/bschur */
            for (int j=0;j<NM;++j){
                double D[9];
                for (int a=0;a<9;++a) D[a]=Hll[j*9+a];
                D[0]+=lambda; D[4]+=lambda; D[8]+=lambda;
                double Di[9];
                if (inv3_d(D, Di)!=0) {
                    for (int a=0;a<9;++a) Dinv[j*9+a]=0;
                    continue;
                }
                for (int a=0;a<9;++a) Dinv[j*9+a]=Di[a];
                double Dibl[3] = { Di[0]*bl[j*3+0]+Di[1]*bl[j*3+1]+Di[2]*bl[j*3+2],
                                   Di[3]*bl[j*3+0]+Di[4]*bl[j*3+1]+Di[5]*bl[j*3+2],
                                   Di[6]*bl[j*3+0]+Di[7]*bl[j*3+1]+Di[8]*bl[j*3+2] };
                g_stats.schur_mults += 18;
                g_stats.dense_lu_mults_equiv += 18;
                const int o0=poff[j], o1=poff[j+1];
                const int nobs=o1-o0;
                g_stats.dense_lu_mults_equiv += 36*(long long)nobs + 162*(long long)nobs*nobs;
                /* BD_a = B_a * Di  (reused for diagonal + pair updates) */
                for (int a=o0;a<o1;++a) mul63(&pl_blk[a*18], Di, &BDscr[(a-o0)*18]);
                for (int a=o0;a<o1;++a){
                    int i1=pl_pose[a];
                    const double *Ba=&pl_blk[a*18];
                    const double *BD=&BDscr[(a-o0)*18];
                    for (int r=0;r<6;++r){
                        double s=0; for(int k=0;k<3;++k) s+=Ba[r*3+k]*Dibl[k];
                        bschur[6*i1+r]-=s;
                    }
                    double S[36];
                    mul636T(BD,Ba,S);
                    double *blk=&ps->a_val[36*ps->a_ptr[i1]];
                    for (int r=0;r<36;++r) blk[r]-=S[r];
                    g_stats.schur_mults += 18+54+108;
                }
                /* unordered pairs a<b: upper block (min,max) -= B_a D B_b^T
                 * (cached (slotA,slotB,block) — no per-trial slot searches) */
                for (int q=pair_ptr[j]; q<pair_ptr[j+1]; ++q){
                    const double *BDa=&BDscr[(pair_ab[2*q]-o0)*18];
                    double S[36];
                    mul636T(BDa, &pl_blk[pair_ab[2*q+1]*18], S);
                    double *blk=&ps->a_val[36*pair_s[q]];
                    for (int r=0;r<36;++r) blk[r]-=S[r];
                    g_stats.schur_mults += 54+108;
                }
            }

            /* factor + solve the damped pose system */
            int solved = 1;
            if (dim_p>0) {
                g_mults=0;
                if (psys_factor(ps)!=0) {
                    solved=0;
                    g_stats.factor_fails++;
                } else {
                    psys_solve(ps, bschur);
                    memcpy(dxp, bschur, (size_t)dim_p*sizeof(double));
                }
                g_stats.factor_mults += g_mults;
                g_stats.dense_lu_mults_equiv += (long long)dim_p*dim_p*dim_p/3 + (long long)dim_p*dim_p;
            }

            if (!solved) { qmax++; lambda*=ni; ni*=2.0; continue; }

            /* back-sub: dxl_j = Dinv_j * (bl_j - Hpl_{.,j}^T * dxp) */
            for (int j=0;j<NM;++j){
                double rhs[3] = {bl[j*3+0], bl[j*3+1], bl[j*3+2]};
                for (int a=poff[j]; a<poff[j+1]; ++a){
                    int i1=pl_pose[a];
                    const double *Ba=&pl_blk[a*18];
                    for (int k=0;k<3;++k){
                        double s=0; for(int r=0;r<6;++r) s+=Ba[r*3+k]*dxp[6*i1+r];
                        rhs[k]-=s;
                    }
                }
                const double *Di=&Dinv[j*9];
                dxl[j*3+0]=Di[0]*rhs[0]+Di[1]*rhs[1]+Di[2]*rhs[2];
                dxl[j*3+1]=Di[3]*rhs[0]+Di[4]*rhs[1]+Di[5]*rhs[2];
                dxl[j*3+2]=Di[6]*rhs[0]+Di[7]*rhs[1]+Di[8]*rhs[2];
            }

            /* trial estimate: poses exp(dxp)*T (free only), points += dxl */
            memcpy(tq, pq, (size_t)NP*4*sizeof(double));
            memcpy(tt, pt, (size_t)NP*3*sizeof(double));
            memcpy(tX, X,  (size_t)NM*3*sizeof(double));
            for (int i=0;i<NP;++i){
                int fi=free_idx[i];
                if (fi<0) continue;
                double dq[4], dt[3];
                se3_exp_d(&dxp[6*fi], dq, dt);
                se3_mul_d(dq, dt, &pq[i*4], &pt[i*3], &tq[i*4], &tt[i*3]);
            }
            for (int j=0;j<NM;++j){ tX[j*3+0]=X[j*3+0]+dxl[j*3+0];
                                     tX[j*3+1]=X[j*3+1]+dxl[j*3+1];
                                     tX[j*3+2]=X[j*3+2]+dxl[j*3+2]; }

            /* tempChi at trial (residual-only; chi2 identical to ba.c) */
            double tempChi=0.0;
            for (int e=0;e<NE;++e){
                const ngd_ba_edge *E=&p->edges[e];
                double chi2;
                edge_residual(p,&tq[E->pose_idx*4],&tt[E->pose_idx*3],
                              &tX[E->point_idx*3],E->stereo,E->octave,E->obs,&chi2);
                double delta = E->stereo ? sqrt(NGD_BA_DELTA_STEREO_SQ) : sqrt(NGD_BA_DELTA_MONO_SQ);
                tempChi += huber_rho0(chi2, delta);
                if (!isfinite(tempChi)) { tempChi=1e30; break; }
            }

            /* Nielsen gain ratio over [dxp|dxl] / [bp|bl] */
            double scale=1e-3;
            for (int i=0;i<dim_p;++i) scale += dxp[i]*(lambda*dxp[i] + bp[i]);
            for (int j=0;j<NM;++j) for(int k=0;k<3;++k) scale += dxl[j*3+k]*(lambda*dxl[j*3+k] + bl[j*3+k]);
            double rho = (currentChi - tempChi)/scale;

            if (rho>0 && isfinite(tempChi)) {
                double alpha = 1.0 - pow(2.0*rho-1.0, 3.0);
                if (alpha < 1.0/3.0) alpha=1.0/3.0;
                if (alpha > 2.0/3.0) alpha=2.0/3.0;
                lambda *= alpha;
                ni=2.0;
                currentChi=tempChi;
                memcpy(pq, tq, (size_t)NP*4*sizeof(double));
                memcpy(pt, tt, (size_t)NP*3*sizeof(double));
                memcpy(X,  tX,  (size_t)NM*3*sizeof(double));
                /* re-linearise at the accepted estimate */
                for (int e=0;e<NE;++e){
                    const ngd_ba_edge *E=&p->edges[e];
                    double chi2;
                    int dim=edge_linearize(p,&pq[E->pose_idx*4],&pt[E->pose_idx*3],
                                           &X[E->point_idx*3],E->stereo,E->octave,E->obs,
                                           &er[e*3],&chi2,&eJp[e*18],&eJl[e*9]);
                    edim[e]=dim;
                    if (dim==0){ ew[e]=0; efi[e]=-1; continue; }
                    double delta = E->stereo ? sqrt(NGD_BA_DELTA_STEREO_SQ) : sqrt(NGD_BA_DELTA_MONO_SQ);
                    ew[e]=huber_rho1(chi2,delta)*einvS2[e];
                    efi[e]=free_idx[E->pose_idx];
                }
                accepted=1;
                break;
            } else {
                lambda*=ni; ni*=2.0;
            }
            qmax++;
        } while (!accepted && qmax < NGD_BA_MAX_TRIALS);

        if (!accepted) break;   /* inner exhausted: stop */
    }

    /* ---- write back (float) ---- */
    for (int i=0;i<NP;++i){
        if (p->pose_fixed[i]) continue;
        p->poses[i].q.w=(float)pq[i*4+0]; p->poses[i].q.x=(float)pq[i*4+1];
        p->poses[i].q.y=(float)pq[i*4+2]; p->poses[i].q.z=(float)pq[i*4+3];
        p->poses[i].t.x=(float)pt[i*3+0]; p->poses[i].t.y=(float)pt[i*3+1]; p->poses[i].t.z=(float)pt[i*3+2];
    }
    for (int j=0;j<NM;++j){ p->points[j].x=(float)X[j*3+0]; p->points[j].y=(float)X[j*3+1]; p->points[j].z=(float)X[j*3+2]; }

    /* ---- outlier classification (chi2 > threshold or depth<=0) ---- */
    for (int e=0;e<NE;++e){
        const ngd_ba_edge *E=&p->edges[e];
        double chi2;
        int dim=edge_residual(p,&pq[E->pose_idx*4],&pt[E->pose_idx*3],
                              &X[E->point_idx*3],E->stereo,E->octave,E->obs,&chi2);
        double thr = E->stereo ? NGD_BA_DELTA_STEREO_SQ : NGD_BA_DELTA_MONO_SQ;
        p->outlier[e] = (dim==0 || chi2>thr) ? 1 : 0;
    }

cleanup:
    free(free_idx);
    free(pq); free(pt); free(X); free(tq); free(tt); free(tX);
    free(eJp); free(eJl); free(er); free(ew); free(edim); free(einvS2); free(efi);
    free(Hll); free(bl); free(plen);
    psys_free(ps);
    free(bp); free(bschur); free(dxp); free(dxl); free(Dinv);
    free(poff); free(pl_pose); free(pl_blk); free(pcurs); free(BDscr);
    free(eslot); free(pair_ptr); free(pair_ab); free(pair_s);

    return currentChi;
}
