#define NDEBUG

#include "libobjectpool.h"

#define max(a, b) (((a) > (b)) ? (a) : (b))
#define min(a, b) (((a) < (b)) ? (a) : (b))
#define breakpoint() assert(0 == 1)

void ObjectPool__init(ObjectPool *self) {
    self->size = 0;
}

void Object__repr(Object *self) {
    printf("Object(%f, %f, %f, %f, %f, %d, %d)\n", self->x1, self->y1, self->x2, self->y2, self->confidence,
           self->class_id, self->id);
}

void ObjectPool__push_back(ObjectPool *self, Object *x) {
    self->data[self->size] = x;
    self->size++;
}

Object *ObjectPool__get(ObjectPool *self, int i) {
    assert(i >= 0 && i <= self->size);
    return self->data[i];
}

void ObjectPool__repr(ObjectPool *self) {
    for (int i = 0; i < self->size; i++) {
        Object *obj = self->data[i];
        printf("Object[%d](%f, %f, %f, %f, %f, %d, %d)\n", i, obj->x1, obj->y1, obj->x2, obj->y2, obj->confidence,
               obj->class_id, obj->id);
    }
    printf("\n");
}

void ObjectPool__clear(ObjectPool *self) {
    for (int i = 0; i < self->size; i++) {
        free(self->data[i]);
    }
    self->size = 0;
}

void ObjectPool__serialize(ObjectPool *pool, float *dump_buffer) {
    dump_buffer[0] = pool->size;
    for (int i = 0; i < pool->size; i++) {
        Object *obj = ObjectPool__get(pool, i);
        dump_buffer[1 + i * 7 + 0] = obj->x1;
        dump_buffer[1 + i * 7 + 1] = obj->y1;
        dump_buffer[1 + i * 7 + 2] = obj->x2;
        dump_buffer[1 + i * 7 + 3] = obj->y2;
        dump_buffer[1 + i * 7 + 4] = obj->confidence;
        dump_buffer[1 + i * 7 + 5] = obj->class_id;
        dump_buffer[1 + i * 7 + 6] = obj->id;
    }
}

void ObjectPool__deserialize(float *dump_buffer, ObjectPool *res) {
    ObjectPool__init(res);
    int n = dump_buffer[0];
    for (int i = 0; i < n; i++) {
        Object *obj = (Object *)malloc(sizeof(Object));
        obj->x1 = dump_buffer[1 + i * 7 + 0];
        obj->y1 = dump_buffer[1 + i * 7 + 1];
        obj->x2 = dump_buffer[1 + i * 7 + 2];
        obj->y2 = dump_buffer[1 + i * 7 + 3];
        obj->confidence = dump_buffer[1 + i * 7 + 4];
        obj->class_id = dump_buffer[1 + i * 7 + 5];
        obj->id = dump_buffer[1 + i * 7 + 6];
        ObjectPool__push_back(res, obj);
        if (res->size >= ObjectPool__MAX_SIZE - 1) return;
    }
}

float IoU(const Object *A, const Object *B) {
    float ix1 = max(A->x1, B->x1);
    float iy1 = max(A->y1, B->y1);
    float ix2 = min(A->x2, B->x2);
    float iy2 = min(A->y2, B->y2);
    float iw = max(0, ix2 - ix1);
    float ih = max(0, iy2 - iy1);

    float inter = iw * ih;
    float area1 = (A->x2 - A->x1) * (A->y2 - A->y1);
    float area2 = (B->x2 - B->x1) * (B->y2 - B->y1);
	if (area1 + area2 - inter < 0.001)
	{
		return 1;
	}
    float iou = inter / (area1 + area2 - inter);
    return iou;
}


float DIoU(const Object *A, const Object *B) {
    float ix1 = max(A->x1, B->x1);
    float iy1 = max(A->y1, B->y1);
    float ix2 = min(A->x2, B->x2);
    float iy2 = min(A->y2, B->y2);
    float iw = max(0, ix2 - ix1);
    float ih = max(0, iy2 - iy1);

    float inter = iw * ih;
    float area1 = (A->x2 - A->x1) * (A->y2 - A->y1);
    float area2 = (B->x2 - B->x1) * (B->y2 - B->y1);
    float iou = inter / (area1 + area2 - inter);

    float ow = (B->x1 + B->x2) / 2.0f - (A->x1 + A->x2) / 2.0f;
    float oh = (B->y1 + B->y2) / 2.0f - (A->y1 + A->y2) / 2.0f;
    float c = iw * iw + ih * ih;
    float d = ow * ow + oh * oh;
    return iou - d / c;
}
