#include <stdio.h>
#include <xtensa/config/core.h>
#include <stdarg.h>
#include <stddef.h>
#include "print_dsp.h"

//old
//unsigned int info_address = 0;
//unsigned int out_address = 0;
//#ifdef FY01_TEST
//unsigned int graphic_address;
//#else
//VDSP_SRC_INFO graphic_address;
//#endif
//unsigned int result_address = 0;

//new, add more case
unsigned int info_address = 0;
unsigned int out_address = 0;
unsigned int result_address = 0;

VDSP_SRC_INFO graphic_address;
SVP_DSP_INPUT_INFO *pInputInfo = 0;
fhTRANS_TRAPEZOID_S *pInputInfoWarp = 0;

//fhTRANS_FaceInfo_S *pInputInfoFace = 0;
//fhTRANS_HandInfo_S *pInputInfoHand = 0;


void deviceInit()
{
	int imgInfo = reg32(IMG_INFO);
	int *pInfo = (int*)imgInfo;

	if(pInfo == 0)
	{
		return;
	}
	pInputInfo = (SVP_DSP_INPUT_INFO*)pInfo;
	pInputInfoWarp = (fhTRANS_TRAPEZOID_S*)pInfo;
//	pInputInfoFace = (fhTRANS_FaceInfo_S*)pInfo;
//	pInputInfoHand = (fhTRANS_HandInfo_S*)pInfo;

	result_address = reg32(OUT_INFO);

	//add deep interface
	graphic_address.u32SrcLeftAddr = pInfo[0];
	graphic_address.u32SrcRightAddr = pInfo[1];
//	graphic_address.u32SrcAlphaAddr = pInfo[2];

	if(0 != graphic_address.u32SrcRightAddr)
	{
		info_address = pInfo[2];

		graphic_address.u32SrcAlphaAddr = info_address;

		out_address = pInfo[3];//(int)(pInfo + 3);
	}
	else
	{
		info_address = (int)(pInfo + 2);
	}


//	int imgInfo = reg32(IMG_INFO);//0xd0000044,graphic_address
//	result_address = reg32(OUT_INFO);//0xd0000048
//
//	int *pInfo = (int*)imgInfo;
//
//	if(pInfo == 0)
//	{
//		return;
//	}
//
//#ifdef FY01_TEST
//	graphic_address = (int)pInfo;
//#else
//	graphic_address.u32SrcLeftAddr = pInfo[0];
//	graphic_address.u32SrcRightAddr = pInfo[1];
//
//	if(0 != graphic_address.u32SrcRightAddr)
//	{
//		info_address = pInfo[2];
//		out_address = pInfo[3];//(int)(pInfo + 3);
//	}
//	else
//	{
//		info_address = (int)(pInfo + 2);
//	}
////	info_address = (int)(pInfo + 2);
////	out_address = (int)(pInfo + 3);
//
////	printf_dsp("addr: %p %p %p %p\n",
////			graphic_address.u32SrcLeftAddr,
////			graphic_address.u32SrcRightAddr,
////			info_address,
////			out_address
////			);
//#endif

}

void DspInit()
{

	reg32(INTER_EN) = 0xf;


	// printf_dsp("dsp init\n");

//	deviceInit();
}

