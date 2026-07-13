/* test_trackrefkf.c — TrackReferenceKeyFrame end-to-end on a synthetic
 * front-parallel scene (same scene as test_motion).
 *
 * frame1's image is frame0 shifted by integer (DX,DY) -> corresponding patches
 * yield IDENTICAL ORB descriptors (Hamming 0). SearchByBoW therefore matches
 * frame1 features to the reference KF's MapPoints by descriptor identity (no
 * projection-radius ambiguity), then PoseOptimization recovers the pure-
 * translation pose t = (DX*Z/fx, DY*Z/fy, 0) = (0.00934, 0.00556, 0).
 *
 * Requires the real ORB vocabulary (Vocabulary/ORBvoc.bin or .txt) because the
 * tiny 4-word vocab cannot separate ~1000 random descriptors. If the vocabulary
 * file is not found the test is SKIPPED (returns 0), since it is environment-
 * dependent. */
#include "ngd/orb.h"
#include "ngd/calib.h"
#include "ngd/frame.h"
#include "ngd/keyframe.h"
#include "ngd/mappoint.h"
#include "ngd/tracking.h"
#include "ngd/bow.h"
#include "ngd/pinhole.h"
#include "ngd/se3.h"

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

/* Try to load the ORB vocabulary from a list of candidate paths. Returns NULL
 * if none could be opened (test will be skipped). */
static ngd_bow_vocab *load_vocab(void)
{
    const char *bin_cands[] = {
        "../../../Vocabulary/ORBvoc.bin",
        "../../Vocabulary/ORBvoc.bin",
        "../../../../Vocabulary/ORBvoc.bin",
        "../Vocabulary/ORBvoc.bin",
        "Vocabulary/ORBvoc.bin",
        NULL
    };
    for (int i = 0; bin_cands[i]; ++i) {
        FILE *tf = fopen(bin_cands[i], "rb");
        if (tf) {
            fclose(tf);
            ngd_bow_vocab *v = ngd_bow_vocab_load_binary(bin_cands[i]);
            if (v) { printf("  vocab: loaded %s (%u nodes, %u words)\n", bin_cands[i], v->n_nodes, v->n_words); return v; }
        }
    }
    const char *txt_cands[] = {
        "../../../Vocabulary/ORBvoc.txt",
        "../../Vocabulary/ORBvoc.txt",
        "../../../../Vocabulary/ORBvoc.txt",
        "Vocabulary/ORBvoc.txt",
        NULL
    };
    for (int i = 0; txt_cands[i]; ++i) {
        FILE *tf = fopen(txt_cands[i], "r");
        if (tf) {
            fclose(tf);
            ngd_bow_vocab *v = ngd_bow_vocab_load_text(txt_cands[i]);
            if (v) { printf("  vocab: loaded %s (%u nodes, %u words)\n", txt_cands[i], v->n_nodes, v->n_words); return v; }
        }
    }
    return NULL;
}

int main(void)
{
    rng = 7u;
    ngd_bow_vocab *voc = load_vocab();
    if (!voc) {
        printf("test_trackrefkf: SKIP (ORB vocabulary not found)\n");
        return 0;
    }

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
    int N0 = ngd_frame_build_rgbd(&f0, &ex, &cam, img0, W, H, W, depth, W, 0, kps0, MAXK, desc0);
    int N1 = ngd_frame_build_rgbd(&f1, &ex, &cam, img1, W, H, W, depth, W, 1, kps1, MAXK, desc1);
    printf("  frame0 N=%d, frame1 N=%d\n", N0, N1);
    CHECK(N0 > 500, "frame0 enough features");

    ngd_keyframe *kf0 = NULL; ngd_mappoint **mps = NULL; int nmps = 0;
    int r = ngd_stereo_initialization(&f0, &kf0, &mps, &nmps);
    CHECK(r > 0, "stereo init succeeds");
    printf("  init: %d map points\n", nmps);

    ngd_tracking tk; ngd_tracking_init(&tk);
    tk.current = &f1; tk.last = &f0; tk.refKF = kf0; tk.vocab = voc; tk.sensor = NGD_SENSOR_RGBD;
    int ok = ngd_tracking_track_reference_keyframe(&tk);
    CHECK(ok == 1, "TrackReferenceKeyFrame returns success (nmatchesMap>=10)");

    int ninl = 0; for (int i = 0; i < f1.N; ++i) if (f1.mvpMapPoints[i] && !f1.mvbOutlier[i]) ninl++;
    printf("  inlier matches: %d\n", ninl);
    CHECK(ninl >= 10, "enough inlier matches");

    /* Reprojection RMS of inliers. BoW matches are descriptor-exact
     * correspondences, so residuals should be small. */
    double sumsq = 0; int cnt = 0;
    for (int i = 0; i < f1.N; ++i) {
        ngd_mappoint *mp = f1.mvpMapPoints[i];
        if (!mp || f1.mvbOutlier[i]) continue;
        ngd_vec3 Pw = ngd_v3(mp->worldPos[0], mp->worldPos[1], mp->worldPos[2]);
        ngd_vec3 Pc = ngd_se3_map(f1.pose, Pw);
        if (Pc.z <= 0) continue;
        float uv[2]; ngd_pinhole_project(f1.cam.cam, Pc, uv);
        float du = uv[0] - f1.keys[i].x, dv = uv[1] - f1.keys[i].y;
        sumsq += (double)(du*du + dv*dv); cnt++;
    }
    double rms = cnt ? sqrt(sumsq / cnt) : 999.0;
    printf("  reprojection RMS: %.4f px over %d inliers\n", rms, cnt);
    CHECK(rms < 5.0, "RMS bounded (no divergence)");

    ngd_vec3 t = f1.pose.t;
    double w = f1.pose.q.w; if (w > 1.0) w = 1.0; if (w < -1.0) w = -1.0;
    double rangle = 2.0 * acos(w);
    printf("  pose: t=(%.5f,%.5f,%.5f) rot=%.5f rad (truth t~(0.00934,0.00556,0))\n", t.x, t.y, t.z, rangle);
    CHECK(fabs(t.x - 0.00934) < 1e-2, "tx near DX*Z/fx");
    CHECK(fabs(t.y - 0.00556) < 1e-2, "ty near DY*Z/fy");
    CHECK(fabs(t.z) < 5e-3, "tz ~0 (stereo constrains z)");
    CHECK(rangle < 1e-2, "rotation ~0");
    CHECK(fabs(t.x) + fabs(t.y) + rangle > 1e-4, "pose moved from identity");

    for (int i = 0; i < nmps; ++i) ngd_mappoint_free(mps[i]);
    free(mps);
    ngd_keyframe_free(kf0);
    ngd_frame_free(&f0); ngd_frame_free(&f1);
    ngd_tracking_destroy(&tk);
    ngd_bow_vocab_free(voc);

    if (fails == 0) { printf("test_trackrefkf: PASS\n"); return 0; }
    printf("test_trackrefkf: %d FAILURES\n", fails);
    return 1;
}
