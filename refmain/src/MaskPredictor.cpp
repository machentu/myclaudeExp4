#include "MaskPredictor.h"

#include <opencv2/imgproc.hpp>
#include <opencv2/video/tracking.hpp>   // cv::calcOpticalFlowPyrLK
#include <algorithm>
#include <limits>

cv::Mat MaskPredictor::predict(const cv::Mat& prevGray, const cv::Mat& prevMask,
                               const cv::Mat& curGray,  const cv::Mat& curDepthMeters,
                               const Params& p)
{
    // Guard: all four inputs must agree in size and be non-empty.
    if (prevGray.empty() || prevMask.empty() || curGray.empty() || curDepthMeters.empty())
        return cv::Mat::zeros(curGray.size(), CV_8UC1);
    if (prevGray.size() != curGray.size() || curGray.size() != curDepthMeters.size()
        || prevMask.size() != curGray.size())
        return cv::Mat::zeros(curGray.size(), CV_8UC1);

    std::vector<cv::Point2f> lastDynaPoints, currDynaPoints;
    std::vector<cv::Point3f> trackedDynaPoints;
    std::map<int, std::vector<cv::Point3f>> clusters;
    std::vector<unsigned char> status;
    std::vector<float> error;

    // Output mask starts as all-static (0). Static background is 0.
    cv::Mat outMask = cv::Mat::zeros(prevMask.size(), CV_8UC1);

    // Erode the previous mask (operate on a COPY so the caller's prevMask is
    // not mutated — original NGD-SLAM eroded mImMaskLastKey in place).
    cv::Mat prevMaskEroded = prevMask.clone();
    int erosionSize = p.erosionSize;
    cv::Mat erosionElement = cv::getStructuringElement(
        cv::MORPH_RECT, cv::Size(2 * erosionSize + 1, 2 * erosionSize + 1),
        cv::Point(erosionSize, erosionSize));
    cv::erode(prevMaskEroded, prevMaskEroded, erosionElement);

    extractDynaPoints(lastDynaPoints, prevGray, prevMaskEroded, p.cellSize);

    if (!lastDynaPoints.empty())
    {
        cv::calcOpticalFlowPyrLK(prevGray, curGray, lastDynaPoints, currDynaPoints, status, error);

        // LK flow can return points outside the image (negative / beyond
        // cols/rows) or NaN on divergence; cv::Mat::at<float> does no bounds
        // checking, so an out-of-range index is an access violation (0xC0000005).
        // On Linux the heap layout happened to make the OOB read survive; on
        // Windows it crashes. Skip such points.
        const int  depthCols   = curDepthMeters.cols;
        const int  depthRows   = curDepthMeters.rows;
        const bool depthValid  = !curDepthMeters.empty();

        for (size_t j = 0; j < status.size(); ++j)
        {
            if (!status[j])
                continue;

            const float px = currDynaPoints[j].x;
            const float py = currDynaPoints[j].y;

            // Reject NaN / out-of-image coordinates.
            if (px != px || py != py)              // NaN check
                continue;
            if (px < 0.0f || py < 0.0f || px >= (float)depthCols || py >= (float)depthRows)
                continue;
            if (!depthValid)
                continue;

            float depth = curDepthMeters.at<float>((int)py, (int)px);
            if (depth >= p.depthValidMin)
                trackedDynaPoints.push_back(cv::Point3f(px, py, depth));
        }

        clusterWithDBSCAN(clusters, trackedDynaPoints, p.eps, p.minPts);
        createMaskFromClusters(outMask, curDepthMeters, clusters, p.dilationSize);
    }

    return outMask;
}

void MaskPredictor::computePixelPotential(int& potential, const cv::Point2f& pixel, const cv::Mat& imGray)
{
    // Bresenham circle of radius 3 around the pixel (4-neighbors at distance 3).
    static const cv::Point2f circleOffsets[4] = {
        cv::Point2f(-3, 0), cv::Point2f(0, 3), cv::Point2f(3, 0), cv::Point2f(0, -3)
    };

    // If the center pixel itself is out of bounds, bail.
    if (pixel.x < 0 || pixel.x >= imGray.cols || pixel.y < 0 || pixel.y >= imGray.rows)
    {
        potential = -1;
        return;
    }

    int centerIntensity = (int)imGray.at<uchar>(pixel.y, pixel.x);

    // BUG FIX (bug #1, NGD-SLAM Tracking.cc:4325): isBrighter/isDarker were
    // left uninitialized and then ++'d (UB). Initialize to 0.
    int isBrighter = 0, isDarker = 0;
    int brightLowerBound = -1;
    int darkLowerBound   = -1;

    // Examine each pixel in the circle.
    // BUG FIX (bug #1, NGD-SLAM Tracking.cc:4333): the original accessed
    // imGray.at<uchar>(y, x) for circle pixels with NO bounds check, and the
    // earlier "ensure pixel within valid range" loop was dead code (its
    // potential=-1 was immediately overwritten by potential=0). Skip circle
    // pixels that fall outside the image.
    for (const cv::Point2f& offset : circleOffsets)
    {
        int x = (int)(pixel.x + offset.x);
        int y = (int)(pixel.y + offset.y);
        if (x < 0 || x >= imGray.cols || y < 0 || y >= imGray.rows)
            continue;

        int intensity = (int)imGray.at<uchar>(y, x);

        if (intensity > centerIntensity)
        {
            int diff = intensity - centerIntensity;
            if (diff < brightLowerBound || brightLowerBound == -1) brightLowerBound = diff;
            isBrighter++;
        }
        else if (intensity < centerIntensity)
        {
            int diff = centerIntensity - intensity;
            if (diff < darkLowerBound || darkLowerBound == -1) darkLowerBound = diff;
            isDarker++;
        }
    }

    if (isBrighter >= 3)      potential = 200 + brightLowerBound;
    else if (isDarker >= 3)   potential = 200 + darkLowerBound;
    else                      potential = std::max(brightLowerBound, darkLowerBound);
}

void MaskPredictor::extractDynaPoints(std::vector<cv::Point2f>& out,
                                      const cv::Mat& imGray, const cv::Mat& imMask, int cellSize)
{
    out.clear();
    // Guard: if either image is empty or they disagree in size, accessing
    // imMask.at<> in the loop below is an out-of-bounds access violation.
    if (imGray.empty() || imMask.empty() ||
        imGray.cols != imMask.cols || imGray.rows != imMask.rows)
        return;
    if (cellSize <= 0)
        return;

    int threshold = 250;

    for (int y = 0; y < imGray.rows; y += cellSize)
    {
        for (int x = 0; x < imGray.cols; x += cellSize)
        {
            int maxPotential = -1;
            cv::Point2f bestPixel(-1, -1);

            int dyEnd = std::min(cellSize, imGray.rows - y);
            int dxEnd = std::min(cellSize, imGray.cols - x);
            for (int dy = 0; dy < dyEnd; ++dy)
            {
                for (int dx = 0; dx < dxEnd; ++dx)
                {
                    cv::Point2f currPixel(x + dx, y + dy);
                    if ((int)imMask.at<uchar>(currPixel.y, currPixel.x) == 1)
                    {
                        int potential = 0;
                        computePixelPotential(potential, currPixel, imGray);
                        if (potential > maxPotential)
                        {
                            maxPotential = potential;
                            bestPixel = currPixel;
                        }
                    }
                    if (maxPotential > threshold) break;
                }
                if (maxPotential > threshold) break;
            }

            if (maxPotential != -1 && bestPixel.x > 0 && bestPixel.y > 0 &&
                bestPixel.x < imGray.cols && bestPixel.y < imGray.rows)
            {
                out.push_back(cv::Point2f(bestPixel.x, bestPixel.y));
            }
        }
    }
}

void MaskPredictor::clusterWithDBSCAN(std::map<int, std::vector<cv::Point3f>>& clusters,
                                      const std::vector<cv::Point3f>& points, float eps, int minPts)
{
    int clusterId = 0, level = 1;
    float range = 0.5f, minSize = 20.0f, step = 0.1f;
    std::vector<int> clusterIds(points.size(), -2);
    std::vector<cv::Point3f> tempPoints;
    std::vector<cv::Point3f> segmentedPoints;
    std::vector<cv::Point3f> sortedPoints = points;

    std::sort(sortedPoints.begin(), sortedPoints.end(),
              [](const cv::Point3f& a, const cv::Point3f& b) { return a.z < b.z; });

    // Stage 1: depth segmentation.
    for (size_t i = 0; i < sortedPoints.size(); ++i)
    {
        const cv::Point3f& point = sortedPoints[i];
        bool addPoint = true;

        if (!tempPoints.empty() &&
            ((point.z - tempPoints.back().z > step) || (point.z - tempPoints.front().z > range)))
            addPoint = false;

        if (addPoint) tempPoints.push_back(point);
        else
        {
            if (tempPoints.size() > minSize)
            {
                for (auto& pt : tempPoints) pt.z = 200 * level;
                level++;
                segmentedPoints.insert(segmentedPoints.end(), tempPoints.begin(), tempPoints.end());
            }
            tempPoints.clear();
            tempPoints.push_back(point);
        }
    }
    if (segmentedPoints.empty()) segmentedPoints = points;

    // Stage 2: 3D DBSCAN.
    auto regionQuery = [&](int idx) -> std::vector<int>
    {
        std::vector<int> neighbors;
        for (size_t i = 0; i < segmentedPoints.size(); ++i)
        {
            if (cv::norm(segmentedPoints[idx] - segmentedPoints[i]) < eps)
                neighbors.push_back((int)i);
        }
        return neighbors;
    };

    for (size_t i = 0; i < segmentedPoints.size(); ++i)
    {
        if (clusterIds[i] != -2) continue; // already visited

        auto neighbors = regionQuery((int)i);
        if (neighbors.size() < (size_t)minPts)
        {
            clusterIds[i] = -1; // noise
        }
        else
        {
            clusterId++;
            for (size_t j = 0; j < neighbors.size(); ++j)
            {
                int neighIdx = neighbors[j];
                if (clusterIds[neighIdx] == -1) clusterIds[neighIdx] = clusterId;
                if (clusterIds[neighIdx] != -2) continue;

                clusterIds[neighIdx] = clusterId;
                auto newNeighbors = regionQuery(neighIdx);
                if (newNeighbors.size() >= (size_t)minPts)
                    neighbors.insert(neighbors.end(), newNeighbors.begin(), newNeighbors.end());
            }
        }
    }

    for (size_t i = 0; i < segmentedPoints.size(); ++i)
    {
        if (clusterIds[i] > 0)
            clusters[clusterIds[i]].push_back(segmentedPoints[i]);
    }
}

void MaskPredictor::createMaskFromClusters(cv::Mat& outMask, const cv::Mat& curDepthMeters,
                                           const std::map<int, std::vector<cv::Point3f>>& clusters,
                                           int dilationSize)
{
    // Guard: the inner loops index curDepthMeters by (y,x) derived from outMask's
    // dimensions. If they disagree in size, .at<float> goes out of bounds
    // -> 0xC0000005. Bail out on mismatch.
    if (outMask.empty() || curDepthMeters.empty() ||
        outMask.cols != curDepthMeters.cols || outMask.rows != curDepthMeters.rows)
        return;

    for (const auto& cluster : clusters)
    {
        const std::vector<cv::Point3f>& pts = cluster.second;
        if (pts.size() <= 1)
            continue;

        cv::Mat localMask = cv::Mat::zeros(outMask.rows, outMask.cols, CV_8UC1);
        std::vector<float> pointsDepth;

        float minX = std::numeric_limits<float>::max(),  minY = std::numeric_limits<float>::max();
        float maxX = std::numeric_limits<float>::lowest(), maxY = std::numeric_limits<float>::lowest();

        // Find the bounding box and collect valid depths.
        for (const cv::Point3f& pt : pts)
        {
            minX = std::min(minX, pt.x);
            maxX = std::max(maxX, pt.x);
            minY = std::min(minY, pt.y);
            maxY = std::max(maxY, pt.y);

            // Bounds-check before indexing (pt.x/pt.y came from LK-clipped points,
            // but be defensive anyway).
            int ix = (int)pt.x, iy = (int)pt.y;
            if (ix < 0 || ix >= curDepthMeters.cols || iy < 0 || iy >= curDepthMeters.rows)
                continue;
            float depth = curDepthMeters.at<float>(iy, ix);
            if (depth >= 0.05f) pointsDepth.push_back(depth);
        }

        // BUG FIX (bug #2, NGD-SLAM Tracking.cc:4542): if no point had valid
        // depth, pointsDepth is empty and indexing it was UB. Skip.
        if (pointsDepth.empty())
            continue;

        cv::Point topLeft((int)minX, (int)minY);
        cv::Point bottomRight((int)maxX, (int)maxY);

        float width  = bottomRight.x - topLeft.x;
        float height = bottomRight.y - topLeft.y;
        if (width < 50 || height < 50)
            continue;

        cv::Size maskSize = localMask.size();
        int increaseWidth  = 15;
        int increaseHeight = 15;
        cv::Point adjustedTopLeft(std::max(0, topLeft.x - increaseWidth),
                                  std::max(0, topLeft.y - increaseHeight));
        cv::Point adjustedBottomRight(std::min(maskSize.width,  bottomRight.x + increaseWidth),
                                      std::min(maskSize.height, bottomRight.y + increaseHeight));

        cv::rectangle(localMask, adjustedTopLeft, adjustedBottomRight, cv::Scalar(1), cv::FILLED);

        std::sort(pointsDepth.begin(), pointsDepth.end());
        float value = pointsDepth[pointsDepth.size() / 2];

        // Depth gate: clear pixels whose depth deviates too much from the
        // cluster median.
        for (int y = 0; y < localMask.rows; y++)
        {
            for (int x = 0; x < localMask.cols; x++)
            {
                int val = (int)localMask.at<uchar>(y, x);
                if (val == 1 && std::abs(curDepthMeters.at<float>(y, x) - value) > 0.3f)
                {
                    localMask.at<uchar>(y, x) = 0;
                }
            }
        }

        // Keep the largest connected component.
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
                    outMask.at<uchar>(y, x) = 1;
            }
        }
    }

    // Final dilation.
    cv::Mat dilationElement = cv::getStructuringElement(
        cv::MORPH_RECT, cv::Size(2 * dilationSize + 1, 2 * dilationSize + 1),
        cv::Point(dilationSize, dilationSize));
    cv::dilate(outMask, outMask, dilationElement);
}
