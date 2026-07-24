/*
 * test_orb_compare.c — Compare ngd_orb_extract (original) vs demo_orb_extract (demo)
 * step by step on the same input image.
 *
 * Builds as a standalone executable in refappv8seg/test/.
 * Links against ngd_core.lib + ngd_app_v8seg.lib.
 *
 * Usage: test_orb_compare <image.pgm>
 * Output: per-stage comparison to stdout + two PPM marked images.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#include "ngd/orb.h"
#include "ngd_app/demo_orb.h"

/* ---- PGM loader (from demo_feature_extraction.c) ---- */
static uint8_t *read_pgm(const char *filename, int *out_w, int *out_h)
{
    FILE *f = fopen(filename, "rb");
    if (!f) { fprintf(stderr, "ERROR: Cannot open %s\n", filename); return NULL; }
    char magic[3];
    if (!fgets(magic, sizeof(magic), f) || magic[0] != 'P' || magic[1] != '5') {
        fprintf(stderr, "ERROR: Not a PGM P5 file\n"); fclose(f); return NULL;
    }
    int c = fgetc(f);
    while (c == '#') { while (fgetc(f) != '\n'); c = fgetc(f); }
    ungetc(c, f);
    int w, h, maxval;
    if (fscanf(f, "%d %d %d", &w, &h, &maxval) != 3) {
        fprintf(stderr, "ERROR: Invalid PGM header\n"); fclose(f); return NULL;
    }
    fgetc(f);
    uint8_t *data = (uint8_t *)malloc((size_t)w * h);
    if (!data) { fclose(f); return NULL; }
    if (fread(data, 1, (size_t)w * h, f) != (size_t)(w * h)) {
        fprintf(stderr, "ERROR: Truncated PGM\n"); free(data); fclose(f); return NULL;
    }
    *out_w = w; *out_h = h;
    fclose(f);
    return data;
}

/* ---- PPM writer with keypoint overlay ---- */
static void write_ppm_kpts(const char *filename, const uint8_t *gray, int w, int h,
                           const ngd_keypoint *kpts, int n, int r, int g, int b)
{
    uint8_t *rgb = (uint8_t *)malloc((size_t)w * h * 3);
    for (int i = 0; i < w * h; i++)
        rgb[i*3] = rgb[i*3+1] = rgb[i*3+2] = gray[i];
    for (int k = 0; k < n; k++) {
        int cx = (int)lroundf(kpts[k].x), cy = (int)lroundf(kpts[k].y);
        for (int dy = -3; dy <= 3; dy++) {
            for (int dx = -3; dx <= 3; dx++) {
                if (dx != 0 && dy != 0) continue;
                int px = cx + dx, py = cy + dy;
                if (px >= 0 && px < w && py >= 0 && py < h) {
                    rgb[(py*w + px)*3 + 0] = (uint8_t)r;
                    rgb[(py*w + px)*3 + 1] = (uint8_t)g;
                    rgb[(py*w + px)*3 + 2] = (uint8_t)b;
                }
            }
        }
    }
    FILE *f = fopen(filename, "wb");
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    fwrite(rgb, 1, (size_t)w * h * 3, f);
    fclose(f);
    free(rgb);
    printf("  Wrote: %s (%d keypoints)\n", filename, n);
}

/* ---- Main ---- */
int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <image.pgm>\n", argv[0]);
        fprintf(stderr, "  Compares ngd_orb_extract vs demo_orb_extract step by step.\n");
        return 1;
    }
    int w, h;
    uint8_t *img = read_pgm(argv[1], &w, &h);
    if (!img) return 1;
    printf("Image: %s (%dx%d)\n\n", argv[1], w, h);

    /* ---- Init both extractors identically ---- */
    const int nfeatures = 1000;
    const float scaleFactor = 1.2f;
    const int nlevels = 8;
    const int iniThFAST = 20, minThFAST = 7;

    ngd_orb_extractor ex_orig, ex_demo;
    ngd_orb_init(&ex_orig, nfeatures, scaleFactor, nlevels, iniThFAST, minThFAST);
    ngd_orb_init(&ex_demo, nfeatures, scaleFactor, nlevels, iniThFAST, minThFAST);

    printf("Extractor params: nfeatures=%d scale=%.1f levels=%d iniTh=%d minTh=%d\n\n",
           nfeatures, (double)scaleFactor, nlevels, iniThFAST, minThFAST);

    /* Print per-level feature allocation (same for both) */
    printf("Per-level feature targets:\n");
    for (int l = 0; l < nlevels; l++)
        printf("  Level %d: %d  (scale=%.3f, inv=%.3f, sigma2=%.3f)\n",
               l, ex_orig.mnFeaturesPerLevel[l],
               (double)ex_orig.mvScaleFactor[l],
               (double)ex_orig.mvInvScaleFactor[l],
               (double)ex_orig.mvLevelSigma2[l]);

    /* ---- Run both extractors ---- */
    const int max_kps = nfeatures * 2 + 512;
    ngd_keypoint *kps1 = (ngd_keypoint *)malloc(sizeof(ngd_keypoint) * (size_t)max_kps);
    ngd_keypoint *kps2 = (ngd_keypoint *)malloc(sizeof(ngd_keypoint) * (size_t)max_kps);
    uint8_t *desc1 = (uint8_t *)malloc((size_t)max_kps * 32);
    uint8_t *desc2 = (uint8_t *)malloc((size_t)max_kps * 32);

    printf("\n===== Running ngd_orb_extract (original) =====\n");
    int n1 = ngd_orb_extract(&ex_orig, img, w, h, w, kps1, max_kps, desc1);
    printf("  Total keypoints: %d\n", n1);

    printf("\n===== Running demo_orb_extract (demo) =====\n");
    int n2 = demo_orb_extract(&ex_demo, img, w, h, w, kps2, max_kps, desc2);
    printf("  Total keypoints: %d\n", n2);

    /* ---- Per-octave breakdown ---- */
    int oct_cnt1[NGD_ORB_MAX_LEVELS] = {0}, oct_cnt2[NGD_ORB_MAX_LEVELS] = {0};
    for (int i = 0; i < n1; i++) oct_cnt1[kps1[i].octave]++;
    for (int i = 0; i < n2; i++) oct_cnt2[kps2[i].octave]++;

    printf("\n===== Per-octave keypoint count =====\n");
    printf("  Octave | Original | Demo    | Target\n");
    printf("  -------+----------+---------+-------\n");
    for (int l = 0; l < nlevels; l++) {
        if (oct_cnt1[l] == 0 && oct_cnt2[l] == 0 && ex_orig.mnFeaturesPerLevel[l] == 0) continue;
        printf("  %6d | %8d | %7d | %6d\n", l, oct_cnt1[l], oct_cnt2[l],
               ex_orig.mnFeaturesPerLevel[l]);
    }

    /* ---- Spatial coverage ---- */
    {
        float xmin1 = w, xmax1 = 0, ymin1 = h, ymax1 = 0;
        float xmin2 = w, xmax2 = 0, ymin2 = h, ymax2 = 0;
        float sx1 = 0, sy1 = 0, sx2 = 0, sy2 = 0;
        for (int i = 0; i < n1; i++) {
            if (kps1[i].x < xmin1) xmin1 = kps1[i].x;
            if (kps1[i].x > xmax1) xmax1 = kps1[i].x;
            if (kps1[i].y < ymin1) ymin1 = kps1[i].y;
            if (kps1[i].y > ymax1) ymax1 = kps1[i].y;
            sx1 += kps1[i].x; sy1 += kps1[i].y;
        }
        for (int i = 0; i < n2; i++) {
            if (kps2[i].x < xmin2) xmin2 = kps2[i].x;
            if (kps2[i].x > xmax2) xmax2 = kps2[i].x;
            if (kps2[i].y < ymin2) ymin2 = kps2[i].y;
            if (kps2[i].y > ymax2) ymax2 = kps2[i].y;
            sx2 += kps2[i].x; sy2 += kps2[i].y;
        }
        printf("\n===== Spatial coverage =====\n");
        printf("  %15s | %10s | %10s\n", "Original", "Demo", "Image");
        printf("  %15s-+%10s-+%10s\n", "---------------", "----------", "----------");
        printf("  X range:  %4.0f-%-4.0f | %4.0f-%-4.0f | 0-%d\n",
               (double)xmin1, (double)xmax1, (double)xmin2, (double)xmax2, w);
        printf("  Y range:  %4.0f-%-4.0f | %4.0f-%-4.0f | 0-%d\n",
               (double)ymin1, (double)ymax1, (double)ymin2, (double)ymax2, h);
        printf("  Mean:     (%5.0f,%5.0f) | (%5.0f,%5.0f)\n",
               n1 > 0 ? (double)(sx1/n1) : 0.0, n1 > 0 ? (double)(sy1/n1) : 0.0,
               n2 > 0 ? (double)(sx2/n2) : 0.0, n2 > 0 ? (double)(sy2/n2) : 0.0);
    }

    /* ---- Keypoint proximity: for each demo kp, find nearest original kp ---- */
    {
        printf("\n===== Keypoint proximity (demo kp -> nearest original kp) =====\n");
        int within_1 = 0, within_2 = 0, within_5 = 0, within_10 = 0;
        float total_dist = 0;
        float min_dist = 1e9f, max_dist = 0;
        for (int i = 0; i < n2; i++) {
            float best = 1e9f;
            for (int j = 0; j < n1; j++) {
                float dx = kps2[i].x - kps1[j].x;
                float dy = kps2[i].y - kps1[j].y;
                float d = sqrtf(dx*dx + dy*dy);
                if (d < best) best = d;
            }
            total_dist += best;
            if (best < min_dist) min_dist = best;
            if (best > max_dist) max_dist = best;
            if (best <= 1) within_1++;
            if (best <= 2) within_2++;
            if (best <= 5) within_5++;
            if (best <= 10) within_10++;
        }
        printf("  Within 1px: %d (%.1f%%)\n", within_1, n2 > 0 ? 100.0*within_1/n2 : 0.0);
        printf("  Within 2px: %d (%.1f%%)\n", within_2, n2 > 0 ? 100.0*within_2/n2 : 0.0);
        printf("  Within 5px: %d (%.1f%%)\n", within_5, n2 > 0 ? 100.0*within_5/n2 : 0.0);
        printf("  Within 10px: %d (%.1f%%)\n", within_10, n2 > 0 ? 100.0*within_10/n2 : 0.0);
        printf("  Min dist: %.1f  Max dist: %.1f  Avg dist: %.1f\n",
               (double)min_dist, (double)max_dist, n2 > 0 ? (double)(total_dist/n2) : 0.0);
    }

    /* ---- Descriptor statistics ---- */
    {
        printf("\n===== Descriptor stats =====\n");
        int pop1[32] = {0}, pop2[32] = {0};
        for (int i = 0; i < n1; i++) {
            for (int b = 0; b < 32; b++) {
                uint8_t v = desc1[i*32 + b];
                while (v) { pop1[b] += (v & 1); v >>= 1; }
            }
        }
        for (int i = 0; i < n2; i++) {
            for (int b = 0; b < 32; b++) {
                uint8_t v = desc2[i*32 + b];
                while (v) { pop2[b] += (v & 1); v >>= 1; }
            }
        }
        int sum1 = 0, sum2 = 0;
        for (int b = 0; b < 32; b++) { sum1 += pop1[b]; sum2 += pop2[b]; }
        printf("  Avg bits set per byte: orig=%.1f  demo=%.1f (expect ~4.0 for random)\n",
               n1 > 0 ? (double)sum1 / (32.0 * n1) : 0.0,
               n2 > 0 ? (double)sum2 / (32.0 * n2) : 0.0);
    }

    /* ---- Per-level coordinate check (level 0 keypoints from demo) ---- */
    {
        printf("\n===== Demo per-level coordinate sample =====\n");
        for (int l = 0; l < nlevels; l++) {
            if (oct_cnt2[l] == 0) continue;
            int start = 0;
            for (int ll = 0; ll < l; ll++) start += oct_cnt2[ll];
            printf("  Level %d (scale=%.2f): first 3 kpts at level-coords:",
                   l, (double)ex_demo.mvScaleFactor[l]);
            for (int k = start; k < start + 3 && k < n2; k++) {
                /* Reconstruct level coordinates: x/scale, y/scale */
                float lx = kps2[k].x / ex_demo.mvScaleFactor[l];
                float ly = kps2[k].y / ex_demo.mvScaleFactor[l];
                printf(" (%.0f,%.0f)->(%.0f,%.0f)", (double)lx, (double)ly,
                       (double)kps2[k].x, (double)kps2[k].y);
            }
            printf("\n");
        }
    }

    /* ---- Save marked images ---- */
    printf("\n===== Visualization =====\n");
    write_ppm_kpts("test_orig_kpts.ppm", img, w, h, kps1, n1, 0, 255, 0);   /* green */
    write_ppm_kpts("test_demo_kpts.ppm", img, w, h, kps2, n2, 255, 0, 0);   /* red */

    /* Cleanup */
    free(img); free(kps1); free(kps2); free(desc1); free(desc2);
    printf("\nDone.\n");
    return 0;
}
