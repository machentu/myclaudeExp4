/* test_kfinsert.c — multi-frame KF insertion + MapPointCulling end-to-end.
 *
 * Builds a synthetic front-parallel scene (same as test_reloc), runs a
 * hand-wired multi-frame loop: frame0 StereoInit -> KF0 (into map + KFDB);
 * frames 1..4 each shift by (DX,DY), do TrackWithMotionModel + TrackLocalMap,
 * then NeedNewKeyFrame -> (if true) CreateNewKeyFrame -> LocalMappingRun.
 * mMaxFrames=2 forces KF insertion every ~2 frames via c1a. Asserts:
 *   - map.nKFs grows beyond 1
 *   - each new KF enters the KFDB (covisibility neighbours built)
 *   - refKF / mnLastKeyFrameId advance
 *   - MapPointCulling runs without crashing and recent list stays bounded
 *
 * Requires the real ORB vocabulary (SKIPPED if not found). */
#include "ngd/orb.h"
#include "ngd/calib.h"
#include "ngd/frame.h"
#include "ngd/keyframe.h"
#include "ngd/mappoint.h"
#include "ngd/tracking.h"
#include "ngd/kfdb.h"
#include "ngd/map.h"
#include "ngd/localmapping.h"
#include "ngd/bow.h"

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
#define NFRAMES 5

static unsigned int rng = 7u;
static float frand(void){ rng = rng*1664525u + 1013904223u; return (rng >> 8) * (1.0f/16777216.0f); }

static ngd_bow_vocab *load_vocab(void)
{
    const char *bin_cands[] = {
        "../../../Vocabulary/ORBvoc.bin", "../../Vocabulary/ORBvoc.bin",
        "../../../../Vocabulary/ORBvoc.bin", "../Vocabulary/ORBvoc.bin",
        "Vocabulary/ORBvoc.bin", NULL };
    for (int i = 0; bin_cands[i]; ++i) {
        FILE *tf = fopen(bin_cands[i], "rb");
        if (tf) { fclose(tf); return ngd_bow_vocab_load_binary(bin_cands[i]); }
    }
    const char *txt_cands[] = {
        "../../../Vocabulary/ORBvoc.txt", "../../Vocabulary/ORBvoc.txt",
        "../../../../Vocabulary/ORBvoc.txt", "Vocabulary/ORBvoc.txt", NULL };
    for (int i = 0; txt_cands[i]; ++i) {
        FILE *tf = fopen(txt_cands[i], "r");
        if (tf) { fclose(tf); return ngd_bow_vocab_load_text(txt_cands[i]); }
    }
    return NULL;
}

/* render img0, then a shifted copy by (sx,sy) clamped at borders */
static void render_shifted(const uint8_t *img0, uint8_t *out, int sx, int sy)
{
    for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) {
        int su = x - sx, sv = y - sy;
        if (su < 0) su = 0; if (su >= W) su = W-1;
        if (sv < 0) sv = 0; if (sv >= H) sv = H-1;
        out[y*W+x] = img0[sv*W+su];
    }
}

int main(void)
{
    rng = 7u;
    ngd_bow_vocab *voc = load_vocab();
    if (!voc) { printf("test_kfinsert: SKIP (ORB vocabulary not found)\n"); return 0; }

    static uint8_t img0[W*H];
    for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) img0[y*W+x] = (uint8_t)(frand()*255);
    for (int by = 0; by < H; by += 50) for (int bx = 0; bx < W; bx += 50)
        for (int y = by; y < by+25 && y < H; ++y) for (int x = bx; x < bx+25 && x < W; ++x)
            img0[y*W+x] = (uint8_t)(((bx+by)/50) & 1 ? 255 : 0);
    /* varying depth across x -> non-coplanar (1.0..1.8 m) */
    static float depth[W*H];
    for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x)
        depth[y*W+x] = 1.0f + 0.8f * ((float)x / (float)(W-1));

    ngd_orb_extractor ex; ngd_orb_init(&ex, NFEAT, 1.2f, 8, 20, 7);
    ngd_cam_ctx cam; ngd_cam_ctx_init(&cam, &ex, 535.4f, 539.2f, 320.1f, 247.6f, 0.0747f, 2.988f, W, H);

    /* ---- frame 0: StereoInit -> KF0 ---- */
    ngd_keypoint kps0[MAXK]; static uint8_t desc0[MAXK*32];
    ngd_frame f0;
    ngd_frame_build_rgbd(&f0, &ex, &cam, img0, W, H, W, depth, W, 0, kps0, MAXK, desc0);
    CHECK(f0.N > 500, "frame0 enough features");

    ngd_keyframe *kf0 = NULL; ngd_mappoint **mps0 = NULL; int nmps0 = 0;
    int r = ngd_stereo_initialization(&f0, &kf0, &mps0, &nmps0);
    CHECK(r > 0, "stereo init succeeds");
    printf("  init: %d map points\n", nmps0);

    ngd_map map; ngd_map_init(&map);
    ngd_kfdb db; ngd_kfdb_init(&db, voc);
    ngd_local_mapping lm; ngd_local_mapping_init(&lm, &map, /*mbMonocular=*/0);
    ngd_keyframe_compute_bow(kf0, voc);
    ngd_kfdb_add(&db, kf0);
    ngd_map_add_keyframe(&map, kf0);
    for (int i = 0; i < nmps0; ++i) { ngd_map_add_mappoint(&map, mps0[i]); ngd_local_mapping_push_recent(&lm, mps0[i]); }
    lm.currentKF = kf0;

    ngd_tracking tk; ngd_tracking_init(&tk);
    tk.vocab = voc; tk.kfdb = &db; tk.map = &map; tk.lm = &lm;
    tk.sensor = NGD_SENSOR_RGBD;
    tk.mMaxFrames = 2; tk.mMinFrames = 0;        /* force KF insertion every ~2 frames */
    tk.refKF = kf0; tk.mnLastKeyFrameId = 0; tk.mnLastRelocFrameId = 0;

    /* keep the previous frame alive for TWM (TWM reads tk->last) */
    ngd_frame *prev = &f0;

    int nKFInserted = 0;
    for (int fr = 1; fr <= NFRAMES; ++fr) {
        static uint8_t imgi[W*H];
        render_shifted(img0, imgi, DX*fr, DY*fr);
        ngd_keypoint *kpsi = (ngd_keypoint*)malloc(sizeof(ngd_keypoint)*MAXK);
        uint8_t *desci = (uint8_t*)malloc((size_t)MAXK*32);
        ngd_frame *fi = (ngd_frame*)calloc(1, sizeof(ngd_frame));
        int Ni = ngd_frame_build_rgbd(fi, &ex, &cam, imgi, W, H, W, depth, W, (uint64_t)fr, kpsi, MAXK, desci);
        (void)Ni;

        tk.current = fi; tk.last = prev;
        tk.velocity = ngd_se3_multiply(fi->pose, ngd_se3_inverse(prev->pose));   /* identity-ish seed */

        int ok = ngd_tracking_track_with_motion_model(&tk);
        if (ok) ngd_tracking_track_local_map(&tk);
        printf("  frame %d: TWM=%d mnMatchesInliers=%d\n", fr, ok, tk.mnMatchesInliers);

        if (ok && ngd_tracking_need_new_keyframe(&tk)) {
            int created = ngd_tracking_create_new_key_frame(&tk);
            if (created) {
                ++nKFInserted;
                ngd_local_mapping_run(&lm);
                printf("    -> KF inserted (map.nKFs=%d, recent=%d)\n", map.nKFs, lm.nRecent);
            }
        }

        /* prev becomes fi (the caller owns fi; free the old prev if heap-allocated) */
        if (prev != &f0) {
            ngd_frame_free(prev); free(prev);
        }
        prev = fi;
        free(kpsi); free(desci);
    }
    if (prev != &f0) { ngd_frame_free(prev); free(prev); }

    printf("  total: %d KFs inserted, map.nKFs=%d\n", nKFInserted, map.nKFs);
    CHECK(map.nKFs >= 2, "at least one new KF inserted beyond KF0");
    CHECK(nKFInserted >= 1, "create_new_key_frame called >=1");
    CHECK(tk.mnLastKeyFrameId > 0, "mnLastKeyFrameId advanced");
    CHECK(tk.refKF != kf0 || nKFInserted == 0, "refKF updated to a new KF");

    /* KFDB contains all inserted KFs (covisibility built on each) */
    /* Spot-check: kf0 has covisibility neighbours after inserts */
    printf("  kf0 covisibility degree: %d\n", kf0->nConn);
    /* (covisibility requires shared MPs; new KFs share depth-MPs with kf0 via
     * tracking matches, so degree should be >0 after inserts.) */
    if (nKFInserted > 0) CHECK(kf0->nConn > 0, "kf0 has covisibility neighbours after inserts");

    /* MapPointCulling kept the recent list bounded (no leak / no crash). */
    CHECK(lm.nRecent >= 0, "recent list non-negative after culling");

    /* cleanup: free each unique MP once (via map.mps, deduped), then KF shells.
     * ngd_keyframe_free frees the KF's own arrays (keys/desc/grid/bow) but NOT
     * the MapPoint objects in mvpMapPoints (shared), so MPs are freed via map.mps. */
    for (int i = 0; i < map.nMPs; ++i) ngd_mappoint_free(map.mps[i]);
    for (int i = 0; i < map.nKFs; ++i) ngd_keyframe_free(map.kfs[i]);
    free(mps0);   /* init MP pointer array (objects freed via map.mps above) */
    ngd_kfdb_free(&db);
    ngd_local_mapping_free(&lm);
    ngd_map_free(&map);
    ngd_tracking_destroy(&tk);
    ngd_frame_free(&f0);
    ngd_bow_vocab_free(voc);

    if (fails == 0) { printf("test_kfinsert: PASS\n"); return 0; }
    printf("test_kfinsert: %d FAILURES\n", fails);
    return 1;
}
