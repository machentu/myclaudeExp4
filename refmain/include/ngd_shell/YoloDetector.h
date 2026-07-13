#ifndef MASKTRACKER_YOLODETECTOR_H
#define MASKTRACKER_YOLODETECTOR_H

#include <opencv2/dnn.hpp>
#include <string>
#include <vector>

// YoloDetector: single-threaded YOLO person detector that produces a dynamic
// object mask via depth-consistency segmentation.
//
// Stripped from NGD-SLAM/src/YOLO.cc. All threading / Tracking dependencies
// (Run/InsertInput/GetOutput/mutex/SetTracker) were removed. detect() is a
// synchronous, blocking call.
class YoloDetector
{
public:
    struct Params
    {
        float confThreshold = 0.5f;          // detection confidence
        float nmsThreshold  = 0.4f;          // NMS IoU
        int   inpWidth      = 320;           // network input size
        int   inpHeight      = 320;
        std::string classesFile;             // coco.names (line 0 == person)
        std::string modelCfg;                // yolo-fastest-xl.cfg
        std::string modelWeights;            // yolo-fastest-xl.weights
        std::string netName    = "yolo-fastest";
        // Boxes whose aspect ratio exceeds this are tightened to 0.7x (the
        // original NGD-SLAM hard-coded 0 here, so tightening always applied).
        // Pass a large value (e.g. 1e9) to disable tightening.
        float maxBoxRatio     = 0.0f;
    };

    explicit YoloDetector(const Params& p);
    ~YoloDetector() = default;

    // Detect dynamic objects on a single frame.
    // imRGB: BGR or RGB image (any of CV_8UC3).
    // imDepthMeters: CV_32F depth in METERS (TUM raw / 5000). Must match imRGB size.
    // Returns CV_8UC1 mask, same size as imRGB: 1 = dynamic, 0 = static.
    // Returns an empty Mat if the detector failed to initialize.
    // (Non-const: cv::dnn::Net::setInput/forward are non-const.)
    cv::Mat detect(const cv::Mat& imRGB, const cv::Mat& imDepthMeters);

    bool initialized() const { return mInit; }

private:
    // Postprocess YOLO outputs into a dynamic mask (person class only,
    // depth-consistency segmentation). Stripped from YOLO::postprocess.
    cv::Mat postprocess(const cv::Mat& imRGB, const cv::Mat& imDepthMeters,
                        const std::vector<cv::Mat>& outs) const;

    cv::dnn::Net mNet;
    Params       mParams;
    bool         mInit = false;
};

#endif // MASKTRACKER_YOLODETECTOR_H
