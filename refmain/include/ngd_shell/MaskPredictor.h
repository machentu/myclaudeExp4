#ifndef MASKTRACKER_MASKPREDICTOR_H
#define MASKTRACKER_MASKPREDICTOR_H

#include <opencv2/core.hpp>
#include <map>
#include <vector>

// MaskPredictor: propagates the previous frame's dynamic mask to the current
// frame using LK optical flow + DBSCAN clustering.
//
// Stripped from NGD-SLAM/src/Tracking.cc (PredictCurrentMask chain, ~4249-4587).
// Tracking-class members (mImMask / mImMaskLastKey / mImGrayLastKey / mImGray /
// mImDepth2) have been externalized as function parameters. All methods are
// static / pure functions. No ORB / MapPoint / pose dependency.
class MaskPredictor
{
public:
    struct Params
    {
        int   cellSize      = 15;     // ExtractDynaPoints grid size
        float eps           = 50.0f;  // DBSCAN neighborhood radius
        int   minPts        = 15;     // DBSCAN min points
        int   erosionSize   = 5;      // erode prev mask before seeding
        int   dilationSize = 3;      // final dilation
        float depthValidMin = 0.05f;  // depth filter lower bound (meters)
    };

    // Main entry: propagate prevMask to the current frame.
    //   prevGray / curGray    : CV_8UC1 grayscale, same size.
    //   prevMask               : CV_8UC1 mask from previous frame (1=dynamic).
    //   curDepthMeters         : CV_32F depth in meters, same size as curGray.
    // Returns CV_8UC1 mask (1=dynamic), same size as curGray. May be all-zero
    // if no dynamic points survived LK / clustering.
    static cv::Mat predict(const cv::Mat& prevGray, const cv::Mat& prevMask,
                           const cv::Mat& curGray,  const cv::Mat& curDepthMeters,
                           const Params& p = Params());

    // Extract seed points (max FAST-style corner response per grid cell) from
    // the dynamic region of imMask on imGray.
    static void extractDynaPoints(std::vector<cv::Point2f>& out,
                                  const cv::Mat& imGray, const cv::Mat& imMask, int cellSize);

    // FAST-style corner potential (Bresenham circle radius 3). potential=-1 if
    // any circle pixel is out of bounds.
    static void computePixelPotential(int& potential, const cv::Point2f& pixel, const cv::Mat& imGray);

    // Two-stage clustering: depth segmentation then 3D DBSCAN.
    static void clusterWithDBSCAN(std::map<int, std::vector<cv::Point3f>>& clusters,
                                  const std::vector<cv::Point3f>& points, float eps, int minPts);

    // Build a mask from clusters: bounding box + depth gate (±0.3 m of median)
    // + largest connected component + dilation. Writes into outMask.
    static void createMaskFromClusters(cv::Mat& outMask, const cv::Mat& curDepthMeters,
                                      const std::map<int, std::vector<cv::Point3f>>& clusters,
                                      int dilationSize);
};

#endif // MASKTRACKER_MASKPREDICTOR_H
