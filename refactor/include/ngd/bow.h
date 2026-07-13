#ifndef NGD_BOW_H
#define NGD_BOW_H

/*
 * ngd/bow.h — Bag-of-Words vocabulary (pure C), a faithful subset of DBoW2
 * (Thirdparty/DBoW2/DBoW2/TemplatedVocabulary.h) sufficient for the
 * TrackReferenceKeyFrame path:
 *   - load ORBvoc.txt (text) and ORBvoc.bin (DBoW2 binary) vocabularies
 *   - transform(descriptors) -> BowVector + FeatureVector (levelsup = 4)
 *   - save a DBoW2-compatible binary for fast reload
 *
 * Deferred to a later stage (relocalization): Vocabulary::score (L1) and the
 * KeyFrameDatabase inverted index. TrackReferenceKeyFrame only needs
 * FeatureVector (same-NodeId matching); the BowVector is still produced and
 * L1-normalized to stay faithful to transform().
 *
 * Vocabulary layout: the ORB-SLAM3 ORBvoc is k=10, L=6 (~971k words). Nodes
 * are stored flat (structure-of-arrays) with children in CSR form. NodeId /
 * WordId are uint32. Weights are double internally (text load) / cast to float
 * on binary load/save, exactly as DBoW2 does.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NGD_BOW_DESC_LEN 32
#define NGD_BOW_NONLEAF  0xFFFFFFFFu

typedef struct {
    int k, L;                 /* m_k, m_L (branching factor, depth) */
    int scoring;              /* ScoringType: 0=L1_NORM (ORBvoc uses this) */
    int weighting;            /* WeightingType: 0=TF_IDF (ORBvoc uses this) */

    uint32_t n_nodes;         /* m_nodes.size() (root = index 0) */
    uint32_t n_words;         /* m_words.size() (#leaves) */

    /* node arrays (SoA), length n_nodes */
    uint32_t *parent;         /* parent NodeId (root's parent = 0) */
    uint32_t *child_off;      /* CSR offsets, length n_nodes+1; n_children[i] = child_off[i+1]-child_off[i] */
    uint32_t *child_idx;      /* children NodeIds, length child_off[n_nodes] */
    uint8_t  *desc;           /* n_nodes * 32 */
    double   *weight;         /* leaf = IDF weight, internal = 0 */
    uint32_t *word_id;        /* leaf = WordId, internal = NGD_BOW_NONLEAF */
} ngd_bow_vocab;

/* BowVector entry: WordId -> weight (L1-normalized). Kept sorted by id. */
typedef struct { uint32_t id; double value; } ngd_bow_pair;
typedef struct { ngd_bow_pair *items; int n, cap; } ngd_bowvec;

/* FeatureVector entry: NodeId (at level L-levelsup) -> feature indices. Sorted by id. */
typedef struct { uint32_t id; uint32_t *idx; int n, cap; } ngd_feat_entry;
typedef struct { ngd_feat_entry *items; int n, cap; } ngd_featvec;

/* ---- Vocabulary load / save ---- */

/* Parse ORBvoc.txt (TemplatedVocabulary.h:1344-1430). Header "k L scoring
 * weighting"; each node line "parent isLeaf d[0..31] weight(double)". Leaves
 * get WordIds in file order. Returns NULL on failure. */
ngd_bow_vocab *ngd_bow_vocab_load_text(const char *path);

/* Parse a DBoW2 .bin (TemplatedVocabulary.h:1458-1503). Header:
 * nb_nodes(u32) size_node(u32=41) k L scoring weighting; each 41-byte node =
 * parent(i32) desc[32] weight(float) is_leaf(bool). weight float -> double. */
ngd_bow_vocab *ngd_bow_vocab_load_binary(const char *path);

/* Write a DBoW2-compatible .bin (TemplatedVocabulary.h:1509-1529). weight
 * double -> float. Returns 0 on success, non-zero on failure. */
int ngd_bow_vocab_save_binary(const ngd_bow_vocab *v, const char *path);

void ngd_bow_vocab_free(ngd_bow_vocab *v);

/* ---- transform ---- */

/* Transform N descriptors (contiguous, N*32 bytes) into a BowVector and a
 * FeatureVector (TemplatedVocabulary.h:1132-1200). levelsup is the feature-
 * vector node level (ComputeBoW uses 4). For TF_IDF + L1 scoring the BowVector
 * is L1-normalized (sum|v|=1). bv / fv are zeroed on entry and must be freed
 * by the caller. descriptors may be NULL only when N==0. */
void ngd_bow_transform(const ngd_bow_vocab *v,
                       const uint8_t *descriptors, int N, int levelsup,
                       ngd_bowvec *bv, ngd_featvec *fv);

/* L1 scoring between two L1-normalized BowVectors (DBoW2 L1Scoring::score,
 * ScoringObject.cpp:23-68). Two-pointer merge over sorted vectors; for shared
 * words accumulate |vi-wi|-|vi|-|wi|, then return -sum/2. Result in [0,1]
 * (1 == identical, 0 == disjoint). Both vectors must already be L1-normalized
 * (transform() does this for L1 scoring vocabs). */
double ngd_bow_score_l1(const ngd_bowvec *a, const ngd_bowvec *b);

void ngd_bowvec_free(ngd_bowvec *bv);
void ngd_featvec_free(ngd_featvec *fv);

#ifdef __cplusplus
}
#endif
#endif /* NGD_BOW_H */
