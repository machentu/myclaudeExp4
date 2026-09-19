/*
 * brief_int_vp6.c - VP6 strip rotated-BRIEF, FIXED-POINT LUT variant.
 *
 * Same strip-sweep driver as brief_vp6.c (frame streams through two
 * resident SRAM ping/pong buffers; each pass computes descriptors for the
 * keypoints whose clamped centre row falls inside it), but the kernel
 * replaces ALL soft-float work with the xsTileLib ORBAngleBRIEF.c integer
 * form (probes T31a-d in test_ivp_probe.cpp):
 *
 *   - orientation binned to whole degrees: bin = (int)(angle + 0.5f)
 *   - 361-entry Q7 sin LUT | 361-entry cos LUT (722 bytes, built once)
 *   - IVP_LSR2NX8_X splats sin/cos of the bin to all 64 lanes
 *   - rotation per 64-point chunk, i8 lanes:
 *       col = IVP_MULP2NX8(NEG(Y), S, X, C) = X*cos - Y*sin   (GET_VALUE)
 *       row = IVP_MULP2NX8(X, S, Y, C)       = X*sin + Y*cos
 *       -> IVP_PACKVR2NX24(w, 7) = (prod + 64) >> 7 (T31c: rounds, arith)
 *   - +64 bias added in the i8 DOMAIN (r in [-19,19] -> [45,83], always
 *     positive) so the zero-extending UNPKU2NX8_0/_1 split is sign-correct
 *   - the UNPK halves feed the R6 clamp/linearise chain directly in
 *     registers (BIAS=1024 biased u16, mod-2^16 exact per T29/T30,
 *     IVP_MULANX16UPACKL fused multiply) - the rr16/cc16 plane round-trip
 *     and its 16b gathers are gone entirely
 *   - gather stage + descriptor fold copied verbatim from brief_vp6.c
 *
 * Lane-order note: UNPK_0/1 emit even/odd byte lanes, so a natural-order
 * pattern table would land stream points strided across off1/off2 slots.
 * Instead the per-stream tables are PRE-SCRAMBLED once at init (the
 * xsTileLib "bitPatterReArrange" trick):
 *     entry m of chunk q  <-  stream point 64q + (m>>1) + ((m&1)<<5)
 * which makes UNPK_0(chunk q) = points 64q..64q+31 and UNPK_1 = 64q+32..63,
 * i.e. off[s] = stream point s exactly as the gather stage expects - the
 * fold keeps the identity bit mapping with NO permutation at run time.
 *
 * NOT bit-exact with the float golden path (1-degree bins + Q7 sin/cos):
 * test_brief_int_vp6.c checks the vector kernel == scalar integer reference
 * bit-exactly and reports the Hamming distance vs the golden descriptors.
 * Select via BRIEF_INT_VP6 in dsp_orb_vp6.c; default stays on brief_vp6.
 */
#include "brief_int_vp6.h"
#include "frame_strip_vp6.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

/* IVP kernel is the only form of this variant (it exists to be fast on the
 * DSP; the portable scalar integer reference lives in the test). */
#define BRIEF_INT_USE_IVP_INTRIN 1

/* Hardware-round tag (same discipline as BRIEF_VP6_ROUND):
 *   I1 = first fixed-point LUT kernel (fused rotate/clamp/linearise)
 * Measured (checkerboard 320x240, 791 kps, 2026-09-18):
 *   kernel 1,352 cy/kp (rot 295 + gather 460 + resid 597) vs R6 67,769
 *   brief stage 54.89M -> 3.13M (includes one-time 722-entry double
 *   sin/cos LUT build ~1.5-2M; steady-state ~1.5M/frame)
 *   TOTAL 85.88M -> 34.12M (-60%). rot = 37 cy/chunk proves the R5/R6
 *   slow-loop mystery was the rr16/cc16 plane round-trip, not IVP ops. */
#define BRIEF_INT_ROUND "I1"

#ifndef BRIEF_PI
#define BRIEF_PI 3.14159265358979323846
#endif

#define BRIEF_INT_LUT_SIZE 361    /* sin[0..360] then cos[0..360], Q7 i8 */

/* ---- per-sub-stage cycle profiling (mirror of the brief_vp6 counters) */
unsigned long long brief_int_vp6_cycles[BRIEF_INT_T_COUNT];
unsigned long long brief_int_vp6_kps;

#define BRIEF_INT_TIMING 1
#ifdef BRIEF_INT_TIMING
#include <stdio.h>
#ifdef __XTENSA__
typedef unsigned int bi_tick_t;
static bi_tick_t bi_tick(void)
{
    unsigned int c;
    __asm__ volatile ("rsr.ccount %0" : "=a" (c));
    return c;
}
#else
#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif
typedef unsigned long long bi_tick_t;
static bi_tick_t bi_tick(void)
{
#ifdef _WIN32
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    return (bi_tick_t)li.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (bi_tick_t)ts.tv_sec * 1000000000ull + (bi_tick_t)ts.tv_nsec;
#endif
}
static double bi_ticks_to_us(bi_tick_t t)
{
#ifdef _WIN32
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    return (double)t * 1e6 / (double)f.QuadPart;
#else
    return (double)t / 1000.0;
#endif
}
#endif /* __XTENSA__ */
#define BI_TMARK(v)     do { (v) = bi_tick(); } while (0)
#define BI_TACC(v, slot) \
    do { brief_int_vp6_cycles[slot] += bi_tick() - (bi_tick_t)(v); } while (0)
#else
#define BI_TMARK(v)     do { } while (0)
#define BI_TACC(v, slot) do { } while (0)
#endif

#ifdef BRIEF_INT_USE_IVP_INTRIN
#ifdef __XTENSA__
#include <xtensa/tie/xt_ivpn.h>          /* real fy01_202210 core (found via --xtensa-system) */
#else
#include "xtensa/tie/Xm_fy01_202210.h"   /* cstub simulation (host build, CSTUB_PATH -I) */
#endif

/* UNPK even/odd split: the header names differ across releases - cstub and
 * the RI-2022.10 real header carry _0/_1 (T31d), xsTileLib-style headers
 * carry _L/_H. (The SELI 59/60 + MOVNX16_FROM2NX8U chain is WRONG in
 * cstub - identity/garbage lane map - do not use it as a fallback.) */
#ifndef IVP_UNPKU2NX8_0
#define IVP_UNPKU2NX8_0(a) IVP_UNPKU2NX8_L(a)
#define IVP_UNPKU2NX8_1(a) IVP_UNPKU2NX8_H(a)
#endif

/* persistent bank-1 SRAM scratch (base 64B-aligned from xvAllocateBuffer,
 * so every vector user below is 64B-aligned): */
#define BI_WS_OFF1   0        /* u16[256] gather offsets, stream A         */
#define BI_WS_OFF2   512      /* u16[256] gather offsets, stream B         */
#define BI_WS_BUF    1024     /* u8[64]  bit-lane staging                  */
#define BI_WS_ONE    1088     /* u8[64]  ones (LTU true lanes)             */
#define BI_WS_PATXA  1152     /* i8[256] scrambled X, stream A points      */
#define BI_WS_PATYA  1408     /* i8[256] scrambled Y, stream A points      */
#define BI_WS_PATXB  1664     /* i8[256] scrambled X, stream B points      */
#define BI_WS_PATYB  1920     /* i8[256] scrambled Y, stream B points      */
#define BI_WS_SCLUT  2176     /* i8[722] sin[361] | cos[361], Q7           */
#define BI_WS_BYTES  2944
typedef char bi_ws_layout_ok[(BI_WS_OFF1 + 512 <= BI_WS_OFF2
                           && BI_WS_OFF2 + 512 <= BI_WS_BUF
                           && BI_WS_BUF + 64 <= BI_WS_ONE
                           && BI_WS_ONE + 64 <= BI_WS_PATXA
                           && BI_WS_PATXA + 256 <= BI_WS_PATYA
                           && BI_WS_PATYA + 256 <= BI_WS_PATXB
                           && BI_WS_PATXB + 256 <= BI_WS_PATYB
                           && BI_WS_PATYB + 256 <= BI_WS_SCLUT
                           && BI_WS_SCLUT + 2 * BRIEF_INT_LUT_SIZE <= BI_WS_BYTES) ? 1 : -1];
static struct { xvTileManager *tm; uint8_t *buf; } bi_ws;
static int bi_tables_ok;   /* pattern + LUT built for the CURRENT buffer */

static void bi_ws_free(void)
{
    if (bi_ws.tm && bi_ws.buf) xvFreeBuffer(bi_ws.tm, bi_ws.buf);
    bi_ws.tm = NULL;
    bi_ws.buf = NULL;
}

/* ---- scalar integer reference (natural pair order; exported for the test
 *      harness). Same maths as the vector kernel: Q7 LUT, (prod+64)>>7,
 *      +64 bias irrelevant here (plain int), identical clamp chain. ---- */
void brief_int_scalar_kernel(const uint8_t *src_data, int src_pitch,
                             int origin_x, int origin_y,
                             int frame_w, int frame_h,
                             int cx, int cy, float angle, uint8_t *desc,
                             const int8_t *lut)
{
    int bin = (int)(angle + 0.5f);
    if (bin > 360) bin = 360;
    if (bin < 0) bin = 0;
    const int s7 = lut[bin], c7 = lut[BRIEF_INT_LUT_SIZE + bin];

    for (int i = 0; i < 32; ++i) {
        int val = 0;
        for (int bit = 0; bit < 8; ++bit) {
            const int pair = i * 8 + bit;
            const int p1x = brief_bit_pattern_31[pair][0], p1y = brief_bit_pattern_31[pair][1];
            const int p2x = brief_bit_pattern_31[pair][2], p2y = brief_bit_pattern_31[pair][3];
            const int r1 = (p1x * s7 + p1y * c7 + 64) >> 7;
            const int c1 = (p1x * c7 - p1y * s7 + 64) >> 7;
            const int r2 = (p2x * s7 + p2y * c7 + 64) >> 7;
            const int c2 = (p2x * c7 - p2y * s7 + 64) >> 7;
            int y1 = cy + r1, x1 = cx + c1, y2 = cy + r2, x2 = cx + c2;
            if (y1 < 0) y1 = 0; else if (y1 > frame_h - 1) y1 = frame_h - 1;
            if (x1 < 0) x1 = 0; else if (x1 > frame_w - 1) x1 = frame_w - 1;
            if (y2 < 0) y2 = 0; else if (y2 > frame_h - 1) y2 = frame_h - 1;
            if (x2 < 0) x2 = 0; else if (x2 > frame_w - 1) x2 = frame_w - 1;
            const int t0 = src_data[(y1 - origin_y) * src_pitch + (x1 - origin_x)];
            const int t1 = src_data[(y2 - origin_y) * src_pitch + (x2 - origin_x)];
            val |= (t0 < t1) << bit;
        }
        desc[i] = (uint8_t)val;
    }
}

/* Q7 sin|cos LUT: sin[0..360] then cos[0..360], whole degrees. Single
 * source of truth for the kernel init AND the test's scalar reference. */
void brief_int_build_lut(int8_t *dst)
{
    for (int i = 0; i < BRIEF_INT_LUT_SIZE; ++i) {
        const double rad = (double)i * (BRIEF_PI / 180.0);
        dst[i] = (int8_t)lrint(sin(rad) * 127.0);
        dst[BRIEF_INT_LUT_SIZE + i] = (int8_t)lrint(cos(rad) * 127.0);
    }
}

/* ---- vector kernel ---- */
static void brief_int_kernel_ivp(const uint8_t *src_data, int src_pitch,
                                 int origin_x, int origin_y,
                                 int frame_w, int frame_h,
                                 int cx, int cy, float angle, uint8_t *desc)
{
    uint8_t *ws = bi_ws.buf;
    uint16_t *off1 = (uint16_t *)(void *)(ws + BI_WS_OFF1);
    uint16_t *off2 = (uint16_t *)(void *)(ws + BI_WS_OFF2);
    const int8_t *sclut = (const int8_t *)(ws + BI_WS_SCLUT);

    bi_tick_t bt;
    BI_TMARK(bt);
    {
        enum { BF_BIAS = 1024 };
        int bin = (int)(angle + 0.5f);
        if (bin > 360) bin = 360;
        const xb_vec2Nx8 vS = IVP_LSR2NX8_X(sclut, bin);              /* splats */
        const xb_vec2Nx8 vC = IVP_LSR2NX8_X(sclut, BRIEF_INT_LUT_SIZE + bin);
        const xb_vec2Nx8 v64 = 64;                                    /* T31a  */
        const xb_vecNx16U vKy = IVP_REPNX16U(IVP_MOVNX16U_FROM16U((xb_int16U)(cy - 64 + BF_BIAS)), 0);
        const xb_vecNx16U vKx = IVP_REPNX16U(IVP_MOVNX16U_FROM16U((xb_int16U)(cx - 64 + BF_BIAS)), 0);
        const xb_vecNx16U vB0 = IVP_REPNX16U(IVP_MOVNX16U_FROM16U((xb_int16U)BF_BIAS), 0);
        const xb_vecNx16U vBy1 = IVP_REPNX16U(IVP_MOVNX16U_FROM16U((xb_int16U)(BF_BIAS + frame_h - 1)), 0);
        const xb_vecNx16U vBx1 = IVP_REPNX16U(IVP_MOVNX16U_FROM16U((xb_int16U)(BF_BIAS + frame_w - 1)), 0);
        const xb_vecNx16U vBo = IVP_REPNX16U(IVP_MOVNX16U_FROM16U((xb_int16U)(BF_BIAS + origin_y)), 0);
        const xb_vecNx16U vKp = IVP_REPNX16U(IVP_MOVNX16U_FROM16U((xb_int16U)src_pitch), 0);

        for (int st = 0; st < 2; ++st) {
            const int8_t *PX = (const int8_t *)(ws + (st ? BI_WS_PATXB : BI_WS_PATXA));
            const int8_t *PY = (const int8_t *)(ws + (st ? BI_WS_PATYB : BI_WS_PATYA));
            uint16_t *off = st ? off2 : off1;
            for (int q = 0; q < 4; ++q) {
                const xb_vec2Nx8 vX = *(const xb_vec2Nx8 *)(const void *)(PX + q * 64);
                const xb_vec2Nx8 vY = *(const xb_vec2Nx8 *)(const void *)(PY + q * 64);
                /* GET_VALUE: col_off = X*cos - Y*sin ; row_off = X*sin + Y*cos
                 * (a*b + c*d MAC, T31c; (prod + 64) >> 7 rounds, arithmetic) */
                const xb_vec2Nx8 c8 = IVP_PACKVR2NX24(
                    IVP_MULP2NX8(IVP_NEG2NX8(vY), vS, vX, vC), 7);
                const xb_vec2Nx8 r8 = IVP_PACKVR2NX24(
                    IVP_MULP2NX8(vX, vS, vY, vC), 7);
                /* +64 in the i8 domain: r in [-19,19] -> [45,83], always
                 * positive, so the zero-extending UNPK below is sign-correct */
                const xb_vec2Nx8 c8b = IVP_ADD2NX8(c8, v64);
                const xb_vec2Nx8 r8b = IVP_ADD2NX8(r8, v64);

                /* even bytes (= stream points 64q..64q+31, thanks to the
                 * scrambled tables) -> off[64q .. 64q+31] */
                {
                    const xb_vecNx16U vc = (xb_vecNx16U)IVP_UNPKU2NX8_0(c8b);
                    const xb_vecNx16U vr = (xb_vecNx16U)IVP_UNPKU2NX8_0(r8b);
                    const xb_vecNx16U xb = IVP_MINUNX16U(IVP_MAXUNX16U(IVP_ADDNX16U(vc, vKx), vB0), vBx1);
                    const xb_vecNx16U yb = IVP_MINUNX16U(IVP_MAXUNX16U(IVP_ADDNX16U(vr, vKy), vB0), vBy1);
                    const xb_vecNx16U yo = IVP_SUBNX16U(yb, vBo);
                    xb_vecNx16U offv = IVP_SUBNX16U(xb, vB0);   /* xo (origin_x = 0) */
                    IVP_MULANX16UPACKL(offv, yo, vKp);          /* += low16(yo*pitch) */
                    xb_vecNx16U *dst = (xb_vecNx16U *)(void *)(off + q * 64);
                    IVP_SVNX16U_IP(offv, dst, (int)sizeof(offv));
                }
                /* odd bytes (= stream points 64q+32..64q+63) -> off[.. +32] */
                {
                    const xb_vecNx16U vc = (xb_vecNx16U)IVP_UNPKU2NX8_1(c8b);
                    const xb_vecNx16U vr = (xb_vecNx16U)IVP_UNPKU2NX8_1(r8b);
                    const xb_vecNx16U xb = IVP_MINUNX16U(IVP_MAXUNX16U(IVP_ADDNX16U(vc, vKx), vB0), vBx1);
                    const xb_vecNx16U yb = IVP_MINUNX16U(IVP_MAXUNX16U(IVP_ADDNX16U(vr, vKy), vB0), vBy1);
                    const xb_vecNx16U yo = IVP_SUBNX16U(yb, vBo);
                    xb_vecNx16U offv = IVP_SUBNX16U(xb, vB0);
                    IVP_MULANX16UPACKL(offv, yo, vKp);
                    xb_vecNx16U *dst = (xb_vecNx16U *)(void *)(off + q * 64 + 32);
                    IVP_SVNX16U_IP(offv, dst, (int)sizeof(offv));
                }
            }
        }
    }
    BI_TACC(bt, BRIEF_INT_T_ROT);
    BI_TMARK(bt);   /* restart for the gather segment (bt spans one stage) */

    /* gather + bit pack: verbatim from brief_vp6.c (off[s] = stream point s
     * there and here, so lane/byte/bit order carries over unchanged) */
    {
        uint8_t *one64 = ws + BI_WS_ONE, *buf = ws + BI_WS_BUF;
        xb_vec2Nx8U vone = IVP_LV2NX8U_X((const xb_vec2Nx8U *)(const void *)one64, 0);
        xb_vec2Nx8U vzero = IVP_SUB2NX8U(vone, vone);   /* all-zero lanes (wraps) */
        const unsigned char *gbase = src_data;

        for (int ch = 0; ch < 4; ++ch) {
            const uint16_t *o1 = off1 + ch * 64, *o2 = off2 + ch * 64;
            xb_vecNx16U a1 = *(const xb_vecNx16U *)(const void *)(o1);
            xb_vecNx16U b1 = *(const xb_vecNx16U *)(const void *)(o1 + 32);
            xb_vecNx16U a2 = *(const xb_vecNx16U *)(const void *)(o2);
            xb_vecNx16U b2 = *(const xb_vecNx16U *)(const void *)(o2 + 32);
            xb_gsr g1a = IVP_GATHERANX8U(gbase, a1);
            xb_gsr g1b = IVP_GATHERANX8U(gbase, b1);
            xb_gsr g2a = IVP_GATHERANX8U(gbase, a2);
            xb_gsr g2b = IVP_GATHERANX8U(gbase, b2);
            xb_vec2Nx8U t0 = IVP_GATHERD2NX8U(g1b, g1a);   /* pixels o1[0..63] */
            xb_vec2Nx8U t1 = IVP_GATHERD2NX8U(g2b, g2a);   /* pixels o2[0..63] */
            xb_vec2Nx8U bits = IVP_MOV2NX8UT(vone, vzero, IVP_LTU2NX8(t0, t1));
            xb_vec2Nx8U *bp = (xb_vec2Nx8U *)(void *)buf;
            IVP_SV2NX8U_IP(bits, bp, (int)sizeof(bits));
            uint8_t *d = desc + ch * 8;
            for (int i = 0; i < 8; ++i)
                d[i] = (uint8_t)(buf[i * 8 + 0]       | buf[i * 8 + 1] << 1 | buf[i * 8 + 2] << 2 |
                                 buf[i * 8 + 3] << 3 | buf[i * 8 + 4] << 4 | buf[i * 8 + 5] << 5 |
                                 buf[i * 8 + 6] << 6 | buf[i * 8 + 7] << 7);
        }
    }
    BI_TACC(bt, BRIEF_INT_T_GATHER);
}
#endif /* BRIEF_INT_USE_IVP_INTRIN */

/* Round a float keypoint coord the way ngd_compute_descriptor does ((int)lroundf). */
static inline int bi_round(float v) { return (int)lroundf(v); }

/* persistent ping/pong strip pair (allocated once, reused across calls) */
static fs_bufs bi_fs;

int brief_int_vp6_process(xvTileManager *tm, xvFrame *src_frame,
                          int src_w, int src_h,
                          const fast9_keypoint *kps, uint8_t *desc, int n_kps,
                          const BriefIntVp6Config *cfg)
{
    BriefIntVp6Config def = BRIEF_INT_VP6_DEFAULT_CONFIG;
    BriefIntVp6Config c = cfg ? *cfg : def;

    const int SR  = c.strip_rows;
    const int MSW = c.max_src_w;
    const int MSH = c.max_src_h;

    if (!tm || !src_frame || !kps || !desc) return -1;
    if (SR < 1 || MSW < 1) return -1;
    if (MSH < SR + 2 * BRIEF_MARGIN) return -1;      /* halo must fit too */
    if (MSW < src_w) return -1;                      /* strips span width */

    if (n_kps <= 0) return 0;

    if (fs_alloc(&bi_fs, tm, FS_ALIGN64(MSW) * MSH) != 0) return -2;

    {
        /* SRAM scratch (bank 1, same residency argument as brief_vp6) */
        if (bi_ws.tm != tm || !bi_ws.buf) {
            bi_ws_free();
            uint8_t *p = (uint8_t *)xvAllocateBuffer(tm, BI_WS_BYTES,
                                                     XV_MEM_BANK_COLOR_1, 64);
            if (!p || p == (void *)(intptr_t)XVTM_ERROR) return -2;
            bi_ws.buf = p;
            bi_ws.tm = tm;
            bi_tables_ok = 0;                /* fresh buffer: rebuild tables */
        }
        if (!bi_tables_ok) {
            /* scrambled per-stream pattern tables: entry m of chunk q holds
             * stream point 64q + (m>>1) + ((m&1)<<5), so the even/odd UNPK
             * split lands points contiguously in off[] slots (see header). */
            int8_t *pxa = (int8_t *)(void *)(bi_ws.buf + BI_WS_PATXA);
            int8_t *pya = (int8_t *)(void *)(bi_ws.buf + BI_WS_PATYA);
            int8_t *pxb = (int8_t *)(void *)(bi_ws.buf + BI_WS_PATXB);
            int8_t *pyb = (int8_t *)(void *)(bi_ws.buf + BI_WS_PATYB);
            for (int q = 0; q < 4; ++q) {
                for (int m = 0; m < 64; ++m) {
                    const int t = 64 * q + (m >> 1) + ((m & 1) << 5);
                    pxa[64 * q + m] = (int8_t)brief_bit_pattern_31[t][0];
                    pya[64 * q + m] = (int8_t)brief_bit_pattern_31[t][1];
                    pxb[64 * q + m] = (int8_t)brief_bit_pattern_31[t][2];
                    pyb[64 * q + m] = (int8_t)brief_bit_pattern_31[t][3];
                }
            }
            /* Q7 sin|cos LUT, one 722-byte build per workspace allocation */
            brief_int_build_lut((int8_t *)(void *)(bi_ws.buf + BI_WS_SCLUT));
            bi_tables_ok = 1;
        }
        memset(bi_ws.buf + BI_WS_ONE, 1, 64);
    }

    fs_sweep sw;
    fs_sweep_begin(&sw, &bi_fs, src_frame, src_w, src_h, BRIEF_MARGIN);

    /* keypoint -> pass assignment identical to brief_vp6 (clamped centre
     * row; the clamp is 1-Lipschitz so all reads land in the 19-row halo) */
    fs_view v;
    while (fs_sweep_next(&sw, &v)) {
        for (int k = 0; k < n_kps; ++k) {
            int cy = bi_round(kps[k].y);
            if (cy < 0) cy = 0; else if (cy > src_h - 1) cy = src_h - 1;
            if (cy < v.top || cy >= v.top + v.rows) continue;   /* other pass */
            bi_tick_t bk;
            BI_TMARK(bk);
            brief_int_kernel_ivp(v.data, v.pitch, 0, v.srow0,
                                 src_w, src_h,
                                 bi_round(kps[k].x), bi_round(kps[k].y),
                                 kps[k].angle, desc + (size_t)k * 32);
            BI_TACC(bk, BRIEF_INT_T_KERNEL);
            ++brief_int_vp6_kps;
        }
    }
    return 0;
}

void brief_int_vp6_release(void)
{
    fs_free(&bi_fs);
#ifdef BRIEF_INT_USE_IVP_INTRIN
    bi_ws_free();
#endif
}

void brief_int_vp6_timing_report(void)
{
#ifdef BRIEF_INT_TIMING
    const unsigned long long nk = brief_int_vp6_kps ? brief_int_vp6_kps : 1;
    const unsigned long long sub = brief_int_vp6_cycles[BRIEF_INT_T_ROT]
                                 + brief_int_vp6_cycles[BRIEF_INT_T_GATHER];
#ifdef __XTENSA__
    printf("[brief int] round    : %s\n", BRIEF_INT_ROUND);
    printf("[brief int] kps      : %llu\n", brief_int_vp6_kps);
    printf("[brief int] kernel   : %llu  (%llu/kp)\n",
           brief_int_vp6_cycles[BRIEF_INT_T_KERNEL],
           brief_int_vp6_cycles[BRIEF_INT_T_KERNEL] / nk);
    printf("[brief int] rot      : %llu  (%llu/kp)\n",
           brief_int_vp6_cycles[BRIEF_INT_T_ROT],
           brief_int_vp6_cycles[BRIEF_INT_T_ROT] / nk);
    printf("[brief int] gather   : %llu  (%llu/kp)\n",
           brief_int_vp6_cycles[BRIEF_INT_T_GATHER],
           brief_int_vp6_cycles[BRIEF_INT_T_GATHER] / nk);
    printf("[brief int] resid    : %llu  (%llu/kp)\n",
           brief_int_vp6_cycles[BRIEF_INT_T_KERNEL] - sub,
           (brief_int_vp6_cycles[BRIEF_INT_T_KERNEL] - sub) / nk);
#else
    double us[BRIEF_INT_T_COUNT];
    for (int i = 0; i < BRIEF_INT_T_COUNT; ++i)
        us[i] = bi_ticks_to_us((bi_tick_t)brief_int_vp6_cycles[i]);
    const double us_sub = us[BRIEF_INT_T_ROT] + us[BRIEF_INT_T_GATHER];
    printf("[brief int] round    : %s\n", BRIEF_INT_ROUND);
    printf("[brief int] kps      : %llu\n", brief_int_vp6_kps);
    printf("[brief int] kernel   : %10.1f us  (%5.1f us/kp)\n",
           us[BRIEF_INT_T_KERNEL], us[BRIEF_INT_T_KERNEL] / (double)nk);
    printf("[brief int] rot      : %10.1f us  (%5.1f us/kp)\n",
           us[BRIEF_INT_T_ROT], us[BRIEF_INT_T_ROT] / (double)nk);
    printf("[brief int] gather   : %10.1f us  (%5.1f us/kp)\n",
           us[BRIEF_INT_T_GATHER], us[BRIEF_INT_T_GATHER] / (double)nk);
    printf("[brief int] resid    : %10.1f us  (%5.1f us/kp)\n",
           us[BRIEF_INT_T_KERNEL] - us_sub,
           (us[BRIEF_INT_T_KERNEL] - us_sub) / (double)nk);
#endif
#endif /* BRIEF_INT_TIMING */
}
