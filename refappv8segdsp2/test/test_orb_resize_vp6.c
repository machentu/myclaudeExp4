/* test_orb_resize_vp6.c — orb_resize_strip_vp6 (IVP strip driver) vs
 * orb_resize_full (golden pure C): BYTE-EXACT comparison over the pyramid's
 * size family and edge shapes:
 *   - the level-1 sizes of a 320x240 / 640x480 pyramid (267x200 / 533x400),
 *     including chained deeper-level dims (223x167, 444x333-style odd tails)
 *   - tiny levels where dw or both dims drop under 64 (single partial chunk,
 *     stack-buffer tail path)
 *   - aggressive downscale (clamped right-edge columns) and identity scale
 *   - a tall narrow frame (strip passes exercise the yofs cursor)
 * The driver runs with the orb_tile_manager bank pools (bank-1 buffer set,
 * as in the DSP integration). Any differing byte fails the case.
 *
 * Build: linked via CMakeLists.txt (orb_resize_core + orb_tm).
 * Run: test_orb_resize_vp6.exe
 */
#include "orb_resize_core.h"
#include "orb_resize_strip_vp6.h"
#include "orb_tile_manager.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static xvTileManager g_tm;

static uint32_t g_rng = 0x12345678u;
static uint32_t rng32(void)
{
    g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5;
    return g_rng;
}

/* content mix: random noise (all tap values), plus a smooth gradient so the
 * clamped-edge columns (a = 2048/0) carry structured values too */
static void make_src(uint8_t *img, int w, int h, int noise)
{
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            int v = (x * 255) / (w > 1 ? w - 1 : 1);
            if (noise) v = (v >> 1) + (int)(rng32() & 0x7F);
            if (v > 255) v = 255;
            img[y * w + x] = (uint8_t)v;
        }
}

static int run_case(const char *name, int sw, int sh, int dw, int dh, int noise)
{
    uint8_t *src = (uint8_t *)malloc((size_t)sw * sh);
    uint8_t *g = (uint8_t *)malloc((size_t)dw * dh);
    uint8_t *d = (uint8_t *)malloc((size_t)dw * dh);
    make_src(src, sw, sh, noise);
    memset(g, 0xEE, (size_t)dw * dh);
    memset(d, 0xEE, (size_t)dw * dh);

    orb_resize_full(src, sw, sh, g, dw, dh);
    int rc = orb_resize_strip_vp6(&g_tm, src, sw, sh, d, dw, dh);

    int fails = 0;
    if (rc != 0) {
        printf("%-30s %dx%d -> %dx%d: FAIL (driver rc=%d)\n",
               name, sw, sh, dw, dh, rc);
        fails = 1;
    } else if (memcmp(g, d, (size_t)dw * dh) != 0) {
        int n = 0, first = -1;
        for (int i = 0; i < dw * dh; ++i)
            if (g[i] != d[i]) { if (first < 0) first = i; ++n; }
        printf("%-30s %dx%d -> %dx%d: FAIL (%d/%d bytes differ, first at "
               "(x=%d y=%d): golden %d driver %d)\n",
               name, sw, sh, dw, dh, n, dw * dh,
               first % dw, first / dw, g[first], d[first]);
        fails = 1;
    } else {
        printf("%-30s %dx%d -> %dx%d: PASS (bit-exact)\n",
               name, sw, sh, dw, dh);
    }
    free(src); free(g); free(d);
    return fails;
}

int main(void)
{
    if (orb_tile_manager_init(&g_tm) != 0) {
        fprintf(stderr, "orb_tile_manager_init failed\n");
        return 1;
    }

    int fails = 0;
    /* hand-computable regression (m = [1493,3200,4906; 6613,8320,10026],
     * a = [1707,341 / 1024,1024 / 341,1707], b = 2048/0 both rows) - checks
     * the vertical merge against independently derived expected bytes */
    {
        static const uint8_t src[8] = { 10, 20, 30, 40, 50, 60, 70, 80 };
        static const uint8_t want[6] = { 12, 25, 38, 52, 65, 78 };
        uint8_t g[6], d[6];
        orb_resize_full((const uint8_t *)src, 4, 2, g, 3, 2);
        int rc = orb_resize_strip_vp6(&g_tm, (const uint8_t *)src, 4, 2, d, 3, 2);
        if (rc != 0 || memcmp(g, d, 6) != 0 || memcmp(want, d, 6) != 0) {
            printf("%-30s 4x2 -> 3x2: FAIL (rc=%d)\n", "hand-4x2", rc);
            fails = 1;
        } else {
            printf("%-30s 4x2 -> 3x2: PASS (bit-exact)\n", "hand-4x2");
        }
    }

    /* level-1 sizes of the 320x240 and 640x480 pyramids (scale 1.2) */
    fails += run_case("level1-320",        320, 240, 267, 200, 1);
    fails += run_case("level1-640",        640, 480, 533, 400, 1);
    /* chained deeper levels (odd tails exercise the partial chunk paths) */
    fails += run_case("level2-320",        267, 200, 223, 167, 1);
    fails += run_case("level2-640",        533, 400, 444, 333, 1);
    fails += run_case("level3-320",        223, 167, 186, 139, 0);
    /* small frames: dw < 64 (single partial chunk), both dims < 64 */
    fails += run_case("small-dw<64",        74,  56,  62,  47, 1);
    fails += run_case("tiny-both<64",       61,  47,  51,  39, 0);
    fails += run_case("tiny-23",            33,  25,  27,  20, 1);
    /* aggressive downscale (right-edge clamps: a = 2048/0 columns) */
    fails += run_case("downscale-2.7x",    100,  75,  37,  28, 1);
    /* identity scale (every column a0=2048 or a1=2048) */
    fails += run_case("identity",           50,  50,  50,  50, 1);
    /* tall narrow: many strip passes, yofs cursor walks far */
    fails += run_case("tall-narrow",        97, 239,  81, 199, 1);

    orb_resize_strip_vp6_release();
    if (fails == 0) printf("ALL PASS\n");
    else printf("%d CASE(S) FAILED\n", fails);
    return fails ? 1 : 0;
}
