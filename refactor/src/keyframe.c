/* ngd/keyframe.c — KeyFrame minimal (pure C). */
#include "ngd/keyframe.h"
#include "ngd/mappoint.h"
#include "ngd/map.h"
#include "ngd/kfdb.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

ngd_keyframe *ngd_keyframe_new(uint64_t id, ngd_se3 pose, const ngd_cam_ctx *cam,
                                int N, const ngd_keypoint *keys,
                                const uint8_t *descriptors,
                                const float *uRight, const float *depth)
{
    ngd_keyframe *kf = (ngd_keyframe*)calloc(1, sizeof(ngd_keyframe));
    kf->mnId = id;
    kf->N = N;
    kf->cam = *cam;
    ngd_keyframe_set_pose(kf, pose);

    kf->keys = (ngd_keypoint*)malloc(sizeof(ngd_keypoint) * (size_t)(N > 0 ? N : 1));
    memcpy(kf->keys, keys, sizeof(ngd_keypoint) * (size_t)N);
    kf->descriptors = (uint8_t*)malloc((size_t)N * 32);
    memcpy(kf->descriptors, descriptors, (size_t)N * 32);
    kf->uRight = (float*)malloc(sizeof(float) * (size_t)(N > 0 ? N : 1));
    kf->depth  = (float*)malloc(sizeof(float) * (size_t)(N > 0 ? N : 1));
    memcpy(kf->uRight, uRight, sizeof(float) * (size_t)N);
    memcpy(kf->depth,  depth,  sizeof(float) * (size_t)N);
    kf->mvpMapPoints = (struct ngd_mappoint**)calloc((size_t)(N > 0 ? N : 1), sizeof(void*));

    kf->grid = NULL;
    kf->gridInvW = (float)NGD_KF_GRID_COLS / (float)cam->imgW;
    kf->gridInvH = (float)NGD_KF_GRID_ROWS / (float)cam->imgH;
    ngd_keyframe_assign_grid(kf);
    return kf;
}

void ngd_keyframe_free(ngd_keyframe *kf) {
    if (!kf) return;
    free(kf->keys);
    free(kf->descriptors);
    free(kf->uRight);
    free(kf->depth);
    free(kf->mvpMapPoints);
    if (kf->grid) {
        int ncells = NGD_KF_GRID_COLS * NGD_KF_GRID_ROWS;
        for (int i = 0; i < ncells; ++i) free(kf->grid[i].idx);
        free(kf->grid);
    }
    if (kf->bow) { ngd_bowvec_free(kf->bow); free(kf->bow); }
    if (kf->feat) { ngd_featvec_free(kf->feat); free(kf->feat); }
    free(kf->connKFs);
    free(kf->connWeights);
    free(kf);
}

void ngd_keyframe_set_pose(ngd_keyframe *kf, ngd_se3 pose) {
    kf->pose = pose;
    /* Ow = -R^T t (camera centre in world) */
    ngd_mat3 R = ngd_quat_to_matrix(pose.q);
    ngd_mat3 Rt = ngd_m3_transpose(R);
    kf->camCenter = ngd_v3_scale(ngd_m3_transform(Rt, pose.t), -1.0f);
}

ngd_vec3 ngd_keyframe_get_camera_center(const ngd_keyframe *kf) { return kf->camCenter; }

ngd_vec3 ngd_keyframe_unproject_stereo(const ngd_keyframe *kf, int i) {
    /* Frame::UnprojectStereo (Frame.cc:1043+): back-project a stereo/RGBD
     * keypoint at depth z into world space. z<=0 -> zero vector. */
    float z = kf->depth[i];
    if (z <= 0.0f) return ngd_v3(0,0,0);
    float u = kf->keys[i].x, v = kf->keys[i].y;
    float x = (u - kf->cam.cam.cx) * z / kf->cam.cam.fx;
    float y = (v - kf->cam.cam.cy) * z / kf->cam.cam.fy;
    ngd_vec3 Xc = ngd_v3(x, y, z);
    ngd_se3 Twc = ngd_se3_inverse(kf->pose);
    return ngd_se3_map(Twc, Xc);
}

void ngd_keyframe_get_descriptor(const ngd_keyframe *kf, int i, uint8_t out[32]) {
    memcpy(out, kf->descriptors + (size_t)i*32, 32);
}

void ngd_keyframe_compute_bow(ngd_keyframe *kf, const ngd_bow_vocab *vocab) {
    if (kf->feat) return;   /* lazy: already computed */
    kf->bow = (ngd_bowvec*)calloc(1, sizeof(ngd_bowvec));
    kf->feat = (ngd_featvec*)calloc(1, sizeof(ngd_featvec));
    ngd_bow_transform(vocab, kf->descriptors, kf->N, 4, kf->bow, kf->feat);
}

void ngd_keyframe_assign_grid(ngd_keyframe *kf) {
    int ncells = NGD_KF_GRID_COLS * NGD_KF_GRID_ROWS;
    if (!kf->grid) kf->grid = (ngd_grid_cell*)calloc((size_t)ncells, sizeof(ngd_grid_cell));
    for (int i = 0; i < ncells; ++i) { kf->grid[i].n = 0; }
    for (int i = 0; i < kf->N; ++i) {
        int col, row;
        if (!ngd_keyframe_pos_in_grid(kf, kf->keys[i].x, kf->keys[i].y, &col, &row)) continue;
        ngd_grid_cell *cell = &kf->grid[col * NGD_KF_GRID_ROWS + row];
        if (cell->n == cell->cap) {
            cell->cap = cell->cap ? cell->cap*2 : 4;
            cell->idx = (int*)realloc(cell->idx, (size_t)cell->cap * sizeof(int));
        }
        cell->idx[cell->n++] = i;
    }
}

int ngd_keyframe_pos_in_grid(const ngd_keyframe *kf, float x, float y, int *col, int *row) {
    int posX = (int)lroundf(x * kf->gridInvW);
    int posY = (int)lroundf(y * kf->gridInvH);
    if (posX < 0 || posX >= NGD_KF_GRID_COLS || posY < 0 || posY >= NGD_KF_GRID_ROWS) return 0;
    *col = posX; *row = posY;
    return 1;
}

int ngd_keyframe_get_features_in_area(const ngd_keyframe *kf, float x, float y, float r,
                                      int minLevel, int maxLevel, int *out, int max_out)
{
    int nout = 0;
    int minX = (int)floorf((x - r) * kf->gridInvW); if (minX < 0) minX = 0;
    if (minX >= NGD_KF_GRID_COLS) return 0;
    int maxX = (int)ceilf((x + r) * kf->gridInvW); if (maxX >= NGD_KF_GRID_COLS) maxX = NGD_KF_GRID_COLS-1;
    int minY = (int)floorf((y - r) * kf->gridInvH); if (minY < 0) minY = 0;
    if (minY >= NGD_KF_GRID_ROWS) return 0;
    int maxY = (int)ceilf((y + r) * kf->gridInvH); if (maxY >= NGD_KF_GRID_ROWS) maxY = NGD_KF_GRID_ROWS-1;

    for (int cx = minX; cx <= maxX; ++cx)
        for (int cy = minY; cy <= maxY; ++cy) {
            const ngd_grid_cell *cell = &kf->grid[cx * NGD_KF_GRID_ROWS + cy];
            for (int k = 0; k < cell->n; ++k) {
                int idx = cell->idx[k];
                const ngd_keypoint *kp = &kf->keys[idx];
                if (minLevel >= 0 && kp->octave < minLevel) continue;
                if (maxLevel >= 0 && kp->octave > maxLevel) continue;
                if (fabsf(kp->x - x) >= r || fabsf(kp->y - y) >= r) continue;
                if (nout < max_out) out[nout++] = idx;
            }
        }
    return nout;
}

/* ============================ covisibility graph ============================ */

/* Sort the parallel (connKFs, connWeights) arrays by weight DESCENDING. Small
 * N (covisibility degree), so an insertion sort is fine and stable enough. */
static void sort_conn_desc(ngd_keyframe *kf)
{
    for (int i = 1; i < kf->nConn; ++i) {
        int w = kf->connWeights[i];
        ngd_keyframe *k = kf->connKFs[i];
        int j = i - 1;
        while (j >= 0 && kf->connWeights[j] < w) {
            kf->connWeights[j + 1] = kf->connWeights[j];
            kf->connKFs[j + 1]     = kf->connKFs[j];
            --j;
        }
        kf->connWeights[j + 1] = w;
        kf->connKFs[j + 1]     = k;
    }
}

/* how many counter entries meet the threshold (KeyFrame.cc:419-447 fallback test) */
static int count_above(const int *ccnt, int n, int th)
{
    int c = 0;
    for (int k = 0; k < n; ++k) if (ccnt[k] >= th) ++c;
    return c;
}

void ngd_keyframe_add_connection(ngd_keyframe *kf, ngd_keyframe *nb, int weight)
{
    if (nb == kf) return;
    int idx = -1;
    for (int i = 0; i < kf->nConn; ++i) {
        if (kf->connKFs[i] == nb) { idx = i; break; }
    }
    if (idx >= 0) {
        if (kf->connWeights[idx] == weight) return;   /* unchanged */
        kf->connWeights[idx] = weight;
    } else {
        if (kf->nConn == kf->capConn) {
            int nc = kf->capConn ? kf->capConn * 2 : 8;
            kf->connKFs     = (ngd_keyframe**)realloc(kf->connKFs, (size_t)nc * sizeof(ngd_keyframe*));
            kf->connWeights = (int*)realloc(kf->connWeights, (size_t)nc * sizeof(int));
            kf->capConn = nc;
        }
        kf->connKFs[kf->nConn]     = nb;
        kf->connWeights[kf->nConn] = weight;
        kf->nConn++;
    }
    sort_conn_desc(kf);
}

void ngd_keyframe_update_connections(ngd_keyframe *kf)
{
    /* Collect a shared-MP counter over all observing KFs (KeyFrame.cc:390-411).
     * Use a growable array of {kf, count} since we have no hashmap. */
    ngd_keyframe **ckf = NULL;
    int *ccnt = NULL;
    int n = 0, cap = 0;

    for (int i = 0; i < kf->N; ++i) {
        ngd_mappoint *mp = kf->mvpMapPoints[i];
        if (!mp || mp->mbBad) continue;
        for (int o = 0; o < mp->nObsRec; ++o) {
            ngd_keyframe *o_kf = mp->obs[o].kf;
            if (o_kf == kf || o_kf->mnId == kf->mnId) continue;
            /* (skipping o_kf->isBad() / GetMap() filters — single map) */
            int idx = -1;
            for (int k = 0; k < n; ++k) if (ckf[k] == o_kf) { idx = k; break; }
            if (idx >= 0) {
                ccnt[idx]++;
            } else {
                if (n == cap) {
                    cap = cap ? cap * 2 : 16;
                    ckf = (ngd_keyframe**)realloc(ckf, (size_t)cap * sizeof(ngd_keyframe*));
                    ccnt = (int*)realloc(ccnt, (size_t)cap * sizeof(int));
                }
                ckf[n] = o_kf; ccnt[n] = 1; n++;
            }
        }
    }
    if (n == 0) { free(ckf); free(ccnt); return; }

    /* Threshold 15; keep max if none meet it (KeyFrame.cc:419-447). */
    const int th = 15;
    int nmax = 0, imax = -1;
    for (int k = 0; k < n; ++k) {
        if (ccnt[k] > nmax) { nmax = ccnt[k]; imax = k; }
    }
    int any_above = count_above(ccnt, n, th);

    /* Symmetric AddConnection on each retained neighbour. */
    for (int k = 0; k < n; ++k) {
        int keep = (ccnt[k] >= th) || (!any_above && k == imax);
        if (keep) ngd_keyframe_add_connection(ckf[k], kf, ccnt[k]);
    }

    /* Rebuild this KF's ordered list (descending) from the counter, keeping
     * only the retained entries (matches vPairs sort + push_front). */
    free(kf->connKFs); free(kf->connWeights);
    kf->connKFs = NULL; kf->connWeights = NULL; kf->nConn = 0; kf->capConn = 0;

    for (int k = 0; k < n; ++k) {
        int keep = (ccnt[k] >= th) || (!any_above && k == imax);
        if (!keep) continue;
        if (kf->nConn == kf->capConn) {
            int nc = kf->capConn ? kf->capConn * 2 : 8;
            kf->connKFs     = (ngd_keyframe**)realloc(kf->connKFs, (size_t)nc * sizeof(ngd_keyframe*));
            kf->connWeights = (int*)realloc(kf->connWeights, (size_t)nc * sizeof(int));
            kf->capConn = nc;
        }
        kf->connKFs[kf->nConn]     = ckf[k];
        kf->connWeights[kf->nConn] = ccnt[k];
        kf->nConn++;
    }
    sort_conn_desc(kf);

    free(ckf); free(ccnt);
}

int ngd_keyframe_get_best_covisibility_keyframes(const ngd_keyframe *kf, int N,
                                                 ngd_keyframe **out)
{
    if (N <= 0) return 0;
    int n = kf->nConn < N ? kf->nConn : N;
    for (int i = 0; i < n; ++i) out[i] = kf->connKFs[i];
    return n;
}

int ngd_keyframe_get_weight(const ngd_keyframe *kf, const ngd_keyframe *nb)
{
    for (int i = 0; i < kf->nConn; ++i)
        if (kf->connKFs[i] == nb) return kf->connWeights[i];
    return 0;
}

int ngd_keyframe_tracked_map_points(const ngd_keyframe *kf, int nMinObs)
{
    int n = 0;
    for (int i = 0; i < kf->N; ++i) {
        ngd_mappoint *mp = kf->mvpMapPoints[i];
        if (!mp || mp->mbBad) continue;
        if (ngd_mappoint_observations(mp) >= nMinObs) ++n;
    }
    return n;
}

int ngd_keyframe_is_bad(const ngd_keyframe *kf)
{
    return kf ? kf->mbBad : 0;
}

void ngd_keyframe_erase_connection(ngd_keyframe *kf, ngd_keyframe *target)
{
    /* KeyFrame.cc:687-701. Find target and shift-delete (preserves the
     * descending-weight sort; swap-pop would break ordering). */
    if (!kf) return;
    int idx = -1;
    for (int i = 0; i < kf->nConn; ++i)
        if (kf->connKFs[i] == target) { idx = i; break; }
    if (idx < 0) return;
    for (int i = idx; i < kf->nConn - 1; ++i) {
        kf->connKFs[i]     = kf->connKFs[i + 1];
        kf->connWeights[i] = kf->connWeights[i + 1];
    }
    kf->nConn--;
}

void ngd_keyframe_set_bad(ngd_keyframe *kf, ngd_map *map, ngd_kfdb *kfdb)
{
    /* KeyFrame.cc:573-679, non-inertial simplified (no spanning tree, no
     * mbNotErase). Guard init KF; trim covisibility edges symmetrically;
     * erase this KF's observations from its MPs (may set MPs bad); clear own
     * covisibility list; mark bad; erase from map + kfdb. */
    if (!kf || kf->mbBad) return;
    if (map && kf->mnId == map->mnInitKFid) return;

    /* Snapshot the neighbour list: clearing our own list below would
     * otherwise invalidate the iteration. */
    int nNb = kf->nConn;
    ngd_keyframe **nb = NULL;
    if (nNb > 0) {
        nb = (ngd_keyframe**)malloc((size_t)nNb * sizeof(ngd_keyframe*));
        memcpy(nb, kf->connKFs, (size_t)nNb * sizeof(ngd_keyframe*));
    }

    /* EraseConnection(this) on each neighbour (symmetric edge trim). */
    for (int i = 0; i < nNb; ++i)
        ngd_keyframe_erase_connection(nb[i], kf);
    free(nb);

    /* EraseObservation(this) on each MP we observe. */
    for (int i = 0; i < kf->N; ++i) {
        ngd_mappoint *mp = kf->mvpMapPoints[i];
        if (mp) ngd_mappoint_erase_observation(mp, kf);
    }

    /* Clear our own covisibility list. */
    free(kf->connKFs);
    free(kf->connWeights);
    kf->connKFs = NULL; kf->connWeights = NULL;
    kf->nConn = 0; kf->capConn = 0;

    kf->mbBad = 1;

    if (map) ngd_map_erase_keyframe(map, kf);
    if (kfdb) ngd_kfdb_erase(kfdb, kf);
}
