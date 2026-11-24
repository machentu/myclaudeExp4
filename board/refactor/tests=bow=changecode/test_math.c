/* test_math.c — unit tests for ngd/math.h. Plain asserts, no framework. */
#include "ngd/math.h"

#include <stdio.h>
#include <math.h>

static int fails = 0;
#define CHECK(cond, msg) do { if(!(cond)){ printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); fails++; } } while(0)
#define APPROX(a,b,eps) (fabsf((a)-(b)) <= (eps))

int main(void)
{
    /* ---- Vec3 ---- */
    ngd_vec3 a = ngd_v3(1,2,3), b = ngd_v3(4,5,6);
    ngd_vec3 c = ngd_v3_cross(a, b);          /* (-3, 6, -3) */
    CHECK(APPROX(c.x,-3,1e-5f) && APPROX(c.y,6,1e-5f) && APPROX(c.z,-3,1e-5f), "cross product");
    CHECK(APPROX(ngd_v3_dot(a,b), 32, 1e-5f), "dot product");
    CHECK(APPROX(ngd_v3_norm(a), sqrtf(14), 1e-5f), "norm");

    /* ---- Mat3 inverse ---- */
    ngd_mat3 M = ngd_m3_from_rows(ngd_v3(1,2,3), ngd_v3(0,1,4), ngd_v3(5,6,0));
    CHECK(APPROX(ngd_m3_det(M), 1.0f, 1e-5f), "det");
    ngd_mat3 Minv = ngd_m3_inverse(M);
    ngd_mat3 I = ngd_m3_multiply(M, Minv);
    CHECK(APPROX(I.m[0],1,1e-4f) && APPROX(I.m[4],1,1e-4f) && APPROX(I.m[8],1,1e-4f)
          && APPROX(I.m[1],0,1e-4f) && APPROX(I.m[3],0,1e-4f), "M * M^-1 = I");

    /* ---- Quat: 90deg about z maps (1,0,0)->(0,1,0) ---- */
    ngd_quat qz = ngd_quat_from_axis_angle(ngd_v3(0,0,1), 3.14159265358979f/2);
    ngd_mat3 Rz = ngd_quat_to_matrix(qz);
    ngd_vec3 ex = ngd_m3_transform(Rz, ngd_v3(1,0,0));
    CHECK(APPROX(ex.x,0,1e-4f) && APPROX(ex.y,1,1e-4f) && APPROX(ex.z,0,1e-4f), "quat z-rotation");

    /* ---- quat <-> matrix roundtrip ---- */
    ngd_quat q0 = ngd_quat_from_axis_angle(ngd_v3_normalize(ngd_v3(1,2,3)), 0.7f);
    ngd_mat3 R0 = ngd_quat_to_matrix(q0);
    ngd_quat q1 = ngd_quat_from_matrix(R0);
    ngd_mat3 R1 = ngd_quat_to_matrix(q1);
    for (int i=0;i<9;i++) CHECK(APPROX(R0.m[i], R1.m[i], 1e-4f), "quat<->matrix roundtrip");

    /* ---- solve_linear: 3x3 known system ---- */
    float A[9] = { 3,2,1, 1,4,1, 2,1,5 };   /* rows */
    float bb[3] = { 10, 12, 15 };
    float x[3];
    CHECK(ngd_solve_linear(A, bb, x, 3) == 0, "solve_linear ok");
    /* verify A*x = b (rebuild A since solve destroys it) */
    float A2[9] = { 3,2,1, 1,4,1, 2,1,5 };
    CHECK(APPROX(A2[0]*x[0]+A2[1]*x[1]+A2[2]*x[2], 10, 1e-3f) &&
          APPROX(A2[3]*x[0]+A2[4]*x[1]+A2[5]*x[2], 12, 1e-3f) &&
          APPROX(A2[6]*x[0]+A2[7]*x[1]+A2[8]*x[2], 15, 1e-3f), "solve_linear result");

    if (fails == 0) { printf("test_math: PASS\n"); return 0; }
    printf("test_math: %d FAILURES\n", fails);
    return 1;
}
