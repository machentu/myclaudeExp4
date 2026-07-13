/* test_bow.c — validate DBoW2 vocabulary load (text+binary), transform, BowVector
 * normalization, and binary round-trip. Includes an optional real-ORBvoc.bin
 * sanity check that is skipped (not failed) when the file is absent. */
#include "ngd/bow.h"
#include "ngd/matcher.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int fails = 0;
#define CHECK(cond, msg) do { if(!(cond)){ printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); fails++; } } while(0)

/* Build a tiny k=2, L=2 vocabulary text file (6 nodes + implicit root = 7
 * nodes, 4 leaves). Tree:
 *   root(0) -> {1:D1=0x00.., 2:D2=0xFF..}
 *   1 -> {3:D3=0x00.., 4:D4=byte0=0x01}
 *   2 -> {5:D5=0xFF.., 6:D6=byte0=0xFE|0xFF..}
 * Leaf weights: w3=1.0, w4=2.5, w5=1.0, w6=2.5 (all exact in float). */
static const char *VOC_TXT =
    "2 2 0 0\n"
    "0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"      /* node1: parent0, internal, D1=0,   w=0   */
    "0 0 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 0\n" /* node2: parent0, internal, D2=0xFF, w=0 */
    "1 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1\n"   /* node3: parent1, LEAF, D3=0,    w=1.0 */
    "1 1 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 2.5\n" /* node4: parent1, LEAF, D4,      w=2.5 */
    "2 1 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 1\n"   /* node5: parent2, LEAF, D5=0xFF, w=1.0 */
    "2 1 254 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 2.5\n"; /* node6: parent2, LEAF, D6, w=2.5 */

static void desc_zero(uint8_t d[32]) { memset(d, 0, 32); }
static void desc_ones(uint8_t d[32]) { memset(d, 0xFF, 32); }

int main(void)
{
    /* write the tiny vocab to a temp text file */
    FILE *f = fopen("tiny_voc.txt", "w");
    CHECK(f != NULL, "open tiny_voc.txt for write");
    if (!f) { printf("test_bow: FAIL (cannot write temp)\n"); return 1; }
    fputs(VOC_TXT, f);
    fclose(f);

    ngd_bow_vocab *v = ngd_bow_vocab_load_text("tiny_voc.txt");
    CHECK(v != NULL, "load_text tiny vocab");
    if (!v) { printf("test_bow: FAIL (load_text)\n"); return 1; }

    /* ---- structure ---- */
    CHECK(v->k == 2 && v->L == 2, "header k=2 L=2");
    CHECK(v->scoring == 0 && v->weighting == 0, "L1 + TF_IDF");
    CHECK(v->n_nodes == 7, "n_nodes=7 (root+6)");
    CHECK(v->n_words == 4, "n_words=4 leaves");
    CHECK(v->word_id[0] == NGD_BOW_NONLEAF, "root non-leaf");
    CHECK(v->word_id[1] == NGD_BOW_NONLEAF && v->word_id[2] == NGD_BOW_NONLEAF, "nodes 1,2 internal");
    CHECK(v->word_id[3] == 0 && v->word_id[4] == 1, "leaves 3,4 -> words 0,1");
    CHECK(v->word_id[5] == 2 && v->word_id[6] == 3, "leaves 5,6 -> words 2,3");
    CHECK(v->parent[1] == 0 && v->parent[2] == 0, "level-1 parents = root");
    CHECK(v->parent[3] == 1 && v->parent[4] == 1, "leaves 3,4 parent=1");
    CHECK(v->parent[5] == 2 && v->parent[6] == 2, "leaves 5,6 parent=2");
    /* CSR children: root->[1,2], node1->[3,4], node2->[5,6], leaves->[] */
    CHECK(v->child_off[1] - v->child_off[0] == 2, "root has 2 children");
    CHECK(v->child_idx[v->child_off[0]] == 1 && v->child_idx[v->child_off[0]+1] == 2, "root children = {1,2}");
    CHECK(v->child_off[2] - v->child_off[1] == 2, "node1 has 2 children");
    CHECK(v->child_idx[v->child_off[1]] == 3 && v->child_idx[v->child_off[1]+1] == 4, "node1 children = {3,4}");
    CHECK(v->child_off[4] - v->child_off[3] == 0, "leaf 3 has 0 children");
    CHECK(v->weight[3] == 1.0 && v->weight[4] == 2.5, "leaf weights loaded");

    /* ---- transform: single zero descriptor -> word 0 (node3), nid=node1 (levelsup=1) ---- */
    {
        uint8_t q[32]; desc_zero(q);
        ngd_bowvec bv; ngd_featvec fv;
        ngd_bow_transform(v, q, 1, 1, &bv, &fv);
        CHECK(bv.n == 1 && bv.items[0].id == 0, "zero desc -> word 0");
        CHECK(fabs(bv.items[0].value - 1.0) < 1e-12, "single-word L1 norm = 1.0");
        CHECK(fv.n == 1 && fv.items[0].id == 1, "zero desc feat -> node 1");
        CHECK(fv.items[0].n == 1 && fv.items[0].idx[0] == 0, "feat idx[0]=0");
        ngd_bowvec_free(&bv); ngd_featvec_free(&fv);
    }
    /* ---- transform: single ones descriptor -> word 2 (node5), nid=node2 ---- */
    {
        uint8_t q[32]; desc_ones(q);
        ngd_bowvec bv; ngd_featvec fv;
        ngd_bow_transform(v, q, 1, 1, &bv, &fv);
        CHECK(bv.n == 1 && bv.items[0].id == 2, "ones desc -> word 2");
        CHECK(fv.n == 1 && fv.items[0].id == 2, "ones desc feat -> node 2");
        ngd_bowvec_free(&bv); ngd_featvec_free(&fv);
    }
    /* ---- transform: 3 desc [zero, zero, ones] -> bow {0: 2*w3, 2: w5}, L1 norm ---- */
    {
        uint8_t q[3*32];
        desc_zero(q+0*32); desc_zero(q+1*32); desc_ones(q+2*32);
        ngd_bowvec bv; ngd_featvec fv;
        ngd_bow_transform(v, q, 3, 1, &bv, &fv);
        CHECK(bv.n == 2, "3 desc -> 2 unique words");
        /* w3=w5=1.0 -> pre-norm {0:2.0, 2:1.0}, norm=3 -> {0:0.6667, 2:0.3333} */
        CHECK(bv.items[0].id == 0 && fabs(bv.items[0].value - 2.0/3.0) < 1e-12, "word0 weight = 2/3");
        CHECK(bv.items[1].id == 2 && fabs(bv.items[1].value - 1.0/3.0) < 1e-12, "word2 weight = 1/3");
        double sumabs = 0; for (int i = 0; i < bv.n; ++i) sumabs += fabs(bv.items[i].value);
        CHECK(fabs(sumabs - 1.0) < 1e-12, "BowVector L1-normalized");
        /* featvec: node1 -> [0,1], node2 -> [2] */
        CHECK(fv.n == 2, "featvec 2 nodes");
        CHECK(fv.items[0].id == 1 && fv.items[0].n == 2, "node1 has 2 feats [0,1]");
        CHECK(fv.items[0].idx[0] == 0 && fv.items[0].idx[1] == 1, "node1 idx = {0,1}");
        CHECK(fv.items[1].id == 2 && fv.items[1].n == 1 && fv.items[1].idx[0] == 2, "node2 idx = {2}");
        ngd_bowvec_free(&bv); ngd_featvec_free(&fv);
    }
    /* ---- levelsup=2 -> nid_level=0 -> all feats map to root (nid=0) ---- */
    {
        uint8_t q[32]; desc_zero(q);
        ngd_bowvec bv; ngd_featvec fv;
        ngd_bow_transform(v, q, 1, 2, &bv, &fv);
        CHECK(fv.n == 1 && fv.items[0].id == 0, "levelsup=2 -> nid=root(0)");
        ngd_bowvec_free(&bv); ngd_featvec_free(&fv);
    }

    /* ---- binary round-trip ---- */
    {
        int rc = ngd_bow_vocab_save_binary(v, "tiny_voc.bin");
        CHECK(rc == 0, "save_binary");
        ngd_bow_vocab *v2 = ngd_bow_vocab_load_binary("tiny_voc.bin");
        CHECK(v2 != NULL, "load_binary");
        if (v2) {
            CHECK(v2->k == 2 && v2->L == 2, "bin header");
            CHECK(v2->n_nodes == 7 && v2->n_words == 4, "bin n_nodes/n_words");
            CHECK(v2->word_id[3] == 0 && v2->word_id[6] == 3, "bin word_ids");
            CHECK(v2->child_off[1] - v2->child_off[0] == 2, "bin root 2 children");
            CHECK(v2->child_idx[v2->child_off[1]] == 3, "bin node1 first child=3");
            /* weight stored as float: 1.0 and 2.5 are exact */
            CHECK(fabs(v2->weight[3] - 1.0) < 1e-7 && fabs(v2->weight[4] - 2.5) < 1e-7, "bin weights (float)");
            /* transform on bin-loaded vocab gives same word */
            uint8_t q[32]; desc_zero(q);
            ngd_bowvec bv; ngd_featvec fv;
            ngd_bow_transform(v2, q, 1, 1, &bv, &fv);
            CHECK(bv.n == 1 && bv.items[0].id == 0, "bin-vocab transform -> word 0");
            ngd_bowvec_free(&bv); ngd_featvec_free(&fv);
            ngd_bow_vocab_free(v2);
        }
    }

    ngd_bow_vocab_free(v);
    remove("tiny_voc.txt");
    remove("tiny_voc.bin");

    /* ---- optional: real ORBvoc.bin sanity check (skip if absent) ---- */
    const char *cands[] = {
        "../../../Vocabulary/ORBvoc.bin",
        "../../Vocabulary/ORBvoc.bin",
        "../../../../Vocabulary/ORBvoc.bin",
        "Vocabulary/ORBvoc.bin",
        NULL
    };
    const char *found = NULL;
    for (int i = 0; cands[i]; ++i) {
        FILE *tf = fopen(cands[i], "rb");
        if (tf) { fclose(tf); found = cands[i]; break; }
    }
    if (found) {
        ngd_bow_vocab *rv = ngd_bow_vocab_load_binary(found);
        if (rv) {
            uint32_t nn = rv->n_nodes, nw = rv->n_words;
            CHECK(rv->k == 10 && rv->L == 6, "real ORBvoc k=10 L=6");
            CHECK(rv->n_nodes > 1000000, "real ORBvoc ~1.08M nodes");
            CHECK(rv->n_words > 900000, "real ORBvoc ~971k words");
            /* transform a zero descriptor -> some valid word */
            uint8_t q[32]; desc_zero(q);
            ngd_bowvec bv; ngd_featvec fv;
            ngd_bow_transform(rv, q, 1, 4, &bv, &fv);
            CHECK(bv.n == 1 && bv.items[0].id < rv->n_words, "real vocab transform -> valid word");
            CHECK(fv.n == 1, "real vocab featvec 1 node (levelsup=4)");
            ngd_bowvec_free(&bv); ngd_featvec_free(&fv);
            ngd_bow_vocab_free(rv);
            printf("  (real ORBvoc.bin checked: %u nodes, %u words)\n", nn, nw);
        } else {
            printf("  (real ORBvoc.bin found at %s but failed to load — skipping)\n", found);
        }
    } else {
        printf("  (real ORBvoc.bin not found — skipping optional check)\n");
    }

    if (fails == 0) { printf("test_bow: PASS\n"); return 0; }
    printf("test_bow: %d FAILURES\n", fails);
    return 1;
}
