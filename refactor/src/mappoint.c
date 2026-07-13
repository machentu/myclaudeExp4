/* ngd/mappoint.c — MapPoint minimal (pure C). */
#include "ngd/mappoint.h"
#include "ngd/keyframe.h"
#include "ngd/calib.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include "ngd/matcher.h"

static uint64_t s_next_id = 1;

ngd_mappoint *ngd_mappoint_new(const float pos[3], struct ngd_keyframe *refKF) {
    ngd_mappoint *mp = (ngd_mappoint*)calloc(1, sizeof(ngd_mappoint));
    mp->mnId = s_next_id++;
    mp->refKF = refKF;
    mp->mnVisible = 1;
    mp->mnFound = 1;
    mp->mbBad = 0;
    mp->mnFirstKFid = refKF ? refKF->mnId : 0;   /* MapPoint ctor: mnFirstKFid = pRefKF->mnId */
    mp->mfMinDistance = 0.0f;
    mp->mfMaxDistance = 0.0f;
    ngd_mappoint_set_world_pos(mp, pos);
    return mp;
}

void ngd_mappoint_free(ngd_mappoint *mp) {
    if (!mp) return;
    free(mp->obs);
    free(mp);
}

void ngd_mappoint_set_world_pos(ngd_mappoint *mp, const float pos[3]) {
    mp->worldPos[0] = pos[0];
    mp->worldPos[1] = pos[1];
    mp->worldPos[2] = pos[2];
}

void ngd_mappoint_add_observation(ngd_mappoint *mp, struct ngd_keyframe *kf, int leftIdx, int rightIdx) {
    if (mp->nObsRec == mp->capObs) {
        mp->capObs = mp->capObs ? mp->capObs*2 : 4;
        mp->obs = (ngd_observation*)realloc(mp->obs, (size_t)mp->capObs * sizeof(ngd_observation));
    }
    mp->obs[mp->nObsRec].kf = kf;
    mp->obs[mp->nObsRec].leftIdx = leftIdx;
    mp->obs[mp->nObsRec].rightIdx = rightIdx;
    mp->nObsRec++;
    /* MapPoint::nObs counts stereo as 2 (valid rightIdx && !dual-cam). */
    mp->nObs += (rightIdx >= 0) ? 2 : 1;
}

int ngd_mappoint_observations(const ngd_mappoint *mp) { return mp->nObs; }

void ngd_mappoint_compute_distinctive_descriptors(ngd_mappoint *mp) {
    /* Pick the observed descriptor whose total Hamming distance to the others
     * is smallest — the "best representative" (matches ComputeDistinctiveDescriptors).
     * With a single observation it is that observation's descriptor. Iterates
     * over observation RECORDS (nObsRec), not the stereo-counted nObs. */
    if (mp->nObsRec == 0) return;
    uint8_t descs[256][32];
    int cnt = 0;
    for (int i = 0; i < mp->nObsRec && cnt < 256; ++i) {
        if (!mp->obs[i].kf || mp->obs[i].leftIdx < 0) continue;
        ngd_keyframe_get_descriptor(mp->obs[i].kf, mp->obs[i].leftIdx, descs[cnt]);
        cnt++;
    }
    if (cnt == 0) return;
    int best = 0, bestDist = INT_MAX;
    for (int i = 0; i < cnt; ++i) {
        int d = 0;
        for (int j = 0; j < cnt; ++j) {
            if (i == j) continue;
            d += ngd_descriptor_distance(descs[i], descs[j]);
        }
        if (d < bestDist) { bestDist = d; best = i; }
    }
    memcpy(mp->descriptor, descs[best], 32);
}

void ngd_mappoint_update_normal_and_depth(ngd_mappoint *mp) {
    /* Mean viewing direction over observing KFs: sum of normalize(Pos - Owi).
     * For a single KF this is normalize(Pos - Ow). Scale invariance bounds are
     * derived from the reference KF distance and the largest scale factor. */
    ngd_vec3 n = {0,0,0};
    int cnt = 0;
    for (int i = 0; i < mp->nObsRec; ++i) {
        if (!mp->obs[i].kf) continue;
        ngd_vec3 ow = ngd_keyframe_get_camera_center(mp->obs[i].kf);
        ngd_vec3 d = ngd_v3_sub(ngd_v3(mp->worldPos[0],mp->worldPos[1],mp->worldPos[2]), ow);
        if (ngd_v3_norm(d) > 1e-6f) { n = ngd_v3_add(n, ngd_v3_normalize(d)); cnt++; }
    }
    if (cnt == 0 && mp->refKF) {
        ngd_vec3 ow = ngd_keyframe_get_camera_center(mp->refKF);
        ngd_vec3 d = ngd_v3_sub(ngd_v3(mp->worldPos[0],mp->worldPos[1],mp->worldPos[2]), ow);
        if (ngd_v3_norm(d) > 1e-6f) { n = ngd_v3_normalize(d); cnt = 1; }
    }
    if (cnt > 0) {
        ngd_vec3 nn = ngd_v3_normalize(n);
        mp->normal[0] = nn.x; mp->normal[1] = nn.y; mp->normal[2] = nn.z;
    }
    if (mp->refKF) {
        ngd_vec3 ow = ngd_keyframe_get_camera_center(mp->refKF);
        ngd_vec3 d = ngd_v3_sub(ngd_v3(mp->worldPos[0],mp->worldPos[1],mp->worldPos[2]), ow);
        float dist = ngd_v3_norm(d);
        /* MapPoint.cc:476-499 — bounds use the refKF observation's OCTAVE, not
         * the max level. Previously this used mvScaleFactor[nlevels-1] for both
         * max and min, which inflated mfMaxDistance by ~mvScaleFactor[top] (≈3.58
         * for 8 levels @1.2) and broke isInFrustum's distance gate. */
        int leftIdx = -1;
        for (int i = 0; i < mp->nObsRec; ++i)
            if (mp->obs[i].kf == mp->refKF && mp->obs[i].leftIdx >= 0) { leftIdx = mp->obs[i].leftIdx; break; }
        if (leftIdx < 0 && mp->nObsRec > 0 && mp->obs[0].leftIdx >= 0) leftIdx = mp->obs[0].leftIdx; /* defensive */
        int nl = mp->refKF->cam.nlevels;
        int level = (leftIdx >= 0 && leftIdx < mp->refKF->N) ? mp->refKF->keys[leftIdx].octave : 0;
        if (level < 0) level = 0;
        if (nl > 0 && level >= nl) level = nl - 1;
        float levelScaleFactor = (nl > 0) ? mp->refKF->cam.mvScaleFactor[level] : 1.0f;
        float smax = (nl > 0) ? mp->refKF->cam.mvScaleFactor[nl-1] : 1.0f;
        if (smax > 0.0f && levelScaleFactor > 0.0f) {
            mp->mfMaxDistance = dist * levelScaleFactor;
            mp->mfMinDistance = mp->mfMaxDistance / smax;
        }
    }
}

float ngd_mappoint_get_max_distance_invariance(const ngd_mappoint *mp) { return 1.2f * mp->mfMaxDistance; }
float ngd_mappoint_get_min_distance_invariance(const ngd_mappoint *mp) { return 0.8f * mp->mfMinDistance; }

int ngd_mappoint_predict_scale(const ngd_mappoint *mp, float currentDist, const ngd_cam_ctx *cam) {
    /* MapPoint.cc:539-554 (Frame overload). ratio = mfMaxDistance / currentDist
     * (note the direction — NOT currentDist / mfMaxDistance). */
    if (mp->mfMaxDistance <= 0.0f || currentDist <= 0.0f || cam->mfLogScaleFactor == 0.0f) return 0;
    float ratio = mp->mfMaxDistance / currentDist;
    if (ratio <= 0.0f) return 0;
    int nScale = (int)ceilf(logf(ratio) / cam->mfLogScaleFactor);
    if (nScale < 0) nScale = 0;
    else if (nScale >= cam->nlevels) nScale = cam->nlevels - 1;
    return nScale;
}

void ngd_mappoint_increase_visible(ngd_mappoint *mp) { mp->mnVisible++; }
void ngd_mappoint_increase_found(ngd_mappoint *mp) { mp->mnFound++; }

float ngd_mappoint_get_found_ratio(const ngd_mappoint *mp) {
    return (mp->mnVisible > 0) ? (float)mp->mnFound / (float)mp->mnVisible : 0.0f;
}

void ngd_mappoint_set_bad(ngd_mappoint *mp) {
    if (!mp || mp->mbBad) return;
    mp->mbBad = 1;
    free(mp->obs);
    mp->obs = NULL;
    mp->nObsRec = 0; mp->nObs = 0; mp->capObs = 0;
}

int ngd_mappoint_is_in_keyframe(const ngd_mappoint *mp, ngd_keyframe *kf) {
    /* MapPoint.cc:428. */
    if (!mp) return 0;
    for (int i = 0; i < mp->nObsRec; ++i)
        if (mp->obs[i].kf == kf) return 1;
    return 0;
}

void ngd_mappoint_replace(ngd_mappoint *mp, ngd_mappoint *pMP) {
    /* MapPoint.cc:248-300. Transfer mp's observations to pMP, mark mp bad. */
    if (!mp || !pMP) return;
    if (pMP->mnId == mp->mnId) return;
    if (mp->mbBad) return;   /* already replaced */

    int nvisible = mp->mnVisible, nfound = mp->mnFound;

    for (int i = 0; i < mp->nObsRec; ++i) {
        ngd_keyframe *kf = mp->obs[i].kf;
        int leftIdx  = mp->obs[i].leftIdx;
        int rightIdx = mp->obs[i].rightIdx;
        if (!kf) continue;

        if (!ngd_mappoint_is_in_keyframe(pMP, kf)) {
            /* transfer: kf slot -> pMP, pMP gains observation */
            if (leftIdx >= 0 && leftIdx < kf->N) {
                kf->mvpMapPoints[leftIdx] = pMP;
                /* stereo obs (rightIdx>=0) -> nObs += 2 (Stage 6 convention) */
                ngd_mappoint_add_observation(pMP, kf, leftIdx, rightIdx);
            }
        } else {
            /* kf already observes pMP: just clear mp's slot */
            if (leftIdx >= 0 && leftIdx < kf->N && kf->mvpMapPoints[leftIdx] == mp)
                kf->mvpMapPoints[leftIdx] = NULL;
        }
    }

    pMP->mnVisible += nvisible;
    pMP->mnFound   += nfound;
    ngd_mappoint_compute_distinctive_descriptors(pMP);
    ngd_mappoint_update_normal_and_depth(pMP);

    /* mark mp bad + release its obs (consistent with set_bad; map erase left to caller). */
    mp->mbBad = 1;
    free(mp->obs);
    mp->obs = NULL;
    mp->nObsRec = 0; mp->nObs = 0; mp->capObs = 0;
}

void ngd_mappoint_erase_observation(ngd_mappoint *mp, ngd_keyframe *kf) {
    /* MapPoint.cc:168-201. */
    if (!mp || mp->mbBad) return;
    int found = -1;
    for (int i = 0; i < mp->nObsRec; ++i)
        if (mp->obs[i].kf == kf) { found = i; break; }
    if (found < 0) return;

    int leftIdx  = mp->obs[found].leftIdx;
    int rightIdx = mp->obs[found].rightIdx;

    /* nObs: stereo left w/ rightIdx>=0 counts 2 (Stage 6 convention), else 1. */
    mp->nObs -= (rightIdx >= 0) ? 2 : 1;
    if (mp->nObs < 0) mp->nObs = 0;

    /* swap-pop the obs record. */
    mp->obs[found] = mp->obs[mp->nObsRec - 1];
    mp->nObsRec--;

    /* EraseMapPointMatch: clear the KF slot if it still points to us. */
    if (kf && leftIdx >= 0 && leftIdx < kf->N && kf->mvpMapPoints[leftIdx] == mp)
        kf->mvpMapPoints[leftIdx] = NULL;

    /* If the refKF was `kf`, repick from the remaining observations. */
    if (mp->refKF == kf && mp->nObsRec > 0)
        mp->refKF = mp->obs[0].kf;

    /* "If only 2 observations or less, discard point" (MapPoint.cc:194). */
    if (mp->nObs <= 2)
        ngd_mappoint_set_bad(mp);
}
