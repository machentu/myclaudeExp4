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
#else
#define ORB_TMARK(v)     do { } while (0)
#define ORB_TACC(v, slot) do { } while (0)
#endif

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
    orb_tick_t orb_t, orb_t0;   /* stage mark / whole-call mark */
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
        free(scoremap); scoremap = NULL;
        ORB_TACC(orb_t, DSP_ORB_VP6_T_COLLECT);

        /* convert absolute pyramid coords → minBorder-relative for quadtree */
        ORB_TMARK(orb_t);
        for (int k = 0; k < nraw; ++k) {
            raw[k].x -= (float)minBorderX;
            raw[k].y -= (float)minBorderY;
        }

        /* quadtree spatial distribution */
        kept = (ngd_keypoint*)malloc(sizeof(ngd_keypoint) * (size_t)(ex->nfeatures*2 + 64));
        nkept = dsp_distribute_octtree(raw, nraw,
                                       minBorderX, maxBorderX, minBorderY, maxBorderY,
                                       ex->mnFeaturesPerLevel[level], kept);
        free(raw); raw = NULL;

        /* restore absolute coords, set octave + size */
        scaledPatch = (int)(NGD_ORB_PATCH_SIZE * ex->mvScaleFactor[level]);
        for (int i = 0; i < nkept; ++i) {
            kept[i].x += (float)minBorderX;
            kept[i].y += (float)minBorderY;
            kept[i].octave = level;
            kept[i].size = (float)scaledPatch;
        }
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
