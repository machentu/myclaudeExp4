// test_shell.cpp — verify the imio + yaml shell against real TUM3 data.
//
// Usage: test_shell <TUM3.yaml> [<rgb.png> <depth.png>]
//   - yaml only : checks the parsed config matches TUM3 known values.
//   - +rgb+depth: also builds a frame and checks N > 500 (StereoInit gate).
// Graceful: if a file is missing it reports SKIP/PASS for the part that ran.
#include "ngd_app/yaml.h"
#include "ngd_app/imio.h"
#include "ngd/orb.h"
#include "ngd/calib.h"
#include "ngd/frame.h"

#include <cstdio>
#include <cstdlib>

static int fails = 0;
#define CHECK(cond, msg) do { if(!(cond)){ printf("FAIL: %s\n", msg); fails++; } } while(0)

int main(int argc, char **argv)
{
    const char *yaml = (argc > 1) ? argv[1] : "Examples/RGB-D/TUM3.yaml";
    const char *rgb  = (argc > 2) ? argv[2] : nullptr;
    const char *dep  = (argc > 3) ? argv[3] : nullptr;

    ngd_app_config cfg;
    if (!ngd_app_load_yaml(yaml, &cfg)) {
        printf("FAIL: yaml load (%s)\n", yaml);
        return 1;
    }
    printf("cam  fx=%.2f fy=%.2f cx=%.2f cy=%.2f  %dx%d  b=%.4f  thDepth=%.3f m  dmf=%.0f\n",
           cfg.fx, cfg.fy, cfg.cx, cfg.cy, cfg.imgW, cfg.imgH,
           cfg.baseline, cfg.thDepthRaw * cfg.baseline, cfg.depthMapFactor);
    printf("orb  nFeatures=%d  scaleFactor=%.2f  nLevels=%d  ini=%d  min=%d\n",
           cfg.nFeatures, cfg.scaleFactor, cfg.nLevels, cfg.iniThFAST, cfg.minThFAST);

    // TUM3 known values (Examples/RGB-D/TUM3.yaml).
    CHECK(cfg.fx == 535.4f,      "fx mismatch");
    CHECK(cfg.fy == 539.2f,      "fy mismatch");
    CHECK(cfg.cx == 320.1f,      "cx mismatch");
    CHECK(cfg.cy == 247.6f,      "cy mismatch");
    CHECK(cfg.imgW == 640,       "imgW mismatch");
    CHECK(cfg.imgH == 480,       "imgH mismatch");
    CHECK(cfg.baseline == 0.0747f, "baseline mismatch");
    CHECK(cfg.thDepthRaw == 40.0f, "ThDepth mismatch");
    CHECK(cfg.depthMapFactor == 5000.0f, "DepthMapFactor mismatch");
    CHECK(cfg.nFeatures == 1000, "nFeatures mismatch");
    CHECK(cfg.nLevels == 8,      "nLevels mismatch");
    CHECK(cfg.iniThFAST == 20,   "iniThFAST mismatch");
    CHECK(cfg.minThFAST == 7,    "minThFAST mismatch");
    // mThDepth in metres = 40 * 0.0747 = 2.988.
    CHECK(cfg.thDepthRaw * cfg.baseline > 2.98f && cfg.thDepthRaw * cfg.baseline < 2.99f,
          "mThDepth != 2.988 m");
    if (fails == 0) printf("yaml: PASS (TUM3 values match)\n");

    if (!rgb || !dep) {
        printf("imio: SKIP (no rgb/depth paths given)\n");
        return fails ? 1 : 0;
    }

    ngd_orb_extractor ex;
    ngd_cam_ctx cam;
    ngd_app_build(&cfg, &ex, &cam);

    uint8_t *gray = nullptr; int gw = 0, gh = 0;
    if (!ngd_app_load_gray(rgb, &gray, &gw, &gh)) {
        printf("FAIL: gray load (%s)\n", rgb);
        return 1;
    }
    float *depth = nullptr; int dw = 0, dh = 0;
    if (!ngd_app_load_depth_meters(dep, cfg.depthMapFactor, &depth, &dw, &dh)) {
        printf("FAIL: depth load (%s)\n", dep);
        std::free(gray);
        return 1;
    }
    printf("img  %dx%d  depth %dx%d\n", gw, gh, dw, dh);
    CHECK(gw == cfg.imgW && gh == cfg.imgH, "image size != yaml");
    CHECK(dw == cfg.imgW && dh == cfg.imgH, "depth size != yaml");

    // Depth sanity: most TUM depth pixels are > 0; mean in metres ~[0.5, 5].
    double dsum = 0; int dc = 0;
    for (int i = 0; i < dw * dh; ++i) if (depth[i] > 0) { dsum += depth[i]; ++dc; }
    if (dc > 0) printf("depth valid pixels=%d/%d  mean=%.3f m\n", dc, dw*dh, dsum/dc);

    const int MAXK = 2000;
    ngd_keypoint *kps = (ngd_keypoint *)std::malloc(sizeof(ngd_keypoint) * MAXK);
    uint8_t *desc = (uint8_t *)std::malloc((size_t)MAXK * 32);
    ngd_frame f;
    int N = ngd_frame_build_rgbd(&f, &ex, &cam, gray, gw, gh, gw, depth, dw, 0,
                                 kps, MAXK, desc);
    printf("frame N=%d (StereoInit needs > 500)\n", N);
    CHECK(N > 500, "frame N too low for StereoInit");

    ngd_frame_free(&f);
    std::free(gray);
    std::free(depth);
    std::free(kps);
    std::free(desc);

    if (fails == 0) printf("imio+frame: PASS\n");
    return fails ? 1 : 0;
}
