#ifndef MASKTRACKER_MASKTRACKER_H
#define MASKTRACKER_MASKTRACKER_H

#include "YoloDetector.h"
#include "MaskPredictor.h"
#include <opencv2/core.hpp>

// MaskTracker: orchestrates the per-frame pipeline.
//
//   For each frame:
//     1. YOLO detects the person mask (depth-consistency segmentation).
//     2. If a previous frame exists, MaskPredictor propagates the previous
//        mask to the current frame via LK optical flow.
//     3. The propagated mask is the output; if it is empty, optionally fall
//        back to the current YOLO mask.
//     4. The seed for the NEXT frame is either the YOLO mask (default,
//        re-anchors every frame, avoids drift) or the output mask
//        (self-chained, faster but may drift).
class MaskTracker
{
public:
    enum class SeedPolicy { FromYolo, FromOutput };

    struct Params
    {
        YoloDetector::Params  yolo;
        MaskPredictor::Params pred;
        SeedPolicy policy              = SeedPolicy::FromYolo;
        bool       fallbackToYoloIfEmpty = true;
    };

    explicit MaskTracker(const Params& p);

    // Process one frame. Returns the output dynamic mask (CV_8UC1, 1=dynamic),
    // same size as imRGB. May be empty if YOLO produced nothing and no prior
    // state exists.
    cv::Mat processFrame(const cv::Mat& imRGB, const cv::Mat& imDepthMeters);

    void reset();

private:
    YoloDetector mDetector;
    Params      mParams;
    cv::Mat     mPrevGray;   // previous frame grayscale (LK source)
    cv::Mat     mPrevMask;   // previous frame seed mask
    bool        mHasPrev = false;
    int         mFrameNum = 0;
};

#endif // MASKTRACKER_MASKTRACKER_H
