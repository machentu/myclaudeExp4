/* ngd_app/image_proc.c — pure-C image processing (zero-OpenCV).
 * Replaces: cv::resize(INTER_NEAREST), cv::threshold(THRESH_BINARY),
 *           cv::copyMakeBorder(BORDER_CONSTANT), cv::countNonZero,
 *           cv::dnn::blobFromImage.
 * For dilate, reuses ngd_mask_dilate from refactor (ngd/mask.h). */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

//tzg
#define UNUSED(x) (void)(x)


/* ---- nearest-neighbour resize (uint8) ---- */
void ngd_resize_nearest_u8(const uint8_t *src, int sw, int sh, int sstride,
                           uint8_t *dst, int dw, int dh)
{
    const float sx = (float)sw / (float)dw;
    const float sy = (float)sh / (float)dh;
    for (int y = 0; y < dh; ++y) {
        int syi = (int)((float)y * sy);
        if (syi >= sh) syi = sh - 1;
        const uint8_t *srow = src + (size_t)syi * sstride;
        uint8_t       *drow = dst + (size_t)y * dw;
        for (int x = 0; x < dw; ++x) {
            int sxi = (int)((float)x * sx);
            if (sxi >= sw) sxi = sw - 1;
            drow[x] = srow[sxi];
        }
    }
}

/* ---- nearest-neighbour resize (float) ---- */
void ngd_resize_nearest_f32(const float *src, int sw, int sh, int sstride,
                            float *dst, int dw, int dh)
{
    const float sx = (float)sw / (float)dw;
    const float sy = (float)sh / (float)dh;
    for (int y = 0; y < dh; ++y) {
        int syi = (int)((float)y * sy);
        if (syi >= sh) syi = sh - 1;
        const float *srow = src + (size_t)syi * sstride;
        float       *drow = dst + (size_t)y * dw;
        for (int x = 0; x < dw; ++x) {
            int sxi = (int)((float)x * sx);
            if (sxi >= sw) sxi = sw - 1;
            drow[x] = srow[sxi];
        }
    }
}

/* ---- binary threshold (uint8): dst[i] = (src[i] > thresh) ? maxval : 0 ---- */
void ngd_threshold_binary_u8(const uint8_t *src, uint8_t *dst, int n,
                             uint8_t thresh, uint8_t maxval)
{
    for (int i = 0; i < n; ++i)
        dst[i] = (src[i] > thresh) ? maxval : 0;
}

/* ---- count non-zero (uint8) ---- */
int ngd_count_nonzero_u8(const uint8_t *src, int n)
{
    int c = 0;
    for (int i = 0; i < n; ++i) if (src[i]) ++c;
    return c;
}

/* ---- copyMakeBorder with constant fill (uint8) ----
 * dst size = (sh + top + bottom) x (sw + left + right), dst stride = sw + left + right.
 * Fills border with `fill`, copies src centred. */
void ngd_copy_make_border_u8(const uint8_t *src, int sw, int sh, int sstride,
                             uint8_t *dst, int top, int bottom, int left, int right,
                             uint8_t fill)
{
    const int dw = sw + left + right;
    const int dh = sh + top + bottom;
    /* fill top border */
    for (int y = 0; y < top; ++y)
        memset(dst + (size_t)y * dw, fill, (size_t)dw);
    /* fill middle (src + left/right borders) */
    for (int y = 0; y < sh; ++y) {
        uint8_t *drow = dst + (size_t)(y + top) * dw;
        memset(drow, fill, (size_t)left);
        memcpy(drow + left, src + (size_t)y * sstride, (size_t)sw);
        memset(drow + left + sw, fill, (size_t)right);
    }
    /* fill bottom border */
    for (int y = top + sh; y < dh; ++y)
        memset(dst + (size_t)y * dw, fill, (size_t)dw);
}

/* ---- letterbox: resize + constant-border pad ----
 * Equivalent to cv::resize + cv::copyMakeBorder with BORDER_CONSTANT.
 * newShape = (netW, netH); colour = fill value (114 = YOLO grey).
 * out_params[4] = { ratio_x, ratio_y, left, top }. */
void ngd_letterbox_u8(const uint8_t *src, int sw, int sh, int sstride,
                      uint8_t *dst, int netW, int netH,
                      float *out_params, uint8_t fill)
{
    /* compute ratio and new unpadded size */
    float r = (float)netW / (float)sw;
    float rh = (float)netH / (float)sh;
    if (rh < r) r = rh;
    float ratio[2] = { r, r };
    int new_w = (int)lroundf((float)sw * r);
    int new_h = (int)lroundf((float)sh * r);

    float dw = (float)(netW - new_w);
    float dh = (float)(netH - new_h);
    dw /= 2.0f; dh /= 2.0f;

    int top   = (int)lroundf(dh - 0.1f);
    int left  = (int)lroundf(dw - 0.1f);
    /* OpenCV copyMakeBorder computes bottom/right internally; we just need padding
     * so that the resulting image is netW x netH. The actual padding amounts are:
     * top = round(dh - 0.1), bottom = round(dh + 0.1), left = round(dw - 0.1), right = round(dw + 0.1). */
    int bottom = (int)lroundf(dh + 0.1f);
    int right  = (int)lroundf(dw + 0.1f);

    out_params[0] = ratio[0];
    out_params[1] = ratio[1];
    out_params[2] = (float)left;
    out_params[3] = (float)top;

    /* resize to (new_w, new_h) into a temp buffer (3-channel BGR) */
    uint8_t *tmp = (uint8_t *)malloc((size_t)new_w * new_h * 3);
    if (!tmp) return;
    if (new_w != sw || new_h != sh) {
        /* nearest-neighbour resize, 3-channel. Treat each channel identically:
         * source pixel at (sxi, syi) maps to all 3 channels. */
        const float sx = (float)sw / (float)new_w;
        const float sy = (float)sh / (float)new_h;
        for (int y = 0; y < new_h; ++y) {
            int syi = (int)((float)y * sy);
            if (syi >= sh) syi = sh - 1;
            const uint8_t *srow = src + (size_t)syi * sstride;
            uint8_t *drow = tmp + (size_t)y * (3 * new_w);
            for (int x = 0; x < new_w; ++x) {
                int sxi = (int)((float)x * sx);
                if (sxi >= sw) sxi = sw - 1;
                drow[3*x]   = srow[3*sxi];
                drow[3*x+1] = srow[3*sxi+1];
                drow[3*x+2] = srow[3*sxi+2];
            }
        }
    } else {
        memcpy(tmp, src, (size_t)sw * sh * 3);
    }

    /* constant border pad (3-channel). copyMakeBorder args are byte-level:
     * tmp is 3*new_w bytes wide, dst is 3*netW bytes wide. */
    ngd_copy_make_border_u8(tmp, 3*new_w, new_h, 3*new_w,
                            dst, top, bottom, 3*left, 3*right, fill);
    free(tmp);
}


/* ---- letterbox params only (for NPU AIPP: 硬件做 letterbox, 这里只算 crop 参数) ----
 * 与 ngd_letterbox_u8 的 ratio/left/top 计算完全一致, 只是不做 resize+pad。
 * out_params[4] = { ratio_x, ratio_y, left, top }。 */
void ngd_letterbox_params(int sw, int sh, int netW, int netH, float *out_params)
{
    float r = (float)netW / (float)sw;
    float rh = (float)netH / (float)sh;
    if (rh < r) r = rh;
    int new_w = (int)lroundf((float)sw * r);
    int new_h = (int)lroundf((float)sh * r);

    float dw = (float)(netW - new_w) / 2.0f;
    float dh = (float)(netH - new_h) / 2.0f;

    out_params[0] = r;
    out_params[1] = r;
    out_params[2] = (float)lroundf(dw - 0.1f);  /* left */
    out_params[3] = (float)lroundf(dh - 0.1f);  /* top  */
}

/* ---- NCHW blob from BGR uint8 image (cv::dnn::blobFromImage) ----
 * Output: float blob[1 * 3 * netH * netW], NCHW layout, values in [0,1].
 * swapRB=true → BGR→RGB channel swap. scale=1/255, mean=0. */
void ngd_blob_from_image(const uint8_t *bgr, int sw, int sh, int sstride,
                         float *blob, int netW, int netH)
{
    UNUSED(sw);
    UNUSED(sh);

    const float inv255 = 1.0f / 255.0f;
    const int plane = netW * netH;
    /* NCHW: blob[c*plane + y*netW + x] */
    for (int y = 0; y < netH; ++y) {
        const uint8_t *srow = bgr + (size_t)y * sstride;
        for (int x = 0; x < netW; ++x) {
            /* BGR→RGB swap: channel 0 = R (src[2]), 1 = G (src[1]), 2 = B (src[0]) */
            blob[0*plane + y*netW + x] = (float)srow[3*x + 2] * inv255;  /* R */
            blob[1*plane + y*netW + x] = (float)srow[3*x + 1] * inv255;  /* G */
            blob[2*plane + y*netW + x] = (float)srow[3*x + 0] * inv255;  /* B */
        }
    }
}

/* ---- NCHW uint8 blob from BGR uint8 image (no /255, for NPU uint8 tensor input) ----
 * Output: uint8_t blob[1 * 3 * netH * netW], NCHW layout, values in [0,255].
 * BGR→RGB channel swap. No scaling (NPU model dequantize handles /255). */
void ngd_blob_from_image_u8(const uint8_t *bgr, int sw, int sh, int sstride,
                            uint8_t *blob, int netW, int netH)
{
    UNUSED(sw);
    UNUSED(sh);

    const int plane = netW * netH;
    /* NCHW: blob[c*plane + y*netW + x] */
    for (int y = 0; y < netH; ++y) {
        const uint8_t *srow = bgr + (size_t)y * sstride;
        for (int x = 0; x < netW; ++x) {
            /* BGR→RGB swap: channel 0 = R (src[2]), 1 = G (src[1]), 2 = B (src[0]) */
            blob[0*plane + y*netW + x] = srow[3*x + 2];  /* R */
            blob[1*plane + y*netW + x] = srow[3*x + 1];  /* G */
            blob[2*plane + y*netW + x] = srow[3*x + 0];  /* B */
        }
    }
}
