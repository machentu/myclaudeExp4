/* test_dsp_orb_vp6.c — dsp_orb_vp6_extract (four VP6+IVP drivers orchestrated
 * end-to-end) vs dsp_orb_extract (pure-C kernels): BYTE-EXACT comparison.
 *
 * Both paths share the same quadtree (dsp_orb_octtree), the same
 * fast9_collect_cell grid/retry and the same orb_resize_full pyramid; the
 * difference is exactly the four pixel kernels (score map / blur / IC angle /
 * rBRIEF) running through the TileManager strip/tile drivers with the
 * orb_tile_manager bank pools. Keypoints and descriptors must be
 * memcmp-identical for every case, including 640x480 (the default configs'
 * width limit) and tiny levels where the pyramid collapses.
 *
 * Build: linked via CMakeLists.txt (dsp_orb + dsp_orb_vp6 + orb_tm).
 * Run: test_dsp_orb_vp6.exe
 */
#include "ngd/orb.h"
#include "ngd_app/dsp_orb.h"
#include "dsp/dsp_orb_vp6.h"
#include "dsp/brief_vp6.h"      /* brief sub-stage counters (timing report) */
#include "dsp/orb_tile_manager.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXK 2000

static xvTileManager g_tm;

static uint32_t g_rng = 0x9E3779B9u;
static uint32_t rng32(void)
{
    g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5;
    return g_rng;
}

/* 8x8-ish checkerboard: strong corners at every junction, slight gradient so
 * orientation is unambiguous (same generator family as test_dsp_orb.c) */
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

/* uniform random noise: maximal FAST-9 corner density — stresses the strip
 * sweep and the per-keypoint drivers at full load */
static void make_random(uint8_t *gray, int w, int h)
{
    for (int i = 0; i < w * h; ++i) gray[i] = (uint8_t)(rng32() & 0xFF);
}

/* smooth gradient alone has no corners; add sparse high-contrast dots so
 * deeper (tiny) pyramid levels still produce a handful of keypoints */
static void make_gradient_dots(uint8_t *gray, int w, int h)
{
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            gray[y * w + x] = (uint8_t)(x * 255 / (w > 1 ? w - 1 : 1));
    for (int d = 0; d < 12; ++d) {
        int cx = 10 + (int)(rng32() % (uint32_t)(w - 20));
        int cy = 10 + (int)(rng32() % (uint32_t)(h - 20));
        for (int y = cy - 2; y <= cy + 2; ++y)
            for (int x = cx - 2; x <= cx + 2; ++x)
                gray[y * w + x] = (uint8_t)(((x ^ y) & 1) ? 240 : 15);
    }
}

static int run_case(const char *name, const uint8_t *gray, int w, int h)
{
    printf("run_case\n");

    ngd_orb_extractor exA, exB;
    ngd_orb_init(&exA, 1000, 1.2f, 8, 20, 7);
    ngd_orb_init(&exB, 1000, 1.2f, 8, 20, 7);

    ngd_keypoint *kA = (ngd_keypoint*)malloc(sizeof(ngd_keypoint) * (size_t)MAXK);
    ngd_keypoint *kB = (ngd_keypoint*)malloc(sizeof(ngd_keypoint) * (size_t)MAXK);
    uint8_t *dA = (uint8_t*)malloc((size_t)MAXK * 32);
    uint8_t *dB = (uint8_t*)malloc((size_t)MAXK * 32);

    int nA = dsp_orb_extract(&exA, gray, w, h, w, kA, MAXK, dA);

    printf("begin dsp part\n");

    /* per-stage breakdown of the vp6 call (us on host, cycles on Xtensa) */
    for (int i = 0; i < DSP_ORB_VP6_T_COUNT; ++i) dsp_orb_vp6_cycles[i] = 0;
    dsp_orb_vp6_calls = 0;
    for (int i = 0; i < BRIEF_VP6_T_COUNT; ++i) brief_vp6_cycles[i] = 0;
    brief_vp6_kps = 0;
    int nB = dsp_orb_vp6_extract(&g_tm, &exB, gray, w, h, w, kB, MAXK, dB);
    printf("[timing] %s %dx%d:\n", name, w, h);
    dsp_orb_vp6_timing_report();
    brief_vp6_timing_report();

    printf("%-28s %dx%d: pure-C n=%d vp6 n=%d", name, w, h, nA, nB);
    if (nA < 0 || nB < 0) {
        printf("  FAIL (error: pure-C %d, vp6 %d)\n", nA, nB);
        free(kA); free(kB); free(dA); free(dB);
        return 1;
    }
    if (nA != nB) {
        printf("  FAIL (keypoint count differs)\n");
        free(kA); free(kB); free(dA); free(dB);
        return 1;
    }
    if (nA > 0) {
        if (memcmp(kA, kB, sizeof(ngd_keypoint) * (size_t)nA) != 0) {
            printf("  FAIL (keypoint bytes differ)\n");
            /* first differing keypoint for diagnosis */
            for (int i = 0; i < nA; ++i)
                if (memcmp(&kA[i], &kB[i], sizeof(ngd_keypoint)) != 0) {
                    printf("    first diff at kp %d: A(x=%.2f y=%.2f a=%.3f r=%.1f o=%d s=%.1f)"
                           " B(x=%.2f y=%.2f a=%.3f r=%.1f o=%d s=%.1f)\n",
                           i, kA[i].x, kA[i].y, kA[i].angle, kA[i].response, kA[i].octave, kA[i].size,
                           kB[i].x, kB[i].y, kB[i].angle, kB[i].response, kB[i].octave, kB[i].size);
                    break;
                }
            free(kA); free(kB); free(dA); free(dB);
            return 1;
        }
        if (memcmp(dA, dB, (size_t)nA * 32) != 0) {
            printf("  FAIL (descriptor bytes differ)\n");
            free(kA); free(kB); free(dA); free(dB);
            return 1;
        }
    }
    printf("  PASS (bit-exact)\n");
    free(kA); free(kB); free(dA); free(dB);
    return 0;
}

int main(void)
{
    if (orb_tile_manager_init(&g_tm) != 0) {
        fprintf(stderr, "orb_tile_manager_init failed\n");
        return 1;
    }

    int fails = 0;
    {
        int w = 320, h = 240;
        uint8_t *img = (uint8_t*)malloc((size_t)w * h);
        make_checkerboard(img, w, h, 32);
        fails += run_case("checkerboard", img, w, h);
        free(img);
    }
    {
        int w = 640, h = 480;   /* default configs' width limit */
        uint8_t *img = (uint8_t*)malloc((size_t)w * h);
        make_checkerboard(img, w, h, 40);
        fails += run_case("checkerboard-640", img, w, h);
        free(img);
    }
    {
        int w = 200, h = 150;
        uint8_t *img = (uint8_t*)malloc((size_t)w * h);
        make_random(img, w, h);
        fails += run_case("random-noise", img, w, h);
        free(img);
    }
    {
        int w = 128, h = 96;    /* pyramid collapses below 38px levels */
        uint8_t *img = (uint8_t*)malloc((size_t)w * h);
        make_gradient_dots(img, w, h);
        fails += run_case("gradient+dots", img, w, h);
        free(img);
    }

    if (fails == 0) printf("ALL PASS\n");
    else printf("%d CASE(S) FAILED\n", fails);
    return fails ? 1 : 0;
}
