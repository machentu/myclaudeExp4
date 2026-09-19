// main.cpp — rgbd_tum-equivalent driver for the refapp SLAM system (NGD masked-ORB).
//
// Usage: rgbd_tum_app <vocab> <settings> <sequence> <association> [yolo_dir]
//   yolo_dir defaults to ../../models (relative to refappv8seg/build/Release;
//   expects yolov8n-seg-320x448.onnx inside it).
// Single-threaded.
#include "ngd_app/system.h"

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#endif

static void LoadImages(const std::string &strAssociationFilename,
                       std::vector<std::string> &vstrImageFilenamesRGB,
                       std::vector<std::string> &vstrImageFilenamesD,
                       std::vector<double> &vTimestamps)
{
    std::ifstream fAssociation(strAssociationFilename.c_str());
    std::string s;
    while (std::getline(fAssociation, s)) {
        if (s.empty()) continue;
        std::stringstream ss(s);
        double t; std::string sRGB, sD;
        ss >> t; vTimestamps.push_back(t);
        ss >> sRGB; vstrImageFilenamesRGB.push_back(sRGB);
        ss >> t; ss >> sD; vstrImageFilenamesD.push_back(sD);
    }
}

int main(int argc, char **argv)
{
    if (argc < 5 || argc > 6) {
        std::fprintf(stderr, "Usage: %s path_to_vocabulary path_to_settings path_to_sequence path_to_association [yolo_dir]\n", argv[0]);
        return 1;
    }
    const std::string strVoc = argv[1];
    const std::string strSettings = argv[2];
    const std::string strSequence = argv[3];
    const std::string strAssociation = argv[4];
    const std::string strYoloDir = (argc >= 6) ? argv[5] : "../../models";

    std::vector<std::string> vstrImageFilenamesRGB, vstrImageFilenamesD;
    std::vector<double> vTimestamps;
    LoadImages(strAssociation, vstrImageFilenamesRGB, vstrImageFilenamesD, vTimestamps);
    const int nImages = (int)vstrImageFilenamesRGB.size();
    if (nImages == 0) { std::fprintf(stderr, "No images found in %s\n", strAssociation.c_str()); return 1; }
    if ((int)vstrImageFilenamesD.size() != nImages) { std::fprintf(stderr, "RGB/depth count mismatch\n"); return 1; }

    ngd_app_system SLAM;
    if (!ngd_app_system_init(&SLAM, strVoc.c_str(), strSettings.c_str(), strYoloDir.c_str())) {
        std::fprintf(stderr, "System init failed\n");
        return 1;
    }
    const float depthMapFactor = SLAM.cfg.depthMapFactor;

    std::vector<float> vTimesTrack(nImages, 0.0f);
    std::fprintf(stderr, "-------\nStart processing sequence ... %d images\n", nImages);

    int nTracked = 0;
    for (int ni = 0; ni < nImages; ++ni) {
        const std::string pathRGB = strSequence + "/" + vstrImageFilenamesRGB[ni];
        const std::string pathD   = strSequence + "/" + vstrImageFilenamesD[ni];
        const double tframe = vTimestamps[ni];

        cv::Mat imRGB = cv::imread(pathRGB, cv::IMREAD_COLOR);          // CV_8UC3
        cv::Mat imD   = cv::imread(pathD,   cv::IMREAD_UNCHANGED);      // CV_16U (TUM)
        if (imRGB.empty()) { std::fprintf(stderr, "Failed to load %s\n", pathRGB.c_str()); continue; }
        if (imD.empty())   { std::fprintf(stderr, "Failed to load %s\n", pathD.c_str());   continue; }
        /* depth -> float metres (GrabImageRGBD: convertTo CV_32F, 1/DepthMapFactor). */
        if (imD.type() != CV_32F) {
            cv::Mat d32;
            imD.convertTo(d32, CV_32F, 1.0f / depthMapFactor);
            imD = d32;
        }

        auto t1 = std::chrono::steady_clock::now();
        int ok = ngd_app_system_track_rgbd(&SLAM, imRGB, imD, tframe);
        auto t2 = std::chrono::steady_clock::now();
        const double ttrack = std::chrono::duration<double>(t2 - t1).count();
        vTimesTrack[ni] = (float)ttrack;
        if (ok) ++nTracked;

        double T = 0;
        if (ni < nImages - 1) T = vTimestamps[ni + 1] - tframe;
        else if (ni > 0)      T = tframe - vTimestamps[ni - 1];
        if (ttrack < T) {
#ifdef _WIN32
            Sleep((DWORD)((T - ttrack) * 1e3));
#else
            usleep((useconds_t)((T - ttrack) * 1e6));
#endif
        }
    }

    ngd_app_system_save_trajectory_tum(&SLAM, "CameraTrajectory.txt");
    ngd_app_system_save_keyframe_trajectory_tum(&SLAM, "KeyFrameTrajectory.txt");
    ngd_app_system_save_pose_covariance(&SLAM, "PoseCovariance.txt");

    std::sort(vTimesTrack.begin(), vTimesTrack.end());
    double total = 0; for (int i = 0; i < nImages; ++i) total += vTimesTrack[i];
    std::fprintf(stderr, "-------\nmedian tracking time: %.4f s\nmean tracking time: %.4f s\n",
                 vTimesTrack[nImages / 2], total / nImages);
    std::fprintf(stderr, "tracked %d/%d frames\n", nTracked, nImages);

    ngd_app_system_print_timing(&SLAM, nImages);
    ngd_app_system_shutdown(&SLAM);
    return 0;
}
