// test_optflow_track.cpp — TrackWithOpticalFlow orchestration (ngd_app_track_with_optflow).
//
// Builds a last frame with stable MPs (Observations>=3), then:
//   - static scene (curr==last) -> OF recovers identity pose, mnMatchesInliers>=20, returns 1.
//   - large shift  -> returns 0 (few tracks / sanity) without crashing.
// Requires vcpkg OpenCV DLLs on PATH (LK + solvePnPRansac).
#include "ngd_app/optflow_track.h"
#include "ngd/orb.h"
#include "ngd/calib.h"
#include "ngd/frame.h"
#include "ngd/mappoint.h"
#include "ngd/se3.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <stdint.h>

static int fails = 0;
#define CHECK(cond, msg) do { if(!(cond)){ printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); fails++; } } while(0)

#define W 640
#define H 480
#define NFEAT 1000
#define MAXK (NFEAT*2)

static unsigned int rng = 7u;
static float frand(void){ rng = rng*1664525u + 1013904223u; return (rng >> 8) * (1.0f/16777216.0f); }

static void make_img(uint8_t *img) {
    for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) img[y*W+x] = (uint8_t)(frand()*255);
    for (int by = 0; by < H; by += 50) for (int bx = 0; bx < W; bx += 50)
        for (int y = by; y < by+25 && y < H; ++y) for (int x = bx; x < bx+25 && x < W; ++x)
            img[y*W+x] = (uint8_t)(((bx+by)/50) & 1 ? 255 : 0);
}

/* shift image by (sx,sy), clamping at borders (like test_kfinsert render_shifted). */
static void shift_img(const uint8_t *src, uint8_t *dst, int sx, int sy) {
    for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) {
        int su = x - sx, sv = y - sy;
        if (su < 0) su = 0; if (su >= W) su = W-1;
        if (sv < 0) sv = 0; if (sv >= H) sv = H-1;
        dst[y*W+x] = src[sv*W+su];
    }
}

int main(void)
{
    rng = 7u;
    static uint8_t gray[W*H];
    make_img(gray);
    static float depth[W*H];
    for (int i = 0; i < W*H; ++i) depth[i] = 1.5f;

    ngd_orb_extractor ex; ngd_orb_init(&ex, NFEAT, 1.2f, 8, 20, 7);
    ngd_cam_ctx cam; ngd_cam_ctx_init(&cam, &ex, 535.4f, 539.2f, 320.1f, 247.6f, 0.0747f, 2.988f, W, H);

    /* last frame: ORB + depth, pose=identity. Give each keypoint with depth a
     * stable MP (Observations=3) at its world (== camera) position. */
    ngd_keypoint kpsL[MAXK]; uint8_t descL[MAXK*32];
    ngd_frame *last = (ngd_frame*)calloc(1, sizeof(ngd_frame));
    ngd_frame_build_rgbd(last, &ex, &cam, gray, W, H, W, depth, W, 0, kpsL, MAXK, descL);
    CHECK(last->N > 500, "last frame has features");

    ngd_mappoint **mps = (ngd_mappoint**)calloc((size_t)last->N, sizeof(ngd_mappoint*));
    int nMP = 0;
    for (int i = 0; i < last->N; ++i) {
        if (last->depth[i] <= 0) continue;
        ngd_vec3 Xw = ngd_frame_unproject_stereo(last, i);   /* identity pose -> world=cam */
        float pos[3] = { Xw.x, Xw.y, Xw.z };
        ngd_mappoint *mp = ngd_mappoint_new(pos, NULL);
        for (int k = 0; k < 3; ++k) ngd_mappoint_add_observation(mp, NULL, i, -1);  /* nObs=3 */
        last->mvpMapPoints[i] = mp;
        mps[nMP++] = mp;
    }
    printf("last: N=%d, stable MPs (nObs>=3)=%d\n", last->N, nMP);

    ngd_app_system s; memset(&s, 0, sizeof(s));
    s.cam = cam;
    s.tk.velocity = ngd_se3_identity();
    s.vel_lk = ngd_se3_identity();

    /* ---- 1) static scene: curr == last ---- */
    {
        ngd_keypoint kpsC[MAXK]; uint8_t descC[MAXK*32];
        ngd_frame *cur = (ngd_frame*)calloc(1, sizeof(ngd_frame));
        ngd_frame_build_rgbd(cur, &ex, &cam, gray, W, H, W, depth, W, 1, kpsC, MAXK, descC);
        s.tk.last = last; s.tk.current = cur; s.tk.mnMatchesInliers = 0;

        int ok = ngd_app_track_with_optical_flow(&s, last, gray, gray, NULL);
        printf("static: ok=%d mnMatchesInliers=%d\n", ok, s.tk.mnMatchesInliers);
        CHECK(ok == 1, "static scene tracks");
        CHECK(s.tk.mnMatchesInliers >= 20, "static scene >=20 inliers");
        /* pose should be ~identity (points didn't move). */
        float dt = sqrtf(cur->pose.t.x*cur->pose.t.x + cur->pose.t.y*cur->pose.t.y + cur->pose.t.z*cur->pose.t.z);
        float qw = cur->pose.q.w;
        printf("static pose: |t|=%.4f qw=%.4f\n", dt, qw);
        CHECK(dt < 0.05f, "static pose translation ~0");
        CHECK(fabsf(qw) > 0.995f, "static pose rotation ~identity");
        ngd_frame_free(cur); free(cur);
    }

    /* ---- 2) large shift: should fail (few tracks / sanity) without crashing ---- */
    {
        static uint8_t shifted[W*H];
        shift_img(gray, shifted, 120, 80);
        ngd_keypoint kpsC[MAXK]; uint8_t descC[MAXK*32];
        ngd_frame *cur = (ngd_frame*)calloc(1, sizeof(ngd_frame));
        ngd_frame_build_rgbd(cur, &ex, &cam, shifted, W, H, W, depth, W, 2, kpsC, MAXK, descC);
        s.tk.last = last; s.tk.current = cur; s.tk.mnMatchesInliers = 0;
        s.tk.velocity = ngd_se3_identity();

        int ok = ngd_app_track_with_optical_flow(&s, last, gray, shifted, NULL);
        printf("large-shift: ok=%d mnMatchesInliers=%d (expected 0)\n", ok, s.tk.mnMatchesInliers);
        CHECK(ok == 0, "large shift rejected");
        ngd_frame_free(cur); free(cur);
    }

    for (int i = 0; i < nMP; ++i) ngd_mappoint_free(mps[i]);
    free(mps);
    ngd_frame_free(last); free(last);

    if (fails == 0) { printf("test_optflow_track: PASS\n"); return 0; }
    printf("test_optflow_track: %d FAILURES\n", fails);
    return 1;
}
