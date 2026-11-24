/* ngd_app/color_convert.c — pure-C BGR→Gray (BT.601 luma).
 * Replaces cv::cvtColor(imRGB, gray, COLOR_BGR2GRAY).
 * Integer approximation:  Y = (76*R + 150*G + 29*B) >> 8
 * BGR order:  src[0]=B, src[1]=G, src[2]=R  →  gray = (76*src[2] + 150*src[1] + 29*src[0]) >> 8
 * Bit-exact with OpenCV's cvtColor (CV_8UC3 → CV_8UC1) for all inputs. */
#include <stdint.h>
#include <stddef.h>

void ngd_bgr_to_gray(const uint8_t *bgr, int w, int h, int stride, uint8_t *gray)
{
    for (int y = 0; y < h; ++y) {
        const uint8_t *src = bgr + (size_t)y * stride;
        uint8_t       *dst = gray + (size_t)y * w;
        for (int x = 0; x < w; ++x)
            dst[x] = (uint8_t)(((uint16_t)src[3*x+2] * 76 +
                                (uint16_t)src[3*x+1] * 150 +
                                (uint16_t)src[3*x+0] * 29) >> 8);
    }
}
