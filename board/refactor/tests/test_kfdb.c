/* test_kfdb.c — KeyFrameDatabase inverted index + DetectRelocalizationCandidates.
 *
 * Builds three minimal KeyFrames with hand-set, L1-normalized BowVectors (no
 * vocabulary file needed: a dummy ngd_bow_vocab with only n_words set suffices,
 * since KFDB only indexes by WordId). Wires a covisibility graph by hand, then
 * checks the candidate-detection funnel:
 *   shared-word dedup -> maxCommonWords/0.8 filter -> L1 score ->
 *   covisibility accumulation -> 0.75*bestAcc retain.
 *
 * Vocabulary layout (NW=100):
 *   kf0: words 1..10 (0.1 each)            -- 10 shared with F
 *   kf1: words 1..9, 11   (0.1 each)       --  9 shared with F
 *   kf2: words 1..6, 13..16 (0.1 each)     --  6 shared with F
 *   F  : words 1..10 (0.1 each)
 * maxCommonWords=10, minCommonWords=8 -> scored: kf0(10), kf1(9); kf2(6) excluded.
 * score(F,kf0)=1.0, score(F,kf1)=0.9.
 * covisibility: kf0<->kf1 weight 30, kf0<->kf2 weight 5.
 * accumulation: kf0 group accScore=1.0+0.9(kf1's stale-free mRelocScore)=1.9,
 *   bestAcc=1.9, retain>0.75*1.9=1.425 -> kf0 returned. */
#include "ngd/kfdb.h"
#include "ngd/keyframe.h"
#include "ngd/bow.h"
#include "ngd/frame.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int fails = 0;
#define CHECK(cond, msg) do { if(!(cond)){ printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); fails++; } } while(0)

#define NW 100

static ngd_bowvec *make_bowvec(const uint32_t *ids, int n)
{
    ngd_bowvec *bv = (ngd_bowvec*)calloc(1, sizeof(ngd_bowvec));
    bv->n = n; bv->cap = n;
    bv->items = (ngd_bow_pair*)malloc((size_t)n * sizeof(ngd_bow_pair));
    double w = 1.0 / (double)n;   /* equal weights -> L1 norm = 1 */
    for (int i = 0; i < n; ++i) { bv->items[i].id = ids[i]; bv->items[i].value = w; }
    return bv;
}

/* minimal KF: calloc + hand-set bow. ngd_keyframe_free tolerates NULL fields. */
static ngd_keyframe *make_min_kf(uint64_t id, const uint32_t *ids, int n)
{
    ngd_keyframe *kf = (ngd_keyframe*)calloc(1, sizeof(ngd_keyframe));
    kf->mnId = id;
    kf->bow = make_bowvec(ids, n);
    kf->N = 0;
    return kf;
}

/* minimal frame: only mnId + bow used by DetectRelocalizationCandidates. */
static ngd_frame *make_min_frame(uint64_t id, const uint32_t *ids, int n)
{
    ngd_frame *f = (ngd_frame*)calloc(1, sizeof(ngd_frame));
    f->mnId = id;
    f->bow = make_bowvec(ids, n);
    f->N = 0;
    return f;
}

static void free_min_kf(ngd_keyframe *kf) { ngd_keyframe_free(kf); }  /* frees bow + conn */
static void free_min_frame(ngd_frame *f) { if (f->bow) { ngd_bowvec_free(f->bow); free(f->bow); } free(f); }

int main(void)
{
    /* dummy vocab: only n_words matters for KFDB sizing */
    ngd_bow_vocab voc; memset(&voc, 0, sizeof(voc)); voc.n_words = NW;

    uint32_t ids_kf0[] = {1,2,3,4,5,6,7,8,9,10};
    uint32_t ids_kf1[] = {1,2,3,4,5,6,7,8,9,11};
    uint32_t ids_kf2[] = {1,2,3,4,5,6,13,14,15,16};
    uint32_t ids_F[]   = {1,2,3,4,5,6,7,8,9,10};

    ngd_keyframe *kf0 = make_min_kf(10, ids_kf0, 10);
    ngd_keyframe *kf1 = make_min_kf(11, ids_kf1, 10);
    ngd_keyframe *kf2 = make_min_kf(12, ids_kf2, 10);
    ngd_frame    *F   = make_min_frame(100, ids_F, 10);

    /* covisibility: kf0<->kf1 (30), kf0<->kf2 (5) */
    ngd_keyframe_add_connection(kf0, kf1, 30); ngd_keyframe_add_connection(kf1, kf0, 30);
    ngd_keyframe_add_connection(kf0, kf2, 5);  ngd_keyframe_add_connection(kf2, kf0, 5);
    CHECK(ngd_keyframe_get_weight(kf0, kf1) == 30, "cov weight kf0-kf1");
    CHECK(ngd_keyframe_get_weight(kf1, kf0) == 30, "cov weight symmetric");

    ngd_kfdb db; ngd_kfdb_init(&db, &voc);
    ngd_kfdb_add(&db, kf0);
    ngd_kfdb_add(&db, kf1);
    ngd_kfdb_add(&db, kf2);

    /* sanity: word 1 lists all three KFs */
    int wn0 = db.invN[1];
    printf("  invFile[1] size = %d (expect 3)\n", wn0);
    CHECK(wn0 == 3, "word 1 in all 3 KFs");

    ngd_keyframe **cands = NULL; int nCands = 0;
    int rc = ngd_kfdb_detect_reloc_candidates(&db, F, &cands, &nCands);
    CHECK(rc == 0, "detect returns 0");
    printf("  reloc candidates: %d\n", nCands);
    CHECK(nCands >= 1, "at least one candidate");

    /* kf0 must be present (best-scoring group representative); kf2 must not */
    int has_kf0 = 0, has_kf2 = 0;
    for (int i = 0; i < nCands; ++i) {
        if (cands[i] == kf0) has_kf0 = 1;
        if (cands[i] == kf2) has_kf2 = 1;
    }
    CHECK(has_kf0, "kf0 retained (best score group)");
    CHECK(!has_kf2, "kf2 excluded (6 shared <= minCommonWords 8)");

    /* reloc scratch: each KF got mnRelocQuery stamped to F->mnId, mnRelocWords set */
    CHECK(kf0->mnRelocQuery == F->mnId, "kf0 reloc query stamped");
    CHECK(kf0->mnRelocWords == 10, "kf0 reloc words = 10");
    CHECK(kf2->mnRelocWords == 6, "kf2 reloc words = 6");
    CHECK(fabs(kf0->mRelocScore - 1.0) < 1e-6, "kf0 score = 1.0");
    CHECK(fabs(kf1->mRelocScore - 0.9) < 1e-6, "kf1 score = 0.9");

    free(cands);
    ngd_kfdb_free(&db);
    free_min_kf(kf0); free_min_kf(kf1); free_min_kf(kf2);
    free_min_frame(F);

    if (fails == 0) { printf("test_kfdb: PASS\n"); return 0; }
    printf("test_kfdb: %d FAILURES\n", fails);
    return 1;
}
