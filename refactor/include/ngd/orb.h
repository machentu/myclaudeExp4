#ifndef NGD_ORB_H
#define NGD_ORB_H

/*
 * ngd/orb.h — ORB feature extractor (pure C, no OpenCV).
 *
 * Faithful reimplementation of ORB_SLAM3::ORBextractor
 * (src/ORBextractor.cc). Pipeline:
 *   ComputePyramid  (bilinear downscale per level, cascaded)
 *   ComputeKeyPointsOctTree  (35px grid → FAST-9_16 + NMS → quadtree distribute)
 *   computeOrientation (IC_Angle gray-centroid, umax table)
 *   computeOrbDescriptor  (rBRIEF on Gaussian-blurred level, bit_pattern_31_ exact)
 *
 * Input is a plain uint8_t* grayscale buffer (row-major, row stride = `stride`).
 * Output keypoints are in level-0 (original image) pixel coordinates, matching
 * ORBextractor::operator() which rescales level>0 keypoints by mvScaleFactor[L].
 *
 * Faithfulness notes (see refactor/README.md):
 *   - bit_pattern_31_ (256 pairs) and umax (16 entries) copied verbatim →
 *     descriptors are vocabulary-compatible given the same corner+orientation.
 *   - FAST-9 detection + NMS + score, bilinear resize, and Gaussian blur are
 *     reimplemented (not bit-identical to OpenCV) — corner SETS may differ
 *     slightly at edges, but each descriptor is a valid ORB descriptor.
 *   - Keypoints are inset ≥16 px from the level ROI edge; max sampling radius
 *     is 15, so the original's 19 px reflect-101 border is numerically
 *     unreachable. We store levels without that border and clamp sampler access.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NGD_ORB_MAX_LEVELS 16
#define NGD_ORB_PATCH_SIZE 31
#define NGD_ORB_HALF_PATCH 15
#define NGD_ORB_EDGE_THRESHOLD 19

typedef struct {
    float x, y;        /* pixel coords (level-0 frame on output) */
    float angle;       /* orientation, degrees [0,360), like cv::KeyPoint */
    float response;    /* FAST corner score, for quadtree max-response */
    int   octave;      /* pyramid level */
    float size;        /* PATCH_SIZE * mvScaleFactor[level] */
} ngd_keypoint;

typedef struct {
    int   nfeatures;
    float scaleFactor;
    int   nlevels;
    int   iniThFAST;
    int   minThFAST;

    float mvScaleFactor[NGD_ORB_MAX_LEVELS];
    float mvInvScaleFactor[NGD_ORB_MAX_LEVELS];
    float mvLevelSigma2[NGD_ORB_MAX_LEVELS];
    float mvInvLevelSigma2[NGD_ORB_MAX_LEVELS];
    int   mnFeaturesPerLevel[NGD_ORB_MAX_LEVELS];
    int   umax[NGD_ORB_HALF_PATCH + 1];
} ngd_orb_extractor;

/* Initialise the extractor (mirrors ORBextractor ctor). Returns 0 on success. */
int ngd_orb_init(ngd_orb_extractor *ex,
                 int nfeatures, float scaleFactor, int nlevels,
                 int iniThFAST, int minThFAST);

/*
 * Extract ORB features from a grayscale image.
 *   gray/stride : source image (row-major, stride in bytes, >= w)
 *   out_kps     : caller buffer, capacity max_kps
 *   out_desc    : caller buffer, capacity max_kps*32 bytes (row i = keypoint i)
 * Returns the number of keypoints written, or -1 on error.
 */
int ngd_orb_extract(const ngd_orb_extractor *ex,
                    const uint8_t *gray, int w, int h, int stride,
                    ngd_keypoint *out_kps, int max_kps,
                    uint8_t *out_desc);

#ifdef __cplusplus
}
#endif
#endif /* NGD_ORB_H */
