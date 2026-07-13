/* ngd/mask.c — NGD dynamic-mask prediction chain (pure C).
 *
 * Faithful port of Tracking::PredictCurrentMask and its helpers
 * (src/Tracking.cc:4249-4587). See include/ngd/mask.h for the design notes and
 * the two latent-bug fixes (isBrighter/isDarker init; OOB → -1).
 *
 * Pure C, no external deps (<math.h>/<stdlib.h>/<string.h>). All pixel access
 * is bounds-guarded (no Mat::at raw indexing). */

#include "ngd/mask.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Minimal growable int array (stands in for the std::vector the original
 * DBSCAN expand uses — the neighbour set grows past nseg because duplicates are
 * appended just like neighbors.insert(end, newNeighbors)). */
typedef struct { int *p; int n; int cap; } intvec;
static int intvec_reserve(intvec *v, int need)
{
    if (need <= v->cap) return 1;
    int nc = v->cap ? v->cap : 16;
    while (nc < need) nc *= 2;
    int *t = (int*)realloc(v->p, (size_t)nc * sizeof(int));
    if (!t) return 0;
    v->p = t; v->cap = nc;
    return 1;
}
static void intvec_push(intvec *v, int x)
{
    if (intvec_reserve(v, v->n + 1)) v->p[v->n++] = x;
}

/* ------------------------------------------------------------------ */
/* Binary morphology                                                   */
/* ------------------------------------------------------------------ */

static void morphology(const uint8_t *in, uint8_t *out, int w, int h, int half, int do_erode)
{
    /* Handle in==out aliasing by working from a scratch copy. */
    const uint8_t *src = in;
    uint8_t *scratch = NULL;
    if (in == out) {
        scratch = (uint8_t*)malloc((size_t)w * (size_t)h);
        if (!scratch) { /* OOM: leave out untouched */ return; }
        memcpy(scratch, in, (size_t)w * (size_t)h);
        src = scratch;
    }

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int y0 = y - half, y1 = y + half;
            int x0 = x - half, x1 = x + half;
            if (y0 < 0) y0 = 0;
            if (x0 < 0) x0 = 0;
            if (y1 > h - 1) y1 = h - 1;
            if (x1 > w - 1) x1 = w - 1;

            uint8_t v;
            if (do_erode) {
                /* AND over in-bounds window: 1 only if all neighbours are 1. */
                v = 1;
                for (int yy = y0; yy <= y1 && v; ++yy)
                    for (int xx = x0; xx <= x1; ++xx)
                        if (!src[(size_t)yy * w + xx]) { v = 0; break; }
            } else {
                /* OR over in-bounds window: 1 if any neighbour is 1. */
                v = 0;
                for (int yy = y0; yy <= y1 && !v; ++yy)
                    for (int xx = x0; xx <= x1; ++xx)
                        if (src[(size_t)yy * w + xx]) { v = 1; break; }
            }
            out[(size_t)y * w + x] = v;
        }
    }
    free(scratch);
}

void ngd_mask_erode (const uint8_t *in, uint8_t *out, int w, int h, int half) { morphology(in, out, w, h, half, 1); }
void ngd_mask_dilate(const uint8_t *in, uint8_t *out, int w, int h, int half) { morphology(in, out, w, h, half, 0); }

/* ------------------------------------------------------------------ */
/* FAST-style 4-cross corner potential (Tracking.cc:4308-4352)         */
/* ------------------------------------------------------------------ */

int ngd_mask_pixel_potential(const uint8_t *gray, int w, int h, int stride, int x, int y)
{
    /* 4-pixel cross of radius 3. */
    static const int ox[4] = { -3, 0, 3, 0 };
    static const int oy[4] = { 0, 3, 0, -3 };

    /* Intended bounds guard (the original's was dead code → read OOB near the
     * border). Any cross neighbour out of bounds ⇒ not a usable seed. */
    for (int k = 0; k < 4; ++k) {
        int nx = x + ox[k], ny = y + oy[k];
        if (nx < 0 || nx >= w || ny < 0 || ny >= h) return -1;
    }

    int center = gray[(size_t)y * stride + x];
    int isBrighter = 0, isDarker = 0;                 /* FIX: original left these uninitialized */
    int brightLowerBound = -1, darkLowerBound = -1;

    for (int k = 0; k < 4; ++k) {
        int nx = x + ox[k], ny = y + oy[k];
        int intensity = gray[(size_t)ny * stride + nx];
        if (intensity > center) {
            int diff = intensity - center;
            if (brightLowerBound == -1 || diff < brightLowerBound) brightLowerBound = diff;
            isBrighter++;
        } else if (intensity < center) {
            int diff = center - intensity;
            if (darkLowerBound == -1 || diff < darkLowerBound) darkLowerBound = diff;
            isDarker++;
        }
    }

    if (isBrighter >= 3) return 200 + brightLowerBound;
    if (isDarker >= 3)   return 200 + darkLowerBound;
    /* max(): both bounds default to -1 when no brighter/darker neighbour. */
    return brightLowerBound > darkLowerBound ? brightLowerBound : darkLowerBound;
}

/* ------------------------------------------------------------------ */
/* ExtractDynaPoints (Tracking.cc:4355-4399)                           */
/* ------------------------------------------------------------------ */

void ngd_mask_extract_dyna_points(const uint8_t *gray, int w, int h, int stride,
                                  const uint8_t *mask, int cellSize, int threshold,
                                  ngd_pt2f *out, int *out_n)
{
    *out_n = 0;
    if (!gray || !mask || !out) return;
    if (w <= 0 || h <= 0 || cellSize <= 0) return;

    for (int y = 0; y < h; y += cellSize) {
        for (int x = 0; x < w; x += cellSize) {
            int maxPotential = -1, bestX = -1, bestY = -1;
            int ymax = (cellSize < h - y) ? cellSize : h - y;
            int xmax = (cellSize < w - x) ? cellSize : w - x;
            int brk = 0;
            for (int dy = 0; dy < ymax && !brk; ++dy) {
                for (int dx = 0; dx < xmax; ++dx) {
                    int cx = x + dx, cy = y + dy;
                    if (mask[(size_t)cy * w + cx] == 1) {
                        int p = ngd_mask_pixel_potential(gray, w, h, stride, cx, cy);
                        if (p > maxPotential) { maxPotential = p; bestX = cx; bestY = cy; }
                    }
                    if (maxPotential > threshold) { brk = 1; break; }
                }
            }
            if (maxPotential != -1 && bestX > 0 && bestY > 0 && bestX < w && bestY < h) {
                out[*out_n].x = (float)bestX;
                out[*out_n].y = (float)bestY;
                (*out_n)++;
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* ClusterWithDBSCAN (Tracking.cc:4402-4483)                           */
/* ------------------------------------------------------------------ */

typedef struct { float z; int src; } z_key;

static int cmp_zkey(const void *a, const void *b)
{
    float za = ((const z_key*)a)->z, zb = ((const z_key*)b)->z;
    if (za < zb) return -1;
    if (za > zb) return 1;
    return 0;
}

void ngd_mask_cluster_dbscan(const ngd_pt3f *pts, int n, float eps, int minPts,
                             int *labels, int *out_nclusters)
{
    int nclusters = 0;
    int i;
    for (i = 0; i < n; ++i) labels[i] = -1;   /* default: noise / dropped */
    if (out_nclusters) *out_nclusters = 0;
    if (!pts || n <= 0) return;

    /* ---- Phase 1: depth pre-segmentation (sort by z, split, rewrite z) ---- */
    z_key *order = (z_key*)malloc((size_t)n * sizeof(z_key));
    if (!order) return;
    for (i = 0; i < n; ++i) { order[i].z = pts[i].z; order[i].src = i; }
    qsort(order, (size_t)n, sizeof(z_key), cmp_zkey);

    const float range = 0.5f, step = 0.1f;
    const int   minSize = 20;
    ngd_pt3f *seg = (ngd_pt3f*)malloc((size_t)n * sizeof(ngd_pt3f));
    int      *seg_src = (int*)malloc((size_t)n * sizeof(int));
    int       *cids = (int*)malloc((size_t)n * sizeof(int));
    if (!seg || !seg_src || !cids) { free(order); free(seg); free(seg_src); free(cids); return; }

    int nseg = 0;            /* segmentedPoints.size() */
    int level = 1;
    ngd_pt3f *temp = (ngd_pt3f*)malloc((size_t)n * sizeof(ngd_pt3f));
    int      *temp_src = (int*)malloc((size_t)n * sizeof(int));
    int       ntemp = 0;
    if (!temp || !temp_src) { free(order); free(seg); free(seg_src); free(cids); free(temp); free(temp_src); return; }

    for (i = 0; i < n; ++i) {
        const ngd_pt3f *point = &pts[order[i].src];
        int addPoint = 1;
        if (ntemp > 0) {
            float back = temp[ntemp - 1].z;
            float front = temp[0].z;
            if (point->z - back > step || point->z - front > range) addPoint = 0;
        }
        if (addPoint) {
            temp[ntemp] = *point;
            temp_src[ntemp] = order[i].src;
            ntemp++;
        } else {
            /* Close the current segment. Faithful to the original: the trailing
             * segment (after the last split) is NOT flushed — only segments
             * closed by a violating point are kept. */
            if (ntemp > minSize) {
                for (int j = 0; j < ntemp; ++j) temp[j].z = 200.0f * (float)level;
                level++;
                for (int j = 0; j < ntemp; ++j) { seg[nseg] = temp[j]; seg_src[nseg] = temp_src[j]; nseg++; }
            }
            ntemp = 0;
            temp[ntemp] = *point;
            temp_src[ntemp] = order[i].src;
            ntemp++;
        }
    }
    /* Fallback: nothing segmented ⇒ use all original points (original z). */
    if (nseg == 0) {
        for (i = 0; i < n; ++i) { seg[i] = pts[i]; seg_src[i] = i; }
        nseg = n;
    }

    /* ---- Phase 2: DBSCAN on segmented points ---- */
    for (i = 0; i < nseg; ++i) cids[i] = -2;   /* -2 unvisited */

    /* regionQuery writes one query's result into newnbr (<= nseg). The expand
     * set (nbr) is growable: it accumulates newNeighbours of every core point
     * and can exceed nseg, exactly like the original's neighbors vector. */
    int     *newnbr = (int*)malloc((size_t)(nseg > 0 ? nseg : 1) * sizeof(int));
    intvec   nbr = {0,0,0};
    if (!newnbr) {
        free(order); free(seg); free(seg_src); free(cids); free(temp); free(temp_src); free(newnbr); free(nbr.p);
        return;
    }

    for (i = 0; i < nseg; ++i) {
        if (cids[i] != -2) continue;
        /* regionQuery(i) */
        int nnew = 0;
        for (int k = 0; k < nseg; ++k) {
            float dx = seg[i].x - seg[k].x, dy = seg[i].y - seg[k].y, dz = seg[i].z - seg[k].z;
            if (dx*dx + dy*dy + dz*dz < eps*eps) newnbr[nnew++] = k;
        }
        if (nnew < minPts) {
            cids[i] = -1;   /* noise */
            continue;
        }
        nclusters++;       /* next cluster id */
        /* Seed the expand set with i's neighbours. */
        nbr.n = 0;
        for (int k = 0; k < nnew; ++k) intvec_push(&nbr, newnbr[k]);
        /* Expand: iterate j over a growing neighbour set. */
        for (int j = 0; j < nbr.n; ++j) {
            int ni = nbr.p[j];
            if (cids[ni] == -1) cids[ni] = nclusters;          /* noise → border */
            if (cids[ni] != -2) continue;                       /* already visited */
            cids[ni] = nclusters;
            /* regionQuery(ni) */
            int m = 0;
            for (int k = 0; k < nseg; ++k) {
                float dx = seg[ni].x - seg[k].x, dy = seg[ni].y - seg[k].y, dz = seg[ni].z - seg[k].z;
                if (dx*dx + dy*dy + dz*dz < eps*eps) newnbr[m++] = k;
            }
            if (m >= minPts) {
                for (int k = 0; k < m; ++k) intvec_push(&nbr, newnbr[k]);
            }
        }
    }

    /* Map labels back to ORIGINAL point indices. */
    for (i = 0; i < nseg; ++i)
        if (cids[i] > 0) labels[seg_src[i]] = cids[i];

    free(order); free(seg); free(seg_src); free(cids);
    free(temp); free(temp_src); free(newnbr); free(nbr.p);
    if (out_nclusters) *out_nclusters = nclusters;
}

/* ------------------------------------------------------------------ */
/* connectedComponents (cv::connectedComponentsWithStats, 4-conn)      */
/* ------------------------------------------------------------------ */

int ngd_mask_connected_components(const uint8_t *mask, int w, int h, int *labels)
{
    if (!mask || !labels || w <= 0 || h <= 0) return 1;
    memset(labels, 0, (size_t)w * (size_t)h * sizeof(int));
    int *queue = (int*)malloc((size_t)w * (size_t)h * sizeof(int));
    if (!queue) return 1;

    int cur = 0;   /* background = 0 */
    for (int sy = 0; sy < h; ++sy) {
        for (int sx = 0; sx < w; ++sx) {
            if (mask[(size_t)sy * w + sx] != 1 || labels[(size_t)sy * w + sx] != 0) continue;
            cur++;
            int head = 0, tail = 0;
            labels[(size_t)sy * w + sx] = cur;
            queue[tail++] = sy * w + sx;
            while (head < tail) {
                int p = queue[head++];
                int py = p / w, px = p - py * w;
                /* 4-connectivity */
                if (px > 0     && mask[(size_t)p - 1] == 1 && labels[(size_t)p - 1] == 0) { labels[(size_t)p - 1] = cur; queue[tail++] = p - 1; }
                if (px < w - 1 && mask[(size_t)p + 1] == 1 && labels[(size_t)p + 1] == 0) { labels[(size_t)p + 1] = cur; queue[tail++] = p + 1; }
                if (py > 0     && mask[(size_t)p - w] == 1 && labels[(size_t)p - w] == 0) { labels[(size_t)p - w] = cur; queue[tail++] = p - w; }
                if (py < h - 1 && mask[(size_t)p + w] == 1 && labels[(size_t)p + w] == 0) { labels[(size_t)p + w] = cur; queue[tail++] = p + w; }
            }
        }
    }
    free(queue);
    return cur + 1;   /* nLabels = #components + background */
}

/* ------------------------------------------------------------------ */
/* CreateMaskFromClusters (Tracking.cc:4486-4587)                      */
/* ------------------------------------------------------------------ */

static int cmp_float(const void *a, const void *b)
{
    float fa = *(const float*)a, fb = *(const float*)b;
    if (fa < fb) return -1;
    if (fa > fb) return 1;
    return 0;
}

void ngd_mask_create_from_clusters(const ngd_pt3f *pts, const int *labels, int n, int nclusters,
                                   const float *depth, int dw, int dh, int dstride,
                                   uint8_t *out_mask, int w, int h)
{
    if (!out_mask || !depth || w <= 0 || h <= 0) return;
    if (w != dw || h != dh) return;   /* size mismatch guard */

    uint8_t *local = (uint8_t*)malloc((size_t)w * (size_t)h);
    int     *cc    = (int*)malloc((size_t)w * (size_t)h * sizeof(int));
    if (!local || !cc) { free(local); free(cc); return; }

    for (int cid = 1; cid <= nclusters; ++cid) {
        /* Gather this cluster's points + valid depths. */
        float minX = 1e30f, minY = 1e30f, maxX = -1e30f, maxY = -1e30f;
        int count = 0;
        /* pointsDepth collected in a first pass sizing pass is overkill; reuse
         * a stack buffer bounded by n (cluster size <= n). */
        float *pdepth = (float*)malloc((size_t)n * sizeof(float));
        int npd = 0;
        if (!pdepth) { free(local); free(cc); return; }

        for (int i = 0; i < n; ++i) {
            if (labels[i] != cid) continue;
            count++;
            float px = pts[i].x, py = pts[i].y;
            if (px < minX) minX = px; if (px > maxX) maxX = px;
            if (py < minY) minY = py; if (py > maxY) maxY = py;
            int ix = (int)px, iy = (int)py;
            if (ix >= 0 && ix < dw && iy >= 0 && iy < dh) {
                float d = depth[(size_t)iy * dstride + ix];
                if (d >= 0.05f) pdepth[npd++] = d;
            }
        }
        if (count <= 1) { free(pdepth); continue; }   /* original: size() > 1 */

        int tLx = (int)minX, tLy = (int)minY, bRx = (int)maxX, bRy = (int)maxY;
        float width  = (float)(bRx - tLx);
        float height = (float)(bRy - tLy);
        if (width < 50.0f || height < 50.0f) { free(pdepth); continue; }

        int aTLx = tLx - 15, aTLy = tLy - 15;
        int aBRx = bRx + 15, aBRy = bRy + 15;
        if (aTLx < 0) aTLx = 0; if (aTLy < 0) aTLy = 0;
        /* Clamp to w-1 / h-1 (NOT w/h): the fill loop below is inclusive (<=),
         * so a boundary value of w (or h) would write index w (= (yy+1)*w, and
         * h*w past the buffer on the last row) -> heap overflow when the cluster
         * bbox touches the image edge. */
        if (aBRx >= w) aBRx = w - 1; if (aBRy >= h) aBRy = h - 1;

        memset(local, 0, (size_t)w * (size_t)h);
        for (int yy = aTLy; yy <= aBRy; ++yy)
            for (int xx = aTLx; xx <= aBRx; ++xx)
                local[(size_t)yy * w + xx] = 1;

        /* Median depth (original indexes [size/2]); skip gate if none valid. */
        if (npd > 0) {
            qsort(pdepth, (size_t)npd, sizeof(float), cmp_float);
            float value = pdepth[npd / 2];
            for (int yy = aTLy; yy <= aBRy; ++yy)
                for (int xx = aTLx; xx <= aBRx; ++xx)
                    if (local[(size_t)yy * w + xx] == 1 &&
                        fabsf(depth[(size_t)yy * dstride + xx] - value) > 0.3f)
                        local[(size_t)yy * w + xx] = 0;
        }
        free(pdepth);

        /* Largest connected component of local → write into out_mask. */
        int nLabels = ngd_mask_connected_components(local, w, h, cc);
        int largest = 0, largestArea = 0;
        int *area = (int*)calloc((size_t)(nLabels > 0 ? nLabels : 1), sizeof(int));
        if (area) {
            for (int yy = aTLy; yy <= aBRy; ++yy)
                for (int xx = aTLx; xx <= aBRx; ++xx) {
                    int lab = cc[(size_t)yy * w + xx];
                    if (lab > 0) area[lab]++;
                }
            for (int lab = 1; lab < nLabels; ++lab)
                if (area[lab] > largestArea) { largestArea = area[lab]; largest = lab; }
            free(area);
            for (int yy = aTLy; yy <= aBRy; ++yy)
                for (int xx = aTLx; xx <= aBRx; ++xx)
                    if (cc[(size_t)yy * w + xx] == largest)
                        out_mask[(size_t)yy * w + xx] = 1;
        }
    }

    free(local); free(cc);

    /* Final dilate with 7×7 (dilationSize=3). */
    {
        uint8_t *tmp = (uint8_t*)malloc((size_t)w * (size_t)h);
        if (tmp) { ngd_mask_dilate(out_mask, tmp, w, h, 3); memcpy(out_mask, tmp, (size_t)w * (size_t)h); free(tmp); }
    }
}

/* ------------------------------------------------------------------ */
/* PredictCurrentMask orchestrator (Tracking.cc:4249-4305)             */
/* ------------------------------------------------------------------ */

void ngd_mask_predict_current(const uint8_t *last_gray, const uint8_t *curr_gray,
                              const uint8_t *last_mask, const float *depth,
                              int w, int h, int stride,
                              ngd_lk_flow_fn lk, void *lk_user,
                              uint8_t *out_mask)
{
    if (!out_mask || w <= 0 || h <= 0) return;
    /* mImMask = zeros(...) — zero the output up front. */
    memset(out_mask, 0, (size_t)w * (size_t)h);
    if (!last_mask || !depth) return;

    uint8_t *eroded = (uint8_t*)malloc((size_t)w * (size_t)h);
    if (!eroded) return;
    ngd_mask_erode(last_mask, eroded, w, h, 5);   /* 11×11 (erosionSize=5) */

    ngd_pt2f *seeds = (ngd_pt2f*)malloc((size_t)((w / 15 + 1) * (h / 15 + 1)) * sizeof(ngd_pt2f));
    int nseeds = 0;
    if (seeds) {
        ngd_mask_extract_dyna_points(last_gray, w, h, stride, eroded, 15, 250, seeds, &nseeds);

        if (nseeds > 0 && lk) {
            float *pts_in  = (float*)malloc((size_t)nseeds * 2 * sizeof(float));
            float *pts_out = (float*)malloc((size_t)nseeds * 2 * sizeof(float));
            uint8_t *status = (uint8_t*)malloc((size_t)nseeds);
            ngd_pt3f *tracked = (ngd_pt3f*)malloc((size_t)nseeds * sizeof(ngd_pt3f));
            int *labels = (int*)malloc((size_t)nseeds * sizeof(int));

            if (pts_in && pts_out && status && tracked && labels) {
                for (int j = 0; j < nseeds; ++j) { pts_in[2*j] = seeds[j].x; pts_in[2*j+1] = seeds[j].y; }
                lk(last_gray, curr_gray, w, h, stride, pts_in, nseeds, pts_out, status, lk_user);

                int ntracked = 0;
                for (int j = 0; j < nseeds; ++j) {
                    if (!status[j]) continue;
                    float px = pts_out[2*j], py = pts_out[2*j+1];
                    if (px != px || py != py) continue;             /* NaN */
                    if (px < 0.0f || py < 0.0f || px >= (float)w || py >= (float)h) continue;
                    float d = depth[(size_t)((int)py) * stride + (int)px];
                    if (d >= 0.05f) { tracked[ntracked].x = px; tracked[ntracked].y = py; tracked[ntracked].z = d; ntracked++; }
                }

                if (ntracked > 0) {
                    int nclusters = 0;
                    ngd_mask_cluster_dbscan(tracked, ntracked, 50.0f, 15, labels, &nclusters);
                    ngd_mask_create_from_clusters(tracked, labels, ntracked, nclusters,
                                                  depth, w, h, stride, out_mask, w, h);
                }
            }
            free(pts_in); free(pts_out); free(status); free(tracked); free(labels);
        }
        free(seeds);
    }
    free(eroded);
}
