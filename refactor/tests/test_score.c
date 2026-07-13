/* test_score.c — ngd_bow_score_l1 (DBoW2 L1Scoring::score) unit test.
 * No vocabulary needed: BowVectors are built by hand and L1-normalized. */
#include "ngd/bow.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

static int fails = 0;
#define CHECK(cond, msg) do { if(!(cond)){ printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); fails++; } } while(0)

/* Build a BowVector from (id,value) pairs, sorted+L1-normalized. */
static ngd_bowvec *make_bowvec(const uint32_t *ids, const double *vals, int n)
{
    ngd_bowvec *bv = (ngd_bowvec*)calloc(1, sizeof(ngd_bowvec));
    bv->n = n; bv->cap = n;
    bv->items = (ngd_bow_pair*)malloc((size_t)n * sizeof(ngd_bow_pair));
    double norm = 0.0;
    for (int i = 0; i < n; ++i) { bv->items[i].id = ids[i]; bv->items[i].value = vals[i]; norm += fabs(vals[i]); }
    if (norm > 0) for (int i = 0; i < n; ++i) bv->items[i].value /= norm;
    /* sort by id ascending */
    for (int i = 1; i < n; ++i) for (int j = i; j > 0 && bv->items[j-1].id > bv->items[j].id; --j) {
        ngd_bow_pair t = bv->items[j-1]; bv->items[j-1] = bv->items[j]; bv->items[j] = t;
    }
    return bv;
}

int main(void)
{
    {   /* identical vectors -> score 1 */
        uint32_t a_id[] = {1,2}; double a_v[] = {0.5,0.5};
        ngd_bowvec *a = make_bowvec(a_id, a_v, 2);
        ngd_bowvec *b = make_bowvec(a_id, a_v, 2);
        double s = ngd_bow_score_l1(a, b);
        printf("  identical score = %.6f (expect 1.0)\n", s);
        CHECK(fabs(s - 1.0) < 1e-9, "identical -> 1.0");
        ngd_bowvec_free(a); free(a); ngd_bowvec_free(b); free(b);
    }
    {   /* disjoint vectors -> score 0 */
        uint32_t a_id[] = {1,2}; double a_v[] = {0.5,0.5};
        uint32_t b_id[] = {3,4}; double b_v[] = {0.5,0.5};
        ngd_bowvec *a = make_bowvec(a_id, a_v, 2);
        ngd_bowvec *b = make_bowvec(b_id, b_v, 2);
        double s = ngd_bow_score_l1(a, b);
        printf("  disjoint score = %.6f (expect 0.0)\n", s);
        CHECK(fabs(s - 0.0) < 1e-9, "disjoint -> 0.0");
        ngd_bowvec_free(a); free(a); ngd_bowvec_free(b); free(b);
    }
    {   /* partial overlap: a={1:0.5,2:0.5} b={1:0.3,2:0.7} -> 0.8 */
        uint32_t a_id[] = {1,2}; double a_v[] = {0.5,0.5};
        uint32_t b_id[] = {1,2}; double b_v[] = {0.3,0.7};
        ngd_bowvec *a = make_bowvec(a_id, a_v, 2);
        ngd_bowvec *b = make_bowvec(b_id, b_v, 2);
        double s = ngd_bow_score_l1(a, b);
        printf("  partial score  = %.6f (expect 0.8)\n", s);
        CHECK(fabs(s - 0.8) < 1e-9, "partial overlap -> 0.8");
        ngd_bowvec_free(a); free(a); ngd_bowvec_free(b); free(b);
    }
    {   /* one-sided: a has words b doesn't and vice versa, plus one shared */
        /* a={1:0.5, 2:0.5}  b={2:0.5, 3:0.5}  shared word 2 (0.5 vs 0.5)
         *   common term = |0.5-0.5| - 0.5 - 0.5 = -1.0 ; sum=-1 ; score=0.5
         *   hand: ||a-b||_1 = |0.5-0|(word1,a-only) + |0.5-0.5|(word2) + |0-0.5|(word3) = 1.0
         *   scaled = 1 - 0.5*1.0 = 0.5  ✓ */
        uint32_t a_id[] = {1,2}; double a_v[] = {0.5,0.5};
        uint32_t b_id[] = {2,3}; double b_v[] = {0.5,0.5};
        ngd_bowvec *a = make_bowvec(a_id, a_v, 2);
        ngd_bowvec *b = make_bowvec(b_id, b_v, 2);
        double s = ngd_bow_score_l1(a, b);
        printf("  one-shared score = %.6f (expect 0.5)\n", s);
        CHECK(fabs(s - 0.5) < 1e-9, "one shared -> 0.5");
        ngd_bowvec_free(a); free(a); ngd_bowvec_free(b); free(b);
    }
    {   /* symmetry */
        uint32_t a_id[] = {1,2,3}; double a_v[] = {0.2,0.3,0.5};
        uint32_t b_id[] = {2,3,4}; double b_v[] = {0.4,0.1,0.5};
        ngd_bowvec *a = make_bowvec(a_id, a_v, 3);
        ngd_bowvec *b = make_bowvec(b_id, b_v, 3);
        double s1 = ngd_bow_score_l1(a, b);
        double s2 = ngd_bow_score_l1(b, a);
        printf("  symmetric: %.6f vs %.6f\n", s1, s2);
        CHECK(fabs(s1 - s2) < 1e-12, "score symmetric");
        CHECK(s1 >= 0.0 && s1 <= 1.0, "score in [0,1]");
        ngd_bowvec_free(a); free(a); ngd_bowvec_free(b); free(b);
    }

    if (fails == 0) { printf("test_score: PASS\n"); return 0; }
    printf("test_score: %d FAILURES\n", fails);
    return 1;
}
