
#ifndef __LIBKEY_POINT_DETECTOR_POST_H__
#define __LIBKEY_POINT_DETECTOR_POST_H__

#ifdef __cplusplus
extern "C" {
#endif

int decodePosition(float* pData, float* pData1, int featW, int featH, float* top_data, int input_w, int input_h, int maxv);

int decodeMask(float* pData1, int featW, int featH, int c, float* top_data, int input_w, int input_h, int numDet, float* pOutMask);

#ifdef __cplusplus
}
#endif


#endif

