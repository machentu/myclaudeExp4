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
 * keypoint per node. Writes <= N keypoints to `out`, returns the count.
 */
int dsp_distribute_octtree(const ngd_keypoint *keys, int nkeys,
                           int minX, int maxX, int minY, int maxY,
                           int N, ngd_keypoint *out);

#ifdef __cplusplus
}
#endif
#endif /* DSP_ORB_OCTTREE_H */
