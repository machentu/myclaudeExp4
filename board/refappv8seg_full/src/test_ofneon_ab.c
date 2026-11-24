/* test_ofneon_ab.c — NEON LK 对拍 (优化②验证, 2026-10-12)
 *
 * 两个 ARM 二进制: 宏关 (标量, = v34 行为) / 宏开 (NEON)。同确定性输入,
 * 逐点打印跟踪结果 (out x/y, %.6f)。PC 侧 diff 数值对比:
 * 预期差异 = 浮点累加顺序的 ulp 级 (~1e-4 px 内), 而非功能差异。
 *
 * 图像: 平滑正弦叠加 + LCG 散斑 (纹理充足, 双线性友好);
 * 点集: 内部 60 点 (走 NEON) + 贴边 6 点 (走标量回退, 两版都应一致走标量)。
 * 已知位移: (2.35, -1.65) px — 亚像素, 双线性合成 curr。
 */
#include "ngd/optflow.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define W 320
#define H 240

static uint32_t lcg_state = 0x12345678u;
static uint32_t lcg(void) { lcg_state = lcg_state * 1664525u + 1013904223u; return lcg_state >> 8; }

static float base_at(float x, float y)
{
    float v = 96.0f;
    v += 40.0f * sinf(x * 0.101f + 1.7f) * cosf(y * 0.083f);
    v += 22.0f * sinf(x * 0.31f) * sinf(y * 0.27f + 0.6f);
    v += 10.0f * cosf(x * 0.71f - y * 0.53f);
    return v;
}

static void gen_image(uint8_t *img, float dx, float dy, uint32_t speckle_seed)
{
    lcg_state = speckle_seed;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            float v = base_at((float)x - dx, (float)y - dy);
            v += (float)(int)(lcg() % 7) - 3.0f;   /* 散斑 (双线性平滑的微噪) */
            if (v < 0) v = 0; if (v > 255) v = 255;
            img[(size_t)y * W + x] = (uint8_t)(v + 0.5f);
        }
}

int main(int argc, char **argv)
{
    static uint8_t prev_img[W * H], curr_img[W * H];
    gen_image(prev_img, 0.0f, 0.0f, 0xACE1u);
    gen_image(curr_img, 2.35f, -1.65f, 0xACE1u);   /* 同种子散斑随基图平移 */

    enum { N = 66 };
    float pts[2 * N];
    int n = 0;
    /* 60 内部点 (8px 边距) */
    for (int i = 0; i < 60; ++i) {
        pts[2*n]   = 8.0f + (float)((i * 53) % (W - 16));
        pts[2*n+1] = 8.0f + (float)((i * 97) % (H - 16));
        ++n;
    }
    /* 6 贴边点 (走标量回退路径) */
    float edge[6][2] = {{2.5f, 120.0f}, {W-3.5f, 120.0f}, {160.0f, 2.5f},
                        {160.0f, H-3.5f}, {3.5f, 3.5f}, {W-4.5f, H-4.5f}};
    for (int i = 0; i < 6; ++i) { pts[2*n] = edge[i][0]; pts[2*n+1] = edge[i][1]; ++n; }

    float out[2 * N];
    uint8_t st[N];
    ngd_lk_params p = NGD_LK_DEFAULT_TRACKING;   /* {21,3,30,0.01,0,0} 与 SLAM 同参 */
    p.min_eig = 0.0f; p.fb_thresh = 0.0f;
    int reps = (argc > 1) ? atoi(argv[1]) : 1;
    int nuse = N;
    if (argc > 2) { nuse = atoi(argv[2]); if (nuse < 1) nuse = 1; if (nuse > N) nuse = N; }
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    int rc = 0;
    for (int r = 0; r < reps; ++r)
        rc = ngd_optflow_pyr_lk(prev_img, curr_img, W, H, W, pts, nuse, out, st, &p);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double ms = (t1.tv_sec - t0.tv_sec) * 1e3 + (t1.tv_nsec - t0.tv_nsec) / 1e6;
    printf("BENCH reps=%d total_ms=%.1f per_call_ms=%.2f rc=%d N=%d\n",
           reps, ms, ms / reps, rc, nuse);
    float sdx = 0.0f, sdy = 0.0f;
    int nok = 0;
    for (int i = 0; i < N; ++i) {
        float dx = st[i] ? out[2*i] - pts[2*i] : 0.0f;
        float dy = st[i] ? out[2*i+1] - pts[2*i+1] : 0.0f;
        printf("p%02d st=%d in=(%.2f,%.2f) out=(%.6f,%.6f) d=(%.6f,%.6f)\n",
               i, (int)st[i], pts[2*i], pts[2*i+1], out[2*i], out[2*i+1], dx, dy);
        if (st[i]) { sdx += dx; sdy += dy; ++nok; }
    }
    printf("SUM d=(%.6f,%.6f) nok=%d\n", sdx, sdy, nok);
    return 0;
}
