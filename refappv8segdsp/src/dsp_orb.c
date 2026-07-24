/* dsp_orb.c — ORB extractor using VP6/DSP bit-exact kernels.
 *
 * Replicates ngd_orb_extract() using refoptvp6v2 _full() functions for
 * pixel operations (orb_resize_full, fast9_full, orb_blur_full,
 * ic_angle_full, brief_full). The quadtree spatial distribution and
 * coordinate bookkeeping are copied verbatim from refactor/src/orb.c.
 *
 * Build: plain C (also compiles as C++). Requires VP6 _core.h headers
 * (include/dsp/). The _full functions are pure C — no TileManager or
 * cstub dependency.
 *
 * Bit-exactness: each per-pixel kernel (blur, resize, FAST-9, IC_Angle,
 * rBRIEF) is validated against the refactor C reference in refoptvp6v2/test.
 * The quadtree distribution is a direct copy of refactor's code, so the
 * full pipeline output is identical modulo NMS differences at 35px cell
 * boundaries (fast9_full uses full-image NMS vs refactor's per-cell NMS).
 */
#include "ngd_app/dsp_orb.h"

#include "dsp/orb_resize_core.h"
#include "dsp/orb_blur_core.h"
#include "dsp/fast9_core.h"
#include "dsp/ic_angle_core.h"
#include "dsp/brief_core.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef NGD_PI
#define NGD_PI 3.14159265358979323846
#endif

/* ===================================================================== *
 *  Quadtree distribution (copied verbatim from refactor/src/orb.c:331-513)
 * ===================================================================== */
typedef struct dsp_orb_node {
    int ULx, ULy, URx, URy, BLx, BLy, BRx, BRy;
    ngd_keypoint *vKeys;
    int nKeys, capKeys;
    int bNoMore;
    struct dsp_orb_node *prev, *next;
} dsp_orb_node;

static dsp_orb_node *dsp_node_new(int ULx,int ULy,int URx,int URy,int BLx,int BLy,int BRx,int BRy) {
    dsp_orb_node *n = (dsp_orb_node*)calloc(1, sizeof(dsp_orb_node));
    n->ULx=ULx; n->ULy=ULy; n->URx=URx; n->URy=URy;
    n->BLx=BLx; n->BLy=BLy; n->BRx=BRx; n->BRy=BRy;
    return n;
}
static void dsp_node_push(dsp_orb_node *n, const ngd_keypoint *kp) {
    if (n->nKeys == n->capKeys) {
        n->capKeys = n->capKeys ? n->capKeys*2 : 8;
        n->vKeys = (ngd_keypoint*)realloc(n->vKeys, n->capKeys * sizeof(ngd_keypoint));
    }
    n->vKeys[n->nKeys++] = *kp;
}
static void dsp_node_free(dsp_orb_node *n) { if (n) { free(n->vKeys); free(n); } }

static void dsp_list_push_front(dsp_orb_node **head, dsp_orb_node *n) {
    n->prev = NULL; n->next = *head;
    if (*head) (*head)->prev = n;
    *head = n;
}
static void dsp_list_push_back(dsp_orb_node **head, dsp_orb_node **tail, dsp_orb_node *n) {
    n->prev = *tail; n->next = NULL;
    if (*tail) (*tail)->next = n; else *head = n;
    *tail = n;
}
static void dsp_list_erase(dsp_orb_node **head, dsp_orb_node *n) {
    if (n->prev) n->prev->next = n->next; else *head = n->next;
    if (n->next) n->next->prev = n->prev;
    dsp_node_free(n);
}
static int dsp_list_size(dsp_orb_node *head) { int c=0; for (; head; head=head->next) c++; return c; }

static void dsp_divide_node(dsp_orb_node *p, dsp_orb_node *n1, dsp_orb_node *n2,
                            dsp_orb_node *n3, dsp_orb_node *n4)
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
        if (kp->x < n1->URx) { if (kp->y < n1->BRy) dsp_node_push(n1,kp); else dsp_node_push(n3,kp); }
        else if (kp->y < n1->BRy) dsp_node_push(n2,kp); else dsp_node_push(n4,kp);
    }
    n1->bNoMore = (n1->nKeys == 1);
    n2->bNoMore = (n2->nKeys == 1);
    n3->bNoMore = (n3->nKeys == 1);
    n4->bNoMore = (n4->nKeys == 1);
}

typedef struct { int size; dsp_orb_node *node; } dsp_size_ptr;
static int dsp_cmp_size_ptr(const void *a, const void *b) {
    const dsp_size_ptr *sa = a, *sb = b;
    if (sa->size != sb->size) return sa->size - sb->size;
    if (sa->node->ULx < sb->node->ULx) return -1;
    if (sa->node->ULx > sb->node->ULx) return 1;
    return 0;
}

static int dsp_distribute_octtree(const ngd_keypoint *keys, int nkeys,
                                  int minX, int maxX, int minY, int maxY,
                                  int N, ngd_keypoint *out)
{
    int W = maxX - minX, H = maxY - minY;
    if (H <= 0 || W <= 0 || N <= 0) return 0;
    int nIni = (int)lroundf((float)W / (float)H);
    if (nIni < 1) nIni = 1;
    float hX = (float)W / nIni;

    dsp_orb_node *head = NULL, *tail = NULL;
    dsp_orb_node **iniNodes = (dsp_orb_node**)calloc(nIni, sizeof(dsp_orb_node*));
    for (int i = 0; i < nIni; ++i) {
        dsp_orb_node *ni = dsp_node_new((int)(hX*i),0,(int)(hX*(i+1)),0,(int)(hX*i),H,(int)(hX*(i+1)),H);
        dsp_list_push_back(&head, &tail, ni);
        iniNodes[i] = ni;
    }
    for (int i = 0; i < nkeys; ++i) {
        int idx = (int)(keys[i].x / hX);
        if (idx < 0) idx = 0; if (idx >= nIni) idx = nIni-1;
        dsp_node_push(iniNodes[idx], &keys[i]);
    }
    {
        dsp_orb_node *cur = head;
        while (cur) {
            dsp_orb_node *nx = cur->next;
            if (cur->nKeys == 1) cur->bNoMore = 1;
            if (cur->nKeys == 0) dsp_list_erase(&head, cur);
            cur = nx;
        }
    }

    int bFinish = 0;
    while (!bFinish) {
        int prevSize = dsp_list_size(head);
        int nToExpand = 0;
        dsp_size_ptr *sp = (dsp_size_ptr*)malloc(sizeof(dsp_size_ptr) * (size_t)(prevSize*4 + 8));
        int nsp = 0;

        dsp_orb_node *cur = head;
        while (cur) {
            dsp_orb_node *nx = cur->next;
            if (!cur->bNoMore) {
                dsp_orb_node n1,n2,n3,n4;
                dsp_divide_node(cur, &n1,&n2,&n3,&n4);
                dsp_orb_node *kids[4] = {&n1,&n2,&n3,&n4};
                for (int k = 0; k < 4; ++k) {
                    if (kids[k]->nKeys > 0) {
                        dsp_orb_node *heap = dsp_node_new(kids[k]->ULx,kids[k]->ULy,kids[k]->URx,kids[k]->URy,kids[k]->BLx,kids[k]->BLy,kids[k]->BRx,kids[k]->BRy);
                        for (int j = 0; j < kids[k]->nKeys; ++j) dsp_node_push(heap, &kids[k]->vKeys[j]);
                        heap->bNoMore = (heap->nKeys == 1);
                        dsp_list_push_front(&head, heap);
                        if (heap->nKeys > 1) { sp[nsp].size = heap->nKeys; sp[nsp].node = heap; nsp++; nToExpand++; }
                    }
                }
                dsp_list_erase(&head, cur);
            }
            cur = nx;
        }

        if (dsp_list_size(head) >= N || dsp_list_size(head) == prevSize) {
            bFinish = 1;
        } else if (dsp_list_size(head) + nToExpand*3 > N) {
            while (!bFinish) {
                int prevSize2 = dsp_list_size(head);
                dsp_size_ptr *prev = sp; int nprev = nsp;
                sp = (dsp_size_ptr*)malloc(sizeof(dsp_size_ptr) * (size_t)(nprev*4 + 8));
                nsp = 0;
                qsort(prev, nprev, sizeof(dsp_size_ptr), dsp_cmp_size_ptr);
                for (int j = nprev-1; j >= 0 && !bFinish; --j) {
                    dsp_orb_node *par = prev[j].node;
                    if (!par) continue;
                    dsp_orb_node n1,n2,n3,n4;
                    dsp_divide_node(par, &n1,&n2,&n3,&n4);
                    dsp_orb_node *kids[4] = {&n1,&n2,&n3,&n4};
                    for (int k = 0; k < 4; ++k) {
                        if (kids[k]->nKeys > 0) {
                            dsp_orb_node *heap = dsp_node_new(kids[k]->ULx,kids[k]->ULy,kids[k]->URx,kids[k]->URy,kids[k]->BLx,kids[k]->BLy,kids[k]->BRx,kids[k]->BRy);
                            for (int z = 0; z < kids[k]->nKeys; ++z) dsp_node_push(heap, &kids[k]->vKeys[z]);
                            heap->bNoMore = (heap->nKeys == 1);
                            dsp_list_push_front(&head, heap);
                            if (heap->nKeys > 1) { sp[nsp].size = heap->nKeys; sp[nsp].node = heap; nsp++; }
                        }
                    }
                    dsp_list_erase(&head, par);
                    if (dsp_list_size(head) >= N) { bFinish = 1; break; }
                }
                free(prev);
                if (dsp_list_size(head) >= N || dsp_list_size(head) == prevSize2) bFinish = 1;
            }
        }
        free(sp);
    }

    int nout = 0;
    for (dsp_orb_node *cur = head; cur; cur = cur->next) {
        if (cur->nKeys == 0) continue;
        int best = 0; float mr = cur->vKeys[0].response;
        for (int k = 1; k < cur->nKeys; ++k)
            if (cur->vKeys[k].response > mr) { mr = cur->vKeys[k].response; best = k; }
        out[nout++] = cur->vKeys[best];
    }
    while (head) { dsp_orb_node *nx = head->next; dsp_node_free(head); head = nx; }
    free(iniNodes);
    return nout;
}

/* ===================================================================== *
 *  DSP ORB extract driver
 * ===================================================================== */
int dsp_orb_extract(const ngd_orb_extractor *ex,
                    const uint8_t *gray, int w, int h, int stride,
                    ngd_keypoint *out_kps, int max_kps,
                    uint8_t *out_desc)
{
    if (!ex || !gray || w <= 0 || h <= 0) return -1;

    /* ---- Phase 1: build pyramid (orb_resize_full) ---- */
    const uint8_t *pyr[NGD_ORB_MAX_LEVELS];
    int pyrW[NGD_ORB_MAX_LEVELS], pyrH[NGD_ORB_MAX_LEVELS];
    for (int level = 0; level < ex->nlevels; ++level) {
        int sw = (level == 0) ? w : pyrW[level-1];
        int sh = (level == 0) ? h : pyrH[level-1];
        int dw = (level == 0) ? w : (int)lroundf((float)w * ex->mvInvScaleFactor[level]);
        int dh = (level == 0) ? h : (int)lroundf((float)h * ex->mvInvScaleFactor[level]);
        if (dw < 2*NGD_ORB_EDGE_THRESHOLD || dh < 2*NGD_ORB_EDGE_THRESHOLD) {
            pyr[level]=NULL; pyrW[level]=0; pyrH[level]=0; continue;
        }
        uint8_t *buf = (uint8_t*)malloc((size_t)dw*dh);
        if (level == 0) {
            for (int y = 0; y < dh; ++y) memcpy(buf + y*dw, gray + y*stride, (size_t)dw);
        } else {
            orb_resize_full(pyr[level-1], sw, sh, buf, dw, dh);
        }
        pyr[level] = buf; pyrW[level] = dw; pyrH[level] = dh;
    }

    /* ---- Phase 2: per-level extraction ---- */
    int n_out = 0;
    for (int level = 0; level < ex->nlevels; ++level) {
        const uint8_t *im = pyr[level];
        if (!im) continue;
        int pw = pyrW[level], ph = pyrH[level];

        /* ROI inset: EDGE_THRESHOLD(19) - 3(FAST border adjust) = 16, but
         * refactor uses minBorder = EDGE_THRESHOLD - 3 in both x and y. */
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

        /* FAST-9 detection per grid cell with adaptive threshold.
         * fast9_keypoint is layout-compatible with ngd_keypoint. */
        int maxKRaw = ex->nfeatures * 10;
        ngd_keypoint *raw = (ngd_keypoint*)malloc(sizeof(ngd_keypoint) * (size_t)maxKRaw);
        int nraw = 0;

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

                int ncell = 0;
                int remain = maxKRaw - nraw;
                fast9_full(im, pw, ph, pw, iniX, maxX, iniY, maxY, ex->iniThFAST,
                           (fast9_keypoint*)(raw + nraw), &ncell, remain);
                if (ncell == 0) {
                    /* cell produced nothing at high threshold → retry low */
                    fast9_full(im, pw, ph, pw, iniX, maxX, iniY, maxY, ex->minThFAST,
                               (fast9_keypoint*)(raw + nraw), &ncell, remain);
                }
                nraw += ncell;
            }
        }

        /* convert absolute pyramid coords → minBorder-relative for quadtree */
        for (int k = 0; k < nraw; ++k) {
            raw[k].x -= (float)minBorderX;
            raw[k].y -= (float)minBorderY;
        }

        /* quadtree spatial distribution */
        ngd_keypoint *kept = (ngd_keypoint*)malloc(sizeof(ngd_keypoint) * (size_t)(ex->nfeatures*2 + 64));
        int nkept = dsp_distribute_octtree(raw, nraw,
                                           minBorderX, maxBorderX, minBorderY, maxBorderY,
                                           ex->mnFeaturesPerLevel[level], kept);
        free(raw);

        /* restore absolute coords, set octave + size */
        int scaledPatch = (int)(NGD_ORB_PATCH_SIZE * ex->mvScaleFactor[level]);
        for (int i = 0; i < nkept; ++i) {
            kept[i].x += (float)minBorderX;
            kept[i].y += (float)minBorderY;
            kept[i].octave = level;
            kept[i].size = (float)scaledPatch;
        }

        /* Gaussian blur (DSP kernel); orientation uses raw image, descriptor uses blurred */
        uint8_t *blurred = (uint8_t*)malloc((size_t)pw*ph);
        orb_blur_full(im, pw, ph, pw, blurred);

        for (int i = 0; i < nkept; ++i) {
            /* IC_Angle on the unblurred level image (DSP kernel) */
            kept[i].angle = ic_angle_full(im, pw, ph, pw,
                                          (int)lroundf(kept[i].x), (int)lroundf(kept[i].y),
                                          ex->umax);

            /* rescale to level-0 coordinates */
            float sx = (level == 0) ? 1.0f : ex->mvScaleFactor[level];
            ngd_keypoint out = kept[i];
            out.x = kept[i].x * sx;
            out.y = kept[i].y * sx;

            if (n_out < max_kps) {
                out_kps[n_out] = out;
                /* rBRIEF on the blurred level image (DSP kernel) */
                if (out_desc) {
                    brief_full(blurred, pw, ph, pw,
                               (int)lroundf(kept[i].x), (int)lroundf(kept[i].y),
                               kept[i].angle,
                               out_desc + (size_t)n_out * 32);
                }
                n_out++;
            }
        }
        free(blurred);
        free(kept);
    }

    /* ---- Phase 3: cleanup ---- */
    for (int level = 0; level < ex->nlevels; ++level) free((void*)pyr[level]);
    return n_out;
}
