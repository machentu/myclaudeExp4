#ifndef __LIBOBJECTPOOL_H__
#define __LIBOBJECTPOOL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <unistd.h>


float clamp(float x, int min_x, int max_x);

typedef struct {
    float x1;
    float y1;
    float x2;
    float y2;
    float confidence;
    int class_id;
    int id;
    float kpts[32];//32 mask feature???
} Object;

void Object__repr(Object *self);

#define ObjectPool__MAX_SIZE 200 //2000
#define ObjectPool__SERIALIZE_BUFFER_SIZE 701
typedef struct {
    int size;  // number of results
    Object *data[ObjectPool__MAX_SIZE];
} ObjectPool;

Object *ObjectPool__get(ObjectPool *self, int i);
void ObjectPool__init(ObjectPool *self);
void ObjectPool__serialize(ObjectPool *self, float *dump_buffer);
void ObjectPool__deserialize(float *data, ObjectPool *res);
void ObjectPool__push_back(ObjectPool *self, Object *x);
void ObjectPool__repr(ObjectPool *self);
void ObjectPool__clear(ObjectPool *self);

float IoU(const Object *A, const Object *B);
float DIoU(const Object *A, const Object *B);

#ifdef __cplusplus
}
#endif

#endif
