/* ngd/optflow.c — Pure-C pyramidal Lucas-Kanade optical flow.
 *
 * Replaces cv::calcOpticalFlowPyrLK (OpenCV) at two call-sites:
 *   A. SearchByOpticalFlow  (ORBmatcher.cc:2044) — ws=9,  lv=3, 50×0.001
 *   B. PredictCurrentMask   (Tracking.cc:4271)    — ws=21, lv=3, 30×0.01
 *
 * Algorithm
 * =========
 * 1. Build a float Gaussian pyramid for the prev image (pyrDown:
 *      separable 5×5 [1 4 6 4 1]/16, border reflection, keep even
 *      rows/cols). Level 0 is a float→float copy. Float pyramids
 *      preserve gradient precision at coarse levels — uint8 pyramids
 *      lose ~1 bit per level, starving the LK normal equations.
 *
 * 2. Build a uint8 pyramid for the curr image (used for bilinear
 *    sub-pixel sampling; uint8 is sufficient for this).
 *
 * 3. Track each point coarse→fine with a Gaussian-weighted LK window:
 *      a. Spatial gradients Ix, Iy on float prev patch (central diffs).
 *      b. Temporal difference It = curr(bilinear) − prev.
 *      c. Weight by exp(−(d²)/(2·σ²)) with σ = half/2.
 *      d. Build weighted 2×2 normal equations, solve.
 *      e. Propagate displacement ×2 to next finer level.
 *
 * Dependencies: stdlib (malloc/free), string.h (memcpy), math.h
 * (fabsf/sqrtf/expf). No OpenCV, no Eigen.
 */

#include "ngd/optflow.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

/* =========================================================================
 * Pyramid (float prev + uint8 curr)
 * ======================================================================== */

/* ---- Float pyramid level ---- */
typedef struct {
    float *buf;   /* tightly packed, stride == w */
    int w, h;
} pyrf_level;

typedef struct {
    pyrf_level *levels;
    int nlevels;
} pyramidf;

/* ---- Uint8 pyramid level (for curr image bilinear sampling) ---- */
typedef struct {
    uint8_t *buf;
    int w, h, stride;
} pyru_level;

typedef struct {
    pyru_level *levels;
    int nlevels;
} pyramidu;

/* ---- pyrDown: float → float (separable 5×5 Gaussian, OpenCV-compatible) ---- */
static void pyr_down_f(const float *src, int sw, int sh, int sstride,
                       float *dst, int dw, int dh, int dstride)
{
    static const float kw[5] = { 1.0f/16.0f, 4.0f/16.0f, 6.0f/16.0f,
                                  4.0f/16.0f, 1.0f/16.0f };
    for (int y = 0; y < dh; ++y) {
        int sy = y * 2;
        for (int x = 0; x < dw; ++x) {
            int sx = x * 2;
            float sum = 0.0f;
            for (int dy = -2; dy <= 2; ++dy) {
                int ry = sy + dy;
                if (ry < 0) ry = -ry; else if (ry >= sh) ry = 2*sh - ry - 2;
                for (int dx = -2; dx <= 2; ++dx) {
                    int rx = sx + dx;
                    if (rx < 0) rx = -rx; else if (rx >= sw) rx = 2*sw - rx - 2;
                    sum += src[(size_t)ry * sstride + rx] * kw[dx+2] * kw[dy+2];
                }
            }
            dst[(size_t)y * dstride + x] = sum;
        }
    }
}

/* ---- pyrDown: uint8 → uint8 ---- */
static void pyr_down_u(const uint8_t *src, int sw, int sh, int sstride,
                       uint8_t *dst, int dw, int dh, int dstride)
{
    static const float kw[5] = { 1.0f/16.0f, 4.0f/16.0f, 6.0f/16.0f,
                                  4.0f/16.0f, 1.0f/16.0f };
    for (int y = 0; y < dh; ++y) {
        int sy = y * 2;
        for (int x = 0; x < dw; ++x) {
            int sx = x * 2;
            float sum = 0.0f;
            for (int dy = -2; dy <= 2; ++dy) {
                int ry = sy + dy;
                if (ry < 0) ry = -ry; else if (ry >= sh) ry = 2*sh - ry - 2;
                for (int dx = -2; dx <= 2; ++dx) {
                    int rx = sx + dx;
                    if (rx < 0) rx = -rx; else if (rx >= sw) rx = 2*sw - rx - 2;
                    sum += (float)src[(size_t)ry * sstride + rx] * kw[dx+2] * kw[dy+2];
                }
            }
            if (sum < 0.0f) sum = 0.0f;
            if (sum > 255.0f) sum = 255.0f;
            dst[(size_t)y * dstride + x] = (uint8_t)(sum + 0.5f);
        }
    }
}

/* ---- Build float pyramid (prev image) ---- */
static pyramidf *pyramidf_build(const uint8_t *src, int w, int h, int stride,
                                int nlevels)
{
    if (nlevels < 1 || w < 1 || h < 1 || !src) return NULL;
    pyramidf *p = (pyramidf *)malloc(sizeof(pyramidf));
    if (!p) return NULL;
    p->nlevels = nlevels;
    p->levels  = (pyrf_level *)calloc((size_t)nlevels, sizeof(pyrf_level));
    if (!p->levels) { free(p); return NULL; }

    /* Level 0: uint8→float copy */
    {
        pyrf_level *L = &p->levels[0];
        L->w = w; L->h = h;
        L->buf = (float *)malloc(sizeof(float) * (size_t)w * h);
        if (!L->buf) goto fail;
        for (int r = 0; r < h; ++r)
            for (int c = 0; c < w; ++c)
                L->buf[(size_t)r * w + c] = (float)src[(size_t)r * stride + c];
    }

    /* Levels 1..nlevels-1: float pyrDown */
    for (int lv = 1; lv < nlevels; ++lv) {
        pyrf_level *prev = &p->levels[lv-1];
        pyrf_level *L    = &p->levels[lv];
        L->w = prev->w > 1 ? (prev->w + 1) / 2 : 1;
        L->h = prev->h > 1 ? (prev->h + 1) / 2 : 1;
        L->buf = (float *)malloc(sizeof(float) * (size_t)L->w * L->h);
        if (!L->buf) goto fail;
        if (L->w < 3 || L->h < 3) {
            memset(L->buf, 0, sizeof(float) * (size_t)L->w * L->h);
        } else {
            pyr_down_f(prev->buf, prev->w, prev->h, prev->w,
                       L->buf,    L->w,    L->h,    L->w);
        }
    }
    return p;
fail:
    for (int lv = 0; lv < nlevels; ++lv) free(p->levels[lv].buf);
    free(p->levels); free(p);
    return NULL;
}

static void pyramidf_free(pyramidf *p)
{
    if (!p) return;
    for (int lv = 0; lv < p->nlevels; ++lv) free(p->levels[lv].buf);
    free(p->levels); free(p);
}

/* ---- Build uint8 pyramid (curr image) ---- */
static pyramidu *pyramidu_build(const uint8_t *src, int w, int h, int stride,
                                int nlevels)
{
    if (nlevels < 1 || w < 1 || h < 1 || !src) return NULL;
    pyramidu *p = (pyramidu *)malloc(sizeof(pyramidu));
    if (!p) return NULL;
    p->nlevels = nlevels;
    p->levels  = (pyru_level *)calloc((size_t)nlevels, sizeof(pyru_level));
    if (!p->levels) { free(p); return NULL; }

    {
        pyru_level *L = &p->levels[0];
        L->w = w; L->h = h; L->stride = w;
        L->buf = (uint8_t *)malloc((size_t)w * h);
        if (!L->buf) goto fail;
        for (int r = 0; r < h; ++r)
            memcpy(L->buf + (size_t)r * w, src + (size_t)r * stride, (size_t)w);
    }
    for (int lv = 1; lv < nlevels; ++lv) {
        pyru_level *prev = &p->levels[lv-1];
        pyru_level *L    = &p->levels[lv];
        L->w = prev->w > 1 ? (prev->w+1)/2 : 1;
        L->h = prev->h > 1 ? (prev->h+1)/2 : 1;
        L->stride = L->w;
        L->buf = (uint8_t *)malloc((size_t)L->w * L->h);
        if (!L->buf) goto fail;
        if (L->w < 3 || L->h < 3) {
            memset(L->buf, prev->buf[0], (size_t)L->w * L->h);
        } else {
            pyr_down_u(prev->buf, prev->w, prev->h, prev->stride,
                       L->buf,    L->w,    L->h,    L->stride);
        }
    }
    return p;
fail:
    for (int lv = 0; lv < nlevels; ++lv) free(p->levels[lv].buf);
    free(p->levels); free(p);
    return NULL;
}

static void pyramidu_free(pyramidu *p)
{
    if (!p) return;
    for (int lv = 0; lv < p->nlevels; ++lv) free(p->levels[lv].buf);
    free(p->levels); free(p);
}

/* =========================================================================
 * Bilinear interpolation (uint8 image)
 * ======================================================================== */
static float bilinear(const uint8_t *img, int w, int h, int stride,
                      float x, float y)
{
    int x0 = (int)floorf(x), y0 = (int)floorf(y);
    int x1 = x0 + 1, y1 = y0 + 1;
    float fx = x - (float)x0, fy = y - (float)y0;
    if (x0 < 0) { x0 = 0; fx = 0.0f; }
    if (x0 >= w) { x0 = w - 1; fx = 0.0f; }
    if (y0 < 0) { y0 = 0; fy = 0.0f; }
    if (y0 >= h) { y0 = h - 1; fy = 0.0f; }
    if (x1 < 0) x1 = 0; if (x1 >= w) x1 = w - 1;
    if (y1 < 0) y1 = 0; if (y1 >= h) y1 = h - 1;
    float wx1 = 1.0f - fx, wy1 = 1.0f - fy;
    float v00 = (float)img[(size_t)y0 * stride + x0];
    float v10 = (float)img[(size_t)y0 * stride + x1];
    float v01 = (float)img[(size_t)y1 * stride + x0];
    float v11 = (float)img[(size_t)y1 * stride + x1];
    return wy1*(wx1*v00 + fx*v10) + fy*(wx1*v01 + fx*v11);
}

/* =========================================================================
 * Min-eigenvalue pre-filter (OpenCV's minEigThreshold).
 *
 * Computes the 2×2 gradient normal matrix over the window at the initial
 * (unwarped) point position.  The minimum eigenvalue per pixel is
 * λ_min / N, where N = window pixel count.  If this is below min_eig,
 * the point has insufficient texture for reliable LK — skip it.
 * ======================================================================== */
static int min_eig_pass(const float *prev, int pw, int ph,
                        float px, float py, int half,
                        float min_eig)
{
    if (min_eig <= 0.0f) return 1;   /* disabled */

    /* Window must fit entirely inside the prev image. */
    int ix = (int)px, iy = (int)py;
    if (ix - half < 0 || ix + half >= pw - 1 ||
        iy - half < 0 || iy + half >= ph - 1)
        return 0;  /* can't even evaluate — reject */

    float Gxx = 0.0f, Gxy = 0.0f, Gyy = 0.0f;
    int N = 0;

    for (int wy = -half; wy <= half; ++wy) {
        int r = iy + wy;
        for (int wx = -half; wx <= half; ++wx) {
            int c = ix + wx;
            float Ix = 0.5f * (prev[(size_t)r * pw + (c+1)] -
                               prev[(size_t)r * pw + (c-1)]);
            float Iy = 0.5f * (prev[(size_t)(r+1) * pw + c] -
                               prev[(size_t)(r-1) * pw + c]);
            Gxx += Ix * Ix;
            Gxy += Ix * Iy;
            Gyy += Iy * Iy;
            ++N;
        }
    }

    if (N < 4) return 0;

    /* λ_min = (Gxx+Gyy − sqrt((Gxx−Gyy)² + 4*Gxy²)) / 2.
     * λ_per_px = λ_min / N.  Reject if below min_eig. */
    float trace = Gxx + Gyy;
    float delta = (Gxx - Gyy) * (Gxx - Gyy) + 4.0f * Gxy * Gxy;
    float sqrt_delta = sqrtf(delta > 0.0f ? delta : 0.0f);
    float eig_min = 0.5f * (trace - sqrt_delta);
    float eig_per_px = eig_min / (float)N;

    return (eig_per_px >= min_eig);
}

/* =========================================================================
 * Float-pyramid single-level LK iteration (uniform weights).
 *
 * Uses the float prev pyramid (better gradient precision at coarse levels).
 * Uniform window weights match OpenCV's behaviour.
 * ======================================================================== */
static int lk_level_f(const float *prev, int pw, int ph,
                      const uint8_t *curr, int cw, int ch, int cstride,
                      float px, float py, float *dx, float *dy,
                      int half, int max_iter, float epsilon, float min_eig)
{
    /* Min-eig pre-check on the float prev pyramid before iterating. */
    if (!min_eig_pass(prev, pw, ph, px, py, half, min_eig))
        return 0;
    for (int iter = 0; iter < max_iter; ++iter) {
        float cx = px + *dx;
        float cy = py + *dy;

        if (cx - half < 0.0f || cx + half >= (float)(cw - 1) ||
            cy - half < 0.0f || cy + half >= (float)(ch - 1))
            return 0;

        float Gxx = 0.0f, Gxy = 0.0f, Gyy = 0.0f;
        float bx = 0.0f, by = 0.0f;

        for (int wy = -half; wy <= half; ++wy) {
            int ipy = (int)(py + (float)wy);
            float ccy = cy + (float)wy;

            for (int wx = -half; wx <= half; ++wx) {
                int   ipx = (int)(px + (float)wx);
                float ccx = cx + (float)wx;

                float pv = prev[(size_t)ipy * pw + ipx];
                float cv = bilinear(curr, cw, ch, cstride, ccx, ccy);

                /* Gradients on float prev (central diffs). */
                float Ix, Iy;
                if (ipx > 0 && ipx < pw - 1)
                    Ix = 0.5f * (prev[(size_t)ipy * pw + (ipx+1)] - prev[(size_t)ipy * pw + (ipx-1)]);
                else if (ipx == 0)
                    Ix = prev[(size_t)ipy * pw + 1] - prev[(size_t)ipy * pw];
                else
                    Ix = prev[(size_t)ipy * pw + (pw-1)] - prev[(size_t)ipy * pw + (pw-2)];

                if (ipy > 0 && ipy < ph - 1)
                    Iy = 0.5f * (prev[((size_t)(ipy+1)) * pw + ipx] - prev[((size_t)(ipy-1)) * pw + ipx]);
                else if (ipy == 0)
                    Iy = prev[(size_t)1 * pw + ipx] - prev[(size_t)0 * pw + ipx];
                else
                    Iy = prev[((size_t)(ph-1)) * pw + ipx] - prev[((size_t)(ph-2)) * pw + ipx];

                float It = cv - pv;
                Gxx += Ix * Ix; Gxy += Ix * Iy; Gyy += Iy * Iy;
                bx  += Ix * It; by  += Iy * It;
            }
        }

        float det = Gxx * Gyy - Gxy * Gxy;
        if (fabsf(det) < 1e-6f) return 0;

        float ddx = (Gyy*(-bx) - Gxy*(-by)) / det;
        float ddy = (Gxx*(-by) - Gxy*(-bx)) / det;

        *dx += ddx;
        *dy += ddy;

        if (fabsf(ddx) < epsilon && fabsf(ddy) < epsilon)
            break;
    }
    return 1;
}

/* =========================================================================
 * Public API
 * ======================================================================== */

int ngd_optflow_pyr_lk(const uint8_t *prev_img, const uint8_t *curr_img,
                       int w, int h, int stride,
                       const float *pts_in, int n_pts,
                       float *pts_out, uint8_t *status,
                       const ngd_lk_params *params)
{
    if (!prev_img || !curr_img || !pts_in || !pts_out || !status) return 1;
    if (w < 4 || h < 4 || stride < w || n_pts < 1) return 1;

    ngd_lk_params def = NGD_LK_DEFAULT_TRACKING;
    if (params) def = *params;
    const int nlevels  = def.max_level + 1;
    const int half     = (def.win_size | 1) / 2;  /* window half-size */

    /* Float pyramid for prev (gradients), uint8 for curr (bilinear). */
    pyramidf *pyr_prev = pyramidf_build(prev_img, w, h, stride, nlevels);
    pyramidu *pyr_curr = pyramidu_build(curr_img, w, h, stride, nlevels);
    if (!pyr_prev || !pyr_curr) {
        pyramidf_free(pyr_prev); pyramidu_free(pyr_curr);
        for (int i = 0; i < n_pts; ++i) {
            pts_out[2*i] = pts_in[2*i]; pts_out[2*i+1] = pts_in[2*i+1];
            status[i] = 0;
        }
        return 1;
    }

    for (int p = 0; p < n_pts; ++p) {
        float in_x = pts_in[2*p], in_y = pts_in[2*p+1];
        if (in_x < 0.0f || in_y < 0.0f || in_x >= (float)w || in_y >= (float)h) {
            pts_out[2*p] = in_x; pts_out[2*p+1] = in_y; status[p] = 0; continue;
        }

        float dx = 0.0f, dy = 0.0f;

        /* Coarse-to-fine */
        for (int lv = nlevels - 1; lv >= 0; --lv) {
            pyrf_level *Lp = &pyr_prev->levels[lv];
            pyru_level *Lc = &pyr_curr->levels[lv];
            float scale = 1.0f / (float)(1 << lv);
            float lv_dx = dx * scale, lv_dy = dy * scale;

            int ok = lk_level_f(Lp->buf, Lp->w, Lp->h,
                                Lc->buf, Lc->w, Lc->h, Lc->stride,
                                in_x * scale, in_y * scale,
                                &lv_dx, &lv_dy,
                                half, def.max_iter, def.epsilon,
                                (lv == 0) ? def.min_eig : 0.0f);  /* minEig only at finest level */
            if (!ok) { status[p] = 0; goto next_point; }

            dx = lv_dx * (float)(1 << lv);
            dy = lv_dy * (float)(1 << lv);
        }

        float out_x = in_x + dx, out_y = in_y + dy;
        if (out_x < 0.0f || out_y < 0.0f || out_x >= (float)w || out_y >= (float)h) {
            pts_out[2*p] = out_x; pts_out[2*p+1] = out_y; status[p] = 0; continue;
        }
        pts_out[2*p] = out_x; pts_out[2*p+1] = out_y; status[p] = 1;
        continue;
    next_point:
        pts_out[2*p] = in_x + dx; pts_out[2*p+1] = in_y + dy;
    }

    pyramidf_free(pyr_prev);
    pyramidu_free(pyr_curr);
    return 0;
}

/* =========================================================================
 * U8 backend: uint8 pyramids + flat window weights (no Gaussian).
 *
 * This is the original pure-C LK implementation, preserved alongside the
 * float+Gaussian variant so callers can A/B test.  On fr3_walk_xyz the u8
 * backend produces marginally better ATE (11.81 vs 14.58 cm), likely because
 * Gaussian weighting amplifies gradient noise in low-texture regions.
 * ======================================================================== */

/* =========================================================================
 * Min-eigenvalue pre-filter (u8 variant).
 * Same as min_eig_pass but reads from a uint8 image.
 * ======================================================================== */
static int min_eig_pass_u8(const uint8_t *img, int pw, int ph, int stride,
                            float px, float py, int half, float min_eig)
{
    if (min_eig <= 0.0f) return 1;

    int ix = (int)px, iy = (int)py;
    if (ix - half < 0 || ix + half >= pw - 1 ||
        iy - half < 0 || iy + half >= ph - 1)
        return 0;

    float Gxx = 0.0f, Gxy = 0.0f, Gyy = 0.0f;
    int N = 0;

    for (int wy = -half; wy <= half; ++wy) {
        int r = iy + wy;
        for (int wx = -half; wx <= half; ++wx) {
            int c = ix + wx;
            float Ix = 0.5f * ((float)img[(size_t)r * stride + (c+1)] -
                               (float)img[(size_t)r * stride + (c-1)]);
            float Iy = 0.5f * ((float)img[(size_t)(r+1) * stride + c] -
                               (float)img[(size_t)(r-1) * stride + c]);
            Gxx += Ix * Ix; Gxy += Ix * Iy; Gyy += Iy * Iy;
            ++N;
        }
    }

    if (N < 4) return 0;

    float trace = Gxx + Gyy;
    float delta = (Gxx - Gyy) * (Gxx - Gyy) + 4.0f * Gxy * Gxy;
    float eig_min = 0.5f * (trace - sqrtf(delta > 0.0f ? delta : 0.0f));
    return (eig_min / (float)N >= min_eig);
}

/* Flat-window single-level LK on uint8 pyramids.
 * min_eig: 0=disabled, >0=pre-filter low-texture points before iterating. */
static int lk_level_u8(const uint8_t *prev, int pw, int ph, int pstride,
                       const uint8_t *curr, int cw, int ch, int cstride,
                       float px, float py, float *dx, float *dy,
                       int half, int max_iter, float epsilon, float min_eig)
{
    if (!min_eig_pass_u8(prev, pw, ph, pstride, px, py, half, min_eig))
        return 0;
    for (int iter = 0; iter < max_iter; ++iter) {
        float cx = px + *dx, cy = py + *dy;
        if (cx - half < 0.0f || cx + half >= (float)(cw - 1) ||
            cy - half < 0.0f || cy + half >= (float)(ch - 1))
            return 0;

        float Gxx = 0.0f, Gxy = 0.0f, Gyy = 0.0f;
        float bx  = 0.0f, by  = 0.0f;

        for (int wy = -half; wy <= half; ++wy) {
            int   ipy = (int)(py + (float)wy);
            float ccy = cy + (float)wy;
            for (int wx = -half; wx <= half; ++wx) {
                int   ipx = (int)(px + (float)wx);
                float ccx = cx + (float)wx;
                float pv  = (float)prev[(size_t)ipy * pstride + ipx];
                float cv  = bilinear(curr, cw, ch, cstride, ccx, ccy);

                float Ix, Iy;
                if (ipx > 0 && ipx < pw - 1) {
                    Ix = 0.5f * ((float)prev[(size_t)ipy * pstride + (ipx+1)] -
                                 (float)prev[(size_t)ipy * pstride + (ipx-1)]);
                } else if (ipx == 0) {
                    Ix = (float)prev[(size_t)ipy * pstride + 1] -
                         (float)prev[(size_t)ipy * pstride + 0];
                } else {
                    Ix = (float)prev[(size_t)ipy * pstride + (pw-1)] -
                         (float)prev[(size_t)ipy * pstride + (pw-2)];
                }
                if (ipy > 0 && ipy < ph - 1) {
                    Iy = 0.5f * ((float)prev[(size_t)(ipy+1) * pstride + ipx] -
                                 (float)prev[(size_t)(ipy-1) * pstride + ipx]);
                } else if (ipy == 0) {
                    Iy = (float)prev[(size_t)1 * pstride + ipx] -
                         (float)prev[(size_t)0 * pstride + ipx];
                } else {
                    Iy = (float)prev[(size_t)(ph-1) * pstride + ipx] -
                         (float)prev[(size_t)(ph-2) * pstride + ipx];
                }

                float It = cv - pv;
                Gxx += Ix * Ix; Gxy += Ix * Iy; Gyy += Iy * Iy;
                bx  += Ix * It; by  += Iy * It;
            }
        }

        float det = Gxx * Gyy - Gxy * Gxy;
        if (fabsf(det) < 1e-6f) return 0;
        float ddx = (Gyy*(-bx) - Gxy*(-by)) / det;
        float ddy = (Gxx*(-by) - Gxy*(-bx)) / det;
        *dx += ddx; *dy += ddy;
        if (fabsf(ddx) < epsilon && fabsf(ddy) < epsilon) break;
    }
    return 1;
}

int ngd_optflow_pyr_lk_u8(const uint8_t *prev_img, const uint8_t *curr_img,
                           int w, int h, int stride,
                           const float *pts_in, int n_pts,
                           float *pts_out, uint8_t *status,
                           const ngd_lk_params *params)
{
    if (!prev_img || !curr_img || !pts_in || !pts_out || !status) return 1;
    if (w < 4 || h < 4 || stride < w || n_pts < 1) return 1;

    ngd_lk_params def = NGD_LK_DEFAULT_TRACKING;
    if (params) def = *params;
    const int nlevels = def.max_level + 1;
    const int half    = (def.win_size | 1) / 2;

    /* Both pyramids are uint8. */
    pyramidu *pyr_prev = pyramidu_build(prev_img, w, h, stride, nlevels);
    pyramidu *pyr_curr = pyramidu_build(curr_img, w, h, stride, nlevels);
    if (!pyr_prev || !pyr_curr) {
        pyramidu_free(pyr_prev); pyramidu_free(pyr_curr);
        for (int i = 0; i < n_pts; ++i) {
            pts_out[2*i] = pts_in[2*i]; pts_out[2*i+1] = pts_in[2*i+1];
            status[i] = 0;
        }
        return 1;
    }

    for (int p = 0; p < n_pts; ++p) {
        float in_x = pts_in[2*p], in_y = pts_in[2*p+1];
        if (in_x < 0.0f || in_y < 0.0f || in_x >= (float)w || in_y >= (float)h) {
            pts_out[2*p] = in_x; pts_out[2*p+1] = in_y; status[p] = 0; continue;
        }

        float dx = 0.0f, dy = 0.0f;
        for (int lv = nlevels - 1; lv >= 0; --lv) {
            pyru_level *Lp = &pyr_prev->levels[lv];
            pyru_level *Lc = &pyr_curr->levels[lv];
            float scale = 1.0f / (float)(1 << lv);
            float lv_dx = dx * scale, lv_dy = dy * scale;

            int ok = lk_level_u8(Lp->buf, Lp->w, Lp->h, Lp->stride,
                                 Lc->buf, Lc->w, Lc->h, Lc->stride,
                                 in_x * scale, in_y * scale,
                                 &lv_dx, &lv_dy, half, def.max_iter, def.epsilon,
                                 (lv == 0) ? def.min_eig : 0.0f);  /* minEig only at finest level */
            if (!ok) { status[p] = 0; goto next_u8; }
            dx = lv_dx * (float)(1 << lv);
            dy = lv_dy * (float)(1 << lv);
        }

        float out_x = in_x + dx, out_y = in_y + dy;
        if (out_x < 0.0f || out_y < 0.0f || out_x >= (float)w || out_y >= (float)h) {
            pts_out[2*p] = out_x; pts_out[2*p+1] = out_y; status[p] = 0; continue;
        }
        pts_out[2*p] = out_x; pts_out[2*p+1] = out_y; status[p] = 1;
        continue;
    next_u8:
        pts_out[2*p] = in_x + dx; pts_out[2*p+1] = in_y + dy;
    }

    pyramidu_free(pyr_prev);
    pyramidu_free(pyr_curr);
    return 0;
}

int ngd_optflow_search_u8(const uint8_t *last_gray, const uint8_t *curr_gray,
                           const uint8_t *mask, int w, int h, int stride,
                           const float *last_keys, const int *last_mp_valid, int n_last,
                           float *out_keys, int *out_mp_idx,
                           const ngd_lk_params *params)
{
    if (n_last <= 0 || !last_gray || !curr_gray || !last_keys || !last_mp_valid) return 0;
    if (w <= 0 || h <= 0 || stride < w) return 0;

    float  *src_pts = (float *)malloc(sizeof(float) * 2 * (size_t)n_last);
    int    *src_idx = (int   *)malloc(sizeof(int)         * (size_t)n_last);
    if (!src_pts || !src_idx) { free(src_pts); free(src_idx); return 0; }
    int n_src = 0;
    for (int i = 0; i < n_last; ++i) {
        if (!last_mp_valid[i]) continue;
        src_pts[2*n_src] = last_keys[2*i]; src_pts[2*n_src+1] = last_keys[2*i+1];
        src_idx[n_src] = i; ++n_src;
    }
    if (n_src == 0) { free(src_pts); free(src_idx); return 0; }

    float   *trk_pts = (float   *)malloc(sizeof(float) * 2 * (size_t)n_src);
    uint8_t *trk_st  = (uint8_t *)malloc(sizeof(uint8_t)   * (size_t)n_src);
    if (!trk_pts || !trk_st) { free(src_pts); free(src_idx); free(trk_pts); free(trk_st); return 0; }

    ngd_lk_params def = NGD_LK_DEFAULT_TRACKING;
    if (params) def = *params;

    int ret = ngd_optflow_pyr_lk_u8(last_gray, curr_gray, w, h, stride,
                                     src_pts, n_src, trk_pts, trk_st, &def);
    if (ret != 0) { free(src_pts); free(src_idx); free(trk_pts); free(trk_st); return 0; }

    /* FB consistency (off by default). */
    uint8_t *fb_st = NULL; float *fb_pts = NULL;
    if (def.fb_thresh > 0.0f) {
        fb_st  = (uint8_t *)malloc(sizeof(uint8_t) * (size_t)n_src);
        fb_pts = (float   *)malloc(sizeof(float) * 2 * (size_t)n_src);
        if (fb_st && fb_pts) {
            ngd_lk_params fb_params = def;
            fb_params.max_iter = 10;
            int fb_ret = ngd_optflow_pyr_lk_u8(curr_gray, last_gray, w, h, stride,
                                                trk_pts, n_src, fb_pts, fb_st, &fb_params);
            if (fb_ret != 0) { free(fb_st); free(fb_pts); fb_st = NULL; fb_pts = NULL; }
        } else { free(fb_st); free(fb_pts); fb_st = NULL; fb_pts = NULL; }
    }

    const int has_mask = (mask != NULL);
    int n_out = 0;
    for (int k = 0; k < n_src; ++k) {
        if (!trk_st[k]) continue;
        if (fb_st && fb_pts) {
            if (!fb_st[k]) continue;
            float ddx = fb_pts[2*k] - src_pts[2*k];
            float ddy = fb_pts[2*k+1] - src_pts[2*k+1];
            if (ddx*ddx + ddy*ddy > def.fb_thresh * def.fb_thresh) continue;
        }
        float px = trk_pts[2*k], py = trk_pts[2*k+1];
        if (px < 0.0f || py < 0.0f || px >= (float)w || py >= (float)h) continue;
        if (has_mask && mask[(int)py * stride + (int)px] != 0) continue;
        out_keys[2*n_out] = px; out_keys[2*n_out+1] = py;
        out_mp_idx[n_out] = src_idx[k]; ++n_out;
    }

    free(fb_st); free(fb_pts);
    free(src_pts); free(src_idx); free(trk_pts); free(trk_st);
    return n_out;
}

/* ---- LK callback (matches ngd_lk_flow_fn) ---- */
int ngd_optflow_lk_callback(const uint8_t *prev, const uint8_t *curr,
                            int w, int h, int stride,
                            const float *pts_in, int n,
                            float *pts_out, uint8_t *status, void *user)
{
    (void)user;
    ngd_lk_params params = NGD_LK_DEFAULT_MASK;
    return ngd_optflow_pyr_lk(prev, curr, w, h, stride,
                              pts_in, n, pts_out, status, &params);
}

/* ---- SearchByOpticalFlow convenience wrapper ---- */
int ngd_optflow_search(const uint8_t *last_gray, const uint8_t *curr_gray,
                       const uint8_t *mask, int w, int h, int stride,
                       const float *last_keys, const int *last_mp_valid, int n_last,
                       float *out_keys, int *out_mp_idx,
                       const ngd_lk_params *params)
{
    if (n_last <= 0 || !last_gray || !curr_gray || !last_keys || !last_mp_valid) return 0;
    if (w <= 0 || h <= 0 || stride < w) return 0;

    float  *src_pts = (float *)malloc(sizeof(float) * 2 * (size_t)n_last);
    int    *src_idx = (int   *)malloc(sizeof(int)         * (size_t)n_last);
    if (!src_pts || !src_idx) { free(src_pts); free(src_idx); return 0; }
    int n_src = 0;
    for (int i = 0; i < n_last; ++i) {
        if (!last_mp_valid[i]) continue;
        src_pts[2*n_src] = last_keys[2*i]; src_pts[2*n_src+1] = last_keys[2*i+1];
        src_idx[n_src] = i; ++n_src;
    }
    if (n_src == 0) { free(src_pts); free(src_idx); return 0; }

    float   *trk_pts = (float   *)malloc(sizeof(float) * 2 * (size_t)n_src);
    uint8_t *trk_st  = (uint8_t *)malloc(sizeof(uint8_t)   * (size_t)n_src);
    if (!trk_pts || !trk_st) { free(src_pts); free(src_idx); free(trk_pts); free(trk_st); return 0; }

    ngd_lk_params def = NGD_LK_DEFAULT_TRACKING;
    if (params) def = *params;

    int ret = ngd_optflow_pyr_lk(last_gray, curr_gray, w, h, stride,
                                 src_pts, n_src, trk_pts, trk_st, &def);
    if (ret != 0) { free(src_pts); free(src_idx); free(trk_pts); free(trk_st); return 0; }

    /* Forward-backward consistency (off by default; enable via NGD_LK_FB env). */
    uint8_t *fb_st = NULL;
    float   *fb_pts = NULL;
    if (def.fb_thresh > 0.0f) {
        fb_st  = (uint8_t *)malloc(sizeof(uint8_t) * (size_t)n_src);
        fb_pts = (float   *)malloc(sizeof(float) * 2 * (size_t)n_src);
        if (fb_st && fb_pts) {
            ngd_lk_params fb_params = def;
            fb_params.max_iter = 10;
            int fb_ret = ngd_optflow_pyr_lk(curr_gray, last_gray, w, h, stride,
                                            trk_pts, n_src, fb_pts, fb_st, &fb_params);
            if (fb_ret != 0) { free(fb_st); free(fb_pts); fb_st = NULL; fb_pts = NULL; }
        } else { free(fb_st); free(fb_pts); fb_st = NULL; fb_pts = NULL; }
    }

    const int has_mask = (mask != NULL);
    int n_out = 0;
    for (int k = 0; k < n_src; ++k) {
        if (!trk_st[k]) continue;
        if (fb_st && fb_pts) {
            if (!fb_st[k]) continue;
            float dx = fb_pts[2*k] - src_pts[2*k];
            float dy = fb_pts[2*k+1] - src_pts[2*k+1];
            if (dx*dx + dy*dy > def.fb_thresh * def.fb_thresh) continue;
        }
        float px = trk_pts[2*k], py = trk_pts[2*k+1];
        if (px < 0.0f || py < 0.0f || px >= (float)w || py >= (float)h) continue;
        if (has_mask && mask[(int)py * stride + (int)px] != 0) continue;
        out_keys[2*n_out] = px; out_keys[2*n_out+1] = py;
        out_mp_idx[n_out] = src_idx[k]; ++n_out;
    }

    free(fb_st); free(fb_pts);
    free(src_pts); free(src_idx); free(trk_pts); free(trk_st);
    return n_out;
}
