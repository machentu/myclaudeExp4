/*
 * brief_vp6.c - VP6 per-keypoint rotated-BRIEF driver (P6 TileManager).
 *
 * Pipeline (per keypoint, ping/pong patch-tile double-buffered):
 *
 *   prepare in[1-cur] for keypoint k+1  ──┐  (overlap: DMA-in of next patch)
 *   xvWaitForTile(in[cur])              ──┤  wait for keypoint k's patch DMA
 *   brief_kernel(in[cur] -> desc[k*32]) ──┤  run validated kernel on SRAM patch
 *                                         ┘  (no DMA-out: 32 bytes/keypoint)
 *
 * The source region DMA'd per keypoint is its +/-19 patch clamped to the frame
 * (brief_patch_bbox). brief_pix_clamp clamps to the frame edge (matching
 * ngd_pix_clamp), and the clamped coord is always in the clamped patch, so no
 * edge padding is needed (EDGE=0) and the result is bit-exact with
 * ngd_compute_descriptor.
 *
 * Mirrors ic_angle_vp6.c.
 */
#include "brief_vp6.h"

#include <stdlib.h>
#include <math.h>

/* Prepare an input tile for the source region [mn_x,mn_y]-[mx_x,mx_y] of the
 * source frame. Returns 0 ok, -1 if the region exceeds the tile buffer. */
static int brief_prepare_input(xvTile *t, int mn_x, int mn_y, int mx_x, int mx_y,
                               int max_w, int max_h, int pitch)
{
    int w = mx_x - mn_x + 1;
    int h = mx_y - mn_y + 1;
    if (w > max_w || h > max_h) return -1;
    XV_TILE_UPDATE_DIMENSIONS(t, mn_x, mn_y, w, h, pitch);
    return 0;
}

/* Round a float keypoint coord the way ngd_compute_descriptor does ((int)lroundf). */
static inline int brief_round(float v) { return (int)lroundf(v); }

int brief_vp6_process(xvTileManager *tm, xvFrame *src_frame,
                      int src_w, int src_h,
                      const fast9_keypoint *kps, uint8_t *desc, int n_kps,
                      const BriefVp6Config *cfg)
{
    BriefVp6Config def = BRIEF_VP6_DEFAULT_CONFIG;
    BriefVp6Config c = cfg ? *cfg : def;

    const int MSW = c.max_src_w;
    const int MSH = c.max_src_h;

    if (!tm || !src_frame || !kps || !desc) return -1;
    if (MSW < BRIEF_PATCH_SIZE || MSH < BRIEF_PATCH_SIZE) return -1;
    if (n_kps <= 0) return 0;

    /* allocate ping/pong input (patch) tiles. No output tiles: 32 bytes/keypoint
     * written directly to the DRAM `desc` array. */
    const int in_buf = MSW * MSH;
    xvTile *in[2] = { NULL, NULL };
    in[0] = xvCreateTile(tm, in_buf, MSW, MSH, MSW, 0, 0, XV_MEM_BANK_COLOR_0, src_frame, XV_TILE_U8, DATA_ALIGNED_32);
    in[1] = xvCreateTile(tm, in_buf, MSW, MSH, MSW, 0, 0, XV_MEM_BANK_COLOR_1, src_frame, XV_TILE_U8, DATA_ALIGNED_32);
    if (!in[0] || !in[1]) return -2;

    int ret = 0;

    /* helper: prefetch keypoint k's patch into tile `tt` */
    #define BRIEF_PREFETCH(k, tt) do {                                            \
        int cx = brief_round(kps[k].x), cy = brief_round(kps[k].y);               \
        int mnx, mny, mxx, mxy;                                                   \
        brief_patch_bbox(cx, cy, src_w, src_h, &mnx, &mny, &mxx, &mxy);           \
        if (brief_prepare_input(tt, mnx, mny, mxx, mxy, MSW, MSH, MSW) != 0)      \
            { ret = -3; goto done; }                                             \
        xvReqTileTransferIn(tm, tt, NULL, 0);                                     \
    } while (0)

    /* helper: run keypoint k's kernel on tile `tt`, write desc[k*32..] */
    #define BRIEF_RUN(k, tt) do {                                                 \
        int cx = brief_round(kps[k].x), cy = brief_round(kps[k].y);               \
        int mnx, mny, mxx, mxy;                                                   \
        brief_patch_bbox(cx, cy, src_w, src_h, &mnx, &mny, &mxx, &mxy);           \
        brief_kernel((const uint8_t *)XV_TILE_GET_DATA_PTR(tt),                   \
                     XV_TILE_GET_PITCH(tt), mnx, mny, src_w, src_h,               \
                     cx, cy, kps[k].angle, desc + (size_t)(k) * 32);              \
    } while (0)

    /* prefetch keypoint 0 patch */
    BRIEF_PREFETCH(0, in[0]);

    for (int k = 0; k < n_kps; k++) {
        const int cur = k & 1;          /* ping/pong index for the current keypoint */
        const int nxt = 1 - cur;        /* the other buffer, used to prefetch k+1   */

        /* prefetch keypoint k+1 patch into the other buffer (overlap) */
        if (k + 1 < n_kps)
            BRIEF_PREFETCH(k + 1, in[nxt]);

        xvWaitForTile(tm, in[cur]);          /* wait for this keypoint's patch DMA */
        BRIEF_RUN(k, in[cur]);
        if (ret) goto done;
        /* no DMA-out: 32-byte descriptor already in DRAM `desc[k*32]` */
    }

done:
    xvFreeTile(tm, in[0]);  xvFreeTile(tm, in[1]);
    return ret;
    #undef BRIEF_PREFETCH
    #undef BRIEF_RUN
}
