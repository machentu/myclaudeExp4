/*
 * ic_angle_core.c - portable ORB orientation (IC_Angle) kernel (shared PC + VP6).
 *
 * The arithmetic is copied 1:1 from refactor/src/orb.c::ngd_ic_angle /
 * ngd_pix_clamp / ngd_orb_init (umax) so the result is bit-exact with the
 * refactor C reference. The only structural change is that the kernel is
 * tile-aware (origin_x/origin_y + frame_w/h), so the VP6 driver can run it on
 * a per-keypoint SRAM source patch.
 *
 * Build: plain C on PC, or as C++ under the Xtensa cstub (see CMakeLists.txt).
 */
#include "ic_angle_core.h"

#include <math.h>

#ifndef IC_PI
#define IC_PI 3.14159265358979323846
#endif
#define IC_DBL_EPSILON 2.2204460492503131e-16

/* Bit-exact port of cv::fastAtan2 (atan_f32, 7th-order polynomial).
 * Copied verbatim from refactor/src/orb.c::ngd_fast_atan2.
 * Returns degrees in [0,360), matching cv::fastAtan2. */
static const float ic_atan2_p1 = 0.9997878412794807f*(float)(180.0/IC_PI);
static const float ic_atan2_p3 = -0.3258083974640975f*(float)(180.0/IC_PI);
static const float ic_atan2_p5 =  0.1555786518463281f*(float)(180.0/IC_PI);
static const float ic_atan2_p7 = -0.04432655554792128f*(float)(180.0/IC_PI);
static float ic_fast_atan2(float y, float x)
{
    float ax = fabsf(x), ay = fabsf(y);
    float a, c, c2;
    if (ax >= ay) {
        c = ay/(ax + (float)IC_DBL_EPSILON);
        c2 = c*c;
        a = (((ic_atan2_p7*c2 + ic_atan2_p5)*c2 + ic_atan2_p3)*c2 + ic_atan2_p1)*c;
    } else {
        c = ax/(ay + (float)IC_DBL_EPSILON);
        c2 = c*c;
        a = 90.f - (((ic_atan2_p7*c2 + ic_atan2_p5)*c2 + ic_atan2_p3)*c2 + ic_atan2_p1)*c;
    }
    if (x < 0) a = 180.f - a;
    if (y < 0) a = 360.f - a;
    return a;
}

/* 1:1 from ngd_orb_init's umax computation (ngd_cvround == (int)lroundf). */
void ic_angle_init_umax(int umax[IC_ANGLE_UMAX_SIZE])
{
    const int half = IC_ANGLE_HALF_PATCH;
    int vmax = (int)floorf(half * sqrtf(2.0f) / 2.0f + 1.0f);
    int vmin = (int)ceilf(half * sqrtf(2.0f) / 2.0f);
    double hp2 = (double)half * half;
    for (int v = 0; v <= vmax; ++v)
        umax[v] = (int)lroundf((float)sqrt(hp2 - (double)v * v));
    for (int v = half, v0 = 0; v >= vmin; --v) {
        while (umax[v0] == umax[v0 + 1]) ++v0;
        umax[v] = v0;
        ++v0;
    }
}

/* 1:1 with ngd_pix_clamp, but indexes a tile source region (origin_x/origin_y)
 * and clamps against the full frame (frame_w/h). ngd_pix_clamp clamps the coord
 * to [0,w-1]x[0,h-1] then reads img[y*w+x]; here the clamped coord is in the
 * source region (the patch +/-15 clamped to the frame), so the index is valid. */
static inline uint8_t ic_pix_clamp(const uint8_t *src, int pitch,
                                   int origin_x, int origin_y,
                                   int frame_w, int frame_h,
                                   int x, int y)
{
    if (x < 0) x = 0; else if (x >= frame_w) x = frame_w - 1;
    if (y < 0) y = 0; else if (y >= frame_h) y = frame_h - 1;
    return src[(y - origin_y) * pitch + (x - origin_x)];
}

/* 1:1 with ngd_ic_angle, but scores via the tile-aware ic_pix_clamp. */
float ic_angle_kernel(const uint8_t *src_data, int src_pitch,
                      int origin_x, int origin_y,
                      int frame_w, int frame_h,
                      int cx, int cy, const int *umax)
{
    int m_01 = 0, m_10 = 0;

    /* center row, v=0 */
    for (int u = -IC_ANGLE_HALF_PATCH; u <= IC_ANGLE_HALF_PATCH; ++u)
        m_10 += u * (int)ic_pix_clamp(src_data, src_pitch, origin_x, origin_y,
                                      frame_w, frame_h, cx + u, cy);

    for (int v = 1; v <= IC_ANGLE_HALF_PATCH; ++v) {
        int v_sum = 0;
        int d = umax[v];
        for (int u = -d; u <= d; ++u) {
            int val_plus  = (int)ic_pix_clamp(src_data, src_pitch, origin_x, origin_y,
                                              frame_w, frame_h, cx + u, cy + v);
            int val_minus = (int)ic_pix_clamp(src_data, src_pitch, origin_x, origin_y,
                                              frame_w, frame_h, cx + u, cy - v);
            v_sum += (val_plus - val_minus);
            m_10 += u * (val_plus + val_minus);
        }
        m_01 += v * v_sum;
    }
    return ic_fast_atan2((float)m_01, (float)m_10);   /* degrees [0,360), == cv::fastAtan2 */
}

void ic_angle_patch_bbox(int cx, int cy, int frame_w, int frame_h,
                         int *min_x, int *min_y, int *max_x, int *max_y)
{
    int mnx = cx - IC_ANGLE_MARGIN;
    int mny = cy - IC_ANGLE_MARGIN;
    int mxx = cx + IC_ANGLE_MARGIN;   /* cx + 15 */
    int mxy = cy + IC_ANGLE_MARGIN;
    if (mnx < 0) mnx = 0;
    if (mny < 0) mny = 0;
    if (mxx > frame_w - 1) mxx = frame_w - 1;
    if (mxy > frame_h - 1) mxy = frame_h - 1;
    *min_x = mnx; *min_y = mny; *max_x = mxx; *max_y = mxy;
}

float ic_angle_full(const uint8_t *img, int w, int h, int stride,
                    int cx, int cy, const int *umax)
{
    /* origin 0,0, source == full image: the kernel reads img[] directly, exactly
     * ngd_ic_angle. */
    return ic_angle_kernel(img, stride, 0, 0, w, h, cx, cy, umax);
}
