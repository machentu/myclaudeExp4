/* dsp_orb_octtree.c — quadtree spatial distribution, shared by the ORB
 * orchestrators.
 *
 * Algorithm identical to refactor/src/orb.c:331-513 (the code dsp_orb.c
 * carried inline since refappv8segdsp; extracted so the pure-C and VP6+IVP
 * orchestrators distribute identical keypoint sets identically). The inner
 * form is reworked for the DSP core (no FPU, no D-cache) with byte-identical
 * output — every change below is a pure restatement of the original's
 * arithmetic and ordering:
 *   - a live node counter replaces dsp_list_size(): the original re-walked
 *     the whole linked list (one DRAM pointer-chase per node) for every size
 *     check, and the sorted-expansion loop checked after EVERY divided node —
 *     O(nodes) per check, O(nodes^2) per round, all uncached;
 *   - a division moves each keypoint ONCE: the original pushed into four
 *     stack nodes (realloc-doubling arrays) and then copied every key again
 *     into freshly calloc'd heap nodes (more realloc doubling). Now one pass
 *     counts the quadrants (indices stashed in qtmp so the fill pass does
 *     not redo the float compares), each kid gets one exact-size malloc, and
 *     the fill pass preserves the original per-node key order (parent key
 *     order) and creation order (n1,n2,n3,n4 — kids with zero keys are not
 *     created, exactly like before);
 *   - the split point is integer:
 *         (int)ceil((float)w / 2.0f) == (w + 1) / 2   for every integer w >= 0
 *     (w <= 640 < 2^24 is exact in float, /2.0f only decrements the exponent,
 *     and ceil of the exact half-integer rounds odd w up) — kills two
 *     soft-float divisions + two ceil library calls per division.
 * Verified byte-identical by test_bitexact_ab (the legacy orchestrator in
 * test/ref_legacy carries the ORIGINAL quadtree verbatim).
 */
#include "dsp/dsp_orb_octtree.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ===================================================================== *
 *  Quadtree distribution (algorithm of refactor/src/orb.c:331-513)
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

/*
 * Divide p into up to four fresh nodes pushed at the FRONT of the list, in
 * the original creation order (n1,n2,n3,n4); a quadrant with zero keys gets
 * no node, exactly like the original. kids[] receives the created nodes
 * (NULL for the empty quadrants) so the caller can record the expandable
 * ones in the same k order. qtmp (>= p->nKeys bytes) receives each key's
 * quadrant index so the fill pass does not redo the float compares. Returns
 * the number of nodes created (the caller's live-node-count delta).
 *
 * Quadrant test is the original's: x < xmid && y < ymid -> n1, x < xmid &&
 * y >= ymid -> n3, x >= xmid && y < ymid -> n2, else n4, with xmid/ymid the
 * original's halfX/halfY boundaries — restated as q = (x>=xmid) + 2*(y>=ymid).
 */
static int dsp_divide_to_list(dsp_orb_node *p, dsp_orb_node **head,
                              unsigned char *qtmp, dsp_orb_node *kids[4])
{
    /* (int)ceil((float)w / 2.0f) == (w+1)/2 for every integer w >= 0 (see
     * the file header) — no soft-float division, no ceil call. */
    const int halfX = (p->URx - p->ULx + 1) >> 1;
    const int halfY = (p->BRy - p->ULy + 1) >> 1;
    const int xmid = p->ULx + halfX, ymid = p->ULy + halfY;

    int cnt[4] = { 0, 0, 0, 0 };
    for (int i = 0; i < p->nKeys; ++i) {
        const int q = (p->vKeys[i].x >= xmid ? 1 : 0)
                    + (p->vKeys[i].y >= ymid ? 2 : 0);
        qtmp[i] = (unsigned char)q;
        ++cnt[q];
    }

    /* child rects exactly as the original assigned them from halfX/halfY
     * (row k = {ULx,ULy,URx,URy,BLx,BLy,BRx,BRy} of kid k = n1,n2,n3,n4) */
    const int rect[4][8] = {
        { p->ULx, p->ULy, xmid,     p->ULy, p->ULx, ymid, xmid,     ymid     },
        { xmid,     p->ULy, p->URx, p->URy, xmid,     ymid, p->URx, ymid     },
        { p->ULx, ymid,     xmid,     ymid,     p->BLx, p->BLy, xmid,     p->BLy },
        { xmid,     ymid,     p->URx, ymid,     xmid,     p->BLy, p->BRx, p->BRy },
    };
    int created = 0;
    for (int k = 0; k < 4; ++k) {
        kids[k] = NULL;
        if (cnt[k] == 0) continue;
        const int *o = rect[k];
        dsp_orb_node *heap = dsp_node_new(o[0], o[1], o[2], o[3],
                                          o[4], o[5], o[6], o[7]);
        heap->vKeys = (ngd_keypoint*)malloc((size_t)cnt[k] * sizeof(ngd_keypoint));
        heap->capKeys = cnt[k];
        int m = 0;
        for (int i = 0; i < p->nKeys; ++i)
            if (qtmp[i] == k) heap->vKeys[m++] = p->vKeys[i];
        heap->nKeys = m;
        heap->bNoMore = (m == 1);
        dsp_list_push_front(head, heap);
        kids[k] = heap;
        ++created;
    }
    return created;
}

typedef struct { int size; dsp_orb_node *node; } dsp_size_ptr;
static int dsp_cmp_size_ptr(const void *a, const void *b) {
    const dsp_size_ptr *sa = a, *sb = b;
    if (sa->size != sb->size) return sa->size - sb->size;
    if (sa->node->ULx < sb->node->ULx) return -1;
    if (sa->node->ULx > sb->node->ULx) return 1;
    return 0;
}

int dsp_distribute_octtree(const ngd_keypoint *keys, int nkeys,
                                  int minX, int maxX, int minY, int maxY,
                                  int N, ngd_keypoint *out)
{
    int W = maxX - minX, H = maxY - minY;
    if (H <= 0 || W <= 0 || N <= 0) return 0;
    int nIni = (int)lroundf((float)W / (float)H);
    if (nIni < 1) nIni = 1;
    float hX = (float)W / nIni;

    /* quadrant stash for the divisions (one byte per key of the largest
     * node = the initial distribution) */
    unsigned char *qtmp = (unsigned char*)malloc((size_t)(nkeys > 0 ? nkeys : 1));

    dsp_orb_node *head = NULL, *tail = NULL;
    int nList = 0;                       /* live list size (dsp_list_size was O(n) DRAM walk) */
    dsp_orb_node **iniNodes = (dsp_orb_node**)calloc(nIni, sizeof(dsp_orb_node*));
    for (int i = 0; i < nIni; ++i) {
        dsp_orb_node *ni = dsp_node_new((int)(hX*i),0,(int)(hX*(i+1)),0,(int)(hX*i),H,(int)(hX*(i+1)),H);
        dsp_list_push_back(&head, &tail, ni);
        iniNodes[i] = ni;
        ++nList;
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
            if (cur->nKeys == 0) { dsp_list_erase(&head, cur); --nList; }
            cur = nx;
        }
    }

    int bFinish = 0;
    while (!bFinish) {
        int prevSize = nList;
        int nToExpand = 0;
        dsp_size_ptr *sp = (dsp_size_ptr*)malloc(sizeof(dsp_size_ptr) * (size_t)(prevSize*4 + 8));
        int nsp = 0;

        dsp_orb_node *cur = head;
        while (cur) {
            dsp_orb_node *nx = cur->next;
            if (!cur->bNoMore) {
                dsp_orb_node *kids[4];
                nList += dsp_divide_to_list(cur, &head, qtmp, kids);
                for (int k = 0; k < 4; ++k) {
                    if (kids[k] && kids[k]->nKeys > 1) {
                        sp[nsp].size = kids[k]->nKeys; sp[nsp].node = kids[k]; nsp++;
                        nToExpand++;
                    }
                }
                dsp_list_erase(&head, cur); --nList;
            }
            cur = nx;
        }

        if (nList >= N || nList == prevSize) {
            bFinish = 1;
        } else if (nList + nToExpand*3 > N) {
            while (!bFinish) {
                int prevSize2 = nList;
                dsp_size_ptr *prev = sp; int nprev = nsp;
                sp = (dsp_size_ptr*)malloc(sizeof(dsp_size_ptr) * (size_t)(nprev*4 + 8));
                nsp = 0;
                qsort(prev, nprev, sizeof(dsp_size_ptr), dsp_cmp_size_ptr);
                for (int j = nprev-1; j >= 0 && !bFinish; --j) {
                    dsp_orb_node *par = prev[j].node;
                    if (!par) continue;
                    dsp_orb_node *kids[4];
                    nList += dsp_divide_to_list(par, &head, qtmp, kids);
                    for (int k = 0; k < 4; ++k) {
                        if (kids[k] && kids[k]->nKeys > 1) {
                            sp[nsp].size = kids[k]->nKeys; sp[nsp].node = kids[k]; nsp++;
                        }
                    }
                    dsp_list_erase(&head, par); --nList;
                    if (nList >= N) { bFinish = 1; break; }
                }
                free(prev);
                if (nList >= N || nList == prevSize2) bFinish = 1;
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
    free(qtmp);
    return nout;
}
