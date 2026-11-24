/* ngd_opt_lmf32.c — [1c] Local BA 混合精度版 (宏 NGD_BA_F32, 2026-11-24)
 *
 * 思路 (prompt_ba2.txt 采纳入库的唯一新增量): A7 (ARMv7) 的 f64 走标量 VFP
 * (~18 周期/fma, 不流水), f32 有 NEON 通道 → 线性代数降 f32 是最大剩余杠杆。
 * 本文件 = ngd_opt_lmneon.c (BA_FAST A1-A7, f64) 的混合精度复制:
 *
 *   F1  线性代数全 f32: H 组装 (Hpp/Hll/Hpl/bp/bl)、Schur 消元 (Dinv/BD/
 *       Hschur/bschur)、分块 LU (A7 形态)、dxp/dxl 回代 — 缓冲全部 f32,
 *       峰值内存低于 f64 fast 版 (J 缓冲 392→268 B/edge)。
 *   F2  NEON 内核 (__ARM_NEON__, 板端; x86 走标量 f32 等价路径):
 *       - 6×6 块散射 Hu -= BD·B^T (Schur 配对热点, 4+2 车道)
 *       - LU 尾矩阵行更新 Ar[c] -= f·Ak[c] (4 车道)
 *       - 回代行点积 (4+2 车道 + hsum)
 *   F3  f64 收口 (决策链与状态精度不动): 线性化 J/r/chi2 全 f64 评估后
 *       落 f32 缓冲; Huber 权 f64 算完落 f32; lambda/Nielsen/scale f64;
 *       se3_exp/状态更新/写回 f64; tempChi/outlier 分类 f64。
 *   F4  pl_blk 双布局 (6×3 Ba + 3×6 Bt 同值双写, 组装期一次): 散射用 Bt
 *       (行 6 连续), 回代用 Bt (与 dxp 段点积), BD/bschur 用 Ba。
 *
 * 继承 A1-A7 全部结构优化 (CSR 复用/A2 BD 提升/A3 上三角+镜像/A4 R 缓存/
 * A5 chi-only/A6 早停/A7 分块 LU — LU 在此为 f32 副本)。
 *
 * 数值预期 (与 f64 fast 相比): H/dxp/dxl 相对误差 ~1e-6..1e-5 级 → LM 路径
 * 不同 → KF 决策链可漂移 (ulp×决策链教训, 同 -O3/sparse r1)。验收口径 =
 * 质量带 (ATE 门禁 + KF 带宽), 不可能 bit 等价 — 板端 A/B 见
 * slampart/ba_sparse_ab_test.md §六。
 *
 * 钩子: refactor/src/ba.c ngd_local_ba 内 NGD_BA_F32 最高优先分支
 * (F32 > SPARSE > FAST > dense)。开关默认 1 (ngd/ba_f32.h, 2026-11-24
 * 板端四门全过定档); -DNGD_BA_F32=0 = 空翻译单元 (v46 bit 参考回滚路径)。
 */
/* PC ngd_core 用 -std=c11 严格方言 (C_EXTENSIONS OFF), clock_gettime 需要
 * 特性宏; 板端 -std=gnu11 本就可见, __STRICT_ANSI__ 条件保证其预处理不变 */
#if defined(__STRICT_ANSI__) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 199309L
#endif

#include "ngd/ba_f32.h"   /* NGD_BA_F32 switch — 必须在守卫前 (提供默认值) */

#if NGD_BA_F32

#include "ngd/ba.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(__ARM_NEON__) && __ARM_NEON__
#include <arm_neon.h>
#endif

#define NGD_BA_DELTA_MONO_SQ   5.991
#define NGD_BA_DELTA_STEREO_SQ 7.815
#define NGD_BA_TAU             1e-5
#define NGD_BA_MAX_TRIALS      10
#ifndef NGD_BA_STOP_EPS
#define NGD_BA_STOP_EPS 1e-6
#endif
#define NGD_BA_NB_LU 32

/* ------------------------------------------------------------------ *
 * f64 SE3 / camera helpers — 与 ba.c 逐行相同 (static 副本)。
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
        q[0]=(R[2]-R[6])/s; q[1]=(R[1]+R[3])/s; q[2]=(R[5]+R[7])/s; q[3]=0.25*s;
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

/* ------------------------------------------------------------------ *
 * F1: f32 线性代数内核
 * ------------------------------------------------------------------ */

static int inv3_f32(const float A[9], float Ai[9]) {
    float det = A[0]*(A[4]*A[8]-A[5]*A[7])
              - A[1]*(A[3]*A[8]-A[5]*A[6])
              + A[2]*(A[3]*A[7]-A[4]*A[6]);
    if (fabsf(det) < 1e-18f || !isfinite(det)) return -1;
    float inv = 1.0f/det;
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

/* F2: 6×6 块散射 Hu[r][c] -= Σ_k BD[r*3+k]·Bt[k*6+c]
 * Hu 为 Hschur 内 6×6 块 (行距 dim_p); Bt = B^T (3×6, 行连续) */
static inline void f32_scatter6(float *Hu, int dim_p, const float *BD, const float *Bt)
{
#if defined(__ARM_NEON__) && __ARM_NEON__
    const float32x4_t bt0 = vld1q_f32(Bt),     bt1 = vld1q_f32(Bt+6),     bt2 = vld1q_f32(Bt+12);
    const float32x2_t bt0h= vld1_f32 (Bt+4),   bt1h= vld1_f32 (Bt+10),   bt2h= vld1_f32 (Bt+16);
    for (int r=0; r<6; ++r) {
        float *h = Hu + (size_t)r*dim_p;
        float32x4_t h0 = vld1q_f32(h);
        float32x2_t h1 = vld1_f32 (h+4);
        h0 = vmlsq_f32(h0, bt0, vdupq_n_f32(BD[r*3]));
        h1 = vmls_f32 (h1, bt0h, vdup_n_f32 (BD[r*3]));
        h0 = vmlsq_f32(h0, bt1, vdupq_n_f32(BD[r*3+1]));
        h1 = vmls_f32 (h1, bt1h, vdup_n_f32 (BD[r*3+1]));
        h0 = vmlsq_f32(h0, bt2, vdupq_n_f32(BD[r*3+2]));
        h1 = vmls_f32 (h1, bt2h, vdup_n_f32 (BD[r*3+2]));
        vst1q_f32(h, h0); vst1_f32(h+4, h1);
    }
#else
    for (int r=0; r<6; ++r) {
        float *h = Hu + (size_t)r*dim_p;
        const float b0=BD[r*3], b1=BD[r*3+1], b2=BD[r*3+2];
        for (int c=0; c<6; ++c)
            h[c] -= b0*Bt[c] + b1*Bt[6+c] + b2*Bt[12+c];
    }
#endif
}

/* F2: 6 元点积 (回代 rhs: Bt 行 · dxp 段, 双方 6 连续) */
static inline float f32_dot6(const float *a, const float *b)
{
#if defined(__ARM_NEON__) && __ARM_NEON__
    float32x4_t s = vmulq_f32(vld1q_f32(a), vld1q_f32(b));
    float32x2_t t = vmul_f32(vld1_f32(a+4), vld1_f32(b+4));
    float32x2_t sh = vadd_f32(vget_low_f32(s), vget_high_f32(s)); /* [s0+s2, s1+s3] */
    float32x2_t tt = vadd_f32(sh, t);
    return vget_lane_f32(tt, 0) + vget_lane_f32(tt, 1);
#else
    float s=0; for (int i=0;i<6;++i) s+=a[i]*b[i]; return s;
#endif
}

/* A7 分块 LU 的 f32 副本 (部分主元, 行主序, 块 NB; F2: 尾矩阵行更新向量化) */
static int solve_dense_tiled_f32(float *A, float *b, float *x, int n)
{
    const int NB = NGD_BA_NB_LU;
    if (n == 0) return 0;
    for (int i=0;i<n;++i) x[i]=b[i];

    for (int k0=0; k0<n; k0+=NB) {
        const int k1 = (k0+NB < n) ? k0+NB : n;

        for (int col=k0; col<k1; ++col) {
            int piv=col; float mx=fabsf(A[col*n+col]);
            for (int r=col+1;r<n;++r){ float v=fabsf(A[r*n+col]); if(v>mx){mx=v;piv=r;} }
            if (!(mx > 1e-18f) || !isfinite(mx)) return -1;
            if (piv!=col) {
                for (int c=k0;c<n;++c){ float t=A[col*n+c]; A[col*n+c]=A[piv*n+c]; A[piv*n+c]=t; }
                float t=x[col]; x[col]=x[piv]; x[piv]=t;
            }
            float d=A[col*n+col];
            for (int r=col+1;r<n;++r){
                float f=A[r*n+col]/d;
                A[r*n+col]=f;
                if (f==0.0f) continue;
                for (int c=col+1;c<k1;++c) A[r*n+c]-=f*A[col*n+c];
                x[r]-=f*x[col];
            }
        }

        for (int r=k0+1;r<n;++r){
            float *Ar=&A[r*n];
            const int kmax = (r < k1) ? r : k1;
            for (int k=k0;k<kmax;++k){
                float f=Ar[k];
                if (f==0.0f) continue;
                const float *Ak=&A[k*n];
#if defined(__ARM_NEON__) && __ARM_NEON__
                const float32x4_t fv = vdupq_n_f32(f);
                int c=k1;
                for (; c+4<=n; c+=4)
                    vst1q_f32(Ar+c, vmlsq_f32(vld1q_f32(Ar+c), vld1q_f32(Ak+c), fv));
                for (; c<n; ++c) Ar[c]-=f*Ak[c];
#else
                for (int c=k1;c<n;++c) Ar[c]-=f*Ak[c];
#endif
            }
        }
    }

    for (int r=n-1;r>=0;--r){
        float s=x[r];
        for (int c=r+1;c<n;++c) s-=A[r*n+c]*x[c];
        x[r]=s/A[r*n+r];
    }
    return 0;
}

/* ------------------------------------------------------------------ *
 * Per-edge linearisation — f64 评估 (F3 收口), J/r 落 f32 缓冲。
 * ------------------------------------------------------------------ */
static int edge_linearize_r_f32(const ngd_ba_problem *p,
                                const double R[9], const double poset[3],
                                const double Xw[3],
                                int stereo, int octave,
                                const float obs[3],
                                float r[3], double *chi2, float Jp[18], float Jl[9])
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
    r[0]=(float)(o0-u); r[1]=(float)(o1-v);
    if (stereo) r[2]=(float)(o2-(u - p->bf*invz));

    int lvl = octave; if (lvl<0||lvl>=p->calib->nlevels) lvl=0;
    double invS2 = (double)p->calib->mvInvLevelSigma2[lvl];
    double rr = r[0]*r[0]+r[1]*r[1]; if (stereo) rr += r[2]*r[2];
    *chi2 = invS2*rr;

    for (int rr2=0; rr2<dim; ++rr2)
        for (int cc=0; cc<3; ++cc) {
            double s=0; for (int k=0;k<3;++k) s += Pjac[rr2*3+k]*R[k*3+cc];
            Jl[rr2*3+cc] = (float)(-s);
        }
    double sd[18]; se3_deriv_d(Xc, sd);
    for (int rr2=0; rr2<dim; ++rr2)
        for (int cc=0; cc<6; ++cc) {
            double s=0; for (int k=0;k<3;++k) s += Pjac[rr2*3+k]*sd[k*6+cc];
            Jp[rr2*6+cc] = (float)(-s);
        }
    return dim;
}

/* A5: 只算残差与 chi2 (f64) — 与 lmneon.c 逐行相同 */
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
 * Core optimizer (mixed precision)。结构 = ngd_ba_optimize_fast。
 * ------------------------------------------------------------------ */
double ngd_ba_optimize_f32(ngd_ba_problem *p, int max_iters)
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

    /* ---- f64 state working copies (F3 收口: 状态/决策全 f64) ---- */
    double *pq = (double*)malloc((size_t)NP*4*sizeof(double));
    double *pt = (double*)malloc((size_t)NP*3*sizeof(double));
    double *X  = (double*)malloc((size_t)NM*3*sizeof(double));
    for (int i=0;i<NP;++i){ pq[i*4+0]=p->poses[i].q.w; pq[i*4+1]=p->poses[i].q.x;
                            pq[i*4+2]=p->poses[i].q.y; pq[i*4+3]=p->poses[i].q.z;
                            pt[i*3+0]=p->poses[i].t.x; pt[i*3+1]=p->poses[i].t.y; pt[i*3+2]=p->poses[i].t.z; }
    for (int j=0;j<NM;++j){ X[j*3+0]=p->points[j].x; X[j*3+1]=p->points[j].y; X[j*3+2]=p->points[j].z; }

    double *tq = (double*)malloc((size_t)NP*4*sizeof(double));
    double *tt = (double*)malloc((size_t)NP*3*sizeof(double));
    double *tX = (double*)malloc((size_t)NM*3*sizeof(double));

    /* ---- per-edge linearisation storage (F1: J/r/w 落 f32; invS2 f64) ---- */
    float  *eJp = (float*)malloc((size_t)NE*18*sizeof(float));
    float  *eJl = (float*)malloc((size_t)NE*9*sizeof(float));
    float  *er  = (float*)malloc((size_t)NE*3*sizeof(float));
    float  *ew  = (float*)malloc((size_t)NE*sizeof(float));       /* rho1*invS2 */
    int    *edim= (int*)malloc((size_t)NE*sizeof(int));
    double *einvS2=(double*)malloc((size_t)NE*sizeof(double));
    int    *efi = (int*)malloc((size_t)NE*sizeof(int));

    /* ---- Hessian blocks (F1: 全 f32) ---- */
    float  *Hpp = dim_p>0 ? (float*)calloc((size_t)dim_p*dim_p,sizeof(float)) : NULL;
    float  *Hll = (float*)calloc((size_t)NM*9,sizeof(float));
    float  *bp  = dim_p>0 ? (float*)calloc((size_t)dim_p,sizeof(float)) : NULL;
    float  *bl  = (float*)calloc((size_t)NM*3,sizeof(float));
    int *plen = (int*)calloc((size_t)NM, sizeof(int));

    float  *Hschur = dim_p>0 ? (float*)malloc((size_t)dim_p*dim_p*sizeof(float)) : NULL;
    float  *bschur = dim_p>0 ? (float*)malloc((size_t)dim_p*sizeof(float)) : NULL;
    float  *dxp = dim_p>0 ? (float*)malloc((size_t)dim_p*sizeof(float)) : NULL;
    float  *dxl = (float*)malloc((size_t)NM*3*sizeof(float));
    float  *Dinv= (float*)malloc((size_t)NM*9*sizeof(float));

    double *Rc = (double*)malloc((size_t)NP*9*sizeof(double));
    double *Rt = (double*)malloc((size_t)NP*9*sizeof(double));

    int   *poff    = (int*)malloc((size_t)(NM+1)*sizeof(int));
    int   *pl_pose = NE>0 ? (int*)malloc((size_t)NE*sizeof(int)) : NULL;
    float *pl_blk  = NE>0 ? (float*)malloc((size_t)NE*18*sizeof(float)) : NULL;  /* 6x3 Ba */
    float *pl_blkT = NE>0 ? (float*)malloc((size_t)NE*18*sizeof(float)) : NULL;  /* 3x6 Bt (F4) */
    int   *pcurs   = (int*)malloc((size_t)NM*sizeof(int));

    float  *bdscratch = n_free>0 ? (float*)malloc((size_t)n_free*18*sizeof(float)) : NULL;
    float  *Hcopy = dim_p>0 ? (float*)malloc((size_t)dim_p*dim_p*sizeof(float)) : NULL;
    double *dxd   = dim_p>0 ? (double*)malloc((size_t)dim_p*sizeof(double)) : NULL; /* dxp f64 收口 */

    if (!free_idx || !pq || !pt || !X || !tq || !tt || !tX ||
        !eJp || !eJl || !er || !ew || !edim || !einvS2 || !efi ||
        (dim_p>0 && (!Hpp || !bp)) || !Hll || !bl || !plen ||
        (dim_p>0 && (!Hschur || !bschur || !dxp || !Hcopy || !dxd)) || !dxl || !Dinv ||
        !Rc || !Rt || !poff || !pcurs || (NE>0 && (!pl_pose || !pl_blk || !pl_blkT)) ||
        (n_free>0 && !bdscratch))
        goto cleanup;

    double currentChi = 0.0;
    double lambda = -1.0;
    int it_done = 0;
    double last_gain = 0.0;

    /* initial chi2 + linearisation */
    for (int i=0;i<NP;++i) quat_to_R_d(&pq[i*4], &Rc[i*9]);
    for (int e=0;e<NE;++e){
        const ngd_ba_edge *E=&p->edges[e];
        double chi2; int dim=edge_linearize_r_f32(p,&Rc[E->pose_idx*9],&pt[E->pose_idx*3],
                                              &X[E->point_idx*3],E->stereo,E->octave,E->obs,
                                              &er[e*3],&chi2,&eJp[e*18],&eJl[e*9]);
        edim[e]=dim;
        if (dim==0){ einvS2[e]=0; ew[e]=0; efi[e]=-1; continue; }
        int lvl=E->octave; if(lvl<0||lvl>=p->calib->nlevels) lvl=0;
        einvS2[e]=(double)p->calib->mvInvLevelSigma2[lvl];
        double delta = E->stereo ? sqrt(NGD_BA_DELTA_STEREO_SQ) : sqrt(NGD_BA_DELTA_MONO_SQ);
        double rho1 = huber_rho1(chi2, delta);
        ew[e] = (float)(rho1*einvS2[e]);
        efi[e] = free_idx[E->pose_idx];
        currentChi += huber_rho0(chi2, delta);
    }

    for (int it=0; it<max_iters; ++it) {
        it_done = it+1;
        /* ---- assemble undamped Hpp/Hll/Hpl/bp/bl (f32; A3 上三角) ---- */
        if (dim_p>0) memset(Hpp, 0, (size_t)dim_p*dim_p*sizeof(float));
        memset(Hll, 0, (size_t)NM*9*sizeof(float));
        if (dim_p>0) memset(bp, 0, (size_t)dim_p*sizeof(float));
        memset(bl, 0, (size_t)NM*3*sizeof(float));
        memset(plen, 0, (size_t)NM*sizeof(int));
        for (int e=0;e<NE;++e) if (edim[e]>0 && efi[e]>=0) plen[p->edges[e].point_idx]++;

        poff[0]=0; for (int j=0;j<NM;++j) poff[j+1]=poff[j]+plen[j];
        for (int j=0;j<NM;++j) pcurs[j]=poff[j];

        for (int e=0;e<NE;++e){
            if (edim[e]==0) continue;
            const ngd_ba_edge *E=&p->edges[e];
            int dim=edim[e];
            int fi=efi[e];
            int j=E->point_idx;
            const float *Jp=&eJp[e*18];
            const float *Jl=&eJl[e*9];
            const float *r =&er[e*3];
            float w=ew[e];

            for (int a=0;a<3;++a) for (int b=0;b<3;++b){
                float s=0; for (int k=0;k<dim;++k) s+=Jl[k*3+a]*Jl[k*3+b]; Hll[j*9+a*3+b]+=w*s;
            }
            for (int a=0;a<3;++a){ float s=0; for (int k=0;k<dim;++k) s+=Jl[k*3+a]*r[k]; bl[j*3+a]-=w*s; }

            if (fi>=0) {
                for (int a=0;a<6;++a) for (int b=a;b<6;++b){   /* A3 上三角 */
                    float s=0; for (int k=0;k<dim;++k) s+=Jp[k*6+a]*Jp[k*6+b];
                    Hpp[(6*fi+a)*dim_p+(6*fi+b)]+=w*s;
                }
                for (int a=0;a<6;++a){ float s=0; for (int k=0;k<dim;++k) s+=Jp[k*6+a]*r[k]; bp[6*fi+a]-=w*s; }
                int slot=pcurs[j]++;
                pl_pose[slot]=fi;
                float *blk =&pl_blk [slot*18];   /* 6x3 Ba */
                float *blkT=&pl_blkT[slot*18];   /* 3x6 Bt (F4 同值双写) */
                for (int a=0;a<6;++a) for (int b=0;b<3;++b){
                    float s=0; for (int k=0;k<dim;++k) s+=Jp[k*6+a]*Jl[k*3+b];
                    blk[a*3+b]=w*s; blkT[b*6+a]=w*s;
                }
            }
        }

        /* ---- lambda init (iter 0): max diag (f64 收口) ---- */
        if (it==0) {
            double maxd=0;
            for (int i=0;i<dim_p;++i){ double v=fabs((double)Hpp[i*dim_p+i]); if(v>maxd)maxd=v; }
            for (int j=0;j<NM;++j) for (int a=0;a<3;++a){ double v=fabs((double)Hll[j*9+a*3+a]); if(v>maxd)maxd=v; }
            lambda = NGD_BA_TAU*maxd;
            if (lambda<=0) lambda=1e-6;
        }

        /* ---- inner LM trial loop ---- */
        double ni=2.0;
        int qmax=0, accepted=0;
        do {
            if (dim_p>0) {
                memcpy(Hschur, Hpp, (size_t)dim_p*dim_p*sizeof(float));
                const float lamf=(float)lambda;
                for (int i=0;i<dim_p;++i) Hschur[i*dim_p+i]+=lamf;
                memcpy(bschur, bp, (size_t)dim_p*sizeof(float));
            }
            for (int j=0;j<NM;++j){
                float D[9];
                for (int a=0;a<9;++a) D[a]=Hll[j*9+a];
                const float lamf=(float)lambda;
                D[0]+=lamf; D[4]+=lamf; D[8]+=lamf;
                float Di[9];
                if (inv3_f32(D, Di)!=0) {
                    for (int a=0;a<9;++a) Dinv[j*9+a]=0;
                    continue;
                }
                for (int a=0;a<9;++a) Dinv[j*9+a]=Di[a];
                float Dibl[3] = { Di[0]*bl[j*3+0]+Di[1]*bl[j*3+1]+Di[2]*bl[j*3+2],
                                  Di[3]*bl[j*3+0]+Di[4]*bl[j*3+1]+Di[5]*bl[j*3+2],
                                  Di[6]*bl[j*3+0]+Di[7]*bl[j*3+1]+Di[8]*bl[j*3+2] };
                const int s0=poff[j], s1=poff[j+1];
                for (int a=s0;a<s1;++a){
                    int i1=pl_pose[a];
                    const float *Ba=&pl_blk[a*18];
                    for (int r=0;r<6;++r){
                        float s=Ba[r*3+0]*Dibl[0]+Ba[r*3+1]*Dibl[1]+Ba[r*3+2]*Dibl[2];
                        bschur[6*i1+r]-=s;
                    }
                    float *BD=&bdscratch[(a-s0)*18];
                    for (int r=0;r<6;++r) for(int k=0;k<3;++k){
                        float s=Ba[r*3+0]*Di[k] + Ba[r*3+1]*Di[3+k] + Ba[r*3+2]*Di[6+k];
                        BD[r*3+k]=s;
                    }
                }
                /* Hschur_{i1,i2} -= B_{i1}·Di·B_{i2}^T (A3 无序对+上三角; F2 散射内核) */
                for (int a=s0;a<s1;++a){
                    int i1=pl_pose[a];
                    const float *BDa=&bdscratch[(a-s0)*18];
                    const float *Bta=&pl_blkT[a*18];
                    for (int b=a;b<s1;++b){
                        int i2=pl_pose[b];
                        if (i1==i2) {
                            float *Hd=&Hschur[(6*i1)*dim_p+6*i1];
                            const float *Bb=&pl_blk[b*18];
                            for (int r=0;r<6;++r) for (int c=r;c<6;++c){
                                float s=BDa[r*3+0]*Bb[c*3+0]+BDa[r*3+1]*Bb[c*3+1]+BDa[r*3+2]*Bb[c*3+2];
                                Hd[r*dim_p+c]-=s;
                            }
                        } else if (i1<i2) {
                            const float *Btb=&pl_blkT[b*18];
                            f32_scatter6(&Hschur[(6*i1)*dim_p+6*i2], dim_p, BDa, Btb);
                        } else {
                            /* i1>i2: 上三角块在 (i2,i1)，用 BD_b·Ba^T = BD_b·Bta */
                            const float *BDb=&bdscratch[(b-s0)*18];
                            f32_scatter6(&Hschur[(6*i2)*dim_p+6*i1], dim_p, BDb, Bta);
                        }
                    }
                }
            }
            /* A3 镜像补下三角 */
            if (dim_p>0) {
                for (int r=1;r<dim_p;++r) {
                    float *Hrow=&Hschur[r*dim_p];
                    for (int c=0;c<r;++c) Hrow[c]=Hschur[c*dim_p+r];
                }
            }

            int solved = 1;
            if (dim_p>0) {
                memcpy(Hcopy, Hschur, (size_t)dim_p*dim_p*sizeof(float));
                solved = (solve_dense_tiled_f32(Hcopy, bschur, dxp, dim_p)==0);
            }

            if (!solved) { qmax++; lambda*=ni; ni*=2.0; continue; }

            /* back-sub: dxl_j = Dinv_j·(bl_j − Hpl^T·dxp) — Bt 行·dxp 段 (F2) */
            for (int j=0;j<NM;++j){
                float rhs[3] = {bl[j*3+0], bl[j*3+1], bl[j*3+2]};
                for (int a=poff[j]; a<poff[j+1]; ++a){
                    int i1=pl_pose[a];
                    const float *Bta=&pl_blkT[a*18];
                    const float *dxseg=&dxp[6*i1];
                    rhs[0]-=f32_dot6(&Bta[0],  dxseg);
                    rhs[1]-=f32_dot6(&Bta[6],  dxseg);
                    rhs[2]-=f32_dot6(&Bta[12], dxseg);
                }
                const float *Di=&Dinv[j*9];
                dxl[j*3+0]=Di[0]*rhs[0]+Di[1]*rhs[1]+Di[2]*rhs[2];
                dxl[j*3+1]=Di[3]*rhs[0]+Di[4]*rhs[1]+Di[5]*rhs[2];
                dxl[j*3+2]=Di[6]*rhs[0]+Di[7]*rhs[1]+Di[8]*rhs[2];
            }

            /* trial estimate (f64 收口: dxp→f64, se3 全 f64) */
            memcpy(tq, pq, (size_t)NP*4*sizeof(double));
            memcpy(tt, pt, (size_t)NP*3*sizeof(double));
            memcpy(tX, X,  (size_t)NM*3*sizeof(double));
            for (int i=0;i<dim_p;++i) dxd[i]=(double)dxp[i];
            for (int i=0;i<NP;++i){
                int fi=free_idx[i];
                if (fi<0) continue;
                double dq[4], dt[3];
                se3_exp_d(&dxd[6*fi], dq, dt);
                se3_mul_d(dq, dt, &pq[i*4], &pt[i*3], &tq[i*4], &tt[i*3]);
            }
            for (int j=0;j<NM;++j){ tX[j*3+0]=X[j*3+0]+(double)dxl[j*3+0];
                                     tX[j*3+1]=X[j*3+1]+(double)dxl[j*3+1];
                                     tX[j*3+2]=X[j*3+2]+(double)dxl[j*3+2]; }

            /* tempChi at trial (f64, A5 chi-only) */
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

            /* Nielsen gain ratio (f64 收口) */
            double scale=1e-3;
            for (int i=0;i<dim_p;++i){ double d=(double)dxp[i]; scale += d*(lambda*d + (double)bp[i]); }
            for (int j=0;j<NM;++j) for(int k=0;k<3;++k){ double d=(double)dxl[j*3+k]; scale += d*(lambda*d + (double)bl[j*3+k]); }
            double rho = (currentChi - tempChi)/scale;

            if (rho>0 && isfinite(tempChi)) {
                double alpha = 1.0 - pow(2.0*rho-1.0, 3.0);
                if (alpha < 1.0/3.0) alpha=1.0/3.0;
                if (alpha > 2.0/3.0) alpha=2.0/3.0;
                lambda *= alpha;
                ni=2.0;
                double chi_before = currentChi;
                currentChi=tempChi;
                memcpy(pq, tq, (size_t)NP*4*sizeof(double));
                memcpy(pt, tt, (size_t)NP*3*sizeof(double));
                memcpy(X,  tX, (size_t)NM*3*sizeof(double));
                last_gain = chi_before>0 ? (chi_before-tempChi)/chi_before : 1.0;
                /* re-linearise at accepted estimate */
                for (int i=0;i<NP;++i) quat_to_R_d(&pq[i*4], &Rc[i*9]);
                for (int e=0;e<NE;++e){
                    const ngd_ba_edge *E=&p->edges[e];
                    double chi2;
                    int dim=edge_linearize_r_f32(p,&Rc[E->pose_idx*9],&pt[E->pose_idx*3],
                                             &X[E->point_idx*3],E->stereo,E->octave,E->obs,
                                             &er[e*3],&chi2,&eJp[e*18],&eJl[e*9]);
                    edim[e]=dim;
                    if (dim==0){ ew[e]=0; efi[e]=-1; continue; }
                    double delta = E->stereo ? sqrt(NGD_BA_DELTA_STEREO_SQ) : sqrt(NGD_BA_DELTA_MONO_SQ);
                    ew[e]=(float)(huber_rho1(chi2,delta)*einvS2[e]);
                    efi[e]=free_idx[E->pose_idx];
                }
                accepted=1;
                break;
            } else {
                lambda*=ni; ni*=2.0;
            }
            qmax++;
        } while (!accepted && qmax < NGD_BA_MAX_TRIALS);

        if (!accepted) break;
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

    /* ---- outlier classification (f64, A5 chi-only) ---- */
    for (int e=0;e<NE;++e){
        const ngd_ba_edge *E=&p->edges[e];
        double chi2;
        int dim=edge_chi(p,&Rc[E->pose_idx*9],&pt[E->pose_idx*3],
                         &X[E->point_idx*3],E->stereo,E->octave,E->obs,&chi2);
        double thr = E->stereo ? NGD_BA_DELTA_STEREO_SQ : NGD_BA_DELTA_MONO_SQ;
        p->outlier[e] = (dim==0 || chi2>thr) ? 1 : 0;
    }

    fprintf(stderr, "[baf32] call=%d NP=%d NE=%d dim_p=%d iters=%d last_gain=%.2e chi=%.1f opt=%.0fms\n",
            call_count, NP, NE, dim_p, it_done, last_gain, currentChi, baf_now()-_t0);

cleanup:
    free(free_idx); free(pq); free(pt); free(X); free(tq); free(tt); free(tX);
    free(eJp); free(eJl); free(er); free(ew); free(edim); free(einvS2); free(efi);
    free(Hpp); free(Hll); free(bp); free(bl); free(plen);
    free(Hschur); free(bschur); free(dxp); free(dxl); free(Dinv);
    free(Rc); free(Rt);
    free(poff); free(pl_pose); free(pl_blk); free(pl_blkT); free(pcurs);
    free(bdscratch); free(Hcopy); free(dxd);

    return currentChi;
}

#endif /* NGD_BA_F32 */
