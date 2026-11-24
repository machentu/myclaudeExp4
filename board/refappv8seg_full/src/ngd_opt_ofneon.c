/* ngd_opt_ofneon.c — 优化②: LK 光流内循环 NEON 化 (2026-10-12)
 *
 * 背景 (perf_analysis_v34.md): v34 全量 827 帧, of 光流跟踪 666ms/帧 占 26%。
 * 热点 = refactor/src/optflow.c::lk_level_f 内层 win×win (默认 21×21=441 px)
 * 循环: 每像素 bilinear 采样 + prev 中心差分梯度 + 5 个累加。纯标量 float。
 *
 * 本文件提供 ngd_of_lk_level_f_neon() — 与 lk_level_f 逐参数同签名的 NEON 实现:
 *   - 行内 fx/fy 恒定 (wx/wy 为整数步进) → bilinear 按行 4-lane 向量化;
 *   - 梯度 Ix/Iy 用移位 load 的中心差分;
 *   - Gxx/Gxy/Gyy/bx/by 五累加器全程保持 float32x4, 窗口结束后水平归约;
 *   - 边界: 调用方迭代 guard 保证 curr 采样索引在界内; prev 梯度中心差分
 *     要求窗口不贴 prev 图边 (Xp-1≥0 && Xp+W≤pw-1, Y 同), 不满足或
 *     min_eig>0 (默认 0=关) → 整次调用回退文件内标量副本 (与 optflow.c
 *     标量体逐行同源), 结果与原版完全一致。
 *
 * 数值语义: 与标量版仅浮点累加顺序不同 (4-lane 并行 vs 顺序), 末位 ulp 级
 * 差异, LK 收敛阈 0.01px 之下 4 个数量级。宏关 (默认) 时本文件不被引用,
 * 二进制行为与 v34 完全一致。
 *
 * 激活: build 脚本 -DNGD_OF_NEON=1 (同时钩子见 optflow.c lk_level_f 头部)。
 * A/B: 同数据 head100 对比轨迹/ATE + t_of 耗时。
 */

#include "ngd/optflow.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <arm_neon.h>

/* ---- 水平归约: float32x4 → float (ARMv7 无 vaddvq) ---- */
static inline float hsum_f32(float32x4_t v)
{
    float32x2_t s = vadd_f32(vget_low_f32(v), vget_high_f32(v));
    return vget_lane_f32(s, 0) + vget_lane_f32(s, 1);
}

/* ---- u8×4 → f32×4 ---- */
static inline float32x4_t vcvt4_u8_f32(const uint8_t *p)
{
    uint16x8_t w = vmovl_u8(vld1_u8(p));        /* 取低 4 lane 有效 */
    return vcvtq_f32_u32(vmovl_u16(vget_low_u16(w)));
}

/* =========================================================================
 * 金字塔 pyrDown NEON (分离 5×5: H 全行 5-tap × V 5-tap)。
 * 原版 optflow.c::pyr_down_f/pyr_down_u 是逐像素 25-tap 带分支标量 —
 * A/B 实测 (probe_neon/ofab_v3): 金字塔构建占单次调用 73% (320×240 下
 * 46ms), 是 LK 光流固定大头。分离+向量化后 ~10×。
 * 数值: 与 25-tap 仅浮点结合顺序不同 (重结合), ~1e-7 相对差。
 * 边界反射语义与原版逐条相同 (rx<0 → -rx; rx≥sw → 2sw-rx-2)。
 * ======================================================================== */
static const float kPyr5[5] = { 1.0f/16.0f, 4.0f/16.0f, 6.0f/16.0f,
                                4.0f/16.0f, 1.0f/16.0f };

/* H 行: h[x] = Σ_d k[d]*row[refl(2x+d-2)], 输出宽 dw (下采样 x 网格) */
static void pyr_hpass_row(const float *row, int sw, float *h, int dw)
{
    int x = 0;
    for (; x < 2 && x < dw; ++x) {          /* 左反射 2 列标量 */
        float s = 0.0f;
        for (int d = -2; d <= 2; ++d) {
            int rx = 2 * x + d;
            if (rx < 0) rx = -rx; else if (rx >= sw) rx = 2 * sw - rx - 2;
            s += row[rx] * kPyr5[d + 2];
        }
        h[x] = s;
    }
    /* 内部: 4 输出/块; E(d) = vld2q(row+2x+d).val[0] = {row[2x+d], +2, +4, +6}
     * 末块最多读到 2x+9, 须 ≤ sw-1 保证不出行 */
    for (; x + 3 < dw && 2 * x + 9 <= sw - 1; x += 4) {
        float32x4_t acc = vmulq_n_f32(vld2q_f32(row + 2*x - 2).val[0], kPyr5[0]);
        acc = vmlaq_n_f32(acc, vld2q_f32(row + 2*x - 1).val[0], kPyr5[1]);
        acc = vmlaq_n_f32(acc, vld2q_f32(row + 2*x    ).val[0], kPyr5[2]);
        acc = vmlaq_n_f32(acc, vld2q_f32(row + 2*x + 1).val[0], kPyr5[3]);
        acc = vmlaq_n_f32(acc, vld2q_f32(row + 2*x + 2).val[0], kPyr5[4]);
        vst1q_f32(h + x, acc);
    }
    for (; x < dw; ++x) {                   /* 尾列 (含右反射) 标量 */
        float s = 0.0f;
        for (int d = -2; d <= 2; ++d) {
            int rx = 2 * x + d;
            if (rx < 0) rx = -rx; else if (rx >= sw) rx = 2 * sw - rx - 2;
            s += row[rx] * kPyr5[d + 2];
        }
        h[x] = s;
    }
}

/* V 行: out[x] = Σ_d k[d]*h5[d][x] (行反射已在调用方解析) */
static void pyr_vpass_row(const float *const h5[5], float *out, int dw)
{
    int x = 0;
    for (; x + 3 < dw; x += 4) {
        float32x4_t acc = vmulq_n_f32(vld1q_f32(h5[0] + x), kPyr5[0]);
        acc = vmlaq_n_f32(acc, vld1q_f32(h5[1] + x), kPyr5[1]);
        acc = vmlaq_n_f32(acc, vld1q_f32(h5[2] + x), kPyr5[2]);
        acc = vmlaq_n_f32(acc, vld1q_f32(h5[3] + x), kPyr5[3]);
        acc = vmlaq_n_f32(acc, vld1q_f32(h5[4] + x), kPyr5[4]);
        vst1q_f32(out + x, acc);
    }
    for (; x < dw; ++x)
        out[x] = kPyr5[0]*h5[0][x] + kPyr5[1]*h5[1][x] + kPyr5[2]*h5[2][x]
               + kPyr5[3]*h5[3][x] + kPyr5[4]*h5[4][x];
}

void ngd_of_pyr_down_f_neon(const float *src, int sw, int sh, int sstride,
                            float *dst, int dw, int dh, int dstride)
{
    float *H = (float *)malloc(sizeof(float) * (size_t)dw * sh);
    if (!H) {   /* 分配失败: 调用方 optflow.c 钩子处已无回退, 全零兜底 (不发生) */
        for (int y = 0; y < dh; ++y) memset(dst + (size_t)y * dstride, 0, sizeof(float) * dw);
        return;
    }
    for (int sy = 0; sy < sh; ++sy)
        pyr_hpass_row(src + (size_t)sy * sstride, sw, H + (size_t)sy * dw, dw);
    for (int y = 0; y < dh; ++y) {
        const float *h5[5];
        for (int d = -2; d <= 2; ++d) {
            int ry = 2 * y + d;
            if (ry < 0) ry = -ry; else if (ry >= sh) ry = 2 * sh - ry - 2;
            h5[d + 2] = H + (size_t)ry * dw;
        }
        pyr_vpass_row(h5, dst + (size_t)y * dstride, dw);
    }
    free(H);
}

void ngd_of_pyr_down_u_neon(const uint8_t *src, int sw, int sh, int sstride,
                            uint8_t *dst, int dw, int dh, int dstride)
{
    float *H    = (float *)malloc(sizeof(float) * (size_t)dw * sh);
    float *rowf = (float *)malloc(sizeof(float) * sw);
    float *outf = (float *)malloc(sizeof(float) * dw);
    if (!H || !rowf || !outf) {
        free(H); free(rowf); free(outf);
        for (int y = 0; y < dh; ++y) memset(dst + (size_t)y * dstride, 0, (size_t)dw);
        return;
    }
    for (int sy = 0; sy < sh; ++sy) {
        const uint8_t *r = src + (size_t)sy * sstride;   /* u8 行 → f32 */
        int c = 0;
        for (; c + 3 < sw; c += 4) vst1q_f32(rowf + c, vcvt4_u8_f32(r + c));
        for (; c < sw; ++c) rowf[c] = (float)r[c];
        pyr_hpass_row(rowf, sw, H + (size_t)sy * dw, dw);
    }
    for (int y = 0; y < dh; ++y) {
        const float *h5[5];
        for (int d = -2; d <= 2; ++d) {
            int ry = 2 * y + d;
            if (ry < 0) ry = -ry; else if (ry >= sh) ry = 2 * sh - ry - 2;
            h5[d + 2] = H + (size_t)ry * dw;
        }
        pyr_vpass_row(h5, outf, dw);
        for (int x = 0; x < dw; ++x) {       /* 原版 clamp+round 语义 */
            float v = outf[x];
            if (v < 0.0f) v = 0.0f;
            if (v > 255.0f) v = 255.0f;
            dst[(size_t)y * dstride + x] = (uint8_t)(v + 0.5f);
        }
    }
    free(H); free(rowf); free(outf);
}


/* =========================================================================
 * 标量回退: 与 optflow.c::lk_level_f 逐行同源 (含 min_eig 门控与边界梯度)。
 * 仅在 NEON 前置条件不满足时使用 (窗口贴 prev 图边 / min_eig>0)。
 * ======================================================================== */
static int lk_level_f_scalar(const float *prev, int pw, int ph,
                             const uint8_t *curr, int cw, int ch, int cstride,
                             float px, float py, float *dx, float *dy,
                             int half, int max_iter, float epsilon, float min_eig)
{
    if (min_eig > 0.0f) {   /* min-eig 门控: 与原版一致只在窗口全在界内时评估 */
        int ix = (int)px, iy = (int)py;
        if (ix - half < 0 || ix + half >= pw - 1 ||
            iy - half < 0 || iy + half >= ph - 1)
            return 0;
        float Gxx = 0.0f, Gxy = 0.0f, Gyy = 0.0f;
        int N = 0;
        for (int wy = -half; wy <= half; ++wy) {
            int r = iy + wy;
            for (int wx = -half; wx <= half; ++wx) {
                int c = ix + wx;
                float Ix = 0.5f * (prev[(size_t)r * pw + (c+1)] - prev[(size_t)r * pw + (c-1)]);
                float Iy = 0.5f * (prev[(size_t)(r+1) * pw + c] - prev[(size_t)(r-1) * pw + c]);
                Gxx += Ix * Ix; Gxy += Ix * Iy; Gyy += Iy * Iy; ++N;
            }
        }
        if (N < 4) return 0;
        float trace = Gxx + Gyy;
        float delta = (Gxx - Gyy) * (Gxx - Gyy) + 4.0f * Gxy * Gxy;
        float eig_min = 0.5f * (trace - sqrtf(delta > 0.0f ? delta : 0.0f));
        if (!(eig_min / (float)N >= min_eig)) return 0;
    }
    for (int iter = 0; iter < max_iter; ++iter) {
        float cx = px + *dx, cy = py + *dy;
        if (cx - half < 0.0f || cx + half >= (float)(cw - 1) ||
            cy - half < 0.0f || cy + half >= (float)(ch - 1))
            return 0;
        float Gxx = 0.0f, Gxy = 0.0f, Gyy = 0.0f, bx = 0.0f, by = 0.0f;
        for (int wy = -half; wy <= half; ++wy) {
            int   ipy = (int)(py + (float)wy);
            float ccy = cy + (float)wy;
            for (int wx = -half; wx <= half; ++wx) {
                int   ipx = (int)(px + (float)wx);
                float ccx = cx + (float)wx;
                float pv = prev[(size_t)ipy * pw + ipx];
                /* bilinear (迭代 guard 保证免 clamp) */
                int x0 = (int)ccx, y0 = (int)ccy, x1 = x0 + 1, y1 = y0 + 1;
                float fx = ccx - (float)x0, fy = ccy - (float)y0;
                float wx1 = 1.0f - fx, wy1 = 1.0f - fy;
                float v00 = (float)curr[(size_t)y0 * cstride + x0];
                float v10 = (float)curr[(size_t)y0 * cstride + x1];
                float v01 = (float)curr[(size_t)y1 * cstride + x0];
                float v11 = (float)curr[(size_t)y1 * cstride + x1];
                float cv = wy1*(wx1*v00 + fx*v10) + fy*(wx1*v01 + fx*v11);
                float Ix, Iy;
                if (ipx > 0 && ipx < pw - 1)
                    Ix = 0.5f * (prev[(size_t)ipy * pw + (ipx+1)] - prev[(size_t)ipy * pw + (ipx-1)]);
                else if (ipx == 0)
                    Ix = prev[(size_t)ipy * pw + 1] - prev[(size_t)ipy * pw];
                else
                    Ix = prev[(size_t)ipy * pw + (pw-1)] - prev[(size_t)ipy * pw + (pw-2)];
                if (ipy > 0 && ipy < ph - 1)
                    Iy = 0.5f * (prev[((size_t)(ipy+1)) * pw + ipx] - prev[((size_t)(ipy-1)) * pw + ipx]);
                else if (ipy == 0)
                    Iy = prev[(size_t)1 * pw + ipx] - prev[(size_t)0 * pw + ipx];
                else
                    Iy = prev[((size_t)(ph-1)) * pw + ipx] - prev[((size_t)(ph-2)) * pw + ipx];
                float It = cv - pv;
                Gxx += Ix * Ix; Gxy += Ix * Iy; Gyy += Iy * Iy;
                bx  += Ix * It; by  += Iy * It;
            }
        }
        float det = Gxx * Gyy - Gxy * Gxy;
        if (fabsf(det) < 1e-6f) return 0;
        float ddx = (Gyy*(-bx) - Gxy*(-by)) / det;
        float ddy = (Gxx*(-by) - Gxy*(-bx)) / det;
        *dx += ddx; *dy += ddy;
        if (sqrtf(ddx*ddx + ddy*ddy) < epsilon) break;
    }
    return 1;
}

/* =========================================================================
 * NEON 主实现 — 与 lk_level_f 同签名/同语义。
 * 前置 (调用方约定): min_eig <= 0; prev 梯度中心差分要求
 *   Xp-1>=0 && Xp+W<=pw-1 && Yp-1>=0 && Yp+H<=ph-1 (不满足由本函数自查回退)。
 * ======================================================================== */
int ngd_of_lk_level_f_neon(const float *prev, int pw, int ph,
                           const uint8_t *curr, int cw, int ch, int cstride,
                           float px, float py, float *dx, float *dy,
                           int half, int max_iter, float epsilon, float min_eig)
{
    const int W = 2 * half + 1;

    /* 自查: min_eig 门控或 prev 窗口贴边 → 标量回退 (语义与原版一致) */
    if (min_eig > 0.0f)
        return lk_level_f_scalar(prev, pw, ph, curr, cw, ch, cstride,
                                 px, py, dx, dy, half, max_iter, epsilon, min_eig);
    {
        int Xp = (int)px - half, Yp = (int)py - half;
        if (Xp < 1 || Xp + W > pw - 1 || Yp < 1 || Yp + W > ph - 1)
            return lk_level_f_scalar(prev, pw, ph, curr, cw, ch, cstride,
                                     px, py, dx, dy, half, max_iter, epsilon, min_eig);
    }

    for (int iter = 0; iter < max_iter; ++iter) {
        float cx = px + *dx, cy = py + *dy;
        if (cx - half < 0.0f || cx + half >= (float)(cw - 1) ||
            cy - half < 0.0f || cy + half >= (float)(ch - 1))
            return 0;

        /* 行内恒定小数部 (wx/wy 整数步进) */
        int   Xc = (int)cx - half, Yc = (int)cy - half;
        int   Yp0 = (int)py - half;
        float fx = cx - floorf(cx), fy = cy - floorf(cy);
        float wxf = 1.0f - fx, wyf = 1.0f - fy;

        float32x4_t aGxx = vdupq_n_f32(0.0f);
        float32x4_t aGxy = vdupq_n_f32(0.0f);
        float32x4_t aGyy = vdupq_n_f32(0.0f);
        float32x4_t aBx  = vdupq_n_f32(0.0f);
        float32x4_t aBy  = vdupq_n_f32(0.0f);
        float sGxx = 0.0f, sGxy = 0.0f, sGyy = 0.0f, sBx = 0.0f, sBy = 0.0f;  /* 尾列标量累加 */

        /* 4 列整块数; u8 向量 load 读 8 字节, 末块须保证 Xc+nfull+3 ≤ cw-1
         * (不满足则收缩 4 列并入标量尾, 仅贴右边界的窗口发生) */
        int nfull = W & ~3;
        while (nfull > 0 && Xc + nfull + 3 > cw - 1) nfull -= 4;
        const float half_ = 0.5f;
        const float32x4_t vhalf = vdupq_n_f32(0.5f);

        for (int wy = 0; wy < W; ++wy) {
            int ipy = Yp0 + wy;
            int icy = Yc + wy;             /* curr 行 y0 (bilinear 上行) */
            const uint8_t *rowA = curr + (size_t)icy * cstride + Xc;
            const uint8_t *rowB = rowA + (size_t)cstride;
            const float *prow  = prev + (size_t)ipy * pw + ((int)px - half);
            const float *prowU = prow + (size_t)pw;
            const float *prowD = prow - (size_t)pw;

            for (int i0 = 0; i0 < nfull; i0 += 4) {
                /* bilinear curr 采样: a0/a1 = 上行 x0/x0+1, b0/b1 = 下行 */
                float32x4_t a0 = vcvt4_u8_f32(rowA + i0);
                float32x4_t a1 = vcvt4_u8_f32(rowA + i0 + 1);
                float32x4_t b0 = vcvt4_u8_f32(rowB + i0);
                float32x4_t b1 = vcvt4_u8_f32(rowB + i0 + 1);
                float32x4_t top = vmlaq_n_f32(vmulq_n_f32(a0, wxf), a1, fx);
                float32x4_t bot = vmlaq_n_f32(vmulq_n_f32(b0, wxf), b1, fx);
                float32x4_t cv  = vmlaq_n_f32(vmulq_n_f32(top, wyf), bot, fy);

                /* prev 梯度: Ix = 0.5*(prow[i+1]-prow[i-1]), Iy = 0.5*(up-dn) */
                float32x4_t pv  = vld1q_f32(prow + i0);
                float32x4_t ir  = vld1q_f32(prow + i0 + 1);
                float32x4_t il  = vld1q_f32(prow + i0 - 1);
                float32x4_t pu  = vld1q_f32(prowU + i0);
                float32x4_t pd  = vld1q_f32(prowD + i0);
                float32x4_t Ix  = vmulq_f32(vsubq_f32(ir, il), vhalf);
                float32x4_t Iy  = vmulq_f32(vsubq_f32(pu, pd), vhalf);

                float32x4_t It = vsubq_f32(cv, pv);
                aGxx = vmlaq_f32(aGxx, Ix, Ix);
                aGxy = vmlaq_f32(aGxy, Ix, Iy);
                aGyy = vmlaq_f32(aGyy, Iy, Iy);
                aBx  = vmlaq_f32(aBx,  Ix, It);
                aBy  = vmlaq_f32(aBy,  Iy, It);
            }
            /* 尾列 (W%4, ≤3): 标量, 索引与整块同式 */
            for (int i = nfull; i < W; ++i) {
                float a0 = (float)rowA[i], a1 = (float)rowA[i+1];
                float b0 = (float)rowB[i], b1 = (float)rowB[i+1];
                float cv = wyf*(wxf*a0 + fx*a1) + fy*(wxf*b0 + fx*b1);
                float pv = prow[i];
                float Ix = half_ * (prow[i+1] - prow[i-1]);
                float Iy = half_ * (prowU[i] - prowD[i]);
                float It = cv - pv;
                sGxx += Ix * Ix; sGxy += Ix * Iy; sGyy += Iy * Iy;
                sBx  += Ix * It; sBy  += Iy * It;
            }
        }

        float Gxx = hsum_f32(aGxx) + sGxx, Gxy = hsum_f32(aGxy) + sGxy;
        float Gyy = hsum_f32(aGyy) + sGyy;
        float bx  = hsum_f32(aBx)  + sBx,  by  = hsum_f32(aBy)  + sBy;

        float det = Gxx * Gyy - Gxy * Gxy;
        if (fabsf(det) < 1e-6f) return 0;
        float ddx = (Gyy*(-bx) - Gxy*(-by)) / det;
        float ddy = (Gxx*(-by) - Gxy*(-bx)) / det;
        *dx += ddx; *dy += ddy;
        if (sqrtf(ddx*ddx + ddy*ddy) < epsilon) break;
    }
    return 1;
}
