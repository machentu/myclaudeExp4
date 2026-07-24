// ngd_app/yolov8_seg.cpp -- YOLOv8-seg end-to-end dynamic-mask detector.
//
// Decoder flow mirrors testblurDnn Yolov8Seg::Detect (USE_MY_DECODER path):
//   LetterBox -> blobFromImage -> net.forward([output0,output1])
//   decodePosition(output0,[output1]) -> per-detection records + mask coeffs
//   decodeMask(output1 protos, coeffs) -> merged float mask [outh1 x outw1]
// The merged float mask is then remapped network-space -> original image
// (resize -> *255 -> threshold -> crop letterbox content -> resize to original)
// and post-processed (7x7 dilate + >80% coverage safety valve), matching the
// YoloDetector postprocess behaviour downstream code expects.
#include "ngd_app/yolov8_seg.h"

#include "libkptdecode.h"   // decodePosition / decodeMask (extern "C")
#include "outsize.h"        // INPUT_WIDTH/HEIGHT, MASK_WIDTH/HEIGHT, SIZE_*, MAX_DET

#include <opencv2/imgproc.hpp>
#include <opencv2/dnn.hpp>

#include <chrono>
#include <cmath>
#include <cstdio>

/* ---- LetterBox (faithful copy of testblurDnn yolov8_utils.cpp arithmetic).
 * params = [ratio_x, ratio_y, left_pad, top_pad]. Content region in network
 * input space is [left, left + round(W*ratio_x)] x [top, top + round(H*ratio_y)].
 * scaleUp defaults to true (same as the Yolov8Seg::Detect call site). */
static void LetterBox(const cv::Mat &image, cv::Mat &outImage, cv::Vec4d &params,
                      const cv::Size &newShape, const cv::Scalar &color = cv::Scalar(114, 114, 114))
{
    cv::Size shape = image.size();
    float r = std::min((float)newShape.height / (float)shape.height,
                       (float)newShape.width / (float)shape.width);
    /* scaleUp = true (default in Yolov8Seg::Detect): do not cap r at 1.0. */
    float ratio[2] = { r, r };
    int new_un_pad[2] = {
        (int)std::round((float)shape.width * r),
        (int)std::round((float)shape.height * r)
    };

    float dw = (float)(newShape.width - new_un_pad[0]);
    float dh = (float)(newShape.height - new_un_pad[1]);
    dw /= 2.0f;
    dh /= 2.0f;

    if (shape.width != new_un_pad[0] && shape.height != new_un_pad[1])
        cv::resize(image, outImage, cv::Size(new_un_pad[0], new_un_pad[1]));
    else
        outImage = image.clone();

    int top = int(std::round(dh - 0.1f));
    int bottom = int(std::round(dh + 0.1f));
    int left = int(std::round(dw - 0.1f));
    int right = int(std::round(dw + 0.1f));
    params[0] = ratio[0];
    params[1] = ratio[1];
    params[2] = left;
    params[3] = top;
    cv::copyMakeBorder(outImage, outImage, top, bottom, left, right,
                       cv::BORDER_CONSTANT, color);
}

Yolov8SegDetector::Yolov8SegDetector() {}
Yolov8SegDetector::~Yolov8SegDetector() {}

bool Yolov8SegDetector::init(const std::string &onnxPath, bool useCuda)
{
    ok_ = false;
    try {
        net_ = cv::dnn::readNetFromONNX(onnxPath);
    } catch (const std::exception &) {
        std::fprintf(stderr, "Yolov8SegDetector: readNetFromONNX failed for %s\n", onnxPath.c_str());
        return false;
    }
    if (useCuda) {
        net_.setPreferableBackend(cv::dnn::DNN_BACKEND_CUDA);
        net_.setPreferableTarget(cv::dnn::DNN_TARGET_CUDA);
    } else {
        net_.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        net_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
    }
    ok_ = !net_.empty();
    return ok_;
}

cv::Mat Yolov8SegDetector::detect(const cv::Mat &imRGB, const cv::Mat & /*imDepthMeters*/)
{
    if (!ok_) return cv::Mat();
    const int W = imRGB.cols, H = imRGB.rows;
    auto t0 = std::chrono::steady_clock::now();

    /* 1. preprocess: letterbox to netW_ x netH_, BGR->RGB, /255. */
    cv::Mat netInputImg;
    cv::Vec4d params;
    LetterBox(imRGB, netInputImg, params, cv::Size(netW_, netH_));
    cv::Mat blob;
    cv::dnn::blobFromImage(netInputImg, blob, 1.0 / 255.0, cv::Size(netW_, netH_),
                           cv::Scalar(0, 0, 0), /*swapRB=*/true, /*crop=*/false);
    auto t1 = std::chrono::steady_clock::now();

    /* 2. forward both YOLOv8-seg heads. */
    net_.setInput(blob);
    std::vector<cv::Mat> netOutput;
    std::vector<std::string> outNames{ "output0", "output1" };
    net_.forward(netOutput, outNames);

    float *pOut  = (float *)netOutput[0].data;
    float *pOut1 = (float *)netOutput[1].data;
    int outw  = netOutput[0].size[2];
    int outh  = netOutput[0].size[1];
    int outw1 = netOutput[1].size[3];
    int outh1 = netOutput[1].size[2];
    int outc1 = netOutput[1].size[1];
    auto t2 = std::chrono::steady_clock::now();

    /* 3. decode detections + merge mask (verbatim from Yolov8Seg::Detect). */
    float outputDet[MAX_OUT_SIZE] = { 0 };
    float *pOutMask = new float[outw1 * outh1];
    int numDet = decodePosition(pOut, pOut1, outw, outh, outputDet, netW_, netH_, MAX_DET);
    decodeMask(pOut1, outw1, outh1, outc1, outputDet, netW_, netH_, numDet, pOutMask);
    auto t3 = std::chrono::steady_clock::now();

    /* 4. remap merged float mask (outh1 x outw1) -> original W x H binary.
     *   float -> resize to netW_ x netH_ -> *255 -> threshold 127 (matches
     *   BlurBackground bit-for-bit), then crop the letterbox content region
     *   and resize it back to the original image size. */
    cv::Mat fmask(outh1, outw1, CV_32F, pOutMask);
    cv::Mat ffull;
    cv::resize(fmask, ffull, cv::Size(netW_, netH_));
    cv::Mat u8;
    ffull.convertTo(u8, CV_8U, 255.0);
    cv::threshold(u8, u8, 127, 255, cv::THRESH_BINARY);

    float ratio_x = (float)params[0];
    float ratio_y = (float)params[1];
    int left = (int)params[2];
    int top  = (int)params[3];
    int content_w = (int)std::round((float)W * ratio_x);
    int content_h = (int)std::round((float)H * ratio_y);
    cv::Rect content(left, top, content_w, content_h);
    content &= cv::Rect(0, 0, netW_, netH_);

    cv::Mat maskOrig = cv::Mat::zeros(H, W, CV_8U);
    if (content.width > 0 && content.height > 0) {
        cv::Mat crop = u8(content);
        cv::resize(crop, maskOrig, cv::Size(W, H), 0, 0, cv::INTER_NEAREST);
    }

    /* 5. post-process: dilate 7x7 (cover edge ORB keypoints, same as
     *    YoloDetector::postprocess) + >80% coverage safety valve clear. */
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(7, 7));
    cv::dilate(maskOrig, maskOrig, kernel);
    if (cv::countNonZero(maskOrig) > (int)(0.8 * (double)W * (double)H))
        maskOrig.setTo(0);
    auto t4 = std::chrono::steady_clock::now();

    /* accumulate sub-stage timing for analysis */
    t_pre_  += std::chrono::duration<double, std::milli>(t1 - t0).count();
    t_fwd_  += std::chrono::duration<double, std::milli>(t2 - t1).count();
    t_dec_  += std::chrono::duration<double, std::milli>(t3 - t2).count();
    t_post_ += std::chrono::duration<double, std::milli>(t4 - t3).count();
    n_det_++;

    delete[] pOutMask;
    return maskOrig;   /* CV_8UC1, W x H, !=0 = dynamic (person), 0 = static */
}

void Yolov8SegDetector::print_timing(int /*nFrames*/) const
{
    const double total = t_pre_ + t_fwd_ + t_dec_ + t_post_;
    const long long n = n_det_;
    std::fprintf(stderr,
        "------- YOLOv8-seg detect() breakdown (calls=%lld, sum=%.1f ms) -------\n"
        "%-12s %12s %14s %8s\n", n, total, "stage", "total(ms)", "per-call(ms)", "share");
    auto row = [&](const char *name, double t) {
        double pc = (n > 0) ? t / (double)n : 0.0;
        double sh = (total > 1e-9) ? 100.0 * t / total : 0.0;
        std::fprintf(stderr, "%-12s %12.1f %14.3f %7.1f%%\n", name, t, pc, sh);
    };
    row("preprocess", t_pre_);   /* LetterBox + blobFromImage */
    row("forward",    t_fwd_);   /* net.setInput + net.forward (2 heads) */
    row("decode",     t_dec_);   /* decodePosition + decodeMask (C) */
    row("postproc",   t_post_);  /* remap + dilate + safety valve */
    std::fprintf(stderr, "(per-call = total/calls; share = fraction of detect() time.)\n");
}
