/*
 * brief_vp6.h - VP6 whole-frame strip rotated-BRIEF (Cadence P6 TileManager /
 *               iDMA).
 *
 * The frame streams through two resident SRAM strip buffers (ping in bank0,
 * pong in bank1, allocated once and reused across frames/levels - the
 * ExtractImage.cpp / TileMemMgr.cpp pattern). Each sweep pass covers
 * `strip_rows` frame rows plus a +/-19 halo; every keypoint whose clamped
 * centre row falls in the pass gets its 32-byte descriptor from the shared
 * brief_kernel() (validated on PC, bit-exact with refactor's
 * ngd_compute_descriptor) gathering straight from the SRAM strip - no
 * per-keypoint DMA. desc[i*32] is written by index, preserving keypoint order.
 *
 * The kernel is the IVP intrinsic form (IVP gather pixel access + lane
 * compare + scalar bit pack); the rotation stays scalar float (per-keypoint
 * angle). Define BRIEF_NO_IVP when building brief_vp6.c to use the portable
 * kernel.
 *
 * Buffers persist until brief_vp6_release() (or process is called with a
 * larger config). Default sizing: 640-wide strips x (24 + 38) rows = 39,680
 * bytes per bank - the four VP6 drivers' defaults together fit two 128KB
 * banks (see orb_tile_manager.h).
 */
#ifndef BRIEF_VP6_H
#define BRIEF_VP6_H

#include "brief_core.h"
#include "fast9_core.h"     /* fast9_keypoint (keypoint input) */
#include "tileManager.h"    /* P6 TileManager (project-provided) */

#ifdef __cplusplus
extern "C" {
#endif

/* VP6 strip config: strip buffers sized ALIGN64(max_src_w) x max_src_h. */
typedef struct {
    int strip_rows;  /* frame rows assigned per sweep pass (>= 1)         */
    int max_src_w;   /* strip pitch basis: >= largest frame width         */
    int max_src_h;   /* strip buffer rows = strip_rows + 2*BRIEF_MARGIN   */
} BriefVp6Config;

/* Defaults: 640-wide strips, 24 assigned rows/pass (62-row buffers). */
#define BRIEF_VP6_DEFAULT_CONFIG { 24, 640, 62 }

/*
 * Run rotated-BRIEF for all keypoints on VP6 (whole-frame strip sweep).
 *   tm        : xvTileManager created with xvCreateTileManager().
 *   src_frame : xvFrame wrapping the source (blurred pyramid level) image in DRAM.
 *   src_w/h   : source image dimensions (must match src_frame).
 *   kps       : input keypoints (frame coords); kps[i].x/.y float (rounded with
 *               lroundf), kps[i].angle = orientation in degrees (from IC angle).
 *   desc      : output buffer, descriptor for kps[i] at desc[i*32 .. i*32+31].
 *   n_kps     : number of keypoints.
 *   cfg       : strip config (NULL => BRIEF_VP6_DEFAULT_CONFIG).
 * Returns 0 on success, non-zero on error.
 *
 * desc[i*32] is bit-exact with brief_full (== refactor ngd_compute_descriptor)
 * for every keypoint: the per-keypoint kernel maths is the same brief_kernel()
 * validated by test_brief_vp6.c (3-way: verbatim refactor ref vs brief_full vs
 * brief_vp6).
 */
int brief_vp6_process(xvTileManager *tm, xvFrame *src_frame,
                      int src_w, int src_h,
                      const fast9_keypoint *kps, uint8_t *desc, int n_kps,
                      const BriefVp6Config *cfg);

/* Release the persistent strip buffers (optional; they also grow on demand). */
void brief_vp6_release(void);

/* ---- per-stage cycle profiling inside brief (mirrors dsp_orb_vp6_cycles;
 * only the IVP kernel path is instrumented - with BRIEF_NO_IVP the counters
 * stay zero). KERNEL covers the whole per-keypoint call, the four sub-stage
 * slots partition it; KERNEL minus their sum is loop/clamp/staging residual.
 * rsr.ccount on Xtensa (cycles), QPC on host (us). Clear the counters and
 * call brief_vp6_timing_report() from the same harness that reports the
 * orb_vp6 stages. */
enum {
    BRIEF_VP6_T_KERNEL = 0,  /* whole kernel call (all sub-stages)      */
    BRIEF_VP6_T_COSSIN,      /* ar + the two double cos/sin             */
    BRIEF_VP6_T_TABLES,      /* 33x2 bf_mul_p rotation tables           */
    BRIEF_VP6_T_OFFSETS,     /* 256-pair loop: 4x bf_addr + clamp       */
    BRIEF_VP6_T_GATHER,      /* 4x64 gather + LTU/MOVUT + bit pack      */
    BRIEF_VP6_T_COUNT
};
extern unsigned long long brief_vp6_cycles[BRIEF_VP6_T_COUNT];
extern unsigned long long brief_vp6_kps;   /* kernel invocations (= kps) */

void brief_vp6_timing_report(void);

#ifdef __cplusplus
}
#endif
#endif /* BRIEF_VP6_H */
