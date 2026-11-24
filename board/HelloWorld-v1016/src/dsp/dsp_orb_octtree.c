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
 *
 * v1009 pool form (same output, no per-node malloc): board v1006-v1008
 * measured quadtree = 307 ms/frame, malloc-bound (calloc+malloc+free pair
 * = 2041 cycles on this core, ~10k pairs/frame) plus uncached DDR pointer
 * chasing through malloc'd nodes. The tree now lives in one flat node pool
 * (realloc-doubling array, indices instead of pointers) and all keys in ONE
 * arena: a node owns a [base, base+nKeys) span, and a division is an in-place
 * STABLE 4-way partition of the parent span (bucket q = (x>=xmid) +
 * 2*(y>=ymid); target = start[q] + rank-within-bucket, applied by cycle
 * walk), so each kid's key order is still the parent key order and the
 * max-response tie-break (first strict > wins) sees the same sequence.
 * The doubly-linked list and the expand set (sp) become index arrays with
 * the same push_front / push_back / erase sequencing, so the emit order
 * (which IS the output order) matches the malloc form exactly. The init
 * phase keeps the original's per-key soft-float divide (int)(x / hX)
 * verbatim — replacing it by a reciprocal multiply would NOT be bit-exact.
 *
 * v1010 bank staging (same output again): v1009 removed the mallocs but left
 * every partition touch on the DDR heap (ld.heap = 83.9 cyc on this core;
 * rounds ran ~815 cyc per key visit, 176 ms/frame). When
 * dsp_octtree_set_scratch() has installed a bank-SRAM block + block mover,
 * each division stages its span into the scratch and partitions there
 * (scalar bank read ~1 cyc; the mover — an iDMA wait on the firmware —
 * carries the DDR traffic), while spans larger than the scratch fall back
 * to the in-place form (chunking an external partition with a single
 * buffer has a write-before-read hazard); the float compares run as
 * order-preserving integer compares (oct_ford); nIni == 1 skips the
 * initial partition outright (the clamp makes the divide's result
 * irrelevant — every key lands in strip 0). Without a scratch the v1009
 * in-place form runs unchanged.
 *
 * v1011 (same output): scratch budget 41 -> 28 bytes per record (the pos
 * pass re-classifies from the bank records instead of storing a qtmp
 * byte); the in-place fallback classifies with oct_ford too, so no soft
 * float compare remains anywhere; oct_blk_move pieces 4KB -> 16KB;
 * dsp_octtree_qcnt[4] exported for the CYB block (scratch bytes max/min,
 * staged / fallback key-visits).
 *
 * v1012 (same output): v1011 telemetry closed the cost equation — fallback
 * ~1150 and staged ~400-450 cyc per key-visit, i.e. the pos-array +
 * cycle-walk SHAPE (and per-division move latency), not the DDR-vs-bank
 * medium, dominates. Every partition (divisions and the nIni>1 initial
 * strips) is now a single-pass stable counting scatter through running
 * cursors: the fallback reads the span twice, writes temp once and moves
 * back once (~250/visit); the staged path counts on the bank and scatters
 * STRAIGHT into the final arena kid slots (no move-back, no permutation
 * scratch — writes never touch the source, so no hazard). qtmp/pos are
 * gone; the scratch is a pure record buffer at 24 bytes per record.
 */
#include "dsp/dsp_orb_octtree.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ---- v1006 board attribution: sub-phase cycle counters (init / rounds /
 * qsort / emit). Pure timing marks — no control-flow change; host builds
 * compile them away. The caller zeroes the array between measurements. */
unsigned long long dsp_octtree_cs[DSP_OCT_COUNT];
#ifdef __XTENSA__
static unsigned int oct_tick(void)
{
    unsigned int c;
    __asm__ volatile ("rsr.ccount %0" : "=a" (c));
    return c;
}
static unsigned int oct_m, oct_mq;
#define OCT_MARK()   do { oct_m = oct_tick(); } while (0)
#define OCT_ACC(i)   do { dsp_octtree_cs[i] += \
                          (unsigned long long)(oct_tick() - oct_m); } while (0)
#define OCT_MARKQ()  do { oct_mq = oct_tick(); } while (0)
#define OCT_ACCQ(i)  do { dsp_octtree_cs[i] += \
                          (unsigned long long)(oct_tick() - oct_mq); } while (0)
#else
#define OCT_MARK()   do { } while (0)
#define OCT_ACC(i)   do { } while (0)
#define OCT_MARKQ()  do { } while (0)
#define OCT_ACCQ(i)  do { } while (0)
#endif

/* ===================================================================== *
 *  Quadtree distribution (algorithm of refactor/src/orb.c:331-513)
 *  v1009: flat node pool + single key arena, indices for links.
 * ===================================================================== */
typedef struct {
    int ULx, ULy, URx, URy, BLx, BLy, BRx, BRy;
    int base;                        /* span start in the key arena        */
    int nKeys;
    int bNoMore;
    int prev, next;                  /* pool indices, -1 = none            */
} qnode;

/* file-static pool base: the qsort comparator runs without a user pointer */
static qnode *oct_pool;
static int    oct_cap, oct_used;

/* ---- v1010 bank staging state (installed via dsp_octtree_set_scratch) ----
 * Scratch = pure record staging: [ chunk_recs keypoint records ] (24 bytes
 * per record, chunk_recs = bytes / 24). v1012 removed the per-record pos
 * entry — the staged divide scatters through running cursors straight back
 * into the arena kid positions, so it stages records only and needs no
 * permutation scratch.
 * dsp_octtree_qcnt (exported like dsp_octtree_cs): [0] max, [1] min scratch
 * bytes installed across the frame's distribute calls, [2] key-visits through
 * the staged divide, [3] key-visits through the scatter fallback. */
static unsigned char *oct_scratch;
static int            oct_scratch_bytes;
static oct_move_fn    oct_move;
static void          *oct_move_ctx;
static ngd_keypoint  *oct_ch;        /* chunk record buffer inside scratch */
static int            oct_chrecs;
unsigned long long dsp_octtree_qcnt[4];

void dsp_octtree_set_scratch(void *buf, int bytes,
                             oct_move_fn move, void *move_ctx)
{
    oct_scratch = (unsigned char *)buf;
    oct_scratch_bytes = bytes;
    oct_move = move;
    oct_move_ctx = move_ctx;
}

/* block move in <=16KB pieces (v1011: 4KB -> 16KB, fewer submit+wait
 * round trips per division; the firmware mover falls back to memcpy when
 * the request is rejected, so correctness never depends on the piece size);
 * plain memcpy when no mover is installed */
static void oct_blk_move(void *dst, const void *src, int bytes)
{
    while (bytes > 0) {
        int c = bytes > 16384 ? 16384 : bytes;
        if (oct_move) oct_move(oct_move_ctx, dst, src, c);
        else          memcpy(dst, src, (size_t)c);
        dst = (char *)dst + c;
        src = (const char *)src + c;
        bytes -= c;
    }
}

/* v1010 order-preserving map of IEEE-754 bits to unsigned: sign-magnitude
 * -> offset-binary, so unsigned compares match float compares exactly for
 * every non-NaN pair. The only float-equal-but-T-different pair is
 * (-0.0f, +0.0f) (T(-0) < T(+0)): a divergence needs a *produced* -0.0f on
 * one side of a compare - impossible here, keypoint x/y/response come from
 * pixel coordinates and FAST scores (all >= +0, subtractions of equal
 * operands yield +0). Verified byte-exact against the original form. */
static inline unsigned oct_ford(unsigned bits)
{
    return (bits & 0x80000000u) ? ~bits : (bits | 0x80000000u);
}
static inline unsigned oct_fbits(float f)
{
    unsigned b;
    memcpy(&b, &f, sizeof b);
    return oct_ford(b);
}

static int oct_pool_grow(int want)
{
    if (want < oct_cap) return 0;
    int cap = oct_cap ? oct_cap : 512;
    while (cap <= want) cap *= 2;
    qnode *p = (qnode *)realloc(oct_pool, (size_t)cap * sizeof(qnode));
    if (!p) return -1;
    oct_pool = p;
    oct_cap = cap;
    return 0;
}

/* create a node with the given rect and span, push it at the FRONT of the
 * list; returns the pool index (never -1: growth is checked by the caller) */
static int oct_node_front(int *head, int ULx,int ULy,int URx,int URy,
                          int BLx,int BLy,int BRx,int BRy,
                          int base, int nKeys, int bNoMore)
{
    int i = oct_used++;
    qnode *n = &oct_pool[i];
    n->ULx=ULx; n->ULy=ULy; n->URx=URx; n->URy=URy;
    n->BLx=BLx; n->BLy=BLy; n->BRx=BRx; n->BRy=BRy;
    n->base=base; n->nKeys=nKeys; n->bNoMore=bNoMore;
    n->prev=-1; n->next=*head;
    if (*head >= 0) oct_pool[*head].prev = i;
    *head = i;
    return i;
}

static void oct_node_back(int *head, int *tail, int i)
{
    qnode *n = &oct_pool[i];
    n->prev = *tail; n->next = -1;
    if (*tail >= 0) oct_pool[*tail].next = i; else *head = i;
    *tail = i;
}

/* unlink only — pool nodes are released with the pool, never reused */
static void oct_list_erase(int *head, int i)
{
    qnode *n = &oct_pool[i];
    if (n->prev >= 0) oct_pool[n->prev].next = n->next; else *head = n->next;
    if (n->next >= 0) oct_pool[n->next].prev = n->prev;
}

/*
 * Divide node pi into up to four fresh nodes pushed at the FRONT of the
 * list, in the original creation order (n1,n2,n3,n4); a quadrant with zero
 * keys gets no node, exactly like the original. kids[] receives the created
 * pool indices (-1 for the empty quadrants). The parent's span [base,
 * base+nKeys) is stably partitioned into the four kid sub-spans (kid q
 * occupies [base+start[q], base+start[q]+cnt[q]), keys inside a kid still
 * in parent order) by scattering through running cursors into temp (>=
 * nKeys records) and moving the span back. Returns the number of nodes
 * created (the caller's live-node-count delta).
 *
 * Quadrant test is the original's: x < xmid && y < ymid -> n1, x < xmid &&
 * y >= ymid -> n3, x >= xmid && y < ymid -> n2, else n4, with xmid/ymid the
 * original's halfX/halfY boundaries — restated as q = (x>=xmid) + 2*(y>=ymid).
 */
static int oct_make_kids(int *head, int pULx, int pULy, int pURx, int pURy,
                         int pBLx, int pBLy, int pBRx, int pBRy,
                         int xmid, int ymid,
                         const int start[4], const int cnt[4], int kids[4]);
static int oct_divide(int pi, int *head,
                      ngd_keypoint *keys, ngd_keypoint *temp, int kids[4])
{
    /* snapshot: creating kids may realloc the pool */
    const int pULx = oct_pool[pi].ULx, pULy = oct_pool[pi].ULy;
    const int pURx = oct_pool[pi].URx, pURy = oct_pool[pi].URy;
    const int pBLx = oct_pool[pi].BLx, pBLy = oct_pool[pi].BLy;
    const int pBRx = oct_pool[pi].BRx, pBRy = oct_pool[pi].BRy;
    const int n = oct_pool[pi].nKeys, base = oct_pool[pi].base;

    /* (int)ceil((float)w / 2.0f) == (w+1)/2 for every integer w >= 0 (see
     * the file header) — no soft-float division, no ceil call. */
    const int halfX = (pURx - pULx + 1) >> 1;
    const int halfY = (pBRy - pULy + 1) >> 1;
    const int xmid = pULx + halfX, ymid = pULy + halfY;
    /* quadrant bounds as order-preserving ints (oct_ford): kills the two
     * soft-float compares per key */
    const unsigned bx = oct_fbits((float)xmid), by = oct_fbits((float)ymid);
    dsp_octtree_qcnt[3] += (unsigned long long)n;

    /* v1012 stable scatter partition: count the quadrants, then walk the
     * span once, writing each key through a per-quadrant running cursor
     * into temp, and move the span back. Same stable layout the old
     * qtmp+pos+cycle-walk form produced (ascending i through an advancing
     * cursor keeps parent order inside every kid), but a key now costs one
     * read + one write instead of the qtmp/pos round trips and the swap
     * walk — v1011 telemetry put that old shape at ~1150 cyc/key-visit. */
    int cnt[4] = { 0, 0, 0, 0 };
    for (int i = 0; i < n; ++i) {
        const int q = (oct_fbits(keys[base + i].x) >= bx ? 1 : 0)
                    + (oct_fbits(keys[base + i].y) >= by ? 2 : 0);
        ++cnt[q];
    }
    int start[4];
    start[0] = 0;
    start[1] = start[0] + cnt[0];
    start[2] = start[1] + cnt[1];
    start[3] = start[2] + cnt[2];
    int cur[4] = { start[0], start[1], start[2], start[3] };
    for (int i = 0; i < n; ++i) {
        const int q = (oct_fbits(keys[base + i].x) >= bx ? 1 : 0)
                    + (oct_fbits(keys[base + i].y) >= by ? 2 : 0);
        temp[cur[q]++] = keys[base + i];
    }
    oct_blk_move(keys + base, temp, n * (int)sizeof(ngd_keypoint));
    start[0] += base; start[1] += base;            /* absolute for the kids */
    start[2] += base; start[3] += base;

    /* child rects exactly as the original assigned them from halfX/halfY:
     * kid creation shared with the staged divide (row k = kid k) */
    return oct_make_kids(head, pULx, pULy, pURx, pURy,
                         pBLx, pBLy, pBRx, pBRy, xmid, ymid, start, cnt, kids);
}

/* create the up-to-four kids of a divided node at the FRONT of the list in
 * the original creation order (n1,n2,n3,n4; empty quadrant -> no node),
 * with spans [start[k], start[k]+cnt[k]). Returns nodes created. */
static int oct_make_kids(int *head, int pULx, int pULy, int pURx, int pURy,
                         int pBLx, int pBLy, int pBRx, int pBRy,
                         int xmid, int ymid,
                         const int start[4], const int cnt[4], int kids[4])
{
    const int rect[4][8] = {
        { pULx, pULy, xmid,     pULy, pULx, ymid, xmid,     ymid     },
        { xmid,     pULy, pURx, pURy, xmid,     ymid, pURx, ymid     },
        { pULx, ymid,     xmid,     ymid,     pBLx, pBLy, xmid,     pBLy },
        { xmid,     ymid,     pURx, ymid,     xmid,     pBLy, pBRx, pBRy },
    };
    if (oct_pool_grow(oct_used + 4) != 0) {
        kids[0] = kids[1] = kids[2] = kids[3] = -1;   /* allocation guard */
        return 0;
    }
    int created = 0;
    for (int k = 0; k < 4; ++k) {
        kids[k] = -1;
        if (cnt[k] == 0) continue;
        const int *o = rect[k];
        kids[k] = oct_node_front(head, o[0], o[1], o[2], o[3],
                                 o[4], o[5], o[6], o[7],
                                 start[k], cnt[k], cnt[k] == 1);
        ++created;
    }
    return created;
}

/*
 * v1010/v1012 staged divide: same partition as oct_divide for spans that fit
 * the scratch. The span rides the mover (iDMA on the firmware) into the bank
 * (scalar bank read ~1 cyc vs 83.9 in DDR), one pass counts the quadrants
 * there, and the scatter pass writes each key through a running cursor
 * STRAIGHT into its final arena kid position — the writes target disjoint
 * final slots (never the source, which lives in the bank), so there is no
 * write-before-read hazard and no move-back at all. The final arena layout
 * is the identical stable 4-way partition, so kid order and every downstream
 * tie-break match the scatter form byte for byte.
 *
 * Larger spans use oct_divide (the DDR scatter with the temp copy-back):
 * staging them would need an arena-sized bank block that never exists.
 */
static int oct_divide_bank(int pi, int *head,
                           ngd_keypoint *arena, ngd_keypoint *temp,
                           int kids[4])
{
    const int n = oct_pool[pi].nKeys, base = oct_pool[pi].base;
    if (n > oct_chrecs)
        return oct_divide(pi, head, arena, temp, kids);

    const int pULx = oct_pool[pi].ULx, pULy = oct_pool[pi].ULy;
    const int pURx = oct_pool[pi].URx, pURy = oct_pool[pi].URy;
    const int pBLx = oct_pool[pi].BLx, pBLy = oct_pool[pi].BLy;
    const int pBRx = oct_pool[pi].BRx, pBRy = oct_pool[pi].BRy;

    const int halfX = (pURx - pULx + 1) >> 1;
    const int halfY = (pBRy - pULy + 1) >> 1;
    const int xmid = pULx + halfX, ymid = pULy + halfY;
    /* quadrant bounds as order-preserving ints (see oct_ford) */
    const unsigned bx = oct_fbits((float)xmid), by = oct_fbits((float)ymid);

    int start[4], cnt[4] = { 0, 0, 0, 0 };
    oct_blk_move(oct_ch, arena + base, n * (int)sizeof(ngd_keypoint));
    for (int i = 0; i < n; ++i) {                    /* count on the bank */
        const int q = (oct_fbits(oct_ch[i].x) >= bx ? 1 : 0)
                    + (oct_fbits(oct_ch[i].y) >= by ? 2 : 0);
        ++cnt[q];
    }
    int lstart[4], lcur[4];                      /* chunk-local 0-based */
    lstart[0] = 0;
    lstart[1] = lstart[0] + cnt[0];
    lstart[2] = lstart[1] + cnt[1];
    lstart[3] = lstart[2] + cnt[2];
    lcur[0] = lstart[0]; lcur[1] = lstart[1];
    lcur[2] = lstart[2]; lcur[3] = lstart[3];
    for (int i = 0; i < n; ++i) {             /* scatter bank -> arena kids */
        const int q = (oct_fbits(oct_ch[i].x) >= bx ? 1 : 0)
                    + (oct_fbits(oct_ch[i].y) >= by ? 2 : 0);
        arena[base + lcur[q]++] = oct_ch[i];
    }
    dsp_octtree_qcnt[2] += (unsigned long long)n;
    start[0] = base + lstart[0];                 /* absolute for the kids */
    start[1] = base + lstart[1];
    start[2] = base + lstart[2];
    start[3] = base + lstart[3];
    return oct_make_kids(head, pULx, pULy, pURx, pURy,
                         pBLx, pBLy, pBRx, pBRy, xmid, ymid, start, cnt, kids);
}

typedef struct { int size; int node; } dsp_size_ptr;
/* Total order over distinct nodes: rects of live quadtree nodes are pairwise
 * distinct, so comparing the full UL/BR corner tuple never returns 0 for two
 * different nodes. The old ULx-only tie-break fell through to 0 whenever size
 * and ULx matched (e.g. the UL/BL children of one parent), leaving the tied
 * order to the libc qsort — MSVC (golden generation) and Xtensa newlib
 * (xt-run) order them differently, reordering the emitted keypoints. */
static int dsp_cmp_size_ptr(const void *a, const void *b) {
    const dsp_size_ptr *sa = a, *sb = b;
    if (sa->size != sb->size) return sa->size - sb->size;
    const qnode *na = &oct_pool[sa->node], *nb = &oct_pool[sb->node];
    if (na->ULx < nb->ULx) return -1;
    if (na->ULx > nb->ULx) return 1;
    if (na->ULy < nb->ULy) return -1;
    if (na->ULy > nb->ULy) return 1;
    if (na->BRx < nb->BRx) return -1;
    if (na->BRx > nb->BRx) return 1;
    if (na->BRy < nb->BRy) return -1;
    if (na->BRy > nb->BRy) return 1;
    return 0;
}

/* growable (dsp_size_ptr) array */
typedef struct { dsp_size_ptr *v; int n, cap; } sparr;
static int sp_push(sparr *s, int size, int node)
{
    if (s->n == s->cap) {
        int cap = s->cap ? s->cap * 2 : 256;
        dsp_size_ptr *v = (dsp_size_ptr *)realloc(s->v,
                                    (size_t)cap * sizeof(dsp_size_ptr));
        if (!v) return -1;
        s->v = v; s->cap = cap;
    }
    s->v[s->n].size = size; s->v[s->n].node = node; ++s->n;
    return 0;
}

int dsp_distribute_octtree(const ngd_keypoint *keys, int nkeys,
                                  int minX, int maxX, int minY, int maxY,
                                  int N, ngd_keypoint *out)
{
    int W = maxX - minX, H = maxY - minY;
    if (H <= 0 || W <= 0 || N <= 0) return 0;
    OCT_MARK();
    int nIni = (int)lroundf((float)W / (float)H);
    if (nIni < 1) nIni = 1;
    float hX = (float)W / nIni;

    /* single mutable key arena + one scatter scratch of the same size.
     * v1012: every partition (initial strips and node divisions) is a
     * stable counting scatter through running cursors — arena -> temp (or
     * bank -> arena when staged), move back — so the qtmp/pos buffers of
     * the v1009 form are gone entirely. The arena copy rides the block
     * mover (iDMA on the firmware). */
    ngd_keypoint *arena = (ngd_keypoint *)malloc(
        (size_t)(nkeys > 0 ? nkeys : 1) * sizeof(ngd_keypoint));
    ngd_keypoint *temp = (ngd_keypoint *)malloc(
        (size_t)(nkeys > 0 ? nkeys : 1) * sizeof(ngd_keypoint));
    if (!arena || !temp) {
        free(arena); free(temp); return 0;
    }
    oct_chrecs = 0;
    if (oct_scratch && oct_scratch_bytes >= 24) {   /* >= 1 record */
        oct_chrecs = oct_scratch_bytes / 24;
        oct_ch = (ngd_keypoint *)(void *)oct_scratch;
        if (dsp_octtree_qcnt[0] < (unsigned long long)oct_scratch_bytes)
            dsp_octtree_qcnt[0] = (unsigned long long)oct_scratch_bytes;
        if (dsp_octtree_qcnt[1] == 0 ||
            dsp_octtree_qcnt[1] > (unsigned long long)oct_scratch_bytes)
            dsp_octtree_qcnt[1] = (unsigned long long)oct_scratch_bytes;
    }
    oct_blk_move(arena, keys, (int)((size_t)nkeys * sizeof(ngd_keypoint)));
    if (oct_pool_grow(nIni) != 0) {
        free(arena); free(temp); return 0;
    }

    /* initial strip nodes (list order = creation order, as before) */
    int head = -1, tail = -1;
    int nList = 0;                       /* live list size (dsp_list_size was O(n) DRAM walk) */
    for (int i = 0; i < nIni; ++i) {
        const int idx = oct_used++;
        qnode *n = &oct_pool[idx];
        n->ULx=(int)(hX*i); n->ULy=0; n->URx=(int)(hX*(i+1)); n->URy=0;
        n->BLx=(int)(hX*i); n->BLy=H; n->BRx=(int)(hX*(i+1)); n->BRy=H;
        n->base = 0; n->nKeys = 0; n->bNoMore = 0;
        n->prev = -1; n->next = -1;
        oct_node_back(&head, &tail, idx);
        ++nList;
    }
    /* initial distribution: same per-key soft-float divide and clamp; stable
     * counting partition keeps each node's keys in key order (the malloc
     * form's dsp_node_push order).
     * v1010: nIni == 1 skips it outright — the clamp makes the divide's
     * result irrelevant ((int)(x/hX) clamped into [0, 0] is 0 for every x,
     * so every key lands in strip 0, base 0, nKeys = nkeys, no movement),
     * which saves nkeys soft-float divides per call. Production hits this
     * on every level (W/H stays ~4/3 down the pyramid). */
    if (nIni == 1) {
        oct_pool[head].base = 0;
        oct_pool[head].nKeys = nkeys;
    } else {
        int *cnt = (int *)calloc((size_t)nIni, sizeof(int));
        if (!cnt) { free(arena); free(temp); return 0; }
        for (int i = 0; i < nkeys; ++i) {
            int idx = (int)(arena[i].x / hX);
            if (idx < 0) idx = 0; if (idx >= nIni) idx = nIni-1;
            ++cnt[idx];
        }
        int *start = (int *)malloc((size_t)nIni * sizeof(int));
        if (!start) { free(cnt); free(arena); free(temp); return 0; }
        int acc = 0;
        for (int i = 0; i < nIni; ++i) { start[i] = acc; acc += cnt[i]; }
        int *cur = (int *)malloc((size_t)nIni * sizeof(int));
        if (!cur) { free(start); free(cnt); free(arena); free(temp); return 0; }
        memcpy(cur, start, (size_t)nIni * sizeof(int));
        /* same scatter shape as the divisions: the strip index expression
         * (soft-float divide + clamp) is evaluated verbatim in both passes
         * — deterministic soft float gives identical results, and ascending
         * i through advancing cursors keeps each strip's keys in key order,
         * the layout the qtmp+cycle-walk form produced */
        for (int i = 0; i < nkeys; ++i) {
            int idx = (int)(arena[i].x / hX);
            if (idx < 0) idx = 0; if (idx >= nIni) idx = nIni-1;
            temp[cur[idx]++] = arena[i];
        }
        oct_blk_move(arena, temp, (int)((size_t)nkeys * sizeof(ngd_keypoint)));
        /* initial nodes sit in the list in creation order: assign spans */
        {
            int idx = 0;
            for (int c = head; c >= 0; c = oct_pool[c].next) {
                oct_pool[c].base = start[idx];
                oct_pool[c].nKeys = cnt[idx];
                ++idx;
            }
        }
        free(cur); free(start); free(cnt);
    }
    {
        for (int c = head; c >= 0; ) {
            int nx = oct_pool[c].next;
            if (oct_pool[c].nKeys == 1) oct_pool[c].bNoMore = 1;
            if (oct_pool[c].nKeys == 0) { oct_list_erase(&head, c); --nList; }
            c = nx;
        }
    }
    OCT_ACC(DSP_OCT_INIT);
    OCT_MARK();

    sparr sp = { NULL, 0, 0 }, prev = { NULL, 0, 0 };
    int bFinish = 0;
    while (!bFinish) {
        int prevSize = nList;
        int nToExpand = 0;
        sp.n = 0;

        for (int c = head; c >= 0; ) {
            const int nx = oct_pool[c].next;
            if (!oct_pool[c].bNoMore) {
                int kids[4];
                nList += (oct_chrecs ? oct_divide_bank(c, &head, arena, temp, kids)
                                     : oct_divide(c, &head, arena, temp, kids));
                for (int k = 0; k < 4; ++k) {
                    if (kids[k] >= 0 && oct_pool[kids[k]].nKeys > 1) {
                        sp_push(&sp, oct_pool[kids[k]].nKeys, kids[k]);
                        nToExpand++;
                    }
                }
                oct_list_erase(&head, c); --nList;
            }
            c = nx;
        }

        if (nList >= N || nList == prevSize) {
            bFinish = 1;
        } else if (nList + nToExpand*3 > N) {
            while (!bFinish) {
                int prevSize2 = nList;
                sparr t = prev; prev = sp; sp = t;   /* swap roles */
                sp.n = 0;
                OCT_MARKQ();
                qsort(prev.v, (size_t)prev.n, sizeof(dsp_size_ptr), dsp_cmp_size_ptr);
                OCT_ACCQ(DSP_OCT_QSORT);
                for (int j = prev.n-1; j >= 0 && !bFinish; --j) {
                    const int par = prev.v[j].node;
                    if (par < 0) continue;
                    int kids[4];
                    nList += (oct_chrecs ? oct_divide_bank(par, &head, arena, temp, kids)
                                         : oct_divide(par, &head, arena, temp, kids));
                    for (int k = 0; k < 4; ++k) {
                        if (kids[k] >= 0 && oct_pool[kids[k]].nKeys > 1) {
                            sp_push(&sp, oct_pool[kids[k]].nKeys, kids[k]);
                        }
                    }
                    oct_list_erase(&head, par); --nList;
                    if (nList >= N) { bFinish = 1; break; }
                }
                if (nList >= N || nList == prevSize2) bFinish = 1;
            }
        }
    }
    OCT_ACC(DSP_OCT_ROUNDS);
    OCT_MARK();

    int nout = 0;
    for (int c = head; c >= 0; c = oct_pool[c].next) {
        const int base = oct_pool[c].base, n = oct_pool[c].nKeys;
        if (n == 0) continue;
        /* first strict max by response; the order-preserving int form of the
         * float compare (oct_ford) keeps every strict-> decision identical */
        int best = 0;
        unsigned mr = oct_fbits(arena[base].response);
        for (int k = 1; k < n; ++k) {
            const unsigned r = oct_fbits(arena[base + k].response);
            if (r > mr) { mr = r; best = k; }
        }
        out[nout++] = arena[base + best];
    }
    free(sp.v); free(prev.v);
    free(arena); free(temp);
    oct_used = 0;
    oct_chrecs = 0;
    OCT_ACC(DSP_OCT_EMIT);
    return nout;
}
