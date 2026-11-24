// ngd_app/yaml.cpp — settings (TUM3.yaml) thin shell (OpenCV FileStorage).
#include "ngd_app/yaml.h"

#include <opencv2/core.hpp>

extern "C" int ngd_app_load_yaml(const char *path, ngd_app_config *cfg)
{
    if (!path || !cfg) return 0;
    cv::FileStorage fs(path, cv::FileStorage::READ);
    if (!fs.isOpened()) return 0;

    // Defaults (mirror ORB-SLAM3 fallbacks; TUM3 supplies all of these).
    cfg->fx = cfg->fy = cfg->cx = cfg->cy = 0.0f;
    cfg->imgW = 640; cfg->imgH = 480;
    cfg->fps = 30.0f;
    cfg->baseline = 0.0f;
    cfg->thDepthRaw = 0.0f;
    cfg->depthMapFactor = 1.0f;
    cfg->nFeatures = 1000;
    cfg->scaleFactor = 1.2f;
    cfg->nLevels = 8;
    cfg->iniThFAST = 20;
    cfg->minThFAST = 7;
    cfg->viewpointX = 0.0f; cfg->viewpointY = -0.7f; cfg->viewpointZ = -1.8f; cfg->viewpointF = 500.0f;

    cv::FileNode n;
    n = fs["Camera1.fx"];         if (!n.empty()) cfg->fx = (float)n;
    n = fs["Camera1.fy"];         if (!n.empty()) cfg->fy = (float)n;
    n = fs["Camera1.cx"];         if (!n.empty()) cfg->cx = (float)n;
    n = fs["Camera1.cy"];         if (!n.empty()) cfg->cy = (float)n;
    n = fs["Camera.width"];       if (!n.empty()) cfg->imgW = (int)n;
    n = fs["Camera.height"];      if (!n.empty()) cfg->imgH = (int)n;
    n = fs["Camera.fps"];         if (!n.empty()) cfg->fps = (float)n;
    n = fs["Stereo.b"];           if (!n.empty()) cfg->baseline = (float)n;
    n = fs["Stereo.ThDepth"];     if (!n.empty()) cfg->thDepthRaw = (float)n;
    n = fs["RGBD.DepthMapFactor"];if (!n.empty()) cfg->depthMapFactor = (float)n;
    n = fs["ORBextractor.nFeatures"];  if (!n.empty()) cfg->nFeatures = (int)n;
    n = fs["ORBextractor.scaleFactor"];if (!n.empty()) cfg->scaleFactor = (float)n;
    n = fs["ORBextractor.nLevels"];    if (!n.empty()) cfg->nLevels = (int)n;
    n = fs["ORBextractor.iniThFAST"];  if (!n.empty()) cfg->iniThFAST = (int)n;
    n = fs["ORBextractor.minThFAST"];  if (!n.empty()) cfg->minThFAST = (int)n;
    n = fs["Viewer.ViewpointX"];       if (!n.empty()) cfg->viewpointX = (float)n;
    n = fs["Viewer.ViewpointY"];       if (!n.empty()) cfg->viewpointY = (float)n;
    n = fs["Viewer.ViewpointZ"];       if (!n.empty()) cfg->viewpointZ = (float)n;
    n = fs["Viewer.ViewpointF"];       if (!n.empty()) cfg->viewpointF = (float)n;

    fs.release();

    // Intrinsics must be present (pinhole).
    if (cfg->fx <= 0.0f || cfg->fy <= 0.0f) return 0;
    return 1;
}

extern "C" void ngd_app_build(const ngd_app_config *cfg, ngd_orb_extractor *ex, ngd_cam_ctx *cam)
{
    ngd_orb_init(ex, cfg->nFeatures, cfg->scaleFactor, cfg->nLevels, cfg->iniThFAST, cfg->minThFAST);
    // mThDepth (m) = Stereo.ThDepth * Stereo.b  (ORB-SLAM3 Settings semantics).
    const float thDepth_m = cfg->thDepthRaw * cfg->baseline;
    ngd_cam_ctx_init(cam, ex, cfg->fx, cfg->fy, cfg->cx, cfg->cy,
                     cfg->baseline, thDepth_m, cfg->imgW, cfg->imgH);
}
