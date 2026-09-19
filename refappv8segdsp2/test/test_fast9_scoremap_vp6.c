/*
 * test_fast9_scoremap_vp6.c - 3-way test for the VP6+IVP FAST-9 score map.
 *
 *   (1) fast9_build_scoremap     - the PC golden (pure C, fast9_scoremap.c)
 *   (2) fast9_scoremap_vp6_build - strip ping-pong DMA + IVP 64-lane kernel
 *
 * Checks, per case:
 *   - the two maps are byte-identical over the whole frame (the VP6 map is
 *     pre-filled with a 0x5A sentinel, so any byte the driver fails to own -
 *     memset or write - shows up);
 *   - fast9_collect_cell run on both maps produces identical keypoint lists
 *     (count, order, all fields) at several ROIs and thresholds >= t_floor -
 *     i.e. the downstream consumer is bit-exact too.
 *
 * Covers: whole-frame strips at several strip heights {16,32,64}, several
 * t_floor {7,20,40}, random (corner-rich) / checker / constant images, small
 * frames (no vector chunk, pure scalar tail), one-chunk widths, and widths
 * that split chunks + tail unevenly.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fast9_scoremap.h"
#include "fast9_scoremap_vp6.h"
#include "tileManager.h"

/* tile manager / iDMA setup: shared InitTileMgr-style helper (2x128KB banks) */
#include "orb_tile_manager.h"

enum { IMG_CHECKER, IMG_RANDOM, IMG_CONSTANT };
static const char *img_name(int k) {
    switch (k) { case IMG_CHECKER: return "checker"; case IMG_RANDOM: return "random"; default: return "constant"; }
}
static void fill_image(uint8_t *p, int w, int h, int kind) {
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        int g;
        switch (kind) {
        case IMG_CHECKER:  g = (((x / 8) + (y / 8)) & 1) ? 255 : 0; break;
        case IMG_RANDOM:   g = rand() & 255; break;
        default:           g = 128; break;
        }
        p[(size_t)y * w + x] = (uint8_t)g;
    }
}

static int kps_equal(const fast9_keypoint *a, const fast9_keypoint *b, int n) {
    for (int i = 0; i < n; i++)
        if (a[i].x != b[i].x || a[i].y != b[i].y || a[i].response != b[i].response ||
            a[i].octave != b[i].octave || a[i].angle != b[i].angle || a[i].size != b[i].size)
            return 0;
    return 1;
}

/* first differing byte in the two maps, as frame coords (for diagnostics) */
static int first_diff(const uint8_t *a, const uint8_t *b, int w, int h,
                      int *dx, int *dy, int *da, int *db)
{
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            if (a[(size_t)y * w + x] != b[(size_t)y * w + x]) {
                *dx = x; *dy = y;
                *da = a[(size_t)y * w + x]; *db = b[(size_t)y * w + x];
                return 1;
            }
    return 0;
}

/* Returns 0 on full match (map bytes + every collect cell comparison). */
static int run_one(xvTileManager *tm, int w, int h, int t_floor, int strip_h, int kind)
{
    uint8_t *src     = (uint8_t *)malloc((size_t)w * h);
    uint8_t *map_ref = (uint8_t *)malloc((size_t)w * h);
    uint8_t *map_vp6 = (uint8_t *)malloc((size_t)w * h);
    if (!src || !map_ref || !map_vp6) { fprintf(stderr, "oom\n"); free(src); free(map_ref); free(map_vp6); return 1; }
    fill_image(src, w, h, kind);

    fast9_build_scoremap(src, w, h, w, t_floor, map_ref, w);
    memset(map_vp6, 0x5A, (size_t)w * h);   /* sentinel: driver must own every byte */

    xvFrame *src_frame = xvCreateFrame(tm, src, (uint32_t)w * h, w, h, w, 1, 1, FRAME_EDGE_PADDING, 0);
    if (!src_frame) { fprintf(stderr, "  frame create failed\n"); free(src); free(map_ref); free(map_vp6); return 1; }
    Fast9ScoremapVp6Config cfg = { strip_h, w, strip_h + 2 * FAST9_SCOREMAP_HALO };
    int rc = fast9_scoremap_vp6_build(tm, src_frame, w, h, t_floor, map_vp6, w, &cfg);
    xvFreeFrame(tm, src_frame);

    int ok = 0;
    int corners = 0;
    for (int i = 0; i < w * h; ++i) if (map_ref[i] != FAST9_MAP_NONE) ++corners;

    if (rc != 0) {
        fprintf(stderr, "  fast9_scoremap_vp6_build failed: %d\n", rc);
    } else {
        int dx, dy, da, db;
        int same_map = !first_diff(map_ref, map_vp6, w, h, &dx, &dy, &da, &db);

        /* downstream consumer: collect+NMS from both maps must agree */
        const int BIG = 1 << 20;
        int n_roi = 0, n_roi_ok = 0;
        struct { int ix, iy, mx, my; } rois[4] = {
            { 0, 0, w, h },
            { 3, 3, w - 3, h - 3 },
            { 16, 16, w - 16, h - 16 },
            { w / 3, h / 3, w - w / 3, h - h / 3 },
        };
        int ts[3] = { t_floor, t_floor + 6, t_floor + 13 };
        fast9_keypoint *ka = (fast9_keypoint *)malloc(sizeof(fast9_keypoint) * (size_t)(corners + 8));
        fast9_keypoint *kb = (fast9_keypoint *)malloc(sizeof(fast9_keypoint) * (size_t)(corners + 8));
        for (int r = 0; r < 4; ++r) {
            if (rois[r].ix >= rois[r].mx || rois[r].iy >= rois[r].my) continue;
            if (rois[r].mx > w || rois[r].my > h) continue;
            for (int ti = 0; ti < 3; ++ti) {
                int na = 0, nb = 0;
                fast9_collect_cell(map_ref, w, rois[r].ix, rois[r].mx, rois[r].iy, rois[r].my,
                                   ts[ti], ka, &na, BIG);
                fast9_collect_cell(map_vp6, w, rois[r].ix, rois[r].mx, rois[r].iy, rois[r].my,
                                   ts[ti], kb, &nb, BIG);
                ++n_roi;
                if (na == nb && kps_equal(ka, kb, na)) ++n_roi_ok;
                else fprintf(stderr, "    collect DIFF: roi=[%d,%d)x[%d,%d) t=%d na=%d nb=%d\n",
                             rois[r].ix, rois[r].mx, rois[r].iy, rois[r].my, ts[ti], na, nb);
            }
        }
        free(ka); free(kb);

        ok = same_map && n_roi_ok == n_roi;
        printf("  %-9s %dx%d t_floor=%2d strip=%-2d : corners=%-6d map=%s collect=%d/%d\n",
               img_name(kind), w, h, t_floor, strip_h, corners,
               same_map ? "ok" : "DIFF", n_roi_ok, n_roi);
        if (!same_map)
            fprintf(stderr, "    MAP DIFF at (%d,%d): ref=%d vp6=%d\n", dx, dy, da, db);
    }

    free(src); free(map_ref); free(map_vp6);
    return ok ? 0 : 1;
}

int main(void)
{
    srand(1234);
    xvTileManager tm;
    if (orb_tile_manager_init(&tm) != 0) return 1;

    printf("=== VP6+IVP score map vs PC golden (fast9_build_scoremap vs fast9_scoremap_vp6_build) ===\n");

    int fail = 0;
    struct { int w, h, t, sh, kind; } cs[] = {
        {128, 96,   7, 64, IMG_RANDOM},    /* baseline: 2 strips */
        {128, 96,  20, 32, IMG_RANDOM},    /* more strips */
        {128, 96,   7, 16, IMG_CHECKER},   /* many strips, checker */
        {128, 96,  40, 64, IMG_RANDOM},    /* high t_floor */
        {320,240,   7, 64, IMG_RANDOM},    /* bigger frame */
        { 30, 30,  20, 16, IMG_RANDOM},    /* no vector chunk (pure tail) */
        { 70, 50,   7, 32, IMG_RANDOM},    /* exactly one chunk + no tail */
        { 71, 50,  20, 32, IMG_RANDOM},    /* one chunk + 1-column tail */
        {129, 96,   7, 64, IMG_RANDOM},    /* chunk/tail split 2x64+1 */
        { 64, 64,  20, 64, IMG_CONSTANT},  /* no corners anywhere */
        { 16, 16,   7, 16, IMG_RANDOM},    /* tiny (3..w-4 = single col) */
        {128, 96,   7, 96, IMG_RANDOM},    /* strip == frame height */
    };
    const int n = (int)(sizeof(cs) / sizeof(cs[0]));
    for (int i = 0; i < n; i++)
        if (run_one(&tm, cs[i].w, cs[i].h, cs[i].t, cs[i].sh, cs[i].kind) != 0)
            fail = 1;

    printf("=== %s ===\n", fail ? "FAIL" : "ALL PASS");
    return fail ? 1 : 0;
}
