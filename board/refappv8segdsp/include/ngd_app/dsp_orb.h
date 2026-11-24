#ifndef NGD_APP_DSP_ORB_H
#define NGD_APP_DSP_ORB_H

/*
 * ngd_app/dsp_orb.h — ORB extractor using VP6/DSP bit-exact kernels.
 *
 * The pipeline uses refoptvp6v2 _full() functions for pixel operations
 * (bilinear resize, Gaussian blur, FAST-9_16 detect+NMS, IC_Angle,
 * rBRIEF descriptor) and refactor's quadtree for spatial distribution.
 *
 * Same call signature as ngd_orb_extract() so it can be swapped in with
 * a compile-time macro (USE_DSP_ORB).
 *
 * All kernel arithmetic is validated bit-exact with the refactor C
 * reference and OpenCV 4.x — see refoptvp6v2/test/test_*_vp6.c.
 */

#include "ngd/orb.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Extract ORB features using VP6 DSP full-reference kernels.
 * Parameters are read from `ex` (nfeatures, scaleFactor, nlevels,
 * iniThFAST, minThFAST).
 * Multi-scale: pyramid built via orb_resize_full, FAST9 per level,
 * quadtree distribution, IC_Angle + rBRIEF per keypoint.
 * Returns the number of keypoints written, or -1 on error. */
int dsp_orb_extract(const ngd_orb_extractor *ex,
                    const uint8_t *gray, int w, int h, int stride,
                    ngd_keypoint *out_kps, int max_kps,
                    uint8_t *out_desc);

#ifdef __cplusplus
}
#endif
#endif /* NGD_APP_DSP_ORB_H */
