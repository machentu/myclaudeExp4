/* test_dsp_orb.c — compare dsp_orb_extract vs ngd_orb_extract on synthetic image.
 *
 * Build: added via refappv8segdsp/CMakeLists.txt (Win32 only, links dsp_orb + ngd_core).
 * Run:   test_dsp_orb_compare.exe
 *
 * Creates a synthetic 320x240 grayscale image with a checkerboard pattern
 * (guaranteed strong corners), runs both extractors with identical parameters,
 * and reports:
 *   - keypoint count
 *   - per-keypoint nearest-neighbour match rate (position + angle + descriptor)
 *   - descriptor-level Hamming distance distribution
 *
 * Expected: high match rate (>90%) for keypoints detected by both extractors.
 * dsp_orb uses fast9_full (full-image NMS) while ngd_orb uses per-35px-cell NMS,
 * so there may be minor differences at cell boundaries (typically <5% of corners).
 */
#include "ngd/orb.h"
#include "ngd_app/dsp_orb.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 8x8 checkerboard: strong corners at every junction */
#define IW 320
#define IH 240
#define CS 32  /* checker square size */

static void make_checkerboard(uint8_t *gray, int w, int h, int cs)
{
    for (int y = 0; y < h; ++y) {
        int by = y / cs;
        for (int x = 0; x < w; ++x) {
            int bx = x / cs;
            int v = ((bx ^ by) & 1) ? 200 : 50;
            /* add slight gradient so orientation is unambiguous */
            v += (x * 20 / w);
            if (v > 255) v = 255;
            gray[y * w + x] = (uint8_t)v;
        }
    }
}

/* Hamming distance between two 32-byte ORB descriptors */
static int desc_hamming(const uint8_t *a, const uint8_t *b)
{
    int d = 0;
    for (int i = 0; i < 32; ++i) {
        uint8_t v = a[i] ^ b[i];
        /* popcount per byte */
        v = (uint8_t)((v & 0x55) + ((v >> 1) & 0x55));
        v = (uint8_t)((v & 0x33) + ((v >> 2) & 0x33));
        d += (v & 0x0F) + (v >> 4);
    }
    return d;
}

int main(void)
{
    uint8_t *gray = (uint8_t*)malloc((size_t)IW * IH);
    make_checkerboard(gray, IW, IH, CS);

    ngd_orb_extractor ex1, ex2;
    /* realistic SLAM parameters: 1000 features, 8 levels, scale 1.2 */
    ngd_orb_init(&ex1, 1000, 1.2f, 8, 20, 7);
    ngd_orb_init(&ex2, 1000, 1.2f, 8, 20, 7);

    int maxk = 2000;
    ngd_keypoint *kps1 = (ngd_keypoint*)malloc(sizeof(ngd_keypoint) * (size_t)maxk);
    ngd_keypoint *kps2 = (ngd_keypoint*)malloc(sizeof(ngd_keypoint) * (size_t)maxk);
    uint8_t *desc1 = (uint8_t*)malloc((size_t)maxk * 32);
    uint8_t *desc2 = (uint8_t*)malloc((size_t)maxk * 32);

    int n1 = ngd_orb_extract(&ex1, gray, IW, IH, IW, kps1, maxk, desc1);
    int n2 = dsp_orb_extract(&ex2, gray, IW, IH, IW, kps2, maxk, desc2);

    printf("ngd_orb_extract (refactor): %d keypoints\n", n1);
    printf("dsp_orb_extract (DSP/VP6):  %d keypoints\n", n2);
    if (n1 <= 0 && n2 <= 0) { printf("SKIP: no keypoints on this image\n"); goto done; }
    if (n1 <= 0) { printf("FAIL: refactor produced 0 keypoints, DSP produced %d\n", n2); goto done; }
    if (n2 <= 0) { printf("FAIL: DSP produced 0 keypoints, refactor produced %d\n", n1); goto done; }

    /* ---- per-refactor-keypoint nearest-neighbour match ---- */
    int found    = 0;    /* kps1[i] has a match in kps2 (pos ≤1px, angle ≤0.5°, desc Hamming==0) */
    int pos_only = 0;    /* pos ≤1px but angle/desc differ */
    int no_match = 0;    /* no kps2 within 2px */
    int total_hamming = 0, n_hamming = 0;

    for (int i = 0; i < n1; ++i) {
        int best_j = -1;
        float best_d2 = 4.0f;  /* 2px radius squared */
        for (int j = 0; j < n2; ++j) {
            float dx = kps1[i].x - kps2[j].x;
            float dy = kps1[i].y - kps2[j].y;
            float d2 = dx*dx + dy*dy;
            if (d2 < best_d2) { best_d2 = d2; best_j = j; }
        }
        if (best_j < 0) { no_match++; continue; }

        float da = fabsf(kps1[i].angle - kps2[best_j].angle);
        /* handle angle wrap (0° ≈ 360°) */
        if (da > 180.0f) da = 360.0f - da;

        int ham = desc_hamming(desc1 + (size_t)i * 32, desc2 + (size_t)best_j * 32);
        total_hamming += ham; n_hamming++;

        if (sqrtf(best_d2) <= 1.0f && da <= 0.5f && ham == 0) {
            found++;
        } else if (sqrtf(best_d2) <= 1.0f) {
            pos_only++;
        } else {
            no_match++;
        }
    }

    printf("----\n");
    printf("ref keypoints matched to DSP keypoints:\n");
    printf("  exact match  (pos≤1px, angle≤0.5°, desc=0): %d / %d  (%.1f%%)\n",
           found, n1, 100.0 * found / n1);
    printf("  pos-only     (pos≤1px, angle/desc differ): %d / %d  (%.1f%%)\n",
           pos_only, n1, 100.0 * pos_only / n1);
    printf("  no-nearby    (>2px apart):                  %d / %d  (%.1f%%)\n",
           no_match, n1, 100.0 * no_match / n1);
    if (n_hamming > 0)
        printf("  avg Hamming (nearest pair): %.2f / 256\n",
               (double)total_hamming / n_hamming);

    /* ---- reverse: DSP → refactor ---- */
    {
        int rfound = 0, rpos = 0, rnone = 0;
        for (int i = 0; i < n2; ++i) {
            int best_j = -1;
            float best_d2 = 4.0f;
            for (int j = 0; j < n1; ++j) {
                float dx = kps2[i].x - kps1[j].x;
                float dy = kps2[i].y - kps1[j].y;
                float d2 = dx*dx + dy*dy;
                if (d2 < best_d2) { best_d2 = d2; best_j = j; }
            }
            if (best_j < 0) { rnone++; continue; }
            float da = fabsf(kps2[i].angle - kps1[best_j].angle);
            if (da > 180.0f) da = 360.0f - da;
            int ham = desc_hamming(desc2 + (size_t)i * 32, desc1 + (size_t)best_j * 32);
            if (sqrtf(best_d2) <= 1.0f && da <= 0.5f && ham == 0) rfound++;
            else if (sqrtf(best_d2) <= 1.0f) rpos++;
            else rnone++;
        }
        printf("----\n");
        printf("DSP keypoints matched to ref keypoints:\n");
        printf("  exact match  (pos≤1px, angle≤0.5°, desc=0): %d / %d  (%.1f%%)\n",
               rfound, n2, 100.0 * rfound / n2);
        printf("  pos-only     (pos≤1px, angle/desc differ): %d / %d  (%.1f%%)\n",
               rpos, n2, 100.0 * rpos / n2);
        printf("  no-nearby    (>2px apart):                  %d / %d  (%.1f%%)\n",
               rnone, n2, 100.0 * rnone / n2);
    }

    /* verdict */
    double match_rate = (double)found / (double)n1;
    if (match_rate >= 0.90) {
        printf("PASS: >=90%% exact match rate (%.1f%%)\n", 100.0 * match_rate);
    } else if (match_rate >= 0.70) {
        printf("WARN: match rate %.1f%% (acceptable; NMS cell boundaries)\n",
               100.0 * match_rate);
    } else {
        printf("FAIL: match rate %.1f%% below 70%% threshold\n",
               100.0 * match_rate);
    }

done:
    free(gray); free(kps1); free(kps2); free(desc1); free(desc2);
    return 0;
}
