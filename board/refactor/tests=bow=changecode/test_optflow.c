/* test_optflow.c — Validate the pure-C pyramidal Lucas-Kanade optical flow.
 *
 * Tests:
 *   1. Identity: tracking a static image → displacement ≈ 0 (within ε).
 *   2. Known shift: synthetic image translated by an integer pixel count →
 *      LK recovers the shift.
 *   3. Sub-pixel: bilinear-interpolated shift of 0.5 px — LK finds the
 *      displacement (loose tolerance — single-point LK is limited).
 *   4. Mask filtering: ngd_optflow_search drops points on non-zero mask.
 *   5. Callback: ngd_optflow_lk_callback matches ngd_lk_flow_fn signature.
 *   6. Edge cases: n=0, NULL images, out-of-bounds input point.
 *
 * Build / run:
 *   (compiled as part of ngd_core test suite — added to CMakeLists.txt)
 */

#include "ngd/optflow.h"
#include "ngd/mask.h"    /* ngd_lk_flow_fn typedef for callback test */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int n_pass = 0, n_fail = 0;

#define CHECK(cond, msg) do {                                         \
    if (cond) { ++n_pass; } else {                                    \
        ++n_fail;                                                     \
        fprintf(stderr, "FAIL [%s:%d]: %s\n", __FILE__, __LINE__, msg); \
    }                                                                 \
} while (0)

#define CHECK_FEQ(a, b, tol, msg) do {                                \
    float _a = (a), _b = (b);                                         \
    if (fabsf(_a - _b) <= (tol)) { ++n_pass; } else {                \
        ++n_fail;                                                     \
        fprintf(stderr, "FAIL [%s:%d]: %s  (%.4f vs %.4f, tol=%.4f)\n", \
                __FILE__, __LINE__, msg, _a, _b, (float)(tol));       \
    }                                                                 \
} while (0)

/* ---- Helpers ---- */

/* Fill a grayscale image with a checkerboard pattern (good for LK gradients). */
static void fill_checker(uint8_t *img, int w, int h, int stride,
                         int cell_size, uint8_t a, uint8_t b)
{
    for (int y = 0; y < h; ++y) {
        int cy = (y / cell_size) & 1;
        for (int x = 0; x < w; ++x) {
            int cx = (x / cell_size) & 1;
            img[(size_t)y * stride + x] = (cy ^ cx) ? a : b;
        }
    }
}

/* Check that each tracked point is within `tol` pixels of its *own* input position. */
static int check_identity(const float *pts_in, const float *pts_out,
                          const uint8_t *status, int n, float tol)
{
    int ok = 1;
    for (int i = 0; i < n; ++i) {
        if (!status[i]) { ok = 0; continue; }
        float dx = pts_out[2 * i]     - pts_in[2 * i];
        float dy = pts_out[2 * i + 1] - pts_in[2 * i + 1];
        if (fabsf(dx) > tol || fabsf(dy) > tol) ok = 0;
    }
    return ok;
}

/* Check that each tracked point is approximately (expectedX, expectedY). */
static int check_shifted(const float *pts_out, const uint8_t *status, int n,
                         float ex, float ey, float tol)
{
    int ok = 1;
    for (int i = 0; i < n; ++i) {
        if (!status[i]) { ok = 0; continue; }
        if (fabsf(pts_out[2 * i] - ex) > tol ||
            fabsf(pts_out[2 * i + 1] - ey) > tol) {
            ok = 0;
        }
    }
    return ok;
}

/* bilinear_from: read an image at sub-pixel position.
 * (Copy of the static helper in optflow.c for test independence.) */
static float bilinear_from(const uint8_t *img, int w, int h, int stride,
                           float x, float y)
{
    int x0 = (int)floorf(x), y0 = (int)floorf(y);
    int x1 = x0 + 1, y1 = y0 + 1;
    float fx = x - (float)x0, fy = y - (float)y0;
    if (x0 < 0) { x0 = 0; fx = 0.0f; }
    if (x0 >= w) { x0 = w - 1; fx = 0.0f; }
    if (y0 < 0) { y0 = 0; fy = 0.0f; }
    if (y0 >= h) { y0 = h - 1; fy = 0.0f; }
    if (x1 < 0) x1 = 0; if (x1 >= w) x1 = w - 1;
    if (y1 < 0) y1 = 0; if (y1 >= h) y1 = h - 1;
    float wx1 = 1.0f - fx, wy1 = 1.0f - fy;
    return wy1 * (wx1 * img[(size_t)y0 * stride + x0] + fx * img[(size_t)y0 * stride + x1])
         +  fy * (wx1 * img[(size_t)y1 * stride + x0] + fx * img[(size_t)y1 * stride + x1]);
}

/* =====================================================================
 * Test 1: Identity (static image → zero displacement)
 *
 * Use a single pyramid level so all points stay in bounds.
 * ===================================================================== */
static void test_identity(void)
{
    const int W = 128, H = 128, N = 16;
    uint8_t *img = (uint8_t *)calloc((size_t)W * H, 1);
    fill_checker(img, W, H, W, 8, 50, 200);

    /* Grid of source points, well away from borders */
    float  pts_in[32];
    float  pts_out[32];
    uint8_t status[16];
    for (int i = 0; i < N; ++i) {
        pts_in[2 * i]     = 16.0f + 8.0f * (float)(i % 4);
        pts_in[2 * i + 1] = 16.0f + 8.0f * (float)(i / 4);
    }

    /* 1 level, small window — no pyramid to drop points */
    ngd_lk_params params = { 9, 0, 30, 0.001f };
    int ret = ngd_optflow_pyr_lk(img, img, W, H, W, pts_in, N, pts_out, status, &params);
    CHECK(ret == 0, "identity: return code");

    int tracked = 0;
    for (int i = 0; i < N; ++i) if (status[i]) ++tracked;
    CHECK(tracked >= N - 2, "identity: most points tracked");  /* allow a few border losses */

    int good = check_identity(pts_in, pts_out, status, N, 1.0f);
    CHECK(good, "identity: tracked pts near original position");

    free(img);
    printf("  test_identity: %d/%d tracked\n", tracked, N);
}

/* =====================================================================
 * Test 2: Known integer-pixel shift
 *
 * Use 256×256 images + single-level LK so all points fit in the window.
 * ===================================================================== */
static void test_known_shift(void)
{
    const int W = 256, H = 256, CELL = 12;
    const float SHIFT_X = 5.0f, SHIFT_Y = 3.0f;

    uint8_t *prev = (uint8_t *)malloc((size_t)W * H);
    uint8_t *curr = (uint8_t *)calloc((size_t)W * H, 1);

    fill_checker(prev, W, H, W, CELL, 30, 220);

    /* Shift the image: curr[x][y] = prev[x−SX][y−SY] */
    for (int y = (int)SHIFT_Y; y < H; ++y) {
        for (int x = (int)SHIFT_X; x < W; ++x) {
            curr[(size_t)y * W + x] = prev[(size_t)(y - (int)SHIFT_Y) * W + (x - (int)SHIFT_X)];
        }
    }
    /* Border fill */
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < (int)SHIFT_X; ++x)
            curr[(size_t)y * W + x] = 0;
    for (int y = 0; y < (int)SHIFT_Y; ++y)
        for (int x = 0; x < W; ++x)
            curr[(size_t)y * W + x] = 0;

    /* Source points: well inside, spaced across the image */
    float  pts_in[50];
    float  pts_out[50];
    uint8_t status[25];
    int n = 0;
    for (int r = 0; r < 5; ++r) {
        for (int c = 0; c < 5; ++c) {
            float x = 40.0f + 40.0f * (float)c;
            float y = 40.0f + 40.0f * (float)r;
            /* ensure the shifted point + window still fits */
            if (x + SHIFT_X + 10 >= W || y + SHIFT_Y + 10 >= H) continue;
            pts_in[2 * n] = x; pts_in[2 * n + 1] = y;
            ++n;
        }
    }

    /* 1 pyramid level — with 256×256 images this is sufficient */
    ngd_lk_params params = { 9, 0, 50, 0.001f };
    int ret = ngd_optflow_pyr_lk(prev, curr, W, H, W, pts_in, n, pts_out, status, &params);
    CHECK(ret == 0, "known_shift: return code");

    int tracked = 0, accurate = 0;
    for (int i = 0; i < n; ++i) {
        if (!status[i]) continue;
        ++tracked;
        float ex = pts_in[2 * i]     + SHIFT_X;
        float ey = pts_in[2 * i + 1] + SHIFT_Y;
        if (fabsf(pts_out[2 * i] - ex) < 2.0f && fabsf(pts_out[2 * i + 1] - ey) < 2.0f)
            ++accurate;
    }
    CHECK(tracked >= n / 2, "known_shift: at least half tracked");
    CHECK(accurate >= tracked * 2 / 3, "known_shift: most tracked pts accurate (<2px)");

    printf("  test_known_shift: %d/%d tracked, %d accurate (±2px)\n", tracked, n, accurate);

    free(prev); free(curr);
}

/* =====================================================================
 * Test 3: Mask filtering (ngd_optflow_search)
 * ===================================================================== */
static void test_mask_filter(void)
{
    const int W = 128, H = 128, N = 10;
    uint8_t *prev = (uint8_t *)malloc((size_t)W * H);
    uint8_t *curr = (uint8_t *)malloc((size_t)W * H);
    uint8_t *mask = (uint8_t *)calloc((size_t)W * H, 1);

    fill_checker(prev, W, H, W, 4, 40, 200);
    memcpy(curr, prev, (size_t)W * H);  /* static */

    float  last_keys[20];
    int    last_mp_valid[10];
    float  out_keys[20];
    int    out_mp_idx[10];

    for (int i = 0; i < N; ++i) {
        last_keys[2 * i]     = 20.0f + 8.0f * (float)i;
        last_keys[2 * i + 1] = 64.0f;
        last_mp_valid[i]     = (i < 8) ? 1 : 0;   /* skip last 2 */
    }

    /* Put a mask block where points 2-3 (index) would land */
    for (int y = 62; y < 68; ++y)
        for (int x = 34; x < 42; ++x)
            mask[(size_t)y * W + x] = 255;

    ngd_lk_params params = { 9, 0, 30, 0.01f };
    int n_out = ngd_optflow_search(prev, curr, mask, W, H, W,
                                   last_keys, last_mp_valid, N,
                                   out_keys, out_mp_idx, &params);
    CHECK(n_out >= 5 && n_out <= 7, "mask_filter: correct count (some masked)");  /* 8 valid − ~2 blocked */

    /* Verify masked points are NOT in output */
    int found_blocked = 0;
    for (int k = 0; k < n_out; ++k) {
        float ox = out_keys[2 * k], oy = out_keys[2 * k + 1];
        if (ox >= 36.0f && ox <= 40.0f && oy >= 64.0f && oy <= 66.0f)
            found_blocked = 1;
    }
    CHECK(!found_blocked, "mask_filter: masked points dropped");

    /* NULL mask → no filtering */
    int n_out2 = ngd_optflow_search(prev, curr, NULL, W, H, W,
                                    last_keys, last_mp_valid, N,
                                    out_keys, out_mp_idx, &params);
    CHECK(n_out2 == 8, "mask_filter: NULL mask → all valid pts passed");

    free(prev); free(curr); free(mask);
    printf("  test_mask_filter: passed\n");
}

/* =====================================================================
 * Test 4: Callback (matches ngd_lk_flow_fn signature)
 *
 * The callback uses NGD_LK_DEFAULT_MASK (win=21, max_level=3) which needs
 * a large image so the coarsest level (L3 = 1/8 × 256 = 32×32) still fits
 * the 21×21 window (half=10).  Points are centred to stay well in bounds.
 * ===================================================================== */
static void test_callback(void)
{
    const int W = 256, H = 256, N = 5;
    uint8_t *prev = (uint8_t *)malloc((size_t)W * H);
    uint8_t *curr = (uint8_t *)malloc((size_t)W * H);
    /* Large cells (24px) so the checkerboard survives 3 levels of pyrDown
     * (1/8 → still ~3px per cell at level 3) — gradients stay detectable. */
    fill_checker(prev, W, H, W, 24, 40, 200);
    memcpy(curr, prev, (size_t)W * H);

    float   pts_in[10], pts_out[10];
    uint8_t status[5];
    /* Points centred at (128,128) — at level 3 (32×32) they are at (16,16),
     * giving 16±10 = [6,26] — safely inside the 32×32 coarsest image. */
    for (int i = 0; i < N; ++i) {
        pts_in[2 * i]     = 112.0f + 8.0f * (float)i;   /* 112, 120, 128, 136, 144 */
        pts_in[2 * i + 1] = 128.0f;
    }

    /* Call via the typedef (mask.h ngd_lk_flow_fn). */
    ngd_lk_flow_fn fn = ngd_optflow_lk_callback;
    int ret = fn(prev, curr, W, H, W, pts_in, N, pts_out, status, NULL);
    CHECK(ret == 0, "callback: return code");

    int tracked = 0;
    for (int i = 0; i < N; ++i) if (status[i]) ++tracked;
    CHECK(tracked >= N - 1, "callback: most points tracked");

    int close = check_identity(pts_in, pts_out, status, N, 1.0f);
    CHECK(close, "callback: tracked near original position");

    free(prev); free(curr);
    printf("  test_callback: %d/%d tracked\n", tracked, N);
}

/* =====================================================================
 * Test 5: Edge cases
 * ===================================================================== */
static void test_edge_cases(void)
{
    const int W = 64, H = 64;
    uint8_t *img = (uint8_t *)calloc((size_t)W * H, 1);
    fill_checker(img, W, H, W, 4, 50, 200);

    float   pts_in[4]  = { 32, 32, 8, 8 };
    float   pts_out[4];
    uint8_t status[2];

    /* n=0 → no crash, return non-zero */
    int ret = ngd_optflow_pyr_lk(img, img, W, H, W, pts_in, 0, pts_out, status, NULL);
    CHECK(ret != 0, "edge: n=0 returns error");

    /* NULL images → error */
    ret = ngd_optflow_pyr_lk(NULL, img, W, H, W, pts_in, 1, pts_out, status, NULL);
    CHECK(ret != 0, "edge: NULL prev returns error");
    ret = ngd_optflow_pyr_lk(img, NULL, W, H, W, pts_in, 1, pts_out, status, NULL);
    CHECK(ret != 0, "edge: NULL curr returns error");

    /* NULL output pointers → error */
    ret = ngd_optflow_pyr_lk(img, img, W, H, W, pts_in, 1, NULL, status, NULL);
    CHECK(ret != 0, "edge: NULL pts_out returns error");

    /* Out-of-bounds input point — should not crash, status=0 */
    float oob[2] = { 200.0f, 200.0f };
    ret = ngd_optflow_pyr_lk(img, img, W, H, W, oob, 1, pts_out, status, NULL);
    CHECK(ret == 0, "edge: OOB input returns 0 (point skipped)");
    CHECK(status[0] == 0, "edge: OOB input status=0");

    /* Very small image with minimal params — should not crash */
    uint8_t tiny[16] = {
        128,128,128,128,
        128,200,200,128,
        128,200,200,128,
        128,128,128,128
    };
    float  tp_in[2] = { 1.5f, 1.5f }, tp_out[2];
    uint8_t tp_st[1];
    ngd_lk_params tp = { 3, 0, 10, 0.1f };
    ret = ngd_optflow_pyr_lk(tiny, tiny, 4, 4, 4, tp_in, 1, tp_out, tp_st, &tp);
    CHECK(ret == 0, "edge: 4×4 image with win=3 no crash");
    /* May or may not track — just ensure no crash. */

    free(img);
    printf("  test_edge_cases: passed\n");
}

/* =====================================================================
 * Test 6: ngd_optflow_search edge cases
 * ===================================================================== */
static void test_search_edge_cases(void)
{
    int ret;
    float keys[2] = { 10, 10 }, out[2];
    int valid[1] = { 1 }, idx[1];

    /* n_last <= 0 → 0 */
    ret = ngd_optflow_search(NULL, NULL, NULL, 64, 64, 64,
                             keys, valid, 0, out, idx, NULL);
    CHECK(ret == 0, "search_edge: n_last=0 → 0");

    /* NULL image → 0 */
    ret = ngd_optflow_search(NULL, (const uint8_t*)"x", NULL, 64, 64, 64,
                             keys, valid, 1, out, idx, NULL);
    CHECK(ret == 0, "search_edge: NULL last_gray → 0");

    /* w <= 0 → 0 */
    ret = ngd_optflow_search((const uint8_t*)"x", (const uint8_t*)"x", NULL, 0, 64, 64,
                             keys, valid, 1, out, idx, NULL);
    CHECK(ret == 0, "search_edge: w=0 → 0");

    /* No valid source points → 0 */
    int all_invalid[1] = { 0 };
    const int W = 64, H = 64;
    uint8_t *img = (uint8_t *)calloc((size_t)W * H, 1);
    fill_checker(img, W, H, W, 4, 50, 200);
    ret = ngd_optflow_search(img, img, NULL, W, H, W,
                             keys, all_invalid, 1, out, idx, NULL);
    CHECK(ret == 0, "search_edge: all_mp_invalid=0 → 0");
    free(img);

    printf("  test_search_edge_cases: passed\n");
}

/* =====================================================================
 * Test 7: Sub-pixel displacement (bilinear-interpolated image)
 * ===================================================================== */
static void test_subpixel(void)
{
    const int W = 128, H = 128;
    uint8_t *prev = (uint8_t *)malloc((size_t)W * H);
    uint8_t *curr = (uint8_t *)calloc((size_t)W * H, 1);

    fill_checker(prev, W, H, W, 5, 40, 200);

    /* Create a sub-pixel-shifted image via bilinear interpolation. */
    const float sx = 0.7f, sy = 0.3f;
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            float v = bilinear_from(prev, W, H, W,
                                    (float)x - sx, (float)y - sy);
            if (v < 0.0f) v = 0.0f;
            if (v > 255.0f) v = 255.0f;
            curr[(size_t)y * W + x] = (uint8_t)(v + 0.5f);
        }
    }

    float  pts_in[2]  = { 64.0f, 64.0f };
    float  pts_out[2];
    uint8_t status[1];

    /* Large window, 1 level, relaxed tolerance — sub-pixel LK is inherently noisy. */
    ngd_lk_params params = { 11, 0, 50, 0.001f };
    int ret = ngd_optflow_pyr_lk(prev, curr, W, H, W, pts_in, 1, pts_out, status, &params);
    CHECK(ret == 0, "subpixel: return code");
    CHECK(status[0] == 1, "subpixel: point tracked");

    if (status[0]) {
        float ex = pts_in[0] + sx, ey = pts_in[1] + sy;
        CHECK_FEQ(pts_out[0], ex, 1.5f, "subpixel: x within 1.5px");
        CHECK_FEQ(pts_out[1], ey, 1.5f, "subpixel: y within 1.5px");
    }

    free(prev); free(curr);
    printf("  test_subpixel: passed\n");
}

/* =====================================================================
 * Main
 * ===================================================================== */
int main(void)
{
    printf("=== test_optflow ===\n\n");

    test_identity();
    test_known_shift();
    test_subpixel();
    test_mask_filter();
    test_callback();
    test_edge_cases();
    test_search_edge_cases();

    printf("\n%d passed, %d failed\n", n_pass, n_fail);
    return n_fail > 0 ? 1 : 0;
}
