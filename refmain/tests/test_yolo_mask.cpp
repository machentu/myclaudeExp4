// test_yolo_mask.cpp — verify MaskTracker's YoloDetector (reused in-place) loads
// the yolo-fastest-xl Darknet model and produces a non-empty person mask on a
// sample image. Skips (not fails) if the model or image files are not found.
#include "YoloDetector.h"

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

#include <cstdio>
#include <string>
#include <vector>

static int fails = 0;
#define CHECK(cond, msg) do { if(!(cond)){ std::printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); ++fails; } } while(0)

static bool exists(const std::string &p)
{
    FILE *f = std::fopen(p.c_str(), "rb");
    if (!f) return false;
    std::fclose(f);
    return true;
}

int main()
{
    // The exe runs from <repo>/refmain/build; model + sample image live under
    // <repo>/Thirdparty. Try a few candidate relative roots for robustness.
    std::vector<std::string> roots = {
        "../../Thirdparty",          // refmain/build -> repo
        "../../../Thirdparty",       // refmain/build/<config> -> repo (fallback)
        "../Thirdparty",
        "Thirdparty",
    };
    std::string modelDir, imgPath;
    for (const auto &r : roots) {
        std::string cfg = r + "/YOLO/yolo-fastest-xl.cfg";
        std::string w   = r + "/YOLO/yolo-fastest-xl.weights";
        std::string nm  = r + "/YOLO/coco.names";
        if (exists(cfg) && exists(w) && exists(nm)) { modelDir = r + "/YOLO"; break; }
    }
    for (const auto &r : roots) {
        std::string img = r + "/yolov34-cpp-opencv-dnn/bus.jpg";
        if (exists(img)) { imgPath = img; break; }
    }

    if (modelDir.empty()) {
        std::printf("test_yolo_mask: SKIP (yolo-fastest-xl cfg/weights/names not found)\n");
        return 0;   // not a failure — model files absent
    }
    if (imgPath.empty()) {
        std::printf("test_yolo_mask: SKIP (sample image bus.jpg not found)\n");
        return 0;
    }

    YoloDetector::Params p;
    p.classesFile  = modelDir + "/coco.names";
    p.modelCfg     = modelDir + "/yolo-fastest-xl.cfg";
    p.modelWeights = modelDir + "/yolo-fastest-xl.weights";
    p.maxBoxRatio  = 0.0f;     // tighten every person box (original behavior)
    YoloDetector det(p);
    CHECK(det.initialized(), "YoloDetector initialized (Darknet net loaded)");

    cv::Mat imRGB = cv::imread(imgPath, cv::IMREAD_COLOR);
    CHECK(!imRGB.empty(), "sample image loaded");

    // Depth is required by the depth-consistency segmentation; use a constant
    // 1.0 m plane (every in-box pixel is valid and within the ±0.33 m gate, so
    // the mask covers the person box's largest connected component).
    cv::Mat depth(imRGB.rows, imRGB.cols, CV_32F, 1.0f);

    cv::Mat mask = det.detect(imRGB, depth);
    CHECK(!mask.empty(), "detect returned a mask");
    int nz = cv::countNonZero(mask);
    std::printf("  [yolo] %s: person mask = %d px (of %dx%d)\n",
                imgPath.c_str(), nz, imRGB.cols, imRGB.rows);
    CHECK(nz > 0, "YOLO produced a non-empty person mask on bus.jpg");

    if (fails == 0) { std::printf("test_yolo_mask: PASS\n"); return 0; }
    std::printf("test_yolo_mask: %d FAILURES\n", fails);
    return 1;
}
