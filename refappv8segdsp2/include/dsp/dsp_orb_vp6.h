/*
 * dsp_orb_vp6.h - ORB extraction orchestrated over the five VP6+IVP drivers
 *                 (orb_resize_strip_vp6 + fast9_scoremap_vp6 + orb_blur_vp6 +
 *                 ic_angle_vp6 + brief_vp6), bit-exact with dsp_orb_extract().
 *
 * One call replaces dsp_orb_extract's per-pixel kernels with the persistent
 * bank-pool / raw-iDMA drivers (orb_tile_manager.h). The pyramid resize runs
 * orb_resize_strip_vp6 (bank-1 strip sweep, gather+MAC kernels) with a pure-C
 * orb_resize_full fallback on resource failure — outputs are identical.
 *
 * Bit-exactness with dsp_orb_extract: same quadtree (dsp_orb_octtree.c),
 * same fast9_collect_cell cell grid/retry logic, and each driver kernel is
 * byte-identical to its _full counterpart (validated by the per-driver
 * tests); test_dsp_orb_vp6 memcmp-compares keypoints AND descriptors.
 */
#ifndef DSP_ORB_VP6_H
#define DSP_ORB_VP6_H

#include <stdint.h>

#include "tileManager.h"     /* xvTileManager */
#include "ngd/orb.h"         /* ngd_orb_extractor, ngd_keypoint */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Widest frame the DEFAULT driver configs accept (fast9_scoremap / ic_angle /
 * brief default max_src_w = 640). Only the level-0 width binds — pyramid
 * levels shrink from there. Larger frames: raise those configs AND the
 * ORB_TM_POOL_BYTES bank pools.
 */
#define DSP_ORB_VP6_MAX_W 640

/*
 * Extract ORB features from an 8-bit grayscale image using the VP6+IVP
 * drivers.
 *   tm        : xvTileManager initialized by orb_tile_manager_init().
 *   ex        : extractor config from ngd_orb_init() (which fills ex->umax).
 *   gray      : level-0 image, w x h, row pitch `stride`.
 *   out_kps   : receives up to max_kps keypoints (level-0 coords).
 *   out_desc  : receives 32 bytes per emitted keypoint (may be NULL).
 *
 * `stride` must equal `w` (dsp_orb_extract addresses the level-0 image with
 * pitch w — its own stride argument is unused — so the same contract is
 * enforced here to keep both paths bit-exact for every accepted input).
 *
 * Returns the number of emitted keypoints (>= 0), or negative on error:
 * -1 bad arguments, otherwise the error code of the failing VP6 driver
 * (e.g. -2 strip-buffer/bank allocation failure).
 *
 * Output is byte-identical to dsp_orb_extract() for the same inputs.
 */
int dsp_orb_vp6_extract(xvTileManager *tm,
                        const ngd_orb_extractor *ex,
                        const uint8_t *gray, int w, int h, int stride,
                        ngd_keypoint *out_kps, int max_kps,
                        uint8_t *out_desc);

/* ---- per-stage cycle profiling (DSP target only; see dsp_orb_vp6.c) ----
 *
 * Build dsp_orb_vp6.c with -DDSP_ORB_VP6_TIMING (Linux xt-clang build) and
 * the extractor accumulates Xtensa CCOUNT deltas per pipeline stage into
 * dsp_orb_vp6_cycles[] (whole-call total in slot DSP_ORB_VP6_T_TOTAL), so
 * the simulator's whole-program cycle figure can be decomposed. Counters
 * accumulate across calls — zero them between measurements. Without the
 * macro the counters stay zero, dsp_orb_vp6_timing_report() is a no-op,
 * and the instrumentation costs nothing. */
enum {
    DSP_ORB_VP6_T_TOTAL = 0,  /* whole dsp_orb_vp6_extract() call        */
    DSP_ORB_VP6_T_RESIZE,     /* pyramid build (orb_resize_strip_vp6) */
    DSP_ORB_VP6_T_SCOREMAP,   /* fast9_scoremap_vp6_build, all levels    */
    DSP_ORB_VP6_T_COLLECT,    /* cell collect + low-threshold retries    */
    DSP_ORB_VP6_T_QUADTREE,   /* coord shift + octtree + octave/size     */
    DSP_ORB_VP6_T_ICANGLE,    /* ic_angle_vp6_process + angle backfill   */
    DSP_ORB_VP6_T_BLUR,       /* orb_blur_vp6_process + frame wrap       */
    DSP_ORB_VP6_T_BRIEF,      /* brief_vp6_process                       */
    DSP_ORB_VP6_T_EMIT,       /* rescale + descriptor memcpy to output   */
    DSP_ORB_VP6_T_COUNT
};
extern unsigned long long dsp_orb_vp6_cycles[DSP_ORB_VP6_T_COUNT];
extern unsigned long long dsp_orb_vp6_calls;
/* Print the accumulated per-stage cycle counts (printf; no-op without the
 * macro, so callers may call it unconditionally). */
void dsp_orb_vp6_timing_report(void);

#ifdef __cplusplus
}
#endif
#endif /* DSP_ORB_VP6_H */
