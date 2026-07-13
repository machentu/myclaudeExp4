/* ngd/localmapping.c — LocalMapping context (pure C, minimal).
 * See ngd/localmapping.h for scope. Reference: src/LocalMapping.cc. */
#include "ngd/localmapping.h"
#include "ngd/map.h"
#include "ngd/mappoint.h"
#include "ngd/keyframe.h"
#include "ngd/ba.h"
#include "ngd/matcher.h"
#include "ngd/se3.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

void ngd_local_mapping_init(ngd_local_mapping *lm, ngd_map *map, int mbMonocular)
{
    memset(lm, 0, sizeof(*lm));
    lm->map = map;
    lm->mbMonocular = mbMonocular;
}

void ngd_local_mapping_set_kfdb(ngd_local_mapping *lm, struct ngd_kfdb *kfdb)
{
    lm->kfdb = kfdb;
}

void ngd_local_mapping_free(ngd_local_mapping *lm)
{
    free(lm->recent_mps);
    lm->recent_mps = NULL;
    lm->nRecent = lm->capRecent = 0;
    lm->currentKF = NULL;
}

void ngd_local_mapping_push_recent(ngd_local_mapping *lm, ngd_mappoint *mp)
{
    if (lm->nRecent == lm->capRecent) {
        int nc = lm->capRecent ? lm->capRecent * 2 : 64;
        lm->recent_mps = (ngd_mappoint**)realloc(lm->recent_mps, (size_t)nc * sizeof(ngd_mappoint*));
        lm->capRecent = nc;
    }
    lm->recent_mps[lm->nRecent++] = mp;
}

void ngd_local_mapping_map_point_culling(ngd_local_mapping *lm)
{
    /* LocalMapping.cc:354-393. */
    if (!lm->currentKF) return;
    const uint64_t nCurrentKFid = lm->currentKF->mnId;
    const int cnThObs = lm->mbMonocular ? 2 : 3;

    int i = 0;
    while (i < lm->nRecent) {
        ngd_mappoint *mp = lm->recent_mps[i];
        int drop = 0;
        if (mp->mbBad) {
            drop = 1;
        } else if (ngd_mappoint_get_found_ratio(mp) < 0.25f) {
            ngd_mappoint_set_bad(mp);
            drop = 1;
        } else if (((int)nCurrentKFid - (int)mp->mnFirstKFid) >= 2 &&
                   ngd_mappoint_observations(mp) <= cnThObs) {
            ngd_mappoint_set_bad(mp);
            drop = 1;
        } else if (((int)nCurrentKFid - (int)mp->mnFirstKFid) >= 3) {
            drop = 1;   /* matured: leave the MP alive, just drop from recent */
        }

        if (drop) {
            lm->recent_mps[i] = lm->recent_mps[lm->nRecent - 1];   /* swap-pop */
            lm->nRecent--;
        } else {
            ++i;
        }
    }
}

/* Pack an SE3 pose into a row-major 3x4 (R | t) for ngd_triangulate. */
static void se3_to_3x4(ngd_se3 s, float T[12]) {
    ngd_mat3 R = ngd_quat_to_matrix(s.q);
    T[0]=R.m[0]; T[1]=R.m[1]; T[2]=R.m[2];  T[3]=s.t.x;
    T[4]=R.m[3]; T[5]=R.m[4]; T[6]=R.m[5];  T[7]=s.t.y;
    T[8]=R.m[6]; T[9]=R.m[7]; T[10]=R.m[8]; T[11]=s.t.z;
}

void ngd_local_mapping_create_new_map_points(ngd_local_mapping *lm)
{
    /* LocalMapping.cc:396-720 (Nleft==-1 left branch only; IMU / far-point /
     * dual-cam-right branches omitted). */
    ngd_keyframe *pKF = lm->currentKF;
    if (!pKF) return;
    const int nn = lm->mbMonocular ? 30 : 10;
    ngd_keyframe *neigh[30];
    int nN = ngd_keyframe_get_best_covisibility_keyframes(pKF, nn, neigh);
    if (nN == 0) return;

    ngd_vec3 Ow1 = ngd_keyframe_get_camera_center(pKF);
    const float ratioFactor = 1.5f * (pKF->cam.nlevels > 1 ? pKF->cam.mvScaleFactor[1] : 1.0f);

    /* Pre-compute KF1 camera matrices (Rcw1 rows, tcw1). */
    ngd_mat3 Rcw1 = ngd_quat_to_matrix(pKF->pose.q);
    ngd_vec3 tcw1 = pKF->pose.t;
    ngd_mat3 Rwc1 = ngd_m3_transpose(Rcw1);
    float Tcw1[12]; se3_to_3x4(pKF->pose, Tcw1);

    const float fx1=pKF->cam.cam.fx, fy1=pKF->cam.cam.fy, cx1=pKF->cam.cam.cx, cy1=pKF->cam.cam.cy;
    const float mbf1 = pKF->cam.mbf, mb1 = pKF->cam.mb;

    for (int i = 0; i < nN; ++i) {
        ngd_keyframe *pKF2 = neigh[i];

        /* Baseline check (cc:451-468). RGBD: baseline < mb -> skip. */
        ngd_vec3 Ow2 = ngd_keyframe_get_camera_center(pKF2);
        ngd_vec3 vBaseline = ngd_v3_sub(Ow2, Ow1);
        const float baseline = ngd_v3_norm(vBaseline);
        if (!lm->mbMonocular) {
            if (baseline < pKF2->cam.mb) continue;
        } else {
            /* mono ratioBaselineDepth<0.01 check needs ComputeSceneMedianDepth
             * (not modelled); RGBD-only refactor skips this branch. */
            if (baseline < pKF2->cam.mb) continue;
        }

        int idx1[2048], idx2[2048];
        int nmatches = ngd_search_for_triangulation(pKF, pKF2, idx1, idx2, 2048,
                                                    /*bOnlyStereo=*/0, /*bCoarse=*/0);
        if (nmatches <= 0) continue;

        ngd_mat3 Rcw2 = ngd_quat_to_matrix(pKF2->pose.q);
        ngd_vec3 tcw2 = pKF2->pose.t;
        ngd_mat3 Rwc2 = ngd_m3_transpose(Rcw2);
        float Tcw2[12]; se3_to_3x4(pKF2->pose, Tcw2);
        const float fx2=pKF2->cam.cam.fx, fy2=pKF2->cam.cam.fy, cx2=pKF2->cam.cam.cx, cy2=pKF2->cam.cam.cy;

        for (int ikp = 0; ikp < nmatches; ++ikp) {
            const int i1 = idx1[ikp], i2 = idx2[ikp];
            const float kp1_ur = (pKF->uRight && i1 < pKF->N) ? pKF->uRight[i1] : -1.0f;
            const float kp2_ur = (pKF2->uRight && i2 < pKF2->N) ? pKF2->uRight[i2] : -1.0f;
            const int bStereo1 = (kp1_ur >= 0.0f);
            const int bStereo2 = (kp2_ur >= 0.0f);

            /* Bearing rays (Pinhole::unprojectEig): (u-cx)/fx, (v-cy)/fy, 1 */
            ngd_vec3 xn1 = ngd_v3((pKF->keys[i1].x - cx1)/fx1,
                                  (pKF->keys[i1].y - cy1)/fy1, 1.0f);
            ngd_vec3 xn2 = ngd_v3((pKF2->keys[i2].x - cx2)/fx2,
                                  (pKF2->keys[i2].y - cy2)/fy2, 1.0f);
            ngd_vec3 ray1 = ngd_m3_transform(Rwc1, xn1);
            ngd_vec3 ray2 = ngd_m3_transform(Rwc2, xn2);
            float n1 = ngd_v3_norm(ray1), n2 = ngd_v3_norm(ray2);
            float cosParallaxRays = (n1>0 && n2>0) ? ngd_v3_dot(ray1,ray2)/(n1*n2) : 0.0f;

            float cosParallaxStereo = cosParallaxRays + 1.0f;     /* > cosParallaxRays */
            float cosParallaxStereo1 = cosParallaxStereo, cosParallaxStereo2 = cosParallaxStereo;
            if (bStereo1) cosParallaxStereo1 = cosf(2.0f*atan2f(mb1/2.0f, pKF->depth[i1]));
            else if (bStereo2) cosParallaxStereo2 = cosf(2.0f*atan2f(pKF2->cam.mb/2.0f, pKF2->depth[i2]));
            cosParallaxStereo = fminf(cosParallaxStereo1, cosParallaxStereo2);

            float x3D[3]; int goodProj = 0; int bPointStereo = 0;
            if (cosParallaxRays < cosParallaxStereo && cosParallaxRays > 0.0f &&
                (bStereo1 || bStereo2 || cosParallaxRays < 0.9998f)) {
                goodProj = ngd_triangulate(&xn1.x, &xn2.x, Tcw1, Tcw2, x3D);   /* cc:593 */
                if (!goodProj) continue;
            } else if (bStereo1 && cosParallaxStereo1 < cosParallaxStereo2) {
                bPointStereo = 1;
                ngd_vec3 X = ngd_keyframe_unproject_stereo(pKF, i1);            /* cc:601 */
                if (X.z <= 0.0f) { goodProj = 0; } else { x3D[0]=X.x; x3D[1]=X.y; x3D[2]=X.z; goodProj=1; }
            } else if (bStereo2 && cosParallaxStereo2 < cosParallaxStereo1) {
                bPointStereo = 1;
                ngd_vec3 X = ngd_keyframe_unproject_stereo(pKF2, i2);           /* cc:607 */
                if (X.z <= 0.0f) { goodProj = 0; } else { x3D[0]=X.x; x3D[1]=X.y; x3D[2]=X.z; goodProj=1; }
            } else {
                continue;   /* no stereo and very low parallax */
            }
            if (!goodProj) continue;

            /* Front of both cameras (cc:620-627) */
            float z1 = Rcw1.m[6]*x3D[0]+Rcw1.m[7]*x3D[1]+Rcw1.m[8]*x3D[2] + tcw1.z;
            if (z1 <= 0.0f) continue;
            float z2 = Rcw2.m[6]*x3D[0]+Rcw2.m[7]*x3D[1]+Rcw2.m[8]*x3D[2] + tcw2.z;
            if (z2 <= 0.0f) continue;

            /* Reprojection error in KF1 (cc:629-655) */
            const float sigma2_1 = pKF->cam.mvLevelSigma2[pKF->keys[i1].octave];
            const float x1c = Rcw1.m[0]*x3D[0]+Rcw1.m[1]*x3D[1]+Rcw1.m[2]*x3D[2] + tcw1.x;
            const float y1c = Rcw1.m[3]*x3D[0]+Rcw1.m[4]*x3D[1]+Rcw1.m[5]*x3D[2] + tcw1.y;
            const float invz1 = 1.0f/z1;
            if (!bStereo1) {
                float u = fx1*x1c*invz1 + cx1, v = fy1*y1c*invz1 + cy1;
                float ex = u - pKF->keys[i1].x, ey = v - pKF->keys[i1].y;
                if (ex*ex + ey*ey > 5.991f*sigma2_1) continue;
            } else {
                float u = fx1*x1c*invz1 + cx1, v = fy1*y1c*invz1 + cy1;
                float ur = u - mbf1*invz1;
                float ex = u - pKF->keys[i1].x, ey = v - pKF->keys[i1].y, exr = ur - kp1_ur;
                if (ex*ex + ey*ey + exr*exr > 7.8f*sigma2_1) continue;
            }

            /* Reprojection error in KF2 (cc:657-680; uR uses current KF mbf) */
            const float sigma2_2 = pKF2->cam.mvLevelSigma2[pKF2->keys[i2].octave];
            const float x2c = Rcw2.m[0]*x3D[0]+Rcw2.m[1]*x3D[1]+Rcw2.m[2]*x3D[2] + tcw2.x;
            const float y2c = Rcw2.m[3]*x3D[0]+Rcw2.m[4]*x3D[1]+Rcw2.m[5]*x3D[2] + tcw2.y;
            const float invz2 = 1.0f/z2;
            if (!bStereo2) {
                float u = fx2*x2c*invz2 + cx2, v = fy2*y2c*invz2 + cy2;
                float ex = u - pKF2->keys[i2].x, ey = v - pKF2->keys[i2].y;
                if (ex*ex + ey*ey > 5.991f*sigma2_2) continue;
            } else {
                float u = fx2*x2c*invz2 + cx2, v = fy2*y2c*invz2 + cy2;
                float ur = u - mbf1*invz2;
                float ex = u - pKF2->keys[i2].x, ey = v - pKF2->keys[i2].y, exr = ur - kp2_ur;
                if (ex*ex + ey*ey + exr*exr > 7.8f*sigma2_2) continue;
            }

            /* Scale consistency (cc:682-699) */
            ngd_vec3 P = ngd_v3(x3D[0], x3D[1], x3D[2]);
            ngd_vec3 normal1 = ngd_v3_sub(P, Ow1); float dist1 = ngd_v3_norm(normal1);
            ngd_vec3 normal2 = ngd_v3_sub(P, Ow2); float dist2 = ngd_v3_norm(normal2);
            if (dist1 == 0.0f || dist2 == 0.0f) continue;
            const float ratioDist = dist2/dist1;
            const float ratioOctave = pKF->cam.mvScaleFactor[pKF->keys[i1].octave]
                                    / pKF2->cam.mvScaleFactor[pKF2->keys[i2].octave];
            if (ratioDist*ratioFactor < ratioOctave || ratioDist > ratioOctave*ratioFactor) continue;

            /* Triangulation successful -> create MapPoint (cc:701-717) */
            ngd_mappoint *mp = ngd_mappoint_new(x3D, pKF);
            int r1 = bStereo1 ? i1 : -1;        /* stereo obs -> nObs += 2 (Stage 6 convention) */
            int r2 = bStereo2 ? i2 : -1;
            ngd_mappoint_add_observation(mp, pKF, i1, r1);
            ngd_mappoint_add_observation(mp, pKF2, i2, r2);
            pKF->mvpMapPoints[i1] = mp;
            pKF2->mvpMapPoints[i2] = mp;
            ngd_mappoint_compute_distinctive_descriptors(mp);
            ngd_mappoint_update_normal_and_depth(mp);
            ngd_map_add_mappoint(lm->map, mp);
            ngd_local_mapping_push_recent(lm, mp);
            (void)bPointStereo;
        }
    }
}

void ngd_local_mapping_search_in_neighbors(ngd_local_mapping *lm)
{
    /* LocalMapping.cc:722-831. Fuse the current KF's MPs into its covisibility
     * neighbours and vice-versa, then refresh descriptors/normal/connections. */
    ngd_keyframe *pKF = lm->currentKF;
    ngd_map *map = lm->map;
    if (!pKF || !map) return;
    const uint64_t stamp = pKF->mnId;

    const int nn = lm->mbMonocular ? 30 : 10;            /* cc:724-725 */

    /* ---- 1st-order covisibility neighbours (cc:724-737) ---- */
    ngd_keyframe *buf1[64];
    int n1 = ngd_keyframe_get_best_covisibility_keyframes(pKF, nn, buf1);

    /* vpTargetKFs: growable, deduped via mnFuseTargetForKF == stamp. */
    int capT = 0, nT = 0;
    ngd_keyframe **target = NULL;
    for (int i = 0; i < n1; ++i) {
        ngd_keyframe *k = buf1[i];
        if (!k || ngd_keyframe_is_bad(k)) continue;
        if (k->mnFuseTargetForKF == stamp) continue;
        k->mnFuseTargetForKF = stamp;
        if (nT == capT) {
            capT = capT ? capT * 2 : 16;
            target = (ngd_keyframe**)realloc(target, (size_t)capT * sizeof(ngd_keyframe*));
        }
        target[nT++] = k;
    }

    /* ---- 2nd-order: covisibles of each 1st-order target (cc:741-754) ----
     * imax is captured before the loop so newly-appended 2nd-order KFs are NOT
     * recursed into (one expansion level only). */
    int imax = nT;
    for (int i = 0; i < imax; ++i) {
        ngd_keyframe *nbbuf[32];
        int n2 = ngd_keyframe_get_best_covisibility_keyframes(target[i], 20, nbbuf);
        for (int j = 0; j < n2; ++j) {
            ngd_keyframe *k = nbbuf[j];
            if (!k || ngd_keyframe_is_bad(k)) continue;
            if (k->mnFuseTargetForKF == stamp || k == pKF) continue;
            k->mnFuseTargetForKF = stamp;
            if (nT == capT) {
                capT = capT ? capT * 2 : 16;
                target = (ngd_keyframe**)realloc(target, (size_t)capT * sizeof(ngd_keyframe*));
            }
            target[nT++] = k;
        }
    }
    /* (inertial temporal neighbours skipped — refactor is non-IMU) */

    /* ---- Fuse current KF's MPs into each target (cc:774-782) ----
     * Snapshot the current KF's MP matches ONCE (GetMapPointMatches returns a
     * copy in the original); Fuse into a target may Replace MPs and thus mutate
     * pKF->mvpMapPoints, so iterating the live array would diverge from the
     * original across multiple targets. */
    int N = pKF->N;
    ngd_mappoint **vpMPmatches = (ngd_mappoint**)malloc((size_t)(N > 0 ? N : 1) * sizeof(ngd_mappoint*));
    for (int i = 0; i < N; ++i) vpMPmatches[i] = pKF->mvpMapPoints[i];
    for (int i = 0; i < nT; ++i)
        ngd_fuse(target[i], vpMPmatches, N, 3.0f);     /* cc:778, th=3; right cam skipped */
    free(vpMPmatches);

    /* ---- Fuse targets' MPs into current KF (cc:785-811) ---- */
    ngd_mappoint **cand = NULL; int nC = 0, capC = 0;
    for (int i = 0; i < nT; ++i) {
        ngd_keyframe *k = target[i];
        for (int j = 0; j < k->N; ++j) {
            ngd_mappoint *mp = k->mvpMapPoints[j];
            if (!mp || mp->mbBad) continue;
            if (mp->mnFuseCandidateForKF == stamp) continue;
            mp->mnFuseCandidateForKF = stamp;
            if (nC == capC) {
                capC = capC ? capC * 2 : 64;
                cand = (ngd_mappoint**)realloc(cand, (size_t)capC * sizeof(ngd_mappoint*));
            }
            cand[nC++] = mp;
        }
    }
    ngd_fuse(pKF, cand, nC, 3.0f);
    free(cand);

    /* ---- Recompute descriptors/normal on current KF's surviving MPs (cc:814-823) ---- */
    for (int i = 0; i < N; ++i) {
        ngd_mappoint *mp = pKF->mvpMapPoints[i];
        if (mp && !mp->mbBad) {
            ngd_mappoint_compute_distinctive_descriptors(mp);
            ngd_mappoint_update_normal_and_depth(mp);
        }
    }
    ngd_keyframe_update_connections(pKF);              /* cc:830 */

    /* ---- Sweep bad (replaced) MPs from the map (faithful to MapPoint::Replace
     * calling mpMap->EraseMapPoint; MP has no map pointer so the driver does it). ---- */
    int capBad = 0, nBad = 0;
    ngd_mappoint **bad = NULL;
    for (int i = 0; i < map->nMPs; ++i) {
        ngd_mappoint *mp = map->mps[i];
        if (mp && mp->mbBad) {
            if (nBad == capBad) {
                capBad = capBad ? capBad * 2 : 32;
                bad = (ngd_mappoint**)realloc(bad, (size_t)capBad * sizeof(ngd_mappoint*));
            }
            bad[nBad++] = mp;
        }
    }
    for (int i = 0; i < nBad; ++i)
        ngd_map_erase_mappoint(map, bad[i]);
    free(bad);

    free(target);
}

void ngd_local_mapping_keyframe_culling(ngd_local_mapping *lm)
{
    /* LocalMapping.cc:910-1062, non-inertial RGBD. */
    ngd_keyframe *pKF = lm->currentKF;
    ngd_map *map = lm->map;
    if (!pKF || !map) return;

    const float redundant_th = 0.9f;                   /* cc:920-922 (non-inertial) */
    const int thObs = 3;                               /* cc:956-957 */

    /* vpLocalKeyFrames = current KF's covisibles (cc:918, GetVectorCovisible).
     * connKFs is kept sorted descending by invariant, so UpdateBestCovisibles
     * re-sort is a no-op and skipped. Snapshot before culling (set_bad erases
     * from neighbours' lists, which would mutate pKF->connKFs). */
    int capL = pKF->nConn;
    ngd_keyframe **local = NULL;
    int nL = 0;
    if (capL > 0) {
        local = (ngd_keyframe**)malloc((size_t)capL * sizeof(ngd_keyframe*));
        for (int i = 0; i < capL; ++i) local[i] = pKF->connKFs[i];
        nL = capL;
    }

    int count = 0;
    for (int it = 0; it < nL; ++it) {
        count++;
        ngd_keyframe *k = local[it];
        if (!k) continue;
        if (k->mnId == map->mnInitKFid || ngd_keyframe_is_bad(k)) continue;   /* cc:952 */

        int nRedundantObservations = 0;
        int nMPs = 0;
        for (int i = 0; i < k->N; ++i) {
            ngd_mappoint *mp = k->mvpMapPoints[i];
            if (!mp || mp->mbBad) continue;
            /* "We only consider close stereo points" (cc:967-971). */
            if (k->depth[i] > k->cam.thDepth || k->depth[i] < 0.0f) continue;
            nMPs++;
            if (ngd_mappoint_observations(mp) > thObs) {           /* cc:974 */
                int scaleLevel = k->keys[i].octave;                 /* cc:976 (Nleft==-1) */
                int nObs = 0;
                for (int o = 0; o < mp->nObsRec; ++o) {
                    ngd_keyframe *kO = mp->obs[o].kf;
                    if (kO == k) continue;                          /* cc:984 */
                    int leftIdx = mp->obs[o].leftIdx;
                    if (leftIdx < 0 || leftIdx >= kO->N) continue;
                    int scaleLeveli = kO->keys[leftIdx].octave;     /* cc:990 (Nleft==-1) */
                    if (scaleLeveli <= scaleLevel + 1) {            /* cc:1002 */
                        nObs++;
                        if (nObs > thObs) break;                    /* cc:1005 */
                    }
                }
                if (nObs > thObs) nRedundantObservations++;         /* cc:1009 */
            }
        }

        if (nMPs > 0 && (float)nRedundantObservations > redundant_th * (float)nMPs) {
            ngd_keyframe_set_bad(k, map, lm->kfdb);                 /* cc:1054 (non-inertial) */
        }
        if (count > 100) break;                                     /* cc:1057 (mbAbortBA not modelled) */
    }
    free(local);
}

void ngd_local_mapping_run(ngd_local_mapping *lm)
{
    /* LocalMapping.cc:64-290, single-threaded inline. Original Run order:
     * ProcessNewKeyFrame (done in create_new_key_frame) -> MapPointCulling ->
     * CreateNewMapPoints -> SearchInNeighbors/Fuse -> KeyFrameCulling ->
     * LocalBundleAdjustment. */
    ngd_local_mapping_map_point_culling(lm);
    ngd_local_mapping_create_new_map_points(lm);
    ngd_local_mapping_search_in_neighbors(lm);
    ngd_local_mapping_keyframe_culling(lm);
    if (lm->map && lm->currentKF && lm->map->nKFs >= 4) {
        ngd_local_ba(lm->currentKF, lm->map, NULL);
    }
}
