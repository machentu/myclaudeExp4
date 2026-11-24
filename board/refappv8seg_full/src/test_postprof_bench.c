/* test_postprof_bench.c — postproc 分项基准 (优化③勘察, 2026-10-12)
 *
 * v34 全量: postproc 137.6ms/帧 (72.7% of detect)。本基准按 detect() 第 4.5-6
 * 步的真实尺寸分项计时, 在板上直接量化各子步骤 (无需跑全 SLAM):
 *   rs1 : ngd_resize_nearest_f32  64×120 → 480×256   (123k 采样)
 *   bin : f32→u8 二值化循环                              (122880)
 *   crop: 行 memcpy 341×256                              (~87KB)
 *   rs2 : ngd_resize_nearest_u8  341×256 → 640×480      (307k 采样)
 *   dil : ngd_mask_dilate 640×480 half=3 (7×7 窗)       (~15% 前景)
 *   cnt : ngd_count_nonzero_u8                           (307k)
 * 对照: decode 桶 (decodePosition+decodeMask) 已知 10.6ms/帧。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

void ngd_resize_nearest_u8(const uint8_t *src, int sw, int sh, int sstride,
                           uint8_t *dst, int dw, int dh);
void ngd_resize_nearest_f32(const float *src, int sw, int sh, int sstride,
                            float *dst, int dw, int dh);
int  ngd_count_nonzero_u8(const uint8_t *src, int n);
void ngd_mask_dilate(const uint8_t *in, uint8_t *out, int w, int h, int half);

#define NETW 480
#define NETH 256
#define PW 120
#define PH 64
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

int main(void)
{
    static float  pmask[PW * PH];
    static float  ffull[NETW * NETH];
    static uint8_t ubuf[NETW * NETH];
    static uint8_t crop[CW * CH];
    static uint8_t full[FH * FW];
    static uint8_t dil[FH * FW];

    /* 合成: 15% 前景斑块 (mask 值 0..1 float) */
    for (int y = 0; y < PH; ++y)
        for (int x = 0; x < PW; ++x) {
            int bx = (x / 9) % 7, by = (y / 11) % 5;
            pmask[(size_t)y * PW + x] = ((bx * by) % 5 == 0) ? 0.9f : 0.05f;
        }
    memset(full, 0, sizeof full);

    const int reps = 50;
    double t_rs1 = 0, t_bin = 0, t_crop = 0, t_rs2 = 0, t_dil = 0, t_cnt = 0;
    for (int r = 0; r < reps; ++r) {
        double a = now_ms();
        ngd_resize_nearest_f32(pmask, PW, PH, PW, ffull, NETW, NETH);
        double b = now_ms();
        { int np = NETW * NETH;
          for (int i = 0; i < np; ++i) {
              int v = (int)(ffull[i] * 255.0f + 0.5f);
              ubuf[i] = (v > 127) ? (uint8_t)255 : (uint8_t)0;
          } }
        double c = now_ms();
        for (int y = 0; y < CH; ++y)
            memcpy(crop + (size_t)y * CW, ubuf + (size_t)y * NETW, CW);
        double d = now_ms();
        ngd_resize_nearest_u8(crop, CW, CH, CW, full, FW, FH);
        double e = now_ms();
        ngd_mask_dilate(full, dil, FW, FH, 3);
        double f = now_ms();
        volatile int nz = ngd_count_nonzero_u8(dil, FW * FH);
        (void)nz;
        double g = now_ms();
        t_rs1 += b - a; t_bin += c - b; t_crop += d - c;
        t_rs2 += e - d; t_dil += f - e;  t_cnt += g - f;
    }
    printf("postproc breakdown (reps=%d, per-call ms):\n", reps);
    printf("  rs1  resize_nearest_f32  64x120->480x256 : %7.3f\n", t_rs1 / reps);
    printf("  bin  binarize f32->u8                    : %7.3f\n", t_bin / reps);
    printf("  crop row memcpy 341x256                  : %7.3f\n", t_crop / reps);
    printf("  rs2  resize_nearest_u8  341x256->640x480 : %7.3f\n", t_rs2 / reps);
    printf("  dil  mask_dilate 640x480 half=3 (7x7)    : %7.3f\n", t_dil / reps);
    printf("  cnt  count_nonzero                       : %7.3f\n", t_cnt / reps);
    printf("  SUM                                      : %7.3f\n",
           (t_rs1 + t_bin + t_crop + t_rs2 + t_dil + t_cnt) / reps);
    return 0;
}
