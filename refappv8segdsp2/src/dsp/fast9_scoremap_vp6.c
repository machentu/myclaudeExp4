/*
 * fast9_scoremap_vp6.c - VP6 strip driver + IVP pixel kernel for the FAST-9
 * score map.
 *
 * Driver (mirrors fast9_vp6.c): strips of `strip_h` map rows, top-to-bottom,
 * each spanning the FULL frame width (the map is dense over the whole frame).
 * The strip's source region (strip rows +/-3 for the FAST-9 ring, full width)
 * is DMAed into one of two SRAM tiles (XV_MEM_BANK_COLOR_0/1) while the
 * previous strip's kernel runs; map bytes go straight to DRAM in raster
 * order.
 *
 * Kernel bit-exactness with fast9_build_scoremap (golden, fast9_scoremap.c):
 *
 * 1. Cardinal 4-point reject omitted in the vector path. The reject is an
 *    early-out only: any 9-of-16 arc contains >= 2 cardinal points, so
 *    "hi < 2 && lo < 2 at t_floor" implies every arc's min contrast is
 *    <= t_floor, i.e. S <= t_floor, which the vector tree also computes for
 *    those lanes -> identical map bytes either way.
 *
 * 2. Tree on clamped byte lanes. The golden tree runs on d = ring - p (can be
 *    negative); the vector tree runs on clamp(d, 0). clamp is monotone
 *    non-decreasing and commutes with min/max/max-of-arcs, so
 *      tree(clamp(d)) = clamp(tree(d)) = max(S, 0),
 *    and both polarities clamp before the tree give max(S_bright, S_dark, 0).
 *    All lane values are then plain bytes in [0, 255].
 *
 * 3. Saturation-free clamp. u = max(ring - p, 0) per lane as
 *      MOVUT(SUBU(v, c), 0, LTU(c, v))
 *    SUBU wraps mod 256, but the select keeps the wrapped difference only
 *    when ring > p (true difference in [1, 255], no wrap); otherwise 0.
 *    Dark polarity mirrors it with LTU(v, c).
 *
 * 4. Threshold + emit. For t_floor >= 0:  S > t_floor  <=>  max(S,0) > t_floor.
 *    Stored byte = wrapping ADD(S', 0xFF) = S'-1 on the true path (S' >= 1
 *    there, so no true wraparound; S-1 <= 254 never collides with the 0xFF
 *    sentinel), 0xFF (FAST9_MAP_NONE) on the false path.
 *
 * 5. Vector/tail split. Full 64-lane chunks run while x + 63 <= w - 4, so
 *    every ring load reads at most row + w - 1 (in the tile, which spans the
 *    full width). The remaining <= 63 columns use the golden scalar loop
 *    verbatim (reject included). Rows outside [3, h-4] keep the NONE the
 *    driver memset over the whole map, exactly like the golden.
 *
 * Every IVP op used here (lane widths, wrapping ADD/SUB, MOVUT/LTU argument
 * order, MINU/MAXU, LA/LAV/SA/SAPOS protocols) is pinned by
 * test_ivp_probe.cpp.
 *
 * Build: C++ under the Xtensa cstub (LANGUAGE CXX).
 */
#include "fast9_scoremap_vp6.h"
#include "fast9_scoremap.h"

#ifdef __XTENSA__
#include <xtensa/tie/xt_ivpn.h>          /* real fy01_202210 core (found via --xtensa-system) */
#else
#include "xtensa/tie/Xm_fy01_202210.h"   /* cstub simulation (host build, CSTUB_PATH -I) */
#endif

#include <stdlib.h>
#include <string.h>

#ifndef ALIGN64
#  ifdef _MSC_VER
#    define ALIGN64 __declspec(align(64))
#  else
#    define ALIGN64 __attribute__((aligned(64)))
#  endif
#endif

/* ===================================================================== *
 *  IVP kernel
 * ===================================================================== */

/* fast9_circle order (dx,dy) of the 16 ring pixels around (x,y). */
static const int fs_ring_xy[16][2] = {
    { 0,-3},{ 1,-3},{ 2,-2},{ 3,-1},{ 3, 0},{ 3, 1},{ 2, 2},{ 1, 3},
    { 0, 3},{-1, 3},{-2, 2},{-3, 1},{-3, 0},{-3,-1},{-2,-2},{-1,-3}
};

/* Elementwise image of fast9_tree_score on 64 byte lanes: the tree of
 * fast9_scoremap.c (d2[24]/m2[22]/m4[20]/core[16] with d2[i] = d[i & 15])
 * is purely elementwise, so the same index arithmetic on 16 lane vectors
 * computes max-over-arcs(min) for all 64 pixels at once. */
static inline xb_vec2Nx8U fs_tree_ivp(const xb_vec2Nx8U u[16])
{
    xb_vec2Nx8U m2[22], m4[20], core[16];
    for (int j = 0; j < 22; ++j)
        m2[j] = IVP_MINU2NX8(u[j & 15], u[(j + 1) & 15]);
    for (int j = 0; j < 20; ++j)
        m4[j] = IVP_MINU2NX8(m2[j], m2[j + 2]);
    for (int j = 0; j < 16; ++j)
        core[j] = IVP_MINU2NX8(m4[j], m4[j + 4]);

    xb_vec2Nx8U S = IVP_MINU2NX8(core[1], IVP_MAXU2NX8(u[0], u[9]));
    for (int s = 2; s < 16; s += 2)
        S = IVP_MAXU2NX8(S,
              IVP_MINU2NX8(core[s + 1], IVP_MAXU2NX8(u[s], u[(s + 9) & 15])));
    return S;
}

/* verbatim copy of fast9_scoremap.c::fast9_tree_score (scalar tail) */
#define FS_MIN(a, b) (((a) < (b)) ? (a) : (b))
#define FS_MAX(a, b) (((a) > (b)) ? (a) : (b))
static int fs_tree_score_scalar(const int d[16])
{
    int d2[24];
    for (int i = 0; i < 24; ++i) d2[i] = d[i & 15];

    int m2[22];
    for (int j = 0; j < 22; ++j) m2[j] = FS_MIN(d2[j], d2[j + 1]);
    int m4[20];
    for (int j = 0; j < 20; ++j) m4[j] = FS_MIN(m2[j], m2[j + 2]);
    int core[16];
    for (int j = 0; j < 16; ++j) core[j] = FS_MIN(m4[j], m4[j + 4]);

    int S = FS_MIN(core[1], FS_MAX(d2[0], d2[9]));
    for (int s = 2; s < 16; s += 2) {
        int pr = FS_MIN(core[s + 1], FS_MAX(d2[s], d2[s + 9]));
        if (pr > S) S = pr;
    }
    return S;
}

void fast9_scoremap_kernel_ivp(const uint8_t *src_data, int src_pitch,
                               int origin_x, int origin_y,
                               int frame_w, int frame_h,
                               int y0, int y1, int t_floor,
                               uint8_t *map, int map_stride)
{
    int ya = y0 < 3 ? 3 : y0;                 /* ring-legal row band */
    int yb = y1 > frame_h - 3 ? frame_h - 3 : y1;
    if (ya >= yb) return;                     /* rows [ya, yb) <= h-4 */

    /* lane constants: t_floor broadcast + 0xFF (sentinel / S'-1 decrement) */
    ALIGN64 uint8_t tfb[64], ntb[64];
    memset(tfb, (uint8_t)t_floor, sizeof tfb);
    memset(ntb, 0xFF, sizeof ntb);
    xb_vec2Nx8U vtf   = IVP_LV2NX8U_X((const xb_vec2Nx8U *)(const void *)tfb, 0);
    xb_vec2Nx8U vnone = IVP_LV2NX8U_X((const xb_vec2Nx8U *)(const void *)ntb, 0);
    xb_vec2Nx8U vzero = IVP_SUB2NX8U(vtf, vtf);   /* all-zero lanes (wraps) */

    for (int y = ya; y < yb; ++y) {
        const uint8_t *rrow[7];               /* frame rows y-3 .. y+3 */
        for (int k = 0; k < 7; ++k)
            rrow[k] = src_data + (size_t)(y - 3 + k - origin_y) * src_pitch;
        const uint8_t *rm3 = rrow[0], *rm2 = rrow[1], *rm1 = rrow[2];
        const uint8_t *r0  = rrow[3], *rp1 = rrow[4], *rp2 = rrow[5];
        const uint8_t *rp3 = rrow[6];
        uint8_t *mrow = map + (size_t)y * map_stride;

        int x = 3;
        if (x + 63 <= frame_w - 4) {
            /* 17 unaligned streaming loads: 16 ring streams + the centre.
             * Each stream advances 64 bytes per chunk (LA/LAV protocol). */
            const xb_vec2Nx8U *lp[17];
            valign lv[17];
            for (int i = 0; i < 16; ++i) {
                lp[i] = (const xb_vec2Nx8U *)(const void *)
                        (rrow[3 + fs_ring_xy[i][1]] + (x - origin_x) + fs_ring_xy[i][0]);
                lv[i] = IVP_LA2NX8U_PP(lp[i]);
            }
            lp[16] = (const xb_vec2Nx8U *)(const void *)(r0 + (x - origin_x));
            lv[16] = IVP_LA2NX8U_PP(lp[16]);

            /* unaligned streaming store into the map row */
            xb_vec2Nx8U *mp = (xb_vec2Nx8U *)(void *)(mrow + x);
            valign sa;

            for (; x + 63 <= frame_w - 4; x += 64) {
                xb_vec2Nx8U vc;
                IVP_LAV2NX8U_XP(vc, lv[16], lp[16], 64);

                xb_vec2Nx8U u[16], e[16];
                for (int i = 0; i < 16; ++i) {
                    xb_vec2Nx8U vi;
                    IVP_LAV2NX8U_XP(vi, lv[i], lp[i], 64);
                    /* bright: max(ring-p,0); dark: max(p-ring,0) - no sat */
                    u[i] = IVP_MOV2NX8UT(IVP_SUB2NX8U(vi, vc), vzero,
                                         IVP_LTU2NX8(vc, vi));
                    e[i] = IVP_MOV2NX8UT(IVP_SUB2NX8U(vc, vi), vzero,
                                         IVP_LTU2NX8(vi, vc));
                }

                xb_vec2Nx8U S = IVP_MAXU2NX8(fs_tree_ivp(u), fs_tree_ivp(e));
                /* S' > t_floor ? S'-1 (wrapping) : NONE */
                xb_vec2Nx8U out = IVP_MOV2NX8UT(IVP_ADD2NX8U(S, vnone), vnone,
                                                IVP_LTU2NX8(vtf, S));
                IVP_SA2NX8U_IP(out, sa, mp);
            }
            IVP_SAPOS2NX8U_FP(sa, mp);        /* commit exactly 64*n chunks */
        }

        /* scalar tail: golden loop verbatim (columns the chunks didn't cover) */
        for (; x <= frame_w - 4; ++x) {
            int tx = x - origin_x;
            int p = r0[tx];

            int hi = 0, lo = 0;
            if (rm3[tx] > p + t_floor) ++hi; else if (rm3[tx] < p - t_floor) ++lo;
            if (r0[tx + 3] > p + t_floor) ++hi; else if (r0[tx + 3] < p - t_floor) ++lo;
            if (rp3[tx] > p + t_floor) ++hi; else if (rp3[tx] < p - t_floor) ++lo;
            if (r0[tx - 3] > p + t_floor) ++hi; else if (r0[tx - 3] < p - t_floor) ++lo;
            if (hi < 2 && lo < 2) continue;

            int d[16];
            d[0]  = (int)rm3[tx]     - p;
            d[1]  = (int)rm3[tx + 1] - p;
            d[2]  = (int)rm2[tx + 2] - p;
            d[3]  = (int)rm1[tx + 3] - p;
            d[4]  = (int)r0[tx + 3]  - p;
            d[5]  = (int)rp1[tx + 3] - p;
            d[6]  = (int)rp2[tx + 2] - p;
            d[7]  = (int)rp3[tx + 1] - p;
            d[8]  = (int)rp3[tx]     - p;
            d[9]  = (int)rp3[tx - 1] - p;
            d[10] = (int)rp2[tx - 2] - p;
            d[11] = (int)rp1[tx - 3] - p;
            d[12] = (int)r0[tx - 3]  - p;
            d[13] = (int)rm1[tx - 3] - p;
            d[14] = (int)rm2[tx - 2] - p;
            d[15] = (int)rm3[tx - 1] - p;

            int S = fs_tree_score_scalar(d);           /* bright polarity */
            int e[16];
            for (int i = 0; i < 16; ++i) e[i] = -d[i];
            int Sd = fs_tree_score_scalar(e);          /* dark polarity */
            if (Sd > S) S = Sd;

            mrow[x] = (S > t_floor) ? (uint8_t)(S - 1) : FAST9_MAP_NONE;
        }
    }
}

/* ===================================================================== *
 *  VP6 strip driver (whole-frame strip sweep, persistent buffers)
 * ===================================================================== */

#include "frame_strip_vp6.h"

/* persistent ping/pong strip pair (allocated once, reused across calls) */
static fs_bufs fs_state;

int fast9_scoremap_vp6_build(xvTileManager *tm, xvFrame *src_frame,
                             int src_w, int src_h, int t_floor,
                             uint8_t *map, int map_stride,
                             const Fast9ScoremapVp6Config *cfg)
{
    Fast9ScoremapVp6Config def = FAST9_SCOREMAP_VP6_DEFAULT_CONFIG;
    Fast9ScoremapVp6Config c = cfg ? *cfg : def;

    const int SH  = c.strip_h;
    const int MSW = c.max_src_w;
    const int MSH = c.max_src_h;

    if (!tm || !src_frame || !map) return -1;
    if (SH <= 0 || MSW <= 0 || MSH <= 0) return -1;
    if (t_floor < 0 || t_floor > 254) return -1;   /* byte-lane compare */
    if (src_w < 1 || src_h < 1 || map_stride < src_w) return -1;
    if (MSW < src_w) return -1;                    /* strips span the width */
    if (MSH < SH + 2 * FAST9_SCOREMAP_HALO) return -1;

    /* whole-map NONE first, exactly like the golden (borders + non-corners) */
    memset(map, FAST9_MAP_NONE, (size_t)map_stride * (size_t)src_h);
    if (src_w < 7 || src_h < 7) return 0;   /* no pixel has a full ring */

    /* strips: assigned rows [top, top+rows) + FAST-9 ring halo (+/-3), the
     * DMA (raw 2D iDMA, ping/pong) covering [top-3, top+rows-1+3] clamped */
    if (fs_alloc(&fs_state, tm, FS_ALIGN64(MSW) * MSH) != 0) return -2;

    fs_sweep sw;
    fs_sweep_begin(&sw, &fs_state, src_frame, src_w, src_h,
                   FAST9_SCOREMAP_HALO);

    fs_view v;
    while (fs_sweep_next(&sw, &v)) {
        /* score the pass's rows (full frame width); map bytes -> DRAM */
        fast9_scoremap_kernel_ivp(v.data, v.pitch,
                                  0, v.srow0, src_w, src_h,
                                  v.top, v.top + v.rows, t_floor,
                                  map, map_stride);
    }
    return 0;
}

void fast9_scoremap_vp6_release(void)
{
    fs_free(&fs_state);
}
