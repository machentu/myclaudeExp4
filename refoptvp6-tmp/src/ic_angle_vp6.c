/*
 * ic_angle_vp6.c - VP6 per-keypoint IC angle driver (P6 TileManager).
 *
 * Pipeline (per keypoint, ping/pong patch-tile double-buffered):
 *
 *   prepare in[1-cur] for keypoint k+1  ──┐  (overlap: DMA-in of next patch)
 *   xvWaitForTile(in[cur])              ──┤  wait for keypoint k's patch DMA
 *   angles[k] = ic_angle_kernel(in[cur])──┤  run validated kernel on SRAM patch
 *                                         ┘  (no DMA-out: one float per keypoint)
 *
 * The source region DMA'd per keypoint is its +/-15 patch clamped to the frame
 * (ic_angle_patch_bbox). ic_pix_clamp clamps to the frame edge (matching
 * ngd_pix_clamp), and the clamped coord is always in the clamped patch, so no
 * edge padding is needed (EDGE=0) and the result is bit-exact with ngd_ic_angle.
 *
 * Mirrors orb_blur_vp6.c / fast9_vp6.c (minus the dense output).
 */
#include "ic_angle_vp6.h"

#include <stdlib.h>
#include <math.h>

/* Prepare an input tile for the source region [mn_x,mn_y]-[mx_x,mx_y] of the
 * source frame. Returns 0 ok, -1 if the region exceeds the tile buffer. */
static int ic_angle_prepare_input(xvTile *t, int mn_x, int mn_y, int mx_x, int mx_y,
                                  int max_w, int max_h, int pitch)
{
    int w = mx_x - mn_x + 1;
    int h = mx_y - mn_y + 1;
    if (w > max_w || h > max_h) return -1;
    XV_TILE_UPDATE_DIMENSIONS(t, mn_x, mn_y, w, h, pitch);
    return 0;
}

/* Round a float keypoint coord the way ngd_ic_angle does ((int)lroundf). */
static inline int ic_round(float v) { return (int)lroundf(v); }

int ic_angle_vp6_process(xvTileManager *tm, xvFrame *src_frame,
                         int src_w, int src_h,
                         const fast9_keypoint *kps, float *angles, int n_kps,
                         const int *umax, const IcAngleVp6Config *cfg)
{
    IcAngleVp6Config def = IC_ANGLE_VP6_DEFAULT_CONFIG;
    IcAngleVp6Config c = cfg ? *cfg : def;

    const int MSW = c.max_src_w;
    const int MSH = c.max_src_h;

    if (!tm || !src_frame || !kps || !angles || !umax) return -1;
    if (MSW < IC_ANGLE_PATCH_SIZE || MSH < IC_ANGLE_PATCH_SIZE) return -1;
    if (n_kps <= 0) return 0;

    /* allocate ping/pong input (patch) tiles. No output tiles: one float per
     * keypoint written directly to the DRAM `angles` array. */
    const int in_buf = MSW * MSH;
    xvTile *in[2] = { NULL, NULL };
    in[0] = xvCreateTile(tm, in_buf, MSW, MSH, MSW, 0, 0, XV_MEM_BANK_COLOR_0, src_frame, XV_TILE_U8, DATA_ALIGNED_32);
    in[1] = xvCreateTile(tm, in_buf, MSW, MSH, MSW, 0, 0, XV_MEM_BANK_COLOR_1, src_frame, XV_TILE_U8, DATA_ALIGNED_32);
    if (!in[0] || !in[1]) return -2;

    int ret = 0;

    /* helper: prefetch keypoint k's patch into tile `tt` */
    #define IC_PREFETCH(k, tt) do {                                              \
        int cx = ic_round(kps[k].x), cy = ic_round(kps[k].y);                    \
        int mnx, mny, mxx, mxy;                                                  \
        ic_angle_patch_bbox(cx, cy, src_w, src_h, &mnx, &mny, &mxx, &mxy);       \
        if (ic_angle_prepare_input(tt, mnx, mny, mxx, mxy, MSW, MSH, MSW) != 0)  \
            { ret = -3; goto done; }                                             \
        xvReqTileTransferIn(tm, tt, NULL, 0);                                    \
    } while (0)

    /* helper: run keypoint k's kernel on tile `tt`, write angles[k] */
    #define IC_RUN(k, tt) do {                                                   \
        int cx = ic_round(kps[k].x), cy = ic_round(kps[k].y);                    \
        int mnx, mny, mxx, mxy;                                                  \
        ic_angle_patch_bbox(cx, cy, src_w, src_h, &mnx, &mny, &mxx, &mxy);       \
        angles[k] = ic_angle_kernel((const uint8_t *)XV_TILE_GET_DATA_PTR(tt),   \
                                    XV_TILE_GET_PITCH(tt), mnx, mny,             \
                                    src_w, src_h, cx, cy, umax);                 \
    } while (0)

    /* prefetch keypoint 0 patch */
    IC_PREFETCH(0, in[0]);

    for (int k = 0; k < n_kps; k++) {
        const int cur = k & 1;          /* ping/pong index for the current keypoint */
        const int nxt = 1 - cur;        /* the other buffer, used to prefetch k+1   */

        /* prefetch keypoint k+1 patch into the other buffer (overlap) */
        if (k + 1 < n_kps)
            IC_PREFETCH(k + 1, in[nxt]);

        xvWaitForTile(tm, in[cur]);          /* wait for this keypoint's patch DMA */
        IC_RUN(k, in[cur]);
        if (ret) goto done;
        /* no DMA-out: angle already in DRAM `angles[k]` */
    }

done:
    xvFreeTile(tm, in[0]);  xvFreeTile(tm, in[1]);
    return ret;
    #undef IC_PREFETCH
    #undef IC_RUN
}
