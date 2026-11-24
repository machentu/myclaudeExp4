#ifndef NGD_APP_IMAGE_TYPES_H
#define NGD_APP_IMAGE_TYPES_H
/*
 * ngd_app/image_types.h — simple image descriptors (zero-dependency, zero-OpenCV).
 * Replaces cv::Mat in the refappv8seg_full public API.
 */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t *data;
    int      w, h, stride;   /* stride >= w, typically == w for contiguous */
} ngd_image_u8;

typedef struct {
    float *data;
    int    w, h, stride;
} ngd_image_f32;

#ifdef __cplusplus
}
#endif
#endif /* NGD_APP_IMAGE_TYPES_H */
