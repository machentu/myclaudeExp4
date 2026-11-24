#include "libnms.h"

#define max(a, b) (((a) > (b)) ? (a) : (b))
#define min(a, b) (((a) < (b)) ? (a) : (b))
#define breakpoint() assert(0 == 1)

float clamp(float x, int min_x, int max_x) {
    if (x <= min_x) return min_x;
    if (x >= max_x) return max_x;
    return x;
}

void nms_sort_pool(Object **data, int left, int right) {
    if (left >= right) return;
    int l = left;
    int r = right;
    Object *mid = data[left];
    while (l < r) {
        while (l < r && data[r]->confidence <= mid->confidence) r--;
        data[l] = data[r];
        while (l < r && data[l]->confidence > mid->confidence) l++;
        data[r] = data[l];
    }
    data[l] = mid;
    nms_sort_pool(data, left, l - 1);
    nms_sort_pool(data, l + 1, right);
}

#if 0
void nms(float *data, ObjectPool *res) {
    ObjectPool__init(res);

    int offset = 0;
    int stride = FY_AI_DET_MAX_STRIDE / 4;
    for (int l = 0; l < 3; l++) {
        for (int h = 0; h < FY_AI_DET_IMAGE_HEIGHT / stride; h++) {
            for (int w = 0; w < FY_AI_DET_IMAGE_WIDTH / stride; w++) {
                // Get class with max confidence
                int i = h * FY_AI_DET_IMAGE_WIDTH / stride + w + offset;
                float max_conf = 0;
                int cls = 0;
                for (int j = 0; j < FY_AI_DET_NUM_CLASSES; j++) {
                    float conf = data[(4 + j) * FY_AI_DET_NUM_MESH + i];
                    // float conf = Tensor__read(&tensor, 4 + j, i);
                    if (conf > max_conf) {
                        max_conf = conf;
                        cls = j;
                    }
                }
                if (max_conf < FY_AI_DET_CONF_THRESHOLD) continue;

                // Transform distance(ltrb) to box(xywh or xyxy).
                float l = data[0 * FY_AI_DET_NUM_MESH + i];
                float t = data[1 * FY_AI_DET_NUM_MESH + i];
                float r = data[2 * FY_AI_DET_NUM_MESH + i];
                float b = data[3 * FY_AI_DET_NUM_MESH + i];

                // Copy selected objects
                Object *obj = (Object *)malloc(sizeof(Object));
                obj->x1 = clamp((w + 0.5 - l) * stride, 0, FY_AI_DET_IMAGE_WIDTH);
                obj->y1 = clamp((h + 0.5 - t) * stride, 0, FY_AI_DET_IMAGE_HEIGHT);
                obj->x2 = clamp((w + 0.5 + r) * stride, 0, FY_AI_DET_IMAGE_WIDTH);
                obj->y2 = clamp((h + 0.5 + b) * stride, 0, FY_AI_DET_IMAGE_HEIGHT);
                obj->confidence = max_conf;
                obj->class_id = cls;
                obj->id = -1;

                ObjectPool__push_back(res, obj);
                if (res->size >= ObjectPool__MAX_SIZE - 1) return;
            }
        }
        offset += (FY_AI_DET_IMAGE_HEIGHT / stride) * (FY_AI_DET_IMAGE_WIDTH / stride);
        stride *= 2;
    }

    // Sort by confidence
    int N = res->size;
    nms_sort_pool(res->data, 0, N - 1);

    // Non-max suppression
    //int kill[N];
    //for (int i = 0; i < N; i++) kill[i] = 0;
    //for (int i = 0; i < N; i++) {
    //    if (kill[i] > 0) continue;
    //    for (int j = i + 1; j < N; j++) {
    //        if (kill[j] > 0) continue;
    //        Object *objA = ObjectPool__get(res, i);
    //        Object *objB = ObjectPool__get(res, j);
    //        if (objA->class_id == objB->class_id) {
    //            if (IoU(objA, objB) >= FY_AI_DET_IOU_THRESHOLD) {
    //                kill[j] = 1;
    //            }
    //        }
    //    }
    //}

    //// Clean data pool
    //int j = 0;
    //for (int i = 0; i < N; i++) {
    //    while (j < N && kill[j] > 0) {
    //        free(res->data[j]);
    //        res->size--;
    //        j++;
    //    }
    //    if (j >= N) break;
    //    res->data[i] = res->data[j];
    //    j++;
    //}
}


// External function for ctypes
float *CFixedNMSb1(float *data) {
    ObjectPool detections;
    static float dump_buffer[ObjectPool__SERIALIZE_BUFFER_SIZE];
    nms(data, &detections);
    ObjectPool__serialize(&detections, dump_buffer);
    ObjectPool__clear(&detections);
    return dump_buffer;
}
#endif