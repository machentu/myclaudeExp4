/* test_frame.c — validate RGBD Frame construction (stereo-from-depth, grid). */
#include "ngd/orb.h"
#include "ngd/calib.h"
#include "ngd/frame.h"

#include <stdio.h>
#include <math.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { if(!(cond)){ printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); fails++; } } while(0)
#define APPROX(a,b,eps) (fabsf((a)-(b)) <= (eps))

static unsigned int rng = 999u;
static float frand(void){ rng = rng*1664525u + 1013904223u; return (rng >> 8) * (1.0f/16777216.0f); }

#define W 640
#define H 480
#define NFEAT 1000
#define MAXK (NFEAT*2)

int main(void)
{
    static uint8_t img[W*H];
    for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) img[y*W+x] = (uint8_t)(frand()*255);
    for (int by = 0; by < H; by += 60) for (int bx = 0; bx < W; bx += 60)
        for (int y = by; y < by+30 && y < H; ++y) for (int x = bx; x < bx+30 && x < W; ++x)
            img[y*W+x] = (uint8_t)(((bx+by)/60) & 1 ? 255 : 0);

    /* constant 1.0 m depth everywhere */
    static float depth[W*H];
    for (int i = 0; i < W*H; ++i) depth[i] = 1.0f;

    ngd_orb_extractor ex;
    ngd_orb_init(&ex, NFEAT, 1.2f, 8, 20, 7);
    ngd_cam_ctx cam;
    ngd_cam_ctx_init(&cam, &ex, 535.4f, 539.2f, 320.1f, 247.6f, 0.0747f, 2.988f, W, H);
    float mbf = cam.mbf;

    ngd_keypoint kps[MAXK];
    static uint8_t desc[MAXK*32];

    ngd_frame f;
    int N = ngd_frame_build_rgbd(&f, &ex, &cam, img, W, H, W, depth, W, 1, kps, MAXK, desc);
    CHECK(N > 100, "frame built with features");
    printf("  frame N=%d, mbf=%.3f\n", N, mbf);

    /* stereo-from-depth: depth==1 everywhere -> uRight = u - mbf */
    int checked = 0;
    for (int i = 0; i < N && checked < 20; ++i) {
        if (f.depth[i] <= 0) continue;
        CHECK(APPROX(f.depth[i], 1.0f, 1e-4f), "mvDepth stored");
        CHECK(APPROX(f.uRight[i], f.keys[i].x - mbf, 1e-3f), "uRight = u - mbf/Z");
        checked++;
    }
    CHECK(checked > 0, "at least one keypoint with depth checked");

    /* grid: every keypoint binned (all are inset >=16, so in bounds) */
    int total = 0;
    int ncells = 64*48;
    for (int c = 0; c < ncells; ++c) total += f.grid[c].n;
    CHECK(total == N, "all keypoints binned into the grid");

    /* GetFeaturesInArea recovers a keypoint at its own location */
    int found = 0;
    for (int s = 0; s < 5 && s < N; ++s) {
        int idx = s * (N/5);
        if (idx >= N) idx = N-1;
        int out[16];
        int m = ngd_frame_get_features_in_area(&f, f.keys[idx].x, f.keys[idx].y, 2.0f, -1, -1, out, 16);
        int has_self = 0;
        for (int k = 0; k < m; ++k) if (out[k] == idx) has_self = 1;
        if (has_self) found++;
    }
    CHECK(found >= 4, "GetFeaturesInArea returns the keypoint itself");

    /* UnprojectStereo at identity pose: X=(u-cx)/fx, Y=(v-cy)/fy, Z=1 */
    ngd_frame_set_pose(&f, ngd_se3_identity());
    for (int s = 0; s < 5 && s < N; ++s) {
        int idx = s * (N/5); if (idx >= N) idx = N-1;
        if (f.depth[idx] <= 0) continue;
        ngd_vec3 X = ngd_frame_unproject_stereo(&f, idx);
        float ex_ = (f.keys[idx].x - cam.cam.cx) / cam.cam.fx;
        float ey  = (f.keys[idx].y - cam.cam.cy) / cam.cam.fy;
        CHECK(APPROX(X.x, ex_, 1e-3f) && APPROX(X.y, ey, 1e-3f) && APPROX(X.z, 1.0f, 1e-4f),
              "unproject at identity == camera point");
    }

    ngd_frame_free(&f);
    if (fails == 0) { printf("test_frame: PASS\n"); return 0; }
    printf("test_frame: %d FAILURES\n", fails);
    return 1;
}
