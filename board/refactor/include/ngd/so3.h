#ifndef NGD_SO3_H
#define NGD_SO3_H

/*
 * ngd/so3.h — SO(3) Lie-group primitives for the pure-C core.
 *
 * Replaces Sophus::SO3f. The only SO3 primitives the original NGD-SLAM uses
 * are hat/exp (ImuTypes.cc) and the SE3 exp internals. log/hat mirror the
 * standard formulas. See NGD_SLAM_TECHNICAL_DOC.md §6.2/§6.9.
 */

#include "ngd/math.h"

#ifdef __cplusplus
extern "C" {
#endif

/* skew(v) = -skew(v)^T  (3x3) */
ngd_mat3 ngd_so3_hat(ngd_vec3 v);

/* Rodrigues: so3_exp(omega) -> unit quaternion.
 * omega encodes axis*angle (||omega|| = angle). */
ngd_quat ngd_so3_exp(ngd_vec3 omega);

/* so3_log(R) -> omega (axis*angle), with ||omega|| in [0, pi]. */
ngd_vec3 ngd_so3_log(ngd_mat3 R);

#ifdef __cplusplus
}
#endif
#endif /* NGD_SO3_H */
