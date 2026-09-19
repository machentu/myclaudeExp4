/*
 * orb_resize_strip_vp6.c - IVP strip driver for the ORB pyramid resize.
 *
 * Golden chain (orb_resize_core.c orb_resize_kernel, INTER_LINEAR 11-bit):
 *   horizontal per output column dx (per source row):
 *     x0 = xofs[dx];  x1 = min(x0+1, sw-1);
 *     s  = S[x0]*a0 + S[x1]*a1;            a = ialpha (rint(c*2048), a0+a1=2048)
 *   vertical per output row dy:
 *     y0 = yofs[dy];  y1 = min(y0+1, sh-1);
 *     v  = ((b0*(s0>>4))>>16) + ((b1*(s1>>4))>>16) + 2;   b = ibeta
 *     D  = (uint8_t)(v>>2);
 *
 * Everything the IVP form does is an EXACT integer identity on this chain
 * (each step pinned by a numbered test_ivp_probe case):
 *
 *  - Horizontal: one 64-lane chunk computes s for 64 columns at once.
 *    Pixels come from GATHERANX8U(u8 base, u16 offsets) + GATHERD2NX8U
 *    (second argument -> lanes 0-31, T21); weights are per-lane SIGNED 16-bit
 *    through MULUSI2NX8X16 / MULUSAI2NX8X16 with c -> ODD lanes, d -> EVEN
 *    lanes (T24a; the table builder interleaves accordingly). All weights are
 *    in [0,2048] and pixels in [0,255], so s <= 255*2048 = 522,240 < 2^23 -
 *    the 2Nx24 lane stays positive (T20: lanes >= 0x800000 read negative).
 *    The horizontal intermediate m = s>>4 <= 32,640 is emitted as TWO u8
 *    planes: Hm = m>>8 <= 127 via PACKVRNR2NX24(s,12) (no wrap) and
 *    Lm = m&255 via PACKVRNR2NX24(s,4) (wrap = intended low byte, T22).
 *
 *  - Vertical: m = 256*Hm + Lm and b = 256*b_hi + b_lo (b <= 2048, so
 *    b_hi <= 8, b_lo <= 255). Nested-floor identity:
 *        (b*m)>>16 == ( b*Hm + b_hi*Lm + floor(b_lo*Lm/256) ) >> 8
 *    (proof: b*m = 256*b*Hm + 256*b_hi*Lm + b_lo*Lm; the fraction b_lo*Lm/256
 *    never crosses an integer boundary because the other terms are integers).
 *    The right side is computed per tap in ONE 24-bit accumulator:
 *        T = MULUS(Hm,b) + MULUSPA(Lm,b_hi) + MULUSPA(PACKVRNR(MULUS(Lm,b_lo),8), 1)
 *    with T <= 2048*127 + 8*255 + 254 = 262,136 < 262,144 = 2^18 - so
 *        t = T>>8   (= (b*m)>>16 exactly, <= 1020 by the b0+b1=2048 invariant)
 *    splits EXACTLY as 256*hi + lo with
 *        hi = PACKVRNR2NX24(T,16)  (t>>8 <= 3, no wrap)
 *        lo = PACKVRNR2NX24(T,8)   (t&255)
 *    (the pack shifts a 24-bit lane by min(n,23) and keeps the low byte -
 *    verified in the cstub source). Merge of the two taps + the golden's
 *    rounding constant, using 256*H/4 = 64*H exactly:
 *        out = (t0+t1+2)>>2 = 64*(hi0+hi1) + floor((lo0+lo1+2)/4)
 *    where floor((lo0+lo1+2)/4) = PACKVRU2NX24(lo0+lo1, 2) (T20: (v+2)>>2 for
 *    positive v - the golden's +2 IS the pack's round constant); the u8 sums
 *    go through weight-1/64 MULUS/MULUSPA lanes and the final byte is
 *    PACKL2NX24's low byte. Bounds: t0+t1 <= 1020 caps hi0+hi1 <= 3, and each
 *    hi-sum level caps the matching lo-sum, so out <= 255 always.
 *
 *
 *  - Strip sweep: pass p is ASSIGNED source rows [top, top+eff); the strip
 *    additionally carries the row below (asymmetric halo +1: the vertical tap
 *    y1 = min(y0+1, sh-1) of an assigned row is at most top+rows), clamped
 *    at sh-1. Output rows are assigned by yofs[dy] (monotonic non-decreasing
 *    for a downscale) via a cursor - each dy lands in exactly one pass. The
 *    horizontal pass fills Hm/Lm rows [0, srows) of SRAM planes (pitch
 *    ALIGN64(dw)); the vertical pass reads them with plain aligned 64B loads.
 *    Full 64-column chunks store through the SA streaming protocol (dst rows
 *    are arbitrarily aligned, pitch = dw); the tail chunk lands in a 64B
 *    stack buffer and a memcpy moves the dw%64 bytes (table lanes past dw
 *    carry offset 0 / weight 0, so their outputs are don't-cares).
 *
 *  - Buffers: all in memory bank 1 (bank 0 holds the other four drivers'
 *    strips and has only ~7KB left). Persistent grow-reuse set:
 *    strip ping/pong (2 x srows*ALIGN64(sw)), Hm/Lm planes (2 x srows*
 *    ALIGN64(dw)), and the per-level lane tables (512B per 64-column chunk:
 *    off0[64] u16, off1[64] u16, w0c/w0d/w1c/w1d i16[32]). eff is sized so
 *    the whole set stays under ORB_RS_VP6_BANK1_BUDGET (default 45,000B: it
 *    must coexist with the four pixel drivers' resident strips - measured
 *    82,676B, leaving 46,348B of the 129,024B bank). dsp_orb_vp6 calls
 *    orb_resize_strip_vp6_release() once the pyramid is built so phase 2's
 *    drivers get bank 1 to themselves; any allocation failure makes the
 *    caller fall back to orb_resize_full (outputs identical).
 *
 * The coefficient tables are rebuilt per call with the SHARED
 * orb_resize_precompute_h/v (identical rint/floor arithmetic - bit-exact by
 * construction; the same cost the pure-C path pays).
 */
#include "orb_resize_strip_vp6.h"
#include "orb_resize_core.h"

#include <stdlib.h>
#include <string.h>

/* SRAM budget for this driver's whole bank-1 buffer set (see header). */
#ifndef ORB_RS_VP6_BANK1_BUDGET
#define ORB_RS_VP6_BANK1_BUDGET 45000
#endif

#ifndef ALIGN64
#  ifdef _MSC_VER
#    define ALIGN64 __declspec(align(64))
#  else
#    define ALIGN64 __attribute__((aligned(64)))
#  endif
#endif

#define RS_ALIGN64(n) (((n) + 63) & ~63)

#ifdef __XTENSA__
#include <xtensa/tie/xt_ivpn.h>          /* real fy01_202210 core */
#else
#include "xtensa/tie/Xm_fy01_202210.h"   /* cstub simulation (host build) */
#endif

/* ---------------- IVP kernels ---------------- */

/* One 64-column horizontal chunk: gather the two taps, per-lane weighted MAC,
 * emit Hm/Lm bytes at hrow+x / lrow+x (both 64B-aligned, 64 bytes available).
 * o0/o1: u16[64] gather offsets (x0 / x1, lanes past dw are 0).
 * w:     i16[128] = [w0c[32] | w0d[32] | w1c[32] | w1d[32]] (c -> odd lanes,
 *        d -> even lanes - test_ivp_probe T24a). */
static inline void rs_h_chunk(const uint8_t *srow,
                              const uint16_t *o0, const uint16_t *o1,
                              const int16_t *w,
                              uint8_t *hrow, uint8_t *lrow, int x)
{
    xb_gsr g0a = IVP_GATHERANX8U(srow, *(const xb_vecNx16U *)(const void *)(o0));
    xb_gsr g0b = IVP_GATHERANX8U(srow, *(const xb_vecNx16U *)(const void *)(o0 + 32));
    xb_gsr g1a = IVP_GATHERANX8U(srow, *(const xb_vecNx16U *)(const void *)(o1));
    xb_gsr g1b = IVP_GATHERANX8U(srow, *(const xb_vecNx16U *)(const void *)(o1 + 32));
    xb_vec2Nx8U p0u = IVP_GATHERD2NX8U(g0b, g0a);   /* lanes 0-31 <- o0[0..31] */
    xb_vec2Nx8U p1u = IVP_GATHERD2NX8U(g1b, g1a);
    const xb_vec2Nx8 *p0 = (const xb_vec2Nx8 *)(const void *)&p0u;
    const xb_vec2Nx8 *p1 = (const xb_vec2Nx8 *)(const void *)&p1u;
    xb_vec2Nx24 s = IVP_MULUSI2NX8X16(*p0,
        *(const xb_vecNx16 *)(const void *)(w),        /* c: odd lanes  */
        *(const xb_vecNx16 *)(const void *)(w + 32));  /* d: even lanes */
    IVP_MULUSAI2NX8X16(s, *p1,
        *(const xb_vecNx16 *)(const void *)(w + 64),
        *(const xb_vecNx16 *)(const void *)(w + 96));
    xb_vec2Nx8 h8 = IVP_PACKVRNR2NX24(s, 12);         /* Hm = m>>8 <= 127 */
    xb_vec2Nx8 l8 = IVP_PACKVRNR2NX24(s, 4);          /* Lm = m&255       */
    xb_vec2Nx8U *hp = (xb_vec2Nx8U *)(void *)(hrow + x);
    xb_vec2Nx8U *lp = (xb_vec2Nx8U *)(void *)(lrow + x);
    IVP_SV2NX8U_IP(*(xb_vec2Nx8U *)(void *)&h8, hp, 64);
    IVP_SV2NX8U_IP(*(xb_vec2Nx8U *)(void *)&l8, lp, 64);
}

/* One full source strip row -> Hm/Lm plane rows. */
static void rs_h_row(const uint8_t *srow, uint8_t *hrow, uint8_t *lrow,
                     const uint16_t *toff, const int16_t *tw,
                     int chunks)
{
    for (int c = 0; c < chunks; ++c) {
        rs_h_chunk(srow, toff + (size_t)c * 128, toff + (size_t)c * 128 + 64,
                   tw + (size_t)c * 128, hrow, lrow, c * 64);
    }
}

/* One 64-column vertical chunk -> 64 output bytes. The four pointers give
 * the chunk's Hm/Lm plane bytes for tap 0 and tap 1 (64B-aligned). See the
 * file header for the identity chain. */
static inline xb_vec2Nx8U rs_v_chunk(const uint8_t *h0p, const uint8_t *l0p,
                                     const uint8_t *h1p, const uint8_t *l1p,
                                     int b0, int b1)
{
    const int b0h = b0 >> 8, b0l = b0 & 255;
    const int b1h = b1 >> 8, b1l = b1 & 255;
    xb_vec2Nx8U hv0 = IVP_LV2NX8U_X((const xb_vec2Nx8U *)(const void *)h0p, 0);
    xb_vec2Nx8U lv0 = IVP_LV2NX8U_X((const xb_vec2Nx8U *)(const void *)l0p, 0);
    xb_vec2Nx8U hv1 = IVP_LV2NX8U_X((const xb_vec2Nx8U *)(const void *)h1p, 0);
    xb_vec2Nx8U lv1 = IVP_LV2NX8U_X((const xb_vec2Nx8U *)(const void *)l1p, 0);

    /* T_i = b_i*Hm + b_hi*Lm + floor(b_lo*Lm/256)  (<= 262,136 < 2^18) */
    xb_vec2Nx24 t0 = IVP_MULUS2N8XR16(hv0, b0);
    IVP_MULUSPA2N8XR16(t0, lv0, lv0, b0h);
    xb_vec2Nx24 u0 = IVP_MULUS2N8XR16(lv0, b0l);
    xb_vec2Nx8 f0s = IVP_PACKVRNR2NX24(u0, 8);        /* <= 254, no wrap */
    xb_vec2Nx8U f0 = *(const xb_vec2Nx8U *)(const void *)&f0s;
    IVP_MULUSPA2N8XR16(t0, f0, f0, 1);

    xb_vec2Nx24 t1 = IVP_MULUS2N8XR16(hv1, b1);
    IVP_MULUSPA2N8XR16(t1, lv1, lv1, b1h);
    xb_vec2Nx24 u1 = IVP_MULUS2N8XR16(lv1, b1l);
    xb_vec2Nx8 f1s = IVP_PACKVRNR2NX24(u1, 8);
    xb_vec2Nx8U f1 = *(const xb_vec2Nx8U *)(const void *)&f1s;
    IVP_MULUSPA2N8XR16(t1, f1, f1, 1);

    /* out = (t0+t1+2)>>2 = 64*(hi0+hi1) + ((lo0+lo1+2)>>2), t = T>>8 */
    xb_vec2Nx8 h0s = IVP_PACKVRNR2NX24(t0, 16);       /* t0>>8 <= 3 */
    xb_vec2Nx8 l0s = IVP_PACKVRNR2NX24(t0, 8);        /* t0&255     */
    xb_vec2Nx8 h1s = IVP_PACKVRNR2NX24(t1, 16);
    xb_vec2Nx8 l1s = IVP_PACKVRNR2NX24(t1, 8);
    xb_vec2Nx8U h0 = *(const xb_vec2Nx8U *)(const void *)&h0s;
    xb_vec2Nx8U l0 = *(const xb_vec2Nx8U *)(const void *)&l0s;
    xb_vec2Nx8U h1 = *(const xb_vec2Nx8U *)(const void *)&h1s;
    xb_vec2Nx8U l1 = *(const xb_vec2Nx8U *)(const void *)&l1s;

    xb_vec2Nx24 accC = IVP_MULUS2N8XR16(l0, 1);       /* lo0+lo1 <= 510 */
    IVP_MULUSPA2N8XR16(accC, l1, l1, 1);
    xb_vec2Nx8 cvs = IVP_PACKVRU2NX24(accC, 2);       /* (lo0+lo1+2)>>2 */
    xb_vec2Nx8U cv = *(const xb_vec2Nx8U *)(const void *)&cvs;

    xb_vec2Nx24 accF = IVP_MULUS2N8XR16(h0, 64);      /* 64*(hi0+hi1)+carry */
    IVP_MULUSPA2N8XR16(accF, h1, h1, 64);
    IVP_MULUSPA2N8XR16(accF, cv, cv, 1);
    xb_vec2Nx8 o8 = IVP_PACKL2NX24(accF);
    return *(const xb_vec2Nx8U *)(const void *)&o8;
}

/* One output row: two plane rows (r0 = y0-top, r1 = y1-top), weights b0/b1,
 * dst row start (pitch = dw, arbitrary alignment - full chunks stream
 * through SA, the tail chunk goes via a stack buffer). */
static void rs_v_row(const uint8_t *hm, const uint8_t *lm, int r0, int r1,
                     int b0, int b1, int dp, int dw, uint8_t *dst)
{
    const uint8_t *h0 = hm + (size_t)r0 * dp, *l0 = lm + (size_t)r0 * dp;
    const uint8_t *h1 = hm + (size_t)r1 * dp, *l1 = lm + (size_t)r1 * dp;
    const int nfull = dw >> 6;
    int x = 0;
    if (nfull > 0) {
        xb_vec2Nx8U *mp = (xb_vec2Nx8U *)(void *)dst;
        valign sa;
        for (; x < (nfull << 6); x += 64) {
            xb_vec2Nx8U out8 = rs_v_chunk(h0 + x, l0 + x, h1 + x, l1 + x,
                                          b0, b1);
            IVP_SA2NX8U_IP(out8, sa, mp);
        }
        IVP_SAPOS2NX8U_FP(sa, mp);
    }
    if (x < dw) {                                     /* tail: dw % 64 */
        ALIGN64 uint8_t buf[64];
        xb_vec2Nx8U *bp = (xb_vec2Nx8U *)(void *)buf;
        xb_vec2Nx8U out8 = rs_v_chunk(h0 + x, l0 + x, h1 + x, l1 + x,
                                      b0, b1);
        IVP_SV2NX8U_IP(out8, bp, 64);
        memcpy(dst + x, buf, (size_t)(dw - x));
    }
}

/* ---------------- persistent bank-1 buffer set ---------------- */

typedef struct {
    xvTileManager *tm;
    uint8_t *strip[2];    /* ping/pong source strips   */
    uint8_t *hm, *lm;     /* Hm/Lm byte planes         */
    uint8_t *tbl;         /* lane tables (off + w)     */
    int strip_cap, plane_cap, tbl_cap;   /* bytes, 0 = unset */
} rs_bufs;

static rs_bufs g_rs;

static void rs_free_bufs(void)
{
    if (g_rs.tm) {
        if (g_rs.strip[0]) xvFreeBuffer(g_rs.tm, g_rs.strip[0]);
        if (g_rs.strip[1]) xvFreeBuffer(g_rs.tm, g_rs.strip[1]);
        if (g_rs.hm) xvFreeBuffer(g_rs.tm, g_rs.hm);
        if (g_rs.lm) xvFreeBuffer(g_rs.tm, g_rs.lm);
        if (g_rs.tbl) xvFreeBuffer(g_rs.tm, g_rs.tbl);
    }
    g_rs.tm = NULL;
    g_rs.strip[0] = g_rs.strip[1] = g_rs.hm = g_rs.lm = g_rs.tbl = NULL;
    g_rs.strip_cap = g_rs.plane_cap = g_rs.tbl_cap = 0;
}

/* Allocate (or keep / grow) the whole bank-1 set. 0 ok, -2 failed. */
static int rs_alloc_bufs(xvTileManager *tm, int strip_bytes, int plane_bytes,
                         int tbl_bytes)
{
    if (g_rs.tm == tm && g_rs.strip_cap >= strip_bytes
        && g_rs.plane_cap >= plane_bytes && g_rs.tbl_cap >= tbl_bytes)
        return 0;                                     /* resident, reuse */
    rs_free_bufs();
    g_rs.strip[0] = (uint8_t *)xvAllocateBuffer(tm, strip_bytes,
                                                XV_MEM_BANK_COLOR_1, 64);
    g_rs.strip[1] = (uint8_t *)xvAllocateBuffer(tm, strip_bytes,
                                                XV_MEM_BANK_COLOR_1, 64);
    g_rs.hm = (uint8_t *)xvAllocateBuffer(tm, plane_bytes,
                                          XV_MEM_BANK_COLOR_1, 64);
    g_rs.lm = (uint8_t *)xvAllocateBuffer(tm, plane_bytes,
                                          XV_MEM_BANK_COLOR_1, 64);
    g_rs.tbl = (uint8_t *)xvAllocateBuffer(tm, tbl_bytes,
                                           XV_MEM_BANK_COLOR_1, 64);
    if (!g_rs.strip[0] || !g_rs.strip[1] || !g_rs.hm || !g_rs.lm || !g_rs.tbl
        || g_rs.strip[0] == (void *)(intptr_t)XVTM_ERROR
        || g_rs.strip[1] == (void *)(intptr_t)XVTM_ERROR
        || g_rs.hm == (void *)(intptr_t)XVTM_ERROR
        || g_rs.lm == (void *)(intptr_t)XVTM_ERROR
        || g_rs.tbl == (void *)(intptr_t)XVTM_ERROR) {
        rs_free_bufs();
        return -2;
    }
    g_rs.tm = tm;
    g_rs.strip_cap = strip_bytes;
    g_rs.plane_cap = plane_bytes;
    g_rs.tbl_cap = tbl_bytes;
    return 0;
}

void orb_resize_strip_vp6_release(void)
{
    rs_free_bufs();
}

/* ---------------- driver ---------------- */

/* Build the per-chunk lane tables from the golden coefficient arrays.
 * Layout per chunk (512B, 64B-aligned segments):
 *   toff + c*256: off0 u16[64] | off1 u16[64]
 *   tw    + c*256: w0c i16[32] | w0d i16[32] | w1c i16[32] | w1d i16[32]
 * (c -> odd lanes, d -> even lanes: T24a.) Lanes past dw are 0/0 - their
 * gather reads column 0 with weight 0 (s = 0, don't-care output). */
static void rs_build_tables(const int *xofs, const short *ialpha,
                            int dw, int sw, int chunks,
                            uint16_t *toff, int16_t *tw)
{
    for (int c = 0; c < chunks; ++c) {
        uint16_t *o0 = toff + (size_t)c * 128;
        uint16_t *o1 = o0 + 64;
        int16_t *w = tw + (size_t)c * 128;
        for (int l = 0; l < 64; ++l) {
            int dx = c * 64 + l;
            int x0 = 0, x1 = 0, a0 = 0, a1 = 0;
            if (dx < dw) {
                x0 = xofs[dx];
                x1 = (x0 + 1 < sw) ? x0 + 1 : sw - 1;
                a0 = ialpha[2 * dx];
                a1 = ialpha[2 * dx + 1];
            }
            o0[l] = (uint16_t)x0;
            o1[l] = (uint16_t)x1;
            if (l & 1) w[l >> 1] = (int16_t)a0;          /* w0c: odd lanes */
            else       w[32 + (l >> 1)] = (int16_t)a0;   /* w0d: even lanes */
            if (l & 1) w[64 + (l >> 1)] = (int16_t)a1;   /* w1c */
            else       w[96 + (l >> 1)] = (int16_t)a1;   /* w1d */
        }
    }
}

int orb_resize_strip_vp6(xvTileManager *tm, const uint8_t *src, int sw, int sh,
                         uint8_t *dst, int dw, int dh)
{
    if (!tm || !src || !dst || sw < 1 || sh < 1 || dw < 1 || dh < 1) return -1;

    int *xofs = (int *)malloc(sizeof(int) * (size_t)dw);
    short *ialpha = (short *)malloc(sizeof(short) * (size_t)dw * 2);
    int *yofs = (int *)malloc(sizeof(int) * (size_t)dh);
    short *ibeta = (short *)malloc(sizeof(short) * (size_t)dh * 2);
    if (!xofs || !ialpha || !yofs || !ibeta) {
        free(xofs); free(ialpha); free(yofs); free(ibeta);
        return -2;
    }
    orb_resize_precompute_h(NULL, sw, sh, dw, dh, 0, dw, xofs, ialpha);
    orb_resize_precompute_v(NULL, sw, sh, dw, dh, 0, dh, yofs, ibeta);

    const int chunks = (dw + 63) >> 6;
    const int sp = RS_ALIGN64(sw);
    const int dp = RS_ALIGN64(dw);
    const int tbl_bytes = chunks * 512;

    /* size the strip so the whole bank-1 set fits the budget */
    int eff = (ORB_RS_VP6_BANK1_BUDGET - tbl_bytes) / (2 * (sp + dp)) - 1;
    if (eff > sh) eff = sh;
    if (eff < 1) eff = 1;
    const int srows_max = (eff < sh) ? eff + 1 : sh;
    if (rs_alloc_bufs(tm, srows_max * sp, srows_max * dp, tbl_bytes) != 0) {
        free(xofs); free(ialpha); free(yofs); free(ibeta);
        return -2;
    }
    rs_build_tables(xofs, ialpha, dw, sw, chunks,
                    (uint16_t *)(void *)g_rs.tbl,
                    (int16_t *)(void *)(g_rs.tbl + (size_t)chunks * 256));

    const int n_pass = (sh + eff - 1) / eff;
    int dy_cur = 0;                 /* next output row to emit (yofs cursor) */
    int32_t dma[2] = { -1, -1 };
    int pend[2] = { 0, 0 };

    /* pass 0's strip DMA (geometry mirrors the prefetch below) */
    {
        const int qr = (sh < eff) ? sh : eff;
        const int qs = (qr + 1 < sh) ? (qr + 1) : sh;
        dma[0] = xvAddIdmaRequest(tm, g_rs.strip[0], (void *)src,
                                  (size_t)sw, qs, sw, sp, 0);
        pend[0] = 1;
    }

    for (int p = 0; p < n_pass; ++p) {
        const int top = p * eff;
        const int rows = (sh - top < eff) ? (sh - top) : eff;
        const int srows = (rows + 1 < sh - top) ? (rows + 1) : (sh - top);

        /* prefetch pass p+1 into the other strip (overlap), then wait p */
        if (p + 1 < n_pass) {
            const int q = p + 1, qt = q * eff;
            const int qr = (sh - qt < eff) ? (sh - qt) : eff;
            const int qs = (qr + 1 < sh - qt) ? (qr + 1) : (sh - qt);
            dma[q & 1] = xvAddIdmaRequest(tm, g_rs.strip[q & 1],
                (void *)(src + (size_t)qt * sw), (size_t)sw, qs, sw, sp, 0);
            pend[q & 1] = 1;
        }
        if (pend[p & 1]) {
            if (dma[p & 1] < 0) break;               /* DMA failed: bail out */
            xvWaitForiDMA(tm, dma[p & 1]);
            pend[p & 1] = 0;
        }

        /* horizontal: every strip row (incl. the halo row) -> Hm/Lm planes */
        for (int r = 0; r < srows; ++r) {
            rs_h_row(g_rs.strip[p & 1] + (size_t)r * sp,
                     g_rs.hm + (size_t)r * dp, g_rs.lm + (size_t)r * dp,
                     (const uint16_t *)(void *)g_rs.tbl,
                     (const int16_t *)(void *)(g_rs.tbl + (size_t)chunks * 256),
                     chunks);
        }

        /* vertical: output rows whose yofs[dy] falls in [top, top+rows) */
        while (dy_cur < dh && yofs[dy_cur] < top + rows) {
            const int y0 = yofs[dy_cur];
            const int y1 = (y0 + 1 < sh) ? y0 + 1 : sh - 1;
            rs_v_row(g_rs.hm, g_rs.lm, y0 - top, y1 - top,
                     ibeta[2 * dy_cur], ibeta[2 * dy_cur + 1],
                     dp, dw, dst + (size_t)dy_cur * dw);
            ++dy_cur;
        }
    }

    free(xofs); free(ialpha); free(yofs); free(ibeta);
    return (dy_cur == dh) ? 0 : -2;   /* every output row emitted */
}
