/* test_bitexact_ab.c — byte-for-byte A/B between the optimized ORB pipeline of
 * this project (dsp_orb_extract, src/dsp_orb.c) and the frozen refappv8segdsp
 * implementation (dsp_orb_extract_legacy, test/ref_legacy/dsp_orb_legacy.c).
 *
 * Both extractors run on the same synthetic images with the same parameters;
 * the keypoint array and the descriptor bytes are then compared with memcmp.
 * The comparison is over the FULL ngd_keypoint struct (x/y/angle/response/
 * octave/size), so any arithmetic change in resize, FAST-9, blur, IC_Angle or
 * rBRIEF — or any reordering in the cell/quadtree bookkeeping — fails here.
 *
 * Run: test_bitexact_ab.exe   (exit 0 = all cases bit-exact)
 */
#include "ngd/orb.h"
#include "ngd_app/dsp_orb.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* frozen refappv8segdsp implementation (test/ref_legacy/dsp_orb_legacy.c) */
int dsp_orb_extract_legacy(const ngd_orb_extractor *ex,
                           const uint8_t *gray, int w, int h, int stride,
                           ngd_keypoint *out_kps, int max_kps,
                           uint8_t *out_desc);

#define MAXK 8192

/* deterministic LCG so runs are reproducible */
static uint32_t lcg_state;
static void lcg_seed(uint32_t s) { lcg_state = s; }
static uint32_t lcg_next(void)
{
    lcg_state = lcg_state * 1664525u + 1013904223u;
    return lcg_state >> 8;   /* 24-bit value */
}

static void make_checkerboard(uint8_t *gray, int w, int h, int cs)
{
    for (int y = 0; y < h; ++y) {
        int by = y / cs;
        for (int x = 0; x < w; ++x) {
            int bx = x / cs;
            int v = ((bx ^ by) & 1) ? 200 : 50;
            v += (x * 20 / w);
            if (v > 255) v = 255;
            gray[y * w + x] = (uint8_t)v;
        }
    }
}

static void make_noise(uint8_t *gray, int w, int h, uint32_t seed)
{
    lcg_seed(seed);
    for (int i = 0; i < w * h; ++i) gray[i] = (uint8_t)(lcg_next() & 0xFF);
}

/* sparse bright dots on a horizontal gradient: large flat regions (exercises
 * the empty-cell minThFAST retry) plus isolated corners everywhere including
 * the ROI edges (exercises the brief clamp path for keypoints at x/y ~ 16). */
static void make_dots(uint8_t *gray, int w, int h, uint32_t seed, int spacing)
{
    lcg_seed(seed);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            gray[y * w + x] = (uint8_t)(x * 255 / w);
    for (int y = spacing / 2; y < h; y += spacing)
        for (int x = spacing / 2; x < w; x += spacing) {
            int jx = (int)(lcg_next() % (uint32_t)spacing) - spacing / 2;
            int jy = (int)(lcg_next() % (uint32_t)spacing) - spacing / 2;
            int px = x + jx, py = y + jy;
            if (px < 2 || py < 2 || px > w - 3 || py > h - 3) continue;
            gray[py * w + px] = 255;
            gray[py * w + px - 1] = 240; gray[py * w + px + 1] = 240;
            gray[(py - 1) * w + px] = 240; gray[(py + 1) * w + px] = 240;
        }
}

static int g_total_mism = 0;

static void run_case(const char *name, const uint8_t *gray, int w, int h,
                     int nfeat, float scale, int nlev, int iniTh, int minTh,
                     int bench_iter)
{
    ngd_orb_extractor ex1, ex2;
    if (ngd_orb_init(&ex1, nfeat, scale, nlev, iniTh, minTh) != 0 ||
        ngd_orb_init(&ex2, nfeat, scale, nlev, iniTh, minTh) != 0) {
        printf("FAIL %s: ngd_orb_init error\n", name);
        ++g_total_mism;
        return;
    }

    ngd_keypoint *k1 = (ngd_keypoint *)malloc(sizeof(ngd_keypoint) * MAXK);
    ngd_keypoint *k2 = (ngd_keypoint *)malloc(sizeof(ngd_keypoint) * MAXK);
    uint8_t *d1 = (uint8_t *)malloc((size_t)MAXK * 32);
    uint8_t *d2 = (uint8_t *)malloc((size_t)MAXK * 32);

    int n1 = dsp_orb_extract_legacy(&ex1, gray, w, h, w, k1, MAXK, d1);
    int n2 = dsp_orb_extract(&ex2, gray, w, h, w, k2, MAXK, d2);

    int mism = 0;
    if (n1 != n2) {
        printf("FAIL %s: keypoint count legacy=%d new=%d\n", name, n1, n2);
        ++mism;
    } else {
        if (memcmp(k1, k2, sizeof(ngd_keypoint) * (size_t)n1) != 0) {
            int km = 0;
            for (int i = 0; i < n1; ++i)
                if (memcmp(&k1[i], &k2[i], sizeof(ngd_keypoint)) != 0) ++km;
            printf("FAIL %s: %d/%d keypoints differ\n", name, km, n1);
            ++mism;
        }
        if (n1 > 0 && memcmp(d1, d2, (size_t)n1 * 32) != 0) {
            int dm = 0;
            for (int i = 0; i < n1; ++i)
                if (memcmp(d1 + (size_t)i * 32, d2 + (size_t)i * 32, 32) != 0) ++dm;
            printf("FAIL %s: %d/%d descriptors differ\n", name, dm, n1);
            ++mism;
        }
    }

    if (bench_iter > 0 && n1 > 0) {
        /* quick micro-benchmark (informational; not a pass/fail signal) */
        clock_t t0 = clock();
        for (int i = 0; i < bench_iter; ++i)
            dsp_orb_extract_legacy(&ex1, gray, w, h, w, k1, MAXK, d1);
        double t_leg = (double)(clock() - t0) / CLOCKS_PER_SEC;
        t0 = clock();
        for (int i = 0; i < bench_iter; ++i)
            dsp_orb_extract(&ex2, gray, w, h, w, k2, MAXK, d2);
        double t_new = (double)(clock() - t0) / CLOCKS_PER_SEC;
        printf("  %-28s %d kps | legacy %6.1f ms/call | new %6.1f ms/call | %.2fx\n",
               name, n1, t_leg * 1e3 / bench_iter, t_new * 1e3 / bench_iter,
               t_new > 0 ? t_leg / t_new : 0.0);
    }

    if (mism == 0) printf("PASS %s (%d kps bit-exact)\n", name, n1);
    g_total_mism += mism;

    free(k1); free(k2); free(d1); free(d2);
}

int main(void)
{
    static uint8_t img[640 * 480];

    /* two extractor configs: the SLAM default and one with odd thresholds to
     * stress the score>=t boundary conditions of both collection paths */
    printf("== config A: 1200 feat, 1.2, 8 lev, 20/7 (SLAM default) ==\n");
    make_checkerboard(img, 320, 240, 32);
    run_case("checker 320x240", img, 320, 240, 1200, 1.2f, 8, 20, 7, 0);
    make_noise(img, 320, 240, 12345);
    run_case("noise 320x240", img, 320, 240, 1200, 1.2f, 8, 20, 7, 0);
    make_noise(img, 640, 480, 54321);
    run_case("noise 640x480", img, 640, 480, 1200, 1.2f, 8, 20, 7, 20);
    make_dots(img, 320, 240, 999, 24);
    run_case("dots 320x24px", img, 320, 240, 1200, 1.2f, 8, 20, 7, 0);

    printf("== config B: 500 feat, 1.5, 4 lev, 10/5 (odd thresholds) ==\n");
    make_noise(img, 320, 240, 6789);
    run_case("noise 320x240", img, 320, 240, 500, 1.5f, 4, 10, 5, 0);
    make_dots(img, 256, 256, 777, 20);
    run_case("dots 256x20px", img, 256, 256, 500, 1.5f, 4, 10, 5, 10);
    /* small image: upper pyramid levels fall below 2*EDGE_THRESHOLD (NULL path) */
    make_noise(img, 96, 96, 4242);
    run_case("noise 96x96 small", img, 96, 96, 500, 1.5f, 4, 10, 5, 0);

    if (g_total_mism == 0) {
        printf("\nALL PASS: optimized pipeline is byte-identical to refappv8segdsp\n");
        return 0;
    }
    printf("\n%d FAILURE(S)\n", g_total_mism);
    return 1;
}
