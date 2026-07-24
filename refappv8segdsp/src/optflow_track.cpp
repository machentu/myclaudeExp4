// ngd_app/optflow_track.cpp — TrackWithOpticalFlow orchestration (Tracking.cc:2996-3102).
//
// NGD OF mode LK/PnP backends (compile-time #define, see block below):
//   NGD_OF_LK_C=1 (default) → pure-C float LK + EPnP (+ pose_opt refine), zero OpenCV
//   NGD_OF_LK_C=0           → OpenCV LK + cv::solvePnPRansac (NGD_LK_BACKEND unused)
// NGD_PNP: 0=EPnP (default), 1=mlpnp, 2=solvepnp(OpenCV) — pure-C branch only.
#include "ngd_app/optflow_track.h"
#include "ngd/optflow.h"
#include "ngd/pnp.h"
#include "ngd/mlpnp.h"
#include "ngd/se3.h"
#include "ngd/pose_opt.h"   /* pure-C LM refine for the pure-C PnP path */
#include "ngd_shell/optflow.h"   /* OpenCV fallback (NGD_LK_BACKEND=opencv) */
#include <stdlib.h>
#include <math.h>

/* NGD_OF_LK_C - OF tracking LK backend selector (compile-time define).
 *   1 = pure-C float LK + EPnP + pose_opt LM refine (1.82cm / median 1.24cm =
 *       OpenCV-equivalent, fully OpenCV-free) — DEFAULT.
 *   0 = OpenCV LK + solvePnPRansac (1.61cm); NGD_LK_BACKEND unused on this path.
 * The old pure-C PnP gap was closed by EPnP + pose_opt refine (BENCHMARK §10.6);
 * PnP solver is NGD_PNP. See BENCHMARK §10.5/§10.6. */
#ifndef NGD_OF_LK_C
#define NGD_OF_LK_C 0 //NGD_OF_LK_C 0 now NGD_LK_BACKEND=0,so use opencv LK + solvePnPRansac (1.61cm); NGD_LK_BACKEND unused on this path.
#endif

/* ===== ngd_app OF/PnP config (edit here + recompile; was env vars) ===== */
#ifndef NGD_LK_BACKEND
#define NGD_LK_BACKEND 0    /* 0=opencv, 1=u8, 2=float (only when NGD_OF_LK_C=0) */
#endif
#ifndef NGD_LK_OBS3
#define NGD_LK_OBS3 1        /* 1 = Observations>=3 gate (original SearchByOpticalFlow, 原版忠实) */
#endif
#ifndef NGD_MIN_EIG
#define NGD_MIN_EIG 0.0f     /* LK min eigenvalue; 0 = disabled */
#endif
#ifndef NGD_LK_FB
#define NGD_LK_FB 0.0f       /* pure-C LK FB consistency threshold (px); 0 = off */
#endif
#ifndef NGD_PNP
#define NGD_PNP 0            /* pure-C branch: 0=EPnP, 1=mlpnp, 2=solvepnp(OpenCV) */
#endif
#ifndef NGD_PNP_NOREFINE
#define NGD_PNP_NOREFINE 0   /* 1 = disable pure-C pose_opt LM refine */
#endif

extern "C" int ngd_app_track_with_optical_flow(ngd_app_system *s, ngd_frame *last_lk,
                                                const uint8_t *last_gray, const uint8_t *curr_gray,
                                                const uint8_t *mask)
{
    ngd_tracking *tk = &s->tk;
    ngd_frame *cur = tk->current;
    if (!last_lk || !cur || last_lk->N <= 0) { tk->mnMatchesInliers = 0; return 0; }

    const int w = s->cam.imgW, h = s->cam.imgH;

    /* ---- collect last_lk's MPs + keypoints ----
     * Default: track ALL valid (non-bad) points. The original SearchByOpticalFlow
     * (ORBmatcher.cc:2030) gates on Observations()>=3; we drop that gate because
     * it left too few LK candidates -> mnMatchesInliers low -> OF-mode c5
     * over-fired -> ORB ~every frame. Set NGD_LK_OBS3=1 to restore the original
     * obs>=3 gate (A/B vs original). */
    const bool obs3 = (NGD_LK_OBS3 != 0);
    float *last_keys = (float *)malloc(sizeof(float) * 2 * (size_t)last_lk->N);
    float *mp_world   = (float *)malloc(sizeof(float) * 3 * (size_t)last_lk->N);
    int   *mp_valid   = (int *)malloc(sizeof(int) * (size_t)last_lk->N);
    int   *orig_idx   = (int *)malloc(sizeof(int) * (size_t)last_lk->N);   /* compacted -> last_lk index */
    int n_valid = 0;
    for (int i = 0; i < last_lk->N; ++i) {
        ngd_mappoint *mp = last_lk->mvpMapPoints[i];
        if (!mp || mp->mbBad) continue;
        if (obs3 && mp->nObs < 3) continue;   /* original SearchByOpticalFlow gate (ORBmatcher.cc:2030) */
        last_keys[2 * n_valid]     = last_lk->keys[i].x;
        last_keys[2 * n_valid + 1] = last_lk->keys[i].y;
        mp_world[3 * n_valid]      = mp->worldPos[0];
        mp_world[3 * n_valid + 1]  = mp->worldPos[1];
        mp_world[3 * n_valid + 2]  = mp->worldPos[2];
        mp_valid[n_valid] = 1;
        orig_idx[n_valid] = i;
        ++n_valid;
    }
    if (n_valid < 20) { tk->mnMatchesInliers = 0; free(last_keys); free(mp_world); free(mp_valid); free(orig_idx); return 0; }

    /* ---- SearchByOpticalFlow: LK propagate + mask drop ----
     * NGD_OF_LK_C=1 (default) → pure-C float LK; =0 → OpenCV LK with NGD_LK_BACKEND
     *   (0=opencv/1=u8/2=float) selecting the sub-backend. PnP: OpenCV path →
     *   solvePnPRansac; pure-C path → NGD_PNP (0=EPnP default). */
#if NGD_OF_LK_C
    /* Compile-time: pure-C float LK (NGD_OF_LK_C=1). Pure-C LK matches OpenCV
     * accuracy (median 1.24cm fr3_walk); paired with EPnP + pose_opt refine
     * (NGD_PNP=0, see below) for a fully OpenCV-free path at equivalent median
     * ATE. Cost: ~2x slower than OpenCV LK (no SIMD). See BENCHMARK §10.5/§10.6. */
    int use_opencv = 0;
    int use_float  = 1;   /* float LK; u8 is marginally less precise */
#else
    int use_opencv = (NGD_LK_BACKEND == 0);
    int use_float  = (NGD_LK_BACKEND == 2);
#endif

    int n_tracked;
    int n_in_5px = 0;
    float *out_keys   = (float *)malloc(sizeof(float) * 2 * (size_t)n_valid);
    int   *out_mp_idx = (int   *)malloc(sizeof(int)         * (size_t)n_valid);

    if (use_opencv) {
        /* ---- OpenCV LK ---- */
        n_tracked = ngd_shell_search_by_optical_flow(last_gray, curr_gray, mask, w, h, w,
                                                     last_keys, mp_valid, n_valid,
                                                     out_keys, out_mp_idx);
    } else if (use_float) {
        ngd_lk_params fp = NGD_LK_DEFAULT_TRACKING;
        fp.min_eig   = NGD_MIN_EIG;
        fp.fb_thresh = NGD_LK_FB;
        n_tracked = ngd_optflow_search(last_gray, curr_gray, mask, w, h, w,
                                        last_keys, mp_valid, n_valid,
                                        out_keys, out_mp_idx, &fp);
    } else {
        ngd_lk_params up = NGD_LK_DEFAULT_TRACKING;
        up.min_eig   = NGD_MIN_EIG;
        up.fb_thresh = NGD_LK_FB;
        n_tracked = ngd_optflow_search_u8(last_gray, curr_gray, mask, w, h, w,
                                           last_keys, mp_valid, n_valid,
                                           out_keys, out_mp_idx, &up);
    }

    if (n_tracked < 20) {
        tk->mnMatchesInliers = 0;
        free(last_keys); free(mp_world); free(mp_valid); free(orig_idx); free(out_keys); free(out_mp_idx);
        return 0;
    }

    /* ---- build (3D MapPoint, 2D tracked keypoint) correspondences ---- */
    ngd_vec3 *p3d = (ngd_vec3 *)malloc(sizeof(ngd_vec3) * (size_t)n_tracked);
    float *p2 = (float *)malloc(sizeof(float) * 2 * (size_t)n_tracked);
    float *sigma2 = (float *)malloc(sizeof(float) * (size_t)n_tracked);
    int   *trk_orig = (int *)malloc(sizeof(int) * (size_t)n_tracked);
    float sig0 = s->cam.nlevels > 0 ? s->cam.mvLevelSigma2[0] : 1.0f;
    for (int k = 0; k < n_tracked; ++k) {
        int mi = out_mp_idx[k];
        p3d[k].x = mp_world[3 * mi]; p3d[k].y = mp_world[3 * mi + 1]; p3d[k].z = mp_world[3 * mi + 2];
        p2[2*k] = out_keys[2*k]; p2[2*k+1] = out_keys[2*k+1];
        sigma2[k] = sig0;
        trk_orig[k] = orig_idx[mi];
    }

    /* ---- init guess = vel_lk * last_lk.pose (mVelocityLK, cc:3028) ---- */
    ngd_se3 initTcw = ngd_se3_multiply(s->vel_lk, last_lk->pose);

    /* ---- PnP + RANSAC (cc:3040) ----
     * OpenCV backend  → cv::solvePnPRansac, useExtrinsic=false (exact original; initTcw is fallback-only)
     * Pure-C backends → NGD_PNP: 0=EPnP (default), 1=mlpnp, 2=solvepnp             */
    int *inl = (int *)malloc(sizeof(int) * (size_t)n_tracked);
    int n_in = 0;
    ngd_se3 currTcw = initTcw;

    if (use_opencv) {
        /* Exact original: cv::solvePnPRansac(useExtrinsic=false, 100, 5, 0.99, ITERATIVE) -- Tracking.cc:3040.
         * init_Tcw=NULL -> no seed; initTcw (vel_lk*last) is only the rejected-pose fallback. */
        float K[9] = {s->cam.cam.fx, 0.0f, s->cam.cam.cx,
                      0.0f, s->cam.cam.fy, s->cam.cam.cy,
                      0.0f, 0.0f, 1.0f};
        float T16[16];
        int got = ngd_shell_pnp_ransac((const float *)p3d, p2, n_tracked, K, NULL, NULL,
                                       100, 5.0f, 0.99f, inl, T16);
        if (got > 0) { currTcw = ngd_se3_from_matrix4(T16); n_in = got; }
        else         { currTcw = initTcw; n_in = 0; }
    } else {
        /* NGD_PNP (define): 0=EPnP(default), 1=mlpnp, 2=solvepnp(OpenCV, isolation) */
#if NGD_PNP == 1
        {
            ngd_mlpnp_ransac_params rp = ngd_mlpnp_ransac_defaults();
            rp.maxIterations = 100;
            int got = ngd_mlpnp_solve_ransac(p3d, p2, sigma2, n_tracked, &s->cam, &rp,
                                             &currTcw, inl, &n_in);
            if (!got || n_in <= 0) { currTcw = initTcw; n_in = 0; }
        }
#elif NGD_PNP == 2
        {   /* solvePnPRansac (OpenCV) — isolation: pure-C LK + OpenCV PnP */
            float K9[9] = {s->cam.cam.fx, 0.0f, s->cam.cam.cx,
                           0.0f, s->cam.cam.fy, s->cam.cam.cy,
                           0.0f, 0.0f, 1.0f};
            float T16[16];
            int got = ngd_shell_pnp_ransac((const float *)p3d, p2, n_tracked, K9, NULL, NULL,
                                           100, 5.0f, 0.99f, inl, T16);
            if (got > 0) { currTcw = ngd_se3_from_matrix4(T16); n_in = got; }
            else         { currTcw = initTcw; n_in = 0; }
        }
#else
        {
            ngd_pnp_ransac_params rp = ngd_pnp_ransac_defaults();
            rp.maxIterations = 100;
            rp.th2 = 25.0f;
            int got = ngd_pnp_solve_ransac(p3d, p2, sigma2, n_tracked, &s->cam, &rp,
                                           &currTcw, inl, &n_in);
            if (!got || n_in <= 0) { currTcw = initTcw; n_in = 0; }
        }
#endif

        /* Pure-C LM refine (PoseOptimization) on RANSAC inliers — the piece the
         * pure-C EPnP/MLPnP path was missing vs OpenCV solvePnPRansac (which LM-
         * refines in-PnP). Makes the pure-C PnP path accuracy-equivalent to
         * OpenCV with ZERO OpenCV calls. Skipped for solvepnp (already refined).
         * NGD_PNP_NOREFINE=1 to disable (A/B). See BENCHMARK §10.6. */
        if (n_in >= 10 && (NGD_PNP != 2) && !NGD_PNP_NOREFINE) {
            ngd_vec3 *rx = (ngd_vec3*)malloc(sizeof(ngd_vec3)*(size_t)n_in);
            float    *ro = (float*)malloc(sizeof(float)*3*(size_t)n_in);
            int      *rs = (int*)calloc((size_t)n_in, sizeof(int));
            int      *rc = (int*)calloc((size_t)n_in, sizeof(int));
            int m = 0;
            for (int k = 0; k < n_tracked; ++k) {
                if (!inl[k]) continue;
                rx[m] = p3d[k];
                ro[3*m] = p2[2*k]; ro[3*m+1] = p2[2*k+1]; ro[3*m+2] = 0.0f;
                ++m;
            }
            float invs2[1] = { 1.0f };
            ngd_pose_obs obs;
            obs.cam = s->cam.cam; obs.bf = 0.0f; obs.n = m;
            obs.Xw = rx; obs.obs = ro; obs.is_stereo = rs; obs.octave = rc;
            obs.invLevelSigma2 = invs2; obs.nlevels = 1;
            int *outl = (int*)calloc((size_t)m, sizeof(int));
            ngd_pose_optimization(&currTcw, &obs, outl);
            free(rx); free(ro); free(rs); free(rc); free(outl);
        }
    }

    /* 5px reprojection inlier count (original's threshold). */
    {
        float fx=s->cam.cam.fx, fy=s->cam.cam.fy, cx=s->cam.cam.cx, cy=s->cam.cam.cy;
        for(int k=0;k<n_tracked;++k){
            ngd_vec3 pc = ngd_se3_map(currTcw, p3d[k]);
            if(pc.z<=0.0f) continue;
            float ux=fx*pc.x/pc.z+cx, uy=fy*pc.y/pc.z+cy;
            float dx=ux-p2[2*k], dy=uy-p2[2*k+1];
            if(dx*dx+dy*dy < 25.0f) ++n_in_5px;
        }
    }

    /* ---- velocity-magnitude sanity (cc:3078-3092) + NaN guard ----
     * MLPnP can return a NaN pose on degenerate correspondences; NaN comparisons
     * are always false, so we explicitly guard and fall back to initTcw, or to
     * last_lk->pose if init is also NaN — never propagate NaN. */
    ngd_se3 currVelo = ngd_se3_multiply(currTcw, ngd_se3_inverse(last_lk->pose));
    float currMag = sqrtf(currVelo.t.x*currVelo.t.x + currVelo.t.y*currVelo.t.y + currVelo.t.z*currVelo.t.z);
    float lastMag = sqrtf(s->vel_lk.t.x*s->vel_lk.t.x + s->vel_lk.t.y*s->vel_lk.t.y + s->vel_lk.t.z*s->vel_lk.t.z);
    int curr_nan = !(currMag == currMag) || !(currTcw.t.x == currTcw.t.x) || !(currTcw.q.w == currTcw.q.w);
    int init_nan = !(initTcw.t.x == initTcw.t.x) || !(initTcw.q.w == initTcw.q.w);

    int rejected = curr_nan || (currMag > 3.0f * lastMag && currMag > 0.05f);
    ngd_se3 pose = rejected ? (init_nan ? last_lk->pose : initTcw) : currTcw;
    ngd_frame_set_pose(cur, pose);
    tk->mnMatchesInliers = rejected ? 0 : n_in_5px;   /* original's 5px threshold (was MLPnP ~2.45px) */

    /* ---- populate cur with inlier keys + MapPoints + depth (cc:3094-3096) so it
     *      can be the next frame's LK source and support NeedNewKeyFrame. ---- */
    if (!rejected && n_in > 0) {
        free(cur->keys); cur->keys = NULL;
        free(cur->mvpMapPoints); cur->mvpMapPoints = NULL;
        free(cur->mvbOutlier); cur->mvbOutlier = NULL;
        free(cur->depth); cur->depth = NULL;
        free(cur->uRight); cur->uRight = NULL;
        cur->keys = (ngd_keypoint*)malloc(sizeof(ngd_keypoint) * (size_t)n_in);
        cur->mvpMapPoints = (ngd_mappoint**)calloc((size_t)n_in, sizeof(ngd_mappoint*));
        cur->mvbOutlier = (int*)calloc((size_t)n_in, sizeof(int));
        cur->depth = (float*)malloc(sizeof(float) * (size_t)n_in);
        cur->uRight = (float*)malloc(sizeof(float) * (size_t)n_in);
        int m = 0;
        for (int k = 0; k < n_tracked; ++k) {
            if (!inl[k]) continue;
            ngd_vec3 pc = ngd_se3_map(currTcw, p3d[k]);   /* camera-frame point; .z = depth */
            cur->keys[m].x = p2[2*k]; cur->keys[m].y = p2[2*k+1];
            cur->keys[m].angle = 0.0f; cur->keys[m].response = 0.0f;
            cur->keys[m].octave = 0; cur->keys[m].size = 0.0f;
            cur->mvpMapPoints[m] = last_lk->mvpMapPoints[trk_orig[k]];
            cur->mvbOutlier[m] = 0;
            cur->depth[m] = pc.z;
            cur->uRight[m] = -1.0f;
            ++m;
        }
        cur->N = m;
    } else {
        cur->N = 0;   /* rejected: no inliers; pose kept as init guess */
    }

    free(last_keys); free(mp_world); free(mp_valid); free(orig_idx);
    free(out_keys); free(out_mp_idx);
    free(p3d); free(p2); free(sigma2); free(trk_orig); free(inl);

    return tk->mnMatchesInliers >= 20 ? 1 : 0;               /* cc:3101 */
}
