/* ngd/bow.c — Bag-of-Words vocabulary (pure C), faithful subset of DBoW2.
 * See ngd/bow.h for scope. Reference: Thirdparty/DBoW2/DBoW2/TemplatedVocabulary.h. */
#include "ngd/bow.h"
#include "ngd/matcher.h"   /* ngd_descriptor_distance (== FORB::distance) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ScoringType enum (BowVector.h:46-56): 0=L1_NORM. We only need to know that
 * L1 (0) requires L1 normalization of the BowVector. */
#define NGD_SCORING_L1 0

/* ============================ helpers ============================ */

static void *xmalloc(size_t n) { void *p = malloc(n); return p; }
static void *xcalloc(size_t n, size_t s) { void *p = calloc(n, s); return p; }

/* Build the children CSR from parent[]. Nodes are appended in nid order, so
 * each parent's children end up in ascending-nid order (matches DBoW2, which
 * appends in file order). */
static int build_csr(ngd_bow_vocab *v)
{
    v->child_off = (uint32_t*)xcalloc((size_t)v->n_nodes + 1, sizeof(uint32_t));
    if (!v->child_off) return -1;
    for (uint32_t i = 1; i < v->n_nodes; ++i)
        v->child_off[v->parent[i] + 1]++;
    for (uint32_t i = 0; i < v->n_nodes; ++i)
        v->child_off[i + 1] += v->child_off[i];
    v->child_idx = (uint32_t*)xmalloc((size_t)v->child_off[v->n_nodes] * sizeof(uint32_t));
    if (!v->child_idx) return -1;
    /* cursor = copy of prefix offsets */
    uint32_t *cur = (uint32_t*)xmalloc((size_t)v->n_nodes * sizeof(uint32_t));
    if (!cur) return -1;
    memcpy(cur, v->child_off, (size_t)v->n_nodes * sizeof(uint32_t));
    for (uint32_t nid = 1; nid < v->n_nodes; ++nid) {
        uint32_t p = v->parent[nid];
        v->child_idx[cur[p]++] = nid;
    }
    free(cur);
    return 0;
}

static void vocab_free_arrays(ngd_bow_vocab *v)
{
    free(v->parent); free(v->child_off); free(v->child_idx);
    free(v->desc); free(v->weight); free(v->word_id);
}

/* ============================ load (text) ============================ */

/* Read 32 decimal bytes and a double weight from a line that has already had
 * its "parent isLeaf" prefix consumed. Returns 0 on success. */
static int parse_node_rest(const char *p, uint8_t *desc, double *weight)
{
    for (int i = 0; i < NGD_BOW_DESC_LEN; ++i) {
        char *end;
        long v = strtol(p, &end, 10);
        if (end == p) return -1;
        desc[i] = (uint8_t)v;
        p = end;
    }
    char *end;
    double w = strtod(p, &end);
    if (end == p) return -1;
    *weight = w;
    return 0;
}

ngd_bow_vocab *ngd_bow_vocab_load_text(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return NULL;

    ngd_bow_vocab *v = (ngd_bow_vocab*)xcalloc(1, sizeof(ngd_bow_vocab));
    if (!v) { fclose(f); return NULL; }

    /* Header: k L scoring weighting (TemplatedVocabulary.h:1356-1372). */
    if (fscanf(f, "%d %d %d %d", &v->k, &v->L, &v->scoring, &v->weighting) != 4 ||
        v->k < 0 || v->k > 20 || v->L < 1 || v->L > 10) {
        free(v); fclose(f); return NULL;
    }

    /* Growable node arrays (root = index 0 is implicit). */
    uint32_t cap = 1024;
    v->parent  = (uint32_t*)xmalloc((size_t)cap * sizeof(uint32_t));
    v->desc    = (uint8_t *)xmalloc((size_t)cap * NGD_BOW_DESC_LEN);
    v->weight  = (double  *)xmalloc((size_t)cap * sizeof(double));
    v->word_id = (uint32_t*)xmalloc((size_t)cap * sizeof(uint32_t));
    if (!v->parent || !v->desc || !v->weight || !v->word_id) goto fail;

    /* root (nid 0) */
    v->n_nodes = 1;
    v->parent[0] = 0;
    memset(v->desc, 0, NGD_BOW_DESC_LEN);
    v->weight[0] = 0.0;
    v->word_id[0] = NGD_BOW_NONLEAF;

    char line[1 << 15];
    /* consume rest of header line */
    if (!fgets(line, sizeof(line), f)) goto fail;

    while (fgets(line, sizeof(line), f)) {
        /* skip blank lines */
        char *p = line;
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
        if (*p == '\0') continue;

        if (v->n_nodes >= cap) {
            cap *= 2;
            v->parent  = (uint32_t*)realloc(v->parent,  (size_t)cap * sizeof(uint32_t));
            v->desc    = (uint8_t *)realloc(v->desc,    (size_t)cap * NGD_BOW_DESC_LEN);
            v->weight  = (double  *)realloc(v->weight,  (size_t)cap * sizeof(double));
            v->word_id = (uint32_t*)realloc(v->word_id, (size_t)cap * sizeof(uint32_t));
            if (!v->parent || !v->desc || !v->weight || !v->word_id) goto fail;
        }

        char *end;
        long parent = strtol(p, &end, 10);
        if (end == p) continue;
        p = end;
        long isLeaf = strtol(p, &end, 10);
        if (end == p) continue;
        p = end;

        uint32_t nid = v->n_nodes;
        uint8_t desc[NGD_BOW_DESC_LEN];
        double w;
        if (parse_node_rest(p, desc, &w) != 0) continue;

        v->parent[nid] = (uint32_t)parent;
        memcpy(v->desc + (size_t)nid * NGD_BOW_DESC_LEN, desc, NGD_BOW_DESC_LEN);
        v->weight[nid] = w;
        if (isLeaf > 0) {
            v->word_id[nid] = v->n_words++;
        } else {
            v->word_id[nid] = NGD_BOW_NONLEAF;
        }
        v->n_nodes++;
    }
    fclose(f);

    if (build_csr(v) != 0) { vocab_free_arrays(v); free(v); return NULL; }
    return v;

fail:
    fclose(f);
    vocab_free_arrays(v);
    free(v);
    return NULL;
}

/* ============================ load (binary) ============================ */

ngd_bow_vocab *ngd_bow_vocab_load_binary(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    unsigned int nb_nodes, size_node, k, L, scoring, weighting;
    if (fread(&nb_nodes, sizeof(unsigned int), 1, f) != 1 ||
        fread(&size_node, sizeof(unsigned int), 1, f) != 1 ||
        fread(&k,        sizeof(unsigned int), 1, f) != 1 ||
        fread(&L,        sizeof(unsigned int), 1, f) != 1 ||
        fread(&scoring,  sizeof(unsigned int), 1, f) != 1 ||
        fread(&weighting,sizeof(unsigned int), 1, f) != 1) {
        fclose(f); return NULL;
    }
    /* DBoW2 record = parent(i32) + desc[32] + weight(float) + is_leaf(bool) = 41. */
    if (size_node != 4 + NGD_BOW_DESC_LEN + 4 + 1) { fclose(f); return NULL; }

    ngd_bow_vocab *v = (ngd_bow_vocab*)xcalloc(1, sizeof(ngd_bow_vocab));
    if (!v) { fclose(f); return NULL; }
    v->k = (int)k; v->L = (int)L; v->scoring = (int)scoring; v->weighting = (int)weighting;
    v->n_nodes = nb_nodes;
    v->parent  = (uint32_t*)xmalloc((size_t)nb_nodes * sizeof(uint32_t));
    v->desc    = (uint8_t *)xmalloc((size_t)nb_nodes * NGD_BOW_DESC_LEN);
    v->weight  = (double  *)xmalloc((size_t)nb_nodes * sizeof(double));
    v->word_id = (uint32_t*)xmalloc((size_t)nb_nodes * sizeof(uint32_t));
    if (!v->parent || !v->desc || !v->weight || !v->word_id) goto fail;

    /* root (nid 0) */
    v->parent[0] = 0;
    memset(v->desc, 0, NGD_BOW_DESC_LEN);
    v->weight[0] = 0.0;
    v->word_id[0] = NGD_BOW_NONLEAF;

    char *buf = (char*)xmalloc(size_node);
    if (!buf) goto fail;
    for (unsigned int nid = 1; nid < nb_nodes; ++nid) {
        if (fread(buf, 1, size_node, f) != size_node) { free(buf); goto fail; }
        int pval; memcpy(&pval, buf, 4);
        v->parent[nid] = (uint32_t)pval;
        memcpy(v->desc + (size_t)nid * NGD_BOW_DESC_LEN, buf + 4, NGD_BOW_DESC_LEN);
        float wf; memcpy(&wf, buf + 4 + NGD_BOW_DESC_LEN, 4);
        v->weight[nid] = (double)wf;
        if (buf[4 + NGD_BOW_DESC_LEN + 4]) {       /* is_leaf (offset 40) */
            v->word_id[nid] = v->n_words++;
        } else {
            v->word_id[nid] = NGD_BOW_NONLEAF;
        }
    }
    free(buf);
    fclose(f);

    if (build_csr(v) != 0) { vocab_free_arrays(v); free(v); return NULL; }
    return v;

fail:
    fclose(f);
    vocab_free_arrays(v);
    free(v);
    return NULL;
}

/* ============================ save (binary) ============================ */

int ngd_bow_vocab_save_binary(const ngd_bow_vocab *v, const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    unsigned int nb_nodes = v->n_nodes;
    unsigned int size_node = 4 + NGD_BOW_DESC_LEN + 4 + 1;
    unsigned int k = (unsigned)v->k, L = (unsigned)v->L;
    unsigned int scoring = (unsigned)v->scoring, weighting = (unsigned)v->weighting;
    fwrite(&nb_nodes, sizeof(unsigned int), 1, f);
    fwrite(&size_node, sizeof(unsigned int), 1, f);
    fwrite(&k, sizeof(unsigned int), 1, f);
    fwrite(&L, sizeof(unsigned int), 1, f);
    fwrite(&scoring, sizeof(unsigned int), 1, f);
    fwrite(&weighting, sizeof(unsigned int), 1, f);

    char buf[4 + NGD_BOW_DESC_LEN + 4 + 1];
    for (uint32_t nid = 1; nid < v->n_nodes; ++nid) {
        int pval = (int)v->parent[nid];
        memcpy(buf, &pval, 4);
        memcpy(buf + 4, v->desc + (size_t)nid * NGD_BOW_DESC_LEN, NGD_BOW_DESC_LEN);
        float wf = (float)v->weight[nid];
        memcpy(buf + 4 + NGD_BOW_DESC_LEN, &wf, 4);
        int nc = (int)(v->child_off[nid + 1] - v->child_off[nid]);
        buf[4 + NGD_BOW_DESC_LEN + 4] = (nc == 0) ? 1 : 0;   /* is_leaf */
        if (fwrite(buf, 1, size_node, f) != size_node) { fclose(f); return -1; }
    }
    fclose(f);
    return 0;
}

/* ============================ free ============================ */

void ngd_bow_vocab_free(ngd_bow_vocab *v)
{
    if (!v) return;
    vocab_free_arrays(v);
    free(v);
}

void ngd_bowvec_free(ngd_bowvec *bv)
{
    if (!bv) return;
    free(bv->items);
    bv->items = NULL; bv->n = bv->cap = 0;
}

void ngd_featvec_free(ngd_featvec *fv)
{
    if (!fv) return;
    for (int i = 0; i < fv->n; ++i) free(fv->items[i].idx);
    free(fv->items);
    fv->items = NULL; fv->n = fv->cap = 0;
}

/* ============================ transform ============================ */

/* Descend from root to a leaf, picking the min-Hamming child each level
 * (TemplatedVocabulary.h:1224-1265). Records the node at level (L-levelsup)
 * into *nid if non-NULL. */
static void transform_one(const ngd_bow_vocab *v, const uint8_t *desc,
                          uint32_t *word_id, double *weight, uint32_t *nid, int levelsup)
{
    int nid_level = v->L - levelsup;
    if (nid) *nid = 0;                       /* deterministic default */
    if (nid && nid_level <= 0) *nid = 0;

    uint32_t cur = 0;                        /* root */
    int current_level = 0;
    if ((int)(v->child_off[1] - v->child_off[0]) == 0) {   /* root is leaf */
        *word_id = v->word_id[0];
        *weight = v->weight[0];
        return;
    }
    do {
        ++current_level;
        uint32_t off = v->child_off[cur];
        int nc = (int)(v->child_off[cur + 1] - v->child_off[cur]);
        uint32_t best = v->child_idx[off];
        int best_d = ngd_descriptor_distance(desc, v->desc + (size_t)best * NGD_BOW_DESC_LEN);
        for (int i = 1; i < nc; ++i) {
            uint32_t c = v->child_idx[off + i];
            int d = ngd_descriptor_distance(desc, v->desc + (size_t)c * NGD_BOW_DESC_LEN);
            if (d < best_d) { best_d = d; best = c; }
        }
        cur = best;
        if (nid && current_level == nid_level) *nid = cur;
    } while ((int)(v->child_off[cur + 1] - v->child_off[cur]) != 0);

    *word_id = v->word_id[cur];
    *weight = v->weight[cur];
}

static int cmp_bowpair(const void *a, const void *b)
{
    uint32_t x = ((const ngd_bow_pair*)a)->id;
    uint32_t y = ((const ngd_bow_pair*)b)->id;
    return (x > y) - (x < y);
}

typedef struct { uint32_t nid; uint32_t feat; } feat_key;
static int cmp_featkey(const void *a, const void *b)
{
    const feat_key *x = (const feat_key*)a;
    const feat_key *y = (const feat_key*)b;
    if (x->nid != y->nid) return (x->nid > y->nid) - (x->nid < y->nid);
    /* stable ascending feature index within a node (DBoW2 addFeature appends in
     * i_feature order) */
    return (x->feat > y->feat) - (x->feat < y->feat);
}

void ngd_bow_transform(const ngd_bow_vocab *v,
                       const uint8_t *descriptors, int N, int levelsup,
                       ngd_bowvec *bv, ngd_featvec *fv)
{
    bv->items = NULL; bv->n = 0; bv->cap = 0;
    fv->items = NULL; fv->n = 0; fv->cap = 0;
    if (N <= 0) return;

    ngd_bow_pair *bp = (ngd_bow_pair*)xmalloc((size_t)N * sizeof(ngd_bow_pair));
    feat_key     *fk = (feat_key*)xmalloc((size_t)N * sizeof(feat_key));
    if (!bp || !fk) { free(bp); free(fk); return; }

    int nbp = 0;
    for (int i = 0; i < N; ++i) {
        uint32_t wid; double w; uint32_t nid;
        transform_one(v, descriptors + (size_t)i * NGD_BOW_DESC_LEN, &wid, &w, &nid, levelsup);
        if (w > 0.0) {
            bp[nbp].id = wid; bp[nbp].value = w; nbp++;
        }
        fk[i].nid = nid; fk[i].feat = (uint32_t)i;
    }

    /* ---- BowVector: sort by WordId, merge equal ids (addWeight) ---- */
    if (nbp > 0) {
        qsort(bp, (size_t)nbp, sizeof(ngd_bow_pair), cmp_bowpair);
        bv->cap = nbp;
        bv->items = (ngd_bow_pair*)xmalloc((size_t)nbp * sizeof(ngd_bow_pair));
        if (bv->items) {
            int n = 0;
            for (int i = 0; i < nbp; ++i) {
                if (n > 0 && bv->items[n - 1].id == bp[i].id)
                    bv->items[n - 1].value += bp[i].value;
                else
                    bv->items[n++] = bp[i];
            }
            bv->n = n;
        }
    }

    /* ---- FeatureVector: sort by NodeId, group feature indices ---- */
    qsort(fk, (size_t)N, sizeof(feat_key), cmp_featkey);
    fv->cap = N;
    fv->items = (ngd_feat_entry*)xmalloc((size_t)N * sizeof(ngd_feat_entry));
    if (fv->items) {
        int n = 0;
        for (int i = 0; i < N; ) {
            uint32_t nid = fk[i].nid;
            int j = i;
            while (j < N && fk[j].nid == nid) ++j;
            ngd_feat_entry *e = &fv->items[n++];
            e->id = nid; e->n = 0; e->cap = j - i;
            e->idx = (uint32_t*)xmalloc((size_t)e->cap * sizeof(uint32_t));
            if (!e->idx) { /* allocation failure: leave entry empty */ e->cap = 0; }
            else {
                for (int k = i; k < j; ++k) e->idx[e->n++] = fk[k].feat;
            }
            i = j;
        }
        fv->n = n;
    }

    free(bp);
    free(fk);

    /* ---- L1-normalize the BowVector (TF_IDF + L1 scoring → mustNormalize) ---- */
    if (v->scoring == NGD_SCORING_L1 && bv->n > 0) {
        double norm = 0.0;
        for (int i = 0; i < bv->n; ++i) norm += fabs(bv->items[i].value);
        if (norm > 0.0) {
            for (int i = 0; i < bv->n; ++i) bv->items[i].value /= norm;
        }
    }
}

/* ============================ L1 score ============================ */

/* L1Scoring::score (ScoringObject.cpp:23-68). Two-pointer merge; lower_bound
 * jumps are just pointer advances since both arrays are sorted ascending. */
double ngd_bow_score_l1(const ngd_bowvec *a, const ngd_bowvec *b)
{
    double score = 0.0;
    int i = 0, j = 0;
    while (i < a->n && j < b->n) {
        const ngd_bow_pair *pa = &a->items[i];
        const ngd_bow_pair *pb = &b->items[j];
        if (pa->id == pb->id) {
            double vi = pa->value, wi = pb->value;
            score += fabs(vi - wi) - fabs(vi) - fabs(wi);
            ++i; ++j;
        } else if (pa->id < pb->id) {
            ++i;          /* advance a to first id >= pb->id */
        } else {
            ++j;          /* advance b to first id >= pa->id */
        }
    }
    return -score / 2.0;
}
