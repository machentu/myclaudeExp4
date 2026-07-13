/* test_searchbow.c — validate ngd_search_by_bow (FeatureVector two-pointer merge,
 * same-node brute Hamming, TH_LOW gate, ratio test, node-grouping pruning).
 * Uses a tiny k=2 L=2 vocab; FeatureVectors are populated with levelsup=1 so
 * features group by the level-1 node (node1=zeros-side, node2=ones-side).
 * Orientation-histogram pruning is the same ComputeThreeMaxima path already
 * covered by test_projmatch, so here we only verify checkOri runs cleanly. */
#include "ngd/orb.h"
#include "ngd/calib.h"
#include "ngd/frame.h"
#include "ngd/keyframe.h"
#include "ngd/mappoint.h"
#include "ngd/matcher.h"
#include "ngd/bow.h"
#include "ngd/se3.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int fails = 0;
#define CHECK(cond, msg) do { if(!(cond)){ printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); fails++; } } while(0)

#define W 640
#define H 480

static const char *VOC_TXT =
    "2 2 0 0\n"
    "0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
    "0 0 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 0\n"
    "1 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1\n"
    "1 1 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 2.5\n"
    "2 1 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 1\n"
    "2 1 254 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 2.5\n";

static void desc_zero(uint8_t d[32]) { memset(d, 0, 32); }
static void desc_ones(uint8_t d[32]) { memset(d, 0xFF, 32); }
/* byte0=0x01, rest 0 -> Hamming 1 from zeros -> node1, word1 */
static void desc_near_zero(uint8_t d[32]) { memset(d, 0, 32); d[0] = 0x01; }
/* far from zeros (256 bits) but we only use it as a non-matching distractor */
static void desc_far(uint8_t d[32]) { memset(d, 0xFF, 32); d[0] = 0x7F; }

/* Populate bow/feat with a given levelsup (compute_bow hardcodes 4; for the
 * tiny L=2 vocab we use levelsup=1 to get meaningful level-1 node grouping). */
static void populate_bow(ngd_bow_vocab *v, const uint8_t *descs, int N,
                         int levelsup, ngd_bowvec **bow, ngd_featvec **feat)
{
    *bow = (ngd_bowvec*)calloc(1, sizeof(ngd_bowvec));
    *feat = (ngd_featvec*)calloc(1, sizeof(ngd_featvec));
    ngd_bow_transform(v, descs, N, levelsup, *bow, *feat);
}

int main(void)
{
    FILE *f = fopen("tiny_voc_sb.txt", "w");
    CHECK(f != NULL, "open tiny_voc_sb.txt");
    if (!f) { printf("test_searchbow: FAIL (temp)\n"); return 1; }
    fputs(VOC_TXT, f); fclose(f);
    ngd_bow_vocab *v = ngd_bow_vocab_load_text("tiny_voc_sb.txt");
    CHECK(v != NULL, "load vocab");
    if (!v) { printf("test_searchbow: FAIL (load)\n"); return 1; }

    ngd_orb_extractor ex; ngd_orb_init(&ex, 1000, 1.2f, 8, 20, 7);
    ngd_cam_ctx cam; ngd_cam_ctx_init(&cam, &ex, 535.4f, 539.2f, 320.1f, 247.6f, 0.0747f, 2.988f, W, H);

    /* ---- Case A: 2 clean matches across two nodes ----
     * KF: feat0=zeros(node1,mp0), feat1=ones(node2,mp1)
     * F : feat0=zeros(node1),     feat1=ones(node2)  -> 2 matches */
    {
        ngd_keypoint kk[2]; memset(kk, 0, sizeof(kk));
        kk[0].x=100; kk[0].y=100; kk[0].octave=0; kk[0].angle=0;
        kk[1].x=400; kk[1].y=400; kk[1].octave=0; kk[1].angle=0;
        uint8_t kd[2*32]; desc_zero(kd+0*32); desc_ones(kd+1*32);
        float ur[2]={-1,-1}, dp[2]={-1,-1};
        ngd_keyframe *kf = ngd_keyframe_new(0, ngd_se3_identity(), &cam, 2, kk, kd, ur, dp);
        ngd_mappoint *mp0 = ngd_mappoint_new((float[3]){0,0,1}, kf);
        ngd_mappoint *mp1 = ngd_mappoint_new((float[3]){1,0,1}, kf);
        kf->mvpMapPoints[0] = mp0; kf->mvpMapPoints[1] = mp1;
        populate_bow(v, kf->descriptors, kf->N, 1, &kf->bow, &kf->feat);

        ngd_keypoint fk[2]; memset(fk, 0, sizeof(fk));
        fk[0].x=110; fk[0].y=110; fk[0].octave=0; fk[0].angle=0;
        fk[1].x=410; fk[1].y=410; fk[1].octave=0; fk[1].angle=0;
        uint8_t fd[2*32]; desc_zero(fd+0*32); desc_ones(fd+1*32);
        ngd_frame *F = (ngd_frame*)calloc(1, sizeof(ngd_frame));
        F->cam=cam; F->N=2; F->pose=ngd_se3_identity();
        F->keys=malloc(sizeof(ngd_keypoint)*2); memcpy(F->keys, fk, sizeof(fk));
        F->descriptors=malloc(2*32); memcpy(F->descriptors, fd, 2*32);
        F->uRight=malloc(sizeof(float)*2); F->depth=malloc(sizeof(float)*2);
        F->uRight[0]=-1; F->uRight[1]=-1; F->depth[0]=-1; F->depth[1]=-1;
        F->mvpMapPoints=calloc(2, sizeof(void*)); F->mvbOutlier=calloc(2, sizeof(int));
        populate_bow(v, F->descriptors, F->N, 1, &F->bow, &F->feat);

        int n = ngd_search_by_bow(kf, F, F->mvpMapPoints, 0.7f, 1);
        CHECK(n == 2, "case A: 2 clean matches");
        CHECK(F->mvpMapPoints[0] == mp0, "case A: F[0]<-mp0");
        CHECK(F->mvpMapPoints[1] == mp1, "case A: F[1]<-mp1");

        ngd_keyframe_free(kf);
        ngd_mappoint_free(mp0); ngd_mappoint_free(mp1);
        /* frame free: bow/feat are ours, free manually then frame_free */
        ngd_bowvec_free(F->bow); free(F->bow); ngd_featvec_free(F->feat); free(F->feat);
        F->bow=NULL; F->feat=NULL;
        ngd_frame_free(F); free(F);
    }

    /* ---- Case B: ratio pruning. node1 has KF0(zeros,mp) and Frame feats
     * F0=zeros (dist0), F1=near_zero (dist1). best1=0,best2=1 -> 0<0.7 -> match F0.
     * Then add F2=zeros (dist0) too: now best1=0,best2=0 -> 0<0 false -> REJECT. ---- */
    {
        ngd_keypoint kk[1]; memset(kk,0,sizeof(kk)); kk[0].x=100; kk[0].y=100; kk[0].octave=0;
        uint8_t kd[32]; desc_zero(kd);
        float ur[1]={-1}, dp[1]={-1};
        ngd_keyframe *kf = ngd_keyframe_new(0, ngd_se3_identity(), &cam, 1, kk, kd, ur, dp);
        ngd_mappoint *mp0 = ngd_mappoint_new((float[3]){0,0,1}, kf);
        kf->mvpMapPoints[0] = mp0;
        populate_bow(v, kf->descriptors, 1, 1, &kf->bow, &kf->feat);

        /* B1: F0=zeros(0), F1=near_zero(1) -> best1=0 best2=1 -> match */
        {
            ngd_keypoint fk[2]; memset(fk,0,sizeof(fk));
            fk[0].x=100; fk[0].y=100; fk[0].octave=0; fk[0].angle=0;
            fk[1].x=200; fk[1].y=200; fk[1].octave=0; fk[1].angle=0;
            uint8_t fd[2*32]; desc_zero(fd+0*32); desc_near_zero(fd+1*32);
            ngd_frame *F = (ngd_frame*)calloc(1, sizeof(ngd_frame));
            F->cam=cam; F->N=2; F->pose=ngd_se3_identity();
            F->keys=malloc(sizeof(ngd_keypoint)*2); memcpy(F->keys, fk, sizeof(fk));
            F->descriptors=malloc(2*32); memcpy(F->descriptors, fd, 2*32);
            F->uRight=malloc(sizeof(float)*2); F->depth=malloc(sizeof(float)*2);
            F->uRight[0]=-1; F->uRight[1]=-1; F->depth[0]=-1; F->depth[1]=-1;
            F->mvpMapPoints=calloc(2,sizeof(void*)); F->mvbOutlier=calloc(2,sizeof(int));
            populate_bow(v, F->descriptors, 2, 1, &F->bow, &F->feat);
            int n = ngd_search_by_bow(kf, F, F->mvpMapPoints, 0.7f, 0);
            CHECK(n == 1, "case B1: best1=0 best2=1 -> ratio passes");
            CHECK(F->mvpMapPoints[0] == mp0, "case B1: F[0]<-mp0");
            ngd_bowvec_free(F->bow); free(F->bow); ngd_featvec_free(F->feat); free(F->feat);
            F->bow=NULL; F->feat=NULL; ngd_frame_free(F); free(F);
        }
        /* B2: F0=zeros(0), F1=zeros(0) -> best1=0 best2=0 -> 0<0 false -> reject */
        {
            ngd_keypoint fk[2]; memset(fk,0,sizeof(fk));
            fk[0].x=100; fk[0].y=100; fk[0].octave=0; fk[0].angle=0;
            fk[1].x=200; fk[1].y=200; fk[1].octave=0; fk[1].angle=0;
            uint8_t fd[2*32]; desc_zero(fd+0*32); desc_zero(fd+1*32);
            ngd_frame *F = (ngd_frame*)calloc(1, sizeof(ngd_frame));
            F->cam=cam; F->N=2; F->pose=ngd_se3_identity();
            F->keys=malloc(sizeof(ngd_keypoint)*2); memcpy(F->keys, fk, sizeof(fk));
            F->descriptors=malloc(2*32); memcpy(F->descriptors, fd, 2*32);
            F->uRight=malloc(sizeof(float)*2); F->depth=malloc(sizeof(float)*2);
            F->uRight[0]=-1; F->uRight[1]=-1; F->depth[0]=-1; F->depth[1]=-1;
            F->mvpMapPoints=calloc(2,sizeof(void*)); F->mvbOutlier=calloc(2,sizeof(int));
            populate_bow(v, F->descriptors, 2, 1, &F->bow, &F->feat);
            int n = ngd_search_by_bow(kf, F, F->mvpMapPoints, 0.7f, 0);
            CHECK(n == 0, "case B2: best1=best2=0 -> ratio rejects");
            ngd_bowvec_free(F->bow); free(F->bow); ngd_featvec_free(F->feat); free(F->feat);
            F->bow=NULL; F->feat=NULL; ngd_frame_free(F); free(F);
        }

        ngd_keyframe_free(kf); ngd_mappoint_free(mp0);
    }

    /* ---- Case C: TH_LOW rejection. KF0=zeros(mp), Frame F0=desc_far
     * (Hamming 1 from ones -> node2, NOT node1). Different node -> no match.
     * Add F1=ones(node2) but KF0 is node1 -> still no match (node grouping). ---- */
    {
        ngd_keypoint kk[1]; memset(kk,0,sizeof(kk)); kk[0].x=100; kk[0].y=100; kk[0].octave=0;
        uint8_t kd[32]; desc_zero(kd);
        float ur[1]={-1}, dp[1]={-1};
        ngd_keyframe *kf = ngd_keyframe_new(0, ngd_se3_identity(), &cam, 1, kk, kd, ur, dp);
        ngd_mappoint *mp0 = ngd_mappoint_new((float[3]){0,0,1}, kf);
        kf->mvpMapPoints[0] = mp0;
        populate_bow(v, kf->descriptors, 1, 1, &kf->bow, &kf->feat);

        ngd_keypoint fk[1]; memset(fk,0,sizeof(fk)); fk[0].x=400; fk[0].y=400; fk[0].octave=0;
        uint8_t fd[32]; desc_ones(fd);   /* node2, not node1 */
        ngd_frame *F = (ngd_frame*)calloc(1, sizeof(ngd_frame));
        F->cam=cam; F->N=1; F->pose=ngd_se3_identity();
        F->keys=malloc(sizeof(ngd_keypoint)); memcpy(F->keys, fk, sizeof(fk));
        F->descriptors=malloc(32); memcpy(F->descriptors, fd, 32);
        F->uRight=malloc(sizeof(float)); F->depth=malloc(sizeof(float));
        F->uRight[0]=-1; F->depth[0]=-1;
        F->mvpMapPoints=calloc(1,sizeof(void*)); F->mvbOutlier=calloc(1,sizeof(int));
        populate_bow(v, F->descriptors, 1, 1, &F->bow, &F->feat);
        int n = ngd_search_by_bow(kf, F, F->mvpMapPoints, 0.7f, 0);
        CHECK(n == 0, "case C: node-grouping prevents cross-node match");
        ngd_bowvec_free(F->bow); free(F->bow); ngd_featvec_free(F->feat); free(F->feat);
        F->bow=NULL; F->feat=NULL; ngd_frame_free(F); free(F);

        ngd_keyframe_free(kf); ngd_mappoint_free(mp0);
    }

    (void)desc_far;
    ngd_bow_vocab_free(v);
    remove("tiny_voc_sb.txt");

    if (fails == 0) { printf("test_searchbow: PASS\n"); return 0; }
    printf("test_searchbow: %d FAILURES\n", fails);
    return 1;
}
