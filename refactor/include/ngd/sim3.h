#ifndef NGD_SIM3_H
#define NGD_SIM3_H

/*
 * ngd/sim3.h — Sim(3) Lie group {quaternion, translation, scale} for the pure-C core.
 *
 * Faithful to g2o::Sim3 (Thirdparty/g2o/g2o/types/sim3.h), used by the
 * LoopClosing / OptimizeSim3 / OptimizeEssentialGraph paths.  Conventions
 * verified against g2o::Sim3 EXACTLY:
 *
 *   - map(p)        = s*(R*p) + t                         (Sim3::map)
 *   - compose A*B   = { qA*qB, sA*(R_A*tB)+tA, sA*sB }    (Sim3::operator*)
 *   - inverse       = { conj(q), conj(q)*((-1/s)*t), 1/s }(Sim3::inverse)
 *   - exp(xi)       = full Sim3 exponential (Sim3::exp), xi = [omega(3) | upsilon(3) | sigma(1)]
 *                     sigma is the Lie-algebra log-scale: s = exp(sigma).
 *                     R via Rodrigues; t = W*upsilon, W = A*Omega + B*Omega^2 + C*I,
 *                     with the four sigma/theta branch cases replicated verbatim.
 *   - log(S)        = inverse of exp, returns xi = [omega(3) | upsilon(3) | sigma(1)].
 *                     upsilon = W^{-1} * t  (g2o uses LU solve; we use ngd_solve_linear).
 *
 * 7-DoF vertex update (VertexSim3Expmap oplus) is left perturbation
 * T_new = exp(xi) * T_old, same convention as SE3.
 */

#include "ngd/math.h"
#include "ngd/so3.h"
#include "ngd/se3.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct { ngd_quat q; ngd_vec3 t; float s; } ngd_sim3;

ngd_sim3 ngd_sim3_identity(void);                       /* {q=I, t=0, s=1} */
ngd_sim3 ngd_sim3_from_rt(ngd_mat3 R, ngd_vec3 t, float s);
ngd_sim3 ngd_sim3_from_matrix4(const float m[16]);      /* reads [sR | t; 0 1]? NO: reads R,t and s param separately; here s=1, R,t from 4x4 */
void    ngd_sim3_to_matrix4(ngd_sim3 S, float m[16]);   /* row-major homogeneous [sR | t; 0 1] */

ngd_sim3 ngd_sim3_multiply(ngd_sim3 a, ngd_sim3 b);     /* a*b */
ngd_sim3 ngd_sim3_inverse(ngd_sim3 S);
ngd_vec3 ngd_sim3_map(ngd_sim3 S, ngd_vec3 p);          /* s*(R*p)+t */

/* Full Sim3 exponential. xi = [omega(3), upsilon(3), sigma(1)]. */
ngd_sim3 ngd_sim3_exp(const float xi[7]);
/* Full Sim3 logarithm (inverse of exp). out xi = [omega(3), upsilon(3), sigma(1)]. */
void    ngd_sim3_log(ngd_sim3 S, float xi[7]);

/* Recover the SE3 pose corresponding to a Sim3: [R | t/s].  (OptimizeEssentialGraph
 * writes back KF poses this way.) */
ngd_se3  ngd_sim3_to_se3(ngd_sim3 S);

#ifdef __cplusplus
}
#endif
#endif /* NGD_SIM3_H */
