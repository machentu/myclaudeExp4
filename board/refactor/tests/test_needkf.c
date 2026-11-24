/* test_needkf.c — ngd_tracking_need_new_keyframe unit test.
 *
 * Hand-builds a minimal tk with a map (nKFs), refKF (tracked MPs), and a
 * current frame, then exercises the non-IMU core conditions (c1a/c1b/c1c/c2):
 *   - strong tracking, recent frame            -> 0
 *   - weak tracking (c2) + past mMaxFrames(c1a)-> 1
 *   - weak tracking but reloc cooldown         -> 0
 *   - strong tracking but c1a (frames elapsed) -> c2 false -> 0
 */
#include "ngd/tracking.h"
#include "ngd/map.h"
#include "ngd/keyframe.h"
#include "ngd/mappoint.h"
#include "ngd/frame.h"
#include "ngd/calib.h"
#include "ngd/orb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { if(!(cond)){ printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); fails++; } } while(0)

static ngd_orb_extractor g_ex;
static ngd_cam_ctx g_cam;
static int g_inited = 0;
static void init_cam(void){
    if (g_inited) return; g_inited = 1;
    ngd_orb_init(&g_ex, 1000, 1.2f, 8, 20, 7);
    ngd_cam_ctx_init(&g_cam, &g_ex, 535.4f, 539.2f, 320.1f, 247.6f, 0.0747f, 2.988f, 640, 480);
}

/* minimal KF with N mvpMapPoints, each a live MP with nObs observations. */
static ngd_keyframe *make_refkf(uint64_t id, int N, int obsPerMP)
{
    init_cam();
    ngd_keyframe *kf = (ngd_keyframe*)calloc(1, sizeof(ngd_keyframe));
    kf->mnId = id;
    kf->N = N;
    kf->cam = g_cam;
    kf->keys = (ngd_keypoint*)calloc((size_t)(N>0?N:1), sizeof(ngd_keypoint));
    kf->descriptors = (uint8_t*)calloc((size_t)(N>0?N:1)*32, 1);
    kf->uRight = (float*)calloc((size_t)(N>0?N:1), sizeof(float));
    kf->depth  = (float*)calloc((size_t)(N>0?N:1), sizeof(float));
    kf->mvpMapPoints = (ngd_mappoint**)calloc((size_t)(N>0?N:1), sizeof(ngd_mappoint*));
    for (int i = 0; i < N; ++i) { kf->uRight[i] = -1; kf->depth[i] = -1; }
    for (int i = 0; i < N; ++i) {
        float pos[3] = {0,0,1};
        ngd_mappoint *mp = ngd_mappoint_new(pos, kf);
        /* give it obsPerMP observations via nObs (TrackedMapPoints reads nObs) */
        mp->nObs = obsPerMP;
        kf->mvpMapPoints[i] = mp;
    }
    return kf;
}
static void free_refkf(ngd_keyframe *kf)
{
    for (int i = 0; i < kf->N; ++i) if (kf->mvpMapPoints[i]) ngd_mappoint_free(kf->mvpMapPoints[i]);
    free(kf->keys); free(kf->descriptors); free(kf->uRight); free(kf->depth); free(kf->mvpMapPoints);
    free(kf);
}

/* minimal frame: mnId + cam (need_new_keyframe reads cur->mnId, depth, mvpMapPoints, cam.thDepth). */
static ngd_frame *make_frame(uint64_t id, int N)
{
    init_cam();
    ngd_frame *f = (ngd_frame*)calloc(1, sizeof(ngd_frame));
    f->mnId = id;
    f->N = N;
    f->cam = g_cam;
    f->depth = (float*)malloc(sizeof(float)*(size_t)(N>0?N:1));
    f->mvpMapPoints = (ngd_mappoint**)calloc((size_t)(N>0?N:1), sizeof(ngd_mappoint*));
    f->mvbOutlier = (int*)calloc((size_t)(N>0?N:1), sizeof(int));
    for (int i = 0; i < N; ++i) f->depth[i] = -1;   /* no close points -> bNeedToInsertClose=0 */
    return f;
}
static void free_frame(ngd_frame *f) { free(f->depth); free(f->mvpMapPoints); free(f->mvbOutlier); free(f); }

int main(void)
{
    /* map with 5 KFs so nKFs>2 -> nMinObs=3, thRefRatio=0.75 (RGBD). refKF has
     * 200 tracked MPs (nObs=3 each) -> nRefMatches=200. */
    ngd_map map; ngd_map_init(&map);
    for (int i = 0; i < 5; ++i) { ngd_keyframe *k = (ngd_keyframe*)calloc(1,sizeof(ngd_keyframe)); k->mnId=i; ngd_map_add_keyframe(&map, k); }

    ngd_keyframe *refKF = make_refkf(0, 200, 3);   /* 200 tracked MPs */
    ngd_frame *cur = make_frame(0, 50);

    ngd_tracking tk; ngd_tracking_init(&tk);
    tk.current = cur; tk.refKF = refKF; tk.map = &map; tk.sensor = NGD_SENSOR_RGBD;
    tk.mMaxFrames = 30; tk.mMinFrames = 0; tk.mnLastKeyFrameId = 0; tk.mnLastRelocFrameId = 0;

    /* (1) strong tracking (mnMatchesInliers=180 > 200*0.75=150), frame 0, no c1a -> 0 */
    tk.mnMatchesInliers = 180; cur->mnId = 0;
    CHECK(ngd_tracking_need_new_keyframe(&tk) == 0, "strong tracking, frame 0 -> no KF");

    /* (2) weak tracking (50 < 150, >15 -> c2 true) + c1a (frame 30 >= 0+30) -> 1 */
    tk.mnMatchesInliers = 50; cur->mnId = 30;
    CHECK(ngd_tracking_need_new_keyframe(&tk) == 1, "weak tracking + c1a -> need KF");

    /* (3) weak tracking but reloc cooldown (mnId < lastReloc + mMaxFrames, nKFs>2... nKFs=5>mMaxFrames? no, 5<30)
     *     -> use a small mMaxFrames so nKFs>mMaxFrames holds. */
    tk.mMaxFrames = 3; tk.mnLastRelocFrameId = 100; tk.mnLastKeyFrameId = 0;
    tk.mnMatchesInliers = 50; cur->mnId = 101;   /* 101 < 100+3=103, nKFs(5)>3 -> cooldown */
    CHECK(ngd_tracking_need_new_keyframe(&tk) == 0, "reloc cooldown -> no KF");

    /* (4) past cooldown but strong tracking -> c2 false -> 0 */
    tk.mnMatchesInliers = 180; cur->mnId = 200;   /* 200 >= 100+3, c1a true, but c2 false */
    CHECK(ngd_tracking_need_new_keyframe(&tk) == 0, "c1a true but strong tracking (c2 false) -> no KF");

    /* (5) weak + c1a past cooldown -> 1 */
    tk.mnMatchesInliers = 50; cur->mnId = 200;
    CHECK(ngd_tracking_need_new_keyframe(&tk) == 1, "weak + c1a past cooldown -> need KF");

    /* cleanup */
    free_frame(cur);
    free_refkf(refKF);
    for (int i = 0; i < map.nKFs; ++i) free(map.kfs[i]);
    ngd_map_free(&map);
    ngd_tracking_destroy(&tk);

    if (fails == 0) { printf("test_needkf: PASS\n"); return 0; }
    printf("test_needkf: %d FAILURES\n", fails);
    return 1;
}
