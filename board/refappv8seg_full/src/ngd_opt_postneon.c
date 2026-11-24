/* ngd_opt_postneon.c — 优化③: yolo 后处理 NEON 化 + 死代码 dilate 跳过 (2026-10-12)
 *
 * 背景 (perf_analysis_v34.md + 板端分项基准 probe_neon/postprof_b1):
 *   v34 全量 postproc 137.6ms/帧。板端真实尺寸分项 (合成数据):
 *     dil  resize 后 7×7 dilate   68.50ms  ← 78% 大头, 且输出 dilate_tmp_
 *                                             在 detect() 内无任何后继读取
 *                                             (死计算, 见 yolov8_seg.cpp 步骤 6)
 *     rs2  resize_nearest_u8 341×256→640×480   9.23ms
 *     rs1  resize_nearest_f32 64×120→480×256   4.27ms
 *     bin  二值化 f32→u8                        3.87ms
 *     cnt  count_nonzero                        2.05ms
 *   注: decodeMask (32 维系数×proto 点乘) 在 10.6ms decode 桶内, 不在 postproc。
 *
 * 本文件三个函数, 均与 image_proc.c 原版逐位一致 (构造性证明, 非近似):
 *   1. ngd_post_resize_f32_bin_neon: rs1+bin 融合。索引表用与原版逐句相同的
 *      float 语义 ((float)sw/(float)dw 一次除法, (int)((float)x*sx), clamp),
 *      故采样点全等; 阈值链 vmulq_n(255)+vadd(0.5)+vcvtq_s32(截断, 同 C cast)
 *      +vcgtq(127) 与标量 (int)(f*255.0f+0.5f)>127 浮点操作全等。
 *   2. ngd_post_resize_u8_neon: rs2。同一索引表 + 相邻 dst 行 src 行重复时
 *      memcpy 整行 (341×256→640×480 下 ~47% 行为纯复制)。
 *   3. ngd_post_count_nonzero_neon: vcnt 逐字节 popcount 累加, 精确计数。
 * dilate 跳过不在本文件 (在 yolov8_seg.cpp 钩子处 #if 掉, 输出位级不变)。
 *
 * 激活: build 脚本 -DNGD_POST_NEON=1 (钩子见 yolov8_seg.cpp detect() 步骤 5/6)。
 * 宏关 (默认) 时本文件不被引用, 二进制行为与 v34 完全一致。
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <arm_neon.h>

/* 与 image_proc.c::ngd_resize_nearest_* 完全相同的索引表构造 */
static int *build_xidx(int sw, int dw)
{
    int *ix = (int *)malloc(sizeof(int) * (size_t)dw);
    if (!ix) return NULL;
    const float sx = (float)sw / (float)dw;
    for (int x = 0; x < dw; ++x) {
        int sxi = (int)((float)x * sx);
        if (sxi >= sw) sxi = sw - 1;
        ix[x] = sxi;
    }
    return ix;
}

/* ---- rs1+bin 融合: nearest 采样 f32 → (int)(f*255+0.5)>127 ? 255 : 0 ----
 * 输出与 [ngd_resize_nearest_f32; 二值化循环] 串联逐字节一致。 */
void ngd_post_resize_f32_bin_neon(const float *src, int sw, int sh, int sstride,
                                  uint8_t *dst, int dw, int dh)
{
    int *ix = build_xidx(sw, dw);
    if (!ix) {                       /* 分配失败兜底: 全 0 (=f<0.5 的输出) */
        memset(dst, 0, (size_t)dw * dh);
        return;
    }
    const float sy = (float)sh / (float)dh;
    const int32x4_t v127 = vdupq_n_s32(127);
    const float32x4_t v05 = vdupq_n_f32(0.5f);

    for (int y = 0; y < dh; ++y) {
        int syi = (int)((float)y * sy);
        if (syi >= sh) syi = sh - 1;
        const float *srow = src + (size_t)syi * sstride;
        uint8_t       *drow = dst + (size_t)y * dw;

        int x = 0;
        for (; x + 15 < dw; x += 16) {          /* 16 列 = 4×4-lane */
            uint16x4_t n16[4];
            for (int q = 0; q < 4; ++q) {
                /* 4 列 gather (无向量 gather, 标量取数进 lane) */
                float32x4_t v = vdupq_n_f32(0.0f);
                v = vsetq_lane_f32(srow[ix[x + 4*q + 0]], v, 0);
                v = vsetq_lane_f32(srow[ix[x + 4*q + 1]], v, 1);
                v = vsetq_lane_f32(srow[ix[x + 4*q + 2]], v, 2);
                v = vsetq_lane_f32(srow[ix[x + 4*q + 3]], v, 3);
                /* 与标量逐句等价: 显式 mul 后 add (各一步舍入, 不用 vmla 规避
                 * ARMv7 融合语义歧义), vcvtq 截断同 C cast */
                float32x4_t t = vaddq_f32(vmulq_n_f32(v, 255.0f), v05);
                int32x4_t   vi = vcvtq_s32_f32(t);         /* (int) 截断 */
                n16[q] = vmovn_u32(vcgtq_s32(vi, v127));   /* -1/0 → 0xFFFF/0 */
            }
            /* 2×(u16x4×2 → 8 连续有效字节) → 16 字节 */
            vst1q_u8(drow + x, vcombine_u8(
                vmovn_u16(vcombine_u16(n16[0], n16[1])),
                vmovn_u16(vcombine_u16(n16[2], n16[3]))));
        }
        for (; x < dw; ++x) {                   /* 尾列: 原式标量 */
            int v = (int)(srow[ix[x]] * 255.0f + 0.5f);
            drow[x] = (v > 127) ? (uint8_t)255 : (uint8_t)0;
        }
    }
    free(ix);
}

/* ---- rs2: nearest resize u8, 索引表 + 行重复 memcpy ---- */
void ngd_post_resize_u8_neon(const uint8_t *src, int sw, int sh, int sstride,
                             uint8_t *dst, int dw, int dh)
{
    int *ix = build_xidx(sw, dw);
    if (!ix) {
        memset(dst, 0, (size_t)dw * dh);
        return;
    }
    const float sy = (float)sh / (float)dh;
    int prev_syi = -1;
    for (int y = 0; y < dh; ++y) {
        int syi = (int)((float)y * sy);
        if (syi >= sh) syi = sh - 1;
        uint8_t *drow = dst + (size_t)y * dw;
        if (syi == prev_syi && y > 0) {         /* 上采样的行重复 */
            memcpy(drow, drow - (size_t)dw, (size_t)dw);
            continue;
        }
        prev_syi = syi;
        const uint8_t *srow = src + (size_t)syi * sstride;
        for (int x = 0; x < dw; ++x)
            drow[x] = srow[ix[x]];
    }
    free(ix);
}

/* ---- count_nonzero: vtst(0xFF/0)→>>7(1/0)→逐级成对加到 u32 (精确计数) ---- */
int ngd_post_count_nonzero_neon(const uint8_t *src, int n)
{
    uint32x4_t acc = vdupq_n_u32(0);
    int i = 0;
    for (; i + 15 < n; i += 16) {
        uint8x16_t v = vld1q_u8(src + i);
        uint8x16_t one = vshrq_n_u8(vtstq_u8(v, v), 7);     /* 非零字节 → 1 */
        acc = vaddq_u32(acc, vpaddlq_u16(vpaddlq_u8(one))); /* 8→16→32 成对 */
    }
    uint32x2_t a2 = vadd_u32(vget_low_u32(acc), vget_high_u32(acc));
    unsigned c = vget_lane_u32(a2, 0) + vget_lane_u32(a2, 1);
    for (; i < n; ++i) c += (src[i] != 0);
    return (int)c;
}
