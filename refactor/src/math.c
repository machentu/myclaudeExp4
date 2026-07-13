/* ngd/math.c — fixed-size linear algebra (pure C).
 * See include/ngd/math.h for conventions. */
#include "ngd/math.h"

#include <math.h>

/* =============================== Vec3 =============================== */
ngd_vec3 ngd_v3_add(ngd_vec3 a, ngd_vec3 b)  { return ngd_v3(a.x+b.x, a.y+b.y, a.z+b.z); }
ngd_vec3 ngd_v3_sub(ngd_vec3 a, ngd_vec3 b)  { return ngd_v3(a.x-b.x, a.y-b.y, a.z-b.z); }
ngd_vec3 ngd_v3_scale(ngd_vec3 a, float s)   { return ngd_v3(a.x*s, a.y*s, a.z*s); }

float ngd_v3_dot(ngd_vec3 a, ngd_vec3 b) {
    return a.x*b.x + a.y*b.y + a.z*b.z;
}
ngd_vec3 ngd_v3_cross(ngd_vec3 a, ngd_vec3 b) {
    return ngd_v3(a.y*b.z - a.z*b.y,
                  a.z*b.x - a.x*b.z,
                  a.x*b.y - a.y*b.x);
}
float ngd_v3_norm(ngd_vec3 a) { return sqrtf(a.x*a.x + a.y*a.y + a.z*a.z); }
ngd_vec3 ngd_v3_normalize(ngd_vec3 a) {
    float n = ngd_v3_norm(a);
    if (n < 1e-20f) return ngd_v3(0.f,0.f,0.f);
    return ngd_v3_scale(a, 1.0f/n);
}

/* =============================== Mat3 =============================== */
ngd_mat3 ngd_m3_identity(void) {
    ngd_mat3 r = {{1,0,0, 0,1,0, 0,0,1}};
    return r;
}
ngd_mat3 ngd_m3_from_rows(ngd_vec3 r0, ngd_vec3 r1, ngd_vec3 r2) {
    ngd_mat3 r = {{ r0.x,r0.y,r0.z,
                    r1.x,r1.y,r1.z,
                    r2.x,r2.y,r2.z }};
    return r;
}
ngd_mat3 ngd_m3_add(ngd_mat3 a, ngd_mat3 b) {
    ngd_mat3 r;
    for (int i = 0; i < 9; ++i) r.m[i] = a.m[i] + b.m[i];
    return r;
}
ngd_mat3 ngd_m3_add_scaled(ngd_mat3 a, ngd_mat3 b, float s) {
    ngd_mat3 r;
    for (int i = 0; i < 9; ++i) r.m[i] = a.m[i] + s * b.m[i];
    return r;
}
ngd_mat3 ngd_m3_multiply(ngd_mat3 a, ngd_mat3 b) {
    ngd_mat3 r;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            float s = 0.f;
            for (int k = 0; k < 3; ++k) s += a.m[i*3+k] * b.m[k*3+j];
            r.m[i*3+j] = s;
        }
    return r;
}
ngd_vec3 ngd_m3_transform(ngd_mat3 a, ngd_vec3 v) {
    return ngd_v3(a.m[0]*v.x + a.m[1]*v.y + a.m[2]*v.z,
                  a.m[3]*v.x + a.m[4]*v.y + a.m[5]*v.z,
                  a.m[6]*v.x + a.m[7]*v.y + a.m[8]*v.z);
}
ngd_mat3 ngd_m3_transpose(ngd_mat3 a) {
    ngd_mat3 r;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            r.m[i*3+j] = a.m[j*3+i];
    return r;
}
float ngd_m3_det(ngd_mat3 a) {
    return  a.m[0]*(a.m[4]*a.m[8]-a.m[5]*a.m[7])
          - a.m[1]*(a.m[3]*a.m[8]-a.m[5]*a.m[6])
          + a.m[2]*(a.m[3]*a.m[7]-a.m[4]*a.m[6]);
}
ngd_mat3 ngd_m3_inverse(ngd_mat3 a) {
    ngd_mat3 r;
    float det = ngd_m3_det(a);
    if (fabsf(det) < 1e-20f) { return ngd_m3_identity(); } /* singular guard */
    float inv_det = 1.0f / det;
    r.m[0] =  (a.m[4]*a.m[8]-a.m[5]*a.m[7]) * inv_det;
    r.m[1] = -(a.m[1]*a.m[8]-a.m[2]*a.m[7]) * inv_det;
    r.m[2] =  (a.m[1]*a.m[5]-a.m[2]*a.m[4]) * inv_det;
    r.m[3] = -(a.m[3]*a.m[8]-a.m[5]*a.m[6]) * inv_det;
    r.m[4] =  (a.m[0]*a.m[8]-a.m[2]*a.m[6]) * inv_det;
    r.m[5] = -(a.m[0]*a.m[5]-a.m[2]*a.m[3]) * inv_det;
    r.m[6] =  (a.m[3]*a.m[7]-a.m[4]*a.m[6]) * inv_det;
    r.m[7] = -(a.m[0]*a.m[7]-a.m[1]*a.m[6]) * inv_det;
    r.m[8] =  (a.m[0]*a.m[4]-a.m[1]*a.m[3]) * inv_det;
    return r;
}

/* =============================== Quat =============================== */
ngd_quat ngd_quat_identity(void) { ngd_quat q = {1.f,0.f,0.f,0.f}; return q; }

ngd_quat ngd_quat_from_axis_angle(ngd_vec3 axis, float angle_rad) {
    float h = 0.5f * angle_rad;
    float s = sinf(h);
    ngd_vec3 a = ngd_v3_normalize(axis);
    ngd_quat q = { cosf(h), a.x*s, a.y*s, a.z*s };
    return q;
}
ngd_quat ngd_quat_normalize(ngd_quat q) {
    float n = sqrtf(q.w*q.w + q.x*q.x + q.y*q.y + q.z*q.z);
    if (n < 1e-20f) return ngd_quat_identity();
    ngd_quat r = { q.w/n, q.x/n, q.y/n, q.z/n };
    return r;
}
/* Hamilton product. a*b applied as: rotate by b, then by a. */
ngd_quat ngd_quat_multiply(ngd_quat a, ngd_quat b) {
    ngd_quat r;
    r.w = a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z;
    r.x = a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y;
    r.y = a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x;
    r.z = a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w;
    return r;
}
ngd_quat ngd_quat_conjugate(ngd_quat q) { ngd_quat r = {q.w,-q.x,-q.y,-q.z}; return r; }

ngd_mat3 ngd_quat_to_matrix(ngd_quat q) {
    /* Normalise first to absorb accumulated drift (matches SO3 normalisation). */
    q = ngd_quat_normalize(q);
    float xx=q.x*q.x, yy=q.y*q.y, zz=q.z*q.z;
    float xy=q.x*q.y, xz=q.x*q.z, yz=q.y*q.z;
    float wx=q.w*q.x, wy=q.w*q.y, wz=q.w*q.z;
    ngd_mat3 R;
    R.m[0]=1-2*(yy+zz); R.m[1]=2*(xy-wz);   R.m[2]=2*(xz+wy);
    R.m[3]=2*(xy+wz);   R.m[4]=1-2*(xx+zz); R.m[5]=2*(yz-wx);
    R.m[6]=2*(xz-wy);   R.m[7]=2*(yz+wx);   R.m[8]=1-2*(xx+yy);
    return R;
}

/* Shepperd's method — picks the branch with the largest denominator to stay
 * numerically stable, identical in spirit to Eigen's quaternion-from-matrix. */
ngd_quat ngd_quat_from_matrix(ngd_mat3 R) {
    const float *m = R.m;
    float tr = m[0]+m[4]+m[8];
    ngd_quat q;
    if (tr > 0.f) {
        float s = sqrtf(tr + 1.0f) * 2.0f;       /* s = 4w */
        q.w = 0.25f * s;
        q.x = (m[7]-m[5]) / s;
        q.y = (m[2]-m[6]) / s;
        q.z = (m[3]-m[1]) / s;
    } else if (m[0] > m[4] && m[0] > m[8]) {
        float s = sqrtf(1.0f + m[0] - m[4] - m[8]) * 2.0f;  /* s = 4x */
        q.w = (m[7]-m[5]) / s;
        q.x = 0.25f * s;
        q.y = (m[1]+m[3]) / s;
        q.z = (m[2]+m[6]) / s;
    } else if (m[4] > m[8]) {
        float s = sqrtf(1.0f + m[4] - m[0] - m[8]) * 2.0f;  /* s = 4y */
        q.w = (m[2]-m[6]) / s;
        q.x = (m[1]+m[3]) / s;
        q.y = 0.25f * s;
        q.z = (m[5]+m[7]) / s;
    } else {
        float s = sqrtf(1.0f + m[8] - m[0] - m[4]) * 2.0f;  /* s = 4z */
        q.w = (m[3]-m[1]) / s;
        q.x = (m[2]+m[6]) / s;
        q.y = (m[5]+m[7]) / s;
        q.z = 0.25f * s;
    }
    return ngd_quat_normalize(q);
}

/* ===================== small dense linear solve ===================== */
int ngd_solve_linear(float *A, const float *b, float *x, int n) {
    /* In-place Gaussian elimination with partial pivoting. A is n*n row-major;
     * we destroy it. Augment with b into [A|b] conceptually (b mutated too). */
    /* work on a local augmented copy so caller's b stays const */
    for (int i = 0; i < n; ++i) x[i] = b[i];

    for (int col = 0; col < n; ++col) {
        /* pivot */
        int piv = col;
        float maxv = fabsf(A[col*n+col]);
        for (int r = col+1; r < n; ++r) {
            float v = fabsf(A[r*n+col]);
            if (v > maxv) { maxv = v; piv = r; }
        }
        if (maxv < 1e-18f) return -1;  /* singular */
        if (piv != col) {
            for (int k = 0; k < n; ++k) {
                float t = A[col*n+k]; A[col*n+k] = A[piv*n+k]; A[piv*n+k] = t;
            }
            float t = x[col]; x[col] = x[piv]; x[piv] = t;
        }
        /* eliminate below */
        float diag = A[col*n+col];
        for (int r = col+1; r < n; ++r) {
            float f = A[r*n+col] / diag;
            if (f == 0.f) continue;
            for (int k = col; k < n; ++k) A[r*n+k] -= f * A[col*n+k];
            x[r] -= f * x[col];
        }
    }
    /* back-substitute */
    for (int i = n-1; i >= 0; --i) {
        float s = x[i];
        for (int k = i+1; k < n; ++k) s -= A[i*n+k] * x[k];
        x[i] = s / A[i*n+i];
    }
    return 0;
}
