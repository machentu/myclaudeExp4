/* test_frustum.c — validate Frame::isInFrustum + the update_normal_and_depth fix. */
#include "ngd/orb.h"
#include "ngd/calib.h"
#include "ngd/frame.h"
#include "ngd/keyframe.h"
#include "ngd/mappoint.h"
#include "ngd/se3.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { if(!(cond)){ printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); fails++; } } while(0)
#define APPROX(a,b,eps) (fabsf((a)-(b)) <= (eps))

#define W 640
#define H 480

int main(void)
{
    ngd_orb_extractor ex;
    ngd_orb_init(&ex, 1000, 1.2f, 8, 20, 7);
    ngd_cam_ctx cam;
    ngd_cam_ctx_init(&cam, &ex, 535.4f, 539.2f, 320.1f, 247.6f, 0.0747f, 2.988f, W, H);

    /* mfLogScaleFactor must use level-1 (= log(1.2)), not level-0 (log(1)=0). */
    CHECK(APPROX(cam.mfLogScaleFactor, logf(1.2f), 1e-4f), "mfLogScaleFactor == log(scaleFactor)");

    /* ====================================================================
     * Part A: update_normal_and_depth FIX — bounds use the refKF observation's
     * octave, not the max level. (MapPoint.cc:476-499)
     * ==================================================================== */
    {
        /* one keypoint at octave 2 -> level=2 in the bounds formula. */
        ngd_keypoint kps[1];
        memset(kps, 0, sizeof(kps));
        kps[0].x = 320.0f; kps[0].y = 240.0f; kps[0].octave = 2; kps[0].angle = 0.0f;
        uint8_t desc[32]; memset(desc, 0, sizeof(desc));
        float uR[1] = { -1.0f }; float dep[1] = { 1.0f };
        ngd_keyframe *kf = ngd_keyframe_new(0, ngd_se3_identity(), &cam, 1, kps, desc, uR, dep);

        float pos[3] = { 0.0f, 0.0f, 3.0f };   /* dist = 3 from origin KF */
        ngd_mappoint *mp = ngd_mappoint_new(pos, kf);
        ngd_mappoint_add_observation(mp, kf, 0, -1);   /* leftIdx=0 -> keys[0].octave=2 */
        ngd_mappoint_update_normal_and_depth(mp);

        float dist = 3.0f;
        float sf2 = cam.mvScaleFactor[2];            /* 1.2^2 = 1.44 */
        float sftop = cam.mvScaleFactor[7];           /* 1.2^7 = 3.583 */
        CHECK(APPROX(mp->mfMaxDistance, dist * sf2, 1e-3f), "mfMaxDistance = dist*sf[level] (fix)");
        CHECK(APPROX(mp->mfMinDistance, mp->mfMaxDistance / sftop, 1e-3f), "mfMinDistance = max/sf[top]");
        /* The OLD (buggy) code would have given max = dist*sftop = 10.75, not 4.32. */
        CHECK(!APPROX(mp->mfMaxDistance, dist * sftop, 0.05f), "max != buggy dist*sf[top]");
        /* normal = normalize(Pos - Ow) = (0,0,1). */
        CHECK(APPROX(mp->normal[0], 0.0f, 1e-4f) && APPROX(mp->normal[1], 0.0f, 1e-4f) &&
              APPROX(mp->normal[2], 1.0f, 1e-4f), "normal == normalize(Pos-Ow)");

        ngd_mappoint_free(mp);
        ngd_keyframe_free(kf);
    }

    /* ====================================================================
     * Part B: isInFrustum gate cases (fields set directly so each gate is
     * exercised independently). MP at (0,0,1), bounds [0.4, 2.4] (minD=0.5,
     * maxD=2.0 -> invariance [0.8*0.5, 1.2*2.0] = [0.4, 2.4]).
     * ==================================================================== */
    float mbf = cam.mbf;
    float pos_front[3] = { 0.0f, 0.0f, 1.0f };

    ngd_frame f; memset(&f, 0, sizeof(f));
    f.cam = cam;
    f.pose = ngd_se3_identity();   /* Ow = origin */

    /* helper lambda-ish: build a fresh MP at worldPos with given normal/bounds. */
    #define MAKE_MP(mp, P, NX,NY,NZ, MAXD, MIND) do { \
        float _p[3] = {(P)[0],(P)[1],(P)[2]}; \
        (mp) = ngd_mappoint_new(_p, NULL); \
        (mp)->normal[0]=(NX); (mp)->normal[1]=(NY); (mp)->normal[2]=(NZ); \
        (mp)->mfMaxDistance=(MAXD); (mp)->mfMinDistance=(MIND); \
    } while(0)

    /* Case 1: front, fully valid -> 1, all fields set. */
    {
        ngd_mappoint *mp; MAKE_MP(mp, pos_front, 0.0f,0.0f,1.0f, 2.0f, 0.5f);
        int r = ngd_frame_is_in_frustum(&f, mp, 0.5f);
        CHECK(r == 1, "case1 front valid -> visible");
        CHECK(mp->mbTrackInView == 1, "case1 mbTrackInView set");
        CHECK(APPROX(mp->mTrackProjX, 320.1f, 1e-2f), "case1 mTrackProjX");
        CHECK(APPROX(mp->mTrackProjY, 247.6f, 1e-2f), "case1 mTrackProjY");
        CHECK(APPROX(mp->mTrackProjXR, 320.1f - mbf, 1e-1f), "case1 mTrackProjXR = u - mbf/z");
        CHECK(APPROX(mp->mTrackDepth, 1.0f, 1e-4f), "case1 mTrackDepth = |Pc|");
        CHECK(APPROX(mp->mTrackProjZ, 1.0f, 1e-4f), "case1 mTrackProjZ = PcZ");
        CHECK(APPROX(mp->mTrackViewCos, 1.0f, 1e-4f), "case1 mTrackViewCos = 1");
        /* predict_scale direction: ratio = mfMaxDistance/dist = 2/1 = 2;
         * ceil(log(2)/log(1.2)) = ceil(3.80) = 4. Wrong direction would clamp to 0. */
        CHECK(mp->mnTrackScaleLevel == 4, "case1 predict_scale uses mfMaxDistance/dist (level=4)");
        ngd_mappoint_free(mp);
    }

    /* Case 2: behind camera (PcZ<0) -> 0. 180° about Y. */
    {
        ngd_frame f2; memset(&f2, 0, sizeof(f2)); f2.cam = cam;
        ngd_mat3 R = ngd_m3_identity(); R.m[0] = -1.0f; R.m[8] = -1.0f;   /* diag(-1,1,-1) */
        f2.pose = ngd_se3_from_rt(R, ngd_v3(0,0,0));
        ngd_mappoint *mp; MAKE_MP(mp, pos_front, 0.0f,0.0f,1.0f, 2.0f, 0.5f);
        CHECK(ngd_frame_is_in_frustum(&f2, mp, 0.5f) == 0, "case2 behind (z<0) -> 0");
        ngd_mappoint_free(mp);
    }

    /* Case 3: projection out of image bounds -> 0. MP far in +x. */
    {
        ngd_mappoint *mp; float p[3]={5.0f,0.0f,1.0f};
        MAKE_MP(mp, p, 1.0f,0.0f,0.0f, 2.0f, 0.5f);
        CHECK(ngd_frame_is_in_frustum(&f, mp, 0.5f) == 0, "case3 out-of-bounds -> 0");
        ngd_mappoint_free(mp);
    }

    /* Case 4: viewing angle >60° (viewCos<0.5) -> 0. Normal set sideways (1,0,0)
     * while MP sits on +z; PO=(0,0,1), viewCos=(0,0,1)·(1,0,0)=0. */
    {
        ngd_mappoint *mp; MAKE_MP(mp, pos_front, 1.0f,0.0f,0.0f, 2.0f, 0.5f);
        CHECK(ngd_frame_is_in_frustum(&f, mp, 0.5f) == 0, "case4 view-angle>60 -> 0");
        ngd_mappoint_free(mp);
    }

    /* Case 5: distance > maxD_invariance -> 0. MP at z=5, maxD invariance=2.4. */
    {
        ngd_mappoint *mp; float p[3]={0.0f,0.0f,5.0f};
        MAKE_MP(mp, p, 0.0f,0.0f,1.0f, 2.0f, 0.5f);
        CHECK(ngd_frame_is_in_frustum(&f, mp, 0.5f) == 0, "case5 too far -> 0");
        ngd_mappoint_free(mp);
    }

    /* Case 6: distance < minD_invariance -> 0. MP at z=0.1, minD invariance=0.4. */
    {
        ngd_mappoint *mp; float p[3]={0.0f,0.0f,0.1f};
        MAKE_MP(mp, p, 0.0f,0.0f,1.0f, 2.0f, 0.5f);
        CHECK(ngd_frame_is_in_frustum(&f, mp, 0.5f) == 0, "case6 too close -> 0");
        ngd_mappoint_free(mp);
    }

    /* Case 7: viewingCosLimit=0.5 boundary — exactly 0.5 should pass (>=0.5).
     * Construct viewCos ~0.5+epsilon: normal tilted so cos ~0.6 -> visible. */
    {
        ngd_mappoint *mp; MAKE_MP(mp, pos_front, 0.6f,0.0f,0.8f, 2.0f, 0.5f); /* |n|=1 */
        /* PO=(0,0,1); viewCos = (0,0,1)·(0.6,0,0.8) = 0.8 > 0.5 -> visible */
        CHECK(ngd_frame_is_in_frustum(&f, mp, 0.5f) == 1, "case7 viewCos=0.8 -> visible");
        ngd_mappoint_free(mp);
    }

    #undef MAKE_MP

    if (fails == 0) { printf("test_frustum: PASS\n"); return 0; }
    printf("test_frustum: %d FAILURES\n", fails);
    return 1;
}
