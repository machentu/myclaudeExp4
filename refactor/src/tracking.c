/* ngd/tracking.c — tracking front-end (pure C). */
#include "ngd/tracking.h"
#include "ngd/matcher.h"
#include "ngd/pose_opt.h"
#include "ngd/pnp.h"

#include <stdlib.h>
#include <string.h>

int ngd_stereo_initialization(ngd_frame *frame,
                              ngd_keyframe **out_kf,
                              ngd_mappoint ***out_mps, int *out_mps_cap)
{
    /* Tracking.cc:2460 — N > 500 gate (RGBD/stereo init). */
    if (frame->N <= 500) return -1;

    /* identity pose (Tracking.cc:2462). Xw == Xc. */
    ngd_frame_set_pose(frame, ngd_se3_identity());

    /* first KeyFrame, owning deep copies of the frame's arrays (2463-2464) */
    ngd_keyframe *kf = ngd_keyframe_new(0, frame->pose, &frame->cam, frame->N,
                                        frame->keys, frame->descriptors,
                                        frame->uRight, frame->depth);

    ngd_mappoint **mps = (ngd_mappoint**)malloc(sizeof(ngd_mappoint*) * (size_t)frame->N);
    int nmps = 0;

    for (int i = 0; i < frame->N; ++i) {
        if (frame->depth[i] <= 0) continue;            /* Tracking.cc:2465 */
        ngd_vec3 Xw = ngd_frame_unproject_stereo(frame, i);   /* == Xc (identity) */
        float pos[3] = { Xw.x, Xw.y, Xw.z };
        ngd_mappoint *mp = ngd_mappoint_new(pos, kf);          /* 2472 */
        int rightIdx = (frame->uRight[i] >= 0) ? i : -1;       /* stereo obs -> nObs += 2 */
        ngd_mappoint_add_observation(mp, kf, i, rightIdx);     /* 2474 */
        kf->mvpMapPoints[i] = mp;                              /* 2476 */
        frame->mvpMapPoints[i] = mp;                           /* 2478 */
        ngd_mappoint_compute_distinctive_descriptors(mp);      /* 2477 */
        ngd_mappoint_update_normal_and_depth(mp);               /* 2477 */
        mps[nmps++] = mp;
    }

    *out_kf = kf;
    *out_mps = mps;
    if (out_mps_cap) *out_mps_cap = nmps;
    return nmps;
}

/* ============================ ngd_tracking ============================ */

void ngd_tracking_init(ngd_tracking *tk)
{
    memset(tk, 0, sizeof(*tk));
    tk->velocity = ngd_se3_identity();
    tk->sensor = NGD_SENSOR_RGBD;
    tk->mState = NGD_STATE_OK;
    tk->mnLastRelocFrameId = 0;
    tk->mnLastKeyFrameId = 0;
    tk->mMinFrames = 0;
    tk->mbStartOpticalFlow = 0;
    tk->mMaxFrames = 60;
    tk->mbFarPoints = 0;
    tk->mThFarPoints = 50.0f;
}

void ngd_tracking_destroy(ngd_tracking *tk)
{
    free(tk->localKFs); free(tk->kfVotes);
    free(tk->localMPs);
    free(tk->po_Xw); free(tk->po_obs); free(tk->po_is_stereo);
    free(tk->po_octave); free(tk->po_frameidx); free(tk->po_outlier);
    memset(tk, 0, sizeof(*tk));
}

static void grow_local_kfs(ngd_tracking *tk, int need)
{
    int nc = tk->capLocalKFs ? tk->capLocalKFs : 8;
    while (nc < need) nc *= 2;
    tk->localKFs = (ngd_keyframe**)realloc(tk->localKFs, (size_t)nc * sizeof(ngd_keyframe*));
    tk->kfVotes   = (int*)realloc(tk->kfVotes, (size_t)nc * sizeof(int));
    tk->capLocalKFs = nc;
}
static void grow_local_mps(ngd_tracking *tk, int need)
{
    int nc = tk->capLocalMPs ? tk->capLocalMPs : 64;
    while (nc < need) nc *= 2;
    tk->localMPs = (ngd_mappoint**)realloc(tk->localMPs, (size_t)nc * sizeof(ngd_mappoint*));
    tk->capLocalMPs = nc;
}
static void grow_po(ngd_tracking *tk, int need)
{
    int nc = tk->po_cap ? tk->po_cap : 64;
    while (nc < need) nc *= 2;
    tk->po_Xw       = (ngd_vec3*)realloc(tk->po_Xw, (size_t)nc * sizeof(ngd_vec3));
    tk->po_obs      = (float*)realloc(tk->po_obs, (size_t)nc * 3 * sizeof(float));
    tk->po_is_stereo= (int*)realloc(tk->po_is_stereo, (size_t)nc * sizeof(int));
    tk->po_octave   = (int*)realloc(tk->po_octave, (size_t)nc * sizeof(int));
    tk->po_frameidx = (int*)realloc(tk->po_frameidx, (size_t)nc * sizeof(int));
    tk->po_outlier  = (int*)realloc(tk->po_outlier, (size_t)nc * sizeof(int));
    tk->po_cap = nc;
}

/* Collect matched MapPoints of `f` into the PoseOpt scratch, mirroring the
 * per-edge construction in Optimizer::PoseOptimization (Optimizer.cc:858-902):
 * mvuRight[i]<0 -> mono {u,v}; else stereo {u,v,uR}. Returns the edge count. */
static int collect_pose_obs(ngd_tracking *tk, ngd_frame *f)
{
    if (f->N > tk->po_cap) grow_po(tk, f->N);
    int n = 0;
    for (int i = 0; i < f->N; ++i) {
        ngd_mappoint *mp = f->mvpMapPoints[i];
        if (!mp) continue;
        int k = n;
        tk->po_Xw[k] = ngd_v3(mp->worldPos[0], mp->worldPos[1], mp->worldPos[2]);
        tk->po_obs[3*k + 0] = f->keys[i].x;
        tk->po_obs[3*k + 1] = f->keys[i].y;
        int stereo = (f->uRight[i] >= 0.0f);
        tk->po_is_stereo[k] = stereo;
        tk->po_obs[3*k + 2] = stereo ? f->uRight[i] : 0.0f;
        tk->po_octave[k]    = f->keys[i].octave;
        tk->po_frameidx[k] = i;
        n++;
    }
    return n;
}

/* Run PoseOptimization on `f`'s matched MapPoints; write back mvbOutlier by
 * frame index. Returns the inlier count. */
static int run_pose_opt(ngd_tracking *tk, ngd_frame *f)
{
    int n = collect_pose_obs(tk, f);
    if (n < 3) return 0;
    ngd_pose_obs o;
    o.cam = f->cam.cam; o.bf = f->cam.mbf; o.n = n;
    o.Xw = tk->po_Xw; o.obs = tk->po_obs; o.is_stereo = tk->po_is_stereo;
    o.octave = tk->po_octave; o.invLevelSigma2 = f->cam.mvInvLevelSigma2;
    o.nlevels = f->cam.nlevels;
    int ninliers = ngd_pose_optimization(&f->pose, &o, tk->po_outlier);
    for (int k = 0; k < n; ++k)
        f->mvbOutlier[tk->po_frameidx[k]] = tk->po_outlier[k];
    return ninliers;
}

int ngd_tracking_track_with_motion_model(ngd_tracking *tk)
{
    /* Tracking.cc:2901-2994 (non-IMU, non-onlyTracking). */
    ngd_frame *cur = tk->current;
    ngd_frame *last = tk->last;

    /* velocity model: Tcw_cur = V * Tcw_last (cc:2917). NGD: when mbStartOpticalFlow
     * the pose was already set by TrackWithOpticalFlow in GrabImageRGBD (cc:2915
     * `else if (!mbStartOpticalFlow)` guard) — skip the velocity seed so the OF
     * pose is refined (not overwritten) by SearchByProjection+PoseOptimization. */
    if (!tk->mbStartOpticalFlow)
        ngd_frame_set_pose(cur, ngd_se3_multiply(tk->velocity, last->pose));
    for (int i = 0; i < cur->N; ++i) cur->mvpMapPoints[i] = NULL;   /* cc:2923 */

    int bMono = (tk->sensor == NGD_SENSOR_MONOCULAR);
    int th = (tk->sensor == NGD_SENSOR_STEREO) ? 7 : 15;            /* cc:2929-2931 */
    int nmatches = ngd_search_by_projection_motion(cur, last, (float)th, bMono, 1);
    if (nmatches < 20) {                                            /* cc:2936 */
        for (int i = 0; i < cur->N; ++i) cur->mvpMapPoints[i] = NULL;
        nmatches = ngd_search_by_projection_motion(cur, last, (float)(2 * th), bMono, 1);
    }
    if (nmatches < 20) return 0;                                    /* cc:2944 (non-IMU) */

    run_pose_opt(tk, cur);                                          /* cc:2948 */

    int nmatchesMap = 0;
    for (int i = 0; i < cur->N; ++i) {
        ngd_mappoint *mp = cur->mvpMapPoints[i];
        if (!mp) continue;
        if (cur->mvbOutlier[i]) {                                   /* cc:2966 outlier */
            cur->mvpMapPoints[i] = NULL;
            cur->mvbOutlier[i] = 0;
            mp->mbTrackInView = 0;                                  /* cc:2973 */
            mp->mnLastFrameSeen = cur->mnId;                        /* cc:2974 */
            nmatches--;
        } else if (ngd_mappoint_observations(mp) > 0) {
            nmatchesMap++;
        }
    }
    return nmatchesMap >= 10;                                       /* cc:2994 (non-IMU) */
}

int ngd_tracking_track_reference_keyframe(ngd_tracking *tk)
{
    /* Tracking.cc:2767-2826 (non-IMU). */
    ngd_frame *cur = tk->current;
    ngd_keyframe *refKF = tk->refKF;

    ngd_frame_compute_bow(cur, tk->vocab);                          /* cc:2770 */
    if (!refKF->feat) ngd_keyframe_compute_bow(refKF, tk->vocab);

    /* ORBmatcher(0.7, true) -> nnratio=0.7, checkOri=1. Writes cur->mvpMapPoints. */
    int nmatches = ngd_search_by_bow(refKF, cur, cur->mvpMapPoints, 0.7f, 1); /* cc:2777 */
    if (nmatches < 15) return 0;                                    /* cc:2779 */

    ngd_frame_set_pose(cur, tk->last->pose);                        /* cc:2786 */
    run_pose_opt(tk, cur);                                          /* cc:2792 */

    int nmatchesMap = 0;
    for (int i = 0; i < cur->N; ++i) {
        ngd_mappoint *mp = cur->mvpMapPoints[i];
        if (!mp) continue;
        if (cur->mvbOutlier[i]) {                                   /* cc:2801 discard */
            cur->mvpMapPoints[i] = NULL;
            cur->mvbOutlier[i] = 0;
            mp->mbTrackInView = 0;                                  /* cc:2813 (left branch) */
            mp->mnLastFrameSeen = cur->mnId;                        /* cc:2814 */
            nmatches--;                                             /* cc:2815 */
        } else if (ngd_mappoint_observations(mp) > 0) {            /* cc:2817 */
            nmatchesMap++;
        }
    }
    return nmatchesMap >= 10;                                       /* cc:2825 (non-IMU) */
}

/* ---- UpdateLocalKeyFrames (cc:3605-3704), SIMPLIFIED: no covisibility-graph
 * neighbour / spanning-tree expansion — only the KFs observing the current
 * frame's matched MPs, deduped. mpReferenceKF = most-shared KF. ---- */
static void update_local_keyframes(ngd_tracking *tk)
{
    ngd_frame *cur = tk->current;
    tk->nLocalKFs = 0;
    tk->refKF = NULL;
    int maxV = 0;
    for (int i = 0; i < cur->N; ++i) {
        ngd_mappoint *mp = cur->mvpMapPoints[i];
        if (!mp) continue;
        if (mp->mbBad) { cur->mvpMapPoints[i] = NULL; continue; }    /* cc:3616-3625 */
        for (int j = 0; j < mp->nObsRec; ++j) {
            ngd_keyframe *kf = mp->obs[j].kf;
            if (!kf) continue;
            int idx = -1;
            for (int k = 0; k < tk->nLocalKFs; ++k)
                if (tk->localKFs[k] == kf) { idx = k; break; }
            if (idx < 0) {
                if (tk->nLocalKFs >= tk->capLocalKFs) grow_local_kfs(tk, tk->nLocalKFs + 1);
                idx = tk->nLocalKFs++;
                tk->localKFs[idx] = kf;
                tk->kfVotes[idx] = 0;
            }
            tk->kfVotes[idx]++;
            if (tk->kfVotes[idx] > maxV) { maxV = tk->kfVotes[idx]; tk->refKF = kf; }   /* cc:3669 */
        }
    }
    /* Covisibility-graph neighbour expansion (cc:3679-3704): for each observing
     * KF, add its best-10 covisibility neighbours. Spanning-tree parent/child
     * expansion (cc:3686-3704) is still skipped (parent not modelled). */
    int n0 = tk->nLocalKFs;
    for (int k = 0; k < n0; ++k) {
        ngd_keyframe *kf = tk->localKFs[k];
        ngd_keyframe *neigh[10];
        int nn = ngd_keyframe_get_best_covisibility_keyframes(kf, 10, neigh);
        for (int j = 0; j < nn; ++j) {
            ngd_keyframe *nb = neigh[j];
            int idx = -1;
            for (int m = 0; m < tk->nLocalKFs; ++m)
                if (tk->localKFs[m] == nb) { idx = m; break; }
            if (idx < 0) {
                if (tk->nLocalKFs >= tk->capLocalKFs) grow_local_kfs(tk, tk->nLocalKFs + 1);
                tk->localKFs[tk->nLocalKFs++] = nb;
            }
        }
    }
}

/* ---- UpdateLocalPoints (cc:3575-3602): reverse-iterate local KFs, collect
 * their non-bad MapPoints, dedup via mnTrackReferenceForFrame. ---- */
static void update_local_points(ngd_tracking *tk)
{
    ngd_frame *cur = tk->current;
    tk->nLocalMPs = 0;
    for (int k = tk->nLocalKFs - 1; k >= 0; --k) {                   /* cc:3581 reverse */
        ngd_keyframe *kf = tk->localKFs[k];
        for (int i = 0; i < kf->N; ++i) {
            ngd_mappoint *mp = kf->mvpMapPoints[i];
            if (!mp) continue;
            if (mp->mnTrackReferenceForFrame == cur->mnId) continue; /* cc:3592 dedup */
            if (mp->mbBad) continue;
            if (tk->nLocalMPs >= tk->capLocalMPs) grow_local_mps(tk, tk->nLocalMPs + 1);
            tk->localMPs[tk->nLocalMPs++] = mp;
            mp->mnTrackReferenceForFrame = cur->mnId;               /* cc:3598 */
        }
    }
}

void ngd_tracking_update_local_map(ngd_tracking *tk)
{
    update_local_keyframes(tk);
    update_local_points(tk);
}

void ngd_tracking_search_local_points(ngd_tracking *tk)
{
    /* Tracking.cc:3491-3563. */
    ngd_frame *cur = tk->current;

    /* (1) current frame's matched MPs: IncreaseVisible, stamp seen, clear in-view. */
    for (int i = 0; i < cur->N; ++i) {
        ngd_mappoint *mp = cur->mvpMapPoints[i];
        if (!mp) continue;
        if (mp->mbBad) { cur->mvpMapPoints[i] = NULL; continue; }
        ngd_mappoint_increase_visible(mp);
        mp->mnLastFrameSeen = cur->mnId;
        mp->mbTrackInView = 0;                                       /* cc:3509 */
    }

    /* (2) project local MPs not seen this frame. */
    int nToMatch = 0;
    for (int k = 0; k < tk->nLocalMPs; ++k) {
        ngd_mappoint *mp = tk->localMPs[k];
        if (mp->mnLastFrameSeen == cur->mnId) continue;              /* cc:3516 */
        if (mp->mbBad) continue;
        if (ngd_frame_is_in_frustum(cur, mp, 0.5f)) {                /* cc:3525 */
            ngd_mappoint_increase_visible(mp);
            nToMatch++;
        }
    }

    /* (3) match. th: RGBD=3 / mono=1; reloc-recent=5; LOST/RECENTLY_LOST=15. */
    if (nToMatch > 0) {
        int th = (tk->sensor == NGD_SENSOR_RGBD) ? 3 : 1;            /* cc:3550 */
        if (cur->mnId < tk->mnLastRelocFrameId + 2) th = 5;          /* cc:3554 */
        if (tk->mState == NGD_STATE_LOST || tk->mState == NGD_STATE_RECENTLY_LOST) th = 15; /* cc:3556 */
        ngd_search_by_projection_local(cur, tk->localMPs, tk->nLocalMPs,
                                      (float)th, tk->mbFarPoints, tk->mThFarPoints, 0.8f);
    }
}

int ngd_tracking_track_local_map(ngd_tracking *tk)
{
    /* Tracking.cc:3104-3204 (non-IMU, non-onlyTracking). */
    ngd_frame *cur = tk->current;

    ngd_tracking_update_local_map(tk);
    ngd_tracking_search_local_points(tk);
    run_pose_opt(tk, cur);                                           /* cc:3129 (non-IMU) */

    tk->mnMatchesInliers = 0;
    for (int i = 0; i < cur->N; ++i) {
        ngd_mappoint *mp = cur->mvpMapPoints[i];
        if (!mp) continue;
        if (!cur->mvbOutlier[i]) {
            ngd_mappoint_increase_found(mp);
            if (ngd_mappoint_observations(mp) > 0) tk->mnMatchesInliers++;   /* cc:3173 */
        } else if (tk->sensor == NGD_SENSOR_STEREO) {
            cur->mvpMapPoints[i] = NULL;                            /* cc:3192 stereo drops */
        }
    }

    /* success gate (cc:3185-3204, non-IMU). */
    if (cur->mnId < tk->mnLastRelocFrameId + (uint64_t)tk->mMaxFrames && tk->mnMatchesInliers < 50)
        return 0;                                                    /* cc:3185 reloc-recent */
    if (tk->mnMatchesInliers > 10 && tk->mState == NGD_STATE_RECENTLY_LOST)
        return 1;                                                    /* cc:3188 */
    return tk->mnMatchesInliers >= 30;                               /* cc:3204 (non-IMU) */
}

int ngd_tracking_need_new_keyframe(ngd_tracking *tk)
{
    /* Tracking.cc:3220-3360 (non-IMU core). */
    ngd_frame *cur = tk->current;
    ngd_keyframe *refKF = tk->refKF;
    if (!cur || !refKF || !tk->map) return 0;

    /* mbOnlyTracking -> false (no VO-only mode ported). LocalMapping idle is
     * always true (single-threaded inline). */

    int nKFs = tk->map->nKFs;

    /* reloc cooldown (cc:3243-3246): too soon after relocalization & map big. */
    if (cur->mnId < tk->mnLastRelocFrameId + (uint64_t)tk->mMaxFrames && nKFs > tk->mMaxFrames)
        return 0;

    /* nRefMatches = refKF->TrackedMapPoints(nMinObs) (cc:3249-3252). */
    int nMinObs = (nKFs <= 2) ? 2 : 3;
    int nRefMatches = ngd_keyframe_tracked_map_points(refKF, nMinObs);

    /* thRefRatio (cc:3282-3297). */
    float thRefRatio = (nKFs < 2) ? 0.4f
                    : (tk->sensor == NGD_SENSOR_MONOCULAR ? 0.9f : 0.75f);

    /* close-point statistics (cc:3258-3279), non-mono only. */
    int nTrackedClose = 0, nNonTrackedClose = 0;
    if (tk->sensor != NGD_SENSOR_MONOCULAR) {
        float thDepth = tk->current->cam.thDepth;
        for (int i = 0; i < cur->N; ++i) {
            float z = cur->depth[i];
            if (z <= 0 || z >= thDepth) continue;
            if (cur->mvpMapPoints[i] && ngd_mappoint_observations(cur->mvpMapPoints[i]) > 0)
                ++nTrackedClose;
            else
                ++nNonTrackedClose;
        }
    }
    int bNeedToInsertClose = (nTrackedClose < 100) && (nNonTrackedClose > 70);

    int mnMatchesInliers = tk->mnMatchesInliers;

    int c1a = cur->mnId >= tk->mnLastKeyFrameId + (uint64_t)tk->mMaxFrames;       /* cc:3300 */
    int c1b = cur->mnId >= tk->mnLastKeyFrameId + (uint64_t)tk->mMinFrames;        /* cc:3302 (idle always true) */
    int c1c = (tk->sensor != NGD_SENSOR_MONOCULAR) &&
              ((mnMatchesInliers < (int)(nRefMatches * 0.25f)) || bNeedToInsertClose);  /* cc:3304 */
    int c2 = ((mnMatchesInliers < (int)(nRefMatches * thRefRatio)) || bNeedToInsertClose) &&
             (mnMatchesInliers > 15);                                              /* cc:3306 */

    /* c5 (NGD, cc:3331): inert while mbStartOpticalFlow==0 (optical flow not ported). */
    int c5 = 0;
    if (tk->mbStartOpticalFlow) {
        c5 = (mnMatchesInliers < 20) ||
             ((mnMatchesInliers < 75) && (cur->mnId > tk->mnLastKeyFrameId + 5)) ||
             ((mnMatchesInliers < 300) && (cur->mnId > tk->mnLastKeyFrameId + 30));
    }

    return ((c1a || c1b || c1c) && c2) || c5;                                      /* cc:3333 */
}

int ngd_tracking_create_new_key_frame(ngd_tracking *tk)
{
    /* Tracking.cc:3363-3489 (non-IMU, non-fisheye). */
    ngd_frame *cur = tk->current;
    if (!cur || !tk->vocab || !tk->kfdb || !tk->map || !tk->lm) return 0;

    /* pKF = new KeyFrame(mCurrentFrame, ...) — deep copy of the frame arrays. */
    ngd_keyframe *kf = ngd_keyframe_new(cur->mnId, cur->pose, &cur->cam, cur->N,
                                        cur->keys, cur->descriptors,
                                        cur->uRight, cur->depth);

    /* Copy the frame's matched MapPoint slots into the new KeyFrame. Tracking
     * only records matches in mvpMapPoints; the observation itself is
     * committed below (and new MPs are created from RGB-D depth for NULL
     * slots, cc:3395-3480). */
    for (int i = 0; i < cur->N; ++i)
        kf->mvpMapPoints[i] = cur->mvpMapPoints[i];

    /* ProcessNewKeyFrame (LocalMapping.cc:315-345): register this KeyFrame as
     * an observer of every already-matched (existing) MapPoint. Without this,
     * tracked MPs never gain the new KF's observation -> nObs stays at the
     * creating KF's count -> TrackedMapPoints(nMinObs=3) collapses once
     * nKFs>2 -> NeedNewKeyFrame's c2 gate dies -> under-keyframing -> sparse
     * map -> tracking loss. Faithful to ORB-SLAM3; the refactor had dropped
     * this step. (Stale MPs with obs<1 are skipped here and replaced by the
     * depth loop below.) */
    for (int i = 0; i < cur->N; ++i) {
        ngd_mappoint *pMP = cur->mvpMapPoints[i];
        if (!pMP || pMP->mbBad) continue;
        if (ngd_mappoint_observations(pMP) < 1) continue;
        int rightIdx = (cur->uRight[i] >= 0) ? i : -1;
        ngd_mappoint_add_observation(pMP, kf, i, rightIdx);          /* cc:336 */
        ngd_mappoint_compute_distinctive_descriptors(pMP);           /* UpdateDescriptor */
    }

    if (tk->sensor != NGD_SENSOR_MONOCULAR) {
        /* sort feature indices by ascending depth (cc:3406-3420). */
        int *idx = (int*)malloc((size_t)cur->N * sizeof(int));
        int nDepth = 0;
        for (int i = 0; i < cur->N; ++i) if (cur->depth[i] > 0) idx[nDepth++] = i;
        /* simple insertion sort by depth */
        for (int a = 1; a < nDepth; ++a) {
            int key = idx[a]; float d = cur->depth[key]; int b = a - 1;
            while (b >= 0 && cur->depth[idx[b]] > d) { idx[b + 1] = idx[b]; --b; }
            idx[b + 1] = key;
        }

        const int maxPoint = 100;
        const float thDepth = cur->cam.thDepth;
        int nPoints = 0;
        for (int j = 0; j < nDepth; ++j) {
            int i = idx[j];
            float z = cur->depth[i];

            ngd_mappoint *pMP = cur->mvpMapPoints[i];
            int bCreateNew = 0;
            if (!pMP) bCreateNew = 1;
            else if (ngd_mappoint_observations(pMP) < 1) { bCreateNew = 1; cur->mvpMapPoints[i] = NULL; }

            if (bCreateNew) {
                ngd_vec3 Xw = ngd_frame_unproject_stereo(cur, i);   /* cc:3443 (Nleft==-1) */
                float pos[3] = { Xw.x, Xw.y, Xw.z };
                ngd_mappoint *pNewMP = ngd_mappoint_new(pos, kf);   /* mnFirstKFid = kf->mnId */
                int rightIdx = (cur->uRight[i] >= 0) ? i : -1;       /* stereo obs -> nObs += 2 */
                ngd_mappoint_add_observation(pNewMP, kf, i, rightIdx);
                kf->mvpMapPoints[i] = pNewMP;
                ngd_mappoint_compute_distinctive_descriptors(pNewMP);
                ngd_mappoint_update_normal_and_depth(pNewMP);
                ngd_map_add_mappoint(tk->map, pNewMP);
                ngd_local_mapping_push_recent(tk->lm, pNewMP);
                cur->mvpMapPoints[i] = pNewMP;
                nPoints++;
            } else {
                nPoints++;
            }
            if (z > thDepth && nPoints > maxPoint) break;            /* cc:3473 */
        }
        free(idx);
    }

    /* ComputeBoW + add to KFDB + covisibility (ProcessNewKeyFrame, cc:315,345). */
    ngd_keyframe_compute_bow(kf, tk->vocab);
    ngd_kfdb_add(tk->kfdb, kf);
    ngd_keyframe_update_connections(kf);
    ngd_map_add_keyframe(tk->map, kf);

    /* mpReferenceKF = pKF; mnLastKeyFrameId = mnId; mpLastKeyFrame = pKF (cc:3377,3487-3488). */
    tk->refKF = kf;
    tk->mnLastKeyFrameId = cur->mnId;
    tk->lm->currentKF = kf;
    return 1;
}

/* ============================ Relocalization ============================ */

/* Build EPnP input (p3d, p2d, sigma2) from the matched MapPoints in `matches`
 * (length cur->N, one MapPoint per frame feature or NULL). Returns the count
 * of usable correspondences. *idxMap[k] = the frame index of the k-th corr. */
static int build_pnp_input(ngd_frame *cur, ngd_mappoint **matches,
                           ngd_vec3 *p3d, float *p2d, float *sig2, int *idxMap)
{
    int n = 0;
    for (int i = 0; i < cur->N; ++i) {
        ngd_mappoint *mp = matches[i];
        if (!mp || mp->mbBad) continue;
        p3d[n] = ngd_v3(mp->worldPos[0], mp->worldPos[1], mp->worldPos[2]);
        p2d[2 * n] = cur->keys[i].x;
        p2d[2 * n + 1] = cur->keys[i].y;
        int oct = cur->keys[i].octave;
        sig2[n] = (oct >= 0 && oct < cur->cam.nlevels) ? cur->cam.mvLevelSigma2[oct] : 1.0f;
        idxMap[n] = i;
        ++n;
    }
    return n;
}

/* Single simplified refinement pass (cc:3872-3901): project the candidate KF's
 * MapPoints not yet matched into `cur` and run SearchByProjection (#1, ratio
 * 0.9, th=3), then re-run PoseOptimization. Returns the new nGood. */
static int refine_reloc(ngd_tracking *tk, ngd_frame *cur, ngd_keyframe *kf)
{
    /* collect kf MPs not already matched in cur, frustum-check them */
    int cap = 64, n = 0;
    ngd_mappoint **mps = (ngd_mappoint**)malloc((size_t)cap * sizeof(ngd_mappoint*));
    if (!mps) return -1;
    for (int i = 0; i < kf->N; ++i) {
        ngd_mappoint *mp = kf->mvpMapPoints[i];
        if (!mp || mp->mbBad) continue;
        /* skip if already a match in cur */
        int already = 0;
        for (int j = 0; j < cur->N; ++j) if (cur->mvpMapPoints[j] == mp) { already = 1; break; }
        if (already) continue;
        if (ngd_frame_is_in_frustum(cur, mp, 0.5f)) {
            if (n == cap) { cap *= 2; mps = (ngd_mappoint**)realloc(mps, (size_t)cap * sizeof(ngd_mappoint*)); }
            mps[n++] = mp;
        }
    }
    if (n > 0)
        ngd_search_by_projection_local(cur, mps, n, 3.0f, tk->mbFarPoints, tk->mThFarPoints, 0.9f);
    free(mps);
    return run_pose_opt(tk, cur);
}

int ngd_tracking_relocalization(ngd_tracking *tk)
{
    /* Tracking.cc:3757-3925 (non-IMU, left branch). */
    ngd_frame *cur = tk->current;
    if (!tk->kfdb || !tk->vocab) return 0;
    int bMatch = 0;

    ngd_frame_compute_bow(cur, tk->vocab);                            /* cc:3761 */

    ngd_keyframe **cands = NULL; int nCands = 0;
    if (ngd_kfdb_detect_reloc_candidates(tk->kfdb, cur, &cands, &nCands) != 0 || nCands == 0) {
        free(cands);                                                  /* cc:3767-3770 */
        return 0;
    }

    /* Per-candidate SearchByBoW matches (cc:3776-3810). ORBmatcher(0.75,true). */
    ngd_mappoint ***matches = (ngd_mappoint***)calloc((size_t)nCands, sizeof(ngd_mappoint**));
    int *discarded = (int*)calloc((size_t)nCands, sizeof(int));
    if (!matches || !discarded) { free(cands); free(matches); free(discarded); return 0; }

    int nCandidates = 0;
    for (int c = 0; c < nCands; ++c) {
        matches[c] = (ngd_mappoint**)calloc((size_t)cur->N, sizeof(ngd_mappoint*));
        if (!matches[c]) { discarded[c] = 1; continue; }
        int nm = ngd_search_by_bow(cands[c], cur, matches[c], 0.75f, 1);   /* cc:3793 */
        if (nm < 15) {                                                  /* cc:3797 */
            discarded[c] = 1;
            free(matches[c]); matches[c] = NULL;
        } else {
            ++nCandidates;
        }
    }
    if (nCandidates == 0) goto cleanup;

    /* PnP input scratch (max cur->N correspondences per candidate). */
    ngd_vec3 *p3d = (ngd_vec3*)malloc((size_t)cur->N * sizeof(ngd_vec3));
    float *p2d = (float*)malloc((size_t)cur->N * 2 * sizeof(float));
    float *sig2 = (float*)malloc((size_t)cur->N * sizeof(float));
    int *idxMap = (int*)malloc((size_t)cur->N * sizeof(int));
    int *inl = (int*)malloc((size_t)cur->N * sizeof(int));
    ngd_pnp_ransac_params rp = ngd_pnp_ransac_defaults();             /* cc:3805 */

    /* One full RANSAC pass per active candidate (ngd_pnp_solve_ransac runs the
     * full iteration budget internally, so the original's outer while-loop with
     * 5-iteration slices collapses to a single call per candidate). */
    for (int c = 0; c < nCands && !bMatch; ++c) {
        if (discarded[c]) continue;

        int M = build_pnp_input(cur, matches[c], p3d, p2d, sig2, idxMap);
        if (M < rp.minSet) { discarded[c] = 1; --nCandidates; continue; }

        ngd_se3 Tcw;
        int nIn = 0;
        int got = ngd_pnp_solve_ransac(p3d, p2d, sig2, M, &cur->cam, &rp,
                                       &Tcw, inl, &nIn);
        if (!got || nIn < rp.minInliers) continue;                     /* bNoMore / too few */

        ngd_frame_set_pose(cur, Tcw);                                  /* cc:3843 */
        for (int i = 0; i < cur->N; ++i) cur->mvpMapPoints[i] = NULL;  /* cc:3848 */
        for (int k = 0; k < M; ++k)
            if (inl[k]) cur->mvpMapPoints[idxMap[k]] = matches[c][idxMap[k]];

        int nGood = run_pose_opt(tk, cur);                             /* cc:3862 */
        if (nGood < 10) continue;                                      /* cc:3864 */

        /* clear outlier mvpMapPoints (cc:3867-3869) */
        for (int i = 0; i < cur->N; ++i)
            if (cur->mvbOutlier[i]) cur->mvpMapPoints[i] = NULL;

        if (nGood >= 50) { bMatch = 1; break; }                        /* cc:3905 */

        /* nGood<50 refinement chain (cc:3872-3901), simplified to one pass. */
        nGood = refine_reloc(tk, cur, cands[c]);
        if (nGood >= 50) { bMatch = 1; break; }
    }

    free(p3d); free(p2d); free(sig2); free(idxMap); free(inl);

cleanup:
    for (int c = 0; c < nCands; ++c) free(matches[c]);
    free(matches); free(discarded); free(cands);

    if (bMatch) {
        tk->mnLastRelocFrameId = cur->mnId;                            /* cc:3919 */
        return 1;
    }
    return 0;
}

/* ===================== Track() dispatcher (phase 1) ===================== */

int ngd_tracking_initialize(ngd_tracking *tk)
{
    ngd_frame *frame = tk->current;
    if (!frame) return 0;

    ngd_keyframe *kf = NULL;
    ngd_mappoint **mps = NULL; int nmps = 0;
    int r = ngd_stereo_initialization(frame, &kf, &mps, &nmps);
    if (r <= 0 || !kf) { free(mps); return 0; }

    /* Bookkeeping done inline by StereoInitialization + System init in the
     * original (Tracking.cc:2463-2492): wire KF/MPs into map + kfdb + lm. */
    ngd_keyframe_compute_bow(kf, tk->vocab);
    ngd_kfdb_add(tk->kfdb, kf);
    ngd_map_add_keyframe(tk->map, kf);
    for (int i = 0; i < nmps; ++i) {
        ngd_map_add_mappoint(tk->map, mps[i]);
        ngd_local_mapping_push_recent(tk->lm, mps[i]);
    }
    tk->lm->currentKF = kf;
    tk->refKF = kf;
    tk->mnLastKeyFrameId = frame->mnId;
    tk->mbVelocity = 0;
    tk->mbCreatedKF = 1;          /* init KF: driver may run lm/loopclosing */
    free(mps);                    /* pointer array only; objects owned by map */
    return 1;
}

int ngd_tracking_track(ngd_tracking *tk)
{
    tk->mbCreatedKF = 0;

    /* ---- NOT_INITIALIZED: first-frame stereo init (cc:2019-2042) ---- */
    if (tk->mState == NGD_STATE_NOT_INITIALIZED) {
        int r = ngd_tracking_initialize(tk);
        if (r) tk->mState = NGD_STATE_OK;
        return r ? 1 : 0;
    }

    /* ---- initialized: track frame (cc:2043-2455) ---- */
    int bOK = 0;

    if (tk->mState == NGD_STATE_OK) {
        /* CheckReplacedInLastFrame() skipped (no replaced-in-last helper). */
        if (!tk->mbVelocity || tk->current->mnId < tk->mnLastRelocFrameId + 2) {
            bOK = ngd_tracking_track_reference_keyframe(tk);           /* cc:2068 */
        } else {
            bOK = ngd_tracking_track_with_motion_model(tk);            /* cc:2073 */
            if (!bOK) bOK = ngd_tracking_track_reference_keyframe(tk); /* cc:2074-2075 */
        }
        if (!bOK) {                                                  /* cc:2079-2096 */
            if (tk->map->nKFs > 10) tk->mState = NGD_STATE_RECENTLY_LOST;
            else                     tk->mState = NGD_STATE_LOST;
        }
    } else {
        /* RECENTLY_LOST or LOST: Relocalization (non-IMU, cc:2122-2123) */
        bOK = ngd_tracking_relocalization(tk);
        if (!bOK && tk->mState == NGD_STATE_RECENTLY_LOST) {
            /* timeout -> LOST (original uses 3.0s; frame-count heuristic). */
            if (tk->current->mnId > tk->mnLastRelocFrameId + (uint64_t)tk->mMaxFrames * 2)
                tk->mState = NGD_STATE_LOST;
        }
        if (tk->mState == NGD_STATE_LOST && tk->map->nKFs <= 10) {
            /* few KFs: original resets the map (ResetActiveMap); phase-1 reports lost. */
            return 0;
        }
    }

    /* TrackLocalMap (cc:2247). */
    if (bOK) bOK = ngd_tracking_track_local_map(tk);

    /* state update (cc:2262-2278). */
    if (bOK) tk->mState = NGD_STATE_OK;
    else if (tk->mState == NGD_STATE_OK) tk->mState = NGD_STATE_RECENTLY_LOST;

    /* motion model (cc:2325-2336). */
    if ((bOK || tk->mState == NGD_STATE_RECENTLY_LOST) && tk->last) {
        tk->velocity = ngd_se3_multiply(tk->current->pose,
                                        ngd_se3_inverse(tk->last->pose));
        tk->mbVelocity = 1;
    }

    /* NeedNewKeyFrame / CreateNewKeyFrame (cc:2368-2373). */
    if (bOK && ngd_tracking_need_new_keyframe(tk)) {
        if (ngd_tracking_create_new_key_frame(tk)) tk->mbCreatedKF = 1;
    }

    return bOK ? 1 : 0;
}
