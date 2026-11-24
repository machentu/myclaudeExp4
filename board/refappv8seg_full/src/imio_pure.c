/* ngd_app/imio_pure.c — image + depth I/O via stb_image (zero OpenCV).
 * Replaces src/imio.cpp (formerly cv::imread). Same C API as imio.h. */
#include "ngd_app/imio.h"
#include "stb_image.h"
#include <stdlib.h>
#include <string.h>

int ngd_app_load_gray(const char *path, uint8_t **out, int *w, int *h)
{
    int ch;
    uint8_t *data = stbi_load(path, w, h, &ch, 1);  /* force 1 channel */
    if (!data) return 0;
    *out = data;
    return 1;
}

int ngd_app_load_depth_meters(const char *path, float depthMapFactor,
                               float **out, int *w, int *h)
{
    int ch;
    /* TUM depth images are 16-bit PNG. stbi_load_16 returns unsigned short*. */
    if (stbi_is_16_bit(path)) {
        unsigned short *raw16 = stbi_load_16(path, w, h, &ch, 0);
        if (!raw16) return 0;
        int n = (*w) * (*h);
        float *buf = (float *)malloc((size_t)n * sizeof(float));
        float inv = (depthMapFactor > 0.0f) ? (1.0f / depthMapFactor) : 1.0f;
        for (int i = 0; i < n; ++i)
            buf[i] = (float)raw16[i] * inv;
        stbi_image_free(raw16);
        *out = buf;
        return 1;
    } else {
        /* fallback: 8-bit depth (unusual for TUM, but supported) */
        uint8_t *raw8 = stbi_load(path, w, h, &ch, 0);
        if (!raw8) return 0;
        int n = (*w) * (*h);
        float *buf = (float *)malloc((size_t)n * sizeof(float));
        float inv = (depthMapFactor > 0.0f) ? (1.0f / depthMapFactor) : 1.0f;
        for (int i = 0; i < n; ++i)
            buf[i] = (float)raw8[i] * inv;
        stbi_image_free(raw8);
        *out = buf;
        return 1;
    }
}
