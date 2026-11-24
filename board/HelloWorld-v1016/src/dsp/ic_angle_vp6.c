/*
 * ic_angle_vp6.c - VP6 whole-frame strip IC angle driver (P6 TileManager).
 *
 * Pipeline (full-width strip sweep, ping/pong double-buffered - the
 * ExtractImage.cpp / TileMemMgr.cpp pattern, see frame_strip_vp6.h):
 *
 *   schedule strip DMA (pass p+1)  ──┐  (overlap: raw 2D iDMA of next strip)
 *   xvWaitForiDMA (pass p)         ──┤  (strip = assigned rows +/-15 halo)
 *   for each keypoint assigned to  ──┤  pass p: kernel gathers straight from
 *   ... pass p                        the SRAM strip (no per-keypoint DMA)
 *
 * Per keypoint there is NO patch DMA any more: the frame streams through two
 * resident SRAM strip buffers (allocated once from the tile manager's two
 * memory banks and reused across frames/levels), and every keypoint whose
 * clamped centre row falls in the pass reads its +/-15 disk from the strip.
 * The kernel sees (strip data, strip pitch, origin = strip's first frame
 * row) - pure difference-based indexing on a byte-identical copy of the
 * frame rows, so the result is bit-exact with the patch-DMA form and with
 * ngd_ic_angle (verified 3-way by test_ic_angle_vp6.c).
 *
 * The kernel is the IVP intrinsic form (IVP gather pixel access + per-column
 * accumulators + scalar moment fold, see ic_angle_kernel_ivp below) - bit
 * exact with ic_angle_kernel. Define IC_ANGLE_NO_IVP to fall back to the
 * portable kernel.
 */
#include "ic_angle_vp6.h"
#include "frame_strip_vp6.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

/* IVP pixel kernel: ON by default (validated bit-exact against ic_angle_full by
 * test_ic_angle_vp6). Define IC_ANGLE_NO_IVP to fall back to the portable kernel. */
#ifndef IC_ANGLE_NO_IVP
#define IC_ANGLE_USE_IVP_INTRIN 1
#endif

#ifndef ALIGN64
#  ifdef _MSC_VER
#    define ALIGN64 __declspec(align(64))
#  else
#    define ALIGN64 __attribute__((aligned(64)))
#  endif
#endif

#ifdef IC_ANGLE_USE_IVP_INTRIN
#ifdef __XTENSA__
#include <xtensa/tie/xt_ivpn.h>          /* real fy01_202210 core (found via --xtensa-system) */
#else
#include "xtensa/tie/Xm_fy01_202210.h"   /* cstub simulation (host build, CSTUB_PATH -I) */
#endif

/* Gather 64 pixels: byte offsets offs[0..63] (relative to the tile base) ->
 * one 64-lane byte vector. Chain per test_ivp_probe T21: two gathers
 * (32 u16 offset lanes each) + GATHERD2NX8U (second gsr arg -> lanes 0-31).
 * GATHERANX8U (byte elements), NOT GATHERANX16U (halfword elements): the
 * offsets are byte offsets with arbitrary parity, and the real Gather/Scatter
 * unit NOOPs an element address unaligned to ElemSize (the cstub does not
 * model that - it reads any address) - halfword gathers silently return 0
 * for every odd column on the DSP. GATHERD2NX8U keeps each lane's low byte,
 * so both gathers yield identical lanes on the cstub. */
static inline xb_vec2Nx8U ic_gather64(const unsigned char *gbase,
                                      const uint16_t *offs)
{
    xb_vecNx16U a = *(const xb_vecNx16U *)(const void *)(offs);
    xb_vecNx16U b = *(const xb_vecNx16U *)(const void *)(offs + 32);
    xb_gsr ga = IVP_GATHERANX8U(gbase, a);
    xb_gsr gb = IVP_GATHERANX8U(gbase, b);
    return IVP_GATHERD2NX8U(gb, ga);   /* pixels offs[0..63] */
}

/* Keep the disk lanes of one gathered row: lane l holds column u = l-15, valid
 * for row half-width d iff |u| <= d. (u + d) as an UNSIGNED byte is < 2d+1
 * exactly on [−d, d] (u ∈ [-15,15], d <= 15: below −d it wraps into
 * [241+d, 255] > 2d+1, above +d it lands in [2d+1, 48+d]), so
 *   ADD2NX8U(idxm15, d) ; LTU2NX8(_, 2d+1) ; MOV2NX8UT(t, 0, lt)
 * zeroes every lane the golden's |u| <= d loop would not read. */
static inline xb_vec2Nx8U ic_mask_row(xb_vec2Nx8U t, xb_vec2Nx8U vidx,
                                      xb_vec2Nx8U vzero, int d)
{
    ALIGN64 uint8_t bd[64], b2[64];
    memset(bd, (uint8_t)d, sizeof bd);
    memset(b2, (uint8_t)(2 * d + 1), sizeof b2);
    xb_vec2Nx8U vd = IVP_LV2NX8U_X((const xb_vec2Nx8U *)(const void *)bd, 0);
    xb_vec2Nx8U v2 = IVP_LV2NX8U_X((const xb_vec2Nx8U *)(const void *)b2, 0);
    return IVP_MOV2NX8UT(t, vzero, IVP_LTU2NX8(IVP_ADD2NX8U(vidx, vd), v2));
}

/* Store a 2Nx24 accumulator's 64 lanes as two u16[32] halves. Every lane value
 * here is <= 30,600 (<= 255 * sum(v)), i.e. < 2^16 and < 2^23, so the 24->16
 * conversion never touches the lane sign bit (T20) and never truncates. */
static inline void ic_store_acc64(xb_vec2Nx24 acc, uint16_t *out64)
{
    xb_vecNx16 lo = IVP_CVT16U2NX24L(acc);   /* lanes 0-31  */
    xb_vecNx16 hi = IVP_CVT16U2NX24H(acc);   /* lanes 32-63 */
    xb_vecNx16U *pl = (xb_vecNx16U *)(void *)(out64);
    xb_vecNx16U *ph = (xb_vecNx16U *)(void *)(out64 + 32);
    IVP_SVNX16U_IP(*(xb_vecNx16U *)&lo, pl, (int)sizeof(lo));
    IVP_SVNX16U_IP(*(xb_vecNx16U *)&hi, ph, (int)sizeof(hi));
}

/*
 * IVP form of ic_angle_kernel (same signature, same float result).
 *
 * The golden reduces per column u first (v_sum) then across columns:
 *   m_10 = Σ_u u·(I(u,0) + Σ_{v≥1}(I(u,v)+I(u,−v)))
 *   m_01 = Σ_{v≥1} v·Σ_{|u|≤d_v}(I(u,v)−I(u,−v)) = Σ_u (Σ_v v·I(u,v) − Σ_v v·I(u,−v))
 * Both are exact INTEGER sums, so any regrouping is bit-exact. Lane l of the
 * accumulators holds column u = l−15 (fixed mapping: every row is gathered from
 * the SAME clamped column offsets, only the row offset varies):
 *   accCP[l] = I(u,0)·1 + Σ_v I(u,v)·1   (<= 16·255 = 4,080)
 *   accCM[l] = Σ_v I(u,−v)·1             (<= 15·255 = 3,825)
 *   accDp[l] = Σ_v v·I(u,v)              (<= 255·Σv = 30,600)
 *   accDm[l] = Σ_v v·I(u,−v)             (<= 30,600)
 * (all MULUS/MULUSPA with positive scalar weights; the v-weighted ± split
 * avoids negative lanes). After the row loop the four accumulators are stored
 * as u16 lanes and folded scalarly (31 MACs each) into m_10/m_01; ic_fast_atan2
 * then sees the identical integers as the golden -> identical float bits.
 *
 * Edge clamp: the 64 column offsets are computed ONCE per keypoint with the
 * golden's clamp (x = clamp(cx+u)), so near-edge keypoints gather clamped
 * (duplicated) pixels exactly like ic_pix_clamp; row indices clamp(cy±v) are
 * scalar per v. Lanes 31..63 (|u| > 15) and per-row |u| > d_v lanes are masked
 * to zero by ic_mask_row before the MAC, so they never contribute.
 */
static float ic_angle_kernel_ivp(const uint8_t *src_data, int src_pitch,
                                 int origin_x, int origin_y,
                                 int frame_w, int frame_h,
                                 int cx, int cy, const int *umax)
{
    ALIGN64 uint16_t offp[64], offm[64];
    ALIGN64 uint16_t P16[64], M16[64], Dp16[64], Dm16[64];
    ALIGN64 uint8_t  bidx[64], zero64[64];
    int colx[64];

    for (int l = 0; l < 64; ++l) {
        int u = l - 15;
        int x = cx + u;
        if (x < 0) x = 0; else if (x >= frame_w) x = frame_w - 1;
        colx[l] = x - origin_x;
        bidx[l] = (uint8_t)u;               /* lane index - 15, wraps mod 256 */
    }
    memset(zero64, 0, sizeof zero64);
    xb_vec2Nx8U vzero = IVP_LV2NX8U_X((const xb_vec2Nx8U *)(const void *)zero64, 0);
    xb_vec2Nx8U vidx  = IVP_LV2NX8U_X((const xb_vec2Nx8U *)(const void *)bidx, 0);
    const unsigned char *gbase = src_data;

    /* center row v=0 (d=15, all 31 lanes valid): seeds accCP, and the others
     * start from an all-zero accumulator so the v-loop is uniform. */
    int y0 = cy;
    if (y0 < 0) y0 = 0; else if (y0 >= frame_h) y0 = frame_h - 1;
    for (int l = 0; l < 64; ++l)
        offp[l] = (uint16_t)((y0 - origin_y) * src_pitch + colx[l]);
    xb_vec2Nx8U tc = ic_mask_row(ic_gather64(gbase, offp), vidx, vzero, 15);
    xb_vec2Nx24 accCP = IVP_MULUS2N8XR16(tc, 1);
    xb_vec2Nx24 accCM = IVP_MULUS2N8XR16(vzero, 1);
    xb_vec2Nx24 accDp = IVP_MULUS2N8XR16(vzero, 1);
    xb_vec2Nx24 accDm = IVP_MULUS2N8XR16(vzero, 1);

    for (int v = 1; v <= IC_ANGLE_HALF_PATCH; ++v) {
        const int d = umax[v];
        int yp = cy + v, ym = cy - v;
        if (yp > frame_h - 1) yp = frame_h - 1;
        if (ym < 0) ym = 0;
        const int rp = (yp - origin_y) * src_pitch;
        const int rm = (ym - origin_y) * src_pitch;
        for (int l = 0; l < 64; ++l) {
            offp[l] = (uint16_t)(rp + colx[l]);
            offm[l] = (uint16_t)(rm + colx[l]);
        }
        xb_vec2Nx8U tp = ic_mask_row(ic_gather64(gbase, offp), vidx, vzero, d);
        xb_vec2Nx8U tm = ic_mask_row(ic_gather64(gbase, offm), vidx, vzero, d);
        IVP_MULUSPA2N8XR16(accCP, tp, tp, 1);
        IVP_MULUSPA2N8XR16(accCM, tm, tm, 1);
        IVP_MULUSPA2N8XR16(accDp, tp, tp, v);
        IVP_MULUSPA2N8XR16(accDm, tm, tm, v);
    }

    ic_store_acc64(accCP, P16);
    ic_store_acc64(accCM, M16);
    ic_store_acc64(accDp, Dp16);
    ic_store_acc64(accDm, Dm16);

    int m_01 = 0, m_10 = 0;
    for (int l = 0; l <= 2 * IC_ANGLE_HALF_PATCH; ++l) {   /* lanes 0..30 */
        const int u = l - 15;
        m_10 += u * ((int)P16[l] + (int)M16[l]);
        m_01 += (int)Dp16[l] - (int)Dm16[l];
    }
    return ic_fast_atan2((float)m_01, (float)m_10);   /* same ints -> same bits */
}
#endif /* IC_ANGLE_USE_IVP_INTRIN */

/* ---- kernel selection: IVP intrinsic path (default) or the validated
 *      portable ic_angle_kernel from ic_angle_core.c. ---- */
#ifdef IC_ANGLE_USE_IVP_INTRIN
#define IC_ANGLE_KERNEL ic_angle_kernel_ivp
#else
#define IC_ANGLE_KERNEL ic_angle_kernel
#endif

/* Round a float keypoint coord the way ngd_ic_angle does ((int)lroundf). */
static inline int ic_round(float v) { return (int)lroundf(v); }

/* persistent ping/pong strip pair (allocated once, reused across calls) */
static fs_bufs ic_angle_fs;

int ic_angle_vp6_process(xvTileManager *tm, xvFrame *src_frame,
                         int src_w, int src_h,
                         const fast9_keypoint *kps, float *angles, int n_kps,
                         const int *umax, const IcAngleVp6Config *cfg)
{
    IcAngleVp6Config def = IC_ANGLE_VP6_DEFAULT_CONFIG;
    IcAngleVp6Config c = cfg ? *cfg : def;

    const int SR  = c.strip_rows;
    const int MSW = c.max_src_w;
    const int MSH = c.max_src_h;

    if (!tm || !src_frame || !kps || !angles || !umax) return -1;
    if (SR < 1 || MSW < 1) return -1;
    if (MSH < SR + 2 * IC_ANGLE_MARGIN) return -1;   /* halo must fit too */
    if (MSW < src_w) return -1;                      /* strips span width */

    if (n_kps <= 0) return 0;

    if (fs_alloc(&ic_angle_fs, tm, FS_ALIGN64(MSW) * MSH) != 0) return -2;

    fs_sweep sw;
    fs_sweep_begin(&sw, &ic_angle_fs, src_frame, src_w, src_h, IC_ANGLE_MARGIN);

    /* Assign each keypoint to the pass containing its CLAMPED centre row:
     * every read of the kernel is within +/-15 of clamp(cy) (the clamp is
     * 1-Lipschitz), and the pass's strip carries 15 halo rows, so all reads
     * land in the strip regardless of how far outside the frame cy is. */
    fs_view v;
    while (fs_sweep_next(&sw, &v)) {
        for (int k = 0; k < n_kps; ++k) {
            int cy = ic_round(kps[k].y);
            if (cy < 0) cy = 0; else if (cy > src_h - 1) cy = src_h - 1;
            if (cy < v.top || cy >= v.top + v.rows) continue;   /* other pass */
            angles[k] = IC_ANGLE_KERNEL(v.data, v.pitch, 0, v.srow0,
                                        src_w, src_h,
                                        ic_round(kps[k].x), ic_round(kps[k].y),
                                        umax);
        }
    }
    return 0;
}

void ic_angle_vp6_release(void)
{
    fs_free(&ic_angle_fs);
}
