
#include "libkptdecode.h"
#include "libnms.h"
#include "memory.h"
//#include "cwrapper.h"
#include "outsize.h"
#include "sigmoidTable_q15.h"

#define JUST_KEEP_PERSON_LABEL 1

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif
#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif


#define scoreThre 0.25
#define nmsThre 0.7


float sigmoid2(float x)
{
	return 1.0f / (1.0f + exp(-x));
}

const float qval = 32768.0f;// 64*256.0f; 

float sigmoid_q15(int data, unsigned short int_width)
{
	//unsigned short i = size;
	unsigned short shift_size = 8 + 3;// -int_width;
	unsigned int bit_mask = 0x7FF >> int_width;
	unsigned int full_frac = bit_mask + 1;
	const short* lookup_table = sigmoidTable_q15;

	int in = data;
	short frac = in & bit_mask;
	short value = lookup_table[(in >> shift_size) + 128];//(unsigned char)

	short out = 0;

	if (frac != 0) //(in >> shift_size) != 0x80) //0x7f)
	{
		short value2 = lookup_table[(129 + (in >> shift_size))]; //lookup_table[(unsigned char)(1 + (unsigned char)(in >> shift_size))];
		out = ((int)(full_frac - frac) * value + (int)value2 * frac) >> 8;// shift_size;
	}
	else
	{
		out = value;
	}

	return out/ qval;
}

int decodePosition(float* pData, float* pData1, int featW, int featH, float* top_data, int input_w, int input_h, int maxDet)
{
	ObjectPool resPool;
	ObjectPool* res = &resPool;
	ObjectPool__init(res);

	int nc = 80;
	int nm = featH - nc - 4;

	float* pOneData = (float*)malloc(featH*sizeof(float));

	for (int kid = 0; kid < featW; ++kid)
	{
		
		for (int hid = 0; hid < featH; ++hid) {
			pOneData[hid] = pData[hid * featW + kid];
		}

		float maxConf = -1;
		float curConf = 0;
		int label = 0;
		for (int j = 0; j < nc; ++j)
		{
			curConf = pOneData[(j + 4)];
			if (curConf > maxConf)
			{
				maxConf = curConf;
				label = j;
			}
		}
		//just keep person label
#ifdef JUST_KEEP_PERSON_LABEL
		if (label != 0)
		{
			continue;
		}
#endif
		if (maxConf > scoreThre)
		{
			float x = pOneData[(0) ];
			float y = pOneData[(1) ];
			float w = pOneData[(2) ];
			float h = pOneData[(3) ];

			float x1 = x - w / 2;
			float y1 = y - h / 2;
			float x2 = x + w / 2;
			float y2 = y + h / 2;

			Object* obj = (Object*)malloc(sizeof(Object));
			obj->x1 = x1;
			obj->y1 = y1;
			obj->x2 = x2;
			obj->y2 = y2;
			obj->confidence = maxConf;
			obj->class_id = label;
			obj->id = -1;
			for (int mid = 0; mid < nm; ++mid)
			{
				obj->kpts[mid] = pOneData[4+nc+mid];
			}

			ObjectPool__push_back(res, obj);
			if (res->size >= ObjectPool__MAX_SIZE - 1) break;
		}
	
		if (res->size >= ObjectPool__MAX_SIZE - 1) break;
	}
	
	int j = 0;
	int N = res->size;
	nms_sort_pool(res->data, 0, N - 1);

	// Non-max suppression
	int * kill = (int*)malloc(N*sizeof(int));
	for (int i = 0; i < N; i++) kill[i] = 0;
	for (int i = 0; i < N; i++) {
		if (kill[i] > 0) continue;
		for (int j = i + 1; j < N; j++) {
			if (kill[j] > 0) continue;
			Object* objA = ObjectPool__get(res, i);
			Object* objB = ObjectPool__get(res, j);
			if (objA->class_id == objB->class_id) {
				if (IoU(objA, objB) >= nmsThre) {//FY_AI_DET_IOU_THRESHOLD
					kill[j] = 1;
				}
			}
		}
	}
	//get my result
	int jeachFace = 0;
	int eachFace = SIZE_ONE_OUTPUT;// 6;
	int numFaceDetect = 0;
	// float val = 0;

	//process the proto mask at the same time
	for (j = 0; j < res->size; ++j)
	{
		if (kill[j] != 1)
		{
			Object* obj = res->data[j];
			top_data[jeachFace + 0] = obj->class_id + 1;
			top_data[jeachFace + 1] = obj->confidence;
			top_data[jeachFace + 2] = obj->x1;
			top_data[jeachFace + 3] = obj->y1;
			top_data[jeachFace + 4] = obj->x2;
			top_data[jeachFace + 5] = obj->y2;

			for (int mid = 0; mid < nm; ++mid)
			{
				top_data[jeachFace + 6 + mid] = obj->kpts[mid];
			}

			jeachFace += eachFace;
			numFaceDetect++;
			if (numFaceDetect >= maxDet)
			{
				break;
			}
		}
	}

	// Clean data pool
	ObjectPool__clear(res);
	free(kill);
	free(pOneData);

	return numFaceDetect;
}


int decodeMask(float* protosD, int featW, int featH, int c, float* top_data, int input_w, int input_h, int numDet, float* pOutMask)
{
	int sz = featW * featH;

	float width_ratio = (float)featW / input_w;
	float height_ratio = (float)featH / input_h;
	float downsampled_bboxes[4] = { 0 };

	for (int posid = 0; posid < sz; ++posid)
	{
		pOutMask[posid] = 0;
	}

	for (int jid = 0; jid < numDet; ++jid)
	{
		float* pDet = &top_data[jid * SIZE_ONE_OUTPUT];
		float* pMaskIn = pDet + 6;
		float* pBox = pDet + 2;

		downsampled_bboxes[0] = pBox[0] * width_ratio;
		downsampled_bboxes[1] = pBox[1] * height_ratio;
		downsampled_bboxes[2] = pBox[2] * width_ratio;
		downsampled_bboxes[3] = pBox[3] * height_ratio;

		float* pCurMask = pOutMask;// +jid * sz;
		for (int posid = 0; posid < sz; ++posid) {
#if 1
			int xpos = posid % featW;
			int ypos = posid / featW;
			if (!(xpos > downsampled_bboxes[0] &&
				xpos < downsampled_bboxes[2] &&
				ypos > downsampled_bboxes[1] &&
				ypos < downsampled_bboxes[3]
				))
			{
				//pCurMask[posid] = 0.0f;
				continue;
			}
#endif

			float sum = 0.0f;
			for (int kid = 0; kid < SIZE_MASK; ++kid)
			{
				sum += (pMaskIn[kid] * (protosD[kid * sz + posid]));
			}
			if (pCurMask[posid] < 0.5) //for much label at the same mask
			{
#if 0
				pCurMask[posid] = sigmoid2(sum);
#else
				int temp = sum * qval;
				//if (temp > 2047)temp = 2047;		//2047 = 0x7FF = 7.99(Q7.8)
				//if (temp < -2048)temp = -2048;		//-2048 = -8(Q7.8)

				if (temp > 260096)temp = 260096;		//2047 = 0x7FF = 7.99(Q7.8)
				if (temp < -262144)temp = -262144;		//-2048 = -8(Q7.8)

				pCurMask[posid] = sigmoid_q15(temp, 3);
#endif

			}
		}


	}

	return 0;
}

