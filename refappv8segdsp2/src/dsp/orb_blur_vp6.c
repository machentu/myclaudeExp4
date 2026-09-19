/*
 * orb_blur_vp6.c - VP6 tile-based ORB Gaussian blur driver (P6 TileManager /
 *                  raw 2D iDMA).
 *
 * Pipeline (per output tile, raster order, ping/pong double-buffered - the
 * ExtractImage.cpp / TileMemMgr.cpp pattern):
 *
 *   raw iDMA-in  of tile t+1's region ──┐  (overlap: next source strip)
 *   xvWaitForiDMA (tile t's DMA-in)   ──┤
 *   wait out[cur]'s DMA-out (t-2)     ──┤  (reuse safety)
 *   orb_blur_kernel(in[cur]->out[cur])──┤  run validated kernel on SRAM data
 *   raw iDMA-out of out[cur]          ──┘  (overlap: this output tile)
 *
 * ALL SRAM buffers are allocated ONCE (xvAllocateBuffer, ping in bank0 / pong
 * in bank1, 64B aligned) and reused across frames and levels - no per-call
 * xvCreateTile/xvFreeTile, no malloc (the previous form re-created four tiles
 * and malloc'd the u16 scratch every call). Data movement is one raw iDMA
 * descriptor per tile side (xvAddIdmaRequest + xvWaitForiDMA), parameters
 * computed inline from the frame pointer/pitch - no tile-queue bookkeeping.
 *
 * The source region DMA'd per tile is the 7-tap neighbour extent
 * (output tile +/- 3, clamped to the frame, returned by
 * orb_blur_tile_source_bbox). reflect-101 maps any out-of-frame tap back into
 * the frame, so the region is clamped to the frame and the +/-3 neighbour is
 * real frame data - bit-exact with the PC full-image transform
 * (test_orb_blur_vp6.c). A blur tile always has a valid source region, so
 * there is no zero-fill branch.
 *
 * The per-tile maths is the same orb_blur_kernel() the PC reference
 * (orb_blur_full) calls, so the result is bit-exact with refactor's
 * ngd_gaussian_blur by construction.
 *
 * Build: plain C on PC, or as C++ under the Xtensa cstub (the cstub TIE stubs
 * are C++ and the IVP vector types only match in C++ mode - see CMakeLists.txt,
 * which compiles all .c as CXX, mirroring vp6ref/vp6/CMakeLists.txt).
 */
#include "orb_blur_vp6.h"

#include <stdlib.h>
#include <stdint.h>

/* IVP pixel kernel: ON by default (validated bit-exact against
 * orb_blur_kernel by test_orb_blur_vp6). Define ORB_BLUR_NO_IVP to fall back
 * to the portable C kernel. */
#ifndef ORB_BLUR_NO_IVP
#define ORB_BLUR_USE_IVP_INTRIN 1
#endif

#ifdef ORB_BLUR_USE_IVP_INTRIN
#ifdef __XTENSA__
#include <xtensa/tie/xt_ivpn.h>          /* real fy01_202210 core (found via --xtensa-system) */
#else
#include "xtensa/tie/Xm_fy01_202210.h"   /* cstub simulation (host build, CSTUB_PATH -I) */
#endif

/* reflect-101 with the SAME two-if form as orb_blur_core.c (n >= 1). */
static inline int orb_blur_reflect_v(int x, int n)
{
    if (x < 0) x = -x;
    if (x >= n) x = 2 * (n - 1) - x;
    return x;
}

/* golden horizontal-pass column (window crosses the frame edge, or the
 * scalar tail of the vector chunk loop): verbatim orb_blur_kernel maths. */
static void orb_blur_h_scalar(const uint8_t *row, int frame_w, int origin_x,
                              int tu0, int x, uint16_t *trow)
{
    int ax = tu0 + x;
    uint32_t s = 0;
    for (int k = 0; k < 7; ++k)
        s += (uint32_t)orb_blur_gauss7[k] *
             (uint32_t)row[orb_blur_reflect_v(ax - 3 + k, frame_w) - origin_x];
    trow[x] = (uint16_t)s;
}

/* golden vertical-pass column: verbatim orb_blur_kernel maths. */
static void orb_blur_v_scalar(const uint16_t *tmp, int tmp_pitch,
                              int origin_y, int frame_h,
                              int ay, int x, uint8_t *drow)
{
    uint32_t s = 0;
    for (int k = 0; k < 7; ++k) {
        int yy = orb_blur_reflect_v(ay - 3 + k, frame_h);
        s += (uint32_t)orb_blur_gauss7[k] *
             (uint32_t)tmp[(size_t)(yy - origin_y) * tmp_pitch + x];
    }
    uint32_t v = (s + 32768u) >> 16;
    drow[x] = v > 255u ? 255u : (uint8_t)v;
}

/*
 * IVP form of orb_blur_kernel (same signature, same tmp u16 layout).
 *
 * Horizontal pass, per region row: for output columns whose 7-tap window is
 * fully interior (ax in [3, frame_w-4]), the 7 taps are 7 unaligned LA/LAV
 * byte streams; the MAC runs in 64-lane 24-bit accumulators
 * (MULUS/MULUSPA with the scalar g_k), and CVT16U2NX24 L/H split the lanes
 * into two 32-lane u16 stores of tmp (each lane <= 255*256 = 65280, fits u16
 * exactly). Border columns / tail keep the golden scalar loop verbatim.
 *
 * Vertical pass, per interior output row: tmp u16 values are read as byte
 * pairs (lo at 2i, hi at 2i+1); the plane chain
 *   PACKL2NX24(CVT24U2NX16(UNPK_1(vB), UNPK_1(vA)))  (and UNPK_0 for lo)
 * rebuilds the 64-lane byte planes [A|B] (test_ivp_probe T16). The MAC runs
 * in TWO 24-bit accumulators,
 *   accH = sum_k g_k*hi_k   (<= 256*255 = 65,280)
 *   accL = sum_k g_k*lo_k   (<= 256*255 = 65,280)
 * because a 2Nx24 lane >= 0x800000 is read back as negative by the PACK ops
 * (T20) - the full sum (up to 0xFF0000) cannot live in one accumulator.
 * With sum g_k*tmp_k = 256*H + L exactly,
 *   (s + 32768) >> 16 = (H + (L>>8) + 128) >> 8
 * (write L + 32768 = 256*c + r, r < 256; then 256H + L + 32768 = 256*(H+c)
 * + r and both sides equal (H+c) >> 8). The right side is
 *   L1 = PACKVRNR2NX24(accL, 8)      (truncating L>>8, T20)
 *   MULUSPA2N8XR16(accH, L1, L1, 1)  (accH += L1)
 *   out8 = PACKVRU2NX24(accH, 8)     ((H + L1 + 128) >> 8, T20)
 * accH + L1 <= 65,535, so the shift-8 result <= 256 only when s = 0xFFxx*k
 * patterns exceed the real bound (tmp <= 255*256); the golden's saturate to
 * 255 and PACKVRU's >255 clamp agree there, and both are dead for real tmp
 * (s <= 256*65,280 -> out <= 255). Border rows / tail keep the golden scalar
 * loop verbatim.
 *
 * Alignment note (real DSP): tmp must start 64B-aligned with tmp_pitch a
 * multiple of 32 u16 (the driver arranges this), making the Nx16 stores and
 * the vertical plane loads naturally aligned; src loads and dst stores use
 * the unaligned-safe LA/LAV and SA protocols.
 */
static void orb_blur_kernel_ivp(const uint8_t *src_data, int src_pitch,
                                int origin_x, int origin_y, int region_h,
                                int frame_w, int frame_h,
                                int tu0, int tv0, int dst_w, int dst_h,
                                uint8_t *dst, int dst_pitch,
                                uint16_t *tmp, int tmp_pitch)
{
    int gk[7];                                   /* r16-safe as int */
    for (int k = 0; k < 7; ++k) gk[k] = (int)orb_blur_gauss7[k];

    /* ---- horizontal pass ---- */
    const int x_lo = (3 - tu0 > 0) ? 3 - tu0 : 0;              /* interior */
    const int x_hi = (frame_w - 4 - tu0 < dst_w - 1) ? frame_w - 4 - tu0 : dst_w - 1;
    for (int ry = 0; ry < region_h; ++ry) {
        const uint8_t *row = src_data + (size_t)ry * src_pitch;
        uint16_t *trow = tmp + (size_t)ry * tmp_pitch;

        for (int x = 0; x < x_lo && x < dst_w; ++x)
            orb_blur_h_scalar(row, frame_w, origin_x, tu0, x, trow);

        int x = x_lo;
        if (x + 63 <= x_hi) {
            const xb_vec2Nx8U *lp[7];
            valign lv[7];
            for (int k = 0; k < 7; ++k) {
                lp[k] = (const xb_vec2Nx8U *)(const void *)
                        (row + (tu0 + x - 3 + k - origin_x));
                lv[k] = IVP_LA2NX8U_PP(lp[k]);
            }
            for (; x + 63 <= x_hi; x += 64) {
                xb_vec2Nx8U v0;
                IVP_LAV2NX8U_XP(v0, lv[0], lp[0], 64);
                xb_vec2Nx24 acc = IVP_MULUS2N8XR16(v0, gk[0]);
                for (int k = 1; k < 7; ++k) {
                    xb_vec2Nx8U vk;
                    IVP_LAV2NX8U_XP(vk, lv[k], lp[k], 64);
                    IVP_MULUSPA2N8XR16(acc, vk, vk, gk[k]);
                }
                xb_vecNx16 lo16 = IVP_CVT16U2NX24L(acc);   /* lanes 0-31  */
                xb_vecNx16 hi16 = IVP_CVT16U2NX24H(acc);   /* lanes 32-63 */
                xb_vecNx16U *pl = (xb_vecNx16U *)(void *)(trow + x);
                xb_vecNx16U *ph = (xb_vecNx16U *)(void *)(trow + x + 32);
                IVP_SVNX16U_IP(*(xb_vecNx16U *)&lo16, pl, (int)sizeof(lo16));
                IVP_SVNX16U_IP(*(xb_vecNx16U *)&hi16, ph, (int)sizeof(hi16));
            }
        }
        for (; x <= x_hi; ++x)
            orb_blur_h_scalar(row, frame_w, origin_x, tu0, x, trow);
        for (int x = x_hi + 1; x < dst_w; ++x)
            orb_blur_h_scalar(row, frame_w, origin_x, tu0, x, trow);
    }

    /* ---- vertical pass ---- */
    for (int y = 0; y < dst_h; ++y) {
        const int ay = tv0 + y;
        uint8_t *drow = dst + (size_t)y * dst_pitch;

        if (ay < 3 || ay > frame_h - 4) {         /* border row: golden */
            for (int x = 0; x < dst_w; ++x)
                orb_blur_v_scalar(tmp, tmp_pitch, origin_y, frame_h, ay, x, drow);
            continue;
        }

        const uint8_t *tr[7];                     /* tmp rows ay-3 .. ay+3 */
        for (int k = 0; k < 7; ++k)
            tr[k] = (const uint8_t *)(tmp + (size_t)(ay - 3 + k - origin_y) * tmp_pitch);

        int x = 0;
        if (x + 64 <= dst_w) {
            xb_vec2Nx8U *mp = (xb_vec2Nx8U *)(void *)drow;
            valign sa;
            for (; x + 64 <= dst_w; x += 64) {
                xb_vec2Nx24 accH, accL;
                for (int k = 0; k < 7; ++k) {
                    xb_vec2Nx8U vA = IVP_LV2NX8U_X(
                        (const xb_vec2Nx8U *)(const void *)(tr[k] + 2 * x), 0);
                    xb_vec2Nx8U vB = IVP_LV2NX8U_X(
                        (const xb_vec2Nx8U *)(const void *)(tr[k] + 2 * x + 64), 0);
                    xb_vec2Nx8 hiS = IVP_PACKL2NX24(IVP_CVT24U2NX16(
                        IVP_UNPKU2NX8_1(*(const xb_vec2Nx8 *)&vB),
                        IVP_UNPKU2NX8_1(*(const xb_vec2Nx8 *)&vA)));
                    xb_vec2Nx8 loS = IVP_PACKL2NX24(IVP_CVT24U2NX16(
                        IVP_UNPKU2NX8_0(*(const xb_vec2Nx8 *)&vB),
                        IVP_UNPKU2NX8_0(*(const xb_vec2Nx8 *)&vA)));
                    xb_vec2Nx8U hi8 = *(const xb_vec2Nx8U *)(const void *)&hiS;
                    xb_vec2Nx8U lo8 = *(const xb_vec2Nx8U *)(const void *)&loS;
                    if (k == 0) {
                        accH = IVP_MULUS2N8XR16(hi8, gk[0]);
                        accL = IVP_MULUS2N8XR16(lo8, gk[0]);
                    } else {
                        IVP_MULUSPA2N8XR16(accH, hi8, hi8, gk[k]);
                        IVP_MULUSPA2N8XR16(accL, lo8, lo8, gk[k]);
                    }
                }
                /* (256H + L + 32768) >> 16 == (H + (L>>8) + 128) >> 8 */
                xb_vec2Nx8 L1s = IVP_PACKVRNR2NX24(accL, 8);   /* L>>8 */
                xb_vec2Nx8U L1 = *(const xb_vec2Nx8U *)(const void *)&L1s;
                IVP_MULUSPA2N8XR16(accH, L1, L1, 1);
                xb_vec2Nx8U out8 = IVP_PACKVRU2NX24(accH, 8);
                IVP_SA2NX8U_IP(out8, sa, mp);
            }
            IVP_SAPOS2NX8U_FP(sa, mp);
        }
        for (; x < dst_w; ++x)
            orb_blur_v_scalar(tmp, tmp_pitch, origin_y, frame_h, ay, x, drow);
    }
}
#endif /* ORB_BLUR_USE_IVP_INTRIN */

/* ---- kernel selection: IVP intrinsic path (default) or the validated
 *      portable orb_blur_kernel from orb_blur_core.c. ---- */
#ifdef ORB_BLUR_USE_IVP_INTRIN
#define ORB_BLUR_KERNEL orb_blur_kernel_ivp
#else
#define ORB_BLUR_KERNEL orb_blur_kernel
#endif

#define ORB_BLUR_MIN(a, b) (((a) < (b)) ? (a) : (b))

/* Translate a linear tile index to (tu0, tv0) frame pixel coords (top-left). */
static void orb_blur_tile_xy(int t, int n_tiles_x, int tile_w, int tile_h,
                             int *tu0, int *tv0)
{
    *tv0 = (t / n_tiles_x) * tile_h;
    *tu0 = (t % n_tiles_x) * tile_w;
}

/* ===================================================================== *
 *  VP6 tile driver (persistent SRAM buffers, raw 2D iDMA ping/pong)
 * ===================================================================== */

#include "frame_strip_vp6.h"     /* fs_bufs persistent ping/pong pairs       */

/* persistent ping/pong source-region and output pairs (allocated once,
 * reused across frames/levels); see the budget table in orb_tile_manager.h */
static fs_bufs blur_in, blur_out;

/* persistent u16 horizontal-pass scratch: bank0, 64B aligned. */
static struct {
    xvTileManager *tm;
    uint16_t      *p;
    int32_t        bytes;
} blur_tmp;

/* (re)allocate the persistent scratch so it holds >= bytes in bank0. */
static int orb_blur_tmp_alloc(xvTileManager *tm, int32_t bytes)
{
    if (blur_tmp.tm == tm && blur_tmp.bytes >= bytes) return 0;   /* resident */
    if (blur_tmp.tm && blur_tmp.p) xvFreeBuffer(blur_tmp.tm, (void *)blur_tmp.p);
    blur_tmp.tm = NULL; blur_tmp.p = NULL; blur_tmp.bytes = 0;
    uint8_t *q = (uint8_t *)xvAllocateBuffer(tm, bytes, XV_MEM_BANK_COLOR_0, 64);
    if (!q || q == (void *)(intptr_t)XVTM_ERROR) return -2;
    blur_tmp.tm = tm; blur_tmp.p = (uint16_t *)q; blur_tmp.bytes = bytes;
    return 0;
}

int orb_blur_vp6_process(xvTileManager *tm,
                         xvFrame *src_frame, xvFrame *dst_frame,
                         int src_w, int src_h,
                         const OrbBlurVp6Config *cfg)
{
    OrbBlurVp6Config def = ORB_BLUR_VP6_DEFAULT_CONFIG;
    OrbBlurVp6Config c = cfg ? *cfg : def;

    const int TW  = c.tile_w;
    const int TH  = c.tile_h;
    const int MSW = c.max_src_tile_w;
    const int MSH = c.max_src_tile_h;

    if (!tm || !src_frame || !dst_frame) return -1;
    if (TW <= 0 || TH <= 0 || MSW <= 0 || MSH <= 0) return -1;
    /* an interior tile's 7-tap source region is (TW+6) x (TH+6); the per-tile
     * buffer must hold that. */
    if (MSW < TW + 6 || MSH < TH + 6) return -1;

    const int n_tiles_x = (src_w + TW - 1) / TW;
    const int n_tiles_y = (src_h + TH - 1) / TH;
    const int N         = n_tiles_x * n_tiles_y;

    /* persistent buffers: in pair (source region, pitch MSW), out pair
     * (output tile, pitch TW), tmp (u16 rows of raw horizontal sums; pitch a
     * multiple of 32 u16 = 64 B, keeping the IVP stores/loads aligned). */
    const int in_pitch  = MSW;
    const int out_pitch = TW;
    const int tmp_pitch = (TW + 31) & ~31;
    const int tmp_bytes = MSH * tmp_pitch * (int)sizeof(uint16_t);
    if (fs_alloc(&blur_in,  tm, in_pitch  * MSH) != 0) return -2;
    if (fs_alloc(&blur_out, tm, out_pitch * TH ) != 0) return -2;
    if (orb_blur_tmp_alloc(tm, tmp_bytes)   != 0) return -2;

    uint8_t *fdata  = (uint8_t *)XV_FRAME_GET_DATA_PTR(src_frame);
    const int fpitch = XV_FRAME_GET_PITCH_IN_BYTES(src_frame);
    uint8_t *ddata  = (uint8_t *)XV_FRAME_GET_DATA_PTR(dst_frame);
    const int dpitch = XV_FRAME_GET_PITCH_IN_BYTES(dst_frame);

    int32_t in_dma[2]  = { -1, -1 }, out_dma[2] = { -1, -1 };
    int     in_pend[2] = { 0, 0 },   out_pend[2] = { 0, 0 };
    int ret = 0;

    /* helper to compute a tile's source bbox (always valid for a blur tile) */
    #define BLUR_BBOX(tu, tv, tw, th, mnx, mny, mxx, mxy) \
        orb_blur_tile_source_bbox((tu), (tv), (tw), (th), src_w, src_h, (mnx), (mny), (mxx), (mxy))

    /* one raw 2D iDMA descriptor per tile side; failure -> drain + -3 */
    #define BLUR_DMA_IN(i, mnx, mny, mxx, mxy)                                   \
        do {                                                                     \
            in_dma[i] = xvAddIdmaRequest(tm, blur_in.buf[i],                     \
                (void *)(fdata + (size_t)(mny) * fpitch + (size_t)(mnx)),        \
                (size_t)((mxx) - (mnx) + 1), (mxy) - (mny) + 1,                  \
                fpitch, in_pitch, 0);                                            \
            if (in_dma[i] < 0) { ret = -3; goto drain; }                         \
            in_pend[i] = 1;                                                      \
        } while (0)

    #define BLUR_DMA_OUT(i, tu, tv, tw, th)                                      \
        do {                                                                     \
            out_dma[i] = xvAddIdmaRequest(tm,                                    \
                (void *)(ddata + (size_t)(tv) * dpitch + (size_t)(tu)),          \
                blur_out.buf[i], (size_t)(tw), (th), out_pitch, dpitch, 0);      \
            if (out_dma[i] < 0) { ret = -3; goto drain; }                        \
            out_pend[i] = 1;                                                     \
        } while (0)

    /* prefetch tile 0 source region */
    {
        int tu, tv; orb_blur_tile_xy(0, n_tiles_x, TW, TH, &tu, &tv);
        int tw = ORB_BLUR_MIN(TW, src_w - tu), th = ORB_BLUR_MIN(TH, src_h - tv);
        int mnx, mny, mxx, mxy;
        BLUR_BBOX(tu, tv, tw, th, &mnx, &mny, &mxx, &mxy);
        BLUR_DMA_IN(0, mnx, mny, mxx, mxy);
    }

    for (int t = 0; t < N; t++) {
        const int cur = t & 1;          /* ping/pong index for the current tile */
        const int nxt = 1 - cur;        /* the other buffers, prefetch t+1      */

        int tu, tv; orb_blur_tile_xy(t, n_tiles_x, TW, TH, &tu, &tv);
        int tw = ORB_BLUR_MIN(TW, src_w - tu), th = ORB_BLUR_MIN(TH, src_h - tv);
        int mnx, mny, mxx, mxy;
        BLUR_BBOX(tu, tv, tw, th, &mnx, &mny, &mxx, &mxy);
        int region_h = mxy - mny + 1;   /* source rows present in SRAM */

        /* prefetch tile t+1 source region into the other input buffer (overlap) */
        if (t + 1 < N) {
            int tu1, tv1; orb_blur_tile_xy(t + 1, n_tiles_x, TW, TH, &tu1, &tv1);
            int tw1 = ORB_BLUR_MIN(TW, src_w - tu1), th1 = ORB_BLUR_MIN(TH, src_h - tv1);
            int n_mnx, n_mny, n_mxx, n_mxy;
            BLUR_BBOX(tu1, tv1, tw1, th1, &n_mnx, &n_mny, &n_mxx, &n_mxy);
            BLUR_DMA_IN(nxt, n_mnx, n_mny, n_mxx, n_mxy);
        }

        xvWaitForiDMA(tm, in_dma[cur]);          /* wait for this tile's DMA-in */
        in_pend[cur] = 0;

        /* reuse safety: ensure out[cur]'s previous DMA-out (from t-2) is done */
        if (out_pend[cur]) { xvWaitForiDMA(tm, out_dma[cur]); out_pend[cur] = 0; }

        ORB_BLUR_KERNEL(blur_in.buf[cur], in_pitch,
                        mnx, mny, region_h,
                        src_w, src_h,
                        tu, tv, tw, th,
                        blur_out.buf[cur], out_pitch,
                        blur_tmp.p, tmp_pitch);
        BLUR_DMA_OUT(cur, tu, tv, tw, th);
    }

drain:
    /* drain in-flight DMA-out: at most two output buffers can be outstanding
     * (out[cur] from the last tile and out[nxt] from the tile before it). */
    if (out_pend[0]) xvWaitForiDMA(tm, out_dma[0]);
    if (out_pend[1]) xvWaitForiDMA(tm, out_dma[1]);
    return ret;
    #undef BLUR_DMA_OUT
    #undef BLUR_DMA_IN
    #undef BLUR_BBOX
}

void orb_blur_vp6_release(void)
{
    fs_free(&blur_in);
    fs_free(&blur_out);
    if (blur_tmp.tm && blur_tmp.p) xvFreeBuffer(blur_tmp.tm, (void *)blur_tmp.p);
    blur_tmp.tm = NULL; blur_tmp.p = NULL; blur_tmp.bytes = 0;
}
