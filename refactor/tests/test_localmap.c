/* test_localmap.c — TrackLocalMap end-to-end after TrackWithMotionModel.
 *
 * Same synthetic front-parallel scene as test_motion, but the current frame
 * uses id=100 so the reloc-recent gate (id < mnLastRelocFrameId + mMaxFrames =
 * 0+60) is FALSE -> the non-IMU >=30 inlier gate applies. After TWM matches the
 * init MPs, TrackLocalMap rebuilds the local map ({KF0} + its MPs), re-projects
 * the un-matched ones via isInFrustum + SearchByProjection #1, re-runs PoseOpt,
 * and counts mnMatchesInliers. */
#include "ngd/orb.h"
#include "ngd/calib.h"
#include "ngd/frame.h"
#include "ngd/keyframe.h"
#include "ngd/mappoint.h"
#include "ngd/tracking.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { if(!(cond)){ printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); fails++; } } while(0)

#define W 640
#define H 480
#define NFEAT 1000
#define MAXK (NFEAT*2)
#define DX 5
#define DY 3

static unsigned int rng = 7u;
static float frand(void){ rng = rng*1664525u + 1013904223u; return (rng >> 8) * (1.0f/16777216.0f); }

int main(void)
{
    rng = 7u;
    static uint8_t img0[W*H], img1[W*H];
    for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) img0[y*W+x] = (uint8_t)(frand()*255);
    for (int by = 0; by < H; by += 50) for (int bx = 0; bx < W; bx += 50)
        for (int y = by; y < by+25 && y < H; ++y) for (int x = bx; x < bx+25 && x < W; ++x)
            img0[y*W+x] = (uint8_t)(((bx+by)/50) & 1 ? 255 : 0);
    for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) {
        int su = x - DX, sv = y - DY;
        if (su < 0) su = 0; if (su >= W) su = W-1;
        if (sv < 0) sv = 0; if (sv >= H) sv = H-1;
        img1[y*W+x] = img0[sv*W+su];
    }
    static float depth[W*H];
    for (int i = 0; i < W*H; ++i) depth[i] = 1.0f;

    ngd_orb_extractor ex; ngd_orb_init(&ex, NFEAT, 1.2f, 8, 20, 7);
    ngd_cam_ctx cam; ngd_cam_ctx_init(&cam, &ex, 535.4f, 539.2f, 320.1f, 247.6f, 0.0747f, 2.988f, W, H);

    ngd_keypoint kps0[MAXK], kps1[MAXK];
    static uint8_t desc0[MAXK*32], desc1[MAXK*32];
    ngd_frame f0, f1;
    ngd_frame_build_rgbd(&f0, &ex, &cam, img0, W, H, W, depth, W, 0, kps0, MAXK, desc0);
    /* frame1 id=100 -> non-reloc-recent (100 >= 0+60). */
    ngd_frame_build_rgbd(&f1, &ex, &cam, img1, W, H, W, depth, W, 100, kps1, MAXK, desc1);
    CHECK(f0.N > 500, "frame0 has enough features");

    ngd_keyframe *kf0 = NULL; ngd_mappoint **mps = NULL; int nmps = 0;
    ngd_stereo_initialization(&f0, &kf0, &mps, &nmps);
    CHECK(nmps > 0, "stereo init succeeds");
    printf("  init: %d map points\n", nmps);

    ngd_tracking tk; ngd_tracking_init(&tk);
    tk.current = &f1; tk.last = &f0; tk.sensor = NGD_SENSOR_RGBD;

    int ok1 = ngd_tracking_track_with_motion_model(&tk);
    CHECK(ok1 == 1, "TWM succeeds");
    int twm_inl = 0; for (int i = 0; i < f1.N; ++i) if (f1.mvpMapPoints[i] && !f1.mvbOutlier[i]) twm_inl++;
    printf("  TWM inliers: %d\n", twm_inl);

    int ok2 = ngd_tracking_track_local_map(&tk);
    printf("  TrackLocalMap: mnMatchesInliers=%d, nLocalKFs=%d, nLocalMPs=%d -> %s\n",
           tk.mnMatchesInliers, tk.nLocalKFs, tk.nLocalMPs, ok2 ? "OK" : "FAIL");
    CHECK(ok2 == 1, "TrackLocalMap succeeds (mnMatchesInliers>=30)");
    CHECK(tk.mnMatchesInliers >= 30, "mnMatchesInliers >= 30 (non-IMU gate)");
    CHECK(tk.nLocalKFs == 1, "local KFs == {KF0}");
    CHECK(tk.nLocalKFs >= 1 && tk.localKFs[0] == kf0, "localKFs[0] == kf0");
    CHECK(tk.nLocalMPs > 0, "local map points collected");

    for (int i = 0; i < nmps; ++i) ngd_mappoint_free(mps[i]);
    free(mps);
    ngd_keyframe_free(kf0);
    ngd_frame_free(&f0); ngd_frame_free(&f1);
    ngd_tracking_destroy(&tk);

    if (fails == 0) { printf("test_localmap: PASS\n"); return 0; }
    printf("test_localmap: %d FAILURES\n", fails);
    return 1;
}
