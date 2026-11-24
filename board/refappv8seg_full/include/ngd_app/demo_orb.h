#ifndef NGD_APP_DEMO_ORB_H
#define NGD_APP_DEMO_ORB_H

/*
 * ngd_app/demo_orb.h — alternative ORB extractor adapted from
 * try-slam31-RI/demo_feature_extraction.c (xsTileLib REF-based).
 *
 * Single-scale FAST9 + 3x3 NMS + Top-N by score + ORB angle (LUT) +
 * rBRIEF 256-bit descriptor. Same call signature as ngd_orb_extract()
 * so it can be swapped in with a compile-time macro.
 *
 * Compile with -DUSE_DEMO_ORB to use this instead of ngd_orb_extract.
 */

#include "ngd/orb.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Extract ORB features using the demo (xsTileLib REF) algorithm.
 * Parameters are read from `ex` (nfeatures, iniThFAST).
 * All keypoints are output at octave 0 (single-scale).
 * Returns the number of keypoints written, or -1 on error. */
int demo_orb_extract(const ngd_orb_extractor *ex,
                     const uint8_t *gray, int w, int h, int stride,
                     ngd_keypoint *out_kps, int max_kps,
                     uint8_t *out_desc);

#ifdef __cplusplus
}
#endif
#endif /* NGD_APP_DEMO_ORB_H */
