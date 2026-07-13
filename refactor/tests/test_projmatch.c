/* test_projmatch.c — validate SearchByProjection overload #1 + #2 + orientation pruning. */
#include "ngd/orb.h"
#include "ngd/calib.h"
#include "ngd/frame.h"
#include "ngd/keyframe.h"
#include "ngd/mappoint.h"
#include "ngd/matcher.h"
#include "ngd/se3.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { if(!(cond)){ printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); fails++; } } while(0)

#define W 640
#define H 480

/* ---- frame builder: deep-copies caller arrays, assigns the 64x48 grid. ---- */
static ngd_frame *make_frame(const ngd_cam_ctx *cam, ngd_se3 pose, int N,
                             const ngd_keypoint *keys, const uint8_t *descs,
                             const float *uRight, const float *depth)
{
    ngd_frame *f = (ngd_frame*)calloc(1, sizeof(ngd_frame));
    f->cam = *cam; f->pose = pose; f->N = N;
    f->keys = (ngd_keypoint*)malloc(sizeof(ngd_keypoint)*(size_t)N); memcpy(f->keys, keys, sizeof(ngd_keypoint)*(size_t)N);
    f->descriptors = (uint8_t*)malloc((size_t)N*32); memcpy(f->descriptors, descs, (size_t)N*32);
    f->uRight = (float*)malloc(sizeof(float)*(size_t)N); memcpy(f->uRight, uRight, sizeof(float)*(size_t)N);
    f->depth  = (float*)malloc(sizeof(float)*(size_t)N); memcpy(f->depth,  depth,  sizeof(float)*(size_t)N);
    f->mvpMapPoints = (ngd_mappoint**)calloc((size_t)N, sizeof(void*));
    f->mvbOutlier   = (int*)calloc((size_t)N, sizeof(int));
    f->gridInvW = (float)NGD_KF_GRID_COLS / (float)cam->imgW;
    f->gridInvH = (float)NGD_KF_GRID_ROWS / (float)cam->imgH;
    ngd_frame_assign_grid(f);
    return f;
}
static void free_frame(ngd_frame *f){ ngd_frame_free(f); free(f); }

/* descriptor with exactly n bits set (byte0/byte1 packed). */
static void desc_nbits(uint8_t d[32], int n) {
    memset(d, 0, 32);
    int b = 0;
    while (n >= 8) { d[b++] = 0xFF; n -= 8; }
    if (n > 0) d[b] = (uint8_t)((1u << n) - 1u);
}

int main(void)
{
    ngd_orb_extractor ex; ngd_orb_init(&ex, 1000, 1.2f, 8, 20, 7);
    ngd_cam_ctx cam; ngd_cam_ctx_init(&cam, &ex, 535.4f, 539.2f, 320.1f, 247.6f, 0.0747f, 2.988f, W, H);

    uint8_t D0[32];       memset(D0, 0, 32);             /* Hamming 0 from itself */
    uint8_t D10[32];      desc_nbits(D10, 10);            /* Hamming 10 from D0 */
    uint8_t D11[32];      desc_nbits(D11, 11);            /* Hamming 11 from D0 */
    uint8_t Dfar[32];     memset(Dfar, 0xFF, 32);         /* Hamming 256 from D0 */

    CHECK(ngd_descriptor_distance(D0, D10) == 10, "D0-D10 Hamming=10");
    CHECK(ngd_descriptor_distance(D0, D11) == 11, "D0-D11 Hamming=11");
    CHECK(ngd_descriptor_distance(D0, Dfar) == 256, "D0-Dfar Hamming=256");

    /* ================= overload #1 (search_by_projection_local) ================= */
    /* MP at (0,0,1) projects to (320.1, 247.6) under identity; bounds set so
     * dist=1 is valid and PredictScale -> 0 (so octave window [-1,0] matches
     * our octave-0 test features). mTrackProjXR unused (uRight=-1 -> skip). */
    {
        ngd_keypoint kps[2];
        memset(kps, 0, sizeof(kps));
        kps[0].x = 320.0f; kps[0].y = 248.0f; kps[0].octave = 0;
        kps[1].x = 322.0f; kps[1].y = 250.0f; kps[1].octave = 0;
        uint8_t descs[2*32];
        memcpy(descs+0*32, D10, 32);    /* idx0: D10 */
        memcpy(descs+1*32, D11, 32);    /* idx1: D11 */
        float uR[2] = { -1.0f, -1.0f }, dep[2] = { -1.0f, -1.0f };
        ngd_frame *F = make_frame(&cam, ngd_se3_identity(), 2, kps, descs, uR, dep);

        /* Case A: mp desc=D10 matches idx0 (dist 0). */
        float pos[3] = { 0,0,1 }; ngd_mappoint *mpA = ngd_mappoint_new(pos, NULL);
        memcpy(mpA->descriptor, D10, 32);
        mpA->normal[0]=0; mpA->normal[1]=0; mpA->normal[2]=1;
        mpA->mfMaxDistance = 1.0f; mpA->mfMinDistance = 1.0f / cam.mvScaleFactor[7];
        CHECK(ngd_frame_is_in_frustum(F, mpA, 0.5f) == 1, "#1 mpA in frustum");
        ngd_mappoint *mpsA[1] = { mpA };
        int nA = ngd_search_by_projection_local(F, mpsA, 1, 3.0f, 0, 50.0f, 0.8f);
        CHECK(nA == 1, "#1 case A: D10 mp matches idx0");
        CHECK(F->mvpMapPoints[0] == mpA, "#1 case A: mvpMapPoints[0]=mpA");
        ngd_mappoint_free(mpA);

        /* Case B: mp desc=D0, both features D10/D11 -> bestDist=10 (>... but <=100);
         * ratio: 10 > 0.8*11=8.8, same level -> pruned. nmatches=0. */
        ngd_mappoint *mpB = ngd_mappoint_new(pos, NULL);
        memcpy(mpB->descriptor, D0, 32);
        mpB->normal[0]=0; mpB->normal[1]=0; mpB->normal[2]=1;
        mpB->mfMaxDistance = 1.0f; mpB->mfMinDistance = 1.0f / cam.mvScaleFactor[7];
        CHECK(ngd_frame_is_in_frustum(F, mpB, 0.5f) == 1, "#1 mpB in frustum");
        ngd_mappoint *mpsB[1] = { mpB };
        int nB = ngd_search_by_projection_local(F, mpsB, 1, 3.0f, 0, 50.0f, 0.8f);
        CHECK(nB == 0, "#1 case B: ratio test prunes (10 > 0.8*11)");
        ngd_mappoint_free(mpB);
        free_frame(F);

        /* Case C: mp desc=D0 vs a single Dfar feature (Hamming 256 > TH_HIGH) -> no match. */
        ngd_keypoint kc[1]; memset(kc, 0, sizeof(kc));
        kc[0].x = 320.0f; kc[0].y = 248.0f; kc[0].octave = 0;
        uint8_t dc[32]; memcpy(dc, Dfar, 32);
        float uRc[1]={-1}, dc_d[1]={-1};
        ngd_frame *Fc = make_frame(&cam, ngd_se3_identity(), 1, kc, dc, uRc, dc_d);
        ngd_mappoint *mpC = ngd_mappoint_new(pos, NULL);
        memcpy(mpC->descriptor, D0, 32);
        mpC->normal[0]=0; mpC->normal[1]=0; mpC->normal[2]=1;
        mpC->mfMaxDistance = 1.0f; mpC->mfMinDistance = 1.0f / cam.mvScaleFactor[7];
        ngd_frame_is_in_frustum(Fc, mpC, 0.5f);
        ngd_mappoint *mpsC[1] = { mpC };
        int nC = ngd_search_by_projection_local(Fc, mpsC, 1, 3.0f, 0, 50.0f, 0.8f);
        CHECK(nC == 0, "#1 case C: Hamming>100 -> no match");
        ngd_mappoint_free(mpC);
        free_frame(Fc);
    }

    /* ================= overload #2 (search_by_projection_motion) ================= */
    /* identity poses -> each MP at worldPos=((u-cx)/fx,(v-cy)/fy,1) projects to (u,v).
     * radius = th*mvScaleFactor[0] = 15px; features 40px apart -> 1 candidate each. */
    {
        /* Sub-test A: 1 pair, identity, same descriptor -> matched. */
        float u=320.0f, v=248.0f;
        float Xw = (u-320.1f)/535.4f, Yw = (v-247.6f)/539.2f;
        float posA[3] = { Xw, Yw, 1.0f };
        ngd_mappoint *mpA = ngd_mappoint_new(posA, NULL);
        memcpy(mpA->descriptor, D0, 32);

        ngd_keypoint kL[1]; memset(kL,0,sizeof(kL)); kL[0].x=u; kL[0].y=v; kL[0].octave=0; kL[0].angle=0;
        ngd_keypoint kC[1]; memset(kC,0,sizeof(kC)); kC[0].x=u; kC[0].y=v; kC[0].octave=0; kC[0].angle=0;
        uint8_t dL[32]; memcpy(dL, D0, 32);
        uint8_t dC[32]; memcpy(dC, D0, 32);
        float uRm[1]={-1}, dpm[1]={-1};
        ngd_frame *last = make_frame(&cam, ngd_se3_identity(), 1, kL, dL, uRm, dpm);
        ngd_frame *cur  = make_frame(&cam, ngd_se3_identity(), 1, kC, dC, uRm, dpm);
        last->mvpMapPoints[0] = mpA;
        int nA = ngd_search_by_projection_motion(cur, last, 15.0f, 0, 1);
        CHECK(nA == 1, "#2 A: identity pair matches");
        CHECK(cur->mvpMapPoints[0] == mpA, "#2 A: cur.mvpMapPoints[0]=mpA");
        free_frame(last); free_frame(cur); ngd_mappoint_free(mpA);

        /* Sub-test B: last-frame outlier -> skipped. */
        ngd_mappoint *mpB = ngd_mappoint_new(posA, NULL); memcpy(mpB->descriptor, D0, 32);
        ngd_frame *lastB = make_frame(&cam, ngd_se3_identity(), 1, kL, dL, uRm, dpm);
        ngd_frame *curB  = make_frame(&cam, ngd_se3_identity(), 1, kC, dC, uRm, dpm);
        lastB->mvpMapPoints[0] = mpB; lastB->mvbOutlier[0] = 1;
        int nB = ngd_search_by_projection_motion(curB, lastB, 15.0f, 0, 1);
        CHECK(nB == 0, "#2 B: outlier skipped");
        free_frame(lastB); free_frame(curB); ngd_mappoint_free(mpB);

        /* Sub-test C: orientation histogram pruning. 20 pairs angle-diff 0 (bin 0)
         * + 1 pair angle-diff 180 (bin 6). ComputeThreeMaxima: max1=20, max2=1,
         * 0.1*20=2 > 1 -> keep only bin 0; the bin-6 match is pruned. nmatches=20. */
        #define NP 21
        ngd_keypoint kL2[NP], kC2[NP];
        uint8_t dL2[NP*32], dC2[NP*32];
        float uR2[NP], dp2[NP];
        ngd_mappoint *mpsC[NP];
        memset(dL2, 0, sizeof(dL2)); memset(dC2, 0, sizeof(dC2)); /* all-zero descriptors */
        for (int i = 0; i < NP; ++i) {
            uR2[i] = -1.0f; dp2[i] = -1.0f;
            int col = i % 7, row = i / 7;
            float uu = 60.0f + 40.0f*col, vv = 60.0f + 40.0f*row;   /* 40px apart > 15px radius */
            float p[3] = { (uu-320.1f)/535.4f, (vv-247.6f)/539.2f, 1.0f };
            mpsC[i] = ngd_mappoint_new(p, NULL);
            memset(mpsC[i]->descriptor, 0, 32);
            memset(&kL2[i],0,sizeof(kL2[i])); kL2[i].x=uu; kL2[i].y=vv; kL2[i].octave=0;
            memset(&kC2[i],0,sizeof(kC2[i])); kC2[i].x=uu; kC2[i].y=vv; kC2[i].octave=0;
            kL2[i].angle = 0.0f;
            kC2[i].angle = (i < NP-1) ? 0.0f : 180.0f;   /* last pair: diff 180 -> bin 6 */
        }
        ngd_frame *lastC = make_frame(&cam, ngd_se3_identity(), NP, kL2, dL2, uR2, dp2);
        ngd_frame *curC  = make_frame(&cam, ngd_se3_identity(), NP, kC2, dC2, uR2, dp2);
        for (int i = 0; i < NP; ++i) lastC->mvpMapPoints[i] = mpsC[i];
        int nC = ngd_search_by_projection_motion(curC, lastC, 15.0f, 0, 1);
        CHECK(nC == 20, "#2 C: orientation pruning keeps 20, drops bin-6");
        CHECK(curC->mvpMapPoints[NP-1] == NULL, "#2 C: bin-6 match pruned (NULL)");
        int kept = 0; for (int i = 0; i < NP-1; ++i) if (curC->mvpMapPoints[i]) kept++;
        CHECK(kept == 20, "#2 C: all 20 bin-0 matches retained");
        free_frame(lastC); free_frame(curC);
        for (int i = 0; i < NP; ++i) ngd_mappoint_free(mpsC[i]);
        #undef NP
    }

    if (fails == 0) { printf("test_projmatch: PASS\n"); return 0; }
    printf("test_projmatch: %d FAILURES\n", fails);
    return 1;
}
