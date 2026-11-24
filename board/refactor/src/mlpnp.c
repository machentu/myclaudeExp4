/* ngd/mlpnp.c — Maximum-Likelihood PnP + RANSAC (pure C).
 *
 * Faithful port of NGD-SLAM's MLPnPsolver (src/MLPnPsolver.cpp). See
 * include/ngd/mlpnp.h for scope, faithfulness notes, and the dropped (dead)
 * covariance path. Internal math in double for conditioning.
 *
 * The 200-line hand-symbolic Jacobian (mlpnpJacs) and the design-matrix /
 * pose-recovery blocks are transcribed verbatim from MLPnPsolver.cpp:808-1055
 * and :425-637; only Eigen syntax is replaced by plain C arrays. */
#include "ngd/mlpnp.h"
#include "ngd/math.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

ngd_mlpnp_ransac_params ngd_mlpnp_ransac_defaults(void)
{
    ngd_mlpnp_ransac_params p;
    p.prob = 0.99;
    p.minInliers = 10;
    p.maxIterations = 300;
    p.minSet = 6;
    p.epsilon = 0.5f;
    p.th2 = 5.991f;
    return p;
}

/* ============================ linear algebra ============================ */
/* Self-contained double helpers (same proven implementations as src/pnp.c /
 * src/ba.c; kept private here per the refactor's per-module linalg convention). */

/* Solve A x = b (n×n, n<=16) via Gauss elimination with partial pivoting. */
static int solve_linear_d(double *A, const double *b, double *x, int n)
{
    double Ab[16 * 17];
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) Ab[i * (n + 1) + j] = A[i * n + j];
        Ab[i * (n + 1) + n] = b[i];
    }
    for (int k = 0; k < n; ++k) {
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

/* Cyclic Jacobi eigendecomposition of a symmetric n×n matrix (n<=12).
 * A (row-major, modified in place), V (eigenvectors as columns), w (eigenvalues,
 * ascending). V[i*n+j] = i-th component of j-th eigenvector. */
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

/* SVD of a general 3×3 A = U diag(S) V^T (S descending). U[:,k]=U[r*3+k]. */
static void svd3(const double A[9], double U[9], double S[3], double V[9])
{
    double ATA[9];
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            double s = 0.0;
            for (int k = 0; k < 3; ++k) s += A[k * 3 + i] * A[k * 3 + j];
            ATA[i * 3 + j] = s;
        }
    double Vm[9], w[3];
    jacobi_sym(ATA, 3, Vm, w);
    int idx[3] = {0, 1, 2};
    for (int i = 0; i < 2; ++i)
        for (int j = i + 1; j < 3; ++j)
            if (w[idx[j]] > w[idx[i]]) { int t = idx[i]; idx[i] = idx[j]; idx[j] = t; }
    for (int k = 0; k < 3; ++k) {
        S[k] = (w[idx[k]] > 0.0) ? sqrt(w[idx[k]]) : 0.0;
        for (int r = 0; r < 3; ++r) V[r * 3 + k] = Vm[r * 3 + idx[k]];
    }
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
            double u[3] = {0, 0, 0};
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

/* ============================ Rodrigues ============================ */
/* rodrigues2rot (MLPnPsolver.cpp:660). R = I + sin(θ)/θ [w]× + (1-cos θ)/θ² [w]×². */
static void rodrigues2rot(const double w[3], double R[9])
{
    R[0]=1; R[4]=1; R[8]=1; R[1]=R[2]=R[3]=R[5]=R[6]=R[7]=0;
    double wn = sqrt(w[0]*w[0]+w[1]*w[1]+w[2]*w[2]);
    if (wn > 1e-16) {   /* numeric_limits<double>::epsilon() */
        double sk[9] = {0, -w[2], w[1],  w[2], 0, -w[0],  -w[1], w[0], 0};
        double sk2[9];
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) {
                double s = 0.0;
                for (int k = 0; k < 3; ++k) s += sk[i*3+k]*sk[k*3+j];
                sk2[i*3+j] = s;
            }
        double a = sin(wn)/wn, b = (1.0 - cos(wn))/(wn*wn);
        for (int i = 0; i < 9; ++i) R[i] = R[i] + a*sk[i] + b*sk2[i];
    }
}

/* rot2rodrigues (MLPnPsolver.cpp:677). log map. */
static void rot2rodrigues(const double R[9], double w[3])
{
    w[0]=w[1]=w[2]=0.0;
    double trace = R[0]+R[4]+R[8] - 1.0;
    double wnorm = acos(trace / 2.0);
    if (wnorm > 1e-16) {
        w[0] = R[7] - R[5];   /* R(2,1)-R(1,2) */
        w[1] = R[2] - R[6];   /* R(0,2)-R(2,0) */
        w[2] = R[3] - R[1];   /* R(1,0)-R(0,1) */
        double sc = wnorm / (2.0*sin(wnorm));
        w[0] *= sc; w[1] *= sc; w[2] *= sc;
    }
}

/* ============================ MLPnP Jacobian (verbatim) ============================ */
/* mlpnpJacs (MLPnPsolver.cpp:808-1055). jacs is 2×6 row-major: jacs[row*6+col].
 * nullspace_r / nullspace_s are the two 3-vector nullspace columns. */
static void mlpnp_jacs(const double pt[3],
                       const double nr[3], const double ns[3],
                       const double w[3], const double t[3],
                       double jacs[12])
{
    double r1=nr[0], r2=nr[1], r3=nr[2];
    double s1=ns[0], s2=ns[1], s3=ns[2];
    double X1=pt[0], Y1=pt[1], Z1=pt[2];
    double w1=w[0], w2=w[1], w3=w[2];
    double t1=t[0], t2=t[1], t3=t[2];

    double t5 = w1*w1;
    double t6 = w2*w2;
    double t7 = w3*w3;
    double t8 = t5+t6+t7;
    double t9 = sqrt(t8);
    double t10 = sin(t9);
    double t11 = 1.0/sqrt(t8);
    double t12 = cos(t9);
    double t13 = t12-1.0;
    double t14 = 1.0/t8;
    double t16 = t10*t11*w3;
    double t17 = t13*t14*w1*w2;
    double t19 = t10*t11*w2;
    double t20 = t13*t14*w1*w3;
    double t24 = t6+t7;
    double t27 = t16+t17;
    double t28 = Y1*t27;
    double t29 = t19-t20;
    double t30 = Z1*t29;
    double t31 = t13*t14*t24;
    double t32 = t31+1.0;
    double t33 = X1*t32;
    double t15 = t1-t28+t30+t33;
    double t21 = t10*t11*w1;
    double t22 = t13*t14*w2*w3;
    double t45 = t5+t7;
    double t53 = t16-t17;
    double t54 = X1*t53;
    double t55 = t21+t22;
    double t56 = Z1*t55;
    double t57 = t13*t14*t45;
    double t58 = t57+1.0;
    double t59 = Y1*t58;
    double t18 = t2+t54-t56+t59;
    double t34 = t5+t6;
    double t38 = t19+t20;
    double t39 = X1*t38;
    double t40 = t21-t22;
    double t41 = Y1*t40;
    double t42 = t13*t14*t34;
    double t43 = t42+1.0;
    double t44 = Z1*t43;
    double t23 = t3-t39+t41+t44;
    double t25 = 1.0/pow(t8,3.0/2.0);
    double t26 = 1.0/(t8*t8);
    double t35 = t12*t14*w1*w2;
    double t36 = t5*t10*t25*w3;
    double t37 = t5*t13*t26*w3*2.0;
    double t46 = t10*t25*w1*w3;
    double t47 = t5*t10*t25*w2;
    double t48 = t5*t13*t26*w2*2.0;
    double t49 = t10*t11;
    double t50 = t5*t12*t14;
    double t51 = t13*t26*w1*w2*w3*2.0;
    double t52 = t10*t25*w1*w2*w3;
    double t60 = t15*t15;
    double t61 = t18*t18;
    double t62 = t23*t23;
    double t63 = t60+t61+t62;
    double t64 = t5*t10*t25;
    double t65 = 1.0/sqrt(t63);
    double t66 = Y1*r2*t6;
    double t67 = Z1*r3*t7;
    double t68 = r1*t1*t5;
    double t69 = r1*t1*t6;
    double t70 = r1*t1*t7;
    double t71 = r2*t2*t5;
    double t72 = r2*t2*t6;
    double t73 = r2*t2*t7;
    double t74 = r3*t3*t5;
    double t75 = r3*t3*t6;
    double t76 = r3*t3*t7;
    double t77 = X1*r1*t5;
    double t78 = X1*r2*w1*w2;
    double t79 = X1*r3*w1*w3;
    double t80 = Y1*r1*w1*w2;
    double t81 = Y1*r3*w2*w3;
    double t82 = Z1*r1*w1*w3;
    double t83 = Z1*r2*w2*w3;
    double t84 = X1*r1*t6*t12;
    double t85 = X1*r1*t7*t12;
    double t86 = Y1*r2*t5*t12;
    double t87 = Y1*r2*t7*t12;
    double t88 = Z1*r3*t5*t12;
    double t89 = Z1*r3*t6*t12;
    double t90 = X1*r2*t9*t10*w3;
    double t91 = Y1*r3*t9*t10*w1;
    double t92 = Z1*r1*t9*t10*w2;
    double t102 = X1*r3*t9*t10*w2;
    double t103 = Y1*r1*t9*t10*w3;
    double t104 = Z1*r2*t9*t10*w1;
    double t105 = X1*r2*t12*w1*w2;
    double t106 = X1*r3*t12*w1*w3;
    double t107 = Y1*r1*t12*w1*w2;
    double t108 = Y1*r3*t12*w2*w3;
    double t109 = Z1*r1*t12*w1*w3;
    double t110 = Z1*r2*t12*w2*w3;
    double t93 = t66+t67+t68+t69+t70+t71+t72+t73+t74+t75+t76+t77+t78+t79+t80+t81+t82+t83+t84+t85+t86+t87+t88+t89+t90+t91+t92-t102-t103-t104-t105-t106-t107-t108-t109-t110;
    double t94 = t10*t25*w1*w2;
    double t95 = t6*t10*t25*w3;
    double t96 = t6*t13*t26*w3*2.0;
    double t97 = t12*t14*w2*w3;
    double t98 = t6*t10*t25*w1;
    double t99 = t6*t13*t26*w1*2.0;
    double t100 = t6*t10*t25;
    double t101 = 1.0/pow(t63,3.0/2.0);
    double t111 = t6*t12*t14;
    double t112 = t10*t25*w2*w3;
    double t113 = t12*t14*w1*w3;
    double t114 = t7*t10*t25*w2;
    double t115 = t7*t13*t26*w2*2.0;
    double t116 = t7*t10*t25*w1;
    double t117 = t7*t13*t26*w1*2.0;
    double t118 = t7*t12*t14;
    double t119 = t13*t24*t26*w1*2.0;
    double t120 = t10*t24*t25*w1;
    double t121 = t119+t120;
    double t122 = t13*t26*t34*w1*2.0;
    double t123 = t10*t25*t34*w1;
    double t131 = t13*t14*w1*2.0;
    double t124 = t122+t123-t131;
    double t139 = t13*t14*w3;
    double t125 = -t35+t36+t37+t94-t139;
    double t126 = X1*t125;
    double t127 = t49+t50+t51+t52-t64;
    double t128 = Y1*t127;
    double t129 = t126+t128-Z1*t124;
    double t130 = t23*t129*2.0;
    double t132 = t13*t26*t45*w1*2.0;
    double t133 = t10*t25*t45*w1;
    double t138 = t13*t14*w2;
    double t134 = -t46+t47+t48+t113-t138;
    double t135 = X1*t134;
    double t136 = -t49-t50+t51+t52+t64;
    double t137 = Z1*t136;
    double t140 = X1*s1*t5;
    double t141 = Y1*s2*t6;
    double t142 = Z1*s3*t7;
    double t143 = s1*t1*t5;
    double t144 = s1*t1*t6;
    double t145 = s1*t1*t7;
    double t146 = s2*t2*t5;
    double t147 = s2*t2*t6;
    double t148 = s2*t2*t7;
    double t149 = s3*t3*t5;
    double t150 = s3*t3*t6;
    double t151 = s3*t3*t7;
    double t152 = X1*s2*w1*w2;
    double t153 = X1*s3*w1*w3;
    double t154 = Y1*s1*w1*w2;
    double t155 = Y1*s3*w2*w3;
    double t156 = Z1*s1*w1*w3;
    double t157 = Z1*s2*w2*w3;
    double t158 = X1*s1*t6*t12;
    double t159 = X1*s1*t7*t12;
    double t160 = Y1*s2*t5*t12;
    double t161 = Y1*s2*t7*t12;
    double t162 = Z1*s3*t5*t12;
    double t163 = Z1*s3*t6*t12;
    double t164 = X1*s2*t9*t10*w3;
    double t165 = Y1*s3*t9*t10*w1;
    double t166 = Z1*s1*t9*t10*w2;
    double t183 = X1*s3*t9*t10*w2;
    double t184 = Y1*s1*t9*t10*w3;
    double t185 = Z1*s2*t9*t10*w1;
    double t186 = X1*s2*t12*w1*w2;
    double t187 = X1*s3*t12*w1*w3;
    double t188 = Y1*s1*t12*w1*w2;
    double t189 = Y1*s3*t12*w2*w3;
    double t190 = Z1*s1*t12*w1*w3;
    double t191 = Z1*s2*t12*w2*w3;
    double t167 = t140+t141+t142+t143+t144+t145+t146+t147+t148+t149+t150+t151+t152+t153+t154+t155+t156+t157+t158+t159+t160+t161+t162+t163+t164+t165+t166-t183-t184-t185-t186-t187-t188-t189-t190-t191;
    double t168 = t13*t26*t45*w2*2.0;
    double t169 = t10*t25*t45*w2;
    double t170 = t168+t169;
    double t171 = t13*t26*t34*w2*2.0;
    double t172 = t10*t25*t34*w2;
    double t176 = t13*t14*w2*2.0;
    double t173 = t171+t172-t176;
    double t174 = -t49+t51+t52+t100-t111;
    double t175 = X1*t174;
    double t177 = t13*t24*t26*w2*2.0;
    double t178 = t10*t24*t25*w2;
    double t192 = t13*t14*w1;
    double t179 = -t97+t98+t99+t112-t192;
    double t180 = Y1*t179;
    double t181 = t49+t51+t52-t100+t111;
    double t182 = Z1*t181;
    double t193 = t13*t26*t34*w3*2.0;
    double t194 = t10*t25*t34*w3;
    double t195 = t193+t194;
    double t196 = t13*t26*t45*w3*2.0;
    double t197 = t10*t25*t45*w3;
    double t200 = t13*t14*w3*2.0;
    double t198 = t196+t197-t200;
    double t199 = t7*t10*t25;
    double t201 = t13*t24*t26*w3*2.0;
    double t202 = t10*t24*t25*w3;
    double t203 = -t49+t51+t52-t118+t199;
    double t204 = Y1*t203;
    double t205 = t1*2.0;
    double t206 = Z1*t29*2.0;
    double t207 = X1*t32*2.0;
    double t208 = t205+t206+t207-Y1*t27*2.0;
    double t209 = t2*2.0;
    double t210 = X1*t53*2.0;
    double t211 = Y1*t58*2.0;
    double t212 = t209+t210+t211-Z1*t55*2.0;
    double t213 = t3*2.0;
    double t214 = Y1*t40*2.0;
    double t215 = Z1*t43*2.0;
    double t216 = t213+t214+t215-X1*t38*2.0;

    jacs[0*6+0] = t14*t65*(X1*r1*w1*2.0+X1*r2*w2+X1*r3*w3+Y1*r1*w2+Z1*r1*w3+r1*t1*w1*2.0+r2*t2*w1*2.0+r3*t3*w1*2.0+Y1*r3*t5*t12+Y1*r3*t9*t10-Z1*r2*t5*t12-Z1*r2*t9*t10-X1*r2*t12*w2-X1*r3*t12*w3-Y1*r1*t12*w2+Y1*r2*t12*w1*2.0-Z1*r1*t12*w3+Z1*r3*t12*w1*2.0+Y1*r3*t5*t10*t11-Z1*r2*t5*t10*t11+X1*r2*t12*w1*w3-X1*r3*t12*w1*w2-Y1*r1*t12*w1*w3+Z1*r1*t12*w1*w2-Y1*r1*t10*t11*w1*w3+Z1*r1*t10*t11*w1*w2-X1*r1*t6*t10*t11*w1-X1*r1*t7*t10*t11*w1+X1*r2*t5*t10*t11*w2+X1*r3*t5*t10*t11*w3+Y1*r1*t5*t10*t11*w2-Y1*r2*t5*t10*t11*w1-Y1*r2*t7*t10*t11*w1+Z1*r1*t5*t10*t11*w3-Z1*r3*t5*t10*t11*w1-Z1*r3*t6*t10*t11*w1+X1*r2*t10*t11*w1*w3-X1*r3*t10*t11*w1*w2+Y1*r3*t10*t11*w1*w2*w3+Z1*r2*t10*t11*w1*w2*w3)-t26*t65*t93*w1*2.0-t14*t93*t101*(t130+t15*(-X1*t121+Y1*(t46+t47+t48-t13*t14*w2-t12*t14*w1*w3)+Z1*(t35+t36+t37-t13*t14*w3-t10*t25*w1*w2))*2.0+t18*(t135+t137-Y1*(t132+t133-t13*t14*w1*2.0))*2.0)*(1.0/2.0);
    jacs[0*6+1] = t14*t65*(X1*r2*w1+Y1*r1*w1+Y1*r2*w2*2.0+Y1*r3*w3+Z1*r2*w3+r1*t1*w2*2.0+r2*t2*w2*2.0+r3*t3*w2*2.0-X1*r3*t6*t12-X1*r3*t9*t10+Z1*r1*t6*t12+Z1*r1*t9*t10+X1*r1*t12*w2*2.0-X1*r2*t12*w1-Y1*r1*t12*w1-Y1*r3*t12*w3-Z1*r2*t12*w3+Z1*r3*t12*w2*2.0-X1*r3*t6*t10*t11+Z1*r1*t6*t10*t11+X1*r2*t12*w2*w3-Y1*r1*t12*w2*w3+Y1*r3*t12*w1*w2-Z1*r2*t12*w1*w2-Y1*r1*t10*t11*w2*w3+Y1*r3*t10*t11*w1*w2-Z1*r2*t10*t11*w1*w2-X1*r1*t6*t10*t11*w2+X1*r2*t6*t10*t11*w1-X1*r1*t7*t10*t11*w2+Y1*r1*t6*t10*t11*w1-Y1*r2*t5*t10*t11*w2-Y1*r2*t7*t10*t11*w2+Y1*r3*t6*t10*t11*w3-Z1*r3*t5*t10*t11*w2+Z1*r2*t6*t10*t11*w3-Z1*r3*t6*t10*t11*w2+X1*r2*t10*t11*w2*w3+X1*r3*t10*t11*w1*w2*w3+Z1*r1*t10*t11*w1*w2*w3)-t26*t65*t93*w2*2.0-t14*t93*t101*(t18*(Z1*(-t35+t94+t95+t96-t13*t14*w3)-Y1*t170+X1*(t97+t98+t99-t13*t14*w1-t10*t25*w2*w3))*2.0+t15*(t180+t182-X1*(t177+t178-t13*t14*w2*2.0))*2.0+t23*(t175+Y1*(t35-t94+t95+t96-t13*t14*w3)-Z1*t173)*2.0)*(1.0/2.0);
    jacs[0*6+2] = t14*t65*(X1*r3*w1+Y1*r3*w2+Z1*r1*w1+Z1*r2*w2+Z1*r3*w3*2.0+r1*t1*w3*2.0+r2*t2*w3*2.0+r3*t3*w3*2.0+X1*r2*t7*t12+X1*r2*t9*t10-Y1*r1*t7*t12-Y1*r1*t9*t10+X1*r1*t12*w3*2.0-X1*r3*t12*w1+Y1*r2*t12*w3*2.0-Y1*r3*t12*w2-Z1*r1*t12*w1-Z1*r2*t12*w2+X1*r2*t7*t10*t11-Y1*r1*t7*t10*t11-X1*r3*t12*w2*w3+Y1*r3*t12*w1*w3+Z1*r1*t12*w2*w3-Z1*r2*t12*w1*w3+Y1*r3*t10*t11*w1*w3+Z1*r1*t10*t11*w2*w3-Z1*r2*t10*t11*w1*w3-X1*r1*t6*t10*t11*w3-X1*r1*t7*t10*t11*w3+X1*r3*t7*t10*t11*w1-Y1*r2*t5*t10*t11*w3-Y1*r2*t7*t10*t11*w3+Y1*r3*t7*t10*t11*w2+Z1*r1*t7*t10*t11*w1+Z1*r2*t7*t10*t11*w2-Z1*r3*t5*t10*t11*w3-Z1*r3*t6*t10*t11*w3-X1*r3*t10*t11*w2*w3+X1*r2*t10*t11*w1*w2*w3+Y1*r1*t10*t11*w1*w2*w3)-t26*t65*t93*w3*2.0-t14*t93*t101*(t18*(Z1*(t46-t113+t114+t115-t13*t14*w2)-Y1*t198+X1*(t49+t51+t52+t118-t7*t10*t25))*2.0+t23*(X1*(-t97+t112+t116+t117-t13*t14*w1)+Y1*(-t46+t113+t114+t115-t13*t14*w2)-Z1*t195)*2.0+t15*(t204+Z1*(t97-t112+t116+t117-t13*t14*w1)-X1*(t201+t202-t13*t14*w3*2.0))*2.0)*(1.0/2.0);
    jacs[0*6+3] = r1*t65-t14*t93*t101*t208*(1.0/2.0);
    jacs[0*6+4] = r2*t65-t14*t93*t101*t212*(1.0/2.0);
    jacs[0*6+5] = r3*t65-t14*t93*t101*t216*(1.0/2.0);
    jacs[1*6+0] = t14*t65*(X1*s1*w1*2.0+X1*s2*w2+X1*s3*w3+Y1*s1*w2+Z1*s1*w3+s1*t1*w1*2.0+s2*t2*w1*2.0+s3*t3*w1*2.0+Y1*s3*t5*t12+Y1*s3*t9*t10-Z1*s2*t5*t12-Z1*s2*t9*t10-X1*s2*t12*w2-X1*s3*t12*w3-Y1*s1*t12*w2+Y1*s2*t12*w1*2.0-Z1*s1*t12*w3+Z1*s3*t12*w1*2.0+Y1*s3*t5*t10*t11-Z1*s2*t5*t10*t11+X1*s2*t12*w1*w3-X1*s3*t12*w1*w2-Y1*s1*t12*w1*w3+Z1*s1*t12*w1*w2+X1*s2*t10*t11*w1*w3-X1*s3*t10*t11*w1*w2-Y1*s1*t10*t11*w1*w3+Z1*s1*t10*t11*w1*w2-X1*s1*t6*t10*t11*w1-X1*s1*t7*t10*t11*w1+X1*s2*t5*t10*t11*w2+X1*s3*t5*t10*t11*w3+Y1*s1*t5*t10*t11*w2-Y1*s2*t5*t10*t11*w1-Y1*s2*t7*t10*t11*w1+Z1*s1*t5*t10*t11*w3-Z1*s3*t5*t10*t11*w1-Z1*s3*t6*t10*t11*w1+Y1*s3*t10*t11*w1*w2*w3+Z1*s2*t10*t11*w1*w2*w3)-t14*t101*t167*(t130+t15*(Y1*(t46+t47+t48-t113-t138)+Z1*(t35+t36+t37-t94-t139)-X1*t121)*2.0+t18*(t135+t137-Y1*(-t131+t132+t133))*2.0)*(1.0/2.0)-t26*t65*t167*w1*2.0;
    jacs[1*6+1] = t14*t65*(X1*s2*w1+Y1*s1*w1+Y1*s2*w2*2.0+Y1*s3*w3+Z1*s2*w3+s1*t1*w2*2.0+s2*t2*w2*2.0+s3*t3*w2*2.0-X1*s3*t6*t12-X1*s3*t9*t10+Z1*s1*t6*t12+Z1*s1*t9*t10+X1*s1*t12*w2*2.0-X1*s2*t12*w1-Y1*s1*t12*w1-Y1*s3*t12*w3-Z1*s2*t12*w3+Z1*s3*t12*w2*2.0-X1*s3*t6*t10*t11+Z1*s1*t6*t10*t11+X1*s2*t12*w2*w3-Y1*s1*t12*w2*w3+Y1*s3*t12*w1*w2-Z1*s2*t12*w1*w2+X1*s2*t10*t11*w2*w3-Y1*s1*t10*t11*w2*w3+Y1*s3*t10*t11*w1*w2-Z1*s2*t10*t11*w1*w2-X1*s1*t6*t10*t11*w2+X1*s2*t6*t10*t11*w1-X1*s1*t7*t10*t11*w2+Y1*s1*t6*t10*t11*w1-Y1*s2*t5*t10*t11*w2-Y1*s2*t7*t10*t11*w2+Y1*s3*t6*t10*t11*w3-Z1*s3*t5*t10*t11*w2+Z1*s2*t6*t10*t11*w3-Z1*s3*t6*t10*t11*w2+X1*s3*t10*t11*w1*w2*w3+Z1*s1*t10*t11*w1*w2*w3)-t26*t65*t167*w2*2.0-t14*t101*t167*(t18*(X1*(t97+t98+t99-t112-t192)+Z1*(-t35+t94+t95+t96-t139)-Y1*t170)*2.0+t15*(t180+t182-X1*(-t176+t177+t178))*2.0+t23*(t175+Y1*(t35-t94+t95+t96-t139)-Z1*t173)*2.0)*(1.0/2.0);
    jacs[1*6+2] = t14*t65*(X1*s3*w1+Y1*s3*w2+Z1*s1*w1+Z1*s2*w2+Z1*s3*w3*2.0+s1*t1*w3*2.0+s2*t2*w3*2.0+s3*t3*w3*2.0+X1*s2*t7*t12+X1*s2*t9*t10-Y1*s1*t7*t12-Y1*s1*t9*t10+X1*s1*t12*w3*2.0-X1*s3*t12*w1+Y1*s2*t12*w3*2.0-Y1*s3*t12*w2-Z1*s1*t12*w1-Z1*s2*t12*w2+X1*s2*t7*t10*t11-Y1*s1*t7*t10*t11-X1*s3*t12*w2*w3+Y1*s3*t12*w1*w3+Z1*s1*t12*w2*w3-Z1*s2*t12*w1*w3-X1*s3*t10*t11*w2*w3+Y1*s3*t10*t11*w1*w3+Z1*s1*t10*t11*w2*w3-Z1*s2*t10*t11*w1*w3-X1*s1*t6*t10*t11*w3-X1*s1*t7*t10*t11*w3+X1*s3*t7*t10*t11*w1-Y1*s2*t5*t10*t11*w3-Y1*s2*t7*t10*t11*w3+Y1*s3*t7*t10*t11*w2+Z1*s1*t7*t10*t11*w1+Z1*s2*t7*t10*t11*w2-Z1*s3*t5*t10*t11*w3-Z1*s3*t6*t10*t11*w3+X1*s2*t10*t11*w1*w2*w3+Y1*s1*t10*t11*w1*w2*w3)-t26*t65*t167*w3*2.0-t14*t101*t167*(t18*(Z1*(t46-t113+t114+t115-t138)-Y1*t198+X1*(t49+t51+t52+t118-t199))*2.0+t23*(X1*(-t97+t112+t116+t117-t192)+Y1*(-t46+t113+t114+t115-t138)-Z1*t195)*2.0+t15*(t204+Z1*(t97-t112+t116+t117-t192)-X1*(-t200+t201+t202))*2.0)*(1.0/2.0);
    jacs[1*6+3] = s1*t65-t14*t101*t167*t208*(1.0/2.0);
    jacs[1*6+4] = s2*t65-t14*t101*t167*t212*(1.0/2.0);
    jacs[1*6+5] = s3*t65-t14*t101*t167*t216*(1.0/2.0);
}

/* ============================ MLPnP context ============================ */

typedef struct {
    int N;
    const ngd_vec3 *pws;   /* world 3D points (full set) */
    double *bearings;      /* N*3: (x/z,y/z,1) per point */
    double *p2d;           /* N*2 pixel (u,v) doubles */
    double fu, fv, uc, vc;
} mlpnp_ctx;

/* nullspace accessor: ns[(i*3+row)*2+col] */
#define NS(ns,i,row,col) ((ns)[((i)*3+(row))*2+(col)])

/* mlpnp_residuals_and_jacs (MLPnPsolver.cpp:760-806).
 * pts/nullspaces indexed by 0..n-1 (already subset). r[2n], Jac[2n*6]. */
static void residuals_and_jacs(const double *x, const double *pts, const double *ns,
                               int n, double *r, double *Jac)
{
    double w[3] = {x[0], x[1], x[2]};
    double T[3] = {x[3], x[4], x[5]};
    double R[9];
    rodrigues2rot(w, R);
    int ii = 0;
    for (int i = 0; i < n; ++i) {
        double px = pts[i*3+0], py = pts[i*3+1], pz = pts[i*3+2];
        double cx = R[0]*px+R[1]*py+R[2]*pz + T[0];
        double cy = R[3]*px+R[4]*py+R[5]*pz + T[1];
        double cz = R[6]*px+R[7]*py+R[8]*pz + T[2];
        double nrm = sqrt(cx*cx+cy*cy+cz*cz);
        if (nrm < 1e-12) nrm = 1e-12;
        cx /= nrm; cy /= nrm; cz /= nrm;
        double nr0 = NS(ns,i,0,0), nr1 = NS(ns,i,1,0), nr2 = NS(ns,i,2,0);
        double ns0 = NS(ns,i,0,1), ns1 = NS(ns,i,1,1), ns2 = NS(ns,i,2,1);
        r[ii]   = nr0*cx + nr1*cy + nr2*cz;
        r[ii+1] = ns0*cx + ns1*cy + ns2*cz;
        if (Jac) {
            double jacs[12];
            double nr[3] = {nr0,nr1,nr2}, nsv[3] = {ns0,ns1,ns2};
            mlpnp_jacs(pts+i*3, nr, nsv, w, T, jacs);
            for (int k = 0; k < 6; ++k) {
                Jac[ii*(6)+k]     = jacs[0*6+k];
                Jac[(ii+1)*6+k]   = jacs[1*6+k];
            }
        }
        ii += 2;
    }
}

/* mlpnp_gn (MLPnPsolver.cpp:694-758). x[6] = [w;t], refined in place. */
static void mlpnp_gn(double *x, const double *pts, const double *ns, int n)
{
    const int maxIt = 5;
    const double epsP = 1e-5;
    int it_cnt = 0, stop = 0;
    double *r   = (double*)malloc((size_t)n*2*sizeof(double));
    double *Jac = (double*)malloc((size_t)n*2*6*sizeof(double));
    double A[36], g[6], dx[6];
    if (!r || !Jac) { free(r); free(Jac); return; }
    while (it_cnt < maxIt && !stop) {
        residuals_and_jacs(x, pts, ns, n, r, Jac);
        /* A = Jac^T * Jac (6×6), g = Jac^T * r (P=I, cov path dropped) */
        for (int i = 0; i < 6; ++i) {
            for (int j = 0; j < 6; ++j) {
                double s = 0.0;
                for (int k = 0; k < 2*n; ++k) s += Jac[k*6+i]*Jac[k*6+j];
                A[i*6+j] = s;
            }
            double s = 0.0;
            for (int k = 0; k < 2*n; ++k) s += Jac[k*6+i]*r[k];
            g[i] = s;
        }
        if (solve_linear_d(A, g, dx, 6) != 0) break;
        /* divergence guard: max|dx|>5 || min|dx|>1 */
        double amax = fabs(dx[0]), amin = fabs(dx[0]);
        for (int k = 1; k < 6; ++k) {
            double a = fabs(dx[k]);
            if (a > amax) amax = a;
            if (a < amin) amin = a;
        }
        if (amax > 5.0 || amin > 1.0) break;
        /* dl = Jac*dx; convergence if max|dl| < epsP */
        double dlmax = 0.0;
        for (int k = 0; k < 2*n; ++k) {
            double dl = 0.0;
            for (int j = 0; j < 6; ++j) dl += Jac[k*6+j]*dx[j];
            if (fabs(dl) > dlmax) dlmax = fabs(dl);
        }
        for (int k = 0; k < 6; ++k) x[k] -= dx[k];
        if (dlmax < epsP) stop = 1;
        ++it_cnt;
    }
    free(r); free(Jac);
}

/* inverse of a rigid transform [[R,t],[0,1]] = [[R^T, -R^T t],[0,1]] */
static void rigid_inverse(const double R[9], const double t[3], double Ri[9], double ti[3])
{
    /* Ri = R^T */
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            Ri[i*3+j] = R[j*3+i];
    /* ti = -Ri * t */
    ti[0] = -(Ri[0]*t[0]+Ri[1]*t[1]+Ri[2]*t[2]);
    ti[1] = -(Ri[3]*t[0]+Ri[4]*t[1]+Ri[5]*t[2]);
    ti[2] = -(Ri[6]*t[0]+Ri[7]*t[1]+Ri[8]*t[2]);
}

static double mat3_det(const double R[9])
{
    return R[0]*(R[4]*R[8]-R[5]*R[7]) - R[1]*(R[3]*R[8]-R[5]*R[6]) + R[2]*(R[3]*R[7]-R[4]*R[6]);
}

/* computePose (MLPnPsolver.cpp:356-658). indices select the subset (size n>5).
 * Out R[9] (row-major), t[3]. Returns 1 on success. */
static int compute_pose(const mlpnp_ctx *e, const int *indices, int n,
                        double Rout[9], double tout[3])
{
    if (n <= 5) return 0;

    /* per-point nullspace (3×2) + points3 (rotated) + points3v (original) */
    double *ns = (double*)malloc((size_t)n*3*2*sizeof(double));
    double *p3 = (double*)malloc((size_t)n*3*sizeof(double));   /* points3 (rotated) */
    double *p3v = (double*)malloc((size_t)n*3*sizeof(double)); /* points3v (original) */
    if (!ns || !p3 || !p3v) { free(ns); free(p3); free(p3v); return 0; }

    for (int i = 0; i < n; ++i) {
        int idx = indices[i];
        double fx = e->bearings[idx*3+0], fy = e->bearings[idx*3+1], fz = e->bearings[idx*3+2];
        p3v[i*3+0] = p3[i*3+0] = e->pws[idx].x;
        p3v[i*3+1] = p3[i*3+1] = e->pws[idx].y;
        p3v[i*3+2] = p3[i*3+2] = e->pws[idx].z;
        /* nullspace of bearing f: eigendecomp of f f^T (3×3 symmetric).
         * eigenvalues ascending [0,0,|f|²]; two smallest eigenvectors = nullspace. */
        double fft[9] = {
            fx*fx, fx*fy, fx*fz,
            fy*fx, fy*fy, fy*fz,
            fz*fx, fz*fy, fz*fz
        };
        double V[9], w[3];
        jacobi_sym(fft, 3, V, w);
        /* V[:,0], V[:,1] are the nullspace columns */
        NS(ns,i,0,0) = V[0*3+0]; NS(ns,i,1,0) = V[1*3+0]; NS(ns,i,2,0) = V[2*3+0];
        NS(ns,i,0,1) = V[0*3+1]; NS(ns,i,1,1) = V[1*3+1]; NS(ns,i,2,1) = V[2*3+1];
    }

    /* ---- planar test: planarTest = points3 * points3^T (3×3) ---- */
    double pt[9] = {0,0,0,0,0,0,0,0,0};
    for (int i = 0; i < n; ++i) {
        double x=p3[i*3+0], y=p3[i*3+1], z=p3[i*3+2];
        pt[0]+=x*x; pt[1]+=x*y; pt[2]+=x*z;
        pt[3]+=y*x; pt[4]+=y*y; pt[5]+=y*z;
        pt[6]+=z*x; pt[7]+=z*y; pt[8]+=z*z;
    }
    double ptV[9], ptw[3];
    jacobi_sym(pt, 3, ptV, ptw);
    double maxw = fabs(ptw[0]);
    if (fabs(ptw[1]) > maxw) maxw = fabs(ptw[1]);
    if (fabs(ptw[2]) > maxw) maxw = fabs(ptw[2]);
    int rank = 0;
    for (int k = 0; k < 3; ++k) if (fabs(ptw[k]) > 1e-12 * maxw) rank++;
    int planar = (rank == 2);

    double eigenRot[9] = {1,0,0,0,1,0,0,0,1};   /* eigenRot = V^T (transposed eigenvectors) */
    if (planar) {
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                eigenRot[i*3+j] = ptV[j*3+i];   /* V^T */
        /* points3.col(i) = eigenRot * points3.col(i) */
        for (int i = 0; i < n; ++i) {
            double x=p3[i*3+0], y=p3[i*3+1], z=p3[i*3+2];
            p3[i*3+0] = eigenRot[0]*x+eigenRot[1]*y+eigenRot[2]*z;
            p3[i*3+1] = eigenRot[3]*x+eigenRot[4]*y+eigenRot[5]*z;
            p3[i*3+2] = eigenRot[6]*x+eigenRot[7]*y+eigenRot[8]*z;
        }
    }

    /* ---- design matrix A (2n × colsA) ---- */
    int colsA = planar ? 9 : 12;
    int rowsA = 2 * n;
    double *A = (double*)calloc((size_t)rowsA * colsA, sizeof(double));
    if (!A) { free(ns); free(p3); free(p3v); return 0; }

    if (planar) {
        for (int i = 0; i < n; ++i) {
            double Y=p3[i*3+1], Z=p3[i*3+2];
            double n00=NS(ns,i,0,0), n01=NS(ns,i,0,1);
            double n10=NS(ns,i,1,0), n11=NS(ns,i,1,1);
            double n20=NS(ns,i,2,0), n21=NS(ns,i,2,1);
            A[(2*i)*colsA+0]=n00*Y; A[(2*i+1)*colsA+0]=n01*Y;
            A[(2*i)*colsA+1]=n00*Z; A[(2*i+1)*colsA+1]=n01*Z;
            A[(2*i)*colsA+2]=n10*Y; A[(2*i+1)*colsA+2]=n11*Y;
            A[(2*i)*colsA+3]=n10*Z; A[(2*i+1)*colsA+3]=n11*Z;
            A[(2*i)*colsA+4]=n20*Y; A[(2*i+1)*colsA+4]=n21*Y;
            A[(2*i)*colsA+5]=n20*Z; A[(2*i+1)*colsA+5]=n21*Z;
            A[(2*i)*colsA+6]=n00;   A[(2*i+1)*colsA+6]=n01;
            A[(2*i)*colsA+7]=n10;   A[(2*i+1)*colsA+7]=n11;
            A[(2*i)*colsA+8]=n20;   A[(2*i+1)*colsA+8]=n21;
        }
    } else {
        for (int i = 0; i < n; ++i) {
            double X=p3[i*3+0], Y=p3[i*3+1], Z=p3[i*3+2];
            double n00=NS(ns,i,0,0), n01=NS(ns,i,0,1);
            double n10=NS(ns,i,1,0), n11=NS(ns,i,1,1);
            double n20=NS(ns,i,2,0), n21=NS(ns,i,2,1);
            A[(2*i)*colsA+0]=n00*X;  A[(2*i+1)*colsA+0]=n01*X;
            A[(2*i)*colsA+1]=n00*Y;  A[(2*i+1)*colsA+1]=n01*Y;
            A[(2*i)*colsA+2]=n00*Z;  A[(2*i+1)*colsA+2]=n01*Z;
            A[(2*i)*colsA+3]=n10*X;  A[(2*i+1)*colsA+3]=n11*X;
            A[(2*i)*colsA+4]=n10*Y;  A[(2*i+1)*colsA+4]=n11*Y;
            A[(2*i)*colsA+5]=n10*Z;  A[(2*i+1)*colsA+5]=n11*Z;
            A[(2*i)*colsA+6]=n20*X;  A[(2*i+1)*colsA+6]=n21*X;
            A[(2*i)*colsA+7]=n20*Y;  A[(2*i+1)*colsA+7]=n21*Y;
            A[(2*i)*colsA+8]=n20*Z;  A[(2*i+1)*colsA+8]=n21*Z;
            A[(2*i)*colsA+9]=n00;    A[(2*i+1)*colsA+9]=n01;
            A[(2*i)*colsA+10]=n10;   A[(2*i+1)*colsA+10]=n11;
            A[(2*i)*colsA+11]=n20;   A[(2*i+1)*colsA+11]=n21;
        }
    }

    /* ---- least squares: smallest eigenvector of AtPA ---- */
    double *AtPA = (double*)calloc((size_t)colsA*colsA, sizeof(double));
    double *AtPAV = (double*)malloc((size_t)colsA*colsA*sizeof(double));
    double *atw = (double*)malloc((size_t)colsA*sizeof(double));
    double *result1 = (double*)malloc((size_t)colsA*sizeof(double));
    if (!AtPA || !AtPAV || !atw || !result1) {
        free(ns); free(p3); free(p3v); free(A); free(AtPA); free(AtPAV); free(atw); free(result1);
        return 0;
    }
    for (int i = 0; i < colsA; ++i)
        for (int j = 0; j < colsA; ++j) {
            double s = 0.0;
            for (int r = 0; r < rowsA; ++r) s += A[r*colsA+i]*A[r*colsA+j];
            AtPA[i*colsA+j] = s;
        }
    jacobi_sym(AtPA, colsA, AtPAV, atw);
    for (int k = 0; k < colsA; ++k) result1[k] = AtPAV[k*colsA+0];   /* smallest eigenvector (col 0) */

    free(A); free(AtPA); free(AtPAV); free(atw);

    /* ---- R/t recovery + sign disambiguation ---- */
    double R_out[9], t_out[3];
    if (planar) {
        double tmp[9] = {
            0, result1[0], result1[1],
            0, result1[2], result1[3],
            0, result1[4], result1[5]
        };
        /* tmp.col(0) = tmp.col(1) × tmp.col(2) */
        double c1x=tmp[1], c1y=tmp[4], c1z=tmp[7];     /* col1 */
        double c2x=tmp[2], c2y=tmp[5], c2z=tmp[8];     /* col2 */
        tmp[0] = c1y*c2z - c1z*c2y;
        tmp[3] = c1z*c2x - c1x*c2z;
        tmp[6] = c1x*c2y - c1y*c2x;
        /* transposeInPlace */
        double tr[9] = {tmp[0],tmp[3],tmp[6], tmp[1],tmp[4],tmp[7], tmp[2],tmp[5],tmp[8]};
        double nc1 = sqrt(c1x*c1x+c1y*c1y+c1z*c1z);
        double nc2 = sqrt(c2x*c2x+c2y*c2y+c2z*c2z);
        double scale = 1.0 / sqrt(fabs(nc1*nc2));
        double U[9], S[3], V[9];
        svd3(tr, U, S, V);
        double Rout1[9];
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) {
                double s = 0.0;
                for (int k = 0; k < 3; ++k) s += U[i*3+k]*V[j*3+k];   /* U V^T */
                Rout1[i*3+j] = s;
            }
        if (mat3_det(Rout1) < 0) for (int k=0;k<9;++k) Rout1[k]*=-1.0;
        /* Rout1 = eigenRot^T * Rout1   (eigenRot^T = V) */
        {
            double tmpR[9];
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j) {
                    double s = 0.0;
                    for (int k = 0; k < 3; ++k) s += eigenRot[k*3+i]*Rout1[k*3+j]; /* (eigenRot^T)*Rout1 */
                    tmpR[i*3+j] = s;
                }
            for (int k=0;k<9;++k) Rout1[k]=tmpR[k];
        }
        double t[3] = {scale*result1[6], scale*result1[7], scale*result1[8]};
        /* Rout1.transposeInPlace(); Rout1 *= -1 */
        {
            double tmpR[9] = {Rout1[0],Rout1[3],Rout1[6], Rout1[1],Rout1[4],Rout1[7], Rout1[2],Rout1[5],Rout1[8]};
            for (int k=0;k<9;++k) Rout1[k] = -tmpR[k];
        }
        if (mat3_det(Rout1) < 0) { Rout1[2]*=-1; Rout1[5]*=-1; Rout1[8]*=-1; }   /* col2 *= -1 */
        /* 4 combos: R1=Rout1, R2=[-c0,-c1,c2] */
        double R1[9], R2[9];
        for (int k=0;k<9;++k) R1[k]=Rout1[k];
        R2[0]=-Rout1[0]; R2[3]=-Rout1[3]; R2[6]=-Rout1[6];   /* col0 negated */
        R2[1]=-Rout1[1]; R2[4]=-Rout1[4]; R2[7]=-Rout1[7];   /* col1 negated */
        R2[2]=Rout1[2];  R2[5]=Rout1[5];  R2[8]=Rout1[8];    /* col2 kept */
        double Rs[4][9]; double ts[4][3];
        for (int k=0;k<9;++k){ Rs[0][k]=R1[k]; Rs[1][k]=R1[k]; Rs[2][k]=R2[k]; Rs[3][k]=R2[k]; }
        for (int k=0;k<3;++k){ ts[0][k]=t[k]; ts[1][k]=-t[k]; ts[2][k]=t[k]; ts[3][k]=-t[k]; }
        double normVal[4] = {0,0,0,0};
        int np = n < 6 ? n : 6;
        for (int s = 0; s < 4; ++s) {
            for (int p = 0; p < np; ++p) {
                double px=p3v[p*3+0], py=p3v[p*3+1], pz=p3v[p*3+2];
                double cx=Rs[s][0]*px+Rs[s][1]*py+Rs[s][2]*pz+ts[s][0];
                double cy=Rs[s][3]*px+Rs[s][4]*py+Rs[s][5]*pz+ts[s][1];
                double cz=Rs[s][6]*px+Rs[s][7]*py+Rs[s][8]*pz+ts[s][2];
                double nrm=sqrt(cx*cx+cy*cy+cz*cz);
                if (nrm<1e-12) nrm=1e-12;
                cx/=nrm; cy/=nrm; cz/=nrm;
                int idx = indices[p];
                double dot = cx*e->bearings[idx*3+0] + cy*e->bearings[idx*3+1] + cz*e->bearings[idx*3+2];
                normVal[s] += (1.0 - dot);
            }
        }
        int best = 0;
        for (int s = 1; s < 4; ++s) if (normVal[s] < normVal[best]) best = s;
        for (int k=0;k<9;++k) R_out[k]=Rs[best][k];
        for (int k=0;k<3;++k) t_out[k]=ts[best][k];
    } else {
        /* tmp (3×3) from result1[0..8] (column-major fill) */
        double tmp[9] = {
            result1[0], result1[1], result1[2],
            result1[3], result1[4], result1[5],
            result1[6], result1[7], result1[8]
        };
        /* tmp.col(k).norm() : col0=(tmp[0],tmp[1],tmp[2]), col1=(tmp[3],tmp[4],tmp[5]), col2=(tmp[6],tmp[7],tmp[8]) */
        double nc0 = sqrt(tmp[0]*tmp[0]+tmp[1]*tmp[1]+tmp[2]*tmp[2]);
        double nc1 = sqrt(tmp[3]*tmp[3]+tmp[4]*tmp[4]+tmp[5]*tmp[5]);
        double nc2 = sqrt(tmp[6]*tmp[6]+tmp[7]*tmp[7]+tmp[8]*tmp[8]);
        double scale = 1.0 / pow(fabs(nc0*nc1*nc2), 1.0/3.0);
        double U[9], S[3], V[9];
        svd3(tmp, U, S, V);
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) {
                double s = 0.0;
                for (int k = 0; k < 3; ++k) s += U[i*3+k]*V[j*3+k];   /* U V^T */
                R_out[i*3+j] = s;
            }
        if (mat3_det(R_out) < 0) for (int k=0;k<9;++k) R_out[k]*=-1.0;
        /* tout = Rout * (scale * t_vec) */
        double tv[3] = {scale*result1[9], scale*result1[10], scale*result1[11]};
        t_out[0] = R_out[0]*tv[0]+R_out[1]*tv[1]+R_out[2]*tv[2];
        t_out[1] = R_out[3]*tv[0]+R_out[4]*tv[1]+R_out[5]*tv[2];
        t_out[2] = R_out[6]*tv[0]+R_out[7]*tv[1]+R_out[8]*tv[2];
        /* sign disambiguation: 2 options via Ts[s].inverse() */
        double err[2] = {0,0};
        double Ts_R[2][9], Ts_t[2][3];
        int np = n < 6 ? n : 6;
        for (int s = 0; s < 2; ++s) {
            double tt[3] = { (s==0)? t_out[0]:-t_out[0], (s==0)?t_out[1]:-t_out[1], (s==0)?t_out[2]:-t_out[2] };
            double Ri[9], ti[3];
            rigid_inverse(R_out, tt, Ri, ti);
            for (int k=0;k<9;++k) Ts_R[s][k]=Ri[k];
            for (int k=0;k<3;++k) Ts_t[s][k]=ti[k];
            for (int p = 0; p < np; ++p) {
                double px=p3v[p*3+0], py=p3v[p*3+1], pz=p3v[p*3+2];
                double cx=Ri[0]*px+Ri[1]*py+Ri[2]*pz+ti[0];
                double cy=Ri[3]*px+Ri[4]*py+Ri[5]*pz+ti[1];
                double cz=Ri[6]*px+Ri[7]*py+Ri[8]*pz+ti[2];
                double nrm=sqrt(cx*cx+cy*cy+cz*cz);
                if (nrm<1e-12) nrm=1e-12;
                cx/=nrm; cy/=nrm; cz/=nrm;
                int idx = indices[p];
                double dot = cx*e->bearings[idx*3+0] + cy*e->bearings[idx*3+1] + cz*e->bearings[idx*3+2];
                err[s] += (1.0 - dot);
            }
        }
        int win = (err[0] < err[1]) ? 0 : 1;
        for (int k=0;k<3;++k) t_out[k]=Ts_t[win][k];
        for (int k=0;k<9;++k) R_out[k]=Ts_R[0][k];   /* Rout = Ts[0].R (always) */
    }

    free(ns); free(p3); free(p3v); free(result1);

    /* ---- Gauss-Newton ---- */
    double x[6];
    double omega[3];
    rot2rodrigues(R_out, omega);
    x[0]=omega[0]; x[1]=omega[1]; x[2]=omega[2];
    x[3]=t_out[0]; x[4]=t_out[1]; x[5]=t_out[2];

    /* build per-subset pts + ns arrays (0..n-1 indexed) for GN */
    double *gpts = (double*)malloc((size_t)n*3*sizeof(double));
    double *gns  = (double*)malloc((size_t)n*3*2*sizeof(double));
    if (gpts && gns) {
        for (int i = 0; i < n; ++i) {
            gpts[i*3+0]=e->pws[indices[i]].x;
            gpts[i*3+1]=e->pws[indices[i]].y;
            gpts[i*3+2]=e->pws[indices[i]].z;
        }
        /* recompute nullspaces for the GN subset (ns was freed above) */
        for (int i = 0; i < n; ++i) {
            int idx = indices[i];
            double fx = e->bearings[idx*3+0], fy = e->bearings[idx*3+1], fz = e->bearings[idx*3+2];
            double fft[9] = {fx*fx,fx*fy,fx*fz, fy*fx,fy*fy,fy*fz, fz*fx,fz*fy,fz*fz};
            double V[9], w[3];
            jacobi_sym(fft, 3, V, w);
            NS(gns,i,0,0)=V[0]; NS(gns,i,1,0)=V[3]; NS(gns,i,2,0)=V[6];
            NS(gns,i,0,1)=V[1]; NS(gns,i,1,1)=V[4]; NS(gns,i,2,1)=V[7];
        }
        mlpnp_gn(x, gpts, gns, n);
    }
    free(gpts); free(gns);

    rodrigues2rot(&x[0], R_out);
    t_out[0]=x[3]; t_out[1]=x[4]; t_out[2]=x[5];

    for (int k=0;k<9;++k) Rout[k]=R_out[k];
    for (int k=0;k<3;++k) tout[k]=t_out[k];
    return 1;
}

#undef NS

/* classify inliers (CheckInliers, MLPnPsolver.cpp:262-293). sigma2 in pixel²,
 * gate = sigma2*th2. Returns count; fills inliers[N]. */
static int check_inliers(const mlpnp_ctx *e, const double R[9], const double t[3],
                         const float *sigma2, float th2, int *inliers)
{
    int cnt = 0;
    for (int i = 0; i < e->N; ++i) {
        double X=e->pws[i].x, Y=e->pws[i].y, Z=e->pws[i].z;
        double Xc=R[0]*X+R[1]*Y+R[2]*Z+t[0];
        double Yc=R[3]*X+R[4]*Y+R[5]*Z+t[1];
        double Zc=R[6]*X+R[7]*Y+R[8]*Z+t[2];
        inliers[i] = 0;
        if (Zc <= 0) continue;
        double u = e->fu*Xc/Zc + e->uc;
        double v = e->fv*Yc/Zc + e->vc;
        double du = u - e->p2d[2*i], dv = v - e->p2d[2*i+1];
        double err2 = du*du + dv*dv;
        if (err2 < (double)sigma2[i] * th2) { inliers[i] = 1; ++cnt; }
    }
    return cnt;
}

/* build ctx bearings from pixels (pinhole unproject / z). */
static int ctx_init(mlpnp_ctx *e, const ngd_vec3 *p3d, const float *p2d, int N,
                    const ngd_cam_ctx *cam)
{
    e->N = N; e->pws = p3d;
    e->bearings = (double*)malloc((size_t)N*3*sizeof(double));
    e->p2d = (double*)malloc((size_t)N*2*sizeof(double));
    if (!e->bearings || !e->p2d) { free(e->bearings); free(e->p2d); return 0; }
    e->fu = cam->cam.fx; e->fv = cam->cam.fy; e->uc = cam->cam.cx; e->vc = cam->cam.cy;
    for (int i = 0; i < N; ++i) {
        double u = p2d[2*i], v = p2d[2*i+1];
        e->p2d[2*i] = u; e->p2d[2*i+1] = v;
        /* unproject: ray = ((u-cx)/fx, (v-cy)/fy, 1) */
        e->bearings[i*3+0] = (u - e->uc) / e->fu;
        e->bearings[i*3+1] = (v - e->vc) / e->fv;
        e->bearings[i*3+2] = 1.0;
    }
    return 1;
}

static void ctx_free(mlpnp_ctx *e) { free(e->bearings); free(e->p2d); }

/* ============================ public API ============================ */

int ngd_mlpnp_solve(const ngd_vec3 *p3d, const float *p2d, int N,
                    const ngd_cam_ctx *cam, ngd_se3 *outTcw)
{
    if (N < 6 || !p3d || !p2d || !cam || !outTcw) return 0;
    mlpnp_ctx e;
    if (!ctx_init(&e, p3d, p2d, N, cam)) return 0;
    int *idx = (int*)malloc((size_t)N*sizeof(int));
    if (!idx) { ctx_free(&e); return 0; }
    for (int i = 0; i < N; ++i) idx[i] = i;
    double R[9], t[3];
    int ok = compute_pose(&e, idx, N, R, t);
    free(idx); ctx_free(&e);
    if (!ok) return 0;
    ngd_mat3 Rm; for (int i=0;i<9;++i) Rm.m[i] = (float)R[i];
    outTcw->q = ngd_quat_from_matrix(Rm);
    outTcw->t = ngd_v3((float)t[0],(float)t[1],(float)t[2]);
    return 1;
}

int ngd_mlpnp_solve_ransac(const ngd_vec3 *p3d, const float *p2d, const float *sigma2,
                           int N, const ngd_cam_ctx *cam,
                           const ngd_mlpnp_ransac_params *p,
                           ngd_se3 *outTcw, int *outInliers, int *outNInliers)
{
    ngd_mlpnp_ransac_params def;
    if (!p) { def = ngd_mlpnp_ransac_defaults(); p = &def; }
    if (N < p->minSet || !p3d || !p2d || !sigma2 || !cam || !outTcw) return 0;

    mlpnp_ctx e;
    if (!ctx_init(&e, p3d, p2d, N, cam)) return 0;

    /* derived params (SetRansacParameters, MLPnPsolver.cpp:225-260) */
    int minInl = (int)(N * p->epsilon);
    if (minInl < p->minInliers) minInl = p->minInliers;
    if (minInl < p->minSet)     minInl = p->minSet;
    float eps = p->epsilon;
    if (eps < (float)minInl / N) eps = (float)minInl / N;
    int nIters;
    if (minInl == N) nIters = 1;
    else {
        double denom = log(1.0 - pow((double)eps, 3));   /* original hardcodes exponent 3 */
        nIters = (denom < 0) ? (int)ceil(log(1.0 - p->prob) / denom) : p->maxIterations;
    }
    if (nIters > p->maxIterations) nIters = p->maxIterations;
    if (nIters < 1) nIters = 1;

    int *sample = (int*)malloc((size_t)p->minSet*sizeof(int));
    int *inliers = (int*)malloc((size_t)N*sizeof(int));
    int *bestInliers = (int*)malloc((size_t)N*sizeof(int));
    int *avail = (int*)malloc((size_t)N*sizeof(int));
    if (!sample || !inliers || !bestInliers || !avail) {
        free(sample); free(inliers); free(bestInliers); free(avail); ctx_free(&e);
        return 0;
    }

    srand(12345);
    int bestCount = 0;
    double bestR[9] = {1,0,0,0,1,0,0,0,1}, bestT[3] = {0,0,0};

    for (int it = 0; it < nIters; ++it) {
        for (int i = 0; i < N; ++i) avail[i] = i;
        int availN = N;
        for (int k = 0; k < p->minSet; ++k) {
            int r = (availN > 0) ? (rand() % availN) : 0;
            sample[k] = avail[r];
            avail[r] = avail[availN-1];
            availN--;
        }
        double R[9], t[3];
        if (!compute_pose(&e, sample, p->minSet, R, t)) continue;
        int cnt = check_inliers(&e, R, t, sigma2, p->th2, inliers);
        if (cnt > bestCount) {
            bestCount = cnt;
            for (int k=0;k<9;++k) bestR[k]=R[k];
            for (int k=0;k<3;++k) bestT[k]=t[k];
            for (int i=0;i<N;++i) bestInliers[i]=inliers[i];
        }
    }

    int ok = 0;
    if (bestCount >= minInl) {
        /* Refine: computePose on all best inliers, re-check inliers */
        int *refidx = (int*)malloc((size_t)bestCount*sizeof(int));
        if (refidx) {
            int m = 0;
            for (int i = 0; i < N; ++i) if (bestInliers[i]) refidx[m++] = i;
            double R[9], t[3];
            if (m > 5 && compute_pose(&e, refidx, m, R, t)) {
                int cnt = check_inliers(&e, R, t, sigma2, p->th2, bestInliers);
                if (cnt > minInl) {
                    for (int k=0;k<9;++k) bestR[k]=R[k];
                    for (int k=0;k<3;++k) bestT[k]=t[k];
                    bestCount = cnt;
                    ok = 1;
                }
            }
            free(refidx);
        }
    }

    if (outInliers) for (int i=0;i<N;++i) outInliers[i]=bestInliers[i];
    if (outNInliers) *outNInliers = bestCount;

    if (ok || bestCount >= minInl) {
        ngd_mat3 Rm; for (int i=0;i<9;++i) Rm.m[i]=(float)bestR[i];
        outTcw->q = ngd_quat_from_matrix(Rm);
        outTcw->t = ngd_v3((float)bestT[0],(float)bestT[1],(float)bestT[2]);
        ok = 1;
    }

    free(sample); free(inliers); free(bestInliers); free(avail); ctx_free(&e);
    return ok ? 1 : 0;
}
