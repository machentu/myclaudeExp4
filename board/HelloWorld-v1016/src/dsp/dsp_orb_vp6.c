/* dsp_orb_vp6.c — ORB extractor orchestrating the four VP6+IVP drivers.
 *
 * Mirror of dsp_orb_extract() (src/dsp_orb.c) with the per-pixel work moved
 * onto the persistent bank-pool / raw-iDMA drivers:
 *   orb_resize_full       -> orb_resize_strip_vp6       (bank-1 strip sweep;
 *                              pure-C fallback on resource failure)
 *   fast9_build_scoremap  -> fast9_scoremap_vp6_build   (whole-frame strips)
 *   orb_blur_full_opt     -> orb_blur_vp6_process       (64x64 tile grid)
 *   ic_angle_full_opt     -> ic_angle_vp6_process       (whole-frame strips)
 *   brief_full_opt        -> brief_vp6_process          (whole-frame strips)
 * Everything else (cell grid, low-threshold retry, quadtree via
 * dsp_distribute_octtree, coordinate bookkeeping, output ordering) is copied
 * verbatim, so the output is byte-identical (test_dsp_orb_vp6 memcmps both).
 *
 * Driver differences from the pure-C loop, all output-preserving:
 *  - IC angle / BRIEF run per LEVEL over all kept keypoints at once (the
 *    drivers write angles[i] / desc[i*32] by index, order-independent) instead
 *    of per-keypoint inside the emit loop; BRIEF may compute a few descriptors
 *    beyond max_kps that the pure-C loop skips — they are simply not copied.
 *  - `kept[i].angle` is filled from angles[] BEFORE brief_vp6_process (the
 *    driver reads kps[i].angle), matching the pure-C order angle->brief.
 *
 * Build: plain C (also compiles as C++). scoremap/blur buffers are per-level
 * mallocs like the pure-C path; the persistent SRAM pools live inside the
 * drivers (orb_tile_manager.h).
 */
#include "dsp/dsp_orb_vp6.h"
#include "dsp/dsp_calib_vp6.h"   /* on-chip cost calibration (v1005) */

#define BRIEF_INT_VP6 1 //need add this

#include "dsp/orb_resize_core.h"
#include "dsp/orb_resize_strip_vp6.h"
#include "dsp/fast9_core.h"
#include "dsp/fast9_scoremap.h"
#include "dsp/fast9_scoremap_vp6.h"
#include "dsp/orb_blur_vp6.h"
#include "dsp/ic_angle_vp6.h"
#include "dsp/brief_vp6.h"
#ifdef BRIEF_INT_VP6
#include "dsp/brief_int_vp6.h"
#endif
#include "dsp/dsp_orb_octtree.h"

#ifdef __XTENSA__
#include <xtensa/tie/xt_ivpn.h>   /* IVP types/intrinsics: collect prefilter
                                     (v1006) is the first user in this file —
                                     must sit at file scope, the real core's
                                     headers define functions/objects */
#endif

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define DSP_ORB_VP6_TIMING 1

/* ---- per-stage cycle profiling (Xtensa CCOUNT; see dsp_orb_vp6.h) ----
 * The fy01_202210 core has XCHAL_HAVE_CCOUNT=1, so rsr.ccount gives the
 * simulator's cycle counter directly. Compile with -DDSP_ORB_VP6_TIMING
 * to accumulate per-stage deltas; with the macro off every mark/acc below
 * compiles to nothing. */
unsigned long long dsp_orb_vp6_cycles[DSP_ORB_VP6_T_COUNT];
unsigned long long dsp_orb_vp6_calls;
unsigned long long dsp_orb_vp6_qt[DSP_ORB_VP6_Q_COUNT];
unsigned long long dsp_orb_vp6_cs[DSP_ORB_VP6_C_COUNT];

#ifdef DSP_ORB_VP6_TIMING
#include <stdio.h>
#ifdef __XTENSA__
/* DSP: rsr.ccount — the simulator's cycle counter (XCHAL_HAVE_CCOUNT=1). */
typedef unsigned int orb_tick_t;
static orb_tick_t orb_vp6_tick(void)
{
    unsigned int c;
    __asm__ volatile ("rsr.ccount %0" : "=a" (c));
    return c;
}
#else
/* Host: wall clock — lets the PC test print the same per-stage breakdown
 * (in us). Host numbers are NOT DSP cycles (host has FPU + cache, and the
 * cstub emulates IVP in C++), but they show the stage mix on real data. */
#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif
typedef unsigned long long orb_tick_t;
static orb_tick_t orb_vp6_tick(void)
{
#ifdef _WIN32
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    return (orb_tick_t)li.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (orb_tick_t)ts.tv_sec * 1000000000ull + (orb_tick_t)ts.tv_nsec;
#endif
}
static double orb_vp6_ticks_to_us(orb_tick_t t)
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
#define ORB_TMARK(v)     do { (v) = orb_vp6_tick(); } while (0)
#define ORB_TACC(v, slot) \
    do { dsp_orb_vp6_cycles[slot] += orb_vp6_tick() - (orb_tick_t)(v); } while (0)
#define ORB_QTACC(v, slot) \
    do { dsp_orb_vp6_qt[slot] += orb_vp6_tick() - (orb_tick_t)(v); } while (0)
#define ORB_CSACC(v, slot) \
    do { dsp_orb_vp6_cs[slot] += orb_vp6_tick() - (orb_tick_t)(v); } while (0)
#else
#define ORB_TMARK(v)     do { } while (0)
#define ORB_TACC(v, slot) do { } while (0)
#define ORB_QTACC(v, slot) do { } while (0)
#define ORB_CSACC(v, slot) do { } while (0)
#endif

/* v1010: synchronous block mover for the quadtree bank staging
 * (dsp_octtree_set_scratch) — iDMA request + wait, memcpy if rejected. */
static void qt_move(void *ctx, void *dst, const void *src, int bytes)
{
    xvTileManager *tm = (xvTileManager *)ctx;
    int dma = xvAddIdmaRequest(tm, dst, (void *)src, (size_t)bytes, 1,
                               (size_t)bytes, (size_t)bytes, 0);
    if (dma >= 0) xvWaitForiDMA(tm, dma);
    else          memcpy(dst, src, (size_t)bytes);
}

int dsp_orb_vp6_extract(xvTileManager *tm,
                        const ngd_orb_extractor *ex,
                        const uint8_t *gray, int w, int h, int stride,
                        ngd_keypoint *out_kps, int max_kps,
                        uint8_t *out_desc)
{
    if (!ex || !gray || w <= 0 || h <= 0) return -1;
    if (!tm) return -1;
    if (w > DSP_ORB_VP6_MAX_W) return -1;
    /* dsp_orb_extract addresses the level-0 image with pitch w (its `stride`
     * argument is unused); enforce the same contract so both paths agree on
     * every accepted input. */
    if (stride != w) return -1;

#ifdef DSP_ORB_VP6_TIMING
    orb_tick_t orb_t, orb_t0, orb_qt;   /* stage mark / whole-call / sub-stage */
    orb_tick_t orb_ct;                  /* collect sub-stage (dma / scan)      */
    ++dsp_orb_vp6_calls;
    ORB_TMARK(orb_t0);
#endif

    /* ---- Phase 1: build pyramid (orb_resize_strip_vp6, IVP strip driver
     * with pure-C fallback — bit-exact, see orb_resize_strip_vp6.c) ---- */
    const uint8_t *pyr[NGD_ORB_MAX_LEVELS];
    int pyrW[NGD_ORB_MAX_LEVELS], pyrH[NGD_ORB_MAX_LEVELS];
    ORB_TMARK(orb_t);
    for (int level = 0; level < ex->nlevels; ++level) {
        int sw = (level == 0) ? w : pyrW[level-1];
        int sh = (level == 0) ? h : pyrH[level-1];
        int dw = (level == 0) ? w : (int)lroundf((float)w * ex->mvInvScaleFactor[level]);
        int dh = (level == 0) ? h : (int)lroundf((float)h * ex->mvInvScaleFactor[level]);
        if (dw < 2*NGD_ORB_EDGE_THRESHOLD || dh < 2*NGD_ORB_EDGE_THRESHOLD) {
            pyr[level]=NULL; pyrW[level]=0; pyrH[level]=0; continue;
        }
        if (level == 0) {
            /* level 0 IS the input: alias it. Phase 3 does not free it. */
            pyr[level] = gray; pyrW[level] = dw; pyrH[level] = dh;
            continue;
        }
        uint8_t *buf = (uint8_t*)malloc((size_t)dw*dh);
        /* IVP strip driver (bank-1 buffers, bit-exact); fall back to the
         * pure-C golden on any resource failure - outputs are identical. */
        if (orb_resize_strip_vp6(tm, pyr[level-1], sw, sh, buf, dw, dh) != 0)
            orb_resize_full(pyr[level-1], sw, sh, buf, dw, dh);
        pyr[level] = buf; pyrW[level] = dw; pyrH[level] = dh;
    }
    ORB_TACC(orb_t, DSP_ORB_VP6_T_RESIZE);
    /* hand bank 1 back before phase 2: the four pixel drivers hold their own
     * persistent strips there, and the two sets must not coexist */
    orb_resize_strip_vp6_release();

    /* ---- Phase 2: per-level extraction (VP6+IVP drivers) ---- */
    int n_out = 0, ret = 0;
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

        /* per-level scratch; all freed at level_done (NULL-safe double free).
         * Level-scope locals are declared up front (assigned later) so the
         * level_done gotos never cross an initialization — the file must
         * compile as both C and C++ (the Linux DSP build feeds .c to
         * xt-clang++). */
        ngd_keypoint *raw = NULL, *kept = NULL;
        uint8_t *scoremap = NULL, *descbuf = NULL, *blurred = NULL;
        float *angles = NULL;
        xvFrame *im_fr = NULL, *bl_fr = NULL;
        int rc = 0, nraw = 0, nkept = 0, scaledPatch = 0;

        int maxKRaw = ex->nfeatures * 10;
        raw = (ngd_keypoint*)malloc(sizeof(ngd_keypoint) * (size_t)maxKRaw);

        int t_floor = ex->iniThFAST < ex->minThFAST ? ex->iniThFAST : ex->minThFAST;
        scoremap = (uint8_t*)malloc((size_t)pw * ph);

        /* wrap the level image for the drivers (pitch = pw: level 0 is the
         * caller's tight gray, deeper levels are tight resize outputs) */
        im_fr = xvCreateFrame(tm, (uint8_t*)(uintptr_t)im,
                              (uint32_t)pw * (uint32_t)ph, pw, ph, pw,
                              1, 1, FRAME_EDGE_PADDING, 0);
        if (!im_fr) { rc = -2; goto level_done; }

        ORB_TMARK(orb_t);
        rc = fast9_scoremap_vp6_build(tm, im_fr, pw, ph, t_floor,
                                      scoremap, pw, NULL);
        if (rc < 0) goto level_done;
        ORB_TACC(orb_t, DSP_ORB_VP6_T_SCOREMAP);

        ORB_TMARK(orb_t);
        /* Board v1002: CPU byte loads from the DDR heap cost ~150 cycles each
         * (collect ran 7.3x sim). Stage each cell-row band of the scoremap
         * into bank SRAM with one 2D iDMA and scan it there. The scan pointer
         * is shifted so the collect call keeps ABSOLUTE y coords (v1003 wrote
         * band-relative y: n=1009 vs golden 1011); on any buffer/DMA failure
         * fall back to the in-place scan (same output). */
        {
            int band_rows = hCell + 6;
            uint8_t *band = (uint8_t *)xvAllocateBuffer(
                tm, (size_t)pw * (size_t)band_rows, XV_MEM_BANK_COLOR_1, 64);
            if (band == (void *)(intptr_t)XVTM_ERROR) band = NULL;
            /* v1006 chunk-hit prefilter reverted in v1007: board showed hit
             * rate ~100% (at t_floor=7 nearly every 64px chunk holds some
             * byte >= t_floor), so the bitmap build + skip test was pure
             * overhead (collect 174 -> 186 ms). fast9_collect_cell keeps
             * the hitmask parameter for a finer-grained future variant. */
            for (int i = 0; i < nRows; ++i) {
                int iniY = minBorderY + i*hCell;
                int maxY = iniY + hCell + 6;
                if (iniY >= maxBorderY-3) continue;
                if (maxY > maxBorderY) maxY = maxBorderY;

                /* scan[y*pw + x] with y in [iniY, maxY) must hit the buffer:
                 * in-place = scoremap, staged = band shifted back to y=0 */
                const uint8_t *scan = scoremap;
                if (band) {
                    ORB_TMARK(orb_ct);
                    int rows = maxY - iniY;
                    int dma = xvAddIdmaRequest(tm, band,
                        (void *)(scoremap + (size_t)iniY * pw),
                        (size_t)pw, rows, pw, pw, 0);
                    if (dma >= 0) {
                        xvWaitForiDMA(tm, dma);
                        scan = band - (size_t)iniY * pw;
                    }
                    ORB_CSACC(orb_ct, DSP_ORB_VP6_C_DMA);
                }

                ORB_TMARK(orb_ct);
                for (int j = 0; j < nCols; ++j) {
                    int iniX = minBorderX + j*wCell;
                    int maxX = iniX + wCell + 6;
                    if (iniX >= maxBorderX-6) continue;
                    if (maxX > maxBorderX) maxX = maxBorderX;

                    int ncell = 0;
                    int remain = maxKRaw - nraw;
                    fast9_collect_cell(scan, pw, iniX, maxX, iniY, maxY,
                                       ex->iniThFAST,
                                       (fast9_keypoint*)(raw + nraw), &ncell, remain,
                                       NULL, 0);
                    if (ncell == 0) {
                        /* cell produced nothing at high threshold → retry low */
                        fast9_collect_cell(scan, pw, iniX, maxX, iniY, maxY,
                                           ex->minThFAST,
                                           (fast9_keypoint*)(raw + nraw), &ncell, remain,
                                           NULL, 0);
                    }
                    nraw += ncell;
                }
                ORB_CSACC(orb_ct, DSP_ORB_VP6_C_SCAN);
            }
            if (band) xvFreeBuffer(tm, band);
        }
        free(scoremap); scoremap = NULL;
        ORB_TACC(orb_t, DSP_ORB_VP6_T_COLLECT);

        /* convert absolute pyramid coords → minBorder-relative for quadtree */
        ORB_TMARK(orb_t);
        for (int k = 0; k < nraw; ++k) {
            raw[k].x -= (float)minBorderX;
            raw[k].y -= (float)minBorderY;
        }
        ORB_QTACC(orb_t, DSP_ORB_VP6_Q_PREP);

        /* quadtree spatial distribution */
        ORB_TMARK(orb_qt);
        kept = (ngd_keypoint*)malloc(sizeof(ngd_keypoint) * (size_t)(ex->nfeatures*2 + 64));
        {
            /* v1010: stage the quadtree partitions in bank SRAM — the v1009
             * pool form still paid ld.heap=83.9 for every arena/qtmp/pos
             * touch (rounds 176 ms). Bank 0 is the roomier side at this
             * point (resize strips released, only blur's ~27KB tiles stay
             * there; the strip drivers' pongs + brief sit in bank 1), so
             * probe it first with large sizes and step down over both banks
             * like the band alloc does; no scratch at all -> the octtree
             * keeps its in-place DDR form (same output). */
            void *qtsc = NULL;
            int qtsc_bytes = 0;
            static const int qt_try[6] = { 96*1024, 64*1024, 32*1024,
                                           16*1024, 8*1024, 4*1024 };
            for (int t = 0; t < 6 && !qtsc; ++t) {
                uint8_t *p = (uint8_t *)xvAllocateBuffer(tm, qt_try[t],
                                                         XV_MEM_BANK_COLOR_0, 64);
                if (p == (void *)(intptr_t)XVTM_ERROR)
                    p = (uint8_t *)xvAllocateBuffer(tm, qt_try[t],
                                                    XV_MEM_BANK_COLOR_1, 64);
                if (p != (void *)(intptr_t)XVTM_ERROR) {
                    qtsc = p; qtsc_bytes = qt_try[t];
                }
            }
            dsp_octtree_set_scratch(qtsc, qtsc_bytes,
                                    qtsc ? qt_move : NULL, tm);
            nkept = dsp_distribute_octtree(raw, nraw,
                                           minBorderX, maxBorderX, minBorderY, maxBorderY,
                                           ex->mnFeaturesPerLevel[level], kept);
            dsp_octtree_set_scratch(NULL, 0, NULL, NULL);
            if (qtsc) xvFreeBuffer(tm, qtsc);
        }
        free(raw); raw = NULL;
        ORB_QTACC(orb_qt, DSP_ORB_VP6_Q_TREE);

        /* restore absolute coords, set octave + size */
        ORB_TMARK(orb_qt);
        scaledPatch = (int)(NGD_ORB_PATCH_SIZE * ex->mvScaleFactor[level]);
        for (int i = 0; i < nkept; ++i) {
            kept[i].x += (float)minBorderX;
            kept[i].y += (float)minBorderY;
            kept[i].octave = level;
            kept[i].size = (float)scaledPatch;
        }
        ORB_QTACC(orb_qt, DSP_ORB_VP6_Q_POST);
        ORB_TACC(orb_t, DSP_ORB_VP6_T_QUADTREE);

        if (nkept > 0) {
            /* IC angle on the unblurred level image, all keypoints at once */
            ORB_TMARK(orb_t);
            angles = (float*)malloc(sizeof(float) * (size_t)nkept);
            rc = ic_angle_vp6_process(tm, im_fr, pw, ph,
                                      (const fast9_keypoint*)kept, angles, nkept,
                                      ex->umax, NULL);
            if (rc < 0) goto level_done;
            for (int i = 0; i < nkept; ++i) kept[i].angle = angles[i];
            ORB_TACC(orb_t, DSP_ORB_VP6_T_ICANGLE);

            /* blur (skip only if the quadtree kept nothing — guarded above) */
            ORB_TMARK(orb_t);
            blurred = (uint8_t*)malloc((size_t)pw*ph);
            descbuf = (uint8_t*)malloc((size_t)nkept * 32);
            bl_fr = xvCreateFrame(tm, blurred, (uint32_t)pw * (uint32_t)ph,
                                  pw, ph, pw, 1, 1, FRAME_ZERO_PADDING, 0);
            if (!bl_fr) { rc = -2; goto level_done; }
            rc = orb_blur_vp6_process(tm, im_fr, bl_fr, pw, ph, NULL);
            if (rc < 0) goto level_done;
            ORB_TACC(orb_t, DSP_ORB_VP6_T_BLUR);

            /* rBRIEF on the blurred level image, all keypoints at once.
             * BRIEF_INT_VP6 selects the fixed-point LUT variant (NOT
             * bit-exact with the pure-C path - descriptors differ by a few
             * bits/kp); default stays on the bit-exact brief_vp6. */
            ORB_TMARK(orb_t);
#ifdef BRIEF_INT_VP6
            rc = brief_int_vp6_process(tm, bl_fr, pw, ph,
                                       (const fast9_keypoint*)kept, descbuf, nkept, NULL);
#else
            rc = brief_vp6_process(tm, bl_fr, pw, ph,
                                   (const fast9_keypoint*)kept, descbuf, nkept, NULL);
#endif
            if (rc < 0) goto level_done;
            ORB_TACC(orb_t, DSP_ORB_VP6_T_BRIEF);
        }

        ORB_TMARK(orb_t);
        for (int i = 0; i < nkept; ++i) {
            /* rescale to level-0 coordinates */
            float sx = (level == 0) ? 1.0f : ex->mvScaleFactor[level];
            ngd_keypoint out = kept[i];
            out.x = kept[i].x * sx;
            out.y = kept[i].y * sx;

            if (n_out < max_kps) {
                out_kps[n_out] = out;
                if (out_desc)
                    memcpy(out_desc + (size_t)n_out * 32,
                           descbuf + (size_t)i * 32, 32);
                n_out++;
            }
        }
        ORB_TACC(orb_t, DSP_ORB_VP6_T_EMIT);

level_done:
        free(raw); free(scoremap); free(kept); free(angles);
        free(descbuf); free(blurred);
        if (im_fr) xvFreeFrame(tm, im_fr);
        if (bl_fr) xvFreeFrame(tm, bl_fr);
        if (rc < 0) { ret = rc; break; }
    }

    /* ---- Phase 3: cleanup (level 0 is the caller's gray, not owned) ---- */
    for (int level = 1; level < ex->nlevels; ++level) free((void*)pyr[level]);
    /* Board v1002 measured: with the four pixel drivers' strips left
     * resident in bank 1, the NEXT call's resize set no longer fits and
     * every level falls back to pure-C resize (645 ms vs 27 ms, 24x; the
     * C fallback is bit-exact so golden cannot catch it). Hand the strips
     * back so each call starts from the same bank state as the first. */
    fast9_scoremap_vp6_release();
    ic_angle_vp6_release();
    orb_blur_vp6_release();
#ifdef BRIEF_INT_VP6
    brief_int_vp6_release();
#else
    brief_vp6_release();
#endif
    ORB_TACC(orb_t0, DSP_ORB_VP6_T_TOTAL);
    return ret ? ret : n_out;
}

void dsp_orb_vp6_timing_report(void)
{
#ifdef DSP_ORB_VP6_TIMING
    unsigned long long stages = 0;
    for (int i = 1; i < DSP_ORB_VP6_T_COUNT; ++i)
        stages += dsp_orb_vp6_cycles[i];
#ifdef __XTENSA__
    printf("[orb_vp6] calls    : %llu\n", dsp_orb_vp6_calls);
    printf("[orb_vp6] TOTAL    : %llu cycles\n",
           dsp_orb_vp6_cycles[DSP_ORB_VP6_T_TOTAL]);
    printf("[orb_vp6] resize   : %llu\n",
           dsp_orb_vp6_cycles[DSP_ORB_VP6_T_RESIZE]);
    printf("[orb_vp6] scoremap : %llu\n",
           dsp_orb_vp6_cycles[DSP_ORB_VP6_T_SCOREMAP]);
    printf("[orb_vp6] collect  : %llu\n",
           dsp_orb_vp6_cycles[DSP_ORB_VP6_T_COLLECT]);
    printf("[orb_vp6] quadtree : %llu\n",
           dsp_orb_vp6_cycles[DSP_ORB_VP6_T_QUADTREE]);
    printf("[orb_vp6] ic_angle : %llu\n",
           dsp_orb_vp6_cycles[DSP_ORB_VP6_T_ICANGLE]);
    printf("[orb_vp6] blur     : %llu\n",
           dsp_orb_vp6_cycles[DSP_ORB_VP6_T_BLUR]);
    printf("[orb_vp6] brief    : %llu\n",
           dsp_orb_vp6_cycles[DSP_ORB_VP6_T_BRIEF]);
    printf("[orb_vp6] emit     : %llu\n",
           dsp_orb_vp6_cycles[DSP_ORB_VP6_T_EMIT]);
    printf("[orb_vp6] other    : %llu\n",
           dsp_orb_vp6_cycles[DSP_ORB_VP6_T_TOTAL] - stages);
#else
    /* host: ticks -> microseconds; stage mix only, NOT DSP cycles */
    double us[ DSP_ORB_VP6_T_COUNT ];
    for (int i = 0; i < DSP_ORB_VP6_T_COUNT; ++i)
        us[i] = orb_vp6_ticks_to_us(dsp_orb_vp6_cycles[i]);
    double stages_us = 0;
    for (int i = 1; i < DSP_ORB_VP6_T_COUNT; ++i)
        stages_us += us[i];
    printf("[orb_vp6] calls    : %llu\n", dsp_orb_vp6_calls);
    printf("[orb_vp6] TOTAL    : %10.1f us\n", us[DSP_ORB_VP6_T_TOTAL]);
    printf("[orb_vp6] resize   : %10.1f us\n", us[DSP_ORB_VP6_T_RESIZE]);
    printf("[orb_vp6] scoremap : %10.1f us\n", us[DSP_ORB_VP6_T_SCOREMAP]);
    printf("[orb_vp6] collect  : %10.1f us\n", us[DSP_ORB_VP6_T_COLLECT]);
    printf("[orb_vp6] quadtree : %10.1f us\n", us[DSP_ORB_VP6_T_QUADTREE]);
    printf("[orb_vp6] ic_angle : %10.1f us\n", us[DSP_ORB_VP6_T_ICANGLE]);
    printf("[orb_vp6] blur     : %10.1f us\n", us[DSP_ORB_VP6_T_BLUR]);
    printf("[orb_vp6] brief    : %10.1f us\n", us[DSP_ORB_VP6_T_BRIEF]);
    printf("[orb_vp6] emit     : %10.1f us\n", us[DSP_ORB_VP6_T_EMIT]);
    printf("[orb_vp6] other    : %10.1f us\n", us[DSP_ORB_VP6_T_TOTAL] - stages_us);
#endif
#ifdef BRIEF_INT_VP6
    brief_int_vp6_timing_report();   /* [brief int] sub-stages, same harness */
#endif
#else
    /* no timing build: nothing to report */
#endif
}

/* ===================================================================== *
 *  On-chip cost calibration (v1005) — lives in this file (not a new .c)
 *  so the Xplorer-generated build list on the XCC machine stays valid.
 *  See dsp_calib_vp6.h for the measurement set and units.
 * ===================================================================== */
unsigned long long dsp_calib_vp6_res[DSP_CALIB_COUNT];
unsigned long long dsp_calib2_vp6_res[DSP_CALIB2_COUNT];
unsigned long long dsp_calib3_vp6_res[DSP_CALIB3_COUNT];
unsigned long long dsp_calib4_vp6_res[DSP_CALIB4_COUNT];

#ifndef __XTENSA__
void dsp_calib_vp6_run(xvTileManager *tm) { (void)tm; }
#else /* __XTENSA__ */
/* xt_ivpn.h included at the first user (collect prefilter) above */

static unsigned int cal_tick(void)
{
    unsigned int c;
    __asm__ volatile ("rsr.ccount %0" : "=a" (c));
    return c;
}

/* single u32 read that cannot be optimized away */
static volatile unsigned int cal_sink;

#define CAL_BYTES  4096   /* working set per pass                       */
#define CAL_SLACK  16     /* LAV/gather streams read up to +6 past it   */
#define CAL_OUTER  8      /* repeats of the whole pass                  */

/* whole-test cycles -> cycles/op x100 */
static unsigned long long cal_norm(unsigned long long cyc, unsigned long long n)
{
    return n ? cyc * 100 / n : 0;
}

static unsigned long long cal_scalar_ld(const uint8_t *buf)
{
    unsigned long long t0 = cal_tick();
    unsigned long long ops = 0;
    for (int o = 0; o < CAL_OUTER; ++o) {
        unsigned int s = 0;
        for (int i = 0; i < CAL_BYTES; ++i) s += buf[i];
        cal_sink = s;
        ops += CAL_BYTES;
    }
    return cal_norm(cal_tick() - t0, ops);
}

static unsigned long long cal_vec_lv(const uint8_t *buf)
{
    unsigned long long t0 = cal_tick();
    unsigned long long ops = 0;
    for (int o = 0; o < CAL_OUTER; ++o) {
        unsigned char s = 0;
        for (int i = 0; i < CAL_BYTES; i += 64) {
            xb_vec2Nx8U v = IVP_LV2NX8U_X(
                (const xb_vec2Nx8U *)(const void *)(buf + i), 0);
            s ^= *(const unsigned char *)&v;
        }
        cal_sink = s;
        ops += CAL_BYTES / 64;
    }
    return cal_norm(cal_tick() - t0, ops);
}

/* n_streams concurrent unaligned LAV streams (blur hpass shape) */
static unsigned long long cal_lav(const uint8_t *buf, int n_streams)
{
    unsigned long long t0 = cal_tick();
    unsigned long long ops = 0;
    for (int o = 0; o < CAL_OUTER; ++o) {
        const xb_vec2Nx8U *lp[7];
        valign lv[7];
        for (int k = 0; k < n_streams; ++k) {
            lp[k] = (const xb_vec2Nx8U *)(const void *)(buf + k);
            lv[k] = IVP_LA2NX8U_PP(lp[k]);
        }
        unsigned char s = 0;
        for (int i = 0; i + 63 < CAL_BYTES; i += 64) {
            for (int k = 0; k < n_streams; ++k) {
                xb_vec2Nx8U v;
                IVP_LAV2NX8U_XP(v, lv[k], lp[k], 64);
                s ^= *(const unsigned char *)&v;
            }
            ops += n_streams;
        }
        cal_sink = s;
    }
    return cal_norm(cal_tick() - t0, ops);
}

/* 7-tap stride-1 gather (resize shape): GATHERAN x2 + GATHERD per tap */
static unsigned long long cal_gather7(const uint8_t *buf)
{
    static uint16_t offs_lo[32], offs_hi[32];
    for (int i = 0; i < 32; ++i) {
        offs_lo[i] = (uint16_t)i;
        offs_hi[i] = (uint16_t)(i + 32);
    }
    unsigned long long t0 = cal_tick();
    unsigned long long ops = 0;
    for (int o = 0; o < CAL_OUTER; ++o) {
        unsigned char s = 0;
        for (int i = 0; i + 63 < CAL_BYTES; i += 64) {
            const uint8_t *base = buf + i;
            for (int k = 0; k < 7; ++k) {
                xb_gsr ga = IVP_GATHERANX8U(base + k,
                    *(const xb_vecNx16U *)(const void *)offs_lo);
                xb_gsr gb = IVP_GATHERANX8U(base + k,
                    *(const xb_vecNx16U *)(const void *)offs_hi);
                xb_vec2Nx8U v = IVP_GATHERD2NX8U(gb, ga);
                s ^= *(const unsigned char *)&v;
                ops += 3;
            }
        }
        cal_sink = s;
    }
    return cal_norm(cal_tick() - t0, ops);
}

/* MULUS + 6x MULUSPA chain on a resident vector (no memory traffic) */
static unsigned long long cal_mac7(const uint8_t *buf)
{
    xb_vec2Nx8U v = IVP_LV2NX8U_X(
        (const xb_vec2Nx8U *)(const void *)buf, 0);
    unsigned long long t0 = cal_tick();
    unsigned long long ops = 0;
    for (int o = 0; o < CAL_OUTER; ++o) {
        xb_vec2Nx24 acc = IVP_MULUS2N8XR16(v, 7);
        for (int k = 1; k < 7; ++k)
            IVP_MULUSPA2N8XR16(acc, v, v, 7);
        xb_vecNx16 lo = IVP_CVT16U2NX24L(acc);
        cal_sink ^= *(const unsigned char *)&lo;
        ops += 7;
    }
    return cal_norm(cal_tick() - t0, ops);
}

/* u16 vector stores to bank (per u16 stored) */
static unsigned long long cal_sv16(uint8_t *buf, const xb_vecNx16 *vsrc)
{
    unsigned long long t0 = cal_tick();
    unsigned long long ops = 0;
    for (int o = 0; o < CAL_OUTER; ++o) {
        for (int i = 0; i + 63 < CAL_BYTES; i += 64) {
            xb_vecNx16U *p = (xb_vecNx16U *)(void *)(buf + i);
            IVP_SVNX16U_IP(*(const xb_vecNx16U *)vsrc, p, 64);
            ops += 32;
        }
    }
    return cal_norm(cal_tick() - t0, ops);
}

/* float mul+add pair (soft- vs hard-float cost) */
static unsigned long long cal_fadd(void)
{
    volatile float fs = 1.5f;
    unsigned long long t0 = cal_tick();
    for (int i = 0; i < 4096; ++i)
        fs = fs * 1.000001f + 0.5f;
    cal_sink = (unsigned int)fs;
    return cal_norm(cal_tick() - t0, 4096);
}

/* CPU memset heap (per byte written) */
static unsigned long long cal_memset_h(uint8_t *buf)
{
    unsigned long long t0 = cal_tick();
    for (int o = 0; o < CAL_OUTER; ++o)
        memset(buf, o, CAL_BYTES);
    return cal_norm(cal_tick() - t0, (unsigned long long)CAL_OUTER * CAL_BYTES);
}

/* ================= v1006 CAL2: blur/quadtree decomposition ============= */

/* CPU u8 load from the C stack — prices register spills if the runtime
 * stack is mapped in DDR (stack size unknown, keep the frame small) */
static unsigned long long cal_stack_ld(void)
{
    volatile uint8_t loc[256];
    unsigned long long t0 = cal_tick();
    unsigned long long ops = 0;
    for (int o = 0; o < 8 * CAL_OUTER; ++o) {
        unsigned int s = 0;
        for (int i = 0; i < 256; ++i) s += loc[i];
        cal_sink = s;
        ops += 256;
    }
    return cal_norm(cal_tick() - t0, ops);
}

/* malloc(64)+free round trip — the octtree does one pair per node/array */
static unsigned long long cal_malloc_pair(void)
{
    unsigned long long t0 = cal_tick();
    enum { N_MP = 64 };
    for (int i = 0; i < N_MP; ++i) {
        void *p = malloc(64);
        cal_sink ^= (unsigned int)(uintptr_t)p;
        free(p);
    }
    return cal_norm(cal_tick() - t0, N_MP);
}

/* one blur-hpass row over a staged region row: 7 stream primes + one
 * 64-lane chunk (the tile driver's exact per-row shape; tile_w = 64) */
static void cal_hpass_row(const uint8_t *row, uint16_t *trow, const int gk[7])
{
    const xb_vec2Nx8U *lp[7];
    valign lv[7];
    for (int k = 0; k < 7; ++k) {
        lp[k] = (const xb_vec2Nx8U *)(const void *)(row + k);
        lv[k] = IVP_LA2NX8U_PP(lp[k]);
    }
    xb_vec2Nx8U v0;
    IVP_LAV2NX8U_XP(v0, lv[0], lp[0], 64);
    xb_vec2Nx24 acc = IVP_MULUS2N8XR16(v0, gk[0]);
    for (int k = 1; k < 7; ++k) {
        xb_vec2Nx8U vk;
        IVP_LAV2NX8U_XP(vk, lv[k], lp[k], 64);
        IVP_MULUSPA2N8XR16(acc, vk, vk, gk[k]);
    }
    xb_vecNx16 lo16 = IVP_CVT16U2NX24L(acc);
    xb_vecNx16 hi16 = IVP_CVT16U2NX24H(acc);
    xb_vecNx16U *pl = (xb_vecNx16U *)(void *)trow;
    xb_vecNx16U *ph = (xb_vecNx16U *)(void *)(trow + 32);
    IVP_SVNX16U_IP(*(const xb_vecNx16U *)&lo16, pl, (int)sizeof(lo16));
    IVP_SVNX16U_IP(*(const xb_vecNx16U *)&hi16, ph, (int)sizeof(hi16));
}

/* 7 stream primes + one consuming LAV (per row) — isolates the per-row
 * LA2NX8U_PP cost; the trailing LAV (~3 cyc, cf. cal.lav1) keeps the
 * valigns live so the primes cannot be optimized away */
static void cal_hpass_primes(const uint8_t *row)
{
    const xb_vec2Nx8U *lp[7];
    valign lv[7];
    for (int k = 0; k < 7; ++k) {
        lp[k] = (const xb_vec2Nx8U *)(const void *)(row + k);
        lv[k] = IVP_LA2NX8U_PP(lp[k]);
    }
    xb_vec2Nx8U v;
    IVP_LAV2NX8U_XP(v, lv[0], lp[0], 64);
    cal_sink ^= *(const unsigned char *)&v;
}

/* one blur-vpass output chunk: 14 aligned LV from a u16 tmp (COLOR_0) +
 * the UNPK/CVT24/PACKL plane rebuild + 14 MAC + shift-fix + SA store */
static void cal_vpass_chunk(const uint8_t *tr0, int tmp_pitch_bytes,
                            uint8_t *drow, const int gk[7])
{
    const uint8_t *tr[7];
    for (int k = 0; k < 7; ++k)
        tr[k] = tr0 + (size_t)k * (size_t)tmp_pitch_bytes;
    xb_vec2Nx8U *mp = (xb_vec2Nx8U *)(void *)drow;
    valign sa;
    xb_vec2Nx24 accH, accL;
    for (int k = 0; k < 7; ++k) {
        xb_vec2Nx8U vA = IVP_LV2NX8U_X(
            (const xb_vec2Nx8U *)(const void *)(tr[k]), 0);
        xb_vec2Nx8U vB = IVP_LV2NX8U_X(
            (const xb_vec2Nx8U *)(const void *)(tr[k] + 64), 0);
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
    xb_vec2Nx8 L1s = IVP_PACKVRNR2NX24(accL, 8);
    xb_vec2Nx8U L1 = *(const xb_vec2Nx8U *)(const void *)&L1s;
    IVP_MULUSPA2N8XR16(accH, L1, L1, 1);
    xb_vec2Nx8U out8 = IVP_PACKVRU2NX24(accH, 8);
    IVP_SA2NX8U_IP(out8, sa, mp);
    IVP_SAPOS2NX8U_FP(sa, mp);
}

/* hpass composite over a 72x70-shaped region: cycles per ROW x100 */
static unsigned long long cal_hpass_rows(const uint8_t *src, int src_pitch,
                                         uint16_t *tmp, int tmp_rows)
{
    const int gk[7] = { 7, 7, 7, 7, 7, 7, 7 };   /* value-free timing probe */
    unsigned long long t0 = cal_tick();
    unsigned long long rows = 0;
    for (int o = 0; o < CAL_OUTER; ++o)
        for (int r = 0; r < tmp_rows; ++r) {
            cal_hpass_row(src + (size_t)r * src_pitch, tmp, gk);
            ++rows;
        }
    return cal_norm(cal_tick() - t0, rows);
}

/* primes-only variant: cycles per ROW x100 */
static unsigned long long cal_hpass_primes_n(const uint8_t *src, int src_pitch,
                                             int rows_n)
{
    unsigned long long t0 = cal_tick();
    unsigned long long rows = 0;
    for (int o = 0; o < CAL_OUTER; ++o)
        for (int r = 0; r < rows_n; ++r) {
            cal_hpass_primes(src + (size_t)r * src_pitch);
            ++rows;
        }
    return cal_norm(cal_tick() - t0, rows);
}

/* vpass composite: cycles per CHUNK x100 (chunks = tmp rows here) */
static unsigned long long cal_vpass_chunks(const uint8_t *tmp,
                                           int tmp_pitch_bytes, int rows_n,
                                           uint8_t *dst)
{
    const int gk[7] = { 7, 7, 7, 7, 7, 7, 7 };
    unsigned long long t0 = cal_tick();
    unsigned long long chunks = 0;
    for (int o = 0; o < CAL_OUTER; ++o)
        for (int r = 0; r + 6 < rows_n; ++r) {
            cal_vpass_chunk(tmp + (size_t)r * (size_t)tmp_pitch_bytes,
                            tmp_pitch_bytes, dst, gk);
            ++chunks;
        }
    return cal_norm(cal_tick() - t0, chunks);
}

/* ================= v1007 CAL3: production-shaped kernel + scan =============
 * v1006 showed the standalone row/chunk kernels are cheap (hp.row 20.77,
 * vp.chunk 83.4) while production costs ~2.4M cyc/tile. CAL3 times the REAL
 * orb_blur_kernel_ivp on a synthetic interior tile, with and without the
 * tile driver's ping-pong DMA overlap, plus the REAL collect scan loop. */

/* real kernel on interior tiles (72x72 in -> 64x64 out, one 64-lane chunk
 * per row, no border columns). with_dma reproduces the driver's overlap:
 * wait in[cur] + out[cur] reuse guard, submit DMA-in for tile t+1, run
 * kernel on tile t, submit DMA-out for tile t — both transfers in flight
 * during compute, exactly like orb_blur_vp6_sub. Cycles per TILE x100. */
static unsigned long long cal_ktiles(xvTileManager *tm,
                                     uint8_t *in0, uint8_t *in1,
                                     uint8_t *out0, uint8_t *out1,
                                     uint8_t *tmp,
                                     uint8_t *heap_in, uint8_t *heap_out,
                                     int with_dma)
{
    enum { TILES = 8 };
    uint8_t *in[2]  = { in0, in1 };
    uint8_t *out[2] = { out0, out1 };
    int pend_in[2]  = { -1, -1 };
    int pend_out[2] = { -1, -1 };
    unsigned long long t0 = cal_tick();
    for (int o = 0; o < CAL_OUTER; ++o) {
        for (int t = 0; t < TILES; ++t) {
            const int cur = t & 1, nxt = cur ^ 1;
            if (with_dma) {
                if (pend_in[cur]  >= 0) { xvWaitForiDMA(tm, pend_in[cur]);   pend_in[cur] = -1; }
                if (pend_out[cur] >= 0) { xvWaitForiDMA(tm, pend_out[cur]); pend_out[cur] = -1; }
                if (t + 1 < TILES)
                    pend_in[nxt] = xvAddIdmaRequest(
                        tm, in[nxt], heap_in, 64, 64, 64, 64, 0);
            }
            orb_blur_kernel_ivp(in[cur], 72, 0, 0, 72, 1000, 1000, 3, 3,
                                64, 64, out[cur], 64,
                                (uint16_t *)(void *)tmp, 64);
            if (with_dma)
                pend_out[cur] = xvAddIdmaRequest(
                    tm, heap_out, out[cur], 64, 64, 64, 64, 0);
        }
    }
    if (pend_in[0]  >= 0) xvWaitForiDMA(tm, pend_in[0]);
    if (pend_in[1]  >= 0) xvWaitForiDMA(tm, pend_in[1]);
    if (pend_out[0] >= 0) xvWaitForiDMA(tm, pend_out[0]);
    if (pend_out[1] >= 0) xvWaitForiDMA(tm, pend_out[1]);
    return cal_norm(cal_tick() - t0, (unsigned long long)CAL_OUTER * TILES);
}

/* the REAL fast9_collect_cell scan loop on a synthetic 64x64 bank band with
 * a scoremap-like mix (mostly 0, sub-threshold scores, ~1% NONE, ~0.5%
 * candidates at t=20). Cycles per scanned BYTE x100. */
static unsigned long long cal_scan_bytes(uint8_t *band)
{
    fast9_keypoint *out =
        (fast9_keypoint *)malloc(sizeof(fast9_keypoint) * 512);
    if (!out) return 0;
    for (int i = 0; i < 64 * 64; ++i) {
        int v = 0;
        if ((i % 97) == 0)        v = 255;                     /* NONE     */
        else if ((i % 211) == 0)  v = 25 + (i / 211) % 100;    /* >= t=20  */
        else if ((i % 13) == 0)   v = 1 + (i / 13) % 19;       /* < t=20   */
        band[i] = (uint8_t)v;
    }
    unsigned long long t0 = cal_tick();
    unsigned long long cells = 0;
    for (int o = 0; o < CAL_OUTER * 8; ++o) {
        int n = 0;
        fast9_collect_cell(band, 72, 0, 64, 0, 64, 20,
                           out, &n, 512, NULL, 0);
        ++cells;
    }
    free(out);
    return cal_norm(cal_tick() - t0, cells * 64 * 64);
}

/* ================= v1015 CAL4: collect scan decomposition ================
 * CAL3 priced the real scan loop at 39.5 cyc/byte on a synthetic mix. CAL4
 * splits that into base loop vs candidate tail, and prices the vector skip
 * that would replace the base loop in a vectorized collect. */

/* the REAL fast9_collect_cell on an all-NONE band: every byte takes the
 * `s == FAST9_MAP_NONE` continue — the base-loop price with zero candidate,
 * NMS and emit work. Cycles per BYTE x100. */
static unsigned long long cal_scan_zero(uint8_t *band)
{
    fast9_keypoint *out =
        (fast9_keypoint *)malloc(sizeof(fast9_keypoint) * 512);
    if (!out) return 0;
    memset(band, 0xFF, 72 * 64);               /* pitch-72 rows 0..63 */
    unsigned long long t0 = cal_tick();
    unsigned long long cells = 0;
    for (int o = 0; o < CAL_OUTER * 8; ++o) {
        int n = 0;
        fast9_collect_cell(band, 72, 0, 64, 0, 64, 20,
                           out, &n, 512, NULL, 0);
        ++cells;
    }
    free(out);
    return cal_norm(cal_tick() - t0, cells * 64 * 64);
}

/* vector threshold-skip prototype on the same synthetic mix (fill rule
 * identical to cal_scan_bytes, so CAL3 SCANBYTE / CAL4 SCANVEC are directly
 * comparable per byte). Per 64B chunk: one streaming load, skip-lane masks
 * (v < t or v == NONE — exactly the scalar continue predicate), two selects
 * + MIN as a lane AND, 64B scratch store + 16-word scalar OR for the
 * all-skip test; hit chunks fall to the production byte predicate.
 * Cycles per BYTE x100. */
static unsigned long long cal_scan_vec(uint8_t *buf)
{
    enum { ROWS = 8, CHUNKS = 8 };             /* 8 rows x 512B = CAL_BYTES */
    for (int i = 0; i < ROWS * CHUNKS * 64; ++i) {
        int v = 0;
        if ((i % 97) == 0)        v = 255;                     /* NONE     */
        else if ((i % 211) == 0)  v = 25 + (i / 211) % 100;    /* >= t=20  */
        else if ((i % 13) == 0)   v = 1 + (i / 13) % 19;       /* < t=20   */
        buf[i] = (uint8_t)v;
    }

    /* lane constants (stack scratch, the scoremap kernel's tfb/ntb pattern):
     * t broadcast, 254 (v == NONE <=> 254 < v), 1 */
    __attribute__((aligned(64)))
    uint8_t t_tb[64], h_tb[64], o_tb[64], scr[64];
    memset(t_tb, 20, sizeof t_tb);
    memset(h_tb, 254, sizeof h_tb);
    memset(o_tb, 1, sizeof o_tb);
    xb_vec2Nx8U vt = IVP_LV2NX8U_X((const xb_vec2Nx8U *)(const void *)t_tb, 0);
    xb_vec2Nx8U vh = IVP_LV2NX8U_X((const xb_vec2Nx8U *)(const void *)h_tb, 0);
    xb_vec2Nx8U vo = IVP_LV2NX8U_X((const xb_vec2Nx8U *)(const void *)o_tb, 0);
    xb_vec2Nx8U vz = IVP_SUB2NX8U(vt, vt);      /* all-zero lanes */

    unsigned long long t0 = cal_tick();
    unsigned long long chunks = 0;
    for (int o = 0; o < CAL_OUTER * 8; ++o) {
        for (int r = 0; r < ROWS; ++r) {
            const uint8_t *row = buf + (size_t)r * (CHUNKS * 64);
            const xb_vec2Nx8U *lp = (const xb_vec2Nx8U *)(const void *)row;
            valign lv = IVP_LA2NX8U_PP(lp);     /* one prime per row */
            for (int c = 0; c < CHUNKS; ++c, ++chunks) {
                xb_vec2Nx8U v;
                IVP_LAV2NX8U_XP(v, lv, lp, 64);
                /* skip lane <=> v < t || v == NONE: AND of two 0/1 selects */
                xb_vec2Nx8U s1 = IVP_MOV2NX8UT(vz, vo, IVP_LTU2NX8U(v, vt));
                xb_vec2Nx8U s2 = IVP_MOV2NX8UT(vz, vo, IVP_LTU2NX8U(vh, v));
                xb_vec2Nx8U sk = IVP_MINU2NX8U(s1, s2);
                IVP_SV2NX8U_X(sk, (xb_vec2Nx8U *)(void *)scr, 0);
                const unsigned int *w =
                    (const unsigned int *)(const void *)scr;
                if ((w[0]|w[1]|w[2]|w[3]|w[4]|w[5]|w[6]|w[7]
                   |w[8]|w[9]|w[10]|w[11]|w[12]|w[13]|w[14]|w[15])
                    != 0x01010101u) {
                    /* hit chunk: byte predicate exactly as production */
                    for (int x = 0; x < 64; ++x) {
                        int s = row[c * 64 + x];
                        if (s == FAST9_MAP_NONE || s < 20) continue;
                        cal_sink ^= (unsigned int)s;
                    }
                }
            }
        }
    }
    return cal_norm(cal_tick() - t0, chunks * 64);
}


void dsp_calib_vp6_run(xvTileManager *tm)
{
    static int cal_done = 0;
    if (cal_done) return;           /* measure once, export every frame */
    cal_done = 1;

    memset(dsp_calib_vp6_res, 0, sizeof(dsp_calib_vp6_res));
    memset(dsp_calib2_vp6_res, 0, sizeof(dsp_calib2_vp6_res));
    memset(dsp_calib3_vp6_res, 0, sizeof(dsp_calib3_vp6_res));
    memset(dsp_calib4_vp6_res, 0, sizeof(dsp_calib4_vp6_res));

    uint8_t *heap = (uint8_t *)malloc(CAL_BYTES + CAL_SLACK);
    if (!heap) return;
    memset(heap, 0x5a, CAL_BYTES);
    uint8_t *bank = (uint8_t *)xvAllocateBuffer(
        tm, CAL_BYTES + CAL_SLACK, XV_MEM_BANK_COLOR_1, 64);
    if (!bank || bank == (void *)(intptr_t)XVTM_ERROR) { free(heap); return; }
    memset(bank, 0xa5, CAL_BYTES);

    dsp_calib_vp6_res[DSP_CALIB_LD_HEAP]  = cal_scalar_ld(heap);
    dsp_calib_vp6_res[DSP_CALIB_LD_BANK]  = cal_scalar_ld(bank);
    dsp_calib_vp6_res[DSP_CALIB_LD_BANK1] = cal_scalar_ld(bank + 1);
    dsp_calib_vp6_res[DSP_CALIB_LV_BANK]  = cal_vec_lv(bank);
    dsp_calib_vp6_res[DSP_CALIB_LV_HEAP]  = cal_vec_lv(heap);
    dsp_calib_vp6_res[DSP_CALIB_LAV1]     = cal_lav(bank, 1);
    dsp_calib_vp6_res[DSP_CALIB_LAV2]     = cal_lav(bank, 2);
    dsp_calib_vp6_res[DSP_CALIB_LAV7]     = cal_lav(bank, 7);
    dsp_calib_vp6_res[DSP_CALIB_GATH7]    = cal_gather7(bank);
    dsp_calib_vp6_res[DSP_CALIB_MAC7]     = cal_mac7(bank);
    {
        /* a resident u16 vector for the store test (loaded, not built) */
        xb_vecNx16 v16 = IVP_CVT16U2NX24L(IVP_MULUS2N8XR16(
            IVP_LV2NX8U_X((const xb_vec2Nx8U *)(const void *)bank, 0), 7));
        dsp_calib_vp6_res[DSP_CALIB_SV16] = cal_sv16(bank, &v16);
    }
    dsp_calib_vp6_res[DSP_CALIB_FADD]     = cal_fadd();
    dsp_calib_vp6_res[DSP_CALIB_MEMSET]   = cal_memset_h(heap);

    /* iDMA 2D copy 64x64 heap->bank: cycles per byte moved */
    {
        unsigned long long t0 = cal_tick();
        enum { N_DMA = 16 };
        for (int i = 0; i < N_DMA; ++i) {
            int dma = xvAddIdmaRequest(tm, bank, heap, 64, 64, 64, 64, 0);
            if (dma >= 0) xvWaitForiDMA(tm, dma);
        }
        dsp_calib_vp6_res[DSP_CALIB_IDMA] =
            cal_norm(cal_tick() - t0, (unsigned long long)N_DMA * 64 * 64);
    }

    /* ---- CAL2 (v1006): blur composites + CAL1 blind spots ----
     * ping/tmp0 = COLOR_0 and pong/tmp1 = COLOR_1 mirror the tile driver's
     * exact buffer colors (fs_alloc ping COLOR_0 / pong COLOR_1, blur tmp
     * COLOR_0); region shape 72x72 src, 70 rows, u16 tmp pitch 128B. */
    {
        uint8_t *ping = (uint8_t *)xvAllocateBuffer(tm, 72 * 72,
                                                    XV_MEM_BANK_COLOR_0, 64);
        uint8_t *pong = (uint8_t *)xvAllocateBuffer(tm, 72 * 72,
                                                    XV_MEM_BANK_COLOR_1, 64);
        uint8_t *tmp0 = (uint8_t *)xvAllocateBuffer(tm, 72 * 128,
                                                    XV_MEM_BANK_COLOR_0, 64);
        uint8_t *tmp1 = (uint8_t *)xvAllocateBuffer(tm, 72 * 128,
                                                    XV_MEM_BANK_COLOR_1, 64);
        uint8_t *dst  = (uint8_t *)xvAllocateBuffer(tm, 128,
                                                    XV_MEM_BANK_COLOR_1, 64);
        int ok = ping && ping != (void *)(intptr_t)XVTM_ERROR
              && pong && pong != (void *)(intptr_t)XVTM_ERROR
              && tmp0 && tmp0 != (void *)(intptr_t)XVTM_ERROR
              && tmp1 && tmp1 != (void *)(intptr_t)XVTM_ERROR
              && dst  && dst  != (void *)(intptr_t)XVTM_ERROR;
        if (ok) {
            memset(ping, 0x5a, 72 * 72);
            memset(pong, 0x5a, 72 * 72);
            memset(tmp0, 0x33, 72 * 128);
            memset(tmp1, 0x33, 72 * 128);

            dsp_calib2_vp6_res[DSP_CALIB2_LD_STACK]  = cal_stack_ld();
            dsp_calib2_vp6_res[DSP_CALIB2_LD_C0]     = cal_scalar_ld(ping);
            dsp_calib2_vp6_res[DSP_CALIB2_LV_C0]     = cal_vec_lv(ping);
            dsp_calib2_vp6_res[DSP_CALIB2_LAV7_C0]   = cal_lav(ping, 7);
            dsp_calib2_vp6_res[DSP_CALIB2_MALLOC]    = cal_malloc_pair();
            dsp_calib2_vp6_res[DSP_CALIB2_HP_ROW_C1] = cal_hpass_rows(
                pong, 72, (uint16_t *)(void *)tmp1, 70);
            dsp_calib2_vp6_res[DSP_CALIB2_HP_ROW_C0] = cal_hpass_rows(
                ping, 72, (uint16_t *)(void *)tmp0, 70);
            dsp_calib2_vp6_res[DSP_CALIB2_HP_PRIME]  = cal_hpass_primes_n(
                ping, 72, 70);
            dsp_calib2_vp6_res[DSP_CALIB2_VP_CHUNK_C0] = cal_vpass_chunks(
                tmp0, 128, 70, dst);

            /* hpass rows with concurrent heap->pong iDMA traffic (the tile
             * driver's overlap): one 4KB copy submitted every 4 rows, a
             * batch of 16 outstanding max, all drained inside the window */
            {
                const int gk[7] = { 7, 7, 7, 7, 7, 7, 7 };
                int32_t dma[16];
                int ndma = 0;
                unsigned long long t0 = cal_tick();
                unsigned long long rows = 0;
                for (int o = 0; o < CAL_OUTER; ++o)
                    for (int r = 0; r < 72; ++r) {
                        if ((r & 3) == 0) {
                            if (ndma == 16) {
                                for (int i = 0; i < ndma; ++i)
                                    if (dma[i] >= 0) xvWaitForiDMA(tm, dma[i]);
                                ndma = 0;
                            }
                            dma[ndma++] = xvAddIdmaRequest(
                                tm, pong, heap, CAL_BYTES, 1,
                                CAL_BYTES, CAL_BYTES, 0);
                        }
                        cal_hpass_row(ping + (size_t)r * 72,
                                      (uint16_t *)(void *)tmp0, gk);
                        ++rows;
                    }
                for (int i = 0; i < ndma; ++i)
                    if (dma[i] >= 0) xvWaitForiDMA(tm, dma[i]);
                dsp_calib2_vp6_res[DSP_CALIB2_HP_ROW_DMA] =
                    cal_norm(cal_tick() - t0, rows);
            }
        }
        if (ping && ping != (void *)(intptr_t)XVTM_ERROR) xvFreeBuffer(tm, ping);
        if (pong && pong != (void *)(intptr_t)XVTM_ERROR) xvFreeBuffer(tm, pong);
        if (tmp0 && tmp0 != (void *)(intptr_t)XVTM_ERROR) xvFreeBuffer(tm, tmp0);
        if (tmp1 && tmp1 != (void *)(intptr_t)XVTM_ERROR) xvFreeBuffer(tm, tmp1);
        if (dst  && dst  != (void *)(intptr_t)XVTM_ERROR) xvFreeBuffer(tm, dst);
    }

    /* ---- CAL3 (v1007): the REAL production kernel + the REAL scan loop.
     * Buffer colors mirror the driver: in ping COLOR_0 / pong COLOR_1, out
     * ping COLOR_0 / pong COLOR_1, tmp COLOR_0. heap2 is the DMA-out target
     * (separate from the DMA-in source region so the two transfers in
     * flight touch disjoint memory, like the driver's frame regions). */
    {
        uint8_t *ping = (uint8_t *)xvAllocateBuffer(tm, 72 * 72,
                                                    XV_MEM_BANK_COLOR_0, 64);
        uint8_t *pong = (uint8_t *)xvAllocateBuffer(tm, 72 * 72,
                                                    XV_MEM_BANK_COLOR_1, 64);
        uint8_t *out0 = (uint8_t *)xvAllocateBuffer(tm, 64 * 64,
                                                    XV_MEM_BANK_COLOR_0, 64);
        uint8_t *out1 = (uint8_t *)xvAllocateBuffer(tm, 64 * 64,
                                                    XV_MEM_BANK_COLOR_1, 64);
        uint8_t *tmp0 = (uint8_t *)xvAllocateBuffer(tm, 72 * 128,
                                                    XV_MEM_BANK_COLOR_0, 64);
        uint8_t *heap2 = (uint8_t *)malloc(CAL_BYTES + CAL_SLACK);
        uint8_t *vbuf = (uint8_t *)xvAllocateBuffer(tm, CAL_BYTES + CAL_SLACK,
                                                    XV_MEM_BANK_COLOR_1, 64);
        int ok = ping && ping != (void *)(intptr_t)XVTM_ERROR
              && pong && pong != (void *)(intptr_t)XVTM_ERROR
              && out0 && out0 != (void *)(intptr_t)XVTM_ERROR
              && out1 && out1 != (void *)(intptr_t)XVTM_ERROR
              && tmp0 && tmp0 != (void *)(intptr_t)XVTM_ERROR
              && heap2
              && vbuf && vbuf != (void *)(intptr_t)XVTM_ERROR;
        if (ok) {
            memset(ping, 0x5a, 72 * 72);
            memset(pong, 0x5a, 72 * 72);
            memset(heap2, 0, CAL_BYTES);

            dsp_calib3_vp6_res[DSP_CALIB3_KTILE] = cal_ktiles(
                tm, ping, pong, out0, out1, tmp0, heap, heap2, 0);
            dsp_calib3_vp6_res[DSP_CALIB3_KTILE_DMA] = cal_ktiles(
                tm, ping, pong, out0, out1, tmp0, heap, heap2, 1);
            /* CAL4 (v1015): base loop / vector skip. cal_scan_zero refills
             * pong all-NONE, so it must run before cal_scan_bytes' mix. */
            dsp_calib4_vp6_res[DSP_CALIB4_SCAN_ZERO] = cal_scan_zero(pong);
            dsp_calib3_vp6_res[DSP_CALIB3_SCANBYTE] = cal_scan_bytes(pong);
            dsp_calib4_vp6_res[DSP_CALIB4_SCAN_VEC] = cal_scan_vec(vbuf);
        }
        if (ping  && ping  != (void *)(intptr_t)XVTM_ERROR) xvFreeBuffer(tm, ping);
        if (pong  && pong  != (void *)(intptr_t)XVTM_ERROR) xvFreeBuffer(tm, pong);
        if (out0  && out0  != (void *)(intptr_t)XVTM_ERROR) xvFreeBuffer(tm, out0);
        if (out1  && out1  != (void *)(intptr_t)XVTM_ERROR) xvFreeBuffer(tm, out1);
        if (tmp0  && tmp0  != (void *)(intptr_t)XVTM_ERROR) xvFreeBuffer(tm, tmp0);
        if (vbuf  && vbuf  != (void *)(intptr_t)XVTM_ERROR) xvFreeBuffer(tm, vbuf);
        free(heap2);
    }

    xvFreeBuffer(tm, bank);
    free(heap);
}
#endif /* __XTENSA__ */
