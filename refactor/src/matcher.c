/* ngd/matcher.c — ORB matching primitives (pure C). */
#include "ngd/matcher.h"
#include "ngd/frame.h"
#include "ngd/keyframe.h"
#include "ngd/bow.h"
#include "ngd/mappoint.h"
#include "ngd/sim3.h"

#include <stdlib.h>
#include <math.h>

/* Forward decls: epipolar constraint + FeatureVector lower-bound are internal
 * helpers used by SearchForTriangulation / SearchByBoW. */
static int ngd_epipolar_constrain(const ngd_keyframe *kf1, const ngd_keyframe *kf2,
                                  ngd_mat3 F12, int idx1, int idx2);

/* MapPoint::GetIndexInKeyFrame (MapPoint.cc:428-432): the left-keypoint index of
 * `mp` in `kf`, or -1 if not observed there. Scans the obs records. */
static int mp_index_in_kf(const ngd_mappoint *mp, const ngd_keyframe *kf) {
    for (int i = 0; i < mp->nObsRec; ++i)
        if (mp->obs[i].kf == kf) return mp->obs[i].leftIdx;
    return -1;
}
static int featvec_lower_bound(const ngd_featvec *fv, uint32_t target);

int ngd_descriptor_distance(const uint8_t *a, const uint8_t *b)
{
    /* 32 bytes == 8 x uint32, exactly as the original casts to int32_t*. */
    const uint32_t *pa = (const uint32_t *)(const void *)a;
    const uint32_t *pb = (const uint32_t *)(const void *)b;
    int dist = 0;
    for (int i = 0; i < 8; ++i) {
        uint32_t v = pa[i] ^ pb[i];
        v = v - ((v >> 1) & 0x55555555u);
        v = (v & 0x33333333u) + ((v >> 2) & 0x33333333u);
        dist += (int)((((v + (v >> 4)) & 0x0F0F0F0Fu) * 0x01010101u) >> 24);
    }
    return dist;
}

float ngd_radius_by_viewing_cos(float viewCos)
{
    /* ORBmatcher.cc:219-225. */
    return (viewCos > 0.998f) ? 2.5f : 4.0f;
}

void ngd_compute_three_maxima(const int *counts, int L,
                              int *ind1, int *ind2, int *ind3)
{
    /* ORBmatcher.cc:2085-2126. */
    int max1 = 0, max2 = 0, max3 = 0;
    *ind1 = -1; *ind2 = -1; *ind3 = -1;
    for (int i = 0; i < L; ++i) {
        int s = counts[i];
        if (s > max1) {
            max3 = max2; max2 = max1; max1 = s;
            *ind3 = *ind2; *ind2 = *ind1; *ind1 = i;
        } else if (s > max2) {
            max3 = max2; max2 = s;
            *ind3 = *ind2; *ind2 = i;
        } else if (s > max3) {
            max3 = s; *ind3 = i;
        }
    }
    if (max2 < (int)(0.1f * (float)max1)) { *ind2 = -1; *ind3 = -1; }
    else if (max3 < (int)(0.1f * (float)max1)) { *ind3 = -1; }
}

/* Candidate index scratch (per call). A search window at the highest octave
 * (radius ~ th*mvScaleFactor[top] ~ 15*3.58 ~ 54px) covers ~12 grid cells, each
 * holding ~N/3072 features; for N~5000 that stays well under 1024. get_features
 * _in_area caps the written count at max_out, so nc <= 1024 holds. */
#define NGD_MATCH_CAND 1024

int ngd_search_by_projection_local(struct ngd_frame *F, struct ngd_mappoint **vpMPs,
                                   int nMPs, float th, int bFarPoints, float thFarPoints,
                                   float nnratio)
{
    /* ORBmatcher.cc:47-146 (left branch; Nleft==-1, no fisheye right branch). */
    int nmatches = 0;
    int bFactor = (th != 1.0f);                                  /* cc:51 */
    int cand[NGD_MATCH_CAND];

    for (int iMP = 0; iMP < nMPs; ++iMP) {
        ngd_mappoint *pMP = vpMPs[iMP];
        if (!pMP) continue;
        if (!pMP->mbTrackInView) continue;                       /* cc:56 (right branch omitted) */
        if (bFarPoints && pMP->mTrackDepth > thFarPoints) continue; /* cc:59 */
        if (pMP->mbBad) continue;                                  /* cc:62 */

        int nPL = pMP->mnTrackScaleLevel;                         /* cc:67 */
        if (nPL < 0) nPL = 0;
        if (nPL >= F->cam.nlevels) nPL = F->cam.nlevels - 1;

        float r = ngd_radius_by_viewing_cos(pMP->mTrackViewCos);  /* cc:70 */
        if (bFactor) r *= th;                                     /* cc:72-73 */
        float scaleFactor = F->cam.mvScaleFactor[nPL];

        int nc = ngd_frame_get_features_in_area(F, pMP->mTrackProjX, pMP->mTrackProjY,
                                               r * scaleFactor, nPL - 1, nPL,
                                               cand, NGD_MATCH_CAND);   /* cc:76 */
        if (nc == 0) continue;

        int bestDist = 256, bestLevel = -1, bestDist2 = 256, bestLevel2 = -1, bestIdx = -1;
        for (int k = 0; k < nc; ++k) {
            int idx = cand[k];
            if (F->mvpMapPoints[idx] &&
                ngd_mappoint_observations(F->mvpMapPoints[idx]) > 0) continue;   /* cc:92-94 */
            if (F->uRight[idx] > 0.0f) {                          /* cc:96 */
                float er = fabsf(pMP->mTrackProjXR - F->uRight[idx]);
                if (er > r * scaleFactor) continue;               /* cc:98-100 */
            }
            int dist = ngd_descriptor_distance(pMP->descriptor, F->descriptors + (size_t)idx * 32);
            if (dist < bestDist) {                                /* cc:107 */
                bestDist2 = bestDist; bestDist = dist;
                bestLevel2 = bestLevel; bestLevel = F->keys[idx].octave;
                bestIdx = idx;
            } else if (dist < bestDist2) {                        /* cc:117 */
                bestLevel2 = F->keys[idx].octave; bestDist2 = dist;
            }
        }
        if (bestDist <= NGD_TH_HIGH) {                            /* cc:127 */
            if (bestLevel == bestLevel2 && bestDist > nnratio * bestDist2) continue;       /* cc:129 */
            if (bestLevel != bestLevel2 || bestDist <= nnratio * bestDist2) {              /* cc:132 */
                F->mvpMapPoints[bestIdx] = pMP;
                nmatches++;
            }
        }
    }
    return nmatches;
}

int ngd_search_by_projection_motion(struct ngd_frame *cur, const struct ngd_frame *last,
                                    float th, int bMono, int checkOri)
{
    /* ORBmatcher.cc:1680-1860 (Nleft==-1 left camera only; no fisheye right branch). */
    int nmatches = 0;

    ngd_se3 Tcw = cur->pose;
    ngd_vec3 twc = ngd_se3_inverse(Tcw).t;                        /* cc:1691 (current cam centre, world) */
    ngd_vec3 tlc = ngd_se3_map(last->pose, twc);                   /* cc:1694 Tlw * twc (centre in last-cam frame) */
    int bForward  = (tlc.z > cur->cam.mb) && !bMono;              /* cc:1696 */
    int bBackward = (-tlc.z > cur->cam.mb) && !bMono;            /* cc:1697 */
    const float factor = 1.0f / (float)NGD_HISTO_LENGTH;          /* cc:1688 = 1/30 */

    int *matchBin = NULL, *matchIdx = NULL; int nmatch = 0;
    if (checkOri) {
        matchBin = (int *)malloc(sizeof(int) * (size_t)(last->N > 0 ? last->N : 1));
        matchIdx = (int *)malloc(sizeof(int) * (size_t)(last->N > 0 ? last->N : 1));
    }
    int cand[NGD_MATCH_CAND];

    for (int i = 0; i < last->N; ++i) {
        ngd_mappoint *pMP = last->mvpMapPoints[i];
        if (!pMP) continue;
        if (last->mvbOutlier[i]) continue;                         /* cc:1704 */

        ngd_vec3 x3Dw = ngd_v3(pMP->worldPos[0], pMP->worldPos[1], pMP->worldPos[2]);
        ngd_vec3 x3Dc = ngd_se3_map(Tcw, x3Dw);                    /* cc:1708 */
        if (x3Dc.z <= 0.0f) continue;                              /* cc:1714 */
        float invzc = 1.0f / x3Dc.z;

        float uv[2];
        ngd_pinhole_project(cur->cam.cam, x3Dc, uv);               /* cc:1717 */
        if (uv[0] < 0.0f || uv[0] > (float)cur->cam.imgW) continue; /* cc:1719 */
        if (uv[1] < 0.0f || uv[1] > (float)cur->cam.imgH) continue; /* cc:1722 */

        int nLO = last->keys[i].octave;                             /* cc:1724 */
        if (nLO < 0) nLO = 0;
        if (nLO >= cur->cam.nlevels) nLO = cur->cam.nlevels - 1;
        float radius = th * cur->cam.mvScaleFactor[nLO];           /* cc:1728 (no RadiusByViewingCos) */

        int nc;
        if (bForward)       nc = ngd_frame_get_features_in_area(cur, uv[0], uv[1], radius, nLO, -1, cand, NGD_MATCH_CAND);     /* cc:1733 */
        else if (bBackward) nc = ngd_frame_get_features_in_area(cur, uv[0], uv[1], radius, 0, nLO, cand, NGD_MATCH_CAND);       /* cc:1735 */
        else                nc = ngd_frame_get_features_in_area(cur, uv[0], uv[1], radius, nLO - 1, nLO + 1, cand, NGD_MATCH_CAND); /* cc:1737 */
        if (nc == 0) continue;

        int bestDist = 256, bestIdx2 = -1;                         /* no bestDist2 -> no ratio */
        for (int k = 0; k < nc; ++k) {
            int i2 = cand[k];
            if (cur->mvpMapPoints[i2] &&
                ngd_mappoint_observations(cur->mvpMapPoints[i2]) > 0) continue;                 /* cc:1751-1753 */
            if (cur->uRight[i2] > 0.0f) {                          /* cc:1755 Nleft==-1 */
                float ur = uv[0] - cur->cam.mbf * invzc;
                if (fabsf(ur - cur->uRight[i2]) > radius) continue; /* cc:1758-1760 */
            }
            int dist = ngd_descriptor_distance(pMP->descriptor, cur->descriptors + (size_t)i2 * 32);
            if (dist < bestDist) { bestDist = dist; bestIdx2 = i2; }   /* cc:1767 */
        }

        if (bestDist <= NGD_TH_HIGH) {                             /* cc:1774 */
            cur->mvpMapPoints[bestIdx2] = pMP; nmatches++;          /* cc:1777 */
            if (checkOri) {
                float rot = last->keys[i].angle - cur->keys[bestIdx2].angle;   /* cc:1788 */
                if (rot < 0.0f) rot += 360.0f;                     /* cc:1789-1790 */
                int bin = (int)lroundf(rot * factor);              /* cc:1791 round(rot/30) */
                if (bin == NGD_HISTO_LENGTH) bin = 0;              /* cc:1792-1793 wrap */
                matchBin[nmatch] = bin;
                matchIdx[nmatch] = bestIdx2;
                nmatch++;
            }
        }
    }

    if (checkOri) {
        int counts[NGD_HISTO_LENGTH];
        for (int b = 0; b < NGD_HISTO_LENGTH; ++b) counts[b] = 0;
        for (int m = 0; m < nmatch; ++m) counts[matchBin[m]]++;
        int i1, i2, i3;
        ngd_compute_three_maxima(counts, NGD_HISTO_LENGTH, &i1, &i2, &i3);   /* cc:2085 */
        for (int m = 0; m < nmatch; ++m) {
            int b = matchBin[m];
            if (b != i1 && b != i2 && b != i3) {                   /* cc: non-top-3 -> discard */
                cur->mvpMapPoints[matchIdx[m]] = NULL;
                nmatches--;
            }
        }
        free(matchBin); free(matchIdx);
    }
    return nmatches;
}

/* ------------------------------------------------------------------ *
 * Fuse (ORBmatcher.cc:1152-1342, left branch — Nleft==-1 mono/RGBD).
 * Projects each MP in vMPs into pKF; if it lands on a keypoint whose
 * descriptor matches (TH_LOW=50) within a stereo/mono reprojection gate,
 * either adds the MP as a new observation of pKF or, if pKF already has a MP
 * there, Replace()s the one with fewer observations. Returns nFused.
 * ------------------------------------------------------------------ */
int ngd_fuse(ngd_keyframe *pKF, ngd_mappoint **vMPs, int nMPs, float th)
{
    ngd_se3 Tcw = pKF->pose;
    ngd_vec3 Ow  = ngd_keyframe_get_camera_center(pKF);
    const float fx=pKF->cam.cam.fx, fy=pKF->cam.cam.fy, cx=pKF->cam.cam.cx, cy=pKF->cam.cam.cy;
    const float bf = pKF->cam.mbf;
    const int imgW = pKF->cam.imgW, imgH = pKF->cam.imgH;
    int nFused = 0;

    for (int i = 0; i < nMPs; ++i) {
        ngd_mappoint *pMP = vMPs[i];
        if (!pMP || pMP->mbBad) continue;
        if (ngd_mappoint_is_in_keyframe(pMP, pKF)) continue;        /* cc:1196 */

        ngd_vec3 p3Dw = ngd_v3(pMP->worldPos[0], pMP->worldPos[1], pMP->worldPos[2]);
        ngd_vec3 p3Dc = ngd_se3_map(Tcw, p3Dw);                     /* cc:1203 */
        if (p3Dc.z < 0.0f) continue;                                 /* cc:1206 */
        float invz = 1.0f / p3Dc.z;
        float uvx = fx * p3Dc.x * invz + cx;
        float uvy = fy * p3Dc.y * invz + cy;
        if (uvx < 0.0f || uvx >= imgW || uvy < 0.0f || uvy >= imgH) continue;  /* cc:1217 */
        float ur = uvx - bf * invz;                                  /* cc:1223 */

        const float maxD = ngd_mappoint_get_max_distance_invariance(pMP);  /* cc:1225 */
        const float minD = ngd_mappoint_get_min_distance_invariance(pMP);  /* cc:1226 */
        ngd_vec3 PO = ngd_v3_sub(p3Dw, Ow);
        float dist3D = ngd_v3_norm(PO);                              /* cc:1228 */
        if (dist3D < minD || dist3D > maxD) continue;                /* cc:1231 */

        ngd_vec3 Pn = ngd_v3(pMP->normal[0], pMP->normal[1], pMP->normal[2]);
        if (ngd_v3_dot(PO, Pn) < 0.5f * dist3D) continue;            /* cc:1239 (cos<60°) */

        int nPredictedLevel = ngd_mappoint_predict_scale(pMP, dist3D, &pKF->cam);  /* cc:1245 */
        float radius = th * pKF->cam.mvScaleFactor[nPredictedLevel];               /* cc:1248 */

        int idxbuf[256];
        int nidx = ngd_keyframe_get_features_in_area(pKF, uvx, uvy, radius,
                                                     -1, -1, idxbuf, 256);   /* cc:1250 */
        if (nidx <= 0) continue;

        int bestDist = 256, bestIdx = -1;
        for (int k = 0; k < nidx; ++k) {
            int idx = idxbuf[k];
            int kpLevel = pKF->keys[idx].octave;
            if (kpLevel < nPredictedLevel - 1 || kpLevel > nPredictedLevel) continue;  /* cc:1273 */
            float kpx = pKF->keys[idx].x, kpy = pKF->keys[idx].y;
            if (pKF->uRight && pKF->uRight[idx] >= 0.0f) {           /* cc:1276 stereo */
                float kpr = pKF->uRight[idx];
                float ex = uvx - kpx, ey = uvy - kpy, er = ur - kpr;
                float e2 = ex*ex + ey*ey + er*er;
                if (e2 * pKF->cam.mvInvLevelSigma2[kpLevel] > 7.8f) continue;   /* cc:1287 */
            } else {                                                 /* cc:1290 mono */
                float ex = uvx - kpx, ey = uvy - kpy;
                float e2 = ex*ex + ey*ey;
                if (e2 * pKF->cam.mvInvLevelSigma2[kpLevel] > 5.99f) continue;  /* cc:1298 */
            }
            const uint8_t *dKF = pKF->descriptors + (size_t)idx * 32;
            int dist = ngd_descriptor_distance(pMP->descriptor, dKF);   /* cc:1306 */
            if (dist < bestDist) { bestDist = dist; bestIdx = idx; }     /* cc:1308 */
        }

        if (bestDist <= NGD_TH_LOW) {                                /* cc:1316 */
            ngd_mappoint *pMPinKF = (bestIdx >= 0 && bestIdx < pKF->N) ? pKF->mvpMapPoints[bestIdx] : NULL;
            if (pMPinKF) {
                if (!pMPinKF->mbBad) {
                    if (ngd_mappoint_observations(pMPinKF) > ngd_mappoint_observations(pMP))
                        ngd_mappoint_replace(pMP, pMPinKF);          /* cc:1324: pMP -> pMPinKF */
                    else
                        ngd_mappoint_replace(pMPinKF, pMP);          /* cc:1326: pMPinKF -> pMP */
                }
            } else {
                int rightIdx = (pKF->uRight && pKF->uRight[bestIdx] >= 0.0f) ? bestIdx : -1;  /* stereo nObs+2 */
                ngd_mappoint_add_observation(pMP, pKF, bestIdx, rightIdx);   /* cc:1331 */
                pKF->mvpMapPoints[bestIdx] = pMP;                            /* cc:1332 */
            }
            nFused++;
        }
    }
    return nFused;
}

/* ------------------------------------------------------------------ *
 * Triangulate (GeometricTools::Triangulate, GeometricTools.cc:47-66).
 *   A(4x4) row 2k+i = x_ci[k] * Tc_i.row(2) - Tc_i.row(k),  k=0,1 ; i=1,2
 *   x3D = smallest-eigenvalue eigenvector of A^T A (== last right singular
 *   vector of A), dehomogenised. Returns 0 if w==0 (point at infinity).
 *
 * Tcw is row-major 3x4 (R | t): row0=T[0..3], row1=T[4..7], row2=T[8..11].
 * xn = bearing ray in the camera frame (Pinhole::unprojectEig: (u-cx)/fx,
 * (v-cy)/fy, 1).
 * ------------------------------------------------------------------ */
static void jacobi_eigen_sym4(const double A[16], double V[16], double eig[4])
{
    /* Cyclic Jacobi for a 4x4 real symmetric matrix (mirrors pnp.c::jacobi_sym,
     * proven). J=[[c,-s],[s,c]], A'=J^T A J via two-sided rotation (columns then
     * rows). V columns = eigenvectors, eig = eigenvalues (ascending not sorted;
     * caller picks the min). */
    double a[16]; for (int i=0;i<16;++i) a[i]=A[i];
    const int n=4;
    for (int i=0;i<n;++i) for (int j=0;j<n;++j) V[i*n+j] = (i==j)?1.0:0.0;
    for (int sweep=0; sweep<60; ++sweep) {
        double off = 0;
        for (int p=0;p<n;++p) for (int q=p+1;q<n;++q) off += a[p*n+q]*a[p*n+q];
        if (off < 1e-24) break;
        for (int p=0;p<n;++p) for (int q=p+1;q<n;++q) {
            double apq = a[p*n+q];
            if (fabs(apq) < 1e-18) continue;
            double app = a[p*n+p], aqq = a[q*n+q];
            double phi = 0.5*atan2(2.0*apq, app-aqq);   /* pnp.c convention */
            double c = cos(phi), s = sin(phi);
            for (int i=0;i<n;++i){ double aip=a[i*n+p], aiq=a[i*n+q];
                a[i*n+p] = c*aip + s*aiq; a[i*n+q] = -s*aip + c*aiq; }
            for (int i=0;i<n;++i){ double api=a[p*n+i], aqi=a[q*n+i];
                a[p*n+i] = c*api + s*aqi; a[q*n+i] = -s*api + c*aqi; }
            for (int i=0;i<n;++i){ double vip=V[i*n+p], viq=V[i*n+q];
                V[i*n+p] = c*vip + s*viq; V[i*n+q] = -s*vip + c*viq; }
        }
    }
    for (int i=0;i<n;++i) eig[i] = a[i*n+i];
}

int ngd_triangulate(const float xn1[3], const float xn2[3],
                    const float Tcw1[12], const float Tcw2[12], float x3D[3])
{
    /* Tcw.row2 = T[8..11], row0 = T[0..3], row1 = T[4..7]. */
    double A[16];
    /* row0: xn1[0]*Tcw1.row2 - Tcw1.row0 ; row1: xn1[1]*Tcw1.row2 - Tcw1.row1 */
    for (int k=0;k<4;++k){
        A[0*4+k] = (double)xn1[0]*Tcw1[8+k] - Tcw1[0+k];
        A[1*4+k] = (double)xn1[1]*Tcw1[8+k] - Tcw1[4+k];
        A[2*4+k] = (double)xn2[0]*Tcw2[8+k] - Tcw2[0+k];
        A[3*4+k] = (double)xn2[1]*Tcw2[8+k] - Tcw2[4+k];
    }
    /* AtA (4x4 symmetric) */
    double AtA[16];
    for (int i=0;i<4;++i) for (int j=0;j<4;++j){
        double s=0; for (int k=0;k<4;++k) s+=A[k*4+i]*A[k*4+j]; AtA[i*4+j]=s;
    }
    double V[16], eig[4];
    jacobi_eigen_sym4(AtA, V, eig);
    /* smallest eigenvalue */
    int mi=0; for (int i=1;i<4;++i) if (eig[i]<eig[mi]) mi=i;
    double w = V[3*4+mi];
    if (fabs(w) < 1e-12) return 0;
    x3D[0] = (float)(V[0*4+mi]/w);
    x3D[1] = (float)(V[1*4+mi]/w);
    x3D[2] = (float)(V[2*4+mi]/w);
    return 1;
}

/* ------------------------------------------------------------------ *
 * SearchForTriangulation (ORBmatcher.cc:911-1150, left branch only —
 * Nleft==-1 mono/RGBD). Matches kf1 features that have NO MapPoint to kf2
 * features that have NO MapPoint and are not yet matched, sharing a vocabulary
 * node. Epipole-distance reject + fundamental-matrix epipolar constraint
 * (Pinhole::epipolarConstrain, Pinhole.cpp:107-129). TH_LOW=50 best-only (no
 * ratio). 30-bin orientation histogram + ComputeThreeMaxima pruning.
 *
 * out_idx1/out_idx2 are caller arrays of length max_pairs; returns nmatches.
 * ------------------------------------------------------------------ */
int ngd_search_for_triangulation(const ngd_keyframe *kf1, const ngd_keyframe *kf2,
                                 int *out_idx1, int *out_idx2, int max_pairs,
                                 int bOnlyStereo, int bCoarse)
{
    const ngd_featvec *f1 = kf1->feat;
    const ngd_featvec *f2 = kf2->feat;
    if (!f1 || !f2) return 0;

    /* T12 = Tcw1 * Twc2 ; R12, t12 (cc:918-935, single-camera branch) */
    ngd_se3 Twc2 = ngd_se3_inverse(kf2->pose);
    ngd_se3 T12  = ngd_se3_multiply(kf1->pose, Twc2);
    ngd_mat3 R12 = ngd_quat_to_matrix(T12.q);
    ngd_vec3 t12 = T12.t;

    /* Epipole of kf1's centre in kf2 (cc:921-924): Cw = kf1 centre (world),
     * C2 = Tcw2 * Cw, ep = project(C2). */
    ngd_vec3 Cw  = ngd_keyframe_get_camera_center(kf1);
    ngd_vec3 C2  = ngd_se3_map(kf2->pose, Cw);
    float ep[2] = { 1e9f, 1e9f };
    if (C2.z > 1e-6f) {
        ep[0] = kf2->cam.cam.fx * C2.x / C2.z + kf2->cam.cam.cx;
        ep[1] = kf2->cam.cam.fy * C2.y / C2.z + kf2->cam.cam.cy;
    }

    /* F12 = K1^-T * [t12]x * R12 * K2^-1  (Pinhole.cpp:109-112) */
    float fx1=kf1->cam.cam.fx, fy1=kf1->cam.cam.fy, cx1=kf1->cam.cam.cx, cy1=kf1->cam.cam.cy;
    float fx2=kf2->cam.cam.fx, fy2=kf2->cam.cam.fy, cx2=kf2->cam.cam.cx, cy2=kf2->cam.cam.cy;
    /* K2inv = [[1/fx2,0,-cx2/fx2],[0,1/fy2,-cy2/fy2],[0,0,1]] */
    ngd_mat3 K2inv = {{ 1/fx2,0,-cx2/fx2, 0,1/fy2,-cy2/fy2, 0,0,1 }};
    ngd_mat3 tx  = ngd_so3_hat(t12);                       /* [t12]x */
    ngd_mat3 tmp = ngd_m3_multiply(tx, R12);               /* [t12]x * R12 */
    ngd_mat3 Fm  = ngd_m3_multiply(tmp, K2inv);            /* [t12]x * R12 * K2^-1 */
    /* K1^-T = (K1^-1)^T ; K1^-1 = [[1/fx1,0,-cx1/fx1],[0,1/fy1,-cy1/fy1],[0,0,1]] */
    ngd_mat3 K1invT = {{ 1/fx1,0,0, 0,1/fy1,0, -cx1/fx1,-cy1/fy1,1 }};
    ngd_mat3 F12 = ngd_m3_multiply(K1invT, Fm);

    int nmatches = 0;
    int *matched2 = (int*)calloc((size_t)kf2->N, sizeof(int));   /* vbMatched2 */
    int *matches12 = (int*)malloc((size_t)kf1->N * sizeof(int));
    for (int i=0;i<kf1->N;++i) matches12[i] = -1;

    /* orientation histogram */
    const float factor = 1.0f/(float)NGD_HISTO_LENGTH;
    int *rotBin = NULL, *rotIdx = NULL, nrot = 0, caprot = 0;

    int i1it = 0, i2it = 0;
    while (i1it < f1->n && i2it < f2->n) {
        uint32_t n1 = f1->items[i1it].id;
        uint32_t n2 = f2->items[i2it].id;
        if (n1 == n2) {
            const ngd_feat_entry *e1 = &f1->items[i1it];
            const ngd_feat_entry *e2 = &f2->items[i2it];
            for (int a=0; a<e1->n; ++a) {
                int idx1 = (int)e1->idx[a];
                if (kf1->mvpMapPoints[idx1]) continue;                 /* cc:978 has MP */
                int stereo1 = (kf2->cam.mbf>0 && kf1->uRight && kf1->uRight[idx1]>=0) ? 1 : 0;
                if (bOnlyStereo && !stereo1) continue;
                const uint8_t *d1 = kf1->descriptors + (size_t)idx1*32;
                int bestDist = NGD_TH_LOW, bestIdx2 = -1;             /* cc:998 */
                for (int b=0; b<e2->n; ++b) {
                    int idx2 = (int)e2->idx[b];
                    if (matched2[idx2] || kf2->mvpMapPoints[idx2]) continue; /* cc:1008 */
                    int stereo2 = (kf2->cam.mbf>0 && kf2->uRight && kf2->uRight[idx2]>=0) ? 1 : 0;
                    if (bOnlyStereo && !stereo2) continue;
                    int dist = ngd_descriptor_distance(d1, kf2->descriptors + (size_t)idx2*32);
                    if (dist > NGD_TH_LOW || dist > bestDist) continue;       /* cc:1021 */
                    /* epipole reject (cc:1030-1038): only when both mono */
                    if (!stereo1 && !stereo2) {
                        float ex = ep[0]-kf2->keys[idx2].x;
                        float ey = ep[1]-kf2->keys[idx2].y;
                        if (ex*ex+ey*ey < 100.0f*kf2->cam.mvScaleFactor[kf2->keys[idx2].octave])
                            continue;
                    }
                    /* epipolar constraint (cc:1076) */
                    if (bCoarse || ngd_epipolar_constrain(kf1,kf2,F12,idx1,idx2)) {
                        bestIdx2 = idx2; bestDist = dist;
                    }
                }
                if (bestIdx2 >= 0) {
                    matches12[idx1] = bestIdx2;
                    matched2[bestIdx2] = 1;
                    nmatches++;
                    if (nrot == caprot) {
                        caprot = caprot? caprot*2:64;
                        rotBin = (int*)realloc(rotBin, sizeof(int)*(size_t)caprot);
                        rotIdx = (int*)realloc(rotIdx, sizeof(int)*(size_t)caprot);
                    }
                    float rot = kf1->keys[idx1].angle - kf2->keys[bestIdx2].angle; /* cc:1093 */
                    if (rot < 0.0f) rot += 360.0f;
                    int bin = (int)lroundf(rot*factor);
                    if (bin == NGD_HISTO_LENGTH) bin = 0;
                    rotBin[nrot] = bin; rotIdx[nrot] = idx1; nrot++;
                }
            }
            i1it++; i2it++;
        } else if (n1 < n2) {
            i1it = featvec_lower_bound(f1, n2);
        } else {
            i2it = featvec_lower_bound(f2, n1);
        }
    }

    /* orientation pruning (cc:1118-1137) */
    {
        int counts[NGD_HISTO_LENGTH]; for (int b=0;b<NGD_HISTO_LENGTH;++b) counts[b]=0;
        for (int m=0;m<nrot;++m) counts[rotBin[m]]++;
        int i1,i2,i3; ngd_compute_three_maxima(counts, NGD_HISTO_LENGTH, &i1,&i2,&i3);
        for (int m=0;m<nrot;++m) {
            int b=rotBin[m];
            if (b!=i1 && b!=i2 && b!=i3) {
                if (matches12[rotIdx[m]] >= 0) { matches12[rotIdx[m]] = -1; nmatches--; }
            }
        }
    }

    int out = 0;
    for (int i=0;i<kf1->N && out<max_pairs;++i) {
        if (matches12[i] < 0) continue;
        out_idx1[out] = i; out_idx2[out] = matches12[i]; out++;
    }

    free(matched2); free(matches12); free(rotBin); free(rotIdx);
    return nmatches;
}

/* Pinhole::epipolarConstrain (Pinhole.cpp:107-129). F12 precomputed (matches
 * kf1->kp1 to kf2->kp2). l = F12^T * kp1h ; dsqr = (l.kp2h)^2 / (a^2+b^2) ;
 * accept if dsqr < 3.84 * mvLevelSigma2[kp2.octave]. */
static int ngd_epipolar_constrain(const ngd_keyframe *kf1, const ngd_keyframe *kf2,
                                  ngd_mat3 F12, int idx1, int idx2)
{
    float x1 = kf1->keys[idx1].x, y1 = kf1->keys[idx1].y;
    float x2 = kf2->keys[idx2].x, y2 = kf2->keys[idx2].y;
    /* l = F12^T * [x1 y1 1]  (column-row: a = x1*F(0,0)+y1*F(1,0)+1*F(2,0)) */
    float a = x1*F12.m[0] + y1*F12.m[3] + F12.m[6];
    float b = x1*F12.m[1] + y1*F12.m[4] + F12.m[7];
    float c = x1*F12.m[2] + y1*F12.m[5] + F12.m[8];
    float num = a*x2 + b*y2 + c;
    float den = a*a + b*b;
    if (den == 0.0f) return 0;
    float dsqr = num*num/den;
    float unc = kf2->cam.mvLevelSigma2[kf2->keys[idx2].octave];
    return dsqr < 3.84f*unc ? 1 : 0;
}


/* lower_bound on a sorted ngd_featvec: first entry index with id >= target, or fv->n. */
static int featvec_lower_bound(const ngd_featvec *fv, uint32_t target)
{
    int lo = 0, hi = fv->n;
    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        if (fv->items[mid].id < target) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

/* Shared core of SearchByBoW (ORBmatcher.cc:227-429, left branch). Operates on
 * primitive arrays so it serves both the KF-vs-Frame and KF-vs-KF overloads
 * (the original has both).  out[] (length NF) receives the matched MP from the
 * "kf" side for each matched feature on the "F" side; cleared on entry. */
static int search_by_bow_core(const ngd_featvec *fkf,
                              const uint8_t *descKF, ngd_mappoint * const *mpKF,
                              const ngd_keypoint *keysKF,
                              const ngd_featvec *ff,
                              const uint8_t *descF, const ngd_keypoint *keysF, int NF,
                              ngd_mappoint **out, float nnratio, int checkOri)
{
    for (int i = 0; i < NF; ++i) out[i] = NULL;              /* cc:231 init */
    if (!fkf || !ff) return 0;

    int nmatches = 0;
    const float factor = 1.0f / (float)NGD_HISTO_LENGTH;     /* cc:240 = 1/30 */

    int *matchBin = NULL, *matchIdx = NULL; int nmatch = 0, capmatch = 0;
    if (checkOri) {
        capmatch = 64;
        matchBin = (int*)malloc(sizeof(int) * (size_t)capmatch);
        matchIdx = (int*)malloc(sizeof(int) * (size_t)capmatch);
    }

    int ki = 0, fi = 0;
    while (ki < fkf->n && fi < ff->n) {
        uint32_t nKF = fkf->items[ki].id;
        uint32_t nF  = ff->items[fi].id;
        if (nKF == nF) {
            const ngd_feat_entry *eKF = &fkf->items[ki];
            const ngd_feat_entry *eF  = &ff->items[fi];

            for (int a = 0; a < eKF->n; ++a) {
                uint32_t realIdxKF = eKF->idx[a];
                ngd_mappoint *pMP = mpKF[realIdxKF];               /* cc:259 */
                if (!pMP) continue;                                  /* cc:261 */
                if (pMP->mbBad) continue;                            /* cc:264 */

                const uint8_t *dKF = descKF + (size_t)realIdxKF * 32;
                int bestDist1 = 256, bestIdxF = -1, bestDist2 = 256; /* cc:269-271 */

                for (int b = 0; b < eF->n; ++b) {
                    uint32_t realIdxF = eF->idx[b];
                    if (out[realIdxF]) continue;                     /* cc:282 already matched */
                    const uint8_t *dF = descF + (size_t)realIdxF * 32;
                    int dist = ngd_descriptor_distance(dKF, dF);     /* cc:287 */
                    if (dist < bestDist1) {                          /* cc:289 */
                        bestDist2 = bestDist1;
                        bestDist1 = dist;
                        bestIdxF = (int)realIdxF;
                    } else if (dist < bestDist2) {                   /* cc:295 */
                        bestDist2 = dist;
                    }
                }

                if (bestDist1 <= NGD_TH_LOW) {                       /* cc:331 */
                    if ((float)bestDist1 < nnratio * (float)bestDist2) { /* cc:333 */
                        out[bestIdxF] = pMP;                         /* cc:335 */
                        nmatches++;
                        if (checkOri) {
                            float rot = keysKF[realIdxKF].angle - keysF[bestIdxF].angle; /* cc:349 */
                            if (rot < 0.0f) rot += 360.0f;           /* cc:350-351 */
                            int bin = (int)lroundf(rot * factor);    /* cc:352 round(rot/30) */
                            if (bin == NGD_HISTO_LENGTH) bin = 0;    /* cc:353-354 wrap */
                            if (nmatch >= capmatch) {
                                capmatch *= 2;
                                matchBin = (int*)realloc(matchBin, sizeof(int) * (size_t)capmatch);
                                matchIdx = (int*)realloc(matchIdx, sizeof(int) * (size_t)capmatch);
                            }
                            matchBin[nmatch] = bin;
                            matchIdx[nmatch] = bestIdxF;
                            nmatch++;
                        }
                    }
                }
            }
            ki++; fi++;                                             /* cc:395-396 */
        } else if (nKF < nF) {
            ki = featvec_lower_bound(fkf, nF);                      /* cc:400 */
        } else {
            fi = featvec_lower_bound(ff, nKF);                      /* cc:404 */
        }
    }

    if (checkOri) {
        int counts[NGD_HISTO_LENGTH];
        for (int b = 0; b < NGD_HISTO_LENGTH; ++b) counts[b] = 0;
        for (int m = 0; m < nmatch; ++m) counts[matchBin[m]]++;
        int i1, i2, i3;
        ngd_compute_three_maxima(counts, NGD_HISTO_LENGTH, &i1, &i2, &i3);   /* cc:414 */
        for (int m = 0; m < nmatch; ++m) {
            int b = matchBin[m];
            if (b != i1 && b != i2 && b != i3) {                   /* cc:418 non-top-3 -> discard */
                out[matchIdx[m]] = NULL;                            /* cc:422 */
                nmatches--;                                         /* cc:423 */
            }
        }
        free(matchBin); free(matchIdx);
    }
    return nmatches;
}

int ngd_search_by_bow(const ngd_keyframe *kf, ngd_frame *F,
                      ngd_mappoint **out, float nnratio, int checkOri)
{
    return search_by_bow_core(kf->feat, kf->descriptors, kf->mvpMapPoints, kf->keys,
                              F->feat, F->descriptors, F->keys, F->N,
                              out, nnratio, checkOri);
}

/* KF-vs-KF SearchByBoW (ORBmatcher.cc:769-907 overload used by LoopClosing):
 * out[i] (length kfB->N) receives the kfA MP matched to kfB keypoint i. */
int ngd_search_by_bow_kf(const ngd_keyframe *kfA, const ngd_keyframe *kfB,
                         ngd_mappoint **out, float nnratio, int checkOri)
{
    return search_by_bow_core(kfA->feat, kfA->descriptors, kfA->mvpMapPoints, kfA->keys,
                              kfB->feat, kfB->descriptors, kfB->keys, kfB->N,
                              out, nnratio, checkOri);
}

/* ------------------------------------------------------------------ *
 * SearchBySim3 (ORBmatcher.cc:1461-1678, left branch -- mono/RGBD).
 * Given an initial set of matches vpMatches12[i] (MP in KF2 matched to KF1
 * MP i, or NULL) and the relative Sim3 S12 (cam2->cam1), expand the matches
 * by projecting each KF1 MP into KF2 (and vice-versa via S21) and searching a
 * radius for the best descriptor (TH_HIGH=100, octave gate [nL-1,nL], no
 * ratio test, no orientation histogram).  A match is accepted only on
 * bidirectional agreement (vnMatch1[i1]==idx2 && vnMatch2[idx2]==i1).
 * Writes newly found matches into vpMatches12[i1] = KF2 MP at idx2.
 * Returns nFound.
 * ------------------------------------------------------------------ */
int ngd_search_by_sim3(ngd_keyframe *kf1, ngd_keyframe *kf2,
                       ngd_mappoint **vpMatches12, ngd_sim3 S12, float th)
{
    ngd_se3 T1w = kf1->pose;
    ngd_se3 T2w = kf2->pose;
    ngd_sim3 S21 = ngd_sim3_inverse(S12);

    int N1 = kf1->N, N2 = kf2->N;
    const int imgW1 = kf1->cam.imgW, imgH1 = kf1->cam.imgH;
    const int imgW2 = kf2->cam.imgW, imgH2 = kf2->cam.imgH;

    /* vbAlreadyMatched1/2 mark features that already have a match. */
    char *vbM1 = (char*)calloc(N1 > 0 ? N1 : 1, 1);
    char *vbM2 = (char*)calloc(N2 > 0 ? N2 : 1, 1);
    int *vnM1 = (int*)malloc(sizeof(int)*(N1 > 0 ? N1 : 1));
    int *vnM2 = (int*)malloc(sizeof(int)*(N2 > 0 ? N2 : 1));
    for (int i = 0; i < N1; ++i) vnM1[i] = -1;
    for (int i = 0; i < N2; ++i) vnM2[i] = -1;

    for (int i = 0; i < N1; ++i) {
        ngd_mappoint *pMP = vpMatches12[i];
        if (pMP) {
            vbM1[i] = 1;
            int idx2 = mp_index_in_kf(pMP, kf2);          /* cc:1490 */
            if (idx2 >= 0 && idx2 < N2) vbM2[idx2] = 1;
        }
    }

    /* ---- Loop 1: KF1 -> KF2 (project MP1 via S21 into KF2 image) ---- */
    const float fx2=kf2->cam.cam.fx, fy2=kf2->cam.cam.fy, cx2=kf2->cam.cam.cx, cy2=kf2->cam.cam.cy;
    for (int i1 = 0; i1 < N1; ++i1) {
        ngd_mappoint *pMP = kf1->mvpMapPoints[i1];
        if (!pMP || vbM1[i1] || pMP->mbBad) continue;       /* cc:1504-1508 */

        ngd_vec3 p3Dw = ngd_v3(pMP->worldPos[0], pMP->worldPos[1], pMP->worldPos[2]);
        ngd_vec3 p3Dc1 = ngd_se3_map(T1w, p3Dw);             /* cc:1511 */
        ngd_vec3 p3Dc2 = ngd_sim3_map(S21, p3Dc1);           /* cc:1512 */
        if (p3Dc2.z < 0.0f) continue;                        /* cc:1515 */
        float invz = 1.0f / p3Dc2.z;
        float u = fx2 * p3Dc2.x * invz + cx2;
        float v = fy2 * p3Dc2.y * invz + cy2;
        if (u < 0.0f || u >= imgW2 || v < 0.0f || v >= imgH2) continue;  /* cc:1526 */

        float maxD = ngd_mappoint_get_max_distance_invariance(pMP);
        float minD = ngd_mappoint_get_min_distance_invariance(pMP);
        float dist3D = ngd_v3_norm(p3Dc2);                   /* cc:1531 */
        if (dist3D < minD || dist3D > maxD) continue;        /* cc:1534 */

        int nL = ngd_mappoint_predict_scale(pMP, dist3D, &kf2->cam);   /* cc:1538 */
        float radius = th * kf2->cam.mvScaleFactor[nL];                 /* cc:1541 */

        int idxbuf[256];
        int nidx = ngd_keyframe_get_features_in_area(kf2, u, v, radius, -1, -1, idxbuf, 256);
        if (nidx <= 0) continue;

        int bestDist = 256, bestIdx = -1;
        for (int k = 0; k < nidx; ++k) {
            int idx = idxbuf[k];
            int lvl = kf2->keys[idx].octave;
            if (lvl < nL - 1 || lvl > nL) continue;         /* cc:1559 */
            const uint8_t *dKF = kf2->descriptors + (size_t)idx * 32;
            int dist = ngd_descriptor_distance(pMP->descriptor, dKF);
            if (dist < bestDist) { bestDist = dist; bestIdx = idx; }
        }
        if (bestDist <= NGD_TH_HIGH) vnM1[i1] = bestIdx;    /* cc:1573 */
    }

    /* ---- Loop 2: KF2 -> KF1 (project MP2 via S12 into KF1 image) ---- */
    const float fx1=kf1->cam.cam.fx, fy1=kf1->cam.cam.fy, cx1=kf1->cam.cam.cx, cy1=kf1->cam.cam.cy;
    for (int i2 = 0; i2 < N2; ++i2) {
        ngd_mappoint *pMP = kf2->mvpMapPoints[i2];
        if (!pMP || vbM2[i2] || pMP->mbBad) continue;
        ngd_vec3 p3Dw = ngd_v3(pMP->worldPos[0], pMP->worldPos[1], pMP->worldPos[2]);
        ngd_vec3 p3Dc2 = ngd_se3_map(T2w, p3Dw);
        ngd_vec3 p3Dc1 = ngd_sim3_map(S12, p3Dc2);
        if (p3Dc1.z < 0.0f) continue;
        float invz = 1.0f / p3Dc1.z;
        float u = fx1 * p3Dc1.x * invz + cx1;
        float v = fy1 * p3Dc1.y * invz + cy1;
        if (u < 0.0f || u >= imgW1 || v < 0.0f || v >= imgH1) continue;

        float maxD = ngd_mappoint_get_max_distance_invariance(pMP);
        float minD = ngd_mappoint_get_min_distance_invariance(pMP);
        float dist3D = ngd_v3_norm(p3Dc1);
        if (dist3D < minD || dist3D > maxD) continue;

        int nL = ngd_mappoint_predict_scale(pMP, dist3D, &kf1->cam);
        float radius = th * kf1->cam.mvScaleFactor[nL];

        int idxbuf[256];
        int nidx = ngd_keyframe_get_features_in_area(kf1, u, v, radius, -1, -1, idxbuf, 256);
        if (nidx <= 0) continue;

        int bestDist = 256, bestIdx = -1;
        for (int k = 0; k < nidx; ++k) {
            int idx = idxbuf[k];
            int lvl = kf1->keys[idx].octave;
            if (lvl < nL - 1 || lvl > nL) continue;
            const uint8_t *dKF = kf1->descriptors + (size_t)idx * 32;
            int dist = ngd_descriptor_distance(pMP->descriptor, dKF);
            if (dist < bestDist) { bestDist = dist; bestIdx = idx; }
        }
        if (bestDist <= NGD_TH_HIGH) vnM2[i2] = bestIdx;
    }

    /* ---- Agreement check (cc:1659-1675) ---- */
    int nFound = 0;
    for (int i1 = 0; i1 < N1; ++i1) {
        int idx2 = vnM1[i1];
        if (idx2 >= 0) {
            int idx1 = vnM2[idx2];
            if (idx1 == i1) {
                vpMatches12[i1] = kf2->mvpMapPoints[idx2];
                nFound++;
            }
        }
    }

    free(vbM1); free(vbM2); free(vnM1); free(vnM2);
    return nFound;
}
