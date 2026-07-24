#ifndef NGD_APP_YAML_H
#define NGD_APP_YAML_H

/*
 * ngd_app/yaml.h — settings (TUM3.yaml) thin shell (OpenCV FileStorage).
 *
 * Parses the ORB-SLAM3 settings file into a flat config, then builds the
 * core's ORB extractor + camera context. Field names match Settings.cc
 * (Camera1.fx / Stereo.b / Stereo.ThDepth / RGBD.DepthMapFactor /
 *  ORBextractor.*) — OpenCV FileStorage reads "Camera1.fx" as a flat key.
 *
 * thDepth semantics: yaml "Stereo.ThDepth" is baseline-times (40.0); the
 * actual metre threshold is mThDepth = ThDepth * b (= 2.988 for TUM3), per
 * ORB-SLAM3 Settings. ngd_cam_ctx_init takes the metre value (test_kfinsert
 * passes 2.988).
 */

#include "ngd/calib.h"
#include "ngd/orb.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    /* Camera1 (pinhole). TUM3 zero distortion -> not stored. */
    float fx, fy, cx, cy;
    int   imgW, imgH;          /* Camera.width / Camera.height */
    float fps;                 /* Camera.fps (-> Tracking::mMaxFrames = round(fps)) */

    float baseline;            /* Stereo.b (m) */
    float thDepthRaw;          /* Stereo.ThDepth (baseline-times) */

    float depthMapFactor;      /* RGBD.DepthMapFactor (5000 for TUM) */

    int   nFeatures;           /* ORBextractor.nFeatures */
    float scaleFactor;         /* ORBextractor.scaleFactor */
    int   nLevels;             /* ORBextractor.nLevels */
    int   iniThFAST;           /* ORBextractor.iniThFAST */
    int   minThFAST;           /* ORBextractor.minThFAST */

    /* Viewer.Viewpoint* — the original Pangolin Map viewer's fixed camera
     * (Viewer.cc: ModelViewLookAt(X,Y,Z, 0,0,0, 0,-1,0), Projection F).
     * The refapp OpenCV viewer replicates this viewpoint so the on-screen
     * camera motion matches the original (scale = F / |(X,Y,Z)|). */
    float viewpointX, viewpointY, viewpointZ, viewpointF;
} ngd_app_config;

/* Parse the settings yaml. Returns 1 on success, 0 if the file won't open or
 * intrinsics are missing (fx==0). */
int ngd_app_load_yaml(const char *path, ngd_app_config *cfg);

/* Build the ORB extractor + camera context from cfg.
 *   thDepth_m = thDepthRaw * baseline  (Settings::mThDepth semantics)
 * Caller owns ex / cam (no allocation). */
void ngd_app_build(const ngd_app_config *cfg, ngd_orb_extractor *ex, ngd_cam_ctx *cam);

#ifdef __cplusplus
}
#endif
#endif /* NGD_APP_YAML_H */
