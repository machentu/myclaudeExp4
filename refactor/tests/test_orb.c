/* test_orb.c — validate the pure-C ORB extractor on synthetic images. */
#include "ngd/orb.h"
#include "ngd/matcher.h"

#include <stdio.h>
#include <math.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { if(!(cond)){ printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); fails++; } } while(0)

static unsigned int rng = 12345u;
static float frand(void){ rng = rng*1664525u + 1013904223u; return (rng >> 8) * (1.0f/16777216.0f); }

#define W 640
#define H 480
#define NFEAT 1000

static void fill_texture(uint8_t *img) {
    /* high-contrast random block texture -> many FAST corners */
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            img[y*W + x] = (uint8_t)(frand()*255);
    /* overlay some solid black/white squares for guaranteed corners */
    for (int by = 0; by < H; by += 60)
        for (int bx = 0; bx < W; bx += 60)
            for (int y = by; y < by+30 && y < H; ++y)
                for (int x = bx; x < bx+30 && x < W; ++x)
                    img[y*W + x] = (uint8_t)(((bx+by)/60) & 1 ? 255 : 0);
}

int main(void)
{
    static uint8_t img[W*H];
    fill_texture(img);

    ngd_orb_extractor ex;
    ngd_orb_init(&ex, NFEAT, 1.2f, 8, 20, 7);

    ngd_keypoint kps1[NFEAT*2], kps2[NFEAT*2];
    static uint8_t desc1[NFEAT*2*32], desc2[NFEAT*2*32];

    int n1 = ngd_orb_extract(&ex, img, W, H, W, kps1, NFEAT*2, desc1);
    int n2 = ngd_orb_extract(&ex, img, W, H, W, kps2, NFEAT*2, desc2);

    CHECK(n1 > 100, "extracts a healthy feature count");
    CHECK(n1 <= NFEAT + NFEAT/4, "feature count near the budget (quadtree may overshoot)");
    CHECK(n1 == n2, "deterministic count across runs");

    /* bounds + descriptor validity */
    int nonzero_desc = 0;
    for (int i = 0; i < n1; ++i) {
        CHECK(kps1[i].x >= 0 && kps1[i].x < W && kps1[i].y >= 0 && kps1[i].y < H, "kp in bounds");
        CHECK(kps1[i].octave >= 0 && kps1[i].octave < 8, "octave in range");
        int any = 0; for (int b = 0; b < 32; ++b) if (desc1[i*32+b]) { any = 1; break; }
        if (any) nonzero_desc++;
    }
    CHECK(nonzero_desc > n1/2, "most descriptors are non-trivial");
    /* determinism: identical descriptors */
    CHECK(memcmp(desc1, desc2, (size_t)n1*32) == 0, "deterministic descriptors");

    /* spatial distribution: features should span the image (quadtree) */
    int quad[3][3] = {{0}};
    for (int i = 0; i < n1; ++i) {
        int qx = (int)(kps1[i].x / (W/3)); if (qx>2) qx=2;
        int qy = (int)(kps1[i].y / (H/3)); if (qy>2) qy=2;
        quad[qx][qy]++;
    }
    int filled = 0;
    for (int a = 0; a < 3; ++a) for (int b = 0; b < 3; ++b) if (quad[a][b] > 5) filled++;
    CHECK(filled >= 7, "features distributed across most quadrants");
    printf("  extracted %d/%d features, %d/9 quadrants filled\n", n1, NFEAT, filled);

    /* ---- rotation invariance: isolated planted corner, 90 deg rotation ---- */
    #define SW 120
    static uint8_t small[SW*SW], rot[SW*SW];
    memset(small, 200, sizeof(small));
    /* black square with a sharp corner near centre */
    for (int y = 40; y < 80; ++y) for (int x = 40; x < 80; ++x) small[y*SW+x] = 40;
    /* 90-deg CW rotation: rot[y][x] = small[x][SW-1-y] */
    for (int y = 0; y < SW; ++y) for (int x = 0; x < SW; ++x) rot[y*SW+x] = small[x*SW + (SW-1-y)];

    ngd_keypoint ks1[64], ks2[64];
    uint8_t ds1[64*32], ds2[64*32];
    int m1 = ngd_orb_extract(&ex, small, SW, SW, SW, ks1, 64, ds1);
    int m2 = ngd_orb_extract(&ex, rot,   SW, SW, SW, ks2, 64, ds2);
    CHECK(m1 > 0 && m2 > 0, "planted-corner images yield features");
    /* find the corner keypoint nearest the square corner (60,60) in each image;
     * under 90-deg CW rotation (60,60) -> (60, 59). Descriptors should match. */
    int bi1 = 0; float bd1 = 1e9f;
    for (int i = 0; i < m1; ++i) { float d = hypotf(ks1[i].x-60.f, ks1[i].y-60.f); if (d < bd1) { bd1=d; bi1=i; } }
    int bi2 = 0; float bd2 = 1e9f;
    for (int i = 0; i < m2; ++i) { float d = hypotf(ks2[i].x-60.f, ks2[i].y-59.f); if (d < bd2) { bd2=d; bi2=i; } }
    int ham = ngd_descriptor_distance(ds1 + bi1*32, ds2 + bi2*32);
    printf("  rotation invariance: hamming(90deg) = %d / 256\n", ham);
    CHECK(ham < 64, "rBRIEF rotation invariance within 64 bits");

    if (fails == 0) { printf("test_orb: PASS\n"); return 0; }
    printf("test_orb: %d FAILURES\n", fails);
    return 1;
}
