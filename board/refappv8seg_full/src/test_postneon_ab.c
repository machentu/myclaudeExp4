/* test_postneon_ab.c — 优化③ NEON 后处理对拍 + 基准 (2026-10-12)
 *
 * 一个二进制同时调用 image_proc.c 原版与 ngd_opt_postneon.c 新版:
 *   1. rs1+bin: [ngd_resize_nearest_f32; 二值化循环] vs 融合 NEON
 *   2. rs2    : ngd_resize_nearest_u8 vs 索引表 NEON
 *   3. cnt    : ngd_count_nonzero_u8 vs vcnt popcount
 * 判定: memcmp / 整数相等 = PASS (逐位一致, 非 ulp 级)。
 * 输入含阈值边界值 (0.5f, nextafter(0.5,0), nextafter(0.5,1)) 与随机 u8。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <time.h>

void ngd_resize_nearest_u8(const uint8_t *src, int sw, int sh, int sstride,
                           uint8_t *dst, int dw, int dh);
void ngd_resize_nearest_f32(const float *src, int sw, int sh, int sstride,
                            float *dst, int dw, int dh);
int  ngd_count_nonzero_u8(const uint8_t *src, int n);

void ngd_post_resize_f32_bin_neon(const float *src, int sw, int sh, int sstride,
                                  uint8_t *dst, int dw, int dh);
void ngd_post_resize_u8_neon(const uint8_t *src, int sw, int sh, int sstride,
                             uint8_t *dst, int dw, int dh);
int  ngd_post_count_nonzero_neon(const uint8_t *src, int n);

#define PW 120
#define PH 64
#define NETW 480
#define NETH 256
#define CW 341
#define CH 256
#define FW 640
#define FH 480

static double now_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1e3 + t.tv_nsec / 1e6;
}

static uint32_t lcg_state = 0xC0FFEEu;
static uint32_t lcg(void) { lcg_state = lcg_state * 1664525u + 1013904223u; return lcg_state >> 8; }

int main(void)
{
    static float  pmask[PW * PH];
    static float  fref[NETW * NETH];
    static uint8_t uref[NETW * NETH], unew[NETW * NETH];
    static uint8_t crop[CW * CH], rref[FH * FW], rnew[FH * FW];

    /* 合成 pmask: 斑块 + 0.35..0.65 连续带 + 精确阈值边界值 */
    for (int y = 0; y < PH; ++y)
        for (int x = 0; x < PW; ++x) {
            float v = ((x / 9 + y / 11) % 5 == 0) ? 0.92f : 0.07f;
            v += 0.3f * (float)(lcg() % 100) / 100.0f;      /* 0.30..0.60 抖动跨阈值 */
            pmask[(size_t)y * PW + x] = v;
        }
    pmask[0] = 0.5f;                                        /* (int)(0.5*255+0.5)=128>127 */
    pmask[1] = nextafterf(0.5f, 0.0f);
    pmask[2] = nextafterf(0.5f, 1.0f);
    pmask[3] = 0.49999997f;

    for (int i = 0; i < CW * CH; ++i) crop[i] = (uint8_t)(lcg() & 0xFF);

    /* ---- 1. rs1+bin ---- */
    ngd_resize_nearest_f32(pmask, PW, PH, PW, fref, NETW, NETH);
    { int np = NETW * NETH;
      for (int i = 0; i < np; ++i) {
          int v = (int)(fref[i] * 255.0f + 0.5f);
          uref[i] = (v > 127) ? (uint8_t)255 : (uint8_t)0;
      } }
    ngd_post_resize_f32_bin_neon(pmask, PW, PH, PW, unew, NETW, NETH);
    int ok1 = memcmp(uref, unew, (size_t)NETW * NETH) == 0;

    /* ---- 2. rs2 ---- */
    ngd_resize_nearest_u8(crop, CW, CH, CW, rref, FW, FH);
    ngd_post_resize_u8_neon(crop, CW, CH, CW, rnew, FW, FH);
    int ok2 = memcmp(rref, rnew, (size_t)FW * FH) == 0;

    /* ---- 3. cnt ---- */
    int cref = ngd_count_nonzero_u8(rref, FW * FH);
    int cnew = ngd_post_count_nonzero_neon(rref, FW * FH);
    int ok3 = cref == cnew;

    printf("rs1+bin %s | rs2 %s | cnt %s (%d==%d)\n",
           ok1 ? "PASS" : "FAIL", ok2 ? "PASS" : "FAIL",
           ok3 ? "PASS" : "FAIL", cref, cnew);

    /* ---- 基准 ---- */
    const int reps = 50;
    double t0, tA = 0, tB = 0, tC = 0, tD = 0;
    for (int r = 0; r < reps; ++r) {
        t0 = now_ms();
        ngd_resize_nearest_f32(pmask, PW, PH, PW, fref, NETW, NETH);
        { int np = NETW * NETH;
          for (int i = 0; i < np; ++i) {
              int v = (int)(fref[i] * 255.0f + 0.5f);
              uref[i] = (v > 127) ? (uint8_t)255 : (uint8_t)0;
          } }
        tA += now_ms() - t0;
        t0 = now_ms();
        ngd_post_resize_f32_bin_neon(pmask, PW, PH, PW, unew, NETW, NETH);
        tB += now_ms() - t0;
        t0 = now_ms();
        ngd_resize_nearest_u8(crop, CW, CH, CW, rref, FW, FH);
        tC += now_ms() - t0;
        t0 = now_ms();
        ngd_post_resize_u8_neon(crop, CW, CH, CW, rnew, FW, FH);
        tD += now_ms() - t0;
    }
    printf("rs1+bin: orig %.3f -> neon %.3f ms | rs2: orig %.3f -> neon %.3f ms (reps=%d)\n",
           tA / reps, tB / reps, tC / reps, tD / reps, reps);
    return (ok1 && ok2 && ok3) ? 0 : 1;
}
