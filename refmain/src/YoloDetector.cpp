#include "YoloDetector.h"

#include <opencv2/imgproc.hpp>
#include <opencv2/dnn.hpp>

#include <fstream>
#include <iostream>
#include <algorithm>

YoloDetector::YoloDetector(const Params& p) : mParams(p)
{
    std::cout << "Net use " << mParams.netName << std::endl;

    try
    {
        mNet = cv::dnn::readNetFromDarknet(mParams.modelCfg, mParams.modelWeights);
    }
    catch (const cv::Exception& e)
    {
        std::cerr << "ERROR: failed to load YOLO model: " << e.what() << std::endl;
        mInit = false;
        return;
    }

    mNet.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
    mNet.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);

    // Load class names (only used for logging / sanity; mask generation keys on classId==0).
    std::ifstream ifs(mParams.classesFile);
    if (!ifs.is_open())
    {
        std::cerr << "WARNING: could not open classes file: " << mParams.classesFile << std::endl;
    }

    mInit = true;
}

cv::Mat YoloDetector::postprocess(const cv::Mat& imRGB, const cv::Mat& imDepthMeters,
                                  const std::vector<cv::Mat>& outs) const
{
    std::vector<int>   classIds;
    std::vector<float> confidences;
    std::vector<cv::Rect> boxes;

    for (size_t i = 0; i < outs.size(); ++i)
    {
        // Scan through all the bounding boxes output from the network and keep only
        // the ones with high confidence scores.
        const float* data = (float*)outs[i].data;
        for (int j = 0; j < outs[i].rows; ++j, data += outs[i].cols)
        {
            cv::Mat scores = outs[i].row(j).colRange(5, outs[i].cols);
            cv::Point classIdPoint;
            double confidence;
            cv::minMaxLoc(scores, 0, &confidence, 0, &classIdPoint);
            if (confidence > mParams.confThreshold)
            {
                int centerX = (int)(data[0] * imRGB.cols);
                int centerY = (int)(data[1] * imRGB.rows);
                int width   = (int)(data[2] * imRGB.cols);
                int height  = (int)(data[3] * imRGB.rows);
                int left    = centerX - width / 2;
                int top     = centerY - height / 2;

                classIds.push_back(classIdPoint.x);
                confidences.push_back((float)confidence);
                boxes.push_back(cv::Rect(left, top, width, height));
            }
        }
    }

    // Non maximum suppression to eliminate redundant overlapping boxes.
    std::vector<int> indices;
    cv::Mat globalMask = cv::Mat::zeros(imRGB.rows, imRGB.cols, CV_8UC1);
    cv::dnn::NMSBoxes(boxes, confidences, mParams.confThreshold, mParams.nmsThreshold, indices);

    for (size_t i = 0; i < indices.size(); ++i)
    {
        int idx = indices[i];

        // Only person (coco class 0) contributes to the dynamic mask.
        if (classIds[idx] != 0)
            continue;

        cv::Mat localMask = cv::Mat::zeros(imRGB.rows, imRGB.cols, CV_8UC1);
        cv::Rect box = boxes[idx];
        cv::Rect boxTight = box;

        float aspectRatio = static_cast<float>(box.width) / static_cast<float>(box.height);
        if (aspectRatio > mParams.maxBoxRatio)
        {
            float newWidth  = box.width  * 0.7f;
            float newHeight = box.height * 0.7f;
            float diffWidth  = box.width  - newWidth;
            float diffHeight = box.height - newHeight;

            boxTight.x      += (int)(diffWidth  / 2);
            boxTight.y      += (int)(diffHeight / 2);
            boxTight.width   = (int)newWidth;
            boxTight.height  = (int)newHeight;
        }

        // Clamp to image bounds.
        boxTight.x = std::max(boxTight.x, 0);
        boxTight.y = std::max(boxTight.y, 0);
        boxTight.width  = std::min(boxTight.width,  imDepthMeters.cols - boxTight.x);
        boxTight.height = std::min(boxTight.height, imDepthMeters.rows - boxTight.y);

        // BUG FIX (bug #3): after clamping the box can have zero/negative area
        // (e.g. fully out of image). Skip to avoid divide-by-zero below.
        if (boxTight.width <= 0 || boxTight.height <= 0)
            continue;

        cv::Mat imBoxTight = imDepthMeters(boxTight);

        // Compute median depth in box.
        std::vector<float> validPixelsInBox;
        for (int y = 0; y < imBoxTight.rows; y++)
        {
            for (int x = 0; x < imBoxTight.cols; x++)
            {
                float val = imBoxTight.at<float>(y, x);
                if (val >= 0.05f) validPixelsInBox.push_back(val);
            }
        }
        // Require at least 70% valid depth pixels in the box.
        if (validPixelsInBox.size() / (float)(boxTight.width * boxTight.height) < 0.7f)
            continue;

        std::sort(validPixelsInBox.begin(), validPixelsInBox.end());
        float median = validPixelsInBox[validPixelsInBox.size() / 2];
        float third  = validPixelsInBox[validPixelsInBox.size() / 3];
        float value  = (median + third) / 2.0f;

        int expansion = 30;
        for (int y = 0; y < imDepthMeters.rows; ++y)
        {
            for (int x = 0; x < imDepthMeters.cols; ++x)
            {
                if (std::abs(imDepthMeters.at<float>(y, x) - value) <= 0.33f &&
                    x >= box.x - expansion && x < box.x + box.width  + expansion &&
                    y >= box.y - expansion && y < box.y + box.height + expansion)
                {
                    localMask.at<uchar>(y, x) = 1;
                }
            }
        }

        // Keep the largest connected component of this box's mask.
        cv::Mat labels, stats, centroids;
        int nLabels = cv::connectedComponentsWithStats(localMask, labels, stats, centroids, 4, CV_32S);

        int largestLabel = 0;
        int largestArea  = 0;
        for (int label = 1; label < nLabels; ++label)
        {
            int area = stats.at<int>(label, cv::CC_STAT_AREA);
            if (area > largestArea)
            {
                largestArea  = area;
                largestLabel = label;
            }
        }

        for (int y = 0; y < labels.rows; ++y)
        {
            for (int x = 0; x < labels.cols; ++x)
            {
                if (labels.at<int>(y, x) == largestLabel)
                    globalMask.at<uchar>(y, x) = 1;
            }
        }
    }

    // Dilate the final mask.
    int dilationSize = 3;
    cv::Mat dilationElement = cv::getStructuringElement(
        cv::MORPH_RECT, cv::Size(2 * dilationSize + 1, 2 * dilationSize + 1),
        cv::Point(dilationSize, dilationSize));
    cv::dilate(globalMask, globalMask, dilationElement);

    // Safety valve: if the mask covers most of the image, it's meaningless; clear it.
    int totalPixels = globalMask.rows * globalMask.cols;
    int countOne = cv::countNonZero(globalMask);
    if ((double)countOne / totalPixels > 0.8)
        globalMask = cv::Mat::zeros(imRGB.rows, imRGB.cols, CV_8UC1);

    return globalMask;
}

cv::Mat YoloDetector::detect(const cv::Mat& imRGB, const cv::Mat& imDepthMeters)
{
    if (!mInit)
    {
        std::cerr << "WARNING: YoloDetector not initialized, returning empty mask" << std::endl;
        return cv::Mat();
    }

    // Size sanity: mask generation indexes imDepthMeters by imRGB coordinates.
    if (imRGB.size() != imDepthMeters.size())
    {
        std::cerr << "WARNING: RGB and depth size mismatch ("
                  << imRGB.size() << " vs " << imDepthMeters.size()
                  << "), returning empty mask" << std::endl;
        return cv::Mat();
    }

    cv::Mat blob;
    cv::dnn::blobFromImage(imRGB, blob, 1 / 255.0,
                           cv::Size(mParams.inpWidth, mParams.inpHeight),
                           cv::Scalar(0, 0, 0), true, false);
    mNet.setInput(blob);
    std::vector<cv::Mat> outs;
    mNet.forward(outs, mNet.getUnconnectedOutLayersNames());

    return postprocess(imRGB, imDepthMeters, outs);
}
