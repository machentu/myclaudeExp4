/*
 * fast9_vp6.c - VP6 strip-based FAST-9 detect + NMS driver (P6 TileManager).
 *
 * Pipeline (per ROI strip, top-to-bottom, ping/pong input double-buffered):
 *
 *   prepare in[1-cur] for strip j+1  ──┐  (overlap: DMA-in of next strip)
 *   xvWaitForTile(in[cur])           ──┤  wait for strip j's source DMA
 *   fast9_detect_kernel(in[cur]->out)──┤  run validated kernel on SRAM data;
 *                                       │  keypoints appended to DRAM `out`
 *                                       ┘  (no DMA-out: sparse output)
 *
 * Each strip spans the FULL ROI width, so the kernel emits row-by-row across
 * the whole width = refactor's raster order. Strips top-to-bottom concatenate
 * to global raster -> bit-exact with ngd_fast_detect (order + cap). The source
 * region per strip is the strip +/- 4 (ring 3 + NMS 1, clamped to the frame);
 * the +/-4 vertical margin covers NMS neighbours across strip boundaries.
 *
 * Mirrors orb_blur_vp6.c / vp6ref/vp6/ipm_vp6.c (minus the output DMA, since
 * keypoints are sparse).
 */
#include "fast9_vp6.h"

#include <stdlib.h>

#define FAST9_MIN(a, b) (((a) < (b)) ? (a) : (b))

/* Prepare an input tile for the source region [mn_x,mn_y]-[mx_x,mx_y] of the
 * source frame. Returns 0 ok, -1 if the region exceeds the tile buffer. */
static int fast9_prepare_input(xvTile *t, int mn_x, int mn_y, int mx_x, int mx_y,
                               int max_w, int max_h, int pitch)
{
    int w = mx_x - mn_x + 1;
    int h = mx_y - mn_y + 1;
    if (w > max_w || h > max_h) return -1;
    XV_TILE_UPDATE_DIMENSIONS(t, mn_x, mn_y, w, h, pitch);
    return 0;
}

/* Source bbox for a strip emitting [sy0,sy0+sh) over ROI x [iniX,maxX):
 * x in [iniX-4, maxX+3], y in [sy0-4, sy0+sh+3], clamped to the frame. */
static void fast9_strip_bbox(int iniX, int maxX, int sy0, int sh,
                             int frame_w, int frame_h,
                             int *mn_x, int *mn_y, int *mx_x, int *mx_y)
{
    int mnx = iniX - FAST9_MARGIN;
    int mny = sy0  - FAST9_MARGIN;
    int mxx = maxX + FAST9_MARGIN - 1;     /* (maxX-1) + 4 */
    int mxy = sy0 + sh + FAST9_MARGIN - 1; /* (sy0+sh-1) + 4 */
    if (mnx < 0) mnx = 0;
    if (mny < 0) mny = 0;
    if (mxx > frame_w - 1) mxx = frame_w - 1;
    if (mxy > frame_h - 1) mxy = frame_h - 1;
    *mn_x = mnx; *mn_y = mny; *mx_x = mxx; *mx_y = mxy;
}

int fast9_vp6_process(xvTileManager *tm, xvFrame *src_frame,
                      int src_w, int src_h,
                      int iniX, int maxX, int iniY, int maxY, int t,
                      fast9_keypoint *out, int *n_inout, int cap,
                      const Fast9Vp6Config *cfg)
{
    Fast9Vp6Config def = FAST9_VP6_DEFAULT_CONFIG;
    Fast9Vp6Config c = cfg ? *cfg : def;

    const int SH  = c.strip_h;
    const int MSW = c.max_src_w;
    const int MSH = c.max_src_h;

    if (!tm || !src_frame || !out || !n_inout) return -1;
    if (SH <= 0 || MSW <= 0 || MSH <= 0) return -1;
    if (iniX < 0 || iniY < 0 || maxX > src_w || maxY > src_h) return -1;
    if (iniX >= maxX || iniY >= maxY) return -1;
    const int roiW = maxX - iniX;
    const int roiH = maxY - iniY;
    /* strip spans the full ROI width + a +/-4 horizontal margin; vertical margin
     * is +/-4 over the strip height. */
    if (MSW < roiW + 2 * FAST9_MARGIN) return -1;
    if (MSH < SH + 2 * FAST9_MARGIN) return -1;

    const int n_strips = (roiH + SH - 1) / SH;

    /* allocate ping/pong input (source region) tiles. No output tiles: keypoints
     * are sparse and written directly to the DRAM `out` list by the kernel. */
    const int in_buf = MSW * MSH;
    xvTile *in[2] = { NULL, NULL };
    in[0] = xvCreateTile(tm, in_buf, MSW, MSH, MSW, 0, 0, XV_MEM_BANK_COLOR_0, src_frame, XV_TILE_U8, DATA_ALIGNED_32);
    in[1] = xvCreateTile(tm, in_buf, MSW, MSH, MSW, 0, 0, XV_MEM_BANK_COLOR_1, src_frame, XV_TILE_U8, DATA_ALIGNED_32);
    if (!in[0] || !in[1]) return -2;

    int ret = 0;

    /* prefetch strip 0 source region */
    {
        int sy0 = iniY, sh = FAST9_MIN(SH, maxY - sy0);
        int mnx, mny, mxx, mxy;
        fast9_strip_bbox(iniX, maxX, sy0, sh, src_w, src_h, &mnx, &mny, &mxx, &mxy);
        if (fast9_prepare_input(in[0], mnx, mny, mxx, mxy, MSW, MSH, MSW) != 0) { ret = -3; goto done; }
        xvReqTileTransferIn(tm, in[0], NULL, 0);
    }

    for (int j = 0; j < n_strips; j++) {
        const int cur = j & 1;          /* ping/pong index for the current strip */
        const int nxt = 1 - cur;        /* the other buffer, used to prefetch j+1 */

        int sy0 = iniY + j * SH, sh = FAST9_MIN(SH, maxY - sy0);
        int mnx, mny, mxx, mxy;
        fast9_strip_bbox(iniX, maxX, sy0, sh, src_w, src_h, &mnx, &mny, &mxx, &mxy);

        /* prefetch strip j+1 source region into the other input buffer */
        if (j + 1 < n_strips) {
            int sy1 = iniY + (j + 1) * SH, sh1 = FAST9_MIN(SH, maxY - sy1);
            int n_mnx, n_mny, n_mxx, n_mmy;
            fast9_strip_bbox(iniX, maxX, sy1, sh1, src_w, src_h, &n_mnx, &n_mny, &n_mxx, &n_mmy);
            if (fast9_prepare_input(in[nxt], n_mnx, n_mny, n_mxx, n_mmy, MSW, MSH, MSW) != 0) { ret = -3; goto done; }
            xvReqTileTransferIn(tm, in[nxt], NULL, 0);
        }

        xvWaitForTile(tm, in[cur]);          /* wait for this strip's source DMA */

        /* detect + NMS for this strip's pixels (full ROI width, rows
         * [sy0,sy0+sh)). Keypoints appended to `out` in raster order; strips
         * top-to-bottom => global raster, matching refactor. The kernel
         * respects `cap` (returns at cap). */
        fast9_detect_kernel((const uint8_t *)XV_TILE_GET_DATA_PTR(in[cur]), XV_TILE_GET_PITCH(in[cur]),
                            mnx, mny, src_w, src_h,
                            iniX, sy0, roiW, sh,
                            iniX, maxX, iniY, maxY, t,
                            out, n_inout, cap);
        /* no DMA-out: sparse keypoints already in DRAM `out` */
    }

    /* no output tiles to drain */

done:
    xvFreeTile(tm, in[0]);  xvFreeTile(tm, in[1]);
    return ret;
}
