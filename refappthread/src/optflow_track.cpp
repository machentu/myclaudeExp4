// ngd_app/optflow_track.cpp — TrackWithOpticalFlow orchestration (Tracking.cc:2996-3102).
//
// NGD OF mode: LK-propagate the last frame's stable MapPoints (Observations>=3)
// into the current (no-ORB) frame, mask-filter, cv::solvePnPRansac with init =
// velocity*last.pose, velocity-magnitude sanity reject. On success the current
// frame is populated with the inlier keys + MapPoints (+ depth via currTcw∘worldPos)
// so it can serve as the NEXT frame's LK source (mLastFrameLK) and support
// NeedNewKeyFrame's close-point stats. Sets s->tk.current->pose + mnMatchesInliers.
//
// `last_lk` is the OF LK source (mLastFrameLK = previous frame overall), NOT
// tk->last (which is the last KF-branch ORB frame, the TWM source). The two are
// distinct in NGD mode.
#include "ngd_app/optflow_track.h"
#include "ngd_shell/optflow.h"
#include "ngd/mlpnp.h"
#include "ngd/se3.h"

#include <opencv2/core.hpp>
#include <stdlib.h>
#include <math.h>

extern "C" int ngd_app_track_with_optical_flow(ngd_app_system *s, ngd_frame *last_lk,
                                                const uint8_t *last_gray, const uint8_t *curr_gray,
                                                const uint8_t *mask)
{
    ngd_tracking *tk = &s->tk;
    ngd_frame *cur = tk->current;
    if (!last_lk || !cur || last_lk->N <= 0) { tk->mnMatchesInliers = 0; return 0; }

    const int w = s->cam.imgW, h = s->cam.imgH;

    /* ---- collect last_lk's stable MPs (Observations>=3) + keypoints ---- */
    float *last_keys = (float *)malloc(sizeof(float) * 2 * (size_t)last_lk->N);
    float *mp_world   = (float *)malloc(sizeof(float) * 3 * (size_t)last_lk->N);
    int   *mp_valid   = (int *)malloc(sizeof(int) * (size_t)last_lk->N);
    int   *orig_idx   = (int *)malloc(sizeof(int) * (size_t)last_lk->N);   /* compacted -> last_lk index */
    int n_valid = 0;
    for (int i = 0; i < last_lk->N; ++i) {
        ngd_mappoint *mp = last_lk->mvpMapPoints[i];
        if (!mp || mp->mbBad) continue;
        /* Original SearchByOpticalFlow tracks ALL of mLastFrameLK's map points
         * (no Observations>=3 gate). That gate dropped many freshly-created-KF
         * points (stereo obs=2, not yet shared) -> fewer LK candidates -> lower
         * mnMatchesInliers -> c2 over-fires -> ORB ~every frame. Track all valid
         * points to match the original. */
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

    /* ---- SearchByOpticalFlow: LK propagate + mask drop ---- */
    float *out_keys = (float *)malloc(sizeof(float) * 2 * (size_t)n_valid);
    int   *out_mp_idx = (int *)malloc(sizeof(int) * (size_t)n_valid);
    int n_tracked = ngd_shell_search_by_optical_flow(last_gray, curr_gray, mask, w, h, w,
                                                     last_keys, mp_valid, n_valid,
                                                     out_keys, out_mp_idx);
    if (n_tracked < 20) {                                    /* cc:3001-3006 */
        tk->mnMatchesInliers = 0;
        free(last_keys); free(mp_world); free(mp_valid); free(orig_idx); free(out_keys); free(out_mp_idx);
        return 0;
    }

    /* ---- build (3D MapPoint, 2D tracked keypoint) correspondences ---- */
    ngd_vec3 *p3d = (ngd_vec3 *)malloc(sizeof(ngd_vec3) * (size_t)n_tracked);
    float *p2 = (float *)malloc(sizeof(float) * 2 * (size_t)n_tracked);
    float *sigma2 = (float *)malloc(sizeof(float) * (size_t)n_tracked);
    int   *trk_orig = (int *)malloc(sizeof(int) * (size_t)n_tracked);  /* last_lk index per tracked pt */
    float sig0 = s->cam.nlevels > 0 ? s->cam.mvLevelSigma2[0] : 1.0f;  /* OF points @ level 0 */
    for (int k = 0; k < n_tracked; ++k) {
        int mi = out_mp_idx[k];
        p3d[k].x = mp_world[3 * mi];
        p3d[k].y = mp_world[3 * mi + 1];
        p3d[k].z = mp_world[3 * mi + 2];
        p2[2 * k]     = out_keys[2 * k];
        p2[2 * k + 1] = out_keys[2 * k + 1];
        sigma2[k] = sig0;
        trk_orig[k]   = orig_idx[mi];
    }

    /* ---- init guess = vel_lk * last_lk.pose (cc:3028, mVelocityLK is the per-frame
     *      OF velocity, NOT the TWM velocity) — used as fallback if MLPnP fails. ---- */
    ngd_se3 initTcw = ngd_se3_multiply(s->vel_lk, last_lk->pose);

    /* ---- PnP + RANSAC (cc:3040). DEFAULT = cv::solvePnPRansac, the original's
     *      exact call (useExtrinsic=false, 5px, 100, 0.99, SOLVEPNP_ITERATIVE).
     *      Its RANSAC rejects bad LK tracks in-PnP, so no FB pre-filter is needed
     *      -> more points survive -> mnMatchesInliers stays high -> c2 rarely
     *      fires -> ORB skipped ~every other frame like the original (12% vs 99%).
     *      NGD_PNP=mlpnp selects the pure-C MLPnP solver instead (more stable
     *      *when FB is on*, but worse without FB — see BENCHMARK §9). ---- */
    int *inl = (int *)malloc(sizeof(int) * (size_t)n_tracked);
    int n_in = 0;
    ngd_se3 currTcw = initTcw;
    const char *pnp = getenv("NGD_PNP");
    if (pnp && (pnp[0]=='m' || pnp[0]=='M')) {
        ngd_mlpnp_ransac_params rp = ngd_mlpnp_ransac_defaults();
        rp.maxIterations = 100;   /* RANSAC converges by 100 (verified: 300 gives identical ATE) */
        int got = ngd_mlpnp_solve_ransac(p3d, p2, sigma2, n_tracked, &s->cam, &rp,
                                         &currTcw, inl, &n_in);
        if (!got || n_in <= 0) { currTcw = initTcw; n_in = 0; }
    } else {
        float K[9] = {s->cam.cam.fx, 0.0f, s->cam.cam.cx,
                      0.0f, s->cam.cam.fy, s->cam.cam.cy,
                      0.0f, 0.0f, 1.0f};
        float T16[16];
        int got = ngd_shell_pnp_ransac((const float *)p3d, p2, n_tracked, K, NULL, NULL,
                                       100, 5.0f, 0.99f, inl, T16);
        if (got > 0) { currTcw = ngd_se3_from_matrix4(T16); n_in = got; }
        else { currTcw = initTcw; n_in = 0; }
    }

    /* Count inliers at the original's 5px reprojection threshold
     * (cv::solvePnPRansac reprojectionError=5.0, Tracking.cc:3040). MLPnP's
     * internal inlier gate is ~2.45px (5.991*sigma2, sigma2=mvLevelSigma2[0]≈1),
     * which under-reports inliers vs the original -> c5 over-fires -> ORB
     * extracted ~every frame. Use the 5px count for mnMatchesInliers so c5/c2
     * gauge tracking quality on the same threshold as the original. Pose still
     * from MLPnP (more stable than solvePnPRansac, see BENCHMARK §8). */
    int n_in_5px = 0;
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
