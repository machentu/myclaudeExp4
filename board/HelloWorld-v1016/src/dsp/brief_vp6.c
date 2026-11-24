/*
 * brief_vp6.c - VP6 whole-frame strip rotated-BRIEF driver (P6 TileManager).
 *
 * Pipeline (full-width strip sweep, ping/pong double-buffered - the
 * ExtractImage.cpp / TileMemMgr.cpp pattern, see frame_strip_vp6.h):
 *
 *   schedule strip DMA (pass p+1)  ──┐  (overlap: raw 2D iDMA of next strip)
 *   xvWaitForiDMA (pass p)         ──┤  (strip = assigned rows +/-19 halo)
 *   for each keypoint assigned to  ──┤  pass p: kernel gathers straight from
 *   ... pass p                        the SRAM strip (no per-keypoint DMA)
 *
 * Per keypoint there is NO patch DMA any more (that was the dominant
 * per-descriptor overhead of the previous form): the frame streams through
 * two resident SRAM strip buffers allocated once from the tile manager's two
 * memory banks, and every keypoint whose clamped centre row falls in the
 * pass computes its descriptor gathering from the strip. The kernel sees
 * (strip data, strip pitch, origin = strip's first frame row) - pure
 * difference-based indexing on a byte-identical copy of the frame rows, so
 * the 32-byte descriptors are bit-exact with the patch-DMA form and with
 * ngd_compute_descriptor (verified 3-way by test_brief_vp6.c).
 *
 * The kernel is the IVP intrinsic form (gather pixel access, see
 * brief_kernel_ivp below). Define BRIEF_NO_IVP to fall back to the portable
 * kernel.
 */
#include "brief_vp6.h"
#include "frame_strip_vp6.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

/* IVP pixel kernel: ON by default (validated bit-exact against brief_full by
 * test_brief_vp6). Define BRIEF_NO_IVP to fall back to the portable kernel. */
#ifndef BRIEF_NO_IVP
#define BRIEF_USE_IVP_INTRIN 1
#endif

/* Hardware-round tag: bump on every kernel change that goes to the DSP sim,
 * so a pasted cycle table always maps to the exact code (printed first line
 * of brief_vp6_timing_report; snapshot dirs keep the sources per tag):
 *   R4b = pidx dedup memo          (offsets 68,943 cy/kp, TOTAL 91.71M)
 *   R5  = loop2 IVP vectorised     (offsets 61,662 cy/kp, TOTAL 85.93M)
 *   R6  = fused MULANX16UPACKL     (loop2 multiply: 3 ops + Nx48 -> 1 Nx16 op)
 */
#define BRIEF_VP6_ROUND "R6"

#ifndef BRIEF_PI
#define BRIEF_PI 3.14159265358979323846
#endif

#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>      /* _BitScanReverse (bf_clz) */
#endif

/* ---- per-sub-stage cycle profiling (mirrors the dsp_orb_vp6.c timing;
 * the enum/counters live in brief_vp6.h so the reporting harness can clear
 * them). Host numbers are the stage mix only - cstub IVP is emulated C++,
 * so host ratios are NOT DSP ratios. */
unsigned long long brief_vp6_cycles[BRIEF_VP6_T_COUNT];
unsigned long long brief_vp6_kps;

#define BRIEF_VP6_TIMING 1
#ifdef BRIEF_VP6_TIMING
#include <stdio.h>
#ifdef __XTENSA__
/* DSP: rsr.ccount - the simulator's cycle counter (XCHAL_HAVE_CCOUNT=1). */
typedef unsigned int brief_tick_t;
static brief_tick_t brief_tick(void)
{
    unsigned int c;
    __asm__ volatile ("rsr.ccount %0" : "=a" (c));
    return c;
}
#else
/* Host: wall clock (us), for the stage mix on real data only. */
#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif
typedef unsigned long long brief_tick_t;
static brief_tick_t brief_tick(void)
{
#ifdef _WIN32
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    return (brief_tick_t)li.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (brief_tick_t)ts.tv_sec * 1000000000ull + (brief_tick_t)ts.tv_nsec;
#endif
}
static double brief_ticks_to_us(brief_tick_t t)
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
#define BRIEF_TMARK(v)     do { (v) = brief_tick(); } while (0)
#define BRIEF_TACC(v, slot) \
    do { brief_vp6_cycles[slot] += brief_tick() - (brief_tick_t)(v); } while (0)
#else
#define BRIEF_TMARK(v)     do { } while (0)
#define BRIEF_TACC(v, slot) do { } while (0)
#endif

#ifdef BRIEF_USE_IVP_INTRIN
#ifdef __XTENSA__
#include <xtensa/tie/xt_ivpn.h>          /* real fy01_202210 core (found via --xtensa-system) */
#else
#include "xtensa/tie/Xm_fy01_202210.h"   /* cstub simulation (host build, CSTUB_PATH -I) */
#endif

/*
 * Soft-float elimination (the DSP core has no FPU: every float op is a
 * soft-float library call, and the kernel's stack/.rodata working set sits
 * in uncached DRAM). Everything except the per-keypoint angle multiply and
 * the two double cos/sin is replaced by EXACT integer emulation on the IEEE
 * binary32 bit patterns:
 *
 *   bf_mul_p(p, x)        fl((float)p * x)      |p| <= 16
 *   bf_add(x, y)          fl(x + y)             (x - y = add with y's sign
 *                                              bit flipped)
 *   bf_round_even(s)      the golden's round-half-to-even, straight from
 *                         the result's bit pattern (same result as
 *                         brief_core.c's trunc+residual helper and as the
 *                         golden's (int)rint((double)f))
 *
 * Each helper implements IEEE-754 round-nearest-even for this kernel's
 * PROVEN operand domain, so every result bit matches the golden's float ops:
 *   - ar = fl(angle * (float)(pi/180)), |angle| <= 360; a = (float)cos(ar),
 *     b = (float)sin(ar). cos/sin are evaluated at the DOUBLE of a float, so
 *     they never land closer than ~4.4e-8 (the distance from the nearest
 *     float to an odd multiple of pi/2) to a zero: a, b are normal with
 *     |.| <= 1, except b == 0 exactly when angle == 0.
 *   - table entries pa[p] = fl(p*a), pb[p] = fl(p*b) are 0 or normal with
 *     magnitude in [16*4.4e-8, 16] ~= [7e-7, 16].
 *   - a pair sum/diff of two table entries is an exact multiple of
 *     ulp(7e-7) ~= 6e-14, hence 0 or normal; nothing can reach NaN/Inf
 *     (|result| <= 32) or overflow the exponent field.
 * So the emulation never has to produce or consume a denormal - the one case
 * the integer forms below do not implement.
 */
static inline int bf_clz(uint32_t v)                 /* v != 0 */
{
#if defined(_MSC_VER) && !defined(__clang__)
    unsigned long i;
    _BitScanReverse(&i, v);
    return 31 - (int)i;
#elif defined(__GNUC__)
    return __builtin_clz(v);
#else
    int n = 0;
    if (!(v >> 16)) { n += 16; v <<= 16; }
    if (!(v >> 24)) { n += 8;  v <<= 8; }
    if (!(v >> 28)) { n += 4;  v <<= 4; }
    if (!(v >> 30)) { n += 2;  v <<= 2; }
    if (!(v >> 31)) { n += 1; }
    return n;
#endif
}

/* fl((float)p * x), |p| <= 16, x normal-or-zero; product normal-or-zero
 * (domain proof above). mantissa(p) is exact (|p| < 2^24), so the product
 * mantissa is p * mant(x) < 16 * 2^24 = 2^28: normalize by a 0..4 shift and
 * round half-to-even on the shifted-out bits. */
static inline uint32_t bf_mul_p(int p, uint32_t x)
{
    if (p == 0 || (x & 0x7fffffffu) == 0) return 0;
    const uint32_t s = (uint32_t)((p < 0) ^ (int)(x >> 31)) << 31;
    const uint32_t m0 = (x & 0x007fffffu) | 0x00800000u;
    uint32_t m = (uint32_t)(p < 0 ? -p : p) * m0;    /* < 2^28 */
    int h = 31 - bf_clz(m);                          /* top bit, 23..27 */
    const int sh = h - 23;
    uint32_t mant = m >> sh;
    if (sh) {
        const uint32_t rem = m & ((1u << sh) - 1u);
        const uint32_t half = 1u << (sh - 1);
        if (rem > half || (rem == half && (mant & 1u)))
            if (++mant == (1u << 24)) { mant = 0x00800000u; h += 1; }
    }
    const uint32_t e = (uint32_t)(((int)(x >> 23) & 0xff) + h - 23);
    return s | (e << 23) | (mant & 0x007fffffu);
}

/* Round a normal-or-zero binary32 pattern (|v| <= 32, no NaN/Inf) to the
 * nearest integer, halves to even - identical to the golden's
 * brief_round_even_inline / (int)rint((double)f). */
static inline int bf_round_even(uint32_t s)
{
    const uint32_t a = s & 0x7fffffffu;
    if (a == 0) return 0;
    const int e = (int)(a >> 23) - 127;              /* v = 1.f * 2^e, e <= 5 */
    if (e < -1) return 0;                            /* |v| < 0.5 */
    const uint32_t mant = (a & 0x007fffffu) | 0x00800000u;
    if (e == -1)                                     /* |v| in [0.5, 1) */
        return (mant > 0x00800000u) ? ((int)(s >> 31) ? -1 : 1) : 0;
    int ip = (int)(mant >> (23 - e));
    const uint32_t frac = mant & ((1u << (23 - e)) - 1u);
    const uint32_t half = 1u << (22 - e);
    if (frac > half || (frac == half && (ip & 1))) ++ip;
    return (int)(s >> 31) ? -ip : ip;
}

/* round-half-even-to-integer of fl(x + y), FUSED: the float24 rounding of
 * the sum and the integer rounding both run on the adder's internal (sign,
 * exponent, 27-bit significand, sticky) state, so the result is identical to
 * bf_round_even(bf_add(x, y)) by construction - same two roundings, applied
 * in the same order, without packing the sum into a float bit pattern and
 * decoding it again. Operands normal-or-zero, result normal-or-zero (domain
 * proof above); the cases that would need extra care cannot occur:
 *   - d >= 28 (y < ulp(x)/16): fl(x+y) = x exactly, so round the bits of x;
 *   - d >= 4 (sticky possible) cannot cancel below bit 25, so the
 *     normalizing left shift is at most 1 and k never overflows;
 *   - d <= 3 keeps everything exact (k = 0), so deep cancellation shifts
 *     only exact bits. */
static inline int bf_addr(uint32_t x, uint32_t y)
{
    const uint32_t ax = x & 0x7fffffffu, ay = y & 0x7fffffffu;
    if (ax == 0) return bf_round_even(y);               /* +-0 + y = y */
    if (ay == 0) return bf_round_even(x);
    if (ax < ay) { uint32_t t = x; x = y; y = t; }      /* |x| >= |y| (normals) */
    const uint32_t sx = x >> 31;
    const int same = (sx == (y >> 31));
    const uint32_t mx24 = (x & 0x007fffffu) | 0x00800000u;
    const uint32_t my24 = (y & 0x007fffffu) | 0x00800000u;
    const int d = (int)((x >> 23) & 0xff) - (int)((y >> 23) & 0xff);
    if (d >= 28) return bf_round_even(x);               /* y < ulp(x)/16 */
    uint32_t m, k;
    int sh;
    if (d <= 3) {
        m = same ? ((mx24 << 3) + (my24 << (3 - d)))
                 : ((mx24 << 3) - (my24 << (3 - d)));
        k = 0; sh = 0;
    } else {
        const uint32_t shv = (uint32_t)(d - 3);         /* 1..24 */
        const uint32_t myg = my24 >> shv;
        k = my24 & ((1u << shv) - 1u);                  /* sticky tail */
        m = same ? ((mx24 << 3) + myg) : ((mx24 << 3) - myg);
        sh = (int)shv;
        if (!same && k != 0) {                          /* borrow: exact is */
            m -= 1;                                     /* (m-1) + (2^sh-k)/2^sh */
            k = (1u << sh) - k;
        }
    }
    int e = (int)((x >> 23) & 0xff);
    if (m >= (1u << 27)) {                              /* carry: 28 significant */
        k = (k << 1) | (m & 1u);                        /* bits, drop LSB into */
        sh += 1;                                        /* the sticky tail    */
        m >>= 1;
        e += 1;
    } else if (m < (1u << 26)) {
        if (m == 0) return 0;                           /* exact cancellation */
        const int s = 26 - (31 - bf_clz(m));
        m <<= s;                                        /* k == 0 whenever s > 1 */
        k <<= s;                                        /* (d >= 4 can't cancel  */
        sh += s;                                        /*  below bit 25)       */
        e -= s;
    }
    uint32_t q = m >> 3;                                /* 24-bit significand */
    const uint32_t rem = ((m & 7u) << sh) + k;
    const uint32_t half = 1u << (sh + 2);
    if (rem > half || (rem == half && (q & 1u)))
        if (++q == (1u << 24)) { q = 0x00800000u; e += 1; }
    /* integer round-half-even of q * 2^(e-150) (== bf_round_even of the
     * packed pattern (sx, e, q)): e-127 in [-32,5] in this domain */
    const int ef = e - 127;
    int r;
    if (ef < -1) r = 0;                                 /* |v| < 0.5 */
    else if (ef == -1) r = (q > 0x00800000u);           /* |v| in [0.5, 1) */
    else {
        const int s2 = 23 - ef;                         /* 18..23 */
        r = (int)(q >> s2);
        const uint32_t fr = q & ((1u << s2) - 1u);
        const uint32_t hf = 1u << (s2 - 1);
        if (fr > hf || (fr == hf && (r & 1))) ++r;
    }
    return (int)sx ? -r : r;
}

/* persistent bank-1 SRAM scratch: the per-keypoint working set (gather
 * offsets, rotation tables, 64B staging buffers, pattern lane indices).
 * Everything the pair loop touches per keypoint would otherwise live on the
 * stack / in .rodata - uncached DRAM on the DSP. Fixed layout (base is
 * 64B-aligned from xvAllocateBuffer, so every vector user below is
 * 64B-aligned too): */
#define BRIEF_WS_OFF1   0        /* u16[256] gather offsets, pair stream 1 */
#define BRIEF_WS_OFF2   512      /* u16[256] gather offsets, pair stream 2 */
#define BRIEF_WS_BUF    1024     /* u8[64]  bit-lane staging               */
#define BRIEF_WS_ONE    1088     /* u8[64]  ones (LTU true lanes)          */
#define BRIEF_WS_PA     1152     /* u32[33] fl(p*cos) table (132B!)        */
#define BRIEF_WS_PB     1344     /* u32[33] fl(p*sin) table (132B!)        */
#define BRIEF_WS_PIX    1536     /* u8[512] pattern x+16 lane indices      */
#define BRIEF_WS_PIY    2048     /* u8[512] pattern y+16 lane indices      */
#define BRIEF_WS_PIDXE  2560     /* u16[256] = 2*pidx[2i]: byte offsets into  */
                                 /* the u16 rr16/cc16 planes for the EVEN    */
                                 /* points. Premultiplied x2 so every gather */
                                 /* element address is halfword-aligned (the */
                                 /* HW gather unit NOOPs unaligned elements, */
                                 /* which the cstub does not model).         */
#define BRIEF_WS_PIDXO  3072     /* u16[256] = 2*pidx[2i+1] (odd points)     */
#define BRIEF_WS_RR16   3584     /* u16[512] rotated r + 64, at first points */
#define BRIEF_WS_CC16   4608     /* u16[512] rotated c + 64, at first points */
#define BRIEF_WS_BYTES  5632
/* compile-time no-overlap proof (each rotation table is 33*4 = 132 bytes,
 * NOT 128 - an overlap silently corrupts the pattern and crashes far away) */
typedef char brief_ws_layout_ok[(BRIEF_WS_PA + 132 <= BRIEF_WS_PB
                              && BRIEF_WS_PB + 132 <= BRIEF_WS_PIX
                              && BRIEF_WS_PIX + 512 <= BRIEF_WS_PIY
                              && BRIEF_WS_PIY + 512 <= BRIEF_WS_PIDXE
                              && BRIEF_WS_PIDXE + 512 <= BRIEF_WS_PIDXO
                              && BRIEF_WS_PIDXO + 512 <= BRIEF_WS_RR16
                              && BRIEF_WS_RR16 + 1024 <= BRIEF_WS_CC16
                              && BRIEF_WS_CC16 + 1024 <= BRIEF_WS_BYTES) ? 1 : -1];
static struct { xvTileManager *tm; uint8_t *buf; } brief_ws;
static int brief_pidx_ok;   /* pidx built for the CURRENT brief_ws buffer */

static void brief_ws_free(void)
{
    if (brief_ws.tm && brief_ws.buf) xvFreeBuffer(brief_ws.tm, brief_ws.buf);
    brief_ws.tm = NULL;
    brief_ws.buf = NULL;
}

/*
 * IVP form of brief_kernel (same signature, same 32-byte output).
 *
 * 1. Scalar precompute of the 512 patch-relative sample offsets: per pattern
 *    pair, rotate with the bf_* integer emulation (bit-identical to the
 *    golden's float ops - see the domain proof above) at the 355 distinct
 *    coordinates only (dedup precomputed in brief_vp6_process), then clamp
 *    and linearise to u16 byte offsets VECTORISED (halfword gathers through
 *    the parity-split pidxe/pidxo tables + biased MINU/MAXU clamps + a
 *    MULUU/PACKL pitch product, all mod 2^16 - lane-exact vs the scalar
 *    form, test_ivp_probe T28/T29). Only the bf_addr rotation stays scalar
 *    (it depends on the per-keypoint angle); offsets stay u16 (strip <= 62
 *    rows at pitch <= 640 -> max offset 61*640+639 = 39679 < 2^16).
 *
 * 2. Four 64-pair chunks: 4x GATHERANX8U + 2x GATHERD2NX8U rebuild the 64
 *    t0 and t1 pixels as 64-lane byte vectors (test_ivp_probe T21: second
 *    gsr argument -> lanes 0-31, so [offs+0 | offs+32] is lane order);
 *    LTU + MOVUT select 64 0/1 lanes; one 64-byte store lands them on the
 *    stack; a scalar fold packs each group of 8 lanes into one descriptor
 *    byte (golden bit order: bit b of byte i = pair i*8+b = lane i*8+b).
 *    GATHERANX8U (byte elements), NOT GATHERANX16U (halfword elements):
 *    the offsets are byte offsets with arbitrary parity, and the real
 *    Gather/Scatter unit NOOPs an element address unaligned to ElemSize
 *    (the cstub does not model that - it reads any address) - halfword
 *    gathers silently return 0 for every odd column on the DSP.
 */
static void brief_kernel_ivp(const uint8_t *src_data, int src_pitch,
                             int origin_x, int origin_y,
                             int frame_w, int frame_h,
                             int cx, int cy, float angle, uint8_t *desc)
{
    /* SRAM working set (brief_ws, set up by brief_vp6_process - the DSP has
     * no D-cache, so stack/.rodata tables would cost a DRAM access for every
     * one of the per-keypoint working-set touches). pix/piy are the 512
     * pattern points pre-expanded to pa/pb lane indices (x+16 / y+16). */
    uint8_t *ws = brief_ws.buf;
    uint16_t *off1 = (uint16_t *)(void *)(ws + BRIEF_WS_OFF1);
    uint16_t *off2 = (uint16_t *)(void *)(ws + BRIEF_WS_OFF2);
    uint32_t *pa = (uint32_t *)(void *)(ws + BRIEF_WS_PA);
    uint32_t *pb = (uint32_t *)(void *)(ws + BRIEF_WS_PB);
    const uint8_t *pix = ws + BRIEF_WS_PIX, *piy = ws + BRIEF_WS_PIY;
    brief_tick_t bt;                       /* sub-stage profiling */

    /* Only real float ops left: the angle multiply and the two double
     * cos/sin (2 transcendental calls per keypoint), exactly like the
     * golden. Their products/sums run through the bf_* integer emulation -
     * bit-identical to the golden's soft-float results (see the domain
     * proof above the helpers). */
    BRIEF_TMARK(bt);
    float ar = angle * (float)(BRIEF_PI / 180.0);
    union { float f; uint32_t u; } ca, cb;
    ca.f = (float)cos(ar);
    cb.f = (float)sin(ar);
    BRIEF_TACC(bt, BRIEF_VP6_T_COSSIN);
    BRIEF_TMARK(bt);
    for (int p = -16; p <= 16; ++p) {
        pa[p + 16] = bf_mul_p(p, ca.u);               /* fl(p*a) */
        pb[p + 16] = bf_mul_p(p, cb.u);               /* fl(p*b) */
    }
    BRIEF_TACC(bt, BRIEF_VP6_T_TABLES);
    BRIEF_TMARK(bt);
    /* 512 sample points but only 355 DISTINCT (x,y) coordinates (the
     * pattern's coords live on a 33x33 grid): compute each distinct point's
     * rotated (r, c) once per keypoint and share it between duplicates. The
     * dedup is precomputed once (brief_vp6_process) as pidx[pt] = the first
     * point with the same coords - stored PARITY-SPLIT and premultiplied x2
     * (pidxe[i] = 2*pidx[2i], pidxo[i] = 2*pidx[2i+1]) so the vector loop
     * below gathers through it directly. Identical inputs give the identical
     * bf_addr result (same rounding path), so this is bit-exact by
     * construction and drops 2x157 bf_addr calls per keypoint. r, c are in
     * [-32, 32]; stored biased +64 as u16 (in [32,96]) so the vector clamps
     * below see only non-negative lanes. */
    const uint16_t *pidxe = (const uint16_t *)(const void *)(ws + BRIEF_WS_PIDXE);
    const uint16_t *pidxo = (const uint16_t *)(const void *)(ws + BRIEF_WS_PIDXO);
    uint16_t *rr16 = (uint16_t *)(void *)(ws + BRIEF_WS_RR16);
    uint16_t *cc16 = (uint16_t *)(void *)(ws + BRIEF_WS_CC16);
    for (int i = 0; i < 256; ++i) {
        if ((pidxe[i] >> 1) == 2 * i) {           /* pidx[2i] == 2i: first  */
            const int pxi = pix[2 * i], pyi = piy[2 * i];
            rr16[2 * i]     = (uint16_t)(bf_addr(pb[pxi], pa[pyi]) + 64);
            cc16[2 * i]     = (uint16_t)(bf_addr(pa[pxi], pb[pyi] ^ 0x80000000u) + 64);
        }
        if ((pidxo[i] >> 1) == 2 * i + 1) {       /* pidx[2i+1] == 2i+1     */
            const int pxi = pix[2 * i + 1], pyi = piy[2 * i + 1];
            rr16[2 * i + 1] = (uint16_t)(bf_addr(pb[pxi], pa[pyi]) + 64);
            cc16[2 * i + 1] = (uint16_t)(bf_addr(pa[pxi], pb[pyi] ^ 0x80000000u) + 64);
        }
    }
    /* Vector loop 2: 512 clamps + linearisations in 16 x 32-lane chunks.
     * Per lane (r' = r+64 plane, BIAS = 1024 keeps every lane non-negative):
     *   yb  = MIN(MAX(r' + (cy-64+BIAS), BIAS), BIAS+h-1) == clamp(cy+r,0,h-1)+BIAS
     *   yo  = yb - (BIAS+origin_y)                    == clamped y - origin_y
     *   xb/xo likewise; origin_x is always 0 so xo = xb - BIAS
     *   off = xo + low16(yo*pitch) == (y-origin_y)*pitch + x  (mod 2^16 - a
     *         negative yo wraps to the same u16 because 65536*pitch == 0
     *         mod 2^16; true offsets <= 61*640+639 < 2^16)
     * Equals the scalar expression lane for lane (test_ivp_probe T29/T30).
     * The multiply+add is the FUSED IVP_MULANX16UPACKL (a += low16(b*c),
     * xsTileLib ORBAngleBRIEF.c's form) - one Nx16 op instead of MULUU's
     * Nx48 product + PACKL + ADD, keeping loop 2 out of the Nx48 register
     * class entirely.
     * Splat semantics: IVP_MOVNX16U_FROM16U loads LANE 0 ONLY; IVP_REPNX16U
     * (v, 0) broadcasts lane 0 to all 32 lanes. */
    {
        enum { BF_BIAS = 1024 };
        const xb_vecNx16U vKy = IVP_REPNX16U(IVP_MOVNX16U_FROM16U((xb_int16U)(cy - 64 + BF_BIAS)), 0);
        const xb_vecNx16U vKx = IVP_REPNX16U(IVP_MOVNX16U_FROM16U((xb_int16U)(cx - 64 + BF_BIAS)), 0);
        const xb_vecNx16U vB0 = IVP_REPNX16U(IVP_MOVNX16U_FROM16U((xb_int16U)BF_BIAS), 0);
        const xb_vecNx16U vBy1 = IVP_REPNX16U(IVP_MOVNX16U_FROM16U((xb_int16U)(BF_BIAS + frame_h - 1)), 0);
        const xb_vecNx16U vBx1 = IVP_REPNX16U(IVP_MOVNX16U_FROM16U((xb_int16U)(BF_BIAS + frame_w - 1)), 0);
        const xb_vecNx16U vBo = IVP_REPNX16U(IVP_MOVNX16U_FROM16U((xb_int16U)(BF_BIAS + origin_y)), 0);
        const xb_vecNx16U vKp = IVP_REPNX16U(IVP_MOVNX16U_FROM16U((xb_int16U)src_pitch), 0);
        const unsigned short *rrp = (const unsigned short *)(const void *)rr16;
        const unsigned short *ccp = (const unsigned short *)(const void *)cc16;
        for (int ch = 0; ch < 8; ++ch) {
            /* even points 2i -> off1, odd points 2i+1 -> off2: the
             * parity-split tables make both halves contiguous, so each
             * 32-lane store lands exactly in the off1/off2 layout the
             * gather stage below reads (off[pt>>1], pt parity selects). */
            const xb_vecNx16U ovE = *(const xb_vecNx16U *)(const void *)(pidxe + ch * 32);
            const xb_vecNx16U ovO = *(const xb_vecNx16U *)(const void *)(pidxo + ch * 32);
            xb_gsr grE = IVP_GATHERANX16U(rrp, ovE);   /* named gsr: the real */
            xb_gsr gcE = IVP_GATHERANX16U(ccp, ovE);   /* HW path expects an  */
            xb_gsr grO = IVP_GATHERANX16U(rrp, ovO);   /* lvalue special reg  */
            xb_gsr gcO = IVP_GATHERANX16U(ccp, ovO);
            xb_vecNx16U vrE = IVP_GATHERDNX16U(grE);
            xb_vecNx16U vcE = IVP_GATHERDNX16U(gcE);
            xb_vecNx16U vrO = IVP_GATHERDNX16U(grO);
            xb_vecNx16U vcO = IVP_GATHERDNX16U(gcO);
            xb_vecNx16U ybE = IVP_MINUNX16U(IVP_MAXUNX16U(IVP_ADDNX16U(vrE, vKy), vB0), vBy1);
            xb_vecNx16U xbE = IVP_MINUNX16U(IVP_MAXUNX16U(IVP_ADDNX16U(vcE, vKx), vB0), vBx1);
            xb_vecNx16U yoE = IVP_SUBNX16U(ybE, vBo);
            xb_vecNx16U offE = IVP_SUBNX16U(xbE, vB0);   /* xo (origin_x = 0)  */
            IVP_MULANX16UPACKL(offE, yoE, vKp);          /* += low16(yo*pitch) */
            xb_vecNx16U ybO = IVP_MINUNX16U(IVP_MAXUNX16U(IVP_ADDNX16U(vrO, vKy), vB0), vBy1);
            xb_vecNx16U xbO = IVP_MINUNX16U(IVP_MAXUNX16U(IVP_ADDNX16U(vcO, vKx), vB0), vBx1);
            xb_vecNx16U yoO = IVP_SUBNX16U(ybO, vBo);
            xb_vecNx16U offO = IVP_SUBNX16U(xbO, vB0);
            IVP_MULANX16UPACKL(offO, yoO, vKp);
            xb_vecNx16U *q1 = (xb_vecNx16U *)(void *)(off1 + ch * 32);
            xb_vecNx16U *q2 = (xb_vecNx16U *)(void *)(off2 + ch * 32);
            IVP_SVNX16U_IP(offE, q1, (int)sizeof(offE));
            IVP_SVNX16U_IP(offO, q2, (int)sizeof(offO));
        }
    }
    BRIEF_TACC(bt, BRIEF_VP6_T_OFFSETS);
    BRIEF_TMARK(bt);

    /* lane constants: 1 (bit set) / 0, via the SRAM 64B buffers */
    uint8_t *one64 = ws + BRIEF_WS_ONE, *buf = ws + BRIEF_WS_BUF;
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
    BRIEF_TACC(bt, BRIEF_VP6_T_GATHER);
}
#endif /* BRIEF_USE_IVP_INTRIN */

/* ---- kernel selection: IVP intrinsic path (default) or the validated
 *      portable brief_kernel from brief_core.c. ---- */
#ifdef BRIEF_USE_IVP_INTRIN
#define BRIEF_KERNEL brief_kernel_ivp
#else
#define BRIEF_KERNEL brief_kernel
#endif

/* Round a float keypoint coord the way ngd_compute_descriptor does ((int)lroundf). */
static inline int brief_round(float v) { return (int)lroundf(v); }

/* persistent ping/pong strip pair (allocated once, reused across calls) */
static fs_bufs brief_fs;

int brief_vp6_process(xvTileManager *tm, xvFrame *src_frame,
                      int src_w, int src_h,
                      const fast9_keypoint *kps, uint8_t *desc, int n_kps,
                      const BriefVp6Config *cfg)
{
    BriefVp6Config def = BRIEF_VP6_DEFAULT_CONFIG;
    BriefVp6Config c = cfg ? *cfg : def;

    const int SR  = c.strip_rows;
    const int MSW = c.max_src_w;
    const int MSH = c.max_src_h;

    if (!tm || !src_frame || !kps || !desc) return -1;
    if (SR < 1 || MSW < 1) return -1;
    if (MSH < SR + 2 * BRIEF_MARGIN) return -1;      /* halo must fit too */
    if (MSW < src_w) return -1;                      /* strips span width */

    if (n_kps <= 0) return 0;

    if (fs_alloc(&brief_fs, tm, FS_ALIGN64(MSW) * MSH) != 0) return -2;

#ifdef BRIEF_USE_IVP_INTRIN
    /* SRAM scratch (bank 1: brief's own strips leave ~16KB there during
     * phase 2 - the resize driver's buffers are already released). */
    if (brief_ws.tm != tm || !brief_ws.buf) {
        brief_ws_free();
        uint8_t *p = (uint8_t *)xvAllocateBuffer(tm, BRIEF_WS_BYTES,
                                                 XV_MEM_BANK_COLOR_1, 64);
        if (!p || p == (void *)(intptr_t)XVTM_ERROR) return -2;
        brief_ws.buf = p;
        brief_ws.tm = tm;
        brief_pidx_ok = 0;                     /* fresh buffer: rebuild pidx */
    }
    /* expand the 512 pattern points to pa/pb lane indices once per call
     * (x+16 / y+16 into u8 - the kernel then reads 1 byte per index instead
     * of 4-byte pattern coords plus an add) */
    {
        uint8_t *pix = brief_ws.buf + BRIEF_WS_PIX, *piy = brief_ws.buf + BRIEF_WS_PIY;
        for (int pair = 0; pair < 256; ++pair) {
            pix[pair * 2]       = (uint8_t)(brief_bit_pattern_31[pair][0] + 16);
            piy[pair * 2]       = (uint8_t)(brief_bit_pattern_31[pair][1] + 16);
            pix[pair * 2 + 1]   = (uint8_t)(brief_bit_pattern_31[pair][2] + 16);
            piy[pair * 2 + 1]   = (uint8_t)(brief_bit_pattern_31[pair][3] + 16);
        }
    }
    /* one-time dedup of the CONST pattern: pidx[pt] = smallest pt' with the
     * same (x, y) (so pidx[pt] == pt marks a first occurrence), then emit it
     * PARITY-SPLIT and premultiplied x2 (pidxe[i] = 2*pidx[2i],
     * pidxo[i] = 2*pidx[2i+1]) - byte offsets into the u16 rr16/cc16 planes
     * that the kernel's halfword gathers consume directly (even by
     * construction, so the HW gather unit never sees an unaligned element).
     * O(512^2/2) compares, run once per workspace allocation - never per
     * keypoint. pidx itself is scratch: the RR16 plane area doubles as its
     * buffer (the kernel overwrites rr16 after this, every keypoint). */
    if (!brief_pidx_ok) {
        uint16_t *pidx = (uint16_t *)(void *)(brief_ws.buf + BRIEF_WS_RR16);
        uint16_t *pidxe = (uint16_t *)(void *)(brief_ws.buf + BRIEF_WS_PIDXE);
        uint16_t *pidxo = (uint16_t *)(void *)(brief_ws.buf + BRIEF_WS_PIDXO);
        const uint8_t *pix = brief_ws.buf + BRIEF_WS_PIX,
                      *piy = brief_ws.buf + BRIEF_WS_PIY;
        for (int i = 0; i < 512; ++i) {
            int j = 0;
            while (j < i && !(pix[j] == pix[i] && piy[j] == piy[i])) ++j;
            pidx[i] = (uint16_t)(j < i ? pidx[j] : i);
        }
        for (int i = 0; i < 256; ++i) {
            pidxe[i] = (uint16_t)(pidx[2 * i] * 2);
            pidxo[i] = (uint16_t)(pidx[2 * i + 1] * 2);
        }
        brief_pidx_ok = 1;
    }
    memset(brief_ws.buf + BRIEF_WS_ONE, 1, 64);
#endif

    fs_sweep sw;
    fs_sweep_begin(&sw, &brief_fs, src_frame, src_w, src_h, BRIEF_MARGIN);

    /* Assign each keypoint to the pass containing its CLAMPED centre row:
     * every rotated sample clamps to within +/-BRIEF_MARGIN of clamp(cy)
     * (the clamp is 1-Lipschitz), and the pass's strip carries a 19-row
     * halo, so all reads land in the strip. */
    fs_view v;
    while (fs_sweep_next(&sw, &v)) {
        for (int k = 0; k < n_kps; ++k) {
            int cy = brief_round(kps[k].y);
            if (cy < 0) cy = 0; else if (cy > src_h - 1) cy = src_h - 1;
            if (cy < v.top || cy >= v.top + v.rows) continue;   /* other pass */
            brief_tick_t bk;
            BRIEF_TMARK(bk);
            BRIEF_KERNEL(v.data, v.pitch, 0, v.srow0,
                         src_w, src_h,
                         brief_round(kps[k].x), brief_round(kps[k].y),
                         kps[k].angle, desc + (size_t)k * 32);
            BRIEF_TACC(bk, BRIEF_VP6_T_KERNEL);
            ++brief_vp6_kps;
        }
    }
    return 0;
}

void brief_vp6_release(void)
{
    fs_free(&brief_fs);
#ifdef BRIEF_USE_IVP_INTRIN
    brief_ws_free();
#endif
}

void brief_vp6_timing_report(void)
{
#ifdef BRIEF_VP6_TIMING
    const unsigned long long nk = brief_vp6_kps ? brief_vp6_kps : 1;
    const unsigned long long sub = brief_vp6_cycles[BRIEF_VP6_T_COSSIN]
                                 + brief_vp6_cycles[BRIEF_VP6_T_TABLES]
                                 + brief_vp6_cycles[BRIEF_VP6_T_OFFSETS]
                                 + brief_vp6_cycles[BRIEF_VP6_T_GATHER];
#ifdef __XTENSA__
    printf("[brief] round    : %s\n", BRIEF_VP6_ROUND);
    printf("[brief] kps      : %llu\n", brief_vp6_kps);
    printf("[brief] kernel   : %llu  (%llu/kp)\n",
           brief_vp6_cycles[BRIEF_VP6_T_KERNEL],
           brief_vp6_cycles[BRIEF_VP6_T_KERNEL] / nk);
    printf("[brief] cos_sin  : %llu  (%llu/kp)\n",
           brief_vp6_cycles[BRIEF_VP6_T_COSSIN],
           brief_vp6_cycles[BRIEF_VP6_T_COSSIN] / nk);
    printf("[brief] tables   : %llu  (%llu/kp)\n",
           brief_vp6_cycles[BRIEF_VP6_T_TABLES],
           brief_vp6_cycles[BRIEF_VP6_T_TABLES] / nk);
    printf("[brief] offsets  : %llu  (%llu/kp)\n",
           brief_vp6_cycles[BRIEF_VP6_T_OFFSETS],
           brief_vp6_cycles[BRIEF_VP6_T_OFFSETS] / nk);
    printf("[brief] gather   : %llu  (%llu/kp)\n",
           brief_vp6_cycles[BRIEF_VP6_T_GATHER],
           brief_vp6_cycles[BRIEF_VP6_T_GATHER] / nk);
    printf("[brief] resid    : %llu  (%llu/kp)\n",
           brief_vp6_cycles[BRIEF_VP6_T_KERNEL] - sub,
           (brief_vp6_cycles[BRIEF_VP6_T_KERNEL] - sub) / nk);
#else
    /* host: ticks -> microseconds; stage mix only, NOT DSP cycles */
    double us[BRIEF_VP6_T_COUNT];
    for (int i = 0; i < BRIEF_VP6_T_COUNT; ++i)
        us[i] = brief_ticks_to_us((brief_tick_t)brief_vp6_cycles[i]);
    const double us_sub = us[BRIEF_VP6_T_COSSIN] + us[BRIEF_VP6_T_TABLES]
                        + us[BRIEF_VP6_T_OFFSETS] + us[BRIEF_VP6_T_GATHER];
    printf("[brief] round    : %s\n", BRIEF_VP6_ROUND);
    printf("[brief] kps      : %llu\n", brief_vp6_kps);
    printf("[brief] kernel   : %10.1f us  (%5.1f us/kp)\n",
           us[BRIEF_VP6_T_KERNEL], us[BRIEF_VP6_T_KERNEL] / (double)nk);
    printf("[brief] cos_sin  : %10.1f us  (%5.1f us/kp)\n",
           us[BRIEF_VP6_T_COSSIN], us[BRIEF_VP6_T_COSSIN] / (double)nk);
    printf("[brief] tables   : %10.1f us  (%5.1f us/kp)\n",
           us[BRIEF_VP6_T_TABLES], us[BRIEF_VP6_T_TABLES] / (double)nk);
    printf("[brief] offsets  : %10.1f us  (%5.1f us/kp)\n",
           us[BRIEF_VP6_T_OFFSETS], us[BRIEF_VP6_T_OFFSETS] / (double)nk);
    printf("[brief] gather   : %10.1f us  (%5.1f us/kp)\n",
           us[BRIEF_VP6_T_GATHER], us[BRIEF_VP6_T_GATHER] / (double)nk);
    printf("[brief] resid    : %10.1f us  (%5.1f us/kp)\n",
           us[BRIEF_VP6_T_KERNEL] - us_sub,
           (us[BRIEF_VP6_T_KERNEL] - us_sub) / (double)nk);
#endif
#endif /* BRIEF_VP6_TIMING */
}
