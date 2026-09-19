/* dsp_orb.c — ORB extractor using VP6/DSP bit-exact kernels (accelerated).
 *
 * Replicates ngd_orb_extract() using refoptvp6v2 _full() functions for
 * pixel operations (orb_resize_full, orb_blur_full, ic_angle_full,
 * brief_full) and the fast9 score map (fast9_build_scoremap /
 * fast9_collect_cell — algorithmic restructure of the per-cell fast9_full
 * calls, bit-exact; see src/dsp/fast9_scoremap.c). The quadtree spatial
 * distribution and coordinate bookkeeping are copied verbatim from
 * refactor/src/orb.c.
 *
 * Build: plain C (also compiles as C++). Requires VP6 _core.h headers
 * (include/dsp/). The _full functions are pure C — no TileManager or
 * cstub dependency.
 *
 * Bit-exactness: each per-pixel kernel (blur, resize, FAST-9, IC_Angle,
 * rBRIEF) is validated against the refactor C reference in refoptvp6v2/test.
 * The quadtree distribution is a direct copy of refactor's code, so the
 * full pipeline output is identical modulo NMS differences at 35px cell
 * boundaries (fast9_full uses full-image NMS vs refactor's per-cell NMS).
 */
#include "ngd_app/dsp_orb.h"

#include "dsp/orb_resize_core.h"
#include "dsp/orb_blur_core.h"
#include "dsp/fast9_core.h"
#include "dsp/fast9_scoremap.h"
#include "dsp/ic_angle_core.h"
#include "dsp/brief_core.h"
#include "dsp/dsp_orb_octtree.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef NGD_PI
#define NGD_PI 3.14159265358979323846
#endif

/* ===================================================================== *
 *  DSP ORB extract driver
 * ===================================================================== */
int dsp_orb_extract(const ngd_orb_extractor *ex,
                    const uint8_t *gray, int w, int h, int stride,
                    ngd_keypoint *out_kps, int max_kps,
                    uint8_t *out_desc)
{
    if (!ex || !gray || w <= 0 || h <= 0) return -1;

    /* ---- Phase 1: build pyramid (orb_resize_full) ---- */
    const uint8_t *pyr[NGD_ORB_MAX_LEVELS];
    int pyrW[NGD_ORB_MAX_LEVELS], pyrH[NGD_ORB_MAX_LEVELS];
    for (int level = 0; level < ex->nlevels; ++level) {
        int sw = (level == 0) ? w : pyrW[level-1];
        int sh = (level == 0) ? h : pyrH[level-1];
        int dw = (level == 0) ? w : (int)lroundf((float)w * ex->mvInvScaleFactor[level]);
        int dh = (level == 0) ? h : (int)lroundf((float)h * ex->mvInvScaleFactor[level]);
        if (dw < 2*NGD_ORB_EDGE_THRESHOLD || dh < 2*NGD_ORB_EDGE_THRESHOLD) {
            pyr[level]=NULL; pyrW[level]=0; pyrH[level]=0; continue;
        }
        if (level == 0) {
            /* level 0 IS the input: alias it (skip the per-frame full-image
             * memcpy). Phase 3 does not free the non-owned level-0 pointer. */
            pyr[level] = gray; pyrW[level] = dw; pyrH[level] = dh;
            continue;
        }
        uint8_t *buf = (uint8_t*)malloc((size_t)dw*dh);
        orb_resize_full(pyr[level-1], sw, sh, buf, dw, dh);
        pyr[level] = buf; pyrW[level] = dw; pyrH[level] = dh;
    }

    /* ---- Phase 2: per-level extraction ---- */
    int n_out = 0;
    for (int level = 0; level < ex->nlevels; ++level) {
        const uint8_t *im = pyr[level];
        if (!im) continue;
        int pw = pyrW[level], ph = pyrH[level];

        /* ROI inset: EDGE_THRESHOLD(19) - 3(FAST border adjust) = 16, but
         * refactor uses minBorder = EDGE_THRESHOLD - 3 in both x and y. */
        int minBorderX = NGD_ORB_EDGE_THRESHOLD - 3;
        int minBorderY = minBorderX;
        int maxBorderX = pw - NGD_ORB_EDGE_THRESHOLD + 3;
        int maxBorderY = ph - NGD_ORB_EDGE_THRESHOLD + 3;
        float width = (float)(maxBorderX - minBorderX);
        float height = (float)(maxBorderY - minBorderY);
        if (width < 35 || height < 35) continue;

        int nCols = (int)(width / 35);
        int nRows = (int)(height / 35);
        if (nCols < 1) nCols = 1; if (nRows < 1) nRows = 1;
        int wCell = (int)ceil(width / nCols);
        int hCell = (int)ceil(height / nRows);

        /* FAST-9 detection per grid cell with adaptive threshold.
         * fast9_keypoint is layout-compatible with ngd_keypoint.
         * One threshold-independent score-map scan per level replaces the
         * per-cell fast9_full passes: the high-threshold collection and the
         * empty-cell low-threshold retry both read the same cached scores
         * (score S-1 does not depend on t; see fast9_scoremap.c). */
        int maxKRaw = ex->nfeatures * 10;
        ngd_keypoint *raw = (ngd_keypoint*)malloc(sizeof(ngd_keypoint) * (size_t)maxKRaw);
        int nraw = 0;

        int t_floor = ex->iniThFAST < ex->minThFAST ? ex->iniThFAST : ex->minThFAST;
        uint8_t *scoremap = (uint8_t*)malloc((size_t)pw * ph);
        fast9_build_scoremap(im, pw, ph, pw, t_floor, scoremap, pw);

        for (int i = 0; i < nRows; ++i) {
            int iniY = minBorderY + i*hCell;
            int maxY = iniY + hCell + 6;
            if (iniY >= maxBorderY-3) continue;
            if (maxY > maxBorderY) maxY = maxBorderY;
            for (int j = 0; j < nCols; ++j) {
                int iniX = minBorderX + j*wCell;
                int maxX = iniX + wCell + 6;
                if (iniX >= maxBorderX-6) continue;
                if (maxX > maxBorderX) maxX = maxBorderX;

                int ncell = 0;
                int remain = maxKRaw - nraw;
                fast9_collect_cell(scoremap, pw, iniX, maxX, iniY, maxY,
                                   ex->iniThFAST,
                                   (fast9_keypoint*)(raw + nraw), &ncell, remain);
                if (ncell == 0) {
                    /* cell produced nothing at high threshold → retry low */
                    fast9_collect_cell(scoremap, pw, iniX, maxX, iniY, maxY,
                                       ex->minThFAST,
                                       (fast9_keypoint*)(raw + nraw), &ncell, remain);
                }
                nraw += ncell;
            }
        }
        free(scoremap);

        /* convert absolute pyramid coords → minBorder-relative for quadtree */
        for (int k = 0; k < nraw; ++k) {
            raw[k].x -= (float)minBorderX;
            raw[k].y -= (float)minBorderY;
        }

        /* quadtree spatial distribution */
        ngd_keypoint *kept = (ngd_keypoint*)malloc(sizeof(ngd_keypoint) * (size_t)(ex->nfeatures*2 + 64));
        int nkept = dsp_distribute_octtree(raw, nraw,
                                           minBorderX, maxBorderX, minBorderY, maxBorderY,
                                           ex->mnFeaturesPerLevel[level], kept);
        free(raw);

        /* restore absolute coords, set octave + size */
        int scaledPatch = (int)(NGD_ORB_PATCH_SIZE * ex->mvScaleFactor[level]);
        for (int i = 0; i < nkept; ++i) {
            kept[i].x += (float)minBorderX;
            kept[i].y += (float)minBorderY;
            kept[i].octave = level;
            kept[i].size = (float)scaledPatch;
        }

        /* Gaussian blur (sliding-window DSP kernel, bit-exact with
         * orb_blur_full); orientation uses raw image, descriptor blurred.
         * Skipped entirely when the quadtree kept nothing on this level —
         * the blurred image feeds only the (empty) descriptor loop. */
        uint8_t *blurred = NULL;
        if (nkept > 0) {
            blurred = (uint8_t*)malloc((size_t)pw*ph);
            orb_blur_full_opt(im, pw, ph, pw, blurred);
        }

        for (int i = 0; i < nkept; ++i) {
            /* IC_Angle on the unblurred level image (in-frame fast path) */
            kept[i].angle = ic_angle_full_opt(im, pw, ph, pw,
                                              (int)lroundf(kept[i].x), (int)lroundf(kept[i].y),
                                              ex->umax);

            /* rescale to level-0 coordinates */
            float sx = (level == 0) ? 1.0f : ex->mvScaleFactor[level];
            ngd_keypoint out = kept[i];
            out.x = kept[i].x * sx;
            out.y = kept[i].y * sx;

            if (n_out < max_kps) {
                out_kps[n_out] = out;
                /* rBRIEF on the blurred level image (in-frame fast path) */
                if (out_desc) {
                    brief_full_opt(blurred, pw, ph, pw,
                                   (int)lroundf(kept[i].x), (int)lroundf(kept[i].y),
                                   kept[i].angle,
                                   out_desc + (size_t)n_out * 32);
                }
                n_out++;
            }
        }
        free(blurred);
        free(kept);
    }

    /* ---- Phase 3: cleanup (level 0 is the caller's gray, not owned) ---- */
    for (int level = 1; level < ex->nlevels; ++level) free((void*)pyr[level]);
    return n_out;
}
