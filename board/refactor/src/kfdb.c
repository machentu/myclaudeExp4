/* ngd/kfdb.c — KeyFrameDatabase inverted index (pure C).
 * See ngd/kfdb.h for scope. Reference: src/KeyFrameDatabase.cc. */
#include "ngd/kfdb.h"
#include "ngd/keyframe.h"
#include "ngd/frame.h"
#include "ngd/bow.h"

#include <stdlib.h>
#include <string.h>

void ngd_kfdb_init(ngd_kfdb *db, const ngd_bow_vocab *vocab)
{
    db->vocab = vocab;
    db->nWords = (int)vocab->n_words;
    db->invFile = (ngd_keyframe***)calloc((size_t)(db->nWords > 0 ? db->nWords : 1), sizeof(ngd_keyframe**));
    db->invN    = (int*)calloc((size_t)(db->nWords > 0 ? db->nWords : 1), sizeof(int));
    db->invCap  = (int*)calloc((size_t)(db->nWords > 0 ? db->nWords : 1), sizeof(int));
}

void ngd_kfdb_free(ngd_kfdb *db)
{
    if (!db->invFile) return;
    for (int w = 0; w < db->nWords; ++w) free(db->invFile[w]);
    free(db->invFile); free(db->invN); free(db->invCap);
    db->invFile = NULL; db->invN = NULL; db->invCap = NULL;
    db->nWords = 0;
}

static void inv_push(ngd_kfdb *db, int w, ngd_keyframe *kf)
{
    if (db->invN[w] == db->invCap[w]) {
        int nc = db->invCap[w] ? db->invCap[w] * 2 : 4;
        db->invFile[w] = (ngd_keyframe**)realloc(db->invFile[w], (size_t)nc * sizeof(ngd_keyframe*));
        db->invCap[w] = nc;
    }
    db->invFile[w][db->invN[w]++] = kf;
}

void ngd_kfdb_add(ngd_kfdb *db, ngd_keyframe *kf)
{
    if (!kf->bow) return;   /* ComputeBoW must have run */
    for (int i = 0; i < kf->bow->n; ++i) {
        uint32_t wid = kf->bow->items[i].id;
        if (wid < (uint32_t)db->nWords) inv_push(db, (int)wid, kf);
    }
}

void ngd_kfdb_erase(ngd_kfdb *db, ngd_keyframe *kf)
{
    if (!kf->bow) return;
    for (int i = 0; i < kf->bow->n; ++i) {
        uint32_t wid = kf->bow->items[i].id;
        if (wid >= (uint32_t)db->nWords) continue;
        int n = db->invN[wid];
        ngd_keyframe **lst = db->invFile[wid];
        for (int k = 0; k < n; ++k) {
            if (lst[k] == kf) {
                lst[k] = lst[n - 1];   /* swap-pop (order irrelevant) */
                db->invN[wid] = n - 1;
                break;
            }
        }
    }
}

/* ------------------------------------------------------------------ *
 * DetectRelocalizationCandidates (KeyFrameDatabase.cc:733-845).
 * Single-map: GetMap() filter (cc:834) skipped.                       */

int ngd_kfdb_detect_reloc_candidates(ngd_kfdb *db, ngd_frame *F,
                                     ngd_keyframe ***out, int *nOut)
{
    *out = NULL; *nOut = 0;
    if (!F->bow) return 0;

    const uint64_t qid = F->mnId;

    /* ---- lKFsSharingWords: KFs sharing >=1 word with F (cc:735-757) ---- */
    int capS = 64, nS = 0;
    ngd_keyframe **share = (ngd_keyframe**)malloc((size_t)capS * sizeof(ngd_keyframe*));
    if (!share) return -1;

    for (int i = 0; i < F->bow->n; ++i) {
        uint32_t wid = F->bow->items[i].id;
        if (wid >= (uint32_t)db->nWords) continue;
        int n = db->invN[wid];
        ngd_keyframe **lst = db->invFile[wid];
        for (int k = 0; k < n; ++k) {
            ngd_keyframe *pKFi = lst[k];
            if (pKFi->mnRelocQuery != qid) {
                pKFi->mnRelocQuery = qid;
                pKFi->mnRelocWords = 0;
                if (nS == capS) {
                    capS *= 2;
                    share = (ngd_keyframe**)realloc(share, (size_t)capS * sizeof(ngd_keyframe*));
                    if (!share) return -1;
                }
                share[nS++] = pKFi;
            }
            pKFi->mnRelocWords++;
        }
    }
    if (nS == 0) { free(share); return 0; }

    /* ---- maxCommonWords / minCommonWords = 0.8*max (cc:762-769) ---- */
    int maxCommonWords = 0;
    for (int k = 0; k < nS; ++k)
        if (share[k]->mnRelocWords > maxCommonWords) maxCommonWords = share[k]->mnRelocWords;
    int minCommonWords = (int)(maxCommonWords * 0.8f);

    /* ---- lScoreAndMatch: score the KFs with enough shared words (cc:776-787) ---- */
    int capM = 64, nM = 0;
    ngd_keyframe **scored = (ngd_keyframe**)malloc((size_t)capM * sizeof(ngd_keyframe*));
    if (!scored) { free(share); return -1; }
    for (int k = 0; k < nS; ++k) {
        ngd_keyframe *pKFi = share[k];
        if (pKFi->mnRelocWords > minCommonWords) {
            float si = (float)ngd_bow_score_l1(F->bow, pKFi->bow);
            pKFi->mRelocScore = si;
            if (nM == capM) {
                capM *= 2;
                scored = (ngd_keyframe**)realloc(scored, (size_t)capM * sizeof(ngd_keyframe*));
                if (!scored) { free(share); return -1; }
            }
            scored[nM++] = pKFi;
        }
    }
    free(share);
    if (nM == 0) { free(scored); return 0; }

    /* ---- accumulate score by covisibility (cc:796-821) ----
     * For each scored KF: take its best-10 covisibility neighbours, accumulate
     * mRelocScore of those that also share words with F (mnRelocQuery==qid),
     * and track the best-scoring KF in the group (pBestKF). */
    float bestAccScore = 0.0f;
    /* For each scored KF we record (accScore, pBestKF). */
    float *accArr = (float*)malloc((size_t)nM * sizeof(float));
    ngd_keyframe **bestArr = (ngd_keyframe**)malloc((size_t)nM * sizeof(ngd_keyframe*));
    if (!accArr || !bestArr) { free(scored); free(accArr); free(bestArr); return -1; }

    ngd_keyframe *neighBuf[10];
    for (int i = 0; i < nM; ++i) {
        ngd_keyframe *pKFi = scored[i];
        int nn = ngd_keyframe_get_best_covisibility_keyframes(pKFi, 10, neighBuf);
        float bestScore = pKFi->mRelocScore;
        float accScore  = bestScore;
        ngd_keyframe *pBestKF = pKFi;
        for (int j = 0; j < nn; ++j) {
            ngd_keyframe *pKF2 = neighBuf[j];
            if (pKF2->mnRelocQuery != qid) continue;
            accScore += pKF2->mRelocScore;
            if (pKF2->mRelocScore > bestScore) {
                pBestKF = pKF2;
                bestScore = pKF2->mRelocScore;
            }
        }
        accArr[i] = accScore;
        bestArr[i] = pBestKF;
        if (accScore > bestAccScore) bestAccScore = accScore;
    }
    free(scored);

    /* ---- retain accScore > 0.75*bestAccScore (cc:824-842) ----
     * GetMap() filter skipped (single map). Dedup on pBestKF. */
    float minScoreToRetain = 0.75f * bestAccScore;
    int capR = 16, nR = 0;
    ngd_keyframe **ret = (ngd_keyframe**)malloc((size_t)capR * sizeof(ngd_keyframe*));
    if (!ret) { free(accArr); free(bestArr); return -1; }
    for (int i = 0; i < nM; ++i) {
        if (accArr[i] > minScoreToRetain) {
            ngd_keyframe *pKFi = bestArr[i];
            int dup = 0;
            for (int k = 0; k < nR; ++k) if (ret[k] == pKFi) { dup = 1; break; }
            if (!dup) {
                if (nR == capR) {
                    capR *= 2;
                    ret = (ngd_keyframe**)realloc(ret, (size_t)capR * sizeof(ngd_keyframe*));
                    if (!ret) { free(accArr); free(bestArr); return -1; }
                }
                ret[nR++] = pKFi;
            }
        }
    }
    free(accArr); free(bestArr);

    *out = ret; *nOut = nR;
    return 0;
}
