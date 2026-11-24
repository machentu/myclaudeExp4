#ifndef FILE_PRINT_DSP_FPGA_IMP_HEADER_H
#define FILE_PRINT_DSP_FPGA_IMP_HEADER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include "imgInfo.h"
#include "orbInfo.h"

#define reg(x) *(volatile unsigned char *)(x)
#define reg32(x) *(volatile unsigned int *)(x)
#define bit(x) (1UL << (x))


#define CMD_REG 0xc0000188  //for fy01: 0xd0000000
#define IMG_INFO 0xc0000500 //for fy01: 0xd0000044
#define OUT_INFO 0xc0000504 //for fy01: 0xd0000048
#define INTERAPT 0xc9300000 //for fy01: 0xd0000030
#define INTER_EN 0xc930000C //for fy01: 0xd0000030

//typedef struct
//{
//    unsigned int         u32SrcLeftAddr;
//    unsigned int         u32SrcRightAddr;
//}VDSP_SRC_INFO;
//
//extern unsigned int info_address;
//extern unsigned int out_address;
//#ifdef FY01_TEST
//extern unsigned int graphic_address;
//#else
//extern VDSP_SRC_INFO graphic_address;
//#endif
//extern unsigned int result_address;


extern unsigned int info_address;
extern unsigned int out_address;
#ifdef FY01_TEST
extern unsigned int graphic_address;
#else
extern VDSP_SRC_INFO graphic_address;
#endif
extern unsigned int result_address;


void deviceInit();
void DspInit();


#ifdef __cplusplus
}
#endif

#endif
