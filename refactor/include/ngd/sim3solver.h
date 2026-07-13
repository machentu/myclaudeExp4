#ifndef NGD_SIM3SOLVER_H
#define NGD_SIM3SOLVER_H

/*
 * ngd/sim3solver.h — closed-form Sim(3) from 3D-3D correspondences (pure C).
 *
 * Faithful port of ORB-SLAM3 Sim3Solver (src/Sim3Solver.cc), which implements
 * Horn 1987 ("Closed-form solution of absolute orientation using unit
 * quaternions") wrapped in a 3-point RANSAC.  Used by LoopClosing's
 * DetectCommonRegionsFromBoW to recover the relative Sim3 between two KFs
 * from BoW-matched MapPoints.
 *
 * API shape follows the refactor convention (pose_opt/pnp): the solver operates
 * on flat correspondence arrays that the caller extracts from KF/MP; it does
 * not depend on the KeyFrame/MapPoint structs.  Correspondences are 3D points
 * expressed in each KF's *camera* frame (Rcw*Pw+tcw), their image projections
 * in each camera, and a per-point max squared error (9.210*sigma^2, chi2 99%).
 *
 * RANSAC is deterministic (srand(seed)) to match pnp.c / mlpnp.c.
 */

#include "ngd/math.h"
#include "ngd/pinhole.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ngd_sim3_solver {
    /* Correspondences (compact, bad/null already filtered out by caller). */
    int N;
    ngd_vec3 *X3Dc1;     /* [N] cam1-frame 3D */
    ngd_vec3 *X3Dc2;     /* [N] cam2-frame 3D */
    float    *P1im1;     /* [2N] image projection of X3Dc1 in cam1 (u,v) */
    float    *P2im2;     /* [2N] image projection of X3Dc2 in cam2 (u,v) */
    float    *maxErr1;   /* [N] 9.210*sigma1^2 */
    float    *maxErr2;   /* [N] 9.210*sigma2^2 */
    ngd_pinhole cam1, cam2;
    int bFixScale;

    /* original-index map (caller may pass NULL for identity); vbInliers is
     * returned in this space, matching Sim3Solver.cc's mvnIndices1 behavior. */
    int nOrig;
    const int *origIdx;  /* [N] -> [0,nOrig); NULL means identity (nOrig==N) */

    /* RANSAC params / state */
    double prob;
    int minInliers;
    int maxIts;
    int mnIterations;
    int mnBestInliers;

    /* best solution */
    ngd_mat3 mBestRotation;   /* R12 */
    ngd_vec3 mBestTranslation;/* t12 */
    float    mBestScale;      /* s12 */
    int      haveBest;

    /* scratch (per-iteration) */
    char *mvbInliersi;   /* [N] */
    char *mvbBestInliers;/* [N] */
    int  *mvAllIndices;  /* [N] */
    int   mnInliersi;
    unsigned int rng;
} ngd_sim3_solver;

/* Build a solver from correspondences. origIdx may be NULL (identity). */
ngd_sim3_solver* ngd_sim3solver_create(
    const ngd_vec3* X3Dc1, const ngd_vec3* X3Dc2,
    const float* P1im1, const float* P2im2,
    const float* maxErr1, const float* maxErr2,
    int N, int bFixScale,
    ngd_pinhole cam1, ngd_pinhole cam2,
    int nOrig, const int* origIdx, unsigned int seed);

void ngd_sim3solver_set_params(ngd_sim3_solver* s, double prob, int minInliers, int maxIts);

/* Run up to nIterations RANSAC rounds.  Fills vbInliers[nOrig] (caller-alloc),
 * *nInliers, and outT12[16] (row-major 4x4 [sR|t;0 1]) with the best T12 found
 * (identity if none).  *bNoMore set when RANSAC exhausted. */
void ngd_sim3solver_iterate(ngd_sim3_solver* s, int nIterations, int* bNoMore,
                            char* vbInliers, int* nInliers, float outT12[16]);

/* Run the full RANSAC (maxIts) and return best T12. */
void ngd_sim3solver_find(ngd_sim3_solver* s, char* vbInliers, int* nInliers, float outT12[16]);

void ngd_sim3solver_destroy(ngd_sim3_solver* s);

#ifdef __cplusplus
}
#endif
#endif /* NGD_SIM3SOLVER_H */
