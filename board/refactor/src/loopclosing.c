/* ngd/loopclosing.c — LoopClosing orchestration (single-map / non-IMU / non-merge).
 * Faithful simplified port of LoopClosing.cc. detect: kfdb-free candidate scan
 * (BoW L1 score) + KF-KF SearchByBoW + Sim3Solver + SearchBySim3 + OptimizeSim3.
 * correct_loop: LoopClosing.cc:972-1226 geometry -> OptimizeEssentialGraph + GBA. */
#include "ngd/loopclosing.h"
#include "ngd/map.h"
#include "ngd/keyframe.h"
#include "ngd/mappoint.h"
#include "ngd/matcher.h"
#include "ngd/bow.h"
#include "ngd/kfdb.h"
#include "ngd/sim3solver.h"
#include "ngd/sim3opt.h"
#include "ngd/essgraph.h"
#include "ngd/ba.h"
#include "ngd/so3.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>

/* thresholds (LoopClosing.cc:584-588 DetectCommonRegionsFromBoW) */
#define LC_N_BOW_MATCHES    20
#define LC_N_BOW_INLIERS    15
#define LC_N_SIM3_INLIERS   20
#define LC_N_PROJ_MATCHES   50

void ngd_loopclosing_init(ngd_loopclosing *lc, ngd_map *map,
                          ngd_kfdb *kfdb, const ngd_bow_vocab *vocab, int bFixScale)
{
    memset(lc, 0, sizeof(*lc));
    lc->map = map; lc->kfdb = kfdb; lc->vocab = vocab; lc->bFixScale = bFixScale;
}

void ngd_loopclosing_free(ngd_loopclosing *lc) {
    free(lc->kfQueue);
    free(lc->loopMatchedMPs);
    memset(lc, 0, sizeof(*lc));
}

void ngd_loopclosing_insert_kf(ngd_loopclosing *lc, ngd_keyframe *kf) {
    if (lc->nQ == lc->capQ) {
        lc->capQ = lc->capQ ? lc->capQ*2 : 16;
        lc->kfQueue = (ngd_keyframe**)realloc(lc->kfQueue, (size_t)lc->capQ*sizeof(ngd_keyframe*));
    }
    lc->kfQueue[lc->nQ++] = kf;
}

/* build Sim3 (current-from-cand) from a Sim3Solver T12 (4x4 [sR|t]; cand->current). */
static ngd_sim3 sim3_from_T12(const float T[16]) {
    ngd_mat3 sR = {{T[0],T[1],T[2],T[4],T[5],T[6],T[8],T[9],T[10]}};
    float sc = sqrtf(sR.m[0]*sR.m[0] + sR.m[3]*sR.m[3] + sR.m[6]*sR.m[6]);
    if (sc < 1e-12f) sc = 1.0f;
    ngd_mat3 R = {{sR.m[0]/sc,sR.m[1]/sc,sR.m[2]/sc,
                   sR.m[3]/sc,sR.m[4]/sc,sR.m[5]/sc,
                   sR.m[6]/sc,sR.m[7]/sc,sR.m[8]/sc}};
    ngd_sim3 S; S.q = ngd_quat_from_matrix(R); S.t = ngd_v3(T[3],T[7],T[11]); S.s = sc;
    return S;
}

static ngd_sim3 se3_as_sim3(ngd_se3 e) { ngd_sim3 s; s.q = e.q; s.t = e.t; s.s = 1.0f; return s; }

/* find currentKF keypoint index that owns mp (via mp->obs), or -1 */
static int cur_kp_of(ngd_mappoint *mp, ngd_keyframe *kf) {
    for (int o = 0; o < mp->nObsRec; ++o) if (mp->obs[o].kf == kf) return mp->obs[o].leftIdx;
    return -1;
}

int ngd_loopclosing_detect_common_regions(ngd_loopclosing *lc, ngd_keyframe *currentKF)
{
    lc->currentKF = currentKF;
    lc->mbLoopDetected = 0;
    if (lc->vocab) ngd_keyframe_compute_bow(currentKF, lc->vocab);

    /* connected KFs of currentKF (skip as candidates) */
    /* (covisibility set: currentKF + its connKFs) */

    double bestScore = 0.0;
    ngd_keyframe *bestCand = NULL;

    for (int i = 0; i < lc->map->nKFs; ++i) {
        ngd_keyframe *cand = lc->map->kfs[i];
        if (!cand || cand == currentKF || ngd_keyframe_is_bad(cand)) continue;
        if (lc->vocab) ngd_keyframe_compute_bow(cand, lc->vocab);
        if (!cand->bow || !currentKF->bow) continue;
        /* skip covisibles (near KFs) */
        int near = 0;
        for (int c = 0; c < currentKF->nConn; ++c) if (currentKF->connKFs[c] == cand) { near = 1; break; }
        if (near) continue;

        double s = ngd_bow_score_l1(currentKF->bow, cand->bow);
        if (s > bestScore) { bestScore = s; bestCand = cand; }
    }
    if (!bestCand || bestScore <= 0.0) return 0;

    ngd_keyframe *cand = bestCand;
    int N = currentKF->N;
    if (N <= 0 || cand->N <= 0) return 0;

    /* ---- KF-KF SearchByBoW(currentKF, cand) -> out[cand_kp] = currentKF MP ---- */
    ngd_mappoint **bowOut = (ngd_mappoint**)calloc((size_t)cand->N, sizeof(ngd_mappoint*));
    int nBow = ngd_search_by_bow_kf(currentKF, cand, bowOut, 0.9f, 1);
    if (nBow < LC_N_BOW_MATCHES) { free(bowOut); return 0; }

    /* ---- build Sim3Solver correspondences (currentKF=KF1, cand=KF2) ---- */
    int cap = nBow;
    ngd_vec3 *X1 = (ngd_vec3*)malloc(sizeof(ngd_vec3)*cap);
    ngd_vec3 *X2 = (ngd_vec3*)malloc(sizeof(ngd_vec3)*cap);
    float *P1 = (float*)malloc(sizeof(float)*2*cap);
    float *P2 = (float*)malloc(sizeof(float)*2*cap);
    float *mE1 = (float*)malloc(sizeof(float)*cap);
    float *mE2 = (float*)malloc(sizeof(float)*cap);
    int nC = 0;
    for (int j = 0; j < cand->N; ++j) {
        ngd_mappoint *mpCur = bowOut[j];        /* currentKF MP matched to cand keypoint j */
        if (!mpCur || mpCur->mbBad) continue;
        ngd_mappoint *mpCand = cand->mvpMapPoints[j];
        if (!mpCand || mpCand->mbBad) continue;
        int curKp = cur_kp_of(mpCur, currentKF);
        if (curKp < 0) continue;
        ngd_vec3 pw1 = ngd_v3(mpCur->worldPos[0], mpCur->worldPos[1], mpCur->worldPos[2]);
        ngd_vec3 pw2 = ngd_v3(mpCand->worldPos[0], mpCand->worldPos[1], mpCand->worldPos[2]);
        ngd_vec3 x1 = ngd_se3_map(currentKF->pose, pw1);   /* currentKF cam frame */
        ngd_vec3 x2 = ngd_se3_map(cand->pose, pw2);        /* cand cam frame */
        if (x1.z <= 1e-3f || x2.z <= 1e-3f) continue;
        if (nC >= cap) break;
        float uv1[2], uv2[2];
        ngd_pinhole_project(currentKF->cam.cam, x1, uv1);
        ngd_pinhole_project(cand->cam.cam, x2, uv2);
        int o1 = currentKF->keys[curKp].octave;
        int o2 = cand->keys[j].octave;
        X1[nC]=x1; X2[nC]=x2;
        P1[2*nC]=uv1[0]; P1[2*nC+1]=uv1[1];
        P2[2*nC]=uv2[0]; P2[2*nC+1]=uv2[1];
        mE1[nC] = 9.210f * currentKF->cam.mvLevelSigma2[o1];
        mE2[nC] = 9.210f * cand->cam.mvLevelSigma2[o2];
        nC++;
    }
    free(bowOut);
    if (nC < LC_N_BOW_INLIERS) { free(X1);free(X2);free(P1);free(P2);free(mE1);free(mE2); return 0; }

    ngd_sim3_solver *slv = ngd_sim3solver_create(X1, X2, P1, P2, mE1, mE2, nC, !lc->bFixScale,
                                                  currentKF->cam.cam, cand->cam.cam,
                                                  nC, NULL, 54321);
    ngd_sim3solver_set_params(slv, 0.99, LC_N_BOW_INLIERS, 300);
    char *inl = (char*)calloc((size_t)nC, 1); int nInl; float T12[16];
    ngd_sim3solver_find(slv, inl, &nInl, T12);
    ngd_sim3solver_destroy(slv);
    free(inl);
    free(X1);free(X2);free(P1);free(P2);free(mE1);free(mE2);
    if (nInl < LC_N_BOW_INLIERS) return 0;

    ngd_sim3 Scm = sim3_from_T12(T12);   /* current-from-cand */

    /* ---- SearchBySim3 to expand matches ---- */
    ngd_mappoint **vm = (ngd_mappoint**)calloc((size_t)N, sizeof(ngd_mappoint*));
    /* seed vm with the BoW-derived currentKF<->cand MP pairs (cur_kp -> cand MP) */
    /* (re-derive compactly: for each cand keypoint j whose currentKF-MP we know) */
    /* Sim3Solver used compact indexing; simpler: re-run via SearchBySim3 from empty */
    int nProj = ngd_search_by_sim3(currentKF, cand, vm, Scm, 3.0f);
    if (nProj < LC_N_PROJ_MATCHES) { free(vm); return 0; }

    /* ---- OptimizeSim3 ---- */
    /* build sim3opt correspondence arrays from vm (cur_kp i -> cand MP) */
    int capE = N;
    float *obs1 = (float*)malloc(sizeof(float)*2*capE);
    float *obs2 = (float*)malloc(sizeof(float)*2*capE);
    ngd_vec3 *Xc1 = (ngd_vec3*)malloc(sizeof(ngd_vec3)*capE);
    ngd_vec3 *Xc2 = (ngd_vec3*)malloc(sizeof(ngd_vec3)*capE);
    float *iS1 = (float*)malloc(sizeof(float)*capE);
    float *iS2 = (float*)malloc(sizeof(float)*capE);
    int nE = 0;
    for (int i = 0; i < N; ++i) {
        ngd_mappoint *mpCand = vm[i];
        if (!mpCand || mpCand->mbBad) continue;
        ngd_mappoint *mpCur = currentKF->mvpMapPoints[i];
        if (!mpCur || mpCur->mbBad) continue;
        int candKp = cur_kp_of(mpCand, cand);
        if (candKp < 0) continue;
        ngd_vec3 x1 = ngd_se3_map(currentKF->pose, ngd_v3(mpCur->worldPos[0],mpCur->worldPos[1],mpCur->worldPos[2]));
        ngd_vec3 x2 = ngd_se3_map(cand->pose, ngd_v3(mpCand->worldPos[0],mpCand->worldPos[1],mpCand->worldPos[2]));
        if (x1.z <= 1e-3f || x2.z <= 1e-3f) continue;
        obs1[2*nE]=currentKF->keys[i].x; obs1[2*nE+1]=currentKF->keys[i].y;
        obs2[2*nE]=cand->keys[candKp].x; obs2[2*nE+1]=cand->keys[candKp].y;
        Xc1[nE]=x1; Xc2[nE]=x2;
        iS1[nE]=currentKF->cam.mvInvLevelSigma2[currentKF->keys[i].octave];
        iS2[nE]=cand->cam.mvInvLevelSigma2[cand->keys[candKp].octave];
        nE++;
    }
    int ok = 0;
    if (nE >= LC_N_SIM3_INLIERS) {
        ngd_sim3_obs so = { nE, obs1, obs2, Xc1, Xc2, iS1, iS2,
                            currentKF->cam.cam, cand->cam.cam,
                            lc->bFixScale ? 7.815f : 5.991f, lc->bFixScale };
        int *outl = (int*)malloc(sizeof(int)*nE);
        int nOpt = ngd_sim3_optimize(&Scm, &so, outl);
        free(outl);
        if (nOpt >= LC_N_SIM3_INLIERS) ok = 1;
    }
    free(obs1);free(obs2);free(Xc1);free(Xc2);free(iS1);free(iS2);

    if (!ok) { free(vm); return 0; }

    /* loopScw (currentKF corrective) = Scm * Smw, Smw = cand pose (s=1) */
    ngd_sim3 Smw = se3_as_sim3(cand->pose);
    lc->loopScw = ngd_sim3_multiply(Scm, Smw);
    lc->loopMatchedKF = cand;
    /* store loopMatchedMPs (cur keypoint -> matched cand MP) */
    if (lc->nCurN != N) {
        free(lc->loopMatchedMPs);
        lc->loopMatchedMPs = (ngd_mappoint**)calloc((size_t)N, sizeof(ngd_mappoint*));
        lc->nCurN = N;
    } else {
        for (int i = 0; i < N; ++i) lc->loopMatchedMPs[i] = NULL;
    }
    for (int i = 0; i < N; ++i) lc->loopMatchedMPs[i] = vm[i];
    free(vm);

    lc->mbLoopDetected = 1;
    lc->mnLastLoopKFid = currentKF->mnId;
    return 1;
}

void ngd_loopclosing_correct_loop(ngd_loopclosing *lc)
{
    ngd_keyframe *cur = lc->currentKF;
    ngd_keyframe *matched = lc->loopMatchedKF;
    ngd_sim3 loopScw = lc->loopScw;
    if (!cur || !matched) return;

    ngd_keyframe_update_connections(cur);

    /* currentConnectedKFs = cur + covisibles */
    int cap = 1 + cur->nConn;
    ngd_keyframe **conn = (ngd_keyframe**)malloc(sizeof(ngd_keyframe*)*cap);
    int nConn = 0;
    conn[nConn++] = cur;
    for (int i = 0; i < cur->nConn; ++i) conn[nConn++] = cur->connKFs[i];

    /* CorrectedSim3 / NonCorrectedSim3 per connected KF */
    ngd_sim3 *corr = (ngd_sim3*)malloc(sizeof(ngd_sim3)*nConn);
    ngd_sim3 *nonc = (ngd_sim3*)malloc(sizeof(ngd_sim3)*nConn);

    ngd_se3 Twc = ngd_se3_inverse(cur->pose);   /* PRE-correction inverse */
    corr[0] = loopScw;                          /* currentKF */
    nonc[0] = se3_as_sim3(cur->pose);
    ngd_se3 corrTcw = ngd_sim3_to_se3(loopScw);
    ngd_keyframe_set_pose(cur, corrTcw);

    for (int k = 1; k < nConn; ++k) {
        ngd_keyframe *kf = conn[k];
        ngd_se3 Tiw = kf->pose;
        ngd_se3 Tic = ngd_se3_multiply(Tiw, Twc);          /* Tiw * Twc */
        ngd_sim3 corrSiw = ngd_sim3_multiply(se3_as_sim3(Tic), loopScw);
        corr[k] = corrSiw;
        ngd_keyframe_set_pose(kf, ngd_sim3_to_se3(corrSiw));
        nonc[k] = se3_as_sim3(Tiw);
    }

    /* correct MapPoints observed by connected KFs */
    for (int k = 0; k < nConn; ++k) {
        ngd_keyframe *kf = conn[k];
        ngd_sim3 cSwi = ngd_sim3_inverse(corr[k]);
        ngd_sim3 Siw = nonc[k];
        for (int idx = 0; idx < kf->N; ++idx) {
            ngd_mappoint *mp = kf->mvpMapPoints[idx];
            if (!mp || mp->mbBad) continue;
            if (mp->mnBALocalForKF == cur->mnId + 1) continue;  /* already corrected (reuse stamp) */
            ngd_vec3 pw = ngd_v3(mp->worldPos[0], mp->worldPos[1], mp->worldPos[2]);
            ngd_vec3 pc = ngd_sim3_map(Siw, pw);
            ngd_vec3 pwn = ngd_sim3_map(cSwi, pc);
            mp->worldPos[0]=pwn.x; mp->worldPos[1]=pwn.y; mp->worldPos[2]=pwn.z;
            mp->mnBALocalForKF = cur->mnId + 1;   /* mark corrected (mnCorrectedByKF stand-in) */
            ngd_mappoint_update_normal_and_depth(mp);
        }
        ngd_keyframe_update_connections(kf);
    }

    /* loop fusion: replace/add currentKF MPs with matched loop MPs */
    for (int i = 0; i < cur->N && i < lc->nCurN; ++i) {
        ngd_mappoint *loopMP = lc->loopMatchedMPs[i];
        if (!loopMP || loopMP->mbBad) continue;
        ngd_mappoint *curMP = cur->mvpMapPoints[i];
        if (curMP && !curMP->mbBad) {
            if (curMP != loopMP) ngd_mappoint_replace(curMP, loopMP);
        } else {
            cur->mvpMapPoints[i] = loopMP;
            ngd_mappoint_add_observation(loopMP, cur, i, -1);
            ngd_mappoint_compute_distinctive_descriptors(loopMP);
        }
    }

    /* SearchAndFuse (simplified): fuse loop MPs into connected KFs */
    int nLoopMP = 0;
    for (int i = 0; i < lc->nCurN; ++i) if (lc->loopMatchedMPs[i] && !lc->loopMatchedMPs[i]->mbBad) nLoopMP++;
    if (nLoopMP > 0) {
        ngd_mappoint **loopMPs = (ngd_mappoint**)malloc(sizeof(ngd_mappoint*)*nLoopMP);
        int m = 0;
        for (int i = 0; i < lc->nCurN; ++i) if (lc->loopMatchedMPs[i] && !lc->loopMatchedMPs[i]->mbBad) loopMPs[m++] = lc->loopMatchedMPs[i];
        for (int k = 0; k < nConn; ++k)
            ngd_fuse(conn[k], loopMPs, nLoopMP, 3.0f);
        free(loopMPs);
    }

    /* build CorrectedSim3 / NonCorrectedSim3 arrays for OptimizeEssentialGraph */
    ngd_kf_sim3 *cArr = (ngd_kf_sim3*)malloc(sizeof(ngd_kf_sim3)*nConn);
    ngd_kf_sim3 *nArr = (ngd_kf_sim3*)malloc(sizeof(ngd_kf_sim3)*nConn);
    for (int k = 0; k < nConn; ++k) { cArr[k].kf = conn[k]; cArr[k].Scw = corr[k];
                                      nArr[k].kf = conn[k]; nArr[k].Scw = nonc[k]; }
    /* one loop connection: cur <-> matched */
    ngd_kf_pair lp = { cur, matched };

    ngd_optimize_essential_graph(lc->map, /*pLoopKF=*/matched, /*pCurKF=*/cur,
                                 cArr, nConn, nArr, nConn, &lp, 1, lc->bFixScale);

    /* GlobalBundleAdjustment (LoopClosing.cc:1212 — small maps) */
    if (lc->map->nKFs < 200)
        ngd_global_ba(lc->map, 10, &cur->cam);

    free(conn); free(corr); free(nonc); free(cArr); free(nArr);
    lc->mbLoopDetected = 0;
}

int ngd_loopclosing_run(ngd_loopclosing *lc)
{
    if (lc->nQ == 0) return 0;
    ngd_keyframe *kf = lc->kfQueue[0];
    for (int i = 1; i < lc->nQ; ++i) lc->kfQueue[i-1] = lc->kfQueue[i];
    lc->nQ--;
    if (ngd_loopclosing_detect_common_regions(lc, kf)) {
        ngd_loopclosing_correct_loop(lc);
        return 1;
    }
    return 0;
}
