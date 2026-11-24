#ifndef NGD_SE3_H
#define NGD_SE3_H

/*
 * ngd/se3.h — SE(3) Lie group {quaternion, translation} for the pure-C core.
 *
 * Drop-in replacement for g2o::SE3Quat / Sophus::SE3<float> as used by the
 * PoseOptimization path.  Conventions match g2o::SE3Quat EXACTLY (verified
 * against Thirdparty/g2o/g2o/types/se3quat.h):
 *
 *   - map(p)        = R*q + t                  (SE3Quat::map)
 *   - compose A*B   = { tA + R_A*tB, qA*qB }   (SE3Quat::operator*)
 *   - inverse       = { conj(q), -R^T t }      (SE3Quat::inverse)
 *   - exp(xi)       = full SE3 exponential:    (SE3Quat::exp)
 *                      R = I + (sin t/t) Ω + ((1-cos t)/t²) Ω²
 *                      t = V·upsilon,  V = I + ((1-cos t)/t²) Ω + ((t-sin t)/t³) Ω²
 *                      xi = [omega(3) | upsilon(3)]
 *   - VertexSE3Expmap oplus: T_new = exp(xi) * T_old   (LEFT perturbation)
 *
 * The pose-only reprojection Jacobian used by the optimizer is the *simplified*
 * block  J = -projectJac(Xc) * SE3deriv(Xc),  where
 *   SE3deriv(Xc) = [ -skew(Xc) | I_3 ]   (3x6)
 * matching OptimizableTypes.cpp:49-63. This is g2o's own (intentional)
 * Jacobian/exp mismatch; we replicate it so the C optimizer behaves like g2o.
 */

#include "ngd/math.h"
#include "ngd/so3.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct { ngd_quat q; ngd_vec3 t; } ngd_se3;

ngd_se3 ngd_se3_identity(void);
ngd_se3 ngd_se3_from_rt(ngd_mat3 R, ngd_vec3 t);
ngd_se3 ngd_se3_from_matrix4(const float m[16]);   /* row-major homogeneous */
void    ngd_se3_to_matrix4(ngd_se3 s, float m[16]);

ngd_se3 ngd_se3_multiply(ngd_se3 a, ngd_se3 b);    /* a*b */
ngd_se3 ngd_se3_inverse(ngd_se3 s);
ngd_vec3 ngd_se3_map(ngd_se3 s, ngd_vec3 p);       /* R*p + t */

/* Full SE3 exponential. xi = [omega(3), upsilon(3)]. */
ngd_se3 ngd_se3_exp(const float xi[6]);
/* Full SE3 logarithm (inverse of exp). out xi = [omega(3), upsilon(3)]. */
void    ngd_se3_log(ngd_se3 s, float xi[6]);

/*
 * SE3deriv(Xc) -> 3x6 row-major, columns ordered [omega(3) | upsilon(3)].
 * Equals [ -skew(Xc) | I_3 ].  Used by the pose Jacobian: J = -Pjac * SE3deriv.
 * out must hold 18 floats.
 */
void ngd_se3_deriv(ngd_vec3 Xc, float out[18]);

#ifdef __cplusplus
}
#endif
#endif /* NGD_SE3_H */
