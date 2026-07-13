/* test_mask.c — NGD dynamic-mask chain unit test.
 *
 * Validates the pure-C port of Tracking::PredictCurrentMask and helpers
 * (src/Tracking.cc:4249-4587):
 *   1. erode/dilate binary morphology
 *   2. pixel_potential (4-cross FAST-style corner response)
 *   3. extract_dyna_points (per-cell max-potential seed pick)
 *   4. cluster_dbscan (depth pre-seg + 3D DBSCAN)
 *   5. connected_components (4-conn)
 *   6. create_from_clusters (bbox + depth gate + largest CC + dilate)
 *   7. end-to-end predict_current with a SYNTHETIC LK callback (pure translation)
 *
 * No OpenCV: the LK step is a callback that injects a known (dx,dy) shift, so the
 * orchestrator wiring is exercised without the thin shell. Plain asserts. */
#include "ngd/mask.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int fails = 0;
#define CHECK(cond, msg) do { if(!(cond)){ printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); fails++; } } while(0)

/* ---------- helpers ---------- */

static uint8_t *zeros(int w, int h) { uint8_t *p = (uint8_t*)calloc((size_t)w*h,1); return p; }
static float  *fzeros(int w, int h) { float  *p = (float  *)calloc((size_t)w*h,sizeof(float)); return p; }

static int count_nonzero(const uint8_t *m, int w, int h)
{
    int c = 0;
    for (int i = 0; i < w*h; ++i) if (m[i]) c++;
    return c;
}

static void fill_rect(uint8_t *m, int w, int x0, int y0, int x1, int y1, uint8_t v)
{
    for (int y = y0; y <= y1; ++y) for (int x = x0; x <= x1; ++x) m[(size_t)y*w + x] = v;
}

/* 3px-period 2D checkerboard: every interior pixel's 4-cross (±3) lands on the
 * opposite colour → potential 200+255 = 455 (> threshold 250). */
static void checkerboard(uint8_t *g, int w, int h)
{
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x)
        g[(size_t)y*w + x] = (((x/3) + (y/3)) & 1) ? 255 : 0;
}

/* Synthetic LK callback: shifts every point by a fixed (dx,dy), status=1. */
static int shift_flow(const uint8_t *prev, const uint8_t *curr, int w, int h, int stride,
                      const float *in, int n, float *out, uint8_t *status, void *user)
{
    (void)prev; (void)curr; (void)w; (void)h; (void)stride;
    const float *s = (const float*)user;
    for (int j = 0; j < n; ++j) { out[2*j] = in[2*j] + s[0]; out[2*j+1] = in[2*j+1] + s[1]; status[j] = 1; }
    return 0;
}

/* ---------- 1. morphology ---------- */
static void test_morph(void)
{
    const int W = 24, H = 24;
    uint8_t *m = zeros(W,H), *e = zeros(W,H), *d = zeros(W,H);
    fill_rect(m, W, 6,6,17,17, 1);

    ngd_mask_erode(m, e, W, H, 2);   /* 5×5 → shrinks by 2 */
    CHECK(e[10*W+10] == 1, "erode: interior kept");
    CHECK(e[6*W+6]   == 0, "erode: original corner eaten");
    CHECK(e[8*W+8]   == 1, "erode: 2px-inside corner kept");

    ngd_mask_dilate(m, d, W, H, 2);  /* grows by 2 */
    CHECK(d[10*W+10] == 1, "dilate: interior kept");
    CHECK(d[4*W+4]   == 1, "dilate: grows 2px outward");
    CHECK(d[3*W+3]   == 0, "dilate: 3px-out still 0");

    /* in-place aliasing must not corrupt */
    uint8_t *a = zeros(W,H); fill_rect(a, W, 6,6,17,17,1);
    ngd_mask_erode(a, a, W, H, 2);
    CHECK(a[10*W+10] == 1 && a[6*W+6] == 0, "erode: in-place aliasing ok");

    free(m); free(e); free(d); free(a);
}

/* ---------- 2. pixel_potential ---------- */
static void test_potential(void)
{
    const int W = 32, H = 32;
    uint8_t *g = zeros(W,H);   /* flat black → -1 everywhere */
    /* feature at (10,10): centre 128, three cross neighbours bright(255). */
    g[10*W+10] = 128;
    g[10*W+ 7] = 255;   /* (-3,0) */
    g[10*W+13] = 255;   /* ( 3,0) */
    g[13*W+10] = 255;   /* (0, 3) */
    /* (0,-3) stays 0 */

    CHECK(ngd_mask_pixel_potential(g,W,H,W, 10,10) == 200+127, "potential: 3-brighter corner = 327");
    CHECK(ngd_mask_pixel_potential(g,W,H,W, 5, 5)  == -1,       "potential: flat region = -1");
    CHECK(ngd_mask_pixel_potential(g,W,H,W, 0, 0)  == -1,       "potential: near border OOB = -1");
    CHECK(ngd_mask_pixel_potential(g,W,H,W, 1, H-1)== -1,       "potential: bottom-edge OOB = -1");
    free(g);
}

/* ---------- 3. extract_dyna_points ---------- */
static void test_extract(void)
{
    const int W = 48, H = 48;
    uint8_t *g = zeros(W,H), *mask = zeros(W,H);
    checkerboard(g, W, H);                 /* every interior pixel → 455 */
    fill_rect(mask, W, 0,0,14,14, 1);      /* one 15×15 cell masked */

    ngd_pt2f pts[64]; int n = 0;
    ngd_mask_extract_dyna_points(g, W, H, W, mask, 15, 250, pts, &n);
    CHECK(n == 1, "extract: one seed in the single masked cell");
    CHECK(n == 1 && pts[0].x > 0 && pts[0].y > 0, "extract: seed inside cell, away from border");

    /* unmasked cells produce no seeds */
    memset(mask, 0, (size_t)W*H);
    ngd_mask_extract_dyna_points(g, W, H, W, mask, 15, 250, pts, &n);
    CHECK(n == 0, "extract: empty mask → 0 seeds");
    free(g); free(mask);
}

/* ---------- 4. cluster_dbscan ---------- */
static void test_dbscan(void)
{
    /* Two tight clusters (all z=1.0) + one noise point. All-z-equal ⇒ depth
     * pre-seg keeps nothing ⇒ fallback uses all original points. */
    ngd_pt3f pts[41];
    int k = 0;
    for (int i = 0; i < 20; ++i) { pts[k].x = (float)(100 + 5*(i%5)); pts[k].y = (float)(100 + 5*(i/5)); pts[k].z = 1.0f; k++; } /* A */
    for (int i = 0; i < 20; ++i) { pts[k].x = (float)(300 + 5*(i%5)); pts[k].y = (float)(300 + 5*(i/5)); pts[k].z = 1.0f; k++; } /* B */
    pts[k].x = 500; pts[k].y = 500; pts[k].z = 1.0f; k++; /* noise */

    int labels[41]; int ncl = 0;
    ngd_mask_cluster_dbscan(pts, k, 50.0f, 15, labels, &ncl);
    CHECK(ncl == 2, "dbscan: two clusters");

    int a_lab = labels[0], b_lab = labels[20];
    CHECK(a_lab >= 1 && b_lab >= 1 && a_lab != b_lab, "dbscan: A and B get distinct labels");
    int a_ok = 1, b_ok = 1;
    for (int i = 0; i < 20; ++i) if (labels[i] != a_lab) a_ok = 0;
    for (int i = 20; i < 40; ++i) if (labels[i] != b_lab) b_ok = 0;
    CHECK(a_ok, "dbscan: all A points share label");
    CHECK(b_ok, "dbscan: all B points share label");
    CHECK(labels[40] == -1, "dbscan: isolated point is noise");
}

/* ---------- 5. connected_components ---------- */
static void test_cc(void)
{
    const int W = 30, H = 30;
    uint8_t *m = zeros(W,H);
    fill_rect(m, W, 2,2,8,8, 1);
    fill_rect(m, W, 20,20,26,26, 1);
    int *lab = (int*)malloc((size_t)W*H*sizeof(int));
    int n = ngd_mask_connected_components(m, W, H, lab);
    CHECK(n == 3, "cc: bg + 2 components = 3 labels");
    CHECK(lab[5*W+5]   != 0 && lab[5*W+5]   != lab[23*W+23], "cc: two blobs get distinct nonzero labels");
    CHECK(lab[0] == 0, "cc: background = 0");
    /* count distinct nonzero labels */
    int seen[8] = {0,0,0,0,0,0,0,0}, distinct = 0;
    for (int i = 0; i < W*H; ++i) if (lab[i] > 0 && lab[i] < 8 && !seen[lab[i]]) { seen[lab[i]] = 1; distinct++; }
    CHECK(distinct == 2, "cc: exactly 2 nonzero labels");
    free(m); free(lab);
}

/* ---------- 6. create_from_clusters ---------- */
static void test_create(void)
{
    const int W = 120, H = 120;
    /* cluster: 5×5 grid spanning [30..90] (bbox 60×60 ≥ 50) */
    ngd_pt3f pts[25];
    int k = 0;
    for (int yy = 0; yy < 5; ++yy) for (int xx = 0; xx < 5; ++xx) {
        pts[k].x = (float)(30 + 15*xx); pts[k].y = (float)(30 + 15*yy); pts[k].z = 1.0f; k++;
    }
    int labels[25]; for (int i = 0; i < 25; ++i) labels[i] = 1;

    /* (a) uniform depth → filled expanded rect */
    float *depth = fzeros(W,H);
    for (int i = 0; i < W*H; ++i) depth[i] = 1.0f;
    uint8_t *out = zeros(W,H);
    ngd_mask_create_from_clusters(pts, labels, 25, 1, depth, W,H,W, out, W,H);
    CHECK(count_nonzero(out,W,H) > 500, "create: large cluster fills a region");
    CHECK(out[60*W+60] == 1, "create: centre of region = 1");
    CHECK(out[5*W+5]   == 0, "create: far corner outside rect = 0");

    /* (b) depth gate: a block of pixels off the median (1.0) gets zeroed */
    memset(out, 0, (size_t)W*H);
    for (int y = 45; y <= 75; ++y) for (int x = 45; x <= 75; ++x) depth[(size_t)y*W+x] = 5.0f;
    ngd_mask_create_from_clusters(pts, labels, 25, 1, depth, W,H,W, out, W,H);
    CHECK(out[60*W+60] == 0, "create: depth-gate zeroes off-median block centre (survives dilate)");
    /* outside the block, depth matches median → still filled */
    CHECK(out[35*W+35] == 1, "create: on-median region kept");

    /* (c) small cluster (bbox < 50) → skipped */
    ngd_pt3f small[2] = { {30,30,1}, {40,40,1} };  /* bbox 10×10 */
    int slab[2] = {1,1};
    memset(out, 0, (size_t)W*H);
    ngd_mask_create_from_clusters(small, slab, 2, 1, depth, W,H,W, out, W,H);
    CHECK(count_nonzero(out,W,H) == 0, "create: small bbox cluster skipped");

    free(depth); free(out);
}

/* ---------- 7. end-to-end predict_current ---------- */
static void test_predict(void)
{
    const int W = 120, H = 120;
    uint8_t *glast = zeros(W,H), *gcur = zeros(W,H);
    checkerboard(glast, W, H);
    checkerboard(gcur,  W, H);
    uint8_t *mask_last = zeros(W,H);
    fill_rect(mask_last, W, 10,10,80,80, 1);   /* 70×70 seed region */

    float *depth = fzeros(W,H);
    for (int i = 0; i < W*H; ++i) depth[i] = 1.0f;

    float shift[2] = { 20.0f, 10.0f };
    uint8_t *out = zeros(W,H);
    ngd_mask_predict_current(glast, gcur, mask_last, depth, W,H,W, shift_flow, shift, out);

    int nz = count_nonzero(out, W, H);
    CHECK(nz > 200, "predict: substantial predicted mask");

    /* centroid of predicted mask should shift ~right/down from the seed region
     * centroid (~45,45) by (20,10) → ~(65,55). */
    double sx = 0, sy = 0; int c = 0;
    for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x)
        if (out[(size_t)y*W+x]) { sx += x; sy += y; c++; }
    if (c > 0) { sx /= c; sy /= c; }
    printf("  predict: nz=%d centroid=(%.1f,%.1f) (expect ~(65,55))\n", nz, sx, sy);
    CHECK(sx > 50 && sx < 80, "predict: centroid x shifted right");
    CHECK(sy > 40 && sy < 70, "predict: centroid y shifted down");
    CHECK(out[65*W+55] == 1 || out[60*W+50] == 1, "predict: a deep-interior shifted pixel is 1");
    CHECK(out[5*W+5]   == 0, "predict: far corner stays 0");

    /* empty last mask → empty output */
    memset(out, 0, (size_t)W*H);
    memset(mask_last, 0, (size_t)W*H);
    ngd_mask_predict_current(glast, gcur, mask_last, depth, W,H,W, shift_flow, shift, out);
    CHECK(count_nonzero(out,W,H) == 0, "predict: empty last mask → empty output");

    free(glast); free(gcur); free(mask_last); free(depth); free(out);
}

int main(void)
{
    test_morph();
    test_potential();
    test_extract();
    test_dbscan();
    test_cc();
    test_create();
    test_predict();

    if (fails) { printf("test_mask: %d FAIL\n", fails); return 1; }
    printf("test_mask: PASS\n");
    return 0;
}
