#ifndef NGD_APP_YOLOV8_SEG_H
#define NGD_APP_YOLOV8_SEG_H

/*
 * ngd_app/yolov8_seg.h — YOLOv8-seg end-to-end dynamic-mask detector (ONNX Runtime).
 *
 * Zero-OpenCV drop-in for refappv8seg_full. Uses ONNX Runtime C API
 * (onnxruntime_c_api.h) instead of cv::dnn::Net for ONNX inference.
 *
 * detect() writes the person segmentation mask directly into caller-provided
 * `mask_out` buffer (W*H uint8, !=0 = dynamic, 0 = static).
 *
 * The C post-processing (decodePosition/decodeMask + sigmoid table + NMS +
 * object pool) is copied verbatim from testblurDnn — unchanged.
 */

#include <stdint.h>
#include <string>

/* 网络输入分辨率 (编译期; 默认 448x320 与历史构建一致). 换模型分辨率时
 * 由构建脚本 -DNGD_YOLO_NET_W/H 覆盖 (如 256x480 clip 模型). */
#ifndef NGD_YOLO_NET_W
#define NGD_YOLO_NET_W 448
#endif
#ifndef NGD_YOLO_NET_H
#define NGD_YOLO_NET_H 320
#endif

/* Opaque DNN session handle (ORT or MNN, depending on NGD_DNN_BACKEND). */
struct DnnSession;

class Yolov8SegDetector {
public:
    Yolov8SegDetector();
    ~Yolov8SegDetector();

    /* Load ONNX model via selected backend (NGD_DNN_BACKEND).
     * Returns false if the model fails to load. */
    bool init(const std::string &onnxPath, bool useCuda = false);
    bool initialized() const { return ok_; }

    /* Person/dynamic mask: writes W*H uint8 into mask_out (caller-allocated).
     * imBGR: W*H*3 uint8 BGR interleaved (stride = 3*W).
     * imDepthMeters is accepted but unused (end-to-end segmentation).
     * Returns 1 on success, 0 if model not loaded. */
    int detect(const uint8_t *imBGR, const float * /*imDepthMeters*/,
               int W, int H, uint8_t *mask_out);

    /* Per-stage timing breakdown of detect() (preprocess / forward / decode /
     * postproc), accumulated across all calls. */
    void print_timing(int nFrames) const;

private:
    DnnSession *dnn_ = nullptr;
    bool ok_ = false;
    int netW_ = NGD_YOLO_NET_W;   /* outsize.h INPUT_WIDTH  */
    int netH_ = NGD_YOLO_NET_H;   /* outsize.h INPUT_HEIGHT */
    /* YOLO C decoder buffers (allocated once, reused per frame). */
    float *blob_      = nullptr;  /* 1 x 3 x netH_ x netW_ NCHW float */
    uint8_t *u8_blob_ = nullptr;  /* 1 x 3 x netH_ x netW_ NCHW uint8 (NPU tensor input) */
    uint8_t *letterbox_buf_ = nullptr;  /* netW_ * netH_ * 3 (BGR interleaved) */
    uint8_t *resize_tmp_    = nullptr;  /* max(W*H, netW_*netH_) * 3 */
    float   *ffull_buf_     = nullptr;  /* netW_ * netH_ float mask */
    uint8_t *u8_buf_        = nullptr;  /* netW_ * netH_ uint8 mask */
    uint8_t *crop_buf_      = nullptr;  /* netW_ * netH_ (crop region) */
    uint8_t *dilate_tmp_    = nullptr;  /* W * H (dilate scratch) */
    int      dilate_sz_     = 0;        /* allocated size of dilate_tmp_ */
    float   *out0_ = nullptr, *out1_ = nullptr;  /* ONNX output buffers */
    /* sub-stage timing accumulators (ms), updated by detect(). */
    double t_pre_ = 0.0, t_fwd_ = 0.0, t_dec_ = 0.0, t_post_ = 0.0;
    long long n_det_ = 0;
};

#endif /* NGD_APP_YOLOV8_SEG_H */
