#ifndef NGD_MATH_H
#define NGD_MATH_H

/*
 * ngd/math.h — fixed-size linear algebra for the NGD-SLAM pure-C core.
 *
 * Conventions (chosen to match the original g2o/Eigen/Sophus code paths):
 *   - Mat3 is ROW-major, indexed m[row*3+col].  (Eigen Matrix3f default.)
 *   - Quat is Hamilton product, w-first: {w,x,y,z}.  (Sophus/Eigen Quaternion.)
 *   - Everything is plain float (matches ORB-SLAM3's Sophus::SE3<float>).
 *
 * All structs are value types; use the *_zero / *_identity ctors to initialise.
 * No dynamic allocation, no hidden globals, no cv::Mat::at-style unguarded
 * indexing (see WINDOWS_PORT_FIXES.md / NGD_TECHNICAL_DOC.md §7.3).
 *
 * Corresponds to NGD_SLAM_TECHNICAL_DOC.md §6.9 (math library layout).
 */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ Vec3 */
typedef struct { float x, y, z; } ngd_vec3;

static inline ngd_vec3 ngd_v3(float x, float y, float z) {
    ngd_vec3 v = {x, y, z}; return v;
}
ngd_vec3 ngd_v3_add(ngd_vec3 a, ngd_vec3 b);
ngd_vec3 ngd_v3_sub(ngd_vec3 a, ngd_vec3 b);
ngd_vec3 ngd_v3_scale(ngd_vec3 a, float s);
float    ngd_v3_dot(ngd_vec3 a, ngd_vec3 b);
ngd_vec3 ngd_v3_cross(ngd_vec3 a, ngd_vec3 b);
float    ngd_v3_norm(ngd_vec3 a);
ngd_vec3 ngd_v3_normalize(ngd_vec3 a);   /* returns zero if norm == 0 */

/* ------------------------------------------------------------------ Mat3 */
/* Row-major 3x3. */
typedef struct { float m[9]; } ngd_mat3;

ngd_mat3 ngd_m3_identity(void);
ngd_mat3 ngd_m3_from_rows(ngd_vec3 r0, ngd_vec3 r1, ngd_vec3 r2);
ngd_mat3 ngd_m3_add(ngd_mat3 a, ngd_mat3 b);
ngd_mat3 ngd_m3_add_scaled(ngd_mat3 a, ngd_mat3 b, float s);   /* a + s*b */
ngd_mat3 ngd_m3_multiply(ngd_mat3 a, ngd_mat3 b);          /* a*b */
ngd_vec3 ngd_m3_transform(ngd_mat3 a, ngd_vec3 v);          /* a*v */
ngd_mat3 ngd_m3_transpose(ngd_mat3 a);
ngd_mat3 ngd_m3_inverse(ngd_mat3 a);                        /* closed-form; zero if singular */
float    ngd_m3_det(ngd_mat3 a);
/* 3x6 * 6xN helpers used by the pose Jacobian assembly */
/* (declared here so callers do not reinvent them) */

/* ----------------------------------------------------------------- Quat */
/* Hamilton, w-first: {w,x,y,z}. */
typedef struct { float w, x, y, z; } ngd_quat;

ngd_quat ngd_quat_identity(void);
ngd_quat ngd_quat_from_axis_angle(ngd_vec3 axis, float angle_rad);
ngd_quat ngd_quat_normalize(ngd_quat q);
ngd_quat ngd_quat_multiply(ngd_quat a, ngd_quat b);        /* Hamilton product */
ngd_quat ngd_quat_conjugate(ngd_quat q);                    /* == inverse for unit quat */
ngd_mat3 ngd_quat_to_matrix(ngd_quat q);                    /* rotation matrix R(q) */
ngd_quat ngd_quat_from_matrix(ngd_mat3 R);                  /* nearest-rotation Shepperd */

/* ---------------------------------------------------- small linear solve */
/*
 * Solve A*x = b for a small dense symmetric-ish system (n <= 16).
 * A is n*n row-major, b is n, x is n (out). Returns 0 on success, -1 if the
 * leading matrix is numerically singular. Uses Gauss elimination with
 * partial pivoting — adequate for the 6x6 LM normal equations (H+lambda*D)
 * which are SPD-ish. No dynamic allocation.
 */
int ngd_solve_linear(float *A, const float *b, float *x, int n);

#ifdef __cplusplus
}
#endif
#endif /* NGD_MATH_H */
