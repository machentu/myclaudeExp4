#include "MaskTracker.h"

#include <opencv2/imgproc.hpp>
#include <iostream>

MaskTracker::MaskTracker(const Params& p) : mDetector(p.yolo), mParams(p)
{
}

cv::Mat MaskTracker::processFrame(const cv::Mat& imRGB, const cv::Mat& imDepthMeters)
{
    // Grayscale of the current frame (LK needs single-channel).
    cv::Mat gray;
    if (imRGB.channels() == 1)
        gray = imRGB;
    else
        cv::cvtColor(imRGB, gray, cv::COLOR_BGR2GRAY);

    // 1. YOLO detection (blocking, single-threaded).
    cv::Mat maskYolo = mDetector.detect(imRGB, imDepthMeters);

    cv::Mat maskOut;
    if (mHasPrev)
    {
        // 2. Propagate the previous mask to the current frame.
        maskOut = MaskPredictor::predict(mPrevGray, mPrevMask, gray, imDepthMeters, mParams.pred);

        // 3. If prediction came up empty, optionally fall back to the YOLO mask.
        if (maskOut.empty() && mParams.fallbackToYoloIfEmpty && !maskYolo.empty())
        {
            maskOut = maskYolo;
        }
    }
    else
    {
        // First frame: no prior state, output is the YOLO mask.
        maskOut = maskYolo;
    }

    // Ensure a non-empty output for downstream consumers (size = current frame).
    if (maskOut.empty())
        maskOut = cv::Mat::zeros(gray.size(), CV_8UC1);

    // 4. Update state for the next frame.
    mPrevGray = gray.clone();
    if (mParams.policy == SeedPolicy::FromYolo)
        mPrevMask = maskYolo.empty() ? maskOut.clone() : maskYolo.clone();
    else // FromOutput
        mPrevMask = maskOut.clone();
    mHasPrev  = true;
    mFrameNum++;

    return maskOut;
}

void MaskTracker::reset()
{
    mPrevGray.release();
    mPrevMask.release();
    mHasPrev = false;
    mFrameNum = 0;
}
