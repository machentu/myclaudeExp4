#ifndef NGD_MATCHER_H
#define NGD_MATCHER_H

/*
 * ngd/matcher.h — ORB matching primitives (pure C).
 *
 * Begins the port of ORB_SLAM3::ORBmatcher (src/ORBmatcher.cc). This header
 * currently exposes DescriptorDistance (Hamming via parallel popcount); the
 * projection-based matchers (SearchByProjection) used by TrackLocalMap and the
 * projection-variant of TrackReferenceKeyFrame arrive in a later iteration.
 *
 * Constants (ORBmatcher.cc:39-41): TH_HIGH=100, TH_LOW=50, HISTO_LENGTH=30.
 */

#include <stdint.h>
#include "ngd/sim3.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ngd_frame;
struct ngd_mappoint;
struct ngd_keyframe;

#define NGD_TH_HIGH 100
#define NGD_TH_LOW  50
#define NGD_HISTO_LENGTH 30

/* Hamming distance between two 32-byte ORB descriptors (8 x uint32 popcount).
 * Faithful copy of ORBmatcher::DescriptorDistance (ORBmatcher.cc:2131-2147). */
int ngd_descriptor_distance(const uint8_t *a, const uint8_t *b);

/* RadiusByViewingCos (ORBmatcher.cc:219-225): cos>0.998 -> 2.5, else 4.0. */
float ngd_radius_by_viewing_cos(float viewCos);

/* ComputeThreeMaxima (ORBmatcher.cc:2085-2126): find the 3 largest bins in
 * counts[L]; prune ind2/ind3 when they fall below 0.1*max1. Sets indN=-1 if
 * absent. */
void ngd_compute_three_maxima(const int *counts, int L,
                              int *ind1, int *ind2, int *ind3);

/* SearchByProjection overload #1 (ORBmatcher.cc:47-146, left branch only —
 * Nleft==-1 mono/RGBD). Consumes the MapPoint track-projection cache filled by
 * ngd_frame_is_in_frustum. Ratio test (nnratio, SearchLocalPoints uses 0.8);
 * no orientation histogram. Writes F->mvpMapPoints[bestIdx]=mp. Returns nmatches. */
int ngd_search_by_projection_local(struct ngd_frame *F, struct ngd_mappoint **vpMPs,
                                   int nMPs, float th, int bFarPoints, float thFarPoints,
                                   float nnratio);

/* SearchByProjection overload #2 (ORBmatcher.cc:1680-1860, TrackWithMotionModel).
 * Re-projects each last-frame MapPoint into `cur` from its world pos + cur pose;
 * radius = th * mvScaleFactor[lastOctave] (no RadiusByViewingCos); forward/backward
 * octave window from tlc.z vs mb (stereo only); NO ratio test; orientation
 * histogram pruning when checkOri. Writes cur->mvpMapPoints[bestIdx2]=mp.
 * bMono disables the forward/backward stereo windowing. Returns nmatches. */
int ngd_search_by_projection_motion(struct ngd_frame *cur, const struct ngd_frame *last,
                                    float th, int bMono, int checkOri);

/* SearchByBoW (ORBmatcher.cc:227-429, left branch only — Nleft==-1 mono/RGBD).
 * Matches `kf`'s MapPoints to `F`'s features that share a vocabulary node in
 * their FeatureVectors (levelsup=4). TH_LOW=50 gate + ratio(nnratio) test;
 * 30-bin orientation histogram + ComputeThreeMaxima pruning when checkOri.
 * Writes out[bestIdxF] = kf->mvpMapPoints[bestIdxKF]; `out` is length F->N and
 * is cleared to NULL on entry (matches the original's vpMapPointMatches init).
 * TrackReferenceKeyFrame passes nnratio=0.7. Returns nmatches. */
int ngd_search_by_bow(const struct ngd_keyframe *kf, struct ngd_frame *F,
                      struct ngd_mappoint **out, float nnratio, int checkOri);

/* KF-vs-KF SearchByBoW (ORBmatcher.cc:769-907 overload, used by LoopClosing).
 * out[i] (length kfB->N) receives the kfA MP matched to kfB keypoint i. */
int ngd_search_by_bow_kf(const struct ngd_keyframe *kfA, const struct ngd_keyframe *kfB,
                         struct ngd_mappoint **out, float nnratio, int checkOri);

/* Triangulate (GeometricTools::Triangulate, GeometricTools.cc:47-66).
 *   x3D = smallest-eigenvalue eigenvector of A^T A where
 *   A row 2k+i = x_ci[k]*Tc_i.row(2) - Tc_i.row(k)  (k=0,1 ; i=1,2)
 * Tcw passed row-major 3x4 (R|t): row0=T[0..3], row1=T[4..7], row2=T[8..11].
 * xn = bearing ray in camera frame ((u-cx)/fx, (v-cy)/fy, 1). Returns 0 if the
 * point is at infinity (w==0), 1 on success. */
int ngd_triangulate(const float xn1[3], const float xn2[3],
                    const float Tcw1[12], const float Tcw2[12], float x3D[3]);

/* SearchForTriangulation (ORBmatcher.cc:911-1150, left branch only). Matches
 * kf1 features that have NO MapPoint to kf2 features that have NO MapPoint and
 * are not yet matched, sharing a vocabulary node. Epipole-distance reject +
 * fundamental-matrix epipolar constraint (Pinhole::epipolarConstrain). TH_LOW=50
 * best-only (no ratio); 30-bin orientation histogram + ComputeThreeMaxima
 * pruning. Fills out_idx1/out_idx2 (length max_pairs) with matched pairs.
 * bOnlyStereo restricts to stereo keypoints; bCoarse skips the epipolar gate.
 * Requires kf1->feat / kf2->feat computed (ngd_*_compute_bow). Returns nmatches. */
int ngd_search_for_triangulation(const struct ngd_keyframe *kf1,
                                 const struct ngd_keyframe *kf2,
                                 int *out_idx1, int *out_idx2, int max_pairs,
                                 int bOnlyStereo, int bCoarse);

/* Fuse (ORBmatcher.cc:1152-1342, left branch — Nleft==-1 mono/RGBD). Projects
 * each MP in vMPs into pKF; if it lands on a keypoint whose descriptor matches
 * (TH_LOW=50) within a stereo/mono reprojection gate (7.8 / 5.99 · inv σ²),
 * either adds the MP as a new observation of pKF or, if pKF already has a MP
 * there, Replace()s the one with fewer observations. th=3 (SearchInNeighbors).
 * Distance-band [0.8·minD, 1.2·maxD] + viewing-angle PO·normal < 0.5·dist
 * (cos60°) gates; no orientation histogram. Returns nFused. */
int ngd_fuse(struct ngd_keyframe *pKF, struct ngd_mappoint **vMPs,
             int nMPs, float th);

/* SearchBySim3 (ORBmatcher.cc:1461-1678, left branch -- mono/RGBD). Given the
 * initial matches vpMatches12[i] (KF2 MP matched to KF1 MP i, or NULL) and the
 * relative Sim3 S12 (cam2->cam1), expand matches by projecting KF1 MPs into KF2
 * (via S21) and KF2 MPs into KF1 (via S12), radius search for the best
 * descriptor (TH_HIGH=100, octave gate [nL-1,nL], no ratio/orientation pruning),
 * accepting only bidirectional agreement.  Writes new matches into
 * vpMatches12[i1] = KF2 MP at the agreed idx2.  Returns nFound.  th=3 (typical). */
int ngd_search_by_sim3(struct ngd_keyframe *kf1, struct ngd_keyframe *kf2,
                       struct ngd_mappoint **vpMatches12, ngd_sim3 S12, float th);

#ifdef __cplusplus
}
#endif
#endif /* NGD_MATCHER_H */
