/*
 * dsp_orb_octtree.h - quadtree spatial distribution shared by the ORB
 *                     orchestrators (dsp_orb.c pure-C path, dsp_orb_vp6.c
 *                     VP6+IVP path).
 *
 * Algorithm of refactor/src/orb.c, in a DSP-friendly inner form with
 * byte-identical output (see src/dsp/dsp_orb_octtree.c); both orchestrators
 * must distribute identical keypoint sets identically, so they call the
 * same code.
 */
#ifndef DSP_ORB_OCTTREE_H
#define DSP_ORB_OCTTREE_H

#include "ngd/orb.h"           /* ngd_keypoint */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Distribute `nkeys` keypoints over the [minX,maxX)x[minY,maxY) window into at
 * most N nodes (refactor ORBextractor quadtree), keeping the max-response
 * keypoint per node. Writes one keypoint per surviving node to `out`; the
 * >= N stop can overshoot N by up to 3 (one division's net node gain), same
 * as refactor/src/orb.c and ORB_SLAM3 — callers must size `out` with
 * headroom (the orchestrators use nfeatures*2 + 64). Returns the count.
 */
int dsp_distribute_octtree(const ngd_keypoint *keys, int nkeys,
                           int minX, int maxX, int minY, int maxY,
                           int N, ngd_keypoint *out);

/*
 * v1010 staging hook: lend the quadtree a bank-SRAM scratch block plus a
 * synchronous block mover (dst/src distinct, bytes > 0). When installed,
 * each node division rides the mover (an iDMA wait on the firmware) into
 * the scratch (scalar bank reads are ~1 cyc vs 83.9 for the DDR heap on
 * this core), counts there, and scatters straight into the final arena
 * kid slots; spans larger than the scratch use the DDR scatter with its
 * temp copy-back. Without a scratch that same DDR scatter runs for every
 * division (same output - both forms lay out the identical stable 4-way
 * partition). Pass buf=NULL/move=NULL to uninstall.
 */
typedef void (*oct_move_fn)(void *ctx, void *dst, const void *src, int bytes);
void dsp_octtree_set_scratch(void *buf, int bytes,
                             oct_move_fn move, void *move_ctx);

/* v1011 staging telemetry (CYB export): [0] max / [1] min scratch bytes
 * installed across the frame's distribute calls (0 = never installed),
 * [2] key-visits through the staged divide, [3] through the fallback. */
extern unsigned long long dsp_octtree_qcnt[4];

/* v1006 sub-phase cycle counters (board attribution of qt.tree): init /
 * rounds-loop total (includes qsort time) / qsort alone / emit + free.
 * Accumulating like dsp_orb_vp6_cycles — zero between measurements. */
enum {
    DSP_OCT_INIT = 0,
    DSP_OCT_ROUNDS,
    DSP_OCT_QSORT,
    DSP_OCT_EMIT,
    DSP_OCT_COUNT
};
extern unsigned long long dsp_octtree_cs[DSP_OCT_COUNT];

#ifdef __cplusplus
}
#endif
#endif /* DSP_ORB_OCTTREE_H */
