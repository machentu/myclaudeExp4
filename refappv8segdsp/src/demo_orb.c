/*
 * ngd_app/demo_orb.c — alternative ORB extractor adapted from
 * try-slam31-RI/demo_feature_extraction.c (xsTileLib REF-based).
 *
 * Pipeline per pyramid level:
 *   bilinear resize → FAST9 + 3x3 NMS → Top-N by score (per-level) →
 *   ORB angle (arctan LUT) → rBRIEF 256-bit descriptor (sin/cos LUT rotation).
 *
 * Uses ORB-SLAM3 bit_pattern_31_ for BoW vocabulary compatibility.
 */

#include "ngd_app/demo_orb.h"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define ORB_DESC_SIZE_BYTES  32
#define ORB_DESC_SIZE_BITS   256
#define MAX_KEYPOINTS        2048

/* ORB angle LUT */
#define ORB_LUT_SIZE         64
#define ORB_LUT_SIZE1        (ORB_LUT_SIZE + 1)
#define SINCOS_LUT_SIZE      361   /* 0~360 deg, step 1 deg */
#define SINCOS_FRAC_BITS     7

/* ============================================================================
 * Pre-computed LUTs (from demo_feature_extraction.c)
 * ============================================================================ */

/* arctan(x / 64) Q0.12 format, x = 0..64 */
static const int16_t arctan_lut[ORB_LUT_SIZE1] = {
    0, 64, 128, 192, 256, 319, 383, 446, 509, 572, 635, 697, 759, 821, 882, 943,
    1003, 1063, 1123, 1182, 1241, 1299, 1356, 1413, 1470, 1525, 1581, 1635, 1689,
    1743, 1795, 1848, 1899, 1950, 2000, 2050, 2099, 2147, 2195, 2242, 2288, 2334,
    2379, 2423, 2467, 2510, 2553, 2595, 2636, 2676, 2716, 2756, 2795, 2833, 2871,
    2908, 2944, 2980, 3016, 3051, 3085, 3119, 3152, 3185, 3217
};

/* sin(angle_deg) << 7, angle_deg = 0..360 */
static const int8_t sin_lut[SINCOS_LUT_SIZE] = {
     0,  2,  4,  6,  8, 11, 13, 15, 17, 19, 22, 24, 26, 28, 30, 32,
    35, 37, 39, 41, 43, 45, 47, 49, 51, 53, 55, 57, 59, 61, 63, 65,
    67, 69, 71, 72, 74, 76, 78, 79, 81, 83, 84, 86, 88, 89, 91, 92,
    94, 95, 97, 98,100,101,102,104,105,106,107,108,109,111,112,113,
   114,115,116,116,117,118,119,120,120,121,122,122,123,123,124,124,
   125,125,125,126,126,126,126,126,126,126,127,126,126,126,126,126,
   126,126,125,125,125,124,124,123,123,122,122,121,120,120,119,118,
   117,116,116,115,114,113,112,111,109,108,107,106,105,104,102,101,
   100, 98, 97, 95, 94, 92, 91, 89, 88, 86, 84, 83, 81, 79, 78, 76,
    74, 72, 71, 69, 67, 65, 63, 61, 59, 57, 55, 53, 51, 49, 47, 45,
    43, 41, 39, 37, 35, 32, 30, 28, 26, 24, 22, 19, 17, 15, 13, 11,
     8,  6,  4,  2,  0, -3, -5, -7, -9,-12,-14,-16,-18,-20,-23,-25,
   -27,-29,-31,-33,-36,-38,-40,-42,-44,-46,-48,-50,-52,-54,-56,-58,
   -60,-62,-64,-66,-68,-70,-72,-73,-75,-77,-79,-80,-82,-84,-85,-87,
   -89,-90,-92,-93,-95,-96,-98,-99,-101,-102,-103,-105,-106,-107,-108,
  -109,-110,-112,-113,-114,-115,-116,-117,-117,-118,-119,-120,-121,
  -121,-122,-123,-123,-124,-124,-125,-125,-126,-126,-126,-127,-127,
  -127,-127,-127,-127,-127,-127,-127,-127,-127,-127,-127,-127,-127,
  -127,-126,-126,-126,-125,-125,-124,-124,-123,-123,-122,-121,-121,
  -120,-119,-118,-117,-117,-116,-115,-114,-113,-112,-110,-109,-108,
  -107,-106,-105,-103,-102,-101, -99, -98, -96, -95, -93, -92, -90,
   -89, -87, -85, -84, -82, -80, -79, -77, -75, -73, -72, -70, -68,
   -66, -64, -62, -60, -58, -56, -54, -52, -50, -48, -46, -44, -42,
   -40, -38, -36, -33, -31, -29, -27, -25, -23, -20, -18, -16, -14,
   -12,  -9,  -7,  -5,  -3
};

/* cos(angle_deg) << 7, angle_deg = 0..360 */
static const int8_t cos_lut[SINCOS_LUT_SIZE] = {
   127,126,126,126,126,126,126,126,125,125,125,124,124,123,123,122,
   122,121,120,120,119,118,117,116,116,115,114,113,112,111,109,108,
   107,106,105,104,102,101,100, 98, 97, 95, 94, 92, 91, 89, 88, 86,
    84, 83, 81, 79, 78, 76, 74, 72, 71, 69, 67, 65, 63, 61, 59, 57,
    55, 53, 51, 49, 47, 45, 43, 41, 39, 37, 35, 32, 30, 28, 26, 24,
    22, 19, 17, 15, 13, 11,  8,  6,  4,  2,  0, -3, -5, -7, -9,-12,
   -14,-16,-18,-20,-23,-25,-27,-29,-31,-33,-36,-38,-40,-42,-44,-46,
   -48,-50,-52,-54,-56,-58,-60,-62,-64,-66,-68,-70,-72,-73,-75,-77,
   -79,-80,-82,-84,-85,-87,-89,-90,-92,-93,-95,-96,-98,-99,-101,-102,
  -103,-105,-106,-107,-108,-109,-110,-112,-113,-114,-115,-116,-117,
  -117,-118,-119,-120,-121,-121,-122,-123,-123,-124,-124,-125,-125,
  -126,-126,-126,-127,-127,-127,-127,-127,-127,-127,-127,-127,-127,
  -127,-127,-127,-127,-127,-126,-126,-126,-125,-125,-124,-124,-123,
  -123,-122,-121,-121,-120,-119,-118,-117,-117,-116,-115,-114,-113,
  -112,-110,-109,-108,-107,-106,-105,-103,-102,-101, -99, -98, -96,
   -95, -93, -92, -90, -89, -87, -85, -84, -82, -80, -79, -77, -75,
   -73, -72, -70, -68, -66, -64, -62, -60, -58, -56, -54, -52, -50,
   -48, -46, -44, -42, -40, -38, -36, -33, -31, -29, -27, -25, -23,
   -20, -18, -16, -14, -12, -9, -7, -5, -3, -1,  2,  4,  6,  8, 11,
    13, 15, 17, 19, 22, 24, 26, 28, 30, 32, 35, 37, 39, 41, 43, 45,
    47, 49, 51, 53, 55, 57, 59, 61, 63, 65, 67, 69, 71, 72, 74, 76,
    78, 79, 81, 83, 84, 86, 88, 89, 91, 92, 94, 95, 97, 98,100,101,
   102,104,105,106,107,108,109,111,112,113,114,115,116,116,117,118,
   119,120,120,121,122,122,123,123,124,124,125,125,125,126,126,126,
   126,126,126,126,127
};

/* ============================================================================
 * ORB-SLAM3 BRIEF pattern (bit_pattern_31_), verbatim from refactor/src/orb.c.
 * Replaces the demo's original ORB-SLAM2 pattern for BoW vocabulary compatibility.
 * 256 pairs, each (x1,y1,x2,y2) — the two endpoints of one binary test.
 * ============================================================================ */
static const int ngd_bit_pattern_31[256][4] = {
    {8,-3, 9,5},{4,2, 7,-12},{-11,9, -8,2},{7,-12, 12,-13},{2,-13, 2,12},
    {1,-7, 1,6},{-2,-10, -2,-4},{-13,-13, -11,-8},{-13,-3, -12,-9},{10,4, 11,9},
    {-13,-8, -8,-9},{-11,7, -9,12},{7,7, 12,6},{-4,-5, -3,0},{-13,2, -12,-3},{-9,0, -7,5},
    {12,-6, 12,-1},{-3,6, -2,12},{-6,-13, -4,-8},{11,-13, 12,-8},{4,7, 5,1},{5,-3, 10,-3},
    {3,-7, 6,12},{-8,-7, -6,-2},{-2,11, -1,-10},{-13,12, -8,10},{-7,3, -5,-3},{-4,2, -3,7},
    {-10,-12, -6,11},{5,-12, 6,-7},{5,-6, 7,-1},{1,0, 4,-5},{9,11, 11,-13},{4,7, 4,12},
    {2,-1, 4,4},{-4,-12, -2,7},{-8,-5, -7,-10},{4,11, 9,12},{0,-8, 1,-13},{-13,-2, -8,2},
    {-3,-2, -2,3},{-6,9, -4,-9},{8,12, 10,7},{0,9, 1,3},{7,-5, 11,-10},{-13,-6, -11,0},
    {10,7, 12,1},{-6,-3, -6,12},{10,-9, 12,-4},{-13,8, -8,-12},{-13,0, -8,-4},{3,3, 7,8},
    {5,7, 10,-7},{-1,7, 1,-12},{3,-10, 5,6},{2,-4, 3,-10},{-13,0, -13,5},{-13,-7, -12,12},
    {-13,3, -11,8},{-7,12, -4,7},{6,-10, 12,8},{-9,-1, -7,-6},{-2,-5, 0,12},{-12,5, -7,5},
    {3,-10, 8,-13},{-7,-7, -4,5},{-3,-2, -1,-7},{2,9, 5,-11},{-11,-13, -5,-13},{-1,6, 0,-1},
    {5,-3, 5,2},{-4,-13, -4,12},{-9,-6, -9,6},{-12,-10, -8,-4},{10,2, 12,-3},{7,12, 12,12},
    {-7,-13, -6,5},{-4,9, -3,4},{7,-1, 12,2},{-7,6, -5,1},{-13,11, -12,5},{-3,7, -2,-6},
    {7,-8, 12,-7},{-13,-7, -11,-12},{1,-3, 12,12},{2,-6, 3,0},{-4,3, -2,-13},{-1,-13, 1,9},
    {7,1, 8,-6},{1,-1, 3,12},{9,1, 12,6},{-1,-9, -1,3},{-13,-13, -10,5},{7,7, 10,12},
    {12,-5, 12,9},{6,3, 7,11},{5,-13, 6,10},{2,-12, 2,3},{3,8, 4,-6},{2,6, 12,-13},
    {9,-12, 10,3},{-8,4, -7,9},{-11,12, -4,-6},{1,12, 2,-8},{6,-9, 7,-4},{2,3, 3,-2},
    {6,3, 11,0},{3,-3, 8,-8},{7,8, 9,3},{-11,-5, -6,-4},{-10,11, -5,10},{-5,-8, -3,12},
    {-10,5, -9,0},{8,-1, 12,-6},{4,-6, 6,-11},{-10,12, -8,7},{4,-2, 6,7},{-2,0, -2,12},
    {-5,-8, -5,2},{7,-6, 10,12},{-9,-13, -8,-8},{-5,-13, -5,-2},{8,-8, 9,-13},{-9,-11, -9,0},
    {1,-8, 1,-2},{7,-4, 9,1},{-2,1, -1,-4},{11,-6, 12,-11},{-12,-9, -6,4},{3,7, 7,12},
    {5,5, 10,8},{0,-4, 2,8},{-9,12, -5,-13},{0,7, 2,12},{-1,2, 1,7},{5,11, 7,-9},{3,5, 6,-8},
    {-13,-4, -8,9},{-5,9, -3,-3},{-4,-7, -3,-12},{6,5, 8,0},{-7,6, -6,12},{-13,6, -5,-2},
    {1,-10, 3,10},{4,1, 8,-4},{-2,-2, 2,-13},{2,-12, 12,12},{-2,-13, 0,-6},{4,1, 9,3},
    {-6,-10, -3,-5},{-3,-13, -1,1},{7,5, 12,-11},{4,-2, 5,-7},{-13,9, -9,-5},{7,1, 8,6},
    {7,-8, 7,6},{-7,-4, -7,1},{-8,11, -7,-8},{-13,6, -12,-8},{2,4, 3,9},{10,-5, 12,3},
    {-6,-5, -6,7},{8,-3, 9,-8},{2,-12, 2,8},{-11,-2, -10,3},{-12,-13, -7,-9},{-11,0, -10,-5},
    {5,-3, 11,8},{-2,-13, -1,12},{-1,-8, 0,9},{-13,-11, -12,-5},{-10,-2, -10,11},{-3,9, -2,-13},
    {2,-3, 3,2},{-9,-13, -4,0},{-4,6, -3,-10},{-4,12, -2,-7},{-6,-11, -4,9},{6,-3, 6,11},
    {-13,11, -5,5},{11,11, 12,6},{7,-5, 12,-2},{-1,12, 0,7},{-4,-8, -3,-2},{-7,1, -6,7},
    {-13,-12, -8,-13},{-7,-2, -6,-8},{-8,5, -6,-9},{-5,-1, -4,5},{-13,7, -8,10},{1,5, 5,-13},
    {1,0, 10,-13},{9,12, 10,-1},{5,-8, 10,-9},{-1,11, 1,-13},{-9,-3, -6,2},{-1,-10, 1,12},
    {-13,1, -8,-10},{8,-11, 10,-6},{2,-13, 3,-6},{7,-13, 12,-9},{-10,-10, -5,-7},{-10,-8, -8,-13},
    {4,-6, 8,5},{3,12, 8,-13},{-4,2, -3,-3},{5,-13, 10,-12},{4,-13, 5,-1},{-9,9, -4,3},
    {0,3, 3,-9},{-12,1, -6,1},{3,2, 4,-8},{-10,-10, -10,9},{8,-13, 12,12},{-8,-12, -6,-5},
    {2,2, 3,7},{10,6, 11,-8},{6,8, 8,-12},{-7,10, -6,5},{-3,-9, -3,9},{-1,-13, -1,5},
    {-3,-7, -3,4},{-8,-2, -8,3},{4,2, 12,12},{2,-5, 3,11},{6,-9, 11,-13},{3,-1, 7,12},
    {11,-1, 12,4},{-3,0, -3,6},{4,-11, 4,12},{2,-4, 2,1},{-10,-6, -8,1},{-13,7, -11,1},
    {-13,12, -11,-13},{6,0, 11,-13},{0,-1, 1,4},{-13,3, -9,-2},{-9,8, -6,-3},{-13,-6, -8,-2},
    {5,-9, 8,10},{2,7, 3,-9},{-1,-6, -1,-1},{9,5, 11,-2},{11,-3, 12,-8},{3,0, 3,5},
    {-1,4, 0,10},{3,-6, 4,5},{-13,0, -10,5},{5,8, 12,11},{8,9, 9,-6},{7,-4, 8,-12},
    {-10,4, -10,9},{7,3, 12,4},{9,-7, 10,-2},{7,0, 12,-2},{-1,-6, 0,-11}
};

/* ============================================================================
 * Helper macros
 * ============================================================================ */

#define ABS(a)    ((a) > 0 ? (a) : -(a))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define CLAMP(v, lo, hi) ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))

/* ============================================================================
 * Static working buffers (reused across calls; NOT thread-safe)
 * ============================================================================ */

static uint16_t tmp_x[MAX_KEYPOINTS * 2];
static uint16_t tmp_y[MAX_KEYPOINTS * 2];
static uint8_t  tmp_score[MAX_KEYPOINTS * 2];

/* ============================================================================
 * Stage 1: FAST9 corner detection + 3x3 NMS
 * Adapted from demo_feature_extraction.c:fast9_detect (lines 330-446)
 * ============================================================================ */

/*
 * FAST9 detection on a 16-pixel Bresenham circle (r=3).
 * Finds points where >=9 consecutive circle pixels are all brighter or all
 * darker than the center by at least `threshold`.
 *
 * Circle pixel offsets relative to centre:
 *   idx 0: ( 0, 3)   idx 4: ( 3, 0)   idx 8:  ( 0,-3)   idx 12: (-3, 0)
 *   idx 1: ( 1, 3)   idx 5: ( 3,-1)   idx 9:  (-1,-3)   idx 13: (-3, 1)
 *   idx 2: ( 2, 2)   idx 6: ( 3,-2)   idx 10: (-2,-2)   idx 14: (-3, 2)
 *   idx 3: ( 3, 1)   idx 7: ( 2,-3)   idx 11: (-3,-1)   idx 15: (-3, 3)
 *
 * Quick-reject: check 4 orthogonal directions (idx 0,4,8,12) first.
 * If <3 pass, skip the full 16-pixel check.
 */
static int fast9_detect(const uint8_t *img, int w, int h, int stride,
                        uint8_t threshold,
                        uint16_t *out_x, uint16_t *out_y, uint8_t *out_score,
                        int max_corners)
{
    static const int8_t circle_dx[16] = { 0, 1, 2, 3, 3, 3, 2, 1,
                                           0,-1,-2,-3,-3,-3,-2,-1 };
    static const int8_t circle_dy[16] = { 3, 3, 2, 1, 0,-1,-2,-3,
                                          -3,-3,-2,-1, 0, 1, 2, 3 };

    int count = 0;
    int margin = 3;

    for (int y = margin; y < h - margin; y++) {
        for (int x = margin; x < w - margin; x++) {
            uint8_t center = img[y * stride + x];
            uint8_t thr_lo = (center > threshold) ? center - threshold : 0;
            uint8_t thr_hi = (center < 255 - threshold) ? center + threshold : 255;

            /* Step 1: quick-reject — check 4 orthogonal directions */
            int b_h = 0, b_l = 0;
            for (int i = 0; i < 16; i += 4) {
                uint8_t p = img[(y + circle_dy[i]) * stride + (x + circle_dx[i])];
                if (p > thr_hi) b_h++;
                if (p < thr_lo) b_l++;
            }
            if (b_h < 3 && b_l < 3) continue;

            /* Step 2: full 16-pixel circle check with bit flags */
            uint16_t brighter = 0, darker = 0;
            for (int i = 0; i < 16; i++) {
                uint8_t p = img[(y + circle_dy[i]) * stride + (x + circle_dx[i])];
                if (p > thr_hi) brighter |= (1 << i);
                if (p < thr_lo) darker   |= (1 << i);
            }

            /* Detect 9 consecutive brighter or darker */
            int is_corner = 0;
            uint16_t pattern;
            if (brighter) {
                pattern = brighter | (brighter << 16);
                for (int i = 0; i < 16; i++) {
                    if ((pattern & (0x1FF << i)) == (0x1FF << i)) {
                        is_corner = 1; break;
                    }
                }
            }
            if (!is_corner && darker) {
                pattern = darker | (darker << 16);
                for (int i = 0; i < 16; i++) {
                    if ((pattern & (0x1FF << i)) == (0x1FF << i)) {
                        is_corner = 1; break;
                    }
                }
            }
            if (!is_corner) continue;

            /* Step 3: compute FAST score (sum of absolute differences) */
            int score = 0;
            for (int i = 0; i < 16; i++) {
                uint8_t p = img[(y + circle_dy[i]) * stride + (x + circle_dx[i])];
                int diff = (int)p - (int)center;
                if (diff > 0) score += diff;
                else          score -= diff;
            }
            if (score > 255) score = 255;

            if (count < max_corners) {
                out_x[count] = (uint16_t)x;
                out_y[count] = (uint16_t)y;
                out_score[count] = (uint8_t)score;
                count++;
            }
        }
    }

    /* Step 4: 3x3 non-maximum suppression */
    uint8_t *score_map = (uint8_t *)calloc((size_t)(w * h), 1);
    if (!score_map) return 0;

    for (int i = 0; i < count; i++) {
        score_map[out_y[i] * w + out_x[i]] = out_score[i];
    }

    int nms_count = 0;
    for (int i = 0; i < count; i++) {
        int cx = out_x[i], cy = out_y[i];
        uint8_t sc = out_score[i];
        int is_max = 1;

        for (int dy = -1; dy <= 1 && is_max; dy++) {
            for (int dx = -1; dx <= 1 && is_max; dx++) {
                if (dx == 0 && dy == 0) continue;
                if (score_map[(cy + dy) * w + (cx + dx)] > sc) {
                    is_max = 0;
                }
            }
        }

        if (is_max) {
            out_x[nms_count] = out_x[i];
            out_y[nms_count] = out_y[i];
            out_score[nms_count] = out_score[i];
            nms_count++;
        }
    }

    free(score_map);
    return nms_count;
}

/* ============================================================================
 * Stage 2: Grid-based spatial distribution by score.
 * Sorts corners by score descending, then distributes them into a grid of
 * cells (≈35px each) so each cell gets at most ceil(N / nCells) corners.
 * This prevents all features from clustering in high-contrast regions while
 * still picking the highest-scoring corners per region.
 * ============================================================================ */

#define GRID_CELL_SIZE 35

typedef struct {
    uint16_t x, y;
    uint8_t  score;
} corner_entry;

static int cmp_score_desc(const void *a, const void *b)
{
    const corner_entry *ca = (const corner_entry *)a;
    const corner_entry *cb = (const corner_entry *)b;
    if (ca->score > cb->score) return -1;
    if (ca->score < cb->score) return 1;
    return 0;
}

static int distribute_grid(uint16_t *x, uint16_t *y, uint8_t *score,
                           uint32_t M, uint32_t N, int img_w, int img_h)
{
    if (M <= N) return (int)M;

    /* Sort all corners by score descending */
    corner_entry *entries = (corner_entry *)malloc(sizeof(corner_entry) * M);
    uint8_t *taken = (uint8_t *)calloc(M, 1);
    if (!entries || !taken) { free(entries); free(taken); return (int)M; }
    for (uint32_t i = 0; i < M; i++) {
        entries[i].x = x[i]; entries[i].y = y[i]; entries[i].score = score[i];
    }
    qsort(entries, M, sizeof(corner_entry), cmp_score_desc);

    /* Grid layout: ≈35px cells */
    int nCols = img_w / GRID_CELL_SIZE;
    int nRows = img_h / GRID_CELL_SIZE;
    if (nCols < 1) nCols = 1; if (nRows < 1) nRows = 1;
    int nCells = nCols * nRows;
    int perCell = (int)(N / (uint32_t)nCells);
    if (perCell < 1) perCell = 1;

    int *cellCnt = (int *)calloc((size_t)nCells, sizeof(int));
    uint32_t out_cnt = 0;

    /* Pass 1: respect perCell cap for spatial diversity */
    for (uint32_t i = 0; i < M && out_cnt < N; i++) {
        int cx = (int)entries[i].x * nCols / img_w;
        int cy = (int)entries[i].y * nRows / img_h;
        int cell = cy * nCols + cx;
        if (cell < 0) cell = 0;
        if (cell >= nCells) cell = nCells - 1;
        if (cellCnt[cell] >= perCell) continue;
        x[out_cnt] = entries[i].x; y[out_cnt] = entries[i].y;
        score[out_cnt] = entries[i].score;
        cellCnt[cell]++; taken[i] = 1; out_cnt++;
    }
    /* Pass 2: fill remaining from any cell, highest scores first */
    for (uint32_t i = 0; i < M && out_cnt < N; i++) {
        if (taken[i]) continue;
        x[out_cnt] = entries[i].x; y[out_cnt] = entries[i].y;
        score[out_cnt] = entries[i].score;
        out_cnt++;
    }

    free(cellCnt); free(taken); free(entries);
    return (int)out_cnt;
}

/* ============================================================================
 * Stage 3: ORB orientation (intensity centroid with arctan LUT)
 * Adapted from demo_feature_extraction.c:compute_orb_angle (lines 498-542)
 * ============================================================================ */

static uint8_t compute_orb_angle(const uint8_t *img, int w, int h, int stride,
                                  int cx, int cy, int patch_radius)
{
    int m01 = 0, m10 = 0;

    for (int dy = -patch_radius; dy <= patch_radius; dy++) {
        int py = cy + dy;
        if (py < 0) py = 0; if (py >= h) py = h - 1;  /* clamp to image bounds */
        for (int dx = -patch_radius; dx <= patch_radius; dx++) {
            if (dx * dx + dy * dy <= patch_radius * patch_radius) {
                int px = cx + dx;
                if (px < 0) px = 0; if (px >= w) px = w - 1;
                uint8_t p = img[py * stride + px];
                m10 += dx * p;
                m01 += dy * p;
            }
        }
    }

    if (m10 == 0 && m01 == 0) return 0;

    int32_t abs_m01 = ABS(m01);
    int32_t abs_m10 = ABS(m10);
    int32_t angle_q12;

    if (abs_m10 >= abs_m01) {
        int32_t idx = (abs_m01 * ORB_LUT_SIZE) / abs_m10;
        if (idx > ORB_LUT_SIZE) idx = ORB_LUT_SIZE;
        angle_q12 = arctan_lut[idx];
    } else {
        int32_t idx = (abs_m10 * ORB_LUT_SIZE) / abs_m01;
        if (idx > ORB_LUT_SIZE) idx = ORB_LUT_SIZE;
        angle_q12 = 6434 - arctan_lut[idx]; /* pi/2 in Q12 = 6434 */
    }

    /* Quadrant adjustment */
    if (m10 < 0 && m01 >= 0)      angle_q12 = 12868 - angle_q12;      /* Q2 */
    else if (m10 < 0 && m01 < 0)  angle_q12 = 12868 + angle_q12;      /* Q3 */
    else if (m10 >= 0 && m01 < 0) angle_q12 = 25736 - angle_q12;      /* Q4 */

    uint32_t angle_q8 = ((uint32_t)angle_q12 >> 4) & 0xFF;
    return (uint8_t)angle_q8;
}

/* ============================================================================
 * Stage 4: rBRIEF descriptor (256-bit, rotated, with pixel clamping)
 * Adapted from demo_feature_extraction.c:compute_orb_descriptor (lines 558-587)
 * Uses ORB-SLAM3 bit_pattern_31_ for vocabulary compatibility.
 *
 * Descriptor is computed on a GAUSSIAN-BLURRED copy of the level image
 * (7×7 kernel, σ=2.0, same as ORB-SLAM3). Orientation uses the unblurred image.
 * ============================================================================ */

/* Gaussian 7×7 kernel, σ=2.0, weights sum to 256 (fixed-point 8.8).
 * Copied from refactor/src/orb.c:83 */
static const uint16_t gauss7[7] = { 18, 34, 48, 56, 48, 34, 18 };

/* BORDER_REFLECT_101 for Gaussian blur edge handling */
static inline int reflect101(int j, int len)
{
    if (j < 0) j = -j;
    if (j >= len) j = 2 * (len - 1) - j;
    return j;
}

/* Apply 7×7 Gaussian blur (σ=2.0, separable) to a level image.
 * src/dst may be the same buffer (in-place). */
static void gaussian_blur_level(uint8_t *img, int w, int h)
{
    /* Horizontal pass → tmp (uint16_t to avoid overflow) */
    uint16_t *tmp = (uint16_t *)malloc((size_t)w * h * sizeof(uint16_t));
    if (!tmp) return;  /* fallback: no blur */
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint32_t sum = 0;
            for (int k = -3; k <= 3; k++)
                sum += (uint32_t)gauss7[k + 3] * (uint32_t)img[y * w + reflect101(x + k, w)];
            tmp[y * w + x] = (uint16_t)(sum >> 8);  /* weights sum to 256 */
        }
    }
    /* Vertical pass → back to img (in-place) */
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint32_t sum = 0;
            for (int k = -3; k <= 3; k++)
                sum += (uint32_t)gauss7[k + 3] * (uint32_t)tmp[reflect101(y + k, h) * w + x];
            uint32_t v = (sum + (1u << 15)) >> 16;  /* round and scale back */
            img[y * w + x] = (uint8_t)(v > 255 ? 255 : v);
        }
    }
    free(tmp);
}

static inline void rotate_point(int x, int y, uint8_t angle_q8,
                                int *rx, int *ry)
{
    /* Convert angle_q8 (0-255) to degrees, then look up sin/cos */
    int deg = (int)(angle_q8 * 360.0f / 256.0f + 0.5f);
    if (deg < 0) deg = 0;
    if (deg >= 360) deg = 359;  /* clamp; 360 maps to 0 via LUT size */
    int a = cos_lut[deg];
    int b = sin_lut[deg];
    *rx = (x * a - y * b + (1 << (SINCOS_FRAC_BITS - 1))) >> SINCOS_FRAC_BITS;
    *ry = (x * b + y * a + (1 << (SINCOS_FRAC_BITS - 1))) >> SINCOS_FRAC_BITS;
}

/* Descriptor rotation uses float sin/cos (same precision as ORB-SLAM3)
 * instead of quantized int8 LUT, to avoid descriptor bit-flip when the
 * quantized angle_q8 changes by ±1 between frames. */
static void compute_orb_descriptor(const uint8_t *img, int w, int h, int stride,
                                    int cx, int cy, uint8_t angle_q8,
                                    uint8_t *desc)
{
    memset(desc, 0, ORB_DESC_SIZE_BYTES);

    /* Convert quantized angle to continuous radians for rotation */
    float angle_rad = (float)angle_q8 * (2.0f * 3.14159265f / 256.0f);
    float c = cosf(angle_rad), s = sinf(angle_rad);

    for (int i = 0; i < ORB_DESC_SIZE_BITS; i++) {
        int x1 = ngd_bit_pattern_31[i][0];
        int y1 = ngd_bit_pattern_31[i][1];
        int x2 = ngd_bit_pattern_31[i][2];
        int y2 = ngd_bit_pattern_31[i][3];

        int rx1, ry1, rx2, ry2;
        if (angle_q8 != 0) {
            /* Row = x*sin + y*cos, Col = x*cos - y*sin (ORB-SLAM3 convention) */
            rx1 = (int)lroundf(x1 * c - y1 * s);  /* col */
            ry1 = (int)lroundf(x1 * s + y1 * c);  /* row */
            rx2 = (int)lroundf(x2 * c - y2 * s);
            ry2 = (int)lroundf(x2 * s + y2 * c);
        } else {
            rx1 = x1; ry1 = y1;
            rx2 = x2; ry2 = y2;
        }

        /* Clamp pixel access to image bounds */
        int px1 = cx + rx1, py1 = cy + ry1;
        int px2 = cx + rx2, py2 = cy + ry2;
        px1 = CLAMP(px1, 0, w - 1); py1 = CLAMP(py1, 0, h - 1);
        px2 = CLAMP(px2, 0, w - 1); py2 = CLAMP(py2, 0, h - 1);

        uint8_t p1 = img[py1 * stride + px1];
        uint8_t p2 = img[py2 * stride + px2];

        if (p1 < p2) {
            desc[i >> 3] |= (1 << (i & 7));
        }
    }
}

/* ============================================================================
 * Bilinear resize (simplified, cascaded pyramid downscale)
 * ============================================================================ */

static void bilinear_resize(const uint8_t *src, int sw, int sh,
                             uint8_t *dst, int dw, int dh)
{
    float scale_x = (float)sw / (float)dw;
    float scale_y = (float)sh / (float)dh;

    for (int dy = 0; dy < dh; dy++) {
        float fy = ((float)dy + 0.5f) * scale_y - 0.5f;
        int sy = (int)floorf(fy);
        float fy_frac = fy - (float)sy;
        if (sy < 0) { sy = 0; fy_frac = 0.0f; }
        if (sy >= sh - 1) { sy = sh - 1; fy_frac = 0.0f; }
        int sy1 = (sy + 1 < sh) ? sy + 1 : sh - 1;
        float w0 = 1.0f - fy_frac, w1 = fy_frac;

        for (int dx = 0; dx < dw; dx++) {
            float fx = ((float)dx + 0.5f) * scale_x - 0.5f;
            int sx = (int)floorf(fx);
            float fx_frac = fx - (float)sx;
            if (sx < 0) { sx = 0; fx_frac = 0.0f; }
            if (sx >= sw - 1) { sx = sw - 1; fx_frac = 0.0f; }
            int sx1 = (sx + 1 < sw) ? sx + 1 : sw - 1;
            float h0 = 1.0f - fx_frac, h1 = fx_frac;

            float v = w0 * (h0 * (float)src[sy  * sw + sx ] + h1 * (float)src[sy  * sw + sx1])
                    + w1 * (h0 * (float)src[sy1 * sw + sx ] + h1 * (float)src[sy1 * sw + sx1]);
            int iv = (int)(v + 0.5f);
            dst[dy * dw + dx] = (uint8_t)CLAMP(iv, 0, 255);
        }
    }
}

/* ============================================================================
 * Public interface: demo_orb_extract() — multi-scale pyramid
 * ============================================================================ */

int demo_orb_extract(const ngd_orb_extractor *ex,
                     const uint8_t *gray, int w, int h, int stride,
                     ngd_keypoint *out_kps, int max_kps,
                     uint8_t *out_desc)
{
    if (!ex || !gray || !out_kps || !out_desc) return -1;
    if (ex->nfeatures <= 0 || w <= 0 || h <= 0) return 0;

    const int nlevels = ex->nlevels;
    const int max_raw = MAX_KEYPOINTS * 2;
    int total = 0;

    /* ---- Build and process pyramid one level at a time ---- */
    /* Cascaded resize: each level is resized from the PREVIOUS level's image.
     * We keep prev_buf alive until the next level finishes its resize. */
    uint8_t *prev_buf = NULL;   /* free'd after NEXT level resizes from it */
    int      prev_w = w, prev_h = h;
    const uint8_t *prev_src = gray;  /* source for resize (level 1 reads from gray) */

    for (int level = 0; level < nlevels; level++) {
        int n_desired = ex->mnFeaturesPerLevel[level];
        if (n_desired <= 0) continue;

        /* Compute level dimensions, image pointer, and stride */
        int lw, lh, lstride;
        const uint8_t *limg;
        uint8_t *cur_buf = NULL;
        if (level == 0) {
            lw = w; lh = h;
            lstride = stride;   /* use caller's stride for level 0 (may differ from width) */
            limg = gray;
        } else {
            lw = (int)((float)w * ex->mvInvScaleFactor[level] + 0.5f);
            lh = (int)((float)h * ex->mvInvScaleFactor[level] + 0.5f);
            if (lw < 7 || lh < 7) break;

            cur_buf = (uint8_t *)malloc((size_t)lw * lh);
            if (!cur_buf) break;
            bilinear_resize(prev_src, prev_w, prev_h, cur_buf, lw, lh);
            limg = cur_buf;
            lstride = lw;        /* malloc'd buffer: stride == width */

            /* Free the previous level's buffer — it's no longer needed */
            free(prev_buf);
            prev_buf = cur_buf;
            prev_src = cur_buf;
            prev_w = lw; prev_h = lh;
        }

        /* ---- FAST9 detection + NMS (two-pass threshold) ---- */
        int n_raw = fast9_detect(limg, lw, lh, lstride, (uint8_t)ex->iniThFAST,
                                 tmp_x, tmp_y, tmp_score, max_raw);
        if (n_raw == 0 && ex->minThFAST < ex->iniThFAST) {
            n_raw = fast9_detect(limg, lw, lh, lstride, (uint8_t)ex->minThFAST,
                                 tmp_x, tmp_y, tmp_score, max_raw);
        }
        if (n_raw <= 0) continue;

        /* ---- Per-level grid distribution by score ---- */
        int n_kps;
        if ((uint32_t)n_raw <= (uint32_t)n_desired) {
            n_kps = n_raw;
        } else {
            n_kps = distribute_grid(tmp_x, tmp_y, tmp_score, (uint32_t)n_raw, (uint32_t)n_desired, lw, lh);
        }
        if (n_kps <= 0) continue;

        /* ---- ORB angle (unblurred) then blur then descriptor ---- */
        float level_scale = ex->mvScaleFactor[level];
        int patch_sz = (int)((float)NGD_ORB_PATCH_SIZE * level_scale + 0.5f);

        /* Step A: compute orientation on unblurred image, store in tmp_score */
        for (int k = 0; k < n_kps; k++) {
            int cx = (int)tmp_x[k], cy = (int)tmp_y[k];
            tmp_score[k] = compute_orb_angle(limg, lw, lh, lstride, cx, cy, 15);
        }

        /* Step B: blur a copy of the level image for descriptors */
        uint8_t *blurred = (uint8_t *)malloc((size_t)lw * lh);
        if (blurred) {
            memcpy(blurred, limg, (size_t)lw * lh);
            gaussian_blur_level(blurred, lw, lh);
        } else {
            blurred = (uint8_t *)limg;  /* fallback: use unblurred */
        }

        /* Step C: descriptors on blurred image + fill output keypoints */
        for (int k = 0; k < n_kps; k++) {
            int cx = (int)tmp_x[k];
            int cy = (int)tmp_y[k];
            uint8_t angle_q8 = tmp_score[k];

            compute_orb_descriptor(blurred, lw, lh, lw, cx, cy, angle_q8,
                                   out_desc + (size_t)total * 32);

            out_kps[total].x        = (float)cx * level_scale;
            out_kps[total].y        = (float)cy * level_scale;
            out_kps[total].angle    = (float)angle_q8 * (360.0f / 256.0f);
            out_kps[total].response = 0.0f;   /* response not used after extraction */
            out_kps[total].octave   = level;
            out_kps[total].size     = (float)patch_sz;

            total++;
            if (total >= max_kps) {
                if (blurred != limg) free(blurred);
                free(prev_buf);
                return total;
            }
        }
        if (blurred != (uint8_t *)limg) free(blurred);
    }

    /* Free the last level's buffer */
    free(prev_buf);
    return total;
}
