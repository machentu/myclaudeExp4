
#ifndef FILE_TILE_MEM_MANAGER_HEADER_H
#define FILE_TILE_MEM_MANAGER_HEADER_H


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#define _USE_MATH_DEFINES
#include <math.h>

#include "tileManager.h"
#include "commonDef.h"
#include "defines.h"
#include "defines1Dwarp.h"
//#include "ResizeBil.h"

#if defined(__XTENSA__)
#include <sys/times.h>
#include <xtensa/sim.h>
#include <xtensa/tie/xt_ivpn.h>
#include <xtensa/tie/xt_misc.h>
#include <xtensa/tie/xt_interrupt.h>
#endif

typedef int32_t   XI_ERR_TYPE;
#define XI_ERR_OK               0

extern xvTileManager xvTMObj;
extern uint8_t pBankBuffPool0[];
extern uint8_t pBankBuffPool1[];

extern IDMA_DRAM idma_buffer_t idmaObjBuff[];

typedef struct intrCallbackDataStruct
{
  int32_t intrCount;
} intrCbDataStruct;
extern intrCbDataStruct cbData;

#define XVTM_RAISE_EXCEPTION() (&xvTMobj)->idmaErrorFlag = XV_ERROR_IDMA;
#define XVTM_RESET_EXCEPTION() (&xvTMobj)->idmaErrorFlag = XV_ERROR_SUCCESS;
#define XVTM_IS_TRANSFER_SUCCESS(pxvTM) (((pxvTM)->idmaErrorFlag == XV_ERROR_SUCCESS) ? 1 : 0)
void errCallbackFunc(const idma_error_details_t* data);
void intrCallbackFunc(void *pCallBackStr);

int InitTileMrg();

typedef struct TileDMATransferStruct {
	xvTile *itile;
	uint8_t *isrc_ptr;
	uint8_t *idst_ptr;
	size_t irow_size;
	int isrc_pitch;
	int idst_pitch;
	size_t inum_rows;
	int is_left;
	int is_right;
	int is_top;
	int is_bottom;
	xvTile *otile;
	uint8_t *osrc_ptr;
	uint8_t *odst_ptr;
	size_t orow_size;
	int osrc_pitch;
	int odst_pitch;
	size_t onum_rows;
} tdma, *ptdma;

typedef struct {
	tdma *tdmas;
	int32_t num_loop;
	int32_t x_scale;
	int32_t y_scale;
} Context;

void Initialize(Context *ctx, xvFrame *inFrame, xvFrame *outFrame,
		float scalex,float scaley);
void PadTile(tdma* p_tdma);

#endif


