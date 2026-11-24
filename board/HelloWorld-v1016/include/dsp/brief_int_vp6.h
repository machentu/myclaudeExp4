/*
 * brief_int_vp6.h - VP6 strip rotated-BRIEF, FIXED-POINT LUT variant.
 *
 * Same strip-sweep driver shape as brief_vp6.h, but the per-keypoint
 * rotation runs as integer vector maths (the xsTileLib ORBAngleBRIEF.c
 * form): the orientation angle is binned to whole degrees, sin/cos come
 * from a 361-entry Q7 (7 fractional bits) byte LUT, and the pattern
 * rotation is IVP_MULP2NX8 + IVP_PACKVR2NX24 in i8 lanes. This is NOT
 * bit-exact with the float golden path - descriptors differ by a few
 * bits per keypoint (measured by test_brief_int_vp6.c) in exchange for
 * removing ALL soft-float work from the kernel. The strip gather stage
 * and the descriptor bit layout are byte-compatible with brief_vp6, so
 * the two variants are drop-in interchangeable at the driver level.
 *
 * Select this kernel in dsp_orb_vp6.c by defining BRIEF_INT_VP6 (the
 * default build keeps the bit-exact brief_vp6 path).
 */
#ifndef BRIEF_INT_VP6_H
#define BRIEF_INT_VP6_H

#include "brief_core.h"
#include "fast9_core.h"     /* fast9_keypoint (keypoint input) */
#include "tileManager.h"    /* P6 TileManager (project-provided) */

#ifdef __cplusplus
extern "C" {
#endif

/* Same strip config contract as BriefVp6Config (independent copy so the
 * variant can be tuned without touching the bit-exact driver). */
typedef struct {
    int strip_rows;  /* frame rows assigned per sweep pass (>= 1)         */
    int max_src_w;   /* strip pitch basis: >= largest frame width         */
    int max_src_h;   /* strip buffer rows = strip_rows + 2*BRIEF_MARGIN   */
} BriefIntVp6Config;

#define BRIEF_INT_VP6_DEFAULT_CONFIG { 24, 640, 62 }

/*
 * Fixed-point rotated-BRIEF for all keypoints (whole-frame strip sweep).
 * Arguments identical to brief_vp6_process(). desc[i*32] approximates the
 * float golden descriptor: rotation quantised to 1-degree bins and Q7
 * sin/cos (test_brief_int_vp6.c reports the Hamming distance profile).
 */
int brief_int_vp6_process(xvTileManager *tm, xvFrame *src_frame,
                          int src_w, int src_h,
                          const fast9_keypoint *kps, uint8_t *desc, int n_kps,
                          const BriefIntVp6Config *cfg);

/* Release the persistent strip buffers. */
void brief_int_vp6_release(void);

/* ---- per-stage cycle profiling (same shape as brief_vp6_cycles) ---- */
enum {
    BRIEF_INT_T_KERNEL = 0, /* whole kernel call                          */
    BRIEF_INT_T_ROT,        /* angle bin + LUT splats + 8 rotation chunks  */
                            /* (MULP/PACKVR/UNPK/clamp/linearise, fused)   */
    BRIEF_INT_T_GATHER,     /* 4x64 gather + LTU/MOVUT + bit pack          */
    BRIEF_INT_T_COUNT
};
extern unsigned long long brief_int_vp6_cycles[BRIEF_INT_T_COUNT];
extern unsigned long long brief_int_vp6_kps;

void brief_int_vp6_timing_report(void);

/* Exposed for the test harness: the scalar integer reference (natural pair
 * order, same LUT + rounding as the vector kernel) and the LUT builder it
 * shares with the kernel init (single source of truth). */
void brief_int_build_lut(int8_t *dst);
void brief_int_scalar_kernel(const uint8_t *src_data, int src_pitch,
                             int origin_x, int origin_y,
                             int frame_w, int frame_h,
                             int cx, int cy, float angle, uint8_t *desc,
                             const int8_t *lut);

#ifdef __cplusplus
}
#endif
#endif /* BRIEF_INT_VP6_H */
