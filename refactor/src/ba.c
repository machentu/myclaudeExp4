/* ngd/ba.c — LocalBundleAdjustment (pure C, Schur complement).
 * Faithful to ORB_SLAM3::Optimizer::LocalBundleAdjustment (src/Optimizer.cc:1116-1500)
 * and g2o BlockSolver_6_3 + OptimizationAlgorithmLevenberg. See include/ngd/ba.h.
 *
 * Internal math is double (g2o uses double; pnp.c precedent). The float SE3
 * helpers in se3.c are not reused — double mirrors of exp/map/deriv are kept
 * local so the whole BA pipeline runs in double.
 */
#include "ngd/ba.h"
#include "ngd/keyframe.h"
#include "ngd/mappoint.h"
#include "ngd/map.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define NGD_BA_DELTA_MONO_SQ   5.991
#define NGD_BA_DELTA_STEREO_SQ 7.815
#define NGD_BA_TAU             1e-5   /* g2o _tau (computeLambdaInit) */
#define NGD_BA_MAX_TRIALS      10     /* maxTrialsAfterFailure */

/* ------------------------------------------------------------------ *
 * Double-precision SE3 / camera helpers (mirror se3.c / pinhole.c).
 * ------------------------------------------------------------------ */

static void quat_to_R_d(const double q[4], double R[9]) {
    /* Hamilton w-first, row-major (matches ngd_quat_to_matrix). */
    double w=q[0], x=q[1], y=q[2], z=q[3];
    R[0]=1-2*(y*y+z*z); R[1]=2*(x*y-z*w);   R[2]=2*(x*z+y*w);
    R[3]=2*(x*y+z*w);   R[4]=1-2*(x*x+z*z); R[5]=2*(y*z-x*w);
    R[6]=2*(x*z-y*w);   R[7]=2*(y*z+x*w);   R[8]=1-2*(x*x+y*y);
}

static void se3_exp_d(const double xi[6], double q[4], double t[3]) {
    /* g2o SE3Quat::exp. xi = [omega(3) | upsilon(3)]. */
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
    /* quaternion from R (Shepperd, w-first) */
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
    /* normalise */
    double n=sqrt(q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3]);
    if (n>0) for (int i=0;i<4;++i) q[i]/=n;
    t[0]=V[0]*ux+V[1]*uy+V[2]*uz;
    t[1]=V[3]*ux+V[4]*uy+V[5]*uz;
    t[2]=V[6]*ux+V[7]*uy+V[8]*uz;
}

static void se3_mul_d(const double qa[4], const double ta[3],
                      const double qb[4], const double tb[3],
                      double qo[4], double to[3]) {
    /* g2o: t = tA + R_A*tB ; q = qA*qB (normalised). */
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
    /* [ -skew(Xc) | I_3 ], 3x6 row-major (ngd_se3_deriv, OptimizableTypes). */
    out[0]=0;     out[1]=Xc[2];  out[2]=-Xc[1];  out[3]=1; out[4]=0; out[5]=0;
    out[6]=-Xc[2];out[7]=0;      out[8]=Xc[0];   out[9]=0; out[10]=1;out[11]=0;
    out[12]=Xc[1];out[13]=-Xc[0];out[14]=0;      out[15]=0;out[16]=0;out[17]=1;
}

/* Closed-form 3x3 inverse (double). Returns 0 on success, -1 if singular. */
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

/* Dense LU solve with partial pivoting: A(n*n, row-major)*x = b. A is clobbered. */
static int solve_dense_d(double *A, double *b, double *x, int n) {
    if (n == 0) return 0;
    for (int i=0;i<n;++i) x[i]=b[i];
    for (int col=0; col<n; ++col) {
        int piv=col; double mx=fabs(A[col*n+col]);
        for (int r=col+1;r<n;++r){ double v=fabs(A[r*n+col]); if(v>mx){mx=v;piv=r;} }
        if (mx < 1e-18) return -1;
        if (piv!=col) {
            for (int c=0;c<n;++c){ double t=A[col*n+c]; A[col*n+c]=A[piv*n+c]; A[piv*n+c]=t; }
            double t=x[col]; x[col]=x[piv]; x[piv]=t;
        }
        double d=A[col*n+col];
        for (int r=col+1;r<n;++r){
            double f=A[r*n+col]/d;
            if (f==0.0) continue;
            for (int c=col;c<n;++c) A[r*n+c]-=f*A[col*n+c];
            x[r]-=f*x[col];
        }
    }
    for (int r=n-1;r>=0;--r){
        double s=x[r];
        for (int c=r+1;c<n;++c) s-=A[r*n+c]*x[c];
        x[r]=s/A[r*n+r];
    }
    return 0;
}

/* ------------------------------------------------------------------ *
 * Per-edge linearisation at the current estimate.
 *   Xc = R*Xw + t  (R from pose quaternion)
 *   proj / r / chi2 / Pjac(dimx3) / J_point(dimx3) = -Pjac*R / J_pose(dimx6) = -Pjac*SE3deriv
 * Returns dim (2 mono / 3 stereo), or 0 if depth<=0 (degenerate).
 * ------------------------------------------------------------------ */
static int edge_linearize(const ngd_ba_problem *p,
                          const double *poseq, const double *poset,  /* this pose's q[4], t[3] */
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

    /* Pjac (dim x 3 row-major) */
    double Pjac[9];
    Pjac[0]=fx*invz;  Pjac[1]=0;           Pjac[2]=-fx*Xc[0]*invz2;
    Pjac[3]=0;        Pjac[4]=fy*invz;     Pjac[5]=-fy*Xc[1]*invz2;
    if (stereo) { Pjac[6]=fx*invz; Pjac[7]=0; Pjac[8]=-fx*Xc[0]*invz2 + p->bf*invz2; }

    /* residual r = obs - proj */
    double o0=obs[0], o1=obs[1], o2=obs[2];
    double u = fx*Xc[0]*invz + cx;
    double v = fy*Xc[1]*invz + cy;
    r[0]=o0-u; r[1]=o1-v;
    if (stereo) r[2]=o2-(u - p->bf*invz);

    /* chi2 = invSigma2 * r·r */
    int lvl = octave; if (lvl<0||lvl>=p->calib->nlevels) lvl=0;
    double invS2 = (double)p->calib->mvInvLevelSigma2[lvl];
    double rr = r[0]*r[0]+r[1]*r[1]; if (stereo) rr += r[2]*r[2];
    *chi2 = invS2*rr;

    /* J_point = -Pjac * R  (dim x 3) */
    for (int rr2=0; rr2<dim; ++rr2)
        for (int cc=0; cc<3; ++cc) {
            double s=0; for (int k=0;k<3;++k) s += Pjac[rr2*3+k]*R[k*3+cc];
            Jl[rr2*3+cc] = -s;
        }
    /* J_pose = -Pjac * SE3deriv  (dim x 6) */
    double sd[18]; se3_deriv_d(Xc, sd);
    for (int rr2=0; rr2<dim; ++rr2)
        for (int cc=0; cc<6; ++cc) {
            double s=0; for (int k=0;k<3;++k) s += Pjac[rr2*3+k]*sd[k*6+cc];
            Jp[rr2*6+cc] = -s;
        }
    return dim;
}

static inline double huber_rho0(double chi2, double delta) {
    if (chi2 <= delta*delta) return chi2;
    return 2.0*delta*sqrt(chi2) - delta*delta;
}
static inline double huber_rho1(double chi2, double delta) {
    if (chi2 <= delta*delta) return 1.0;
    return delta/sqrt(chi2);
}

/* ------------------------------------------------------------------ *
 * Core optimizer.
 * ------------------------------------------------------------------ */
double ngd_ba_optimize(ngd_ba_problem *p, int max_iters)
{
    const int NP = p->n_poses, NM = p->n_points, NE = p->n_edges;
    if (NP==0 || NM==0 || NE==0) return 0.0;
    for (int e=0; e<NE; ++e) p->outlier[e]=0;

    /* ---- packed free-pose index ---- */
    int n_free=0;
    int *free_idx = (int*)malloc((size_t)NP*sizeof(int));
    for (int i=0;i<NP;++i){ free_idx[i] = p->pose_fixed[i] ? -1 : n_free; n_free += p->pose_fixed[i]?0:1; }
    const int dim_p = 6*n_free;

    /* ---- double working copies ---- */
    double *pq = (double*)malloc((size_t)NP*4*sizeof(double));
    double *pt = (double*)malloc((size_t)NP*3*sizeof(double));
    double *X  = (double*)malloc((size_t)NM*3*sizeof(double));
    for (int i=0;i<NP;++i){ pq[i*4+0]=p->poses[i].q.w; pq[i*4+1]=p->poses[i].q.x;
                            pq[i*4+2]=p->poses[i].q.y; pq[i*4+3]=p->poses[i].q.z;
                            pt[i*3+0]=p->poses[i].t.x; pt[i*3+1]=p->poses[i].t.y; pt[i*3+2]=p->poses[i].t.z; }
    for (int j=0;j<NM;++j){ X[j*3+0]=p->points[j].x; X[j*3+1]=p->points[j].y; X[j*3+2]=p->points[j].z; }

    /* trial buffers (poses+points) */
    double *tq = (double*)malloc((size_t)NP*4*sizeof(double));
    double *tt = (double*)malloc((size_t)NP*3*sizeof(double));
    double *tX = (double*)malloc((size_t)NM*3*sizeof(double));

    /* per-edge linearisation storage */
    double *eJp = (double*)malloc((size_t)NE*18*sizeof(double));
    double *eJl = (double*)malloc((size_t)NE*9*sizeof(double));
    double *er  = (double*)malloc((size_t)NE*3*sizeof(double));
    double *ew  = (double*)malloc((size_t)NE*sizeof(double));     /* rho1*invS2 */
    int    *edim= (int*)malloc((size_t)NE*sizeof(int));            /* 0 = degenerate */
    double *einvS2=(double*)malloc((size_t)NE*sizeof(double));
    int    *efi = (int*)malloc((size_t)NE*sizeof(int));            /* free pose idx or -1 */

    /* Hessian blocks */
    double *Hpp = dim_p>0 ? (double*)calloc((size_t)dim_p*dim_p,sizeof(double)) : NULL;
    double *Hll = (double*)calloc((size_t)NM*9,sizeof(double));
    double *bp  = dim_p>0 ? (double*)calloc((size_t)dim_p,sizeof(double)) : NULL;
    double *bl  = (double*)calloc((size_t)NM*3,sizeof(double));
    int *plen = (int*)calloc((size_t)NM, sizeof(int));   /* Hpl slot count per point */

    /* Hpl CSR (per point, free-pose edges only). plen is recounted each outer
     * iteration from the current linearisation (degenerate edges drop out). */

    double *Hschur = dim_p>0 ? (double*)malloc((size_t)dim_p*dim_p*sizeof(double)) : NULL;
    double *bschur = dim_p>0 ? (double*)malloc((size_t)dim_p*sizeof(double)) : NULL;
    double *dxp = dim_p>0 ? (double*)malloc((size_t)dim_p*sizeof(double)) : NULL;
    double *dxl = (double*)malloc((size_t)NM*3*sizeof(double));
    double *Dinv= (double*)malloc((size_t)NM*9*sizeof(double));

    double currentChi = 0.0;
    double lambda = -1.0;

    /* initial chi2 + linearisation (stores per-edge J, r, w) */
    {
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
            double rho1 = huber_rho1(chi2, delta);
            ew[e] = rho1*einvS2[e];
            efi[e] = free_idx[E->pose_idx];
            currentChi += huber_rho0(chi2, delta);
        }
    }

    for (int it=0; it<max_iters; ++it) {
        /* ---- assemble undamped Hpp/Hll/Hpl/bp/bl from current linearisation ---- */
        if (dim_p>0) memset(Hpp, 0, (size_t)dim_p*dim_p*sizeof(double));
        memset(Hll, 0, (size_t)NM*9*sizeof(double));
        if (dim_p>0) memset(bp, 0, (size_t)dim_p*sizeof(double));
        memset(bl, 0, (size_t)NM*3*sizeof(double));
        memset(plen, 0, (size_t)NM*sizeof(int));
        for (int e=0;e<NE;++e) if (edim[e]>0 && efi[e]>=0) plen[p->edges[e].point_idx]++;

        /* CSR offsets */
        int *poff = (int*)malloc((size_t)(NM+1)*sizeof(int));
        poff[0]=0; for (int j=0;j<NM;++j) poff[j+1]=poff[j]+plen[j];
        int total_slots = poff[NM];
        int *pl_pose  = total_slots>0 ? (int*)malloc((size_t)total_slots*sizeof(int)) : NULL;
        double *pl_blk= total_slots>0 ? (double*)malloc((size_t)total_slots*18*sizeof(double)) : NULL;
        int *pcurs = (int*)malloc((size_t)NM*sizeof(int));
        for (int j=0;j<NM;++j) pcurs[j]=poff[j];

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

            /* Hll_jj += w * Jl^T Jl (3x3) ; bl_j -= w * Jl^T r (3) */
            for (int a=0;a<3;++a) for (int b=0;b<3;++b){
                double s=0; for (int k=0;k<dim;++k) s+=Jl[k*3+a]*Jl[k*3+b]; Hll[j*9+a*3+b]+=w*s;
            }
            for (int a=0;a<3;++a){ double s=0; for (int k=0;k<dim;++k) s+=Jl[k*3+a]*r[k]; bl[j*3+a]-=w*s; }

            if (fi>=0) {
                /* Hpp_ii += w * Jp^T Jp (6x6) ; bp_i -= w * Jp^T r (6) */
                for (int a=0;a<6;++a) for (int b=0;b<6;++b){
                    double s=0; for (int k=0;k<dim;++k) s+=Jp[k*6+a]*Jp[k*6+b];
                    Hpp[(6*fi+a)*dim_p+(6*fi+b)]+=w*s;
                }
                for (int a=0;a<6;++a){ double s=0; for (int k=0;k<dim;++k) s+=Jp[k*6+a]*r[k]; bp[6*fi+a]-=w*s; }
                /* Hpl block = w * Jp^T Jl (6x3) */
                int slot=pcurs[j]++;
                pl_pose[slot]=fi;
                double *blk=&pl_blk[slot*18];
                for (int a=0;a<6;++a) for (int b=0;b<3;++b){
                    double s=0; for (int k=0;k<dim;++k) s+=Jp[k*6+a]*Jl[k*3+b]; blk[a*3+b]=w*s;
                }
            }
        }

        /* ---- lambda init (iter 0): max diag over Hpp + Hll ---- */
        if (it==0) {
            double maxd=0;
            for (int i=0;i<dim_p;++i){ double v=fabs(Hpp[i*dim_p+i]); if(v>maxd)maxd=v; }
            for (int j=0;j<NM;++j) for (int a=0;a<3;++a){ double v=fabs(Hll[j*9+a*3+a]); if(v>maxd)maxd=v; }
            lambda = NGD_BA_TAU*maxd;
            if (lambda<=0) lambda=1e-6;
        }

        /* ---- inner LM trial loop ---- */
        double ni=2.0;
        int qmax=0, accepted=0;
        do {
            /* Hschur = Hpp + lambda*I (free pose diag) */
            if (dim_p>0) {
                memcpy(Hschur, Hpp, (size_t)dim_p*dim_p*sizeof(double));
                for (int i=0;i<dim_p;++i) Hschur[i*dim_p+i]+=lambda;
                memcpy(bschur, bp, (size_t)dim_p*sizeof(double));
            }
            /* Dinv_j = (Hll_jj + lambda*I)^-1 ; Schur corrections */
            for (int j=0;j<NM;++j){
                double D[9];
                for (int a=0;a<9;++a) D[a]=Hll[j*9+a];
                D[0]+=lambda; D[4]+=lambda; D[8]+=lambda;
                double Di[9];
                if (inv3_d(D, Di)!=0) { /* singular: zero Dinv (skip point) */
                    for (int a=0;a<9;++a) Dinv[j*9+a]=0; continue;
                }
                for (int a=0;a<9;++a) Dinv[j*9+a]=Di[a];
                /* bschur -= Hpl_{.,j} * Di * bl_j ; Hschur -= Hpl_{i1,j}*Di*Hpl_{i2,j}^T */
                double Dibl[3] = { Di[0]*bl[j*3+0]+Di[1]*bl[j*3+1]+Di[2]*bl[j*3+2],
                                   Di[3]*bl[j*3+0]+Di[4]*bl[j*3+1]+Di[5]*bl[j*3+2],
                                   Di[6]*bl[j*3+0]+Di[7]*bl[j*3+1]+Di[8]*bl[j*3+2] };
                for (int a=poff[j]; a<poff[j+1]; ++a){
                    int i1=pl_pose[a];
                    const double *Ba=&pl_blk[a*18]; /* 6x3 */
                    /* bschur_{i1} -= Ba * Dibl  (6) */
                    for (int r=0;r<6;++r){
                        double s=0; for(int k=0;k<3;++k) s+=Ba[r*3+k]*Dibl[k];
                        bschur[6*i1+r]-=s;
                    }
                    for (int b=poff[j]; b<poff[j+1]; ++b){
                        int i2=pl_pose[b];
                        const double *Bb=&pl_blk[b*18]; /* 6x3 */
                        /* block = Ba * Di * Bb^T  (6x6) */
                        double BD[18]; /* Ba*Di : 6x3 */
                        for (int r=0;r<6;++r) for(int k=0;k<3;++k){
                            double s=0; for(int c=0;c<3;++c) s+=Ba[r*3+c]*Di[c*3+k]; BD[r*3+k]=s;
                        }
                        for (int r=0;r<6;++r) for (int c=0;c<6;++c){
                            double s=0; for(int k=0;k<3;++k) s+=BD[r*3+k]*Bb[c*3+k]; /* Bb^T: (c,k)=Bb[c*3+k] */
                            Hschur[(6*i1+r)*dim_p+(6*i2+c)] -= s;
                        }
                    }
                }
            }

            /* solve Hschur * dxp = bschur (LU clobbers Hschur copy) */
            int solved = 1;
            if (dim_p>0) {
                double *Hcopy = (double*)malloc((size_t)dim_p*dim_p*sizeof(double));
                memcpy(Hcopy, Hschur, (size_t)dim_p*dim_p*sizeof(double));
                solved = (solve_dense_d(Hcopy, bschur, dxp, dim_p)==0);
                free(Hcopy);
            }

            if (!solved) { qmax++; lambda*=ni; ni*=2.0; continue; }

            /* back-sub: dxl_j = Dinv_j * (bl_j - Hpl_{.,j}^T * dxp) */
            for (int j=0;j<NM;++j){
                double rhs[3] = {bl[j*3+0], bl[j*3+1], bl[j*3+2]};
                for (int a=poff[j]; a<poff[j+1]; ++a){
                    int i1=pl_pose[a];
                    const double *Ba=&pl_blk[a*18]; /* 6x3 ; Hpl^T row = Ba^T */
                    for (int k=0;k<3;++k){
                        double s=0; for(int r=0;r<6;++r) s+=Ba[r*3+k]*dxp[6*i1+r]; /* Ba^T * dxp */
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

            /* tempChi at trial (re-evaluate residuals, Huber rho0) */
            double tempChi=0.0;
            for (int e=0;e<NE;++e){
                const ngd_ba_edge *E=&p->edges[e];
                double r[3], chi2;
                int dim=edge_linearize(p,&tq[E->pose_idx*4],&tt[E->pose_idx*3],
                                       &tX[E->point_idx*3],E->stereo,E->octave,E->obs,r,&chi2,
                                       &eJp[e*18],&eJl[e*9]); (void)dim;
                double delta = E->stereo ? sqrt(NGD_BA_DELTA_STEREO_SQ) : sqrt(NGD_BA_DELTA_MONO_SQ);
                tempChi += huber_rho0(chi2, delta);
                if (!isfinite(tempChi)) { tempChi=1e30; break; }
            }

            /* Nielsen gain ratio: scale = dx^T (lambda*dx + b) over [dxp|dxl] / [bp|bl] */
            double scale=1e-3;
            for (int i=0;i<dim_p;++i) scale += dxp[i]*(lambda*dxp[i] + bp[i]);
            for (int j=0;j<NM;++j) for(int k=0;k<3;++k) scale += dxl[j*3+k]*(lambda*dxl[j*3+k] + bl[j*3+k]);
            double rho = (currentChi - tempChi)/scale;

            if (rho>0 && isfinite(tempChi)) {
                /* accept */
                double alpha = 1.0 - pow(2.0*rho-1.0, 3.0);
                if (alpha < 1.0/3.0) alpha=1.0/3.0;
                if (alpha > 2.0/3.0) alpha=2.0/3.0;
                lambda *= alpha;
                ni=2.0;
                currentChi=tempChi;
                memcpy(pq, tq, (size_t)NP*4*sizeof(double));
                memcpy(pt, tt, (size_t)NP*3*sizeof(double));
                memcpy(X,  tX,  (size_t)NM*3*sizeof(double));
                /* re-linearise at the accepted estimate (refresh J/r/w for next outer) */
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

        free(poff); free(pcurs);
        if (pl_pose) free(pl_pose);
        if (pl_blk) free(pl_blk);

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
        double r[3], chi2;
        int dim=edge_linearize(p,&pq[E->pose_idx*4],&pt[E->pose_idx*3],
                               &X[E->point_idx*3],E->stereo,E->octave,E->obs,r,&chi2,
                               &eJp[e*18],&eJl[e*9]);
        double thr = E->stereo ? NGD_BA_DELTA_STEREO_SQ : NGD_BA_DELTA_MONO_SQ;
        p->outlier[e] = (dim==0 || chi2>thr) ? 1 : 0;
    }

    /* cleanup */
    free(free_idx); free(pq); free(pt); free(X); free(tq); free(tt); free(tX);
    free(eJp); free(eJl); free(er); free(ew); free(edim); free(einvS2); free(efi);
    free(Hpp); free(Hll); free(bp); free(bl); free(plen);
    free(Hschur); free(bschur); free(dxp); free(dxl); free(Dinv);

    return currentChi;
}

/* ------------------------------------------------------------------ *
 * Gathering wrapper — Optimizer::LocalBundleAdjustment (Optimizer.cc:1116-1500).
 * ------------------------------------------------------------------ */

/* Minimal EraseObservation: remove the obs record for `kf`, decrement nObs
 * (stereo counts 2), and clear the KF's mvpMapPoints slot (EraseMapPointMatch). */
static void mp_erase_observation(ngd_mappoint *mp, ngd_keyframe *kf)
{
    int found = -1;
    for (int i=0;i<mp->nObsRec;++i) if (mp->obs[i].kf==kf){ found=i; break; }
    if (found<0) return;
    int rightIdx = mp->obs[found].rightIdx;
    int leftIdx  = mp->obs[found].leftIdx;
    /* swap-pop */
    mp->obs[found]=mp->obs[mp->nObsRec-1];
    mp->nObsRec--;
    mp->nObs -= (rightIdx>=0)?2:1;
    if (mp->nObs<0) mp->nObs=0;
    if (leftIdx>=0 && leftIdx<kf->N && kf->mvpMapPoints[leftIdx]==mp) kf->mvpMapPoints[leftIdx]=NULL;
}

int ngd_local_ba(ngd_keyframe *pKF, ngd_map *pmap, const int *stop_flag)
{
    (void)stop_flag;
    const uint64_t stamp = pKF->mnId;

    /* ---- lLocalKeyFrames = pKF + covisible neighbours ---- */
    int capL = 1 + pKF->nConn;
    ngd_keyframe **localKFs = (ngd_keyframe**)malloc((size_t)capL*sizeof(ngd_keyframe*));
    int nL=0;
    pKF->mnBALocalForKF = stamp; pKF->mnBAFixedForKF = 0;
    localKFs[nL++]=pKF;
    for (int i=0;i<pKF->nConn;++i){
        ngd_keyframe *k=pKF->connKFs[i];
        if (!k) continue;
        k->mnBALocalForKF=stamp; k->mnBAFixedForKF=0;
        localKFs[nL++]=k;
    }

    /* ---- lLocalMapPoints = MPs observed in local KFs (dedup) ---- */
    ngd_mappoint **localMPs = NULL; int nM=0, capM=0;
    for (int i=0;i<nL;++i){
        ngd_keyframe *k=localKFs[i];
        for (int idx=0; idx<k->N; ++idx){
            ngd_mappoint *mp=k->mvpMapPoints[idx];
            if (!mp || mp->mbBad) continue;
            if (mp->mnBALocalForKF==stamp) continue;
            mp->mnBALocalForKF=stamp;
            if (nM==capM){ capM=capM?capM*2:64; localMPs=(ngd_mappoint**)realloc(localMPs,(size_t)capM*sizeof(ngd_mappoint*)); }
            localMPs[nM++]=mp;
        }
    }

    /* ---- lFixedCameras = KFs observing local MPs but not local ---- */
    ngd_keyframe **fixedKFs=NULL; int nF=0, capF=0;
    int num_fixed = 0;
    /* init KF among local counts as fixed (Optimizer.cc:1141-1144, 1220) */
    for (int i=0;i<nL;++i) if (localKFs[i]->mnId==pmap->mnInitKFid) { num_fixed=1; break; }

    for (int j=0;j<nM;++j){
        ngd_mappoint *mp=localMPs[j];
        for (int o=0;o<mp->nObsRec;++o){
            ngd_keyframe *k=mp->obs[o].kf;
            if (!k) continue;
            if (k->mnBALocalForKF==stamp) continue;       /* it's a local KF */
            if (k->mnBAFixedForKF==stamp) continue;       /* already collected */
            k->mnBAFixedForKF=stamp;
            if (nF==capF){ capF=capF?capF*2:32; fixedKFs=(ngd_keyframe**)realloc(fixedKFs,(size_t)capF*sizeof(ngd_keyframe*)); }
            fixedKFs[nF++]=k;
        }
    }
    num_fixed += nF;
    if (num_fixed==0){ free(localKFs); free(localMPs); free(fixedKFs); return 1; }

    /* ---- build flat problem ---- */
    const int n_poses = nL + nF;
    ngd_se3 *poses = (ngd_se3*)malloc((size_t)n_poses*sizeof(ngd_se3));
    int *pose_fixed = (int*)calloc((size_t)n_poses, sizeof(int));
    /* map KF -> pose index */
    /* local KFs: 0..nL-1 (fixed iff id==initKFid); fixed KFs: nL..nL+nF-1 (all fixed) */
    for (int i=0;i<nL;++i){ poses[i]=localKFs[i]->pose; pose_fixed[i] = (localKFs[i]->mnId==pmap->mnInitKFid)?1:0; }
    for (int i=0;i<nF;++i){ poses[nL+i]=fixedKFs[i]->pose; pose_fixed[nL+i]=1; }

    const int n_points = nM;
    ngd_vec3 *points = (ngd_vec3*)malloc((size_t)n_points*sizeof(ngd_vec3));
    for (int j=0;j<nM;++j){ points[j].x=localMPs[j]->worldPos[0]; points[j].y=localMPs[j]->worldPos[1]; points[j].z=localMPs[j]->worldPos[2]; }

    /* edges: for each local MP, each observation in local∪fixed KF */
    int capE=0; for (int j=0;j<nM;++j) capE += localMPs[j]->nObsRec;
    ngd_ba_edge *edges = capE>0 ? (ngd_ba_edge*)malloc((size_t)capE*sizeof(ngd_ba_edge)) : NULL;
    /* parallel arrays for post-opt outlier erasure (KF + leftIdx per edge) */
    ngd_keyframe **edge_kf = capE>0 ? (ngd_keyframe**)malloc((size_t)capE*sizeof(ngd_keyframe*)) : NULL;
    int *edge_left = capE>0 ? (int*)malloc((size_t)capE*sizeof(int)) : NULL;
    int *edge_mp = capE>0 ? (int*)malloc((size_t)capE*sizeof(int)) : NULL;   /* local MP index for write-back */
    int nE=0;
    for (int j=0;j<nM;++j){
        ngd_mappoint *mp=localMPs[j];
        for (int o=0;o<mp->nObsRec;++o){
            ngd_keyframe *k=mp->obs[o].kf;
            if (!k) continue;
            if (k->mnBALocalForKF!=stamp && k->mnBAFixedForKF!=stamp) continue;
            int leftIdx=mp->obs[o].leftIdx;
            if (leftIdx<0 || leftIdx>=k->N) continue;
            /* pose index */
            int pi=-1;
            if (k->mnBALocalForKF==stamp){ for (int i=0;i<nL;++i) if(localKFs[i]==k){pi=i;break;} }
            else { for (int i=0;i<nF;++i) if(fixedKFs[i]==k){pi=nL+i;break;} }
            if (pi<0) continue;
            float ur = (k->uRight && leftIdx<k->N) ? k->uRight[leftIdx] : -1.0f;
            int stereo = (ur >= 0.0f) ? 1 : 0;
            ngd_ba_edge *E=&edges[nE];
            E->pose_idx=pi; E->point_idx=j; E->stereo=stereo;
            E->octave = k->keys[leftIdx].octave;
            E->obs[0]=k->keys[leftIdx].x; E->obs[1]=k->keys[leftIdx].y;
            if (stereo) E->obs[2]=ur; else E->obs[2]=0.0f;
            edge_kf[nE]=k; edge_left[nE]=leftIdx; edge_mp[nE]=j;
            nE++;
        }
    }

    ngd_ba_problem prob;
    memset(&prob, 0, sizeof(prob));
    prob.poses=poses; prob.pose_fixed=pose_fixed; prob.n_poses=n_poses;
    prob.points=points; prob.n_points=n_points;
    prob.edges=edges; prob.n_edges=nE;
    prob.cam=&pKF->cam.cam; prob.bf=pKF->cam.mbf; prob.calib=&pKF->cam;
    int *outlier = nE>0 ? (int*)calloc((size_t)nE,sizeof(int)) : NULL;
    prob.outlier=outlier;

    if (nE>=4) ngd_ba_optimize(&prob, 10);

    /* ---- write back optimised poses (non-fixed local KFs) + MP positions ---- */
    for (int i=0;i<nL;++i){
        if (pose_fixed[i]) continue;
        ngd_keyframe_set_pose(localKFs[i], poses[i]);
    }
    for (int j=0;j<nM;++j){
        float pos[3]={points[j].x,points[j].y,points[j].z};
        ngd_mappoint_set_world_pos(localMPs[j], pos);
    }

    /* ---- cull outlier observations (EraseMapPointMatch + EraseObservation).
     * edge_kf/edge_left/edge_mp were captured per edge at build time, so erasure
     * does not need to re-derive the (KF,leftIdx) mapping (which would be
     * invalidated by the swap-pop inside mp_erase_observation). ---- */
    for (int e=0;e<nE;++e){
        if (!outlier[e]) continue;
        ngd_mappoint *mp=localMPs[edge_mp[e]];
        if (mp->mbBad) continue;
        /* only erase if the KF slot still points at this MP (idempotent guard) */
        ngd_keyframe *k=edge_kf[e];
        if (edge_left[e]>=0 && edge_left[e]<k->N && k->mvpMapPoints[edge_left[e]]==mp)
            mp_erase_observation(mp, k);
    }

    free(localKFs); free(localMPs); free(fixedKFs);
    free(poses); free(pose_fixed); free(points); free(edges);
    free(edge_kf); free(edge_left); free(edge_mp); free(outlier);
    return 0;
}

/* ------------------------------------------------------------------ *
 * GlobalBundleAdjustment (Optimizer::GlobalBundleAdjustment,
 * called by LoopClosing::RunGlobalBundleAdjustment).
 * All non-bad KFs (init KF fixed) + all non-bad MPs + all observations.
 * ------------------------------------------------------------------ */
int ngd_global_ba(ngd_map *pmap, int max_iters, const ngd_cam_ctx *cam)
{
    /* gather all non-bad KFs */
    int nK = 0;
    for (int i = 0; i < pmap->nKFs; ++i)
        if (pmap->kfs[i] && !ngd_keyframe_is_bad(pmap->kfs[i])) nK++;
    if (nK < 2) return 1;
    ngd_keyframe **allKFs = (ngd_keyframe**)malloc((size_t)nK*sizeof(ngd_keyframe*));
    nK = 0;
    for (int i = 0; i < pmap->nKFs; ++i)
        if (pmap->kfs[i] && !ngd_keyframe_is_bad(pmap->kfs[i])) allKFs[nK++] = pmap->kfs[i];

    const ngd_cam_ctx *CC = cam ? cam : &allKFs[0]->cam;

    /* poses: init KF fixed */
    ngd_se3 *poses = (ngd_se3*)malloc((size_t)nK*sizeof(ngd_se3));
    int *pose_fixed = (int*)calloc((size_t)nK, sizeof(int));
    for (int i = 0; i < nK; ++i) {
        poses[i] = allKFs[i]->pose;
        pose_fixed[i] = (allKFs[i]->mnId == pmap->mnInitKFid) ? 1 : 0;
    }

    /* gather all non-bad MPs (dedup by pointer: map.mps already dedup'd) */
    int nM = 0;
    for (int i = 0; i < pmap->nMPs; ++i)
        if (pmap->mps[i] && !pmap->mps[i]->mbBad) nM++;
    if (nM == 0) {
        free(allKFs); free(poses); free(pose_fixed);
        return 1;
    }
    ngd_mappoint **allMPs = (ngd_mappoint**)malloc((size_t)nM*sizeof(ngd_mappoint*));
    nM = 0;
    for (int i = 0; i < pmap->nMPs; ++i)
        if (pmap->mps[i] && !pmap->mps[i]->mbBad) allMPs[nM++] = pmap->mps[i];

    ngd_vec3 *points = (ngd_vec3*)malloc((size_t)nM*sizeof(ngd_vec3));
    for (int j = 0; j < nM; ++j) {
        points[j].x = allMPs[j]->worldPos[0];
        points[j].y = allMPs[j]->worldPos[1];
        points[j].z = allMPs[j]->worldPos[2];
    }

    /* edges: every observation of each MP whose KF is in allKFs */
    int capE = 0;
    for (int j = 0; j < nM; ++j) capE += allMPs[j]->nObsRec;
    ngd_ba_edge *edges = capE > 0 ? (ngd_ba_edge*)malloc((size_t)capE*sizeof(ngd_ba_edge)) : NULL;
    int *edge_kf = capE > 0 ? (int*)malloc((size_t)capE*sizeof(int)) : NULL;   /* KF index */
    int *edge_left = capE > 0 ? (int*)malloc((size_t)capE*sizeof(int)) : NULL;
    int *edge_mp = capE > 0 ? (int*)malloc((size_t)capE*sizeof(int)) : NULL;
    int nE = 0;
    for (int j = 0; j < nM; ++j) {
        ngd_mappoint *mp = allMPs[j];
        for (int o = 0; o < mp->nObsRec; ++o) {
            ngd_keyframe *k = mp->obs[o].kf;
            if (!k) continue;
            int pi = -1; for (int i = 0; i < nK; ++i) if (allKFs[i] == k) { pi = i; break; }
            if (pi < 0) continue;
            int li = mp->obs[o].leftIdx;
            if (li < 0 || li >= k->N) continue;
            float ur = (k->uRight && li < k->N) ? k->uRight[li] : -1.0f;
            int stereo = (ur >= 0.0f) ? 1 : 0;
            ngd_ba_edge *E = &edges[nE];
            E->pose_idx = pi; E->point_idx = j; E->stereo = stereo;
            E->octave = k->keys[li].octave;
            E->obs[0] = k->keys[li].x; E->obs[1] = k->keys[li].y;
            E->obs[2] = stereo ? ur : 0.0f;
            edge_kf[nE] = pi; edge_left[nE] = li; edge_mp[nE] = j;
            nE++;
        }
    }

    ngd_ba_problem prob;
    memset(&prob, 0, sizeof(prob));
    prob.poses = poses; prob.pose_fixed = pose_fixed; prob.n_poses = nK;
    prob.points = points; prob.n_points = nM;
    prob.edges = edges; prob.n_edges = nE;
    prob.cam = &CC->cam; prob.bf = CC->mbf; prob.calib = CC;
    int *outlier = nE > 0 ? (int*)calloc((size_t)nE, sizeof(int)) : NULL;
    prob.outlier = outlier;

    if (nE >= 4) ngd_ba_optimize(&prob, max_iters);

    /* write back non-fixed poses + all MP positions */
    for (int i = 0; i < nK; ++i)
        if (!pose_fixed[i]) ngd_keyframe_set_pose(allKFs[i], poses[i]);
    for (int j = 0; j < nM; ++j) {
        float pos[3] = { points[j].x, points[j].y, points[j].z };
        ngd_mappoint_set_world_pos(allMPs[j], pos);
    }
    /* cull outlier observations */
    for (int e = 0; e < nE; ++e) {
        if (!outlier[e]) continue;
        ngd_mappoint *mp = allMPs[edge_mp[e]];
        if (mp->mbBad) continue;
        ngd_keyframe *k = allKFs[edge_kf[e]];
        if (edge_left[e] >= 0 && edge_left[e] < k->N && k->mvpMapPoints[edge_left[e]] == mp)
            mp_erase_observation(mp, k);
    }

    free(allKFs); free(allMPs);
    free(poses); free(pose_fixed); free(points); free(edges);
    free(edge_kf); free(edge_left); free(edge_mp); free(outlier);
    return 0;
}
