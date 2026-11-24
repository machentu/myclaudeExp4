/* test_framemask.c — NGD dynamic-mask Frame variant (ngd_frame_build_rgbd_mask).
 *
 * Synthetic image (random + checkerboard, like test_kfinsert) so ORB finds
 * features. Asserts:
 *   - NULL mask  -> N identical to ngd_frame_build_rgbd on the same image
 *   - right-half mask (mask!=0 for x>=W/2) -> all kept keypoints have x<W/2
 *   - full mask (all !=0) -> N==0
 * Frame.cc:312-419 port: drop keypoints whose mask pixel != 0.
 */
#include "ngd/orb.h"
#include "ngd/calib.h"
#include "ngd/frame.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { if(!(cond)){ printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); fails++; } } while(0)

#define W 640
#define H 480
#define NFEAT 1000
#define MAXK (NFEAT*2)

static unsigned int rng = 7u;
static float frand(void){ rng = rng*1664525u + 1013904223u; return (rng >> 8) * (1.0f/16777216.0f); }

int main(void)
{
    rng = 7u;
    static uint8_t img[W*H];
    for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) img[y*W+x] = (uint8_t)(frand()*255);
    for (int by = 0; by < H; by += 50) for (int bx = 0; bx < W; bx += 50)
        for (int y = by; y < by+25 && y < H; ++y) for (int x = bx; x < bx+25 && x < W; ++x)
            img[y*W+x] = (uint8_t)(((bx+by)/50) & 1 ? 255 : 0);
    static float depth[W*H];
    for (int i = 0; i < W*H; ++i) depth[i] = 1.5f;

    ngd_orb_extractor ex; ngd_orb_init(&ex, NFEAT, 1.2f, 8, 20, 7);
    ngd_cam_ctx cam; ngd_cam_ctx_init(&cam, &ex, 535.4f, 539.2f, 320.1f, 247.6f, 0.0747f, 2.988f, W, H);

    ngd_keypoint kpsA[MAXK]; uint8_t descA[MAXK*32];
    ngd_keypoint kpsB[MAXK]; uint8_t descB[MAXK*32];

    /* 1) baseline: no mask (NULL) == build_rgbd. */
    ngd_frame f0;
    int N0 = ngd_frame_build_rgbd(&f0, &ex, &cam, img, W, H, W, depth, W, 0, kpsA, MAXK, descA);
    ngd_frame f1;
    int N1 = ngd_frame_build_rgbd_mask(&f1, &ex, &cam, img, W, H, W, depth, W, 0, NULL, kpsB, MAXK, descB);
    printf("baseline: build_rgbd N=%d, build_rgbd_mask(NULL) N=%d\n", N0, N1);
    CHECK(N0 > 500, "baseline has enough features");
    CHECK(N1 == N0, "NULL mask gives identical N to build_rgbd");

    ngd_frame_free(&f0);
    ngd_frame_free(&f1);

    /* 2) right-half mask: drop all keypoints with x>=W/2. */
    static uint8_t mask_half[W*H];
    for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) mask_half[y*W+x] = (x >= W/2) ? 1 : 0;
    ngd_frame f2;
    int N2 = ngd_frame_build_rgbd_mask(&f2, &ex, &cam, img, W, H, W, depth, W, 1, mask_half, kpsA, MAXK, descA);
    printf("right-half mask: N=%d (all kept should have x<%d)\n", N2, W/2);
    CHECK(N2 > 0 && N2 < N0, "half-mask keeps fewer than baseline");
    int maxX = -1;
    for (int i = 0; i < N2; ++i) if ((int)f2.keys[i].x > maxX) maxX = (int)f2.keys[i].x;
    CHECK(maxX < W/2, "no kept keypoint in the masked (right) half");
    ngd_frame_free(&f2);

    /* 3) full mask: everything dynamic -> N==0. */
    static uint8_t mask_full[W*H];
    memset(mask_full, 1, sizeof(mask_full));
    ngd_frame f3;
    int N3 = ngd_frame_build_rgbd_mask(&f3, &ex, &cam, img, W, H, W, depth, W, 2, mask_full, kpsA, MAXK, descA);
    printf("full mask: N=%d\n", N3);
    CHECK(N3 == 0, "full mask -> 0 keypoints");
    ngd_frame_free(&f3);

    if (fails == 0) { printf("test_framemask: PASS\n"); return 0; }
    printf("test_framemask: %d FAILURES\n", fails);
    return 1;
}
