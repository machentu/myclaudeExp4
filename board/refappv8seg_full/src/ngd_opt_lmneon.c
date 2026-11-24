/* ngd_opt_lmneon.c — [1b] Local BA 提速 Stage A (宏 NGD_BA_FAST, 2026-10-12)
 *
 * 自包含复制 refactor/src/ba.c 的 ngd_ba_optimize（double 数学全部保持），
 * 依据 v39 NGD_LM_PROBE 全量数据实施结构优化（晚段 nL≈60 时 schur 占 opt
 * 80-83%，iters/trials 恒 10/10 无早停，CSR 每外层迭代 malloc/free ~2MB）：
 *
 *   A1 CSR/Hcopy/BDscratch 一次分配复用（原版每外层迭代 free+malloc）
 *   A2 Schur 内层 BD=Ba*Di 提升到 b 循环外（数值不变，省 2/3 乘法）
 *   A3 Schur 配对只算无序对 a≤b、只写 Hschur 上三角，求解前一次镜像补下三角
 *      （上三角与原版逐位一致；下三角=镜像 vs 原版独立计算 → ulp 级差异；
 *        Hpp 组装同样只写上三角）
 *   A4 每位姿 R 缓存（quat_to_R 纯函数 → 逐位一致）
 *   A5 tempChi / outlier 扫描不计算 Jacobian（原版算了但从未读回 → 逐位一致）
 *   A6 收敛早停：接受步相对改进 ≤ NGD_BA_STOP_EPS(默认 1e-6) 即停。
 *      行为变化（迭代数减少），由 ATE 门禁验证；[bafast] 行输出
 *      iters/last_gain/opt 耗时，供阈值调参。
 *
 * 钩子: refactor/src/ba.c ngd_local_ba 内 NGD_BA_FAST 分发（GBA 仍走原版）。
 * 不编译本文件（宏关）= 与基线完全一致。
 */
#if defined(NGD_BA_FAST) && NGD_BA_FAST

#include "ngd/ba.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define NGD_BA_DELTA_MONO_SQ   5.991
#define NGD_BA_DELTA_STEREO_SQ 7.815
#define NGD_BA_TAU             1e-5
#define NGD_BA_MAX_TRIALS      10
/* A6: 接受步相对改进低于此值即停（可用 -DNGD_BA_STOP_EPS= 覆盖） */
#ifndef NGD_BA_STOP_EPS
#define NGD_BA_STOP_EPS 1e-6
#endif
/* A3 开关: 1=无序对+上三角+镜像 (ulp 级偏差); 0=有序对全写 (基线逐位,
 * 仅保留 BD 提升/CSR 复用等位级等价项) — 诊断对拍用 */
#ifndef NGD_BA_SYM
#define NGD_BA_SYM 1
#endif
/* A7: 1=分块 LU (B=NB_LU)。v39 配对数据定案: 原版稠密 LU 在 dim_p≈500 时
 * 矩阵 2MB 超 L2, 逐列消除反复流 DDR (~670MB/s) → ~1s/trial, 占晚段
 * LBA wall 40%。分块把尾矩阵更新改块状流, 流量降 ~NB 倍。
 * 分块只改变同一元素上更新的累加次序 → ulp 级偏差 (ATE 门禁)。 */
#ifndef NGD_BA_TILED_SOLVE
#define NGD_BA_TILED_SOLVE 1
#endif
#define NGD_BA_NB_LU 32

/* ------------------------------------------------------------------ *
 * Double-precision SE3 / camera helpers — 与 ba.c 逐行相同（static 副本）。
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

#if NGD_BA_TILED_SOLVE
/* A7 分块 LU (部分主元, 行主序, 块 NB)。与 solve_dense_d 同数学 (同主元规则),
 * 仅把尾矩阵的逐列消除改为按块延迟更新 — 同一元素的更新次序改变 → ulp 级
 * 数值差异, 换取 ~NB 倍内存流量下降 (大矩阵不再逐列重流 DDR)。 */
static int solve_dense_tiled(double *A, double *b, double *x, int n)
{
    const int NB = NGD_BA_NB_LU;
    if (n == 0) return 0;
    for (int i=0;i<n;++i) x[i]=b[i];

    for (int k0=0; k0<n; k0+=NB) {
        const int k1 = (k0+NB < n) ? k0+NB : n;

        /* ---- 面板分解 [k0,k1): 逐列选主元/换行(整行 k0..n)/消面板内列,
         *      消去因子 f 存在 A[r][col] (L 部分), 尾列暂不更新 ---- */
        for (int col=k0; col<k1; ++col) {
            int piv=col; double mx=fabs(A[col*n+col]);
            for (int r=col+1;r<n;++r){ double v=fabs(A[r*n+col]); if(v>mx){mx=v;piv=r;} }
            if (mx < 1e-18) return -1;
            if (piv!=col) {
                for (int c=k0;c<n;++c){ double t=A[col*n+c]; A[col*n+c]=A[piv*n+c]; A[piv*n+c]=t; }
                double t=x[col]; x[col]=x[piv]; x[piv]=t;
            }
            double d=A[col*n+col];
            for (int r=col+1;r<n;++r){
                double f=A[r*n+col]/d;
                A[r*n+col]=f;                    /* L 因子 */
                if (f==0.0) continue;
                for (int c=col+1;c<k1;++c) A[r*n+c]-=f*A[col*n+c];
                x[r]-=f*x[col];
            }
        }

        /* ---- 尾矩阵块更新: A[r][k1:] -= Σ_{k<kmax} L[r][k]*A[k][k1:] ----
         * r 升序; r<k1 的面板行读到的是已更新完的上方 U 行尾段。
         * 每行 r 顺序流 ≤B 个源行, 尾部行段连续 → 局部性好 */
        for (int r=k0+1;r<n;++r){
            double *Ar=&A[r*n];
            const int kmax = (r < k1) ? r : k1;
            for (int k=k0;k<kmax;++k){
                double f=Ar[k];
                if (f==0.0) continue;
                const double *Ak=&A[k*n];
                for (int c=k1;c<n;++c) Ar[c]-=f*Ak[c];
            }
        }
    }

    /* 回代 (行主序, 同原版) */
    for (int r=n-1;r>=0;--r){
        double s=x[r];
        for (int c=r+1;c<n;++c) s-=A[r*n+c]*x[c];
        x[r]=s/A[r*n+r];
    }
    return 0;
}
#else
#define solve_dense_tiled solve_dense_d
#endif

/* ------------------------------------------------------------------ *
 * Per-edge linearisation — 与 ba.c edge_linearize 相同，唯 R 由调用方
 * 传入（A4 位姿级缓存，quat_to_R 纯函数 → 数值逐位一致）。
 * ------------------------------------------------------------------ */
static int edge_linearize_r(const ngd_ba_problem *p,
                            const double R[9], const double poset[3],
                            const double Xw[3],
                            int stereo, int octave,
                            const float obs[3],
                            double r[3], double *chi2, double Jp[18], double Jl[9])
{
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

/* A5: 只算残差与 chi2（tempChi / outlier 扫描专用；这两处原版计算的
 * Jacobian 从未被读回，跳过 → 结果逐位一致）。 */
static int edge_chi(const ngd_ba_problem *p,
                    const double R[9], const double poset[3],
                    const double Xw[3],
                    int stereo, int octave,
                    const float obs[3],
                    double *chi2)
{
    double Xc[3] = { R[0]*Xw[0]+R[1]*Xw[1]+R[2]*Xw[2]+poset[0],
                     R[3]*Xw[0]+R[4]*Xw[1]+R[5]*Xw[2]+poset[1],
                     R[6]*Xw[0]+R[7]*Xw[1]+R[8]*Xw[2]+poset[2] };
    if (Xc[2] <= 1e-9) return 0;
    double fx=p->cam->fx, fy=p->cam->fy, cx=p->cam->cx, cy=p->cam->cy;
    double invz=1.0/Xc[2];
    int dim = stereo ? 3 : 2;

    double o0=obs[0], o1=obs[1], o2=obs[2];
    double u = fx*Xc[0]*invz + cx;
    double v = fy*Xc[1]*invz + cy;
    double r0=o0-u, r1=o1-v;
    double r2=0; if (stereo) r2=o2-(u - p->bf*invz);

    int lvl = octave; if (lvl<0||lvl>=p->calib->nlevels) lvl=0;
    double invS2 = (double)p->calib->mvInvLevelSigma2[lvl];
    double rr = r0*r0+r1*r1; if (stereo) rr += r2*r2;
    *chi2 = invS2*rr;
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

static double baf_now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e3 + ts.tv_nsec / 1e6;
}

/* ------------------------------------------------------------------ *
 * Core optimizer (fast path). 与 ba.c ngd_ba_optimize 同构，含 A1-A6。
 * ------------------------------------------------------------------ */
double ngd_ba_optimize_fast(ngd_ba_problem *p, int max_iters)
{
    static int call_count = 0;
    ++call_count;
    const double _t0 = baf_now();

    const int NP = p->n_poses, NM = p->n_points, NE = p->n_edges;
    if (NP==0 || NM==0 || NE==0) return 0.0;
    for (int e=0; e<NE; ++e) p->outlier[e]=0;

    /* ---- packed free-pose index ---- */
    int n_free=0;
    int *free_idx = (int*)malloc((size_t)NP*sizeof(int));
    if (!free_idx) {printf("allocate idx failed\n");return 0.0;}
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

    double *Hschur = dim_p>0 ? (double*)malloc((size_t)dim_p*dim_p*sizeof(double)) : NULL;
    double *bschur = dim_p>0 ? (double*)malloc((size_t)dim_p*sizeof(double)) : NULL;
    double *dxp = dim_p>0 ? (double*)malloc((size_t)dim_p*sizeof(double)) : NULL;
    double *dxl = (double*)malloc((size_t)NM*3*sizeof(double));
    double *Dinv= (double*)malloc((size_t)NM*9*sizeof(double));

    /* A4: per-pose rotation caches (current estimate + trial) */
    double *Rc = (double*)malloc((size_t)NP*9*sizeof(double));
    double *Rt = (double*)malloc((size_t)NP*9*sizeof(double));

    /* A1: CSR scratch allocated ONCE with capacity NE slots (plen can only
     * count <= NE edges); reused across outer iterations. */
    int   *poff    = (int*)malloc((size_t)(NM+1)*sizeof(int));
    int   *pl_pose = NE>0 ? (int*)malloc((size_t)NE*sizeof(int)) : NULL;
    double *pl_blk = NE>0 ? (double*)malloc((size_t)NE*18*sizeof(double)) : NULL;
    int   *pcurs   = (int*)malloc((size_t)NM*sizeof(int));

    /* A2: BD=Ba*Di scratch per point slot (max n_free slots per point) */
    double *bdscratch = n_free>0 ? (double*)malloc((size_t)n_free*18*sizeof(double)) : NULL;
    /* A1: LU work buffer (solve clobbers it), allocated once */
    double *Hcopy = dim_p>0 ? (double*)malloc((size_t)dim_p*dim_p*sizeof(double)) : NULL;

    /* NULL-pointer guard: bail out cleanly on allocation failure. */
    if (!free_idx || !pq || !pt || !X || !tq || !tt || !tX ||
        !eJp || !eJl || !er || !ew || !edim || !einvS2 || !efi ||
        (dim_p>0 && (!Hpp || !bp)) || !Hll || !bl || !plen ||
        (dim_p>0 && (!Hschur || !bschur || !dxp || !Hcopy)) || !dxl || !Dinv ||
        !Rc || !Rt || !poff || !pcurs || (NE>0 && (!pl_pose || !pl_blk)) ||
        (n_free>0 && !bdscratch))
        goto cleanup;

    double currentChi = 0.0;
    double lambda = -1.0;
    int it_done = 0;        /* actual outer iterations executed (A6 telemetry) */
    double last_gain = 0.0; /* relative improvement of last accepted step */

    /* initial chi2 + linearisation (stores per-edge J, r, w) */
    for (int i=0;i<NP;++i) quat_to_R_d(&pq[i*4], &Rc[i*9]);
    for (int e=0;e<NE;++e){
        const ngd_ba_edge *E=&p->edges[e];
        double chi2; int dim=edge_linearize_r(p,&Rc[E->pose_idx*9],&pt[E->pose_idx*3],
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

    for (int it=0; it<max_iters; ++it) {
        it_done = it+1;
        /* ---- assemble undamped Hpp/Hll/Hpl/bp/bl from current linearisation ----
         * A3/A4: Hpp 只写上三角（r<=c），下三角由求解前镜像补齐。 */
        if (dim_p>0) memset(Hpp, 0, (size_t)dim_p*dim_p*sizeof(double));
        memset(Hll, 0, (size_t)NM*9*sizeof(double));
        if (dim_p>0) memset(bp, 0, (size_t)dim_p*sizeof(double));
        memset(bl, 0, (size_t)NM*3*sizeof(double));
        memset(plen, 0, (size_t)NM*sizeof(int));
        for (int e=0;e<NE;++e) if (edim[e]>0 && efi[e]>=0) plen[p->edges[e].point_idx]++;

        /* CSR offsets (A1: buffers reused, only prefix recomputed) */
        poff[0]=0; for (int j=0;j<NM;++j) poff[j+1]=poff[j]+plen[j];
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

            for (int a=0;a<3;++a) for (int b=0;b<3;++b){
                double s=0; for (int k=0;k<dim;++k) s+=Jl[k*3+a]*Jl[k*3+b]; Hll[j*9+a*3+b]+=w*s;
            }
            for (int a=0;a<3;++a){ double s=0; for (int k=0;k<dim;++k) s+=Jl[k*3+a]*r[k]; bl[j*3+a]-=w*s; }

            if (fi>=0) {
                for (int a=0;a<6;++a) for (int b=0;b<6;++b){
#if NGD_BA_SYM
                    if (b<a) continue;   /* A3: 只写上三角, 下三角由镜像补 */
#endif
                    double s=0; for (int k=0;k<dim;++k) s+=Jp[k*6+a]*Jp[k*6+b];
                    Hpp[(6*fi+a)*dim_p+(6*fi+b)]+=w*s;
                }
                for (int a=0;a<6;++a){ double s=0; for (int k=0;k<dim;++k) s+=Jp[k*6+a]*r[k]; bp[6*fi+a]-=w*s; }
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
            /* Dinv_j = (Hll_jj + lambda*I)^-1 ; Schur corrections
             * A2: BD=Ba*Di 每槽一次（原版每 (a,b) 对重复计算）
             * A3: 无序对 a<=b、只写 Hschur 上三角 */
            for (int j=0;j<NM;++j){
                double D[9];
                for (int a=0;a<9;++a) D[a]=Hll[j*9+a];
                D[0]+=lambda; D[4]+=lambda; D[8]+=lambda;
                double Di[9];
                if (inv3_d(D, Di)!=0) { /* singular: zero Dinv (skip point) */
                    for (int a=0;a<9;++a)
                        Dinv[j*9+a]=0;

                    continue;
                }
                for (int a=0;a<9;++a) Dinv[j*9+a]=Di[a];
                /* bschur -= Hpl_{.,j} * Di * bl_j */
                double Dibl[3] = { Di[0]*bl[j*3+0]+Di[1]*bl[j*3+1]+Di[2]*bl[j*3+2],
                                   Di[3]*bl[j*3+0]+Di[4]*bl[j*3+1]+Di[5]*bl[j*3+2],
                                   Di[6]*bl[j*3+0]+Di[7]*bl[j*3+1]+Di[8]*bl[j*3+2] };
                const int s0=poff[j], s1=poff[j+1];
                for (int a=s0;a<s1;++a){
                    int i1=pl_pose[a];
                    const double *Ba=&pl_blk[a*18]; /* 6x3 */
                    for (int r=0;r<6;++r){
                        double s=0; for(int k=0;k<3;++k) s+=Ba[r*3+k]*Dibl[k];
                        bschur[6*i1+r]-=s;
                    }
                    /* A2: BD_a = Ba*Di (6x3), 每槽一次 */
                    double *BD=&bdscratch[(a-s0)*18];
                    for (int r=0;r<6;++r) for(int k=0;k<3;++k){
                        double s=0; for(int c=0;c<3;++c) s+=Ba[r*3+c]*Di[c*3+k]; BD[r*3+k]=s;
                    }
                }
                /* Hschur_{i1,i2} -= B_{i1} * Di * B_{i2}^T */
#if NGD_BA_SYM
                /* A3: 无序对 a<=b、只写上三角 */
                for (int a=s0;a<s1;++a){
                    int i1=pl_pose[a];
                    const double *Ba=&pl_blk[a*18];
                    const double *BDa=&bdscratch[(a-s0)*18];
                    for (int b=a;b<s1;++b){
                        int i2=pl_pose[b];
                        const double *Bb=&pl_blk[b*18];
                        if (i1==i2) {
                            /* 对角位姿块: 只写 r<=c（21 项），其余由镜像补齐 */
                            double *Hd=&Hschur[(6*i1)*dim_p+6*i1];
                            for (int r=0;r<6;++r) for (int c=r;c<6;++c){
                                double s=0; for(int k=0;k<3;++k) s+=BDa[r*3+k]*Bb[c*3+k];
                                Hd[r*dim_p+c]-=s;
                            }
                        } else if (i1<i2) {
                            double *Hu=&Hschur[(6*i1)*dim_p+6*i2];
                            for (int r=0;r<6;++r) for (int c=0;c<6;++c){
                                double s=0; for(int k=0;k<3;++k) s+=BDa[r*3+k]*Bb[c*3+k];
                                Hu[r*dim_p+c]-=s;
                            }
                        } else {
                            /* i1>i2: 上三角块在 (i2,i1)，用 BD_b*Ba^T */
                            const double *BDb=&bdscratch[(b-s0)*18];
                            double *Hu=&Hschur[(6*i2)*dim_p+6*i1];
                            for (int r=0;r<6;++r) for (int c=0;c<6;++c){
                                double s=0; for(int k=0;k<3;++k) s+=BDb[r*3+k]*Ba[c*3+k];
                                Hu[r*dim_p+c]-=s;
                            }
                        }
                    }
                }
#else
                /* 基线形态: 有序对全写（与 ba.c 逐位一致），仅保留 A2 BD 提升 */
                for (int a=s0;a<s1;++a){
                    int i1=pl_pose[a];
                    const double *Ba=&pl_blk[a*18];
                    const double *BDa=&bdscratch[(a-s0)*18];
                    for (int b=s0;b<s1;++b){
                        int i2=pl_pose[b];
                        const double *Bb=&pl_blk[b*18];
                        for (int r=0;r<6;++r) for (int c=0;c<6;++c){
                            double s=0; for(int k=0;k<3;++k) s+=BDa[r*3+k]*Bb[c*3+k];
                            Hschur[(6*i1+r)*dim_p+(6*i2+c)] -= s;
                        }
                    }
                }
#endif
            }
#if NGD_BA_SYM
            /* A3: 一次镜像补下三角（Hschur 数学对称；消除 ulp 级不对称） */
            if (dim_p>0) {
                for (int r=1;r<dim_p;++r) {
                    double *Hrow=&Hschur[r*dim_p];
                    for (int c=0;c<r;++c) Hrow[c]=Hschur[c*dim_p+r];
                }
            }
#else
            (void)0;
#endif

            /* solve Hschur * dxp = bschur (LU clobbers Hcopy; A1 hoisted) */
            int solved = 1;
            if (dim_p>0) {
                memcpy(Hcopy, Hschur, (size_t)dim_p*dim_p*sizeof(double));
                solved = (solve_dense_tiled(Hcopy, bschur, dxp, dim_p)==0);
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

            /* tempChi at trial (A5: chi-only, no Jacobians; A4: trial R cache) */
            for (int i=0;i<NP;++i) quat_to_R_d(&tq[i*4], &Rt[i*9]);
            double tempChi=0.0;
            for (int e=0;e<NE;++e){
                const ngd_ba_edge *E=&p->edges[e];
                double chi2;
                edge_chi(p,&Rt[E->pose_idx*9],&tt[E->pose_idx*3],
                         &tX[E->point_idx*3],E->stereo,E->octave,E->obs,&chi2);
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
                double chi_before = currentChi;
                currentChi=tempChi;
                memcpy(pq, tq, (size_t)NP*4*sizeof(double));
                memcpy(pt, tt, (size_t)NP*3*sizeof(double));
                memcpy(X,  tX,  (size_t)NM*3*sizeof(double));
                last_gain = chi_before>0 ? (chi_before-tempChi)/chi_before : 1.0;
                /* re-linearise at the accepted estimate (refresh J/r/w for next outer) */
                for (int i=0;i<NP;++i) quat_to_R_d(&pq[i*4], &Rc[i*9]);
                for (int e=0;e<NE;++e){
                    const ngd_ba_edge *E=&p->edges[e];
                    double chi2;
                    int dim=edge_linearize_r(p,&Rc[E->pose_idx*9],&pt[E->pose_idx*3],
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
        /* A6: 收敛早停 — 接受步相对改进过小则不再继续外层迭代 */
        if (last_gain <= NGD_BA_STOP_EPS) break;
    }

    /* ---- write back (float) ---- */
    for (int i=0;i<NP;++i){
        if (p->pose_fixed[i]) continue;
        p->poses[i].q.w=(float)pq[i*4+0]; p->poses[i].q.x=(float)pq[i*4+1];
        p->poses[i].q.y=(float)pq[i*4+2]; p->poses[i].q.z=(float)pq[i*4+3];
        p->poses[i].t.x=(float)pt[i*3+0]; p->poses[i].t.y=(float)pt[i*3+1]; p->poses[i].t.z=(float)pt[i*3+2];
    }
    for (int j=0;j<NM;++j){ p->points[j].x=(float)X[j*3+0]; p->points[j].y=(float)X[j*3+1]; p->points[j].z=(float)X[j*3+2]; }

    /* ---- outlier classification (chi2 > threshold or depth<=0); A5 chi-only ---- */
    for (int e=0;e<NE;++e){
        const ngd_ba_edge *E=&p->edges[e];
        double chi2;
        int dim=edge_chi(p,&Rc[E->pose_idx*9],&pt[E->pose_idx*3],
                         &X[E->point_idx*3],E->stereo,E->octave,E->obs,&chi2);
        double thr = E->stereo ? NGD_BA_DELTA_STEREO_SQ : NGD_BA_DELTA_MONO_SQ;
        p->outlier[e] = (dim==0 || chi2>thr) ? 1 : 0;
    }

    fprintf(stderr, "[bafast] call=%d NP=%d NE=%d dim_p=%d iters=%d last_gain=%.2e chi=%.1f opt=%.0fms\n",
            call_count, NP, NE, dim_p, it_done, last_gain, currentChi, baf_now()-_t0);

cleanup:
    free(free_idx); free(pq); free(pt); free(X); free(tq); free(tt); free(tX);
    free(eJp); free(eJl); free(er); free(ew); free(edim); free(einvS2); free(efi);
    free(Hpp); free(Hll); free(bp); free(bl); free(plen);
    free(Hschur); free(bschur); free(dxp); free(dxl); free(Dinv);
    free(Rc); free(Rt);
    free(poff); free(pl_pose); free(pl_blk); free(pcurs); free(bdscratch); free(Hcopy);

    return currentChi;
}

#endif /* NGD_BA_FAST */
