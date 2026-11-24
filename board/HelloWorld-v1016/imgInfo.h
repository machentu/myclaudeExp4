
#ifndef FILE_IMAGE_INFO_INTERFACE
#define FILE_IMAGE_INFO_INTERFACE

typedef enum hiSVP_IMAGE_TYPE_E
{
	SVP_IMAGE_TYPE_YUV400    = 0x1,
	SVP_IMAGE_TYPE_YUV420SP  = 0x2,
	SVP_IMAGE_TYPE_YUV422SP  = 0x3,
	SVP_IMAGE_TYPE_YUV420P   = 0x4,
	SVP_IMAGE_TYPE_YUV422P   = 0x5,
	SVP_IMAGE_TYPE_RGB888   = 0x6,
	SVP_IMAGE_TYPE_ARGB888  = 0x7,
	SVP_IMAGE_TYPE_BUTT
}SVP_DSP_IMAGE_TYPE_E;

typedef struct fySVP_DSP_IMAGE_S
{
	SVP_DSP_IMAGE_TYPE_E  enType; /* RW;The type of the image */
    unsigned int   u32Width;       /* RW;The width of the image */
    unsigned int    u32Height;      /* RW;The height of the image */
    unsigned int    au32PhyAddr[3]; /* RW;The physical address of the image */
    unsigned int    au32Stride[3];  /* RW;The stride of the image */
}SVP_DSP_IMAGE_EXT_S;

typedef struct fySVP_DSP_INFO
{
	SVP_DSP_IMAGE_EXT_S u32ClarityAddr;
	SVP_DSP_IMAGE_EXT_S u32BlurAddr;
	SVP_DSP_IMAGE_EXT_S u32MaskAddr;
	SVP_DSP_IMAGE_EXT_S u32OutAddr;
}SVP_DSP_INPUT_INFO;

extern SVP_DSP_INPUT_INFO *pInputInfo;

typedef struct
{
	int x;
	int y;
}SVP_DSP_VERTEX_S;

typedef struct
{
	SVP_DSP_IMAGE_EXT_S stInputImage;
	SVP_DSP_IMAGE_EXT_S stOutPutImage;
	SVP_DSP_VERTEX_S stSrcPos[4];
	SVP_DSP_VERTEX_S stDstPos[4];
	int bUpdateFlg;
}fhTRANS_TRAPEZOID_S;

extern fhTRANS_TRAPEZOID_S *pInputInfoWarp;

typedef struct
{
    unsigned int         u32SrcLeftAddr;
    unsigned int         u32SrcRightAddr;
    unsigned int         u32SrcAlphaAddr;
}VDSP_SRC_INFO;

#endif


