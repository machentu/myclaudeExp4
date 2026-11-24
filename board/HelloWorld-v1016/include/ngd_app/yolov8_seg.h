#ifndef NGD_APP_YOLOV8_SEG_H
#define NGD_APP_YOLOV8_SEG_H

/*
 * ngd_app/yolov8_seg.h -- YOLOv8-seg end-to-end dynamic-mask detector.
 *
 * Drop-in replacement for refmain/YoloDetector inside the refappv8seg driver.
 * Unlike YoloDetector (yolo-fastest-xl box detection + depth-consistency
 * segmentation + the LK PredictCurrentMask propagation chain), this loads a
 * YOLOv8-seg ONNX and produces a person segmentation mask directly, end to
 * end, every frame -- no LK optical-flow propagation needed.
 *
 * The C post-processing (decodePosition/decodeMask + sigmoid table + NMS +
 * object pool) is copied verbatim from testblurDnn
 * (oldworkspace/study/gesture/testblurDnn). LetterBox + the network->original
 * mask remap are re-implemented here because SLAM passes full-resolution
 * frames (640x480) whereas testblurDnn pre-resized its input to 448x320, so
 * the letterbox inverse (crop content region -> resize to original) is needed.
 *
 * detect() honours the YoloDetector::detect contract so the call site
 * (system.cpp:321) swaps with a one-line type change:
 *   in : imRGB (CV_8UC3), imDepthMeters (CV_32F, UNUSED -- end-to-end
 *        segmentation needs no depth; kept for interface compatibility)
 *   out: CV_8UC1 same size as imRGB, !=0 = dynamic (person), 0 = static
 */

#include <opencv2/core.hpp>
#include <opencv2/dnn.hpp>
#include <string>

class Yolov8SegDetector {
public:
    Yolov8SegDetector();
    ~Yolov8SegDetector();

    /* Load the yolov8n-seg ONNX (320x448 by default, matching outsize.h).
     * Returns false if the model fails to load. useCuda selects the OpenCV
     * DNN CUDA backend/target (needs an OpenCV build with CUDA). */
    bool init(const std::string &onnxPath, bool useCuda = false);
    bool initialized() const { return ok_; }

    /* Person/dynamic mask: CV_8UC1, imRGB.size(), !=0 = dynamic, 0 = static.
     * imDepthMeters is accepted but unused (end-to-end segmentation). */
    cv::Mat detect(const cv::Mat &imRGB, const cv::Mat &imDepthMeters);

    /* Per-stage timing breakdown of detect() (preprocess / forward / decode /
     * postproc), accumulated across all calls. Print after the main stage table. */
    void print_timing(int nFrames) const;

private:
    cv::dnn::Net net_;
    bool ok_ = false;
    int netW_ = 448;   /* outsize.h INPUT_WIDTH  */
    int netH_ = 320;   /* outsize.h INPUT_HEIGHT */
    /* sub-stage timing accumulators (ms), updated by detect(). */
    double t_pre_ = 0.0, t_fwd_ = 0.0, t_dec_ = 0.0, t_post_ = 0.0;
    long long n_det_ = 0;
};

#endif /* NGD_APP_YOLOV8_SEG_H */
