/* test_stereo_init.c — validate RGBD StereoInitialization (first keyframe). */
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
#define APPROX(a,b,eps) (fabsf((a)-(b)) <= (eps))

static unsigned int rng = 7u;
static float frand(void){ rng = rng*1664525u + 1013904223u; return (rng >> 8) * (1.0f/16777216.0f); }

#define W 640
#define H 480
#define NFEAT 1000
#define MAXK (NFEAT*2)

int main(void)
{
    static uint8_t img[W*H];
    for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) img[y*W+x] = (uint8_t)(frand()*255);
    for (int by = 0; by < H; by += 50) for (int bx = 0; bx < W; bx += 50)
        for (int y = by; y < by+25 && y < H; ++y) for (int x = bx; x < bx+25 && x < W; ++x)
            img[y*W+x] = (uint8_t)(((bx+by)/50) & 1 ? 255 : 0);
    static float depth[W*H];
    for (int i = 0; i < W*H; ++i) depth[i] = 1.0f;

    ngd_orb_extractor ex;
    ngd_orb_init(&ex, NFEAT, 1.2f, 8, 20, 7);
    ngd_cam_ctx cam;
    ngd_cam_ctx_init(&cam, &ex, 535.4f, 539.2f, 320.1f, 247.6f, 0.0747f, 2.988f, W, H);

    ngd_keypoint kps[MAXK];
    static uint8_t desc[MAXK*32];
    ngd_frame f;
    int N = ngd_frame_build_rgbd(&f, &ex, &cam, img, W, H, W, depth, W, 1, kps, MAXK, desc);
    printf("  frame N=%d (need >500 for init)\n", N);
    CHECK(N > 500, "enough features for initialization");

    ngd_keyframe *kf = NULL;
    ngd_mappoint **mps = NULL; int nmps = 0;
    int r = ngd_stereo_initialization(&f, &kf, &mps, &nmps);
    CHECK(r > 0, "stereo initialization succeeds");
    if (r <= 0 || !kf) {
        printf("test_stereo_init: init returned %d (kf=%p) — skipping kf checks\n", r, (void*)kf);
        if (fails == 0) { printf("test_stereo_init: PASS\n"); return 0; }
        printf("test_stereo_init: %d FAILURES\n", fails);
        return 1;
    }
    CHECK(r == nmps, "returned count matches array length");
    CHECK(kf != NULL, "keyframe created");
    printf("  created %d map points\n", nmps);

    /* pose identity -> camera centre at origin */
    ngd_vec3 ow = ngd_keyframe_get_camera_center(kf);
    CHECK(ngd_v3_norm(ow) < 1e-4f, "kf camera centre at origin (identity pose)");

    /* validate a sample of map points */
    int checked = 0;
    for (int s = 0; s < nmps && checked < 15; ++s) {
        ngd_mappoint *mp = mps[s];
        /* find the keypoint index in the frame that produced this mp */
        int idx = -1;
        for (int i = 0; i < N; ++i) if (f.mvpMapPoints[i] == mp) { idx = i; break; }
        CHECK(idx >= 0, "map point referenced by the frame");

        /* worldPos == unproject at identity (== camera point) */
        ngd_vec3 X = ngd_frame_unproject_stereo(&f, idx);
        CHECK(APPROX(mp->worldPos[0], X.x, 1e-3f) &&
              APPROX(mp->worldPos[1], X.y, 1e-3f) &&
              APPROX(mp->worldPos[2], X.z, 1e-3f), "worldPos == unprojected point");

        /* nObs: stereo (uRight>=0) -> 2, else 1 */
        int expObs = (f.uRight[idx] >= 0) ? 2 : 1;
        CHECK(ngd_mappoint_observations(mp) == expObs, "observation count (stereo=2)");

        /* representative descriptor == kf's descriptor at idx */
        uint8_t d[32];
        ngd_keyframe_get_descriptor(kf, idx, d);
        CHECK(memcmp(mp->descriptor, d, 32) == 0, "distinctive descriptor == kf descriptor");

        /* normal == normalize(Pos - origin) */
        ngd_vec3 nrm = ngd_v3(mp->worldPos[0], mp->worldPos[1], mp->worldPos[2]);
        nrm = ngd_v3_normalize(nrm);
        CHECK(APPROX(mp->normal[0], nrm.x, 1e-3f) &&
              APPROX(mp->normal[1], nrm.y, 1e-3f) &&
              APPROX(mp->normal[2], nrm.z, 1e-3f), "viewing normal == normalize(Pos)");
        checked++;
    }
    CHECK(checked > 0, "validated at least one map point");
    printf("  validated %d map points (pos/obs/descriptor/normal)\n", checked);

    /* cleanup */
    for (int i = 0; i < nmps; ++i) ngd_mappoint_free(mps[i]);
    free(mps);
    ngd_keyframe_free(kf);
    ngd_frame_free(&f);

    if (fails == 0) { printf("test_stereo_init: PASS\n"); return 0; }
    printf("test_stereo_init: %d FAILURES\n", fails);
    return 1;
}
