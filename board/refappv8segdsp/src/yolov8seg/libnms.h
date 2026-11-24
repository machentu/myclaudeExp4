
#ifndef __LIBNMS_H__
#define __LIBNMS_H__

#ifdef __cplusplus
extern "C" {
#endif


#define NDEBUG

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <unistd.h>

#include "libobjectpool.h"

#define FY_AI_DET_MAX_STRIDE 32
#define FY_AI_DET_IMAGE_WIDTH 640
#define FY_AI_DET_IMAGE_HEIGHT 352
#define FY_AI_DET_NUM_CLASSES 2
//#define FY_AI_DET_CONF_THRESHOLD 0.15f
//#define FY_AI_DET_IOU_THRESHOLD 0.45f
#define FY_AI_DET_NUM_MESH \
    (FY_AI_DET_IMAGE_HEIGHT / FY_AI_DET_MAX_STRIDE) * (FY_AI_DET_IMAGE_WIDTH / FY_AI_DET_MAX_STRIDE) * (16 + 4 + 1)

void nms(float *data, ObjectPool *res);
void nms_sort_pool(Object **data, int left, int right);


#ifdef __cplusplus
}
#endif


#endif
