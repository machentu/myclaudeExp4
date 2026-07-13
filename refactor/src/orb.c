/* ngd/orb.c — ORB feature extractor (pure C). See include/ngd/orb.h.
 *
 * Faithful reimplementation of ORB_SLAM3::ORBextractor (src/ORBextractor.cc).
 * Algorithmic structure and constants mirror the original; the descriptor
 * pattern (bit_pattern_31_) and orientation table (umax) are copied verbatim
 * so descriptors remain vocabulary-compatible. FAST-9, the bilinear resize,
 * and the Gaussian blur are reimplemented (see README faithfulness notes).
 */
#include "ngd/orb.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef NGD_PI
#define NGD_PI 3.14159265358979323846
#endif

/* ===================================================================== *
 *  Static tables
 * ===================================================================== */

/* The calibrated BRIEF pattern (src/ORBextractor.cc:149-407). 256 pairs,
 * each (x1,y1,x2,y2) — the two endpoints of one binary test. Copied verbatim;
 * any change breaks descriptor compatibility with the ORB vocabulary. */
static const int ngd_bit_pattern_31[256][4] = {
    {8,-3, 9,5},{4,2, 7,-12},{-11,9, -8,2},{7,-12, 12,-13},{2,-13, 2,12},
    {1,-7, 1,6},{-2,-10, -2,-4},{-13,-13, -11,-8},{-13,-3, -12,-9},{10,4, 11,9},
    {-13,-8, -8,-9},{-11,7, -9,12},{7,7, 12,6},{-4,-5, -3,0},{-13,2, -12,-3},{-9,0, -7,5},
    {12,-6, 12,-1},{-3,6, -2,12},{-6,-13, -4,-8},{11,-13, 12,-8},{4,7, 5,1},{5,-3, 10,-3},
    {3,-7, 6,12},{-8,-7, -6,-2},{-2,11, -1,-10},{-13,12, -8,10},{-7,3, -5,-3},{-4,2, -3,7},
    {-10,-12, -6,11},{5,-12, 6,-7},{5,-6, 7,-1},{1,0, 4,-5},{9,11, 11,-13},{4,7, 4,12},
    {2,-1, 4,4},{-4,-12, -2,7},{-8,-5, -7,-10},{4,11, 9,12},{0,-8, 1,-13},{-13,-2, -8,2},
    {-3,-2, -2,3},{-6,9, -4,-9},{8,12, 10,7},{0,9, 1,3},{7,-5, 11,-10},{-13,-6, -11,0},
    {10,7, 12,1},{-6,-3, -6,12},{10,-9, 12,-4},{-13,8, -8,-12},{-13,0, -8,-4},{3,3, 7,8},
    {5,7, 10,-7},{-1,7, 1,-12},{3,-10, 5,6},{2,-4, 3,-10},{-13,0, -13,5},{-13,-7, -12,12},
    {-13,3, -11,8},{-7,12, -4,7},{6,-10, 12,8},{-9,-1, -7,-6},{-2,-5, 0,12},{-12,5, -7,5},
    {3,-10, 8,-13},{-7,-7, -4,5},{-3,-2, -1,-7},{2,9, 5,-11},{-11,-13, -5,-13},{-1,6, 0,-1},
    {5,-3, 5,2},{-4,-13, -4,12},{-9,-6, -9,6},{-12,-10, -8,-4},{10,2, 12,-3},{7,12, 12,12},
    {-7,-13, -6,5},{-4,9, -3,4},{7,-1, 12,2},{-7,6, -5,1},{-13,11, -12,5},{-3,7, -2,-6},
    {7,-8, 12,-7},{-13,-7, -11,-12},{1,-3, 12,12},{2,-6, 3,0},{-4,3, -2,-13},{-1,-13, 1,9},
    {7,1, 8,-6},{1,-1, 3,12},{9,1, 12,6},{-1,-9, -1,3},{-13,-13, -10,5},{7,7, 10,12},
    {12,-5, 12,9},{6,3, 7,11},{5,-13, 6,10},{2,-12, 2,3},{3,8, 4,-6},{2,6, 12,-13},
    {9,-12, 10,3},{-8,4, -7,9},{-11,12, -4,-6},{1,12, 2,-8},{6,-9, 7,-4},{2,3, 3,-2},
    {6,3, 11,0},{3,-3, 8,-8},{7,8, 9,3},{-11,-5, -6,-4},{-10,11, -5,10},{-5,-8, -3,12},
    {-10,5, -9,0},{8,-1, 12,-6},{4,-6, 6,-11},{-10,12, -8,7},{4,-2, 6,7},{-2,0, -2,12},
    {-5,-8, -5,2},{7,-6, 10,12},{-9,-13, -8,-8},{-5,-13, -5,-2},{8,-8, 9,-13},{-9,-11, -9,0},
    {1,-8, 1,-2},{7,-4, 9,1},{-2,1, -1,-4},{11,-6, 12,-11},{-12,-9, -6,4},{3,7, 7,12},
    {5,5, 10,8},{0,-4, 2,8},{-9,12, -5,-13},{0,7, 2,12},{-1,2, 1,7},{5,11, 7,-9},{3,5, 6,-8},
    {-13,-4, -8,9},{-5,9, -3,-3},{-4,-7, -3,-12},{6,5, 8,0},{-7,6, -6,12},{-13,6, -5,-2},
    {1,-10, 3,10},{4,1, 8,-4},{-2,-2, 2,-13},{2,-12, 12,12},{-2,-13, 0,-6},{4,1, 9,3},
    {-6,-10, -3,-5},{-3,-13, -1,1},{7,5, 12,-11},{4,-2, 5,-7},{-13,9, -9,-5},{7,1, 8,6},
    {7,-8, 7,6},{-7,-4, -7,1},{-8,11, -7,-8},{-13,6, -12,-8},{2,4, 3,9},{10,-5, 12,3},
    {-6,-5, -6,7},{8,-3, 9,-8},{2,-12, 2,8},{-11,-2, -10,3},{-12,-13, -7,-9},{-11,0, -10,-5},
    {5,-3, 11,8},{-2,-13, -1,12},{-1,-8, 0,9},{-13,-11, -12,-5},{-10,-2, -10,11},{-3,9, -2,-13},
    {2,-3, 3,2},{-9,-13, -4,0},{-4,6, -3,-10},{-4,12, -2,-7},{-6,-11, -4,9},{6,-3, 6,11},
    {-13,11, -5,5},{11,11, 12,6},{7,-5, 12,-2},{-1,12, 0,7},{-4,-8, -3,-2},{-7,1, -6,7},
    {-13,-12, -8,-13},{-7,-2, -6,-8},{-8,5, -6,-9},{-5,-1, -4,5},{-13,7, -8,10},{1,5, 5,-13},
    {1,0, 10,-13},{9,12, 10,-1},{5,-8, 10,-9},{-1,11, 1,-13},{-9,-3, -6,2},{-1,-10, 1,12},
    {-13,1, -8,-10},{8,-11, 10,-6},{2,-13, 3,-6},{7,-13, 12,-9},{-10,-10, -5,-7},{-10,-8, -8,-13},
    {4,-6, 8,5},{3,12, 8,-13},{-4,2, -3,-3},{5,-13, 10,-12},{4,-13, 5,-1},{-9,9, -4,3},
    {0,3, 3,-9},{-12,1, -6,1},{3,2, 4,-8},{-10,-10, -10,9},{8,-13, 12,12},{-8,-12, -6,-5},
    {2,2, 3,7},{10,6, 11,-8},{6,8, 8,-12},{-7,10, -6,5},{-3,-9, -3,9},{-1,-13, -1,5},
    {-3,-7, -3,4},{-8,-2, -8,3},{4,2, 12,12},{2,-5, 3,11},{6,-9, 11,-13},{3,-1, 7,12},
    {11,-1, 12,4},{-3,0, -3,6},{4,-11, 4,12},{2,-4, 2,1},{-10,-6, -8,1},{-13,7, -11,1},
    {-13,12, -11,-13},{6,0, 11,-13},{0,-1, 1,4},{-13,3, -9,-2},{-9,8, -6,-3},{-13,-6, -8,-2},
    {5,-9, 8,10},{2,7, 3,-9},{-1,-6, -1,-1},{9,5, 11,-2},{11,-3, 12,-8},{3,0, 3,5},
    {-1,4, 0,10},{3,-6, 4,5},{-13,0, -10,5},{5,8, 12,11},{8,9, 9,-6},{7,-4, 8,-12},
    {-10,4, -10,9},{7,3, 12,4},{9,-7, 10,-2},{7,0, 12,-2},{-1,-6, 0,-11}
};

/* FAST-9_16 16-point Bresenham circle (radius 3), the standard ring. */
static const int ngd_fast_circle[16][2] = {
    { 0,-3},{ 1,-3},{ 2,-2},{ 3,-1},{ 3, 0},{ 3, 1},{ 2, 2},{ 1, 3},
    { 0, 3},{-1, 3},{-2, 2},{-3, 1},{-3, 0},{-3,-1},{-2,-2},{-1,-3}
};
/* Cardinal pixels for the FAST speed test (indices 0,4,8,12 of the circle). */
#define NGD_FAST_CARDINAL_COUNT 4

/* 7-tap Gaussian, sigma=2 (matches OpenCV getGaussianKernel(7,2) up to fp). */
static const float ngd_gauss7[7] = {
    0.070159f, 0.131072f, 0.190723f, 0.216092f, 0.190723f, 0.131072f, 0.070159f
};

/* ===================================================================== *
 *  Init
 * ===================================================================== */
static int ngd_cvround(float x) {
    /* OpenCV cvRound is round-half-to-even; lround is round-half-away.
     * The difference only hits exact .5 values; lround is close enough here
     * (used for mnFeaturesPerLevel and umax). */
    return (int)lroundf(x);
}

int ngd_orb_init(ngd_orb_extractor *ex, int nfeatures, float scaleFactor,
                 int nlevels, int iniThFAST, int minThFAST)
{
    if (nlevels <= 0 || nlevels > NGD_ORB_MAX_LEVELS) return -1;
    ex->nfeatures = nfeatures;
    ex->scaleFactor = scaleFactor;
    ex->nlevels = nlevels;
    ex->iniThFAST = iniThFAST;
    ex->minThFAST = minThFAST;

    ex->mvScaleFactor[0] = 1.0f;
    ex->mvLevelSigma2[0] = 1.0f;
    for (int i = 1; i < nlevels; ++i) {
        ex->mvScaleFactor[i] = ex->mvScaleFactor[i-1] * scaleFactor;
        ex->mvLevelSigma2[i] = ex->mvScaleFactor[i] * ex->mvScaleFactor[i];
    }
    for (int i = 0; i < nlevels; ++i) {
        ex->mvInvScaleFactor[i] = 1.0f / ex->mvScaleFactor[i];
        ex->mvInvLevelSigma2[i] = 1.0f / ex->mvLevelSigma2[i];
    }

    /* features-per-level (geometric distribution, remainder on last level) */
    float factor = 1.0f / scaleFactor;
    float nDesired = (float)(nfeatures * (1.0 - factor) /
                             (1.0 - pow((double)factor, (double)nlevels)));
    int sumFeatures = 0;
    for (int level = 0; level < nlevels - 1; ++level) {
        ex->mnFeaturesPerLevel[level] = ngd_cvround(nDesired);
        sumFeatures += ex->mnFeaturesPerLevel[level];
        nDesired *= factor;
    }
    ex->mnFeaturesPerLevel[nlevels-1] = nfeatures - sumFeatures;
    if (ex->mnFeaturesPerLevel[nlevels-1] < 0) ex->mnFeaturesPerLevel[nlevels-1] = 0;

    /* umax: per-row horizontal extent of the radius-15 disk (ORBextractor ctor). */
    int vmax = (int)floor(NGD_ORB_HALF_PATCH * sqrtf(2.0f) / 2.0f + 1.0f);
    int vmin = (int)ceil(NGD_ORB_HALF_PATCH * sqrtf(2.0f) / 2.0f);
    double hp2 = (double)NGD_ORB_HALF_PATCH * NGD_ORB_HALF_PATCH;
    for (int v = 0; v <= vmax; ++v)
        ex->umax[v] = ngd_cvround((float)sqrt(hp2 - (double)v * v));
    /* symmetric pass */
    for (int v = NGD_ORB_HALF_PATCH, v0 = 0; v >= vmin; --v) {
        while (ex->umax[v0] == ex->umax[v0+1]) ++v0;
        ex->umax[v] = v0;
        ++v0;
    }
    return 0;
}

/* ===================================================================== *
 *  Image buffer helpers
 * ===================================================================== */
static inline uint8_t ngd_pix_clamp(const uint8_t *img, int w, int h, int x, int y) {
    if (x < 0) x = 0; else if (x >= w) x = w-1;
    if (y < 0) y = 0; else if (y >= h) y = h-1;
    return img[y*w + x];
}

/* bilinear resize src(sw,sh) -> dst(dw,dh), OpenCV INTER_LINEAR pixel-center
 * convention: src = (dst+0.5)*scale_inv - 0.5. */
static void ngd_resize_bilinear(const uint8_t *src, int sw, int sh,
                                 uint8_t *dst, int dw, int dh)
{
    float sx = (float)sw / dw;
    float sy = (float)sh / dh;
    for (int dy = 0; dy < dh; ++dy) {
        float fy = (dy + 0.5f) * sy - 0.5f;
        int y0 = (int)floorf(fy);
        float ay = fy - y0;
        int y1 = y0 + 1;
        if (y0 < 0) { y0 = 0; ay = 0; }
        if (y1 >= sh) y1 = sh - 1;
        for (int dx = 0; dx < dw; ++dx) {
            float fx = (dx + 0.5f) * sx - 0.5f;
            int x0 = (int)floorf(fx);
            float ax = fx - x0;
            int x1 = x0 + 1;
            if (x0 < 0) { x0 = 0; ax = 0; }
            if (x1 >= sw) x1 = sw - 1;
            float v00 = src[y0*sw + x0], v01 = src[y0*sw + x1];
            float v10 = src[y1*sw + x0], v11 = src[y1*sw + x1];
            float r = (v00*(1-ax) + v01*ax) * (1-ay) + (v10*(1-ax) + v11*ax) * ay;
            dst[dy*dw + dx] = (uint8_t)(r + 0.5f);
        }
    }
}

/* separable 7x7 Gaussian blur (sigma=2), reflect-101 boundary. in-place on a
 * scratch copy. dst may == src. */
static void ngd_gaussian_blur(const uint8_t *src, int w, int h, uint8_t *dst)
{
    uint8_t *tmp = (uint8_t*)malloc((size_t)w * h);
    if (!tmp) { if (dst != src) memcpy(dst, src, (size_t)w*h); return; }
    /* horizontal */
    for (int y = 0; y < h; ++y) {
        const uint8_t *row = src + y*w;
        for (int x = 0; x < w; ++x) {
            float s = 0;
            for (int k = -3; k <= 3; ++k) {
                int xx = x + k;
                if (xx < 0) xx = -xx;            /* reflect-101 */
                if (xx >= w) xx = 2*(w-1) - xx;
                s += ngd_gauss7[k+3] * row[xx];
            }
            tmp[y*w + x] = (uint8_t)(s + 0.5f);
        }
    }
    /* vertical */
    for (int x = 0; x < w; ++x) {
        for (int y = 0; y < h; ++y) {
            float s = 0;
            for (int k = -3; k <= 3; ++k) {
                int yy = y + k;
                if (yy < 0) yy = -yy;
                if (yy >= h) yy = 2*(h-1) - yy;
                s += ngd_gauss7[k+3] * tmp[yy*w + x];
            }
            dst[y*w + x] = (uint8_t)(s + 0.5f);
        }
    }
    free(tmp);
}

/* ===================================================================== *
 *  FAST-9_16 corner detection + score + non-max suppression
 * ===================================================================== */
/* For pixel p at (x0,y0) with threshold t: returns the FAST score if it is a
 * FAST-9 corner, else -1. Score = the max threshold for which it stays a
 * corner = best (brighter | darker) contiguous-9 margin. */
static int ngd_fast_score(const uint8_t *img, int w, int h, int stride,
                          int x0, int y0, int t)
{
    int p = img[y0*stride + x0];
    int v[16];
    for (int i = 0; i < 16; ++i) {
        int x = x0 + ngd_fast_circle[i][0];
        int y = y0 + ngd_fast_circle[i][1];
        if (x < 0 || x >= w || y < 0 || y >= h) return -1;
        v[i] = img[y*stride + x];
    }
    /* speed test: cardinal pixels (indices 0,4,8,12) */
    int hi = 0, lo = 0;
    const int card[4] = {0, 4, 8, 12};
    for (int k = 0; k < NGD_FAST_CARDINAL_COUNT; ++k) {
        if (v[card[k]] > p + t) hi++;
        else if (v[card[k]] < p - t) lo++;
    }
    if (hi < 2 && lo < 2) return -1;   /* a 9-contiguous arc spans >=2 cardinals,
                                        * so <2 bright AND <2 dark cannot be a corner */

    int best_hi = -1, best_lo = -1;
    for (int s = 0; s < 16; ++s) {
        int allhi = 1, alllo = 1, minhi = 255, maxlo = 0;
        for (int k = 0; k < 9; ++k) {
            int val = v[(s + k) & 15];
            if (val > p + t) { if (val < minhi) minhi = val; } else allhi = 0;
            if (val < p - t) { if (val > maxlo) maxlo = val; } else alllo = 0;
        }
        if (allhi) { int m = minhi - p; if (m > best_hi) best_hi = m; }
        if (alllo) { int m = p - maxlo; if (m > best_lo) best_lo = m; }
    }
    int best = best_hi > best_lo ? best_hi : best_lo;
    if (best < 0) return -1;
    return best;
}

/* Detect FAST corners in ROI with threshold t; ROI-relative coords.
 * Non-max suppression keeps local score maxima in a 3x3 window. */
static void ngd_fast_detect(const uint8_t *img, int w, int h, int stride,
                            int iniX, int maxX, int iniY, int maxY, int t,
                            ngd_keypoint *out, int *n_inout, int cap)
{
    for (int y = iniY; y < maxY; ++y) {
        for (int x = iniX; x < maxX; ++x) {
            int s = ngd_fast_score(img, w, h, stride, x, y, t);
            if (s < 0) continue;
            int is_max = 1;
            for (int dy = -1; dy <= 1 && is_max; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dy == 0) continue;
                    int xn = x + dx, yn = y + dy;
                    if (xn < iniX || xn >= maxX || yn < iniY || yn >= maxY) continue;
                    int sn = ngd_fast_score(img, w, h, stride, xn, yn, t);
                    if (sn > s) { is_max = 0; break; }
                }
            if (!is_max) continue;
            if (*n_inout >= cap) return;
            ngd_keypoint *kp = &out[(*n_inout)++];
            kp->x = (float)x; kp->y = (float)y;
            kp->response = (float)s; kp->octave = 0; kp->angle = 0.0f; kp->size = 0.0f;
        }
    }
}

/* ===================================================================== *
 *  Quadtree distribution (DistributeOctTree, faithful port)
 * ===================================================================== */
typedef struct ngd_orb_node {
    int ULx, ULy, URx, URy, BLx, BLy, BRx, BRy;
    ngd_keypoint *vKeys;
    int nKeys, capKeys;
    int bNoMore;
    struct ngd_orb_node *prev, *next;
} ngd_orb_node;

static ngd_orb_node *ngd_node_new(int ULx,int ULy,int URx,int URy,int BLx,int BLy,int BRx,int BRy) {
    ngd_orb_node *n = (ngd_orb_node*)calloc(1, sizeof(ngd_orb_node));
    n->ULx=ULx; n->ULy=ULy; n->URx=URx; n->URy=URy;
    n->BLx=BLx; n->BLy=BLy; n->BRx=BRx; n->BRy=BRy;
    return n;
}
static void ngd_node_push(ngd_orb_node *n, const ngd_keypoint *kp) {
    if (n->nKeys == n->capKeys) {
        n->capKeys = n->capKeys ? n->capKeys*2 : 8;
        n->vKeys = (ngd_keypoint*)realloc(n->vKeys, n->capKeys * sizeof(ngd_keypoint));
    }
    n->vKeys[n->nKeys++] = *kp;
}
static void ngd_node_free(ngd_orb_node *n) { if (n) { free(n->vKeys); free(n); } }

static void ngd_list_push_front(ngd_orb_node **head, ngd_orb_node *n) {
    n->prev = NULL; n->next = *head;
    if (*head) (*head)->prev = n;
    *head = n;
}
static void ngd_list_erase(ngd_orb_node **head, ngd_orb_node *n) {
    if (n->prev) n->prev->next = n->next; else *head = n->next;
    if (n->next) n->next->prev = n->prev;
    ngd_node_free(n);
}
static int ngd_list_size(ngd_orb_node *head) { int c=0; for (; head; head=head->next) c++; return c; }

static void ngd_divide_node(ngd_orb_node *p, ngd_orb_node *n1, ngd_orb_node *n2,
                            ngd_orb_node *n3, ngd_orb_node *n4)
{
    int halfX = (int)ceil((float)(p->URx - p->ULx) / 2.0f);
    int halfY = (int)ceil((float)(p->BRy - p->ULy) / 2.0f);
    memset(n1,0,sizeof(*n1)); memset(n2,0,sizeof(*n2));
    memset(n3,0,sizeof(*n3)); memset(n4,0,sizeof(*n4));
    n1->ULx=p->ULx; n1->ULy=p->ULy; n1->URx=p->ULx+halfX; n1->URy=p->ULy;
    n1->BLx=p->ULx; n1->BLy=p->ULy+halfY; n1->BRx=p->ULx+halfX; n1->BRy=p->ULy+halfY;
    n2->ULx=n1->URx; n2->ULy=n1->URy; n2->URx=p->URx; n2->URy=p->URy;
    n2->BLx=n1->BRx; n2->BLy=n1->BRy; n2->BRx=p->URx; n2->BRy=p->ULy+halfY;
    n3->ULx=n1->BLx; n3->ULy=n1->BLy; n3->URx=n1->BRx; n3->URy=n1->BRy;
    n3->BLx=p->BLx; n3->BLy=p->BLy; n3->BRx=n1->BRx; n3->BRy=p->BLy;
    n4->ULx=n3->URx; n4->ULy=n3->URy; n4->URx=n2->BRx; n4->URy=n2->BRy;
    n4->BLx=n3->BRx; n4->BLy=n3->BRy; n4->BRx=p->BRx; n4->BRy=p->BRy;
    for (int i = 0; i < p->nKeys; ++i) {
        const ngd_keypoint *kp = &p->vKeys[i];
        if (kp->x < n1->URx) { if (kp->y < n1->BRy) ngd_node_push(n1,kp); else ngd_node_push(n3,kp); }
        else if (kp->y < n1->BRy) ngd_node_push(n2,kp); else ngd_node_push(n4,kp);
    }
    n1->bNoMore = (n1->nKeys == 1);
    n2->bNoMore = (n2->nKeys == 1);
    n3->bNoMore = (n3->nKeys == 1);
    n4->bNoMore = (n4->nKeys == 1);
}

typedef struct { int size; ngd_orb_node *node; } ngd_size_ptr;
static int ngd_cmp_size_ptr(const void *a, const void *b) {
    const ngd_size_ptr *sa = a, *sb = b;
    if (sa->size != sb->size) return sa->size - sb->size;
    if (sa->node->ULx < sb->node->ULx) return -1;
    if (sa->node->ULx > sb->node->ULx) return 1;
    return 0;
}

static int ngd_distribute_octtree(const ngd_keypoint *keys, int nkeys,
                                  int minX, int maxX, int minY, int maxY,
                                  int N, ngd_keypoint *out)
{
    int W = maxX - minX, H = maxY - minY;
    if (H <= 0 || W <= 0 || N <= 0) return 0;
    int nIni = (int)lroundf((float)W / (float)H);
    if (nIni < 1) nIni = 1;
    float hX = (float)W / nIni;

    ngd_orb_node *head = NULL;
    ngd_orb_node **iniNodes = (ngd_orb_node**)calloc(nIni, sizeof(ngd_orb_node*));
    for (int i = 0; i < nIni; ++i) {
        ngd_orb_node *ni = ngd_node_new((int)(hX*i),0,(int)(hX*(i+1)),0,(int)(hX*i),H,(int)(hX*(i+1)),H);
        ngd_list_push_front(&head, ni);
        iniNodes[i] = ni;
    }
    for (int i = 0; i < nkeys; ++i) {
        int idx = (int)(keys[i].x / hX);
        if (idx < 0) idx = 0; if (idx >= nIni) idx = nIni-1;
        ngd_node_push(iniNodes[idx], &keys[i]);
    }
    {
        ngd_orb_node *cur = head;
        while (cur) {
            ngd_orb_node *nx = cur->next;
            if (cur->nKeys == 1) cur->bNoMore = 1;
            if (cur->nKeys == 0) ngd_list_erase(&head, cur);
            cur = nx;
        }
    }

    int bFinish = 0;
    while (!bFinish) {
        int prevSize = ngd_list_size(head);
        int nToExpand = 0;
        ngd_size_ptr *sp = (ngd_size_ptr*)malloc(sizeof(ngd_size_ptr) * (size_t)(prevSize*4 + 8));
        int nsp = 0;

        ngd_orb_node *cur = head;
        while (cur) {
            ngd_orb_node *nx = cur->next;
            if (!cur->bNoMore) {
                ngd_orb_node n1,n2,n3,n4;
                ngd_divide_node(cur, &n1,&n2,&n3,&n4);
                ngd_orb_node *kids[4] = {&n1,&n2,&n3,&n4};
                for (int k = 0; k < 4; ++k) {
                    if (kids[k]->nKeys > 0) {
                        ngd_orb_node *heap = ngd_node_new(kids[k]->ULx,kids[k]->ULy,kids[k]->URx,kids[k]->URy,kids[k]->BLx,kids[k]->BLy,kids[k]->BRx,kids[k]->BRy);
                        for (int j = 0; j < kids[k]->nKeys; ++j) ngd_node_push(heap, &kids[k]->vKeys[j]);
                        heap->bNoMore = (heap->nKeys == 1);
                        ngd_list_push_front(&head, heap);
                        if (heap->nKeys > 1) { sp[nsp].size = heap->nKeys; sp[nsp].node = heap; nsp++; nToExpand++; }
                    }
                }
                ngd_list_erase(&head, cur);
            }
            cur = nx;
        }

        if (ngd_list_size(head) >= N || ngd_list_size(head) == prevSize) {
            bFinish = 1;
        } else if (ngd_list_size(head) + nToExpand*3 > N) {
            while (!bFinish) {
                int prevSize2 = ngd_list_size(head);
                ngd_size_ptr *prev = sp; int nprev = nsp;
                sp = (ngd_size_ptr*)malloc(sizeof(ngd_size_ptr) * (size_t)(nprev*4 + 8));
                nsp = 0;
                qsort(prev, nprev, sizeof(ngd_size_ptr), ngd_cmp_size_ptr);
                for (int j = nprev-1; j >= 0 && !bFinish; --j) {
                    ngd_orb_node *par = prev[j].node;
                    if (!par) continue;
                    ngd_orb_node n1,n2,n3,n4;
                    ngd_divide_node(par, &n1,&n2,&n3,&n4);
                    ngd_orb_node *kids[4] = {&n1,&n2,&n3,&n4};
                    for (int k = 0; k < 4; ++k) {
                        if (kids[k]->nKeys > 0) {
                            ngd_orb_node *heap = ngd_node_new(kids[k]->ULx,kids[k]->ULy,kids[k]->URx,kids[k]->URy,kids[k]->BLx,kids[k]->BLy,kids[k]->BRx,kids[k]->BRy);
                            for (int z = 0; z < kids[k]->nKeys; ++z) ngd_node_push(heap, &kids[k]->vKeys[z]);
                            heap->bNoMore = (heap->nKeys == 1);
                            ngd_list_push_front(&head, heap);
                            if (heap->nKeys > 1) { sp[nsp].size = heap->nKeys; sp[nsp].node = heap; nsp++; }
                        }
                    }
                    ngd_list_erase(&head, par);
                    if (ngd_list_size(head) >= N) { bFinish = 1; break; }
                }
                free(prev);
                if (ngd_list_size(head) >= N || ngd_list_size(head) == prevSize2) bFinish = 1;
            }
        }
        free(sp);
    }

    int nout = 0;
    for (ngd_orb_node *cur = head; cur; cur = cur->next) {
        if (cur->nKeys == 0) continue;
        int best = 0; float mr = cur->vKeys[0].response;
        for (int k = 1; k < cur->nKeys; ++k)
            if (cur->vKeys[k].response > mr) { mr = cur->vKeys[k].response; best = k; }
        out[nout++] = cur->vKeys[best];
    }
    while (head) { ngd_orb_node *nx = head->next; ngd_node_free(head); head = nx; }
    free(iniNodes);
    return nout;
}

/* ===================================================================== *
 *  Orientation (IC_Angle) + descriptor (computeOrbDescriptor)
 * ===================================================================== */
static float ngd_ic_angle(const uint8_t *img, int w, int h,
                          float ptx, float pty, const int *umax)
{
    int m_01 = 0, m_10 = 0;
    int cx = (int)lroundf(ptx), cy = (int)lroundf(pty);

    /* center row, v=0 */
    for (int u = -NGD_ORB_HALF_PATCH; u <= NGD_ORB_HALF_PATCH; ++u)
        m_10 += u * (int)ngd_pix_clamp(img, w, h, cx+u, cy);

    for (int v = 1; v <= NGD_ORB_HALF_PATCH; ++v) {
        int v_sum = 0;
        int d = umax[v];
        for (int u = -d; u <= d; ++u) {
            int val_plus  = (int)ngd_pix_clamp(img, w, h, cx+u, cy+v);
            int val_minus = (int)ngd_pix_clamp(img, w, h, cx+u, cy-v);
            v_sum += (val_plus - val_minus);
            m_10 += u * (val_plus + val_minus);
        }
        m_01 += v * v_sum;
    }
    return atan2f((float)m_01, (float)m_10) * (float)(180.0 / NGD_PI);   /* degrees [0,360) */
}

/* compute one 32-byte descriptor; img is the blurred level image. */
static void ngd_compute_descriptor(const ngd_keypoint *kp, const uint8_t *img,
                                   int w, int h, uint8_t *desc)
{
    float angle = kp->angle * (float)(NGD_PI / 180.0);
    float a = cosf(angle), b = sinf(angle);
    int cx = (int)lroundf(kp->x), cy = (int)lroundf(kp->y);

    for (int i = 0; i < 32; ++i) {
        int val = 0;
        for (int bit = 0; bit < 8; ++bit) {
            int pair = i*8 + bit;
            /* point1 = pattern[pair*2], point2 = pattern[pair*2+1] */
            int p1x = ngd_bit_pattern_31[pair][0], p1y = ngd_bit_pattern_31[pair][1];
            int p2x = ngd_bit_pattern_31[pair][2], p2y = ngd_bit_pattern_31[pair][3];
            /* GET_VALUE: row_off=round(px*b+py*a), col_off=round(px*a-py*b) */
            int r1 = (int)lroundf(p1x*b + p1y*a), c1 = (int)lroundf(p1x*a - p1y*b);
            int r2 = (int)lroundf(p2x*b + p2y*a), c2 = (int)lroundf(p2x*a - p2y*b);
            int t0 = ngd_pix_clamp(img, w, h, cx+c1, cy+r1);
            int t1 = ngd_pix_clamp(img, w, h, cx+c2, cy+r2);
            val |= (t0 < t1) << bit;
        }
        desc[i] = (uint8_t)val;
    }
}

/* ===================================================================== *
 *  Extract driver (operator())
 * ===================================================================== */
int ngd_orb_extract(const ngd_orb_extractor *ex,
                    const uint8_t *gray, int w, int h, int stride,
                    ngd_keypoint *out_kps, int max_kps, uint8_t *out_desc)
{
    if (!ex || !gray || w <= 0 || h <= 0) return -1;

    /* pyramid buffers (level-local, no 19px border — keypoints inset >=16) */
    const uint8_t *pyr[NGD_ORB_MAX_LEVELS];
    int pyrW[NGD_ORB_MAX_LEVELS], pyrH[NGD_ORB_MAX_LEVELS];
    for (int level = 0; level < ex->nlevels; ++level) {
        int sw = (level == 0) ? w : pyrW[level-1];
        int sh = (level == 0) ? h : pyrH[level-1];
        int dw = (level == 0) ? w : (int)lroundf(w * ex->mvInvScaleFactor[level]);
        int dh = (level == 0) ? h : (int)lroundf(h * ex->mvInvScaleFactor[level]);
        if (dw < 2*NGD_ORB_EDGE_THRESHOLD || dh < 2*NGD_ORB_EDGE_THRESHOLD) { pyr[level]=NULL; pyrW[level]=0; pyrH[level]=0; continue; }
        uint8_t *buf = (uint8_t*)malloc((size_t)dw*dh);
        if (level == 0) {
            for (int y = 0; y < dh; ++y) memcpy(buf + y*dw, gray + y*stride, (size_t)dw);
        } else {
            ngd_resize_bilinear(pyr[level-1], sw, sh, buf, dw, dh);
        }
        pyr[level] = buf; pyrW[level] = dw; pyrH[level] = dh;
    }

    int n_out = 0;
    for (int level = 0; level < ex->nlevels; ++level) {
        const uint8_t *im = pyr[level];
        if (!im) continue;
        int pw = pyrW[level], ph = pyrH[level];

        int minBorderX = NGD_ORB_EDGE_THRESHOLD - 3;
        int minBorderY = minBorderX;
        int maxBorderX = pw - NGD_ORB_EDGE_THRESHOLD + 3;
        int maxBorderY = ph - NGD_ORB_EDGE_THRESHOLD + 3;
        float width = (float)(maxBorderX - minBorderX);
        float height = (float)(maxBorderY - minBorderY);
        if (width < 35 || height < 35) continue;

        int nCols = (int)(width / 35);
        int nRows = (int)(height / 35);
        if (nCols < 1) nCols = 1; if (nRows < 1) nRows = 1;
        int wCell = (int)ceil(width / nCols);
        int hCell = (int)ceil(height / nRows);

        ngd_keypoint *raw = (ngd_keypoint*)malloc(sizeof(ngd_keypoint) * (size_t)(ex->nfeatures*10));
        int nraw = 0, capraw = ex->nfeatures*10;

        for (int i = 0; i < nRows; ++i) {
            int iniY = minBorderY + i*hCell;
            int maxY = iniY + hCell + 6;
            if (iniY >= maxBorderY-3) continue;
            if (maxY > maxBorderY) maxY = maxBorderY;
            for (int j = 0; j < nCols; ++j) {
                int iniX = minBorderX + j*wCell;
                int maxX = iniX + wCell + 6;
                if (iniX >= maxBorderX-6) continue;
                if (maxX > maxBorderX) maxX = maxBorderX;
                int before = nraw;
                ngd_fast_detect(im, pw, ph, pw, iniX, maxX, iniY, maxY, ex->iniThFAST, raw, &nraw, capraw);
                if (nraw == before)   /* cell produced nothing at high threshold → retry low */
                    ngd_fast_detect(im, pw, ph, pw, iniX, maxX, iniY, maxY, ex->minThFAST, raw, &nraw, capraw);
            }
        }

        /* convert absolute pyramid coords -> minBorder-relative (DistributeOctTree
         * works in minBorder-relative space; we add minBorder back afterwards). */
        for (int k = 0; k < nraw; ++k) { raw[k].x -= (float)minBorderX; raw[k].y -= (float)minBorderY; }

        ngd_keypoint *kept = (ngd_keypoint*)malloc(sizeof(ngd_keypoint) * (size_t)(ex->nfeatures*2 + 64));
        int nkept = ngd_distribute_octtree(raw, nraw, minBorderX, maxBorderX, minBorderY, maxBorderY,
                                           ex->mnFeaturesPerLevel[level], kept);
        free(raw);

        int scaledPatch = (int)(NGD_ORB_PATCH_SIZE * ex->mvScaleFactor[level]);
        for (int i = 0; i < nkept; ++i) {
            kept[i].x += minBorderX;
            kept[i].y += minBorderY;
            kept[i].octave = level;
            kept[i].size = (float)scaledPatch;
        }

        /* blur this level for descriptors; orientation uses the raw level */
        uint8_t *blurred = (uint8_t*)malloc((size_t)pw*ph);
        ngd_gaussian_blur(im, pw, ph, blurred);

        for (int i = 0; i < nkept; ++i) {
            kept[i].angle = ngd_ic_angle(im, pw, ph, kept[i].x, kept[i].y, ex->umax);
            /* rescale to level-0 coords (level>0) */
            float sx = (level == 0) ? 1.0f : ex->mvScaleFactor[level];
            ngd_keypoint out = kept[i];
            out.x = kept[i].x * sx;
            out.y = kept[i].y * sx;
            if (n_out < max_kps) {
                out_kps[n_out] = out;
                if (out_desc) ngd_compute_descriptor(&kept[i], blurred, pw, ph, out_desc + n_out*32);
                n_out++;
            }
        }
        free(blurred);
        free(kept);
    }

    for (int level = 0; level < ex->nlevels; ++level) free((void*)pyr[level]);
    return n_out;
}
