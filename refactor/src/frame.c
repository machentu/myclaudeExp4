/* ngd/frame.c — Frame RGBD (pure C). */
#include "ngd/frame.h"
#include "ngd/mappoint.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Finalize an RGBD frame from an already-extracted (and possibly filtered)
 * keypoint/descriptor set: allocate the per-keypoint arrays, copy, run
 * ComputeStereoFromRGBD, set up the grid. Shared by build_rgbd and
 * build_rgbd_mask. f->mnId/cam/pose must already be set. Returns N (or 0). */
static int finalize_rgbd(ngd_frame *f, const ngd_cam_ctx *cam,
                         const ngd_keypoint *keys, const uint8_t *desc, int N,
                         const float *depth_meters, int dstride, int w, int h)
{
    if (N <= 0) { f->N = 0; return 0; }
    f->N = N;

    f->keys = (ngd_keypoint*)malloc(sizeof(ngd_keypoint) * (size_t)N);
    memcpy(f->keys, keys, sizeof(ngd_keypoint) * (size_t)N);
    /* UndistortKeyPoints: TUM3 has zero distortion → mvKeysUn = mvKeys. */
    f->descriptors = (uint8_t*)malloc((size_t)N * 32);
    memcpy(f->descriptors, desc, (size_t)N * 32);
    f->uRight = (float*)malloc(sizeof(float) * (size_t)N);
    f->depth  = (float*)malloc(sizeof(float) * (size_t)N);
    f->mvpMapPoints = (struct ngd_mappoint**)calloc((size_t)N, sizeof(void*));
    f->mvbOutlier   = (int*)calloc((size_t)N, sizeof(int));

    /* ComputeStereoFromRGBD (Frame.cc:1126-1147): sample depth at keypoint,
     * mvDepth=d, mvuRight = u_undistorted - mbf/d. */
    float mbf = cam->mbf;
    for (int i = 0; i < N; ++i) {
        int u = (int)lroundf(f->keys[i].x);
        int v = (int)lroundf(f->keys[i].y);
        if (u < 0) u = 0; if (u >= w) u = w-1;
        if (v < 0) v = 0; if (v >= h) v = h-1;
        float d = depth_meters[v*dstride + u];
        if (d > 0) {
            f->depth[i] = d;
            f->uRight[i] = f->keys[i].x - mbf / d;
        } else {
            f->depth[i] = -1.0f;
            f->uRight[i] = -1.0f;
        }
    }

    f->gridInvW = (float)NGD_KF_GRID_COLS / (float)cam->imgW;
    f->gridInvH = (float)NGD_KF_GRID_ROWS / (float)cam->imgH;
    ngd_frame_assign_grid(f);
    return N;
}

int ngd_frame_build_rgbd(ngd_frame *f, const ngd_orb_extractor *ex, const ngd_cam_ctx *cam,
                         const uint8_t *gray, int w, int h, int stride,
                         const float *depth_meters, int dstride,
                         uint64_t id,
                         ngd_keypoint *keybuf, int max_kps, uint8_t *descbuf)
{
    memset(f, 0, sizeof(*f));
    f->mnId = id;
    f->cam = *cam;
    f->pose = ngd_se3_identity();

    int N = ngd_orb_extract(ex, gray, w, h, stride, keybuf, max_kps, descbuf);
    return finalize_rgbd(f, cam, keybuf, descbuf, N, depth_meters, dstride, w, h);
}

/* NGD dynamic-mask Frame variant (Frame.cc:312-419): extract ORB, then drop
 * keypoints whose mask pixel != 0 (0 = static/keep, !=0 = dynamic/drop — same
 * convention as ngd_shell optflow). mask is uint8 w×h with stride == w; may be
 * NULL (== keep all, identical to ngd_frame_build_rgbd). Then the same finalize
 * (undistort no-op + ComputeStereoFromRGBD + grid). */
int ngd_frame_build_rgbd_mask(ngd_frame *f, const ngd_orb_extractor *ex, const ngd_cam_ctx *cam,
                              const uint8_t *gray, int w, int h, int stride,
                              const float *depth_meters, int dstride,
                              uint64_t id, const uint8_t *mask,
                              ngd_keypoint *keybuf, int max_kps, uint8_t *descbuf)
{
    memset(f, 0, sizeof(*f));
    f->mnId = id;
    f->cam = *cam;
    f->pose = ngd_se3_identity();

    int Nraw = ngd_orb_extract(ex, gray, w, h, stride, keybuf, max_kps, descbuf);
    if (Nraw <= 0) { f->N = 0; return 0; }

    /* Filter in-place: keep keypoints on mask==0 (Frame.cc:350-358). */
    int n = 0;
    for (int i = 0; i < Nraw; ++i) {
        int u = (int)lroundf(keybuf[i].x);
        int v = (int)lroundf(keybuf[i].y);
        if (u < 0) u = 0; if (u >= w) u = w-1;
        if (v < 0) v = 0; if (v >= h) v = h-1;
        if (mask && mask[v * w + u] != 0) continue;   /* dynamic → drop */
        if (n != i) {
            keybuf[n] = keybuf[i];
            memcpy(descbuf + (size_t)n * 32, descbuf + (size_t)i * 32, 32);
        }
        ++n;
    }
    return finalize_rgbd(f, cam, keybuf, descbuf, n, depth_meters, dstride, w, h);
}

void ngd_frame_free(ngd_frame *f) {
    if (!f) return;
    free(f->keys);
    free(f->descriptors);
    free(f->uRight);
    free(f->depth);
    free(f->mvpMapPoints);
    free(f->mvbOutlier);
    if (f->grid) {
        int ncells = NGD_KF_GRID_COLS * NGD_KF_GRID_ROWS;
        for (int i = 0; i < ncells; ++i) free(f->grid[i].idx);
        free(f->grid);
    }
    if (f->bow) { ngd_bowvec_free(f->bow); free(f->bow); }
    if (f->feat) { ngd_featvec_free(f->feat); free(f->feat); }
}

void ngd_frame_set_pose(ngd_frame *f, ngd_se3 pose) { f->pose = pose; }
ngd_se3 ngd_frame_get_pose(const ngd_frame *f) { return f->pose; }

void ngd_frame_compute_bow(ngd_frame *f, const ngd_bow_vocab *vocab) {
    if (f->feat) return;   /* lazy: already computed */
    f->bow = (ngd_bowvec*)calloc(1, sizeof(ngd_bowvec));
    f->feat = (ngd_featvec*)calloc(1, sizeof(ngd_featvec));
    ngd_bow_transform(vocab, f->descriptors, f->N, 4, f->bow, f->feat);
}
ngd_vec3 ngd_frame_get_camera_center(const ngd_frame *f) {
    /* Ow = -R^T t (camera centre in world). */
    ngd_mat3 R = ngd_quat_to_matrix(f->pose.q);
    ngd_mat3 Rt = ngd_m3_transpose(R);
    return ngd_v3_scale(ngd_m3_transform(Rt, f->pose.t), -1.0f);
}

int ngd_frame_is_in_frustum(ngd_frame *f, ngd_mappoint *mp, float viewingCosLimit) {
    /* Faithful port of Frame::isInFrustum (Frame.cc:651-716), mono/RGBD path
     * (Nleft == -1). Image bounds are [0,imgW]×[0,imgH] (zero distortion:
     * mnMinX/mnMinY=0, mnMaxX/mnMaxY=imgW/imgH). */
    mp->mbTrackInView = 0;
    mp->mTrackProjX = mp->mTrackProjY = mp->mTrackProjZ = -1.0f;       /* cc:654-657 */

    ngd_vec3 P = ngd_v3(mp->worldPos[0], mp->worldPos[1], mp->worldPos[2]);
    ngd_mat3 R = ngd_quat_to_matrix(f->pose.q);
    ngd_vec3 Pc = ngd_v3_add(ngd_m3_transform(R, P), f->pose.t);        /* mRcw*P + mtcw (cc:663) */
    float PcZ = Pc.z, Pc_dist = ngd_v3_norm(Pc);                        /* cc:664,667 */
    if (PcZ < 0.0f) return 0;                                           /* cc:669 */
    float invz = 1.0f / PcZ;

    float uv[2];
    ngd_pinhole_project(f->cam.cam, Pc, uv);                           /* cc:672 */
    if (uv[0] < 0.0f || uv[0] > (float)f->cam.imgW) return 0;          /* cc:674 */
    if (uv[1] < 0.0f || uv[1] > (float)f->cam.imgH) return 0;          /* cc:676 */
    mp->mTrackProjX = uv[0]; mp->mTrackProjY = uv[1];                  /* cc:679-680 */

    float maxD = ngd_mappoint_get_max_distance_invariance(mp);         /* cc:683 */
    float minD = ngd_mappoint_get_min_distance_invariance(mp);         /* cc:684 */
    ngd_vec3 Ow = ngd_frame_get_camera_center(f);
    ngd_vec3 PO = ngd_v3_sub(P, Ow); float dist = ngd_v3_norm(PO);      /* cc:685-686 */
    if (dist < minD || dist > maxD) return 0;                          /* cc:688 */

    ngd_vec3 Pn = ngd_v3(mp->normal[0], mp->normal[1], mp->normal[2]);
    float viewCos = ngd_v3_dot(PO, Pn) / dist;                         /* cc:694 */
    if (viewCos < viewingCosLimit) return 0;                           /* cc:696 */

    int nPredictedLevel = ngd_mappoint_predict_scale(mp, dist, &f->cam); /* cc:700 */

    mp->mbTrackInView = 1;                                              /* cc:703 */
    mp->mTrackProjX = uv[0]; mp->mTrackProjY = uv[1];                  /* cc:704 */
    mp->mTrackProjXR = uv[0] - f->cam.mbf * invz;                      /* cc:705 */
    mp->mTrackDepth = Pc_dist;                                         /* cc:707 */
    mp->mnTrackScaleLevel = nPredictedLevel;                           /* cc:710 */
    mp->mTrackViewCos = viewCos;                                       /* cc:711 */
    mp->mTrackProjZ = PcZ;                                             /* cc:713 */
    return 1;
}

ngd_vec3 ngd_frame_unproject_stereo(const ngd_frame *f, int i) {
    float z = f->depth[i];
    if (z <= 0) return ngd_v3(0,0,0);
    float u = f->keys[i].x, v = f->keys[i].y;
    float x = (u - f->cam.cam.cx) * z / f->cam.cam.fx;
    float y = (v - f->cam.cam.cy) * z / f->cam.cam.fy;
    ngd_vec3 Xc = ngd_v3(x, y, z);
    /* Xw = Twc * Xc (Twc = inverse of Tcw). Identity pose ⇒ Xw = Xc. */
    ngd_se3 Twc = ngd_se3_inverse(f->pose);
    return ngd_se3_map(Twc, Xc);
}

void ngd_frame_assign_grid(ngd_frame *f) {
    int ncells = NGD_KF_GRID_COLS * NGD_KF_GRID_ROWS;
    if (!f->grid) f->grid = (ngd_grid_cell*)calloc((size_t)ncells, sizeof(ngd_grid_cell));
    for (int i = 0; i < ncells; ++i) f->grid[i].n = 0;
    for (int i = 0; i < f->N; ++i) {
        int posX = (int)lroundf(f->keys[i].x * f->gridInvW);
        int posY = (int)lroundf(f->keys[i].y * f->gridInvH);
        if (posX < 0 || posX >= NGD_KF_GRID_COLS || posY < 0 || posY >= NGD_KF_GRID_ROWS) continue;
        ngd_grid_cell *cell = &f->grid[posX * NGD_KF_GRID_ROWS + posY];
        if (cell->n == cell->cap) {
            cell->cap = cell->cap ? cell->cap*2 : 4;
            cell->idx = (int*)realloc(cell->idx, (size_t)cell->cap * sizeof(int));
        }
        cell->idx[cell->n++] = i;
    }
}

int ngd_frame_get_features_in_area(const ngd_frame *f, float x, float y, float r,
                                   int minLevel, int maxLevel, int *out, int max_out)
{
    int nout = 0;
    int minX = (int)floorf((x - r) * f->gridInvW); if (minX < 0) minX = 0;
    if (minX >= NGD_KF_GRID_COLS) return 0;
    int maxX = (int)ceilf((x + r) * f->gridInvW); if (maxX >= NGD_KF_GRID_COLS) maxX = NGD_KF_GRID_COLS-1;
    int minY = (int)floorf((y - r) * f->gridInvH); if (minY < 0) minY = 0;
    if (minY >= NGD_KF_GRID_ROWS) return 0;
    int maxY = (int)ceilf((y + r) * f->gridInvH); if (maxY >= NGD_KF_GRID_ROWS) maxY = NGD_KF_GRID_ROWS-1;
    for (int cx = minX; cx <= maxX; ++cx)
        for (int cy = minY; cy <= maxY; ++cy) {
            const ngd_grid_cell *cell = &f->grid[cx * NGD_KF_GRID_ROWS + cy];
            for (int k = 0; k < cell->n; ++k) {
                int idx = cell->idx[k];
                const ngd_keypoint *kp = &f->keys[idx];
                if (minLevel >= 0 && kp->octave < minLevel) continue;
                if (maxLevel >= 0 && kp->octave > maxLevel) continue;
                if (fabsf(kp->x - x) >= r || fabsf(kp->y - y) >= r) continue;
                if (nout < max_out) out[nout++] = idx;
            }
        }
    return nout;
}
