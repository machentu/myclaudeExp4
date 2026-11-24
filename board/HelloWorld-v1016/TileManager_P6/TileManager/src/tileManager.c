/*
 * Copyright (c) 2022 Cadence Design Systems Inc. ALL RIGHTS RESERVED.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**********************************************************************************
 * FILE:  tileManager.c
 *
 * DESCRIPTION:
 *
 *    This file contains Tile Manager implementation. It uses xvmem utility for
 *    buffer allocation and idma library for 2D data transfer.
 *
 *
 ********************************************************************************** */

#include <stdio.h>
#include <string.h>
#include <xtensa/tie/xt_ivpn.h>
#include "tileManager.h"

static void xvExtendEdgesReflect101_I8(xvTile const * tile, int32_t frame_width, int32_t frame_height);
static void xvExtendEdgesReflect101_I16(xvTile const * tile, int32_t frame_width, int32_t frame_height);


/**********************************************************************************
 * FUNCTION: xvInitIdma()
 *
 * DESCRIPTION:
 *     Function to initialize iDMA library. Tile Manager uses iDMA library
 *     in buffer mode. DMA transfer is scheduled as soon as the descriptor
 *     is added.
 *
 *
 * INPUTS:
 *     xvTileManager        *pxvTM              Tile Manager object
 *     idma_buffer_t        *buf                iDMA library handle; contains descriptors and idma library object
 *     int32_t              numDescs            Number of descriptors that can be added in buffer
 *     int32_t              maxBlock            Maximum block size allowed
 *     int32_t              maxPifReq           Maximum number of outstanding pif requests
 *     idma_err_callback_fn errCallbackFunc     Callback for dma transfer error
 *     idma_callback_fn     cbFunc              Callback for dma transfer completion
 *     void                 *cbData             Data needed for completion callback function
 *
 * OUTPUTS:
 *     Returns XVTM_ERROR if it encounters an error, else returns XVTM_SUCCESS
 *
 ********************************************************************************** */

int32_t xvInitIdma(xvTileManager *pxvTM, idma_buffer_t *buf, int32_t numDescs, int32_t maxBlock,
                   int32_t maxPifReq, idma_err_callback_fn errCallbackFunc, idma_callback_fn cbFunc, void * cbData)
{
  idma_status_t retVal;
  XV_CHECK_ERROR_NULL(pxvTM == NULL, XVTM_ERROR, "NULL TM Pointer");
  pxvTM->errFlag = XV_ERROR_SUCCESS;
  XV_CHECK_ERROR(buf == NULL, pxvTM->errFlag = XV_ERROR_BUFFER_NULL, XVTM_ERROR, "NULL Buffer Pointer");
  XV_CHECK_ERROR((numDescs < 1), pxvTM->errFlag = XV_ERROR_BAD_ARG, XVTM_ERROR, "Invalid arg");
  idma_type_t type             = IDMA_2D_DESC;
  
  (void)maxBlock;
  (void)maxPifReq;
  (void)errCallbackFunc;
  retVal = idma_init_loop(buf, type, numDescs, cbData, cbFunc);
  /* No error check on retVal as idma_init_loop() never return a non IDMA_OK value */
  return(XVTM_SUCCESS);
}

/**********************************************************************************
 * FUNCTION: xvInitTileManager()
 *
 * DESCRIPTION:
 *     Function to initialize Tile Manager. Resets Tile Manager's internal structure elements.
 *     Initializes Tile Manager's iDMA object.
 *
 * INPUTS:
 *     xvTileManager *pxvTM      Tile Manager object
 *     void          *pdmaObj    iDMA object. It should be initialized before calling this function
 *
 * OUTPUTS:
 *     Returns XVTM_ERROR if it encounters an error, else returns XVTM_SUCCESS
 *
 ********************************************************************************** */

int32_t xvInitTileManager(xvTileManager *pxvTM, void *pdmaObj)
{
  int32_t index;

  XV_CHECK_ERROR_NULL(pxvTM == NULL, XVTM_ERROR, "NULL TM Pointer");

  pxvTM->errFlag       = XV_ERROR_SUCCESS;
  pxvTM->idmaErrorFlag = XV_ERROR_SUCCESS;

  XV_CHECK_ERROR(pdmaObj == NULL, pxvTM->errFlag = XV_ERROR_BUFFER_NULL, XVTM_ERROR, "NULL TM Pointer");

  // Initialize DMA related elements
  pxvTM->pdmaObj             = pdmaObj;
  pxvTM->tileDMApendingCount = 0;
  pxvTM->tileDMAstartIndex   = 0;

  // Initialize Memory banks related elements
#ifndef XVTM_USE_XMEM
  for (index = 0; index < (int32_t) MAX_NUM_MEM_BANKS; index++) {
    pxvTM->pMemBankStart[index] = NULL;
    pxvTM->memBankSize[index]   = 0;
  }
#endif
  // Reset tile related elements
  pxvTM->tileCount = 0;
  for (index = 0; index < (int32_t) ((MAX_NUM_TILES + 31u) / 32u); index++) {
    pxvTM->tileAllocFlags[index] = 0x00000000;
  }

  // Reset frame related elements
  pxvTM->frameCount = 0;
  for (index = 0; index < (int32_t) ((MAX_NUM_FRAMES + 31u) / 32u); index++) {
    pxvTM->frameAllocFlags[index] = 0x00000000;
  }

  // Reset TM context
#ifdef FIK_FRAMEWORK
  pxvTM->tmContextSize = 0;
  pxvTM->allocationsIdx = 0;
#endif

#ifdef TM_LOG
  pxvTM->tm_log_fp = fopen(TILE_MANAGER_LOG_FILE_NAME, "w");
  if (pxvTM->tm_log_fp == NULL)
  {
    pxvTM->errFlag = XV_ERROR_FILE_OPEN;
    return(XVTM_ERROR);
  }
#endif
  return(XVTM_SUCCESS);
}

/**********************************************************************************
 * FUNCTION: xvResetTileManager()
 *
 * DESCRIPTION:
 *     Function to reset Tile Manager. It releases the buffers
 *     and resets the Tile Manager object.
 *
 * INPUTS:
 *     xvTileManager *pxvTM      Tile Manager object
 *
 * OUTPUTS:
 *     Returns XVTM_ERROR if it encounters an error, else returns XVTM_SUCCESS
 *
 ********************************************************************************** */

int32_t xvResetTileManager(xvTileManager *pxvTM)
{
  XV_CHECK_ERROR_NULL(pxvTM == NULL, XVTM_ERROR, "NULL TM Pointer");
  // Close the Tile Manager logging file
#ifdef TM_LOG
  if (pxvTM->tm_log_fp)
  {
    fclose(pxvTM->tm_log_fp);
  }
#endif
#ifndef XVTM_USE_XMEM
  // Free all the xvmem allocated buffers
  //Type cast to void to avoid MISRA 17.7 violation
  (void) xvFreeAllBuffers(pxvTM);
#endif
  // Resetting the Tile Manager pointer.
  // This will free all allocated tiles and buffers.
  // It will not reset dma object.
  //Type cast to void to avoid MISRA 17.7 violation
  (void) memset(pxvTM, 0, sizeof(xvTileManager));
  return(XVTM_SUCCESS);
}

/**********************************************************************************
 * FUNCTION: xvInitMemAllocator()
 *
 * DESCRIPTION:
 *     Function to initialize memory allocator. Tile Manager uses xvmem library as memory allocator.
 *     It takes array of pointers to memory pool's start addresses and respective sizes as input and
 *     uses the memory pools when memory is allocated.
 *
 * INPUTS:
 *     xvTileManager *pxvTM      Tile Manager object
 *     int32_t numMemBanks       Number of memory pools
 *     void **pBankBuffPool      Array of memory pool start address
 *     int32_t* buffPoolSize     Array of memory pool sizes
 *
 * OUTPUTS:
 *     Returns XVTM_ERROR if it encounters an error, else returns XVTM_SUCCESS
 *
 ********************************************************************************** */
#ifndef XVTM_USE_XMEM
int32_t xvInitMemAllocator(xvTileManager *pxvTM, int32_t numMemBanks, void **pBankBuffPool, int32_t* buffPoolSize)
{
  int32_t indx;
  xvmem_status_t retVal;
  XV_CHECK_ERROR_NULL(pxvTM == NULL, XVTM_ERROR, "NULL TM Pointer");
  pxvTM->errFlag = XV_ERROR_SUCCESS;
  XV_CHECK_ERROR(((numMemBanks <= (int32_t) 0) || (numMemBanks > (int32_t) MAX_NUM_MEM_BANKS)), pxvTM->errFlag = XV_ERROR_BAD_ARG, XVTM_ERROR, "Invalid arg");
  XV_CHECK_ERROR(((pBankBuffPool == NULL) || (buffPoolSize == NULL)), pxvTM->errFlag = XV_ERROR_POINTER_NULL, XVTM_ERROR, "Invalid arg");

  xvmem_mgr_t *pmemBankMgr;
  pxvTM->numMemBanks = numMemBanks;

  for (indx = 0; indx < numMemBanks; indx++) {
    XV_CHECK_ERROR(((pBankBuffPool[indx] == NULL) || (buffPoolSize[indx] <= 0)), pxvTM->errFlag = XV_ERROR_BAD_ARG, XVTM_ERROR, "Invalid arg");
    XV_CHECK_ERROR((!((XV_PTR_START_IN_DRAM(pBankBuffPool[indx])) && (XV_PTR_END_IN_DRAM((((uint8_t *) pBankBuffPool[indx]) + buffPoolSize[indx]))))), pxvTM->errFlag = XV_ERROR_BAD_ARG, XVTM_ERROR, "Invalid arg");
    pmemBankMgr                = &(pxvTM->memBankMgr[indx]);
    pxvTM->pMemBankStart[indx] = pBankBuffPool[indx];
    pxvTM->memBankSize[indx]   = buffPoolSize[indx];
    retVal                     = xvmem_init(pmemBankMgr, pBankBuffPool[indx], (uint32_t) buffPoolSize[indx], 0, NULL);
    XV_CHECK_ERROR((retVal != XVMEM_OK), pxvTM->errFlag = XV_ERROR_XVMEM_INIT, XVTM_ERROR, "Error");
  }
  return(XVTM_SUCCESS);
}
#endif
/**********************************************************************************
 * FUNCTION: xvAllocateBuffer()
 *
 * DESCRIPTION:
 *     Allocates buffer from the given buffer pool. It returns aligned buffer.
 *
 * INPUTS:
 *     xvTileManager *pxvTM      Tile Manager object
 *     int32_t buffSize          Size of the requested buffer
 *     int32_t buffColor         Color/index of requested buffer
 *     int32_t buffAlignment     Alignment of requested buffer
 *
 * OUTPUTS:
 *     Returns the buffer with requested parameters. If an error occurs, returns ((void *)(XVTM_ERROR))
 *
 ********************************************************************************** */

void *xvAllocateBuffer(xvTileManager *pxvTM, int32_t buffSize, int32_t buffColor, int32_t buffAlignment)
{
  void *buffOut = NULL;
  int32_t currColor;

  XV_CHECK_ERROR_NULL(pxvTM == NULL, (void *) XVTM_ERROR, "NULL TM Pointer");
  pxvTM->errFlag = XV_ERROR_SUCCESS;
  XV_CHECK_ERROR((buffSize <= 0), pxvTM->errFlag = XV_ERROR_BAD_ARG, ((void *) XVTM_ERROR), "XV_ERROR_BAD_ARG");
  XV_CHECK_ERROR((buffAlignment <= 0), pxvTM->errFlag = XV_ERROR_BAD_ARG, ((void *) XVTM_ERROR), "XV_ERROR_BAD_ARG");
#ifndef XVTM_USE_XMEM
  int32_t numMemBanks = pxvTM->numMemBanks;
#else
  int32_t numMemBanks = xmem_bank_get_num_banks();
#endif

  XV_CHECK_ERROR((!(((buffColor >= (int32_t) 0) && (buffColor < numMemBanks)) || (buffColor == (int32_t) XV_MEM_BANK_COLOR_ANY))), pxvTM->errFlag = XV_ERROR_BAD_ARG, ((void *) XVTM_ERROR), "XV_ERROR_BAD_ARG");


  // if the color is XV_MEM_BANK_COLOR_ANY, loop through all the buffers and pick one that meets all criteria
  if (buffColor == (int32_t) XV_MEM_BANK_COLOR_ANY) {
    currColor = 0;
    while ((buffOut == NULL) && (currColor < numMemBanks)) {
#ifdef XVTM_USE_XMEM
    	xmem_bank_status_t xmbs;
    	buffOut = xmem_bank_alloc(currColor, buffSize, buffAlignment, &xmbs);
#else
		xvmem_status_t errCode;
        buffOut = xvmem_alloc(&(pxvTM->memBankMgr[currColor]), (uint32_t) buffSize, (uint32_t) buffAlignment, &errCode);
#endif
      currColor++;
    }
  }
  else  {
#ifndef XVTM_USE_XMEM
		xvmem_status_t errCode;
      buffOut = xvmem_alloc(&(pxvTM->memBankMgr[buffColor]), (uint32_t) buffSize, (uint32_t) buffAlignment, &errCode);
#else

	  xmem_bank_status_t xmbs;
	  buffOut = xmem_bank_alloc(buffColor, buffSize, buffAlignment, &xmbs);
#endif
  }
  if (buffOut == NULL)
  {
    pxvTM->errFlag = XV_ERROR_ALLOC_FAILED;
    return((void *) XVTM_ERROR);
  }
#ifdef FIK_FRAMEWORK
  pxvTM->allocatedList[pxvTM->allocationsIdx] = buffOut;
  pxvTM->allocationsIdx++;
  pxvTM->allocationsIdx = pxvTM->allocationsIdx % XV_MAX_ALLOCATIONS;
#endif
  return(buffOut);
}

/**********************************************************************************
 * FUNCTION: xvFreeBuffer()
 *
 * DESCRIPTION:
 *     Releases the given buffer.
 *
 * INPUTS:
 *     xvTileManager *pxvTM      Tile Manager object
 *     void          *pBuff      Buffer that needs to be released
 *
 * OUTPUTS:
 *     Returns XVTM_ERROR if it encounters an error, else returns XVTM_SUCCESS
 *
 ********************************************************************************** */

int32_t xvFreeBuffer(xvTileManager  *pxvTM, void * pBuff)
{
  int32_t index;
  XV_CHECK_ERROR_NULL(pxvTM == NULL, XVTM_ERROR, "NULL TM Pointer");
  pxvTM->errFlag = XV_ERROR_SUCCESS;
  XV_CHECK_ERROR(pBuff == NULL, pxvTM->errFlag = XV_ERROR_BUFFER_NULL, XVTM_ERROR, "XV_ERROR_BUFFER_NULL");
#ifndef XVTM_USE_XMEM
  for (index = 0; index < pxvTM->numMemBanks; index++) {
    if ((pxvTM->pMemBankStart[index] <= pBuff) && (pBuff < (void *) (&((uint8_t *) pxvTM->pMemBankStart[index])[pxvTM->memBankSize[index]]))) {
      xvmem_free(&(pxvTM->memBankMgr[index]), pBuff);
      return(XVTM_SUCCESS);
    }
  }
#else
  int32_t numMemBanks = xmem_bank_get_num_banks();
  for (index = 0; index < numMemBanks; index++) {
    if (xmem_bank_check_bounds(index,pBuff)==XMEM_BANK_OK) {
    	xmem_bank_status_t xmbs;
    	uint32_t a = (uint32_t)pBuff;
    	void* p = (void*)a;
    	xmbs = xmem_bank_free(index, p);
    	XV_CHECK_ERROR_NULL(xmbs != XMEM_BANK_OK, XVTM_ERROR, "xmem error");
    	return(XVTM_SUCCESS);
    }
  }
#endif
  pxvTM->errFlag = XV_ERROR_BAD_ARG;
  return(XVTM_ERROR);
}

/**********************************************************************************
 * FUNCTION: xvFreeAllBuffers()
 *
 * DESCRIPTION:
 *     Releases all buffers. Reinitializes the memory allocator
 *
 * INPUTS:
 *     xvTileManager *pxvTM      Tile Manager object
 *
 * OUTPUTS:
 *     Returns XVTM_ERROR if it encounters an error, else returns XVTM_SUCCESS
 *
 ********************************************************************************** */
#ifndef XVTM_USE_XMEM
int32_t xvFreeAllBuffers(xvTileManager *pxvTM)
{
  XV_CHECK_ERROR_NULL(pxvTM == NULL, XVTM_ERROR, "NULL TM Pointer");
  pxvTM->errFlag = XV_ERROR_SUCCESS;

  int32_t bankIndex;
#ifndef XVTM_USE_XMEM
  int32_t numMemBanks = pxvTM->numMemBanks;
#else
  int32_t numMemBanks = xmem_bank_get_num_banks();
#endif


  // Resetting all memory manager related elements
  for (bankIndex = 0; bankIndex < numMemBanks; bankIndex++)
  {
	int32_t buffPoolSize;
	xvmem_mgr_t *pmemBankMgr;
	void *pBankBuffPool;
    pmemBankMgr   = &(pxvTM->memBankMgr[bankIndex]);
    pBankBuffPool = pxvTM->pMemBankStart[bankIndex];
    buffPoolSize  = pxvTM->memBankSize[bankIndex];
    // Type cast to void to avoid MISRA 17.7 violation
    (void) xvmem_init(pmemBankMgr, pBankBuffPool, (uint32_t) buffPoolSize, 0u, NULL);
  }

  return(XVTM_SUCCESS);
}
#endif

/**********************************************************************************
 * FUNCTION: xvAllocateFrame()
 *
 * DESCRIPTION:
 *     Allocates single frame. It does not allocate buffer required for frame data.
 *
 * INPUTS:
 *     xvTileManager *pxvTM      Tile Manager object
 *
 * OUTPUTS:
 *     Returns the pointer to allocated frame.
 *     Returns ((xvFrame *)(XVTM_ERROR)) if it encounters an error.
 *     Does not allocate frame data buffer.
 *
 ********************************************************************************** */

xvFrame *xvAllocateFrame(xvTileManager *pxvTM)
{
  uint32_t indx, indxArr = 0, indxShift = 0, allocFlags = 0;
  xvFrame *pFrame = NULL;
  XV_CHECK_ERROR_NULL(pxvTM == NULL, (xvFrame *) ((void *) (XVTM_ERROR)), "NULL TM Pointer");
  pxvTM->errFlag = XV_ERROR_SUCCESS;

  for (indx = 0; indx < MAX_NUM_FRAMES; indx++) {
    indxArr    = indx >> 5u;
    indxShift  = indx & 0x1Fu;
    allocFlags = pxvTM->frameAllocFlags[indxArr];
    if (((allocFlags >> indxShift) & 0x1u) == 0u) {
      break;
    }
  }

  if (indx < MAX_NUM_FRAMES) {
    pFrame                          = &(pxvTM->frameArray[indx]);
    pxvTM->frameAllocFlags[indxArr] = (allocFlags | (((uint32_t) 0x1u) << indxShift));
    pxvTM->frameCount++;
  }
  else  {
    pxvTM->errFlag = XV_ERROR_FRAME_BUFFER_FULL;
    return((xvFrame *) ((void *) XVTM_ERROR));
  }

  return(pFrame);
}

/**********************************************************************************
 * FUNCTION: xvFreeFrame()
 *
 * DESCRIPTION:
 *     Releases the given frame. Does not release associated frame data buffer.
 *
 * INPUTS:
 *     xvTileManager * pxvTM      Tile Manager object
 *     xvFrame const * pFrame     Frame that needs to be released
 *
 * OUTPUTS:
 *     Returns XVTM_ERROR if it encounters an error, else returns XVTM_SUCCESS
 *
 ********************************************************************************** */

int32_t xvFreeFrame(xvTileManager *pxvTM, xvFrame const * pFrame)
{
  uint32_t indx, indxArr = 0, indxShift = 0, allocFlags = 0;

  XV_CHECK_ERROR_NULL(pxvTM == NULL, XVTM_ERROR, "NULL TM Pointer");
  pxvTM->errFlag = XV_ERROR_SUCCESS;
  XV_CHECK_ERROR(pFrame == NULL, pxvTM->errFlag = XV_ERROR_FRAME_NULL, XVTM_ERROR, "XV_ERROR_FRAME_NULL");
  for (indx = 0; indx < MAX_NUM_FRAMES; indx++) {
    if (&(pxvTM->frameArray[indx]) == pFrame) {
      break;
    }
  }

  if (indx < MAX_NUM_FRAMES) {
    indxArr                         = indx >> 5u;
    indxShift                       = indx & 0x1Fu;
    allocFlags                      = pxvTM->frameAllocFlags[indxArr];
    pxvTM->frameAllocFlags[indxArr] = (allocFlags & ~(0x1u << indxShift));
    pxvTM->frameCount--;
  }
  else  {
    pxvTM->errFlag = XV_ERROR_BAD_ARG;
    return(XVTM_ERROR);
  }
  return(XVTM_SUCCESS);
}

/**********************************************************************************
 * FUNCTION: xvFreeAllFrames()
 *
 * DESCRIPTION:
 *     Releases all allocated frames.
 *
 * INPUTS:
 *     xvTileManager *pxvTM      Tile Manager object
 *
 * OUTPUTS:
 *     Returns XVTM_ERROR if it encounters an error, else returns XVTM_SUCCESS
 *
 ********************************************************************************** */

int32_t xvFreeAllFrames(xvTileManager *pxvTM)
{
  uint32_t index;
  XV_CHECK_ERROR_NULL(pxvTM == NULL, XVTM_ERROR, "NULL TM Pointer");
  pxvTM->errFlag = XV_ERROR_SUCCESS;

  pxvTM->frameCount = 0;
  for (index = 0u; index < ((MAX_NUM_FRAMES + 31u) / 32u); index++) {
    pxvTM->frameAllocFlags[index] = 0x00000000;
  }
  return(XVTM_SUCCESS);
}

/**********************************************************************************
 * FUNCTION: xvAllocateTile()
 *
 * DESCRIPTION:
 *     Allocates single tile. It does not allocate buffer required for tile data.
 *
 * INPUTS:
 *     xvTileManager *pxvTM      Tile Manager object
 *
 * OUTPUTS:
 *     Returns the pointer to allocated tile.
 *     Returns ((xvTile *)(XVTM_ERROR)) if it encounters an error.
 *     Does not allocate tile data buffer
 *
 ********************************************************************************** */

xvTile *xvAllocateTile(xvTileManager *pxvTM)
{
  uint32_t indx, indxArr = 0, indxShift = 0, allocFlags = 0;
  xvTile *pTile = NULL;
  XV_CHECK_ERROR_NULL(pxvTM == NULL, (xvTile *) ((void *) XVTM_ERROR), "NULL TM Pointer");

  pxvTM->errFlag = XV_ERROR_SUCCESS;

  for (indx = 0; indx < MAX_NUM_TILES; indx++) {
    indxArr    = indx >> 5u;
    indxShift  = indx & 0x1Fu;
    allocFlags = pxvTM->tileAllocFlags[indxArr];
    if (((allocFlags >> indxShift) & 0x1u) == 0u) {
      break;
    }
  }

  if (indx < MAX_NUM_TILES) {
    pTile                          = &(pxvTM->tileArray[indx]);
    pxvTM->tileAllocFlags[indxArr] = allocFlags | (((uint32_t) 0x1u) << indxShift);
    pxvTM->tileCount++;
  }
  else  {
    pxvTM->errFlag = XV_ERROR_TILE_BUFFER_FULL;
    return((xvTile *) ((void *) XVTM_ERROR));
  }

  return(pTile);
}

/**********************************************************************************
 * FUNCTION: xvFreeTile()
 *
 * DESCRIPTION:
 *     Releases the given tile. Does not release associated tile data buffer.
 *
 * INPUTS:
 *     xvTileManager *pxvTM      Tile Manager object
 *     xvTile const * pTile      Tile that needs to be released
 *
 * OUTPUTS:
 *     Returns XVTM_ERROR if it encounters an error, else returns XVTM_SUCCESS
 *
 ********************************************************************************** */
int32_t xvFreeTile(xvTileManager *pxvTM, xvTile const * pTile)
{
  uint32_t indx, indxArr, indxShift, allocFlags;

  XV_CHECK_ERROR_NULL(pxvTM == NULL, XVTM_ERROR, "NULL TM Pointer");

  pxvTM->errFlag = XV_ERROR_SUCCESS;
  if (pTile == NULL) {
    pxvTM->errFlag = XV_ERROR_TILE_NULL;
    return(XVTM_ERROR);
  }

  for (indx = 0u; indx < MAX_NUM_TILES; indx++) {
    if (&(pxvTM->tileArray[indx]) == pTile) {
      break;
    }
  }

  if (indx < MAX_NUM_TILES) {
    indxArr                        = indx >> 5u;
    indxShift                      = indx & 0x1Fu;
    allocFlags                     = pxvTM->tileAllocFlags[indxArr];
    pxvTM->tileAllocFlags[indxArr] = allocFlags & ~(((uint32_t) 0x1u) << indxShift);
    pxvTM->tileCount--;
  }
  else  {
    pxvTM->errFlag = XV_ERROR_BAD_ARG;
    return(XVTM_ERROR);
  }
  return(XVTM_SUCCESS);
}

/**********************************************************************************
 * FUNCTION: xvFreeAllTiles()
 *
 * DESCRIPTION:
 *     Releases all allocated tiles.
 *
 * INPUTS:
 *     xvTileManager *pxvTM      Tile Manager object
 *
 * OUTPUTS:
 *     Returns XVTM_ERROR if it encounters an error, else returns XVTM_SUCCESS
 *
 ********************************************************************************** */

int32_t xvFreeAllTiles(xvTileManager *pxvTM)
{
  uint32_t index;

  XV_CHECK_ERROR_NULL(pxvTM == NULL, XVTM_ERROR, "NULL TM Pointer");
  pxvTM->errFlag = XV_ERROR_SUCCESS;

  pxvTM->tileCount = 0;
  for (index = 0u; index < ((MAX_NUM_TILES + 31u) / 32u); index++) {
    pxvTM->tileAllocFlags[index] = (uint32_t) 0x00000000u;
  }
  return(XVTM_SUCCESS);
}

/**********************************************************************************
 * FUNCTION: xvCreateTileManager()
 *
 * DESCRIPTION:
 *     Creates and initializes Tile Manager, Memory Allocator and iDMA.
 *
 * INPUTS:
 *     xvTileManager        *pxvTM              Tile Manager object
 *     idma_buffer_t        *buf0               iDMA object. It should be initialized before calling this function. Contains descriptors and idma library object
 *     int32_t              numMemBanks         Number of memory pools
 *     void                 **pBankBuffPool     Array of memory pool start address
 *     int32_t              *buffPoolSize       Array of memory pool sizes
 *     idma_err_callback_fn errCallbackFunc     Callback for dma transfer error
 *     idma_callback_fn     intrCallbackFunc    Callback for dma transfer completion
 *     void                 *cbData             Data needed for completion callback function
 *     int32_t              descCount           Number of descriptors that can be added in buffer
 *     int32_t              maxBlock            Maximum block size allowed
 *     int32_t              numOutReq           Maximum number of outstanding pif requests
 *
 *
 * OUTPUTS:
 *     Returns XVTM_ERROR if it encounters an error, else returns XVTM_SUCCESS
 *
 ********************************************************************************** */

int32_t xvCreateTileManager(xvTileManager *pxvTM, void *buf0, int32_t numMemBanks, void **pBankBuffPool, int32_t* buffPoolSize,
                            idma_err_callback_fn errCallbackFunc, idma_callback_fn intrCallbackFunc, void *cbData, int32_t descCount, int32_t maxBlock, int32_t numOutReq)
{
  int32_t retVal;

  retVal = xvInitTileManager(pxvTM, (idma_buffer_t *) buf0);
  XV_CHECK_ERROR_NULL(retVal == XVTM_ERROR, XVTM_ERROR, "XVTM_ERROR");
#ifndef XVTM_USE_XMEM
  retVal = xvInitMemAllocator(pxvTM, numMemBanks, pBankBuffPool, buffPoolSize);
  XV_CHECK_ERROR_NULL(retVal == XVTM_ERROR, XVTM_ERROR, "XVTM_ERROR");
#else
  (void)numMemBanks;
  (void)pBankBuffPool;
  (void)buffPoolSize;
#endif
  retVal = xvInitIdma(pxvTM, (idma_buffer_t *) buf0, descCount, maxBlock, numOutReq, errCallbackFunc, intrCallbackFunc, cbData);
  XV_CHECK_ERROR_NULL(retVal == XVTM_ERROR, XVTM_ERROR, "XVTM_ERROR");
  return(retVal);
}

/**********************************************************************************
 * FUNCTION: xvCreateFrame()
 *
 * DESCRIPTION:
 *     Allocates single frame. It does not allocate buffer required for frame data.
 *     Initializes the frame elements
 *
 * INPUTS:
 *     xvTileManager *pxvTM          Tile Manager object
 *     void          *imgBuff        Pointer to image buffer
 *     uint32_t      frameBuffSize   Size of allocated image buffer
 *     int32_t       width           Width of image
 *     int32_t       height          Height of image
 *     int32_t       pitch           Pitch of image
 *     uint8_t       pixRes          Pixel resolution of image in bytes
 *     uint8_t       numChannels     Number of channels in the image
 *     uint8_t       paddingtype     Supported padding type
 *     uint32_t      paddingVal      Padding value if padding type is edge extension
 *
 * OUTPUTS:
 *     Returns the pointer to allocated frame.
 *     Returns ((xvFrame *)(XVTM_ERROR)) if it encounters an error.
 *     Does not allocate frame data buffer.
 *
 ********************************************************************************** */

xvFrame *xvCreateFrame(xvTileManager *pxvTM, void *imgBuff, uint32_t frameBuffSize, int32_t width, int32_t height, int32_t pitch, uint8_t pixRes, uint8_t numChannels, uint8_t paddingtype, uint32_t paddingVal)
{
  XV_CHECK_ERROR_NULL(((pxvTM == NULL) || (imgBuff == NULL)), ((xvFrame *) ((void *) XVTM_ERROR)), "XVTM_ERROR");
  XV_CHECK_ERROR(((width < 0) || (height < 0) || (pitch < 0)), pxvTM->errFlag = XV_ERROR_BAD_ARG, ((xvFrame *) ((void *) XVTM_ERROR)), "XVTM_ERROR");
  XV_CHECK_ERROR(((width * (int32_t) numChannels) > pitch), pxvTM->errFlag = XV_ERROR_BAD_ARG, ((xvFrame *) ((void *) XVTM_ERROR)), "XVTM_ERROR");
  XV_CHECK_ERROR((frameBuffSize < ((uint32_t) pitch * (uint32_t) height * (int32_t) pixRes)), pxvTM->errFlag = XV_ERROR_BAD_ARG, ((xvFrame *) ((void *) XVTM_ERROR)), "XVTM_ERROR");
  XV_CHECK_ERROR(((numChannels > MAX_NUM_CHANNEL) || ((int8_t) numChannels <= 0)), pxvTM->errFlag = XV_ERROR_BAD_ARG, ((xvFrame *) ((void *) XVTM_ERROR)), "XVTM_ERROR");
  XV_CHECK_ERROR((paddingtype > FRAME_PADDING_MAX), pxvTM->errFlag = XV_ERROR_BAD_ARG, ((xvFrame *) ((void *) XVTM_ERROR)), "XVTM_ERROR");
  xvFrame *pFrame = xvAllocateFrame(pxvTM);

  if ((void *) pFrame == (void *) XVTM_ERROR) {
    pxvTM->errFlag = XV_ERROR_BAD_ARG;
    return((xvFrame *) ((void *) XVTM_ERROR));
  }

  SETUP_FRAME(pFrame, imgBuff, frameBuffSize, width, height, pitch, 0, 0, pixRes, numChannels, paddingtype, paddingVal);
  return(pFrame);
}

/**********************************************************************************
 * FUNCTION: xvCreateTile()
 *
 * DESCRIPTION:
 *     Allocates single tile and associated buffer data.
 *     Initializes the elements in tile
 *
 * INPUTS:
 *     xvTileManager *pxvTM          Tile Manager object
 *     int32_t       tileBuffSize    Size of allocated tile buffer
 *     int32_t       width           Width of tile
 *     uint16_t      height          Height of tile
 *     int32_t       pitch           Pitch of tile
 *     uint16_t      edgeWidth       Edge width of tile
 *     uint16_t      edgeHeight      Edge height of tile
 *     int32_t       color           Memory pool from which the buffer should be allocated
 *     xvFrame       *pFrame         Frame associated with the tile
 *     uint16_t      tileType        Type of tile
 *     int32_t       alignType       Alignment tpye of tile. could be edge aligned of data aligned
 *
 * OUTPUTS:
 *     Returns the pointer to allocated tile.
 *     Returns ((xvTile *)(XVTM_ERROR)) if it encounters an error.
 *
 ********************************************************************************** */

xvTile *xvCreateTile(xvTileManager *pxvTM, int32_t tileBuffSize, int32_t width, uint16_t height, int32_t pitch, uint16_t edgeWidth, uint16_t edgeHeight, int32_t color, xvFrame *pFrame, uint16_t xvTileType, int32_t alignType)
{
  XV_CHECK_ERROR_NULL(pxvTM == NULL, (xvTile *) ((void *) XVTM_ERROR), "NULL TM Pointer");
  XV_CHECK_ERROR(((width < 0) || (pitch < 0)), pxvTM->errFlag = XV_ERROR_BAD_ARG, ((xvTile *) ((void *) XVTM_ERROR)), "XVTM_ERROR");
  XV_CHECK_ERROR(((alignType != (int32_t) XVTM_EDGE_ALIGNED_N) && (alignType != (int32_t) XVTM_DATA_ALIGNED_N) && (alignType != (int32_t) XVTM_EDGE_ALIGNED_2N) && (alignType != (int32_t) XVTM_DATA_ALIGNED_2N)), pxvTM->errFlag = XV_ERROR_BAD_ARG, ((xvTile *) ((void *) XVTM_ERROR)), "XVTM_ERROR");
#ifndef XVTM_USE_XMEM
  XV_CHECK_ERROR((((color < (int32_t) 0) || (color >= (int32_t) pxvTM->numMemBanks)) && (color != (int32_t) XV_MEM_BANK_COLOR_ANY)), pxvTM->errFlag = XV_ERROR_BAD_ARG, ((xvTile *) ((void *) XVTM_ERROR)), "XVTM_ERROR");
#else
  XV_CHECK_ERROR((((color < (int32_t) 0) || (color >= (int32_t) xmem_bank_get_num_banks())) && (color != (int32_t) XV_MEM_BANK_COLOR_ANY)), pxvTM->errFlag = XV_ERROR_BAD_ARG, ((xvTile *) ((void *) XVTM_ERROR)), "XVTM_ERROR");
#endif

  int32_t channel     = XV_TYPE_CHANNELS(xvTileType);
  int32_t bytesPerPix = XV_TYPE_ELEMENT_SIZE(xvTileType);
  int32_t bytePerPel;
  bytePerPel = bytesPerPix / channel;

  XV_CHECK_ERROR((((width + (2 * (int32_t) edgeWidth)) * channel) > pitch), pxvTM->errFlag = XV_ERROR_BAD_ARG, ((xvTile *) ((void *) XVTM_ERROR)), "XVTM_ERROR");
  XV_CHECK_ERROR((tileBuffSize < (pitch * ((int32_t) height + (2 * (int32_t) edgeHeight)) * bytePerPel)), pxvTM->errFlag = XV_ERROR_BAD_ARG, ((xvTile *) ((void *) XVTM_ERROR)), "XVTM_ERROR");

  if (pFrame != NULL) {
    XV_CHECK_ERROR(((int32_t) pFrame->pixelRes != bytePerPel), pxvTM->errFlag = XV_ERROR_BAD_ARG , ((xvTile *) ((void *) XVTM_ERROR)), "XVTM_ERROR");
    XV_CHECK_ERROR(((int32_t) pFrame->numChannels != channel), pxvTM->errFlag = XV_ERROR_BAD_ARG , ((xvTile *) ((void *) XVTM_ERROR)), "XVTM_ERROR");
  }
  void *tileBuff = NULL;
  tileBuff = xvAllocateBuffer(pxvTM, tileBuffSize, color, 64);

  if (tileBuff == (void *) XVTM_ERROR) {
    return((xvTile *) ((void *) XVTM_ERROR));
  }

  xvTile *pTile = xvAllocateTile(pxvTM);
  if ((void *) pTile == (void *) XVTM_ERROR) {
    //Type cast to void to avoid MISRA 17.7 violation
    (void) xvFreeBuffer(pxvTM, tileBuff);
    return((xvTile *) ((void *) XVTM_ERROR));
  }

  SETUP_TILE(pTile, tileBuff, tileBuffSize, pFrame, width, height, pitch, xvTileType, edgeWidth, edgeHeight, 0, 0, alignType);
  return(pTile);
}

/**********************************************************************************
 * FUNCTION: xvAddIdmaRequest()
 *
 * DESCRIPTION:
 *     Add iDMA transfer request. Request is scheduled as soon as it is added
 *
 * INPUTS:
 *     xvTileManager *pxvTM                   Tile Manager object
 *     void          * dst                    Pointer to destination buffer
 *     void          * src                    Pointer to source buffer
 *     size_t        rowSize                  Number of bytes to transfer in a row
 *     int32_t       numRows                  Number of rows to transfer
 *     int32_t       srcPitch                 Source buffer's pitch in bytes
 *     int32_t       dstPitch                 Destination buffer's pitch in bytes
 *     int32_t       interruptOnCompletion    If it is set, iDMA will interrupt after completing transfer
 * OUTPUTS:
 *     Returns dmaIndex for this request. It returns XVTM_ERROR if it encounters an error
 *
 ********************************************************************************** */

int32_t xvAddIdmaRequest(xvTileManager *pxvTM, void *dst, void *src, size_t rowSize,
                         int32_t numRows, int32_t srcPitch, int32_t dstPitch, int32_t interruptOnCompletion)
{
  XV_CHECK_ERROR_NULL(pxvTM == NULL, XVTM_ERROR, "NULL TM Pointer");
  pxvTM->errFlag = XV_ERROR_SUCCESS;
  XV_CHECK_ERROR(((dst == NULL) || (src == NULL)), pxvTM->errFlag = XV_ERROR_POINTER_NULL, XVTM_ERROR, "XVTM_ERROR");
  XV_CHECK_ERROR((((int32_t) rowSize <= 0) || (numRows <= 0) || (srcPitch < 0) || (dstPitch < 0) || (interruptOnCompletion < 0)), pxvTM->errFlag = XV_ERROR_BAD_ARG, XVTM_ERROR, "XVTM_ERROR");

  XV_CHECK_ERROR(((((int32_t) rowSize > srcPitch) && (srcPitch != 0)) || (((int32_t) rowSize > dstPitch) && (dstPitch != 0))), pxvTM->errFlag = XV_ERROR_BAD_ARG, XVTM_ERROR, "XVTM_ERROR");

  XV_CHECK_ERROR((!((XV_PTR_START_IN_DRAM(src) && XV_PTR_END_IN_DRAM(((uint8_t *) src) + (srcPitch * numRows))) || \
                    (XV_PTR_START_IN_DRAM(dst) && XV_PTR_END_IN_DRAM(((uint8_t *) dst) + (dstPitch * numRows))))), pxvTM->errFlag = XV_ERROR_BAD_ARG, XVTM_ERROR, "XVTM_ERROR");

  int32_t dmaIndex;
  uint32_t intrCompletionFlag;

  if (interruptOnCompletion != 0) {
    intrCompletionFlag = DESC_NOTIFY_W_INT;
  }
  else  {
    intrCompletionFlag = 0;
  }

  TM_LOG_PRINT("src: %p, dst, %p, rowsize: %d, numRows: %d, srcPitch: %d, dstPitch: %d, flags: 0x%x, ",
               (void*)src, (void*)dst, rowSize, numRows, srcPitch, dstPitch, intrCompletionFlag);

  idma_add_2d_desc(pxvTM->pdmaObj, dst, (void *) src, rowSize, intrCompletionFlag, numRows, srcPitch, dstPitch);
  	dmaIndex = idma_schedule_desc(1);
	
  TM_LOG_PRINT("dmaIndex: %d\n", dmaIndex);
  return(dmaIndex);
}

/**********************************************************************************
 * FUNCTION: addIdmaRequestInline()
 *
 * DESCRIPTION:
 *     Add iDMA transfer request. Request is scheduled as soon as it is added
 *		Made it inline to speed up xvReqTileTransferIn and xvReqTileTransferOut APIs
 * INPUTS:
 *     void          * dst                    Pointer to destination buffer
 *     void          * src                    Pointer to source buffer
 *     size_t        rowSize                  Number of bytes to transfer in a row
 *     int32_t       numRows                  Number of rows to transfer
 *     int32_t       srcPitchBytes            Source buffer's pitch in bytes
 *     int32_t       dstPitchBytes            Destination buffer's pitch in bytes
 *     int32_t       interruptOnCompletion    If it is set, iDMA will interrupt after completing transfer
 * OUTPUTS:
 *     Returns dmaIndex for this request. It returns XVTM_ERROR if it encounters an error
 *
 ********************************************************************************** */

static inline int32_t addIdmaRequestInline(xvTileManager *pxvTM, void *dst, void  *src, size_t rowSize,
                                           int32_t numRows, int32_t srcPitchBytes, int32_t dstPitchBytes, int32_t interruptOnCompletion)

{
  int32_t dmaIndex;
  uint32_t intrCompletionFlag;

  if (interruptOnCompletion != 0) {
    intrCompletionFlag = DESC_NOTIFY_W_INT;
  }
  else  {
    intrCompletionFlag = 0;
  }

  TM_LOG_PRINT("src: %p, dst, %p, rowsize: %d, numRows: %d, srcPitch: %d, dstPitch: %d, flags: 0x%x, ",
               (void*)src, (void*)dst, rowSize, numRows, srcPitchBytes, dstPitchBytes, intrCompletionFlag);

  idma_add_2d_desc(pxvTM->pdmaObj, dst, (void *) src, rowSize, intrCompletionFlag, numRows, srcPitchBytes, dstPitchBytes);
  	dmaIndex = idma_schedule_desc(1);

  TM_LOG_PRINT("dmaIndex: %d\n", dmaIndex);
  return(dmaIndex);
}

/**********************************************************************************
 * FUNCTION: solveForX()
 *
 * DESCRIPTION:
 *     Part of tile reuse. Checks X direction boundary condition and performs DMA transfers
 *
 * INPUTS:
 *     xvTile const * pTile                   Destination tile
 *     uint8_t       *pCurrBuff               Current tile data pointer
 *     uint8_t       *pPrevBuff               Previous tile data pointer
 *     int32_t       y1                       Y co-ordinate-1
 *     int32_t       y2                       Y co-ordinate-2
 *     int32_t       x1                       Current tile Left X co-ordinate
 *     int32_t       x2                       Current tile Right X co-ordinate
 *     int32_t       px1                      Previous tile Left X co-ordinate
 *     int32_t       px2                      Previous tile Right X co-ordinate
 *     int32_t       tp                       Current tile buffer's pitch in bytes
 *     int32_t       ptp                      Previous tile buffer's pitch in bytes
 *     int32_t       interruptOnCompletion    If it is set, iDMA will interrupt after completing transfer
 * OUTPUTS:
 *     Returns dmaIndex for this request.
 *
 ********************************************************************************** */
// Part of tile reuse. Checks X direction boundary condition and performs DMA transfers

static int32_t solveForX(xvTileManager *pxvTM,xvTile const * pTile, uint8_t *pCurrBuff, uint8_t *pPrevBuff,
                         int32_t y1, int32_t y2, int32_t x1, int32_t x2, int32_t px1, int32_t px2, int32_t tp, int32_t ptp, int32_t interruptOnCompletion)

{
  uint8_t pixWidth;
  int32_t dmaIndex, framePitchBytes, bytes;
  int32_t bytesCopy;

  uint8_t *pSrc, *pDst;
  uint8_t *pSrcCopy, *pDstCopy;

  dmaIndex = 0;
  xvFrame *pFrame = pTile->pFrame;
  pixWidth        = pFrame->pixelRes * pFrame->numChannels;
  framePitchBytes = pFrame->framePitch * (int32_t) pFrame->pixelRes;

  // Case 1. Only left most part overlaps
  if ((px1 <= x1) && (px2 < x2)) {
    pSrcCopy  = &pPrevBuff[((x1 - px1) * ((int32_t) pixWidth))];
    pDstCopy  = pCurrBuff;
    bytesCopy = (((px2 - x1) + 1) * ((int32_t) pixWidth));

    dmaIndex  = addIdmaRequestInline(pxvTM, pDstCopy, pSrcCopy, (size_t) bytesCopy, (y2 - y1) + 1, ptp, tp, 0);


    pSrc     = &((uint8_t *) pFrame->pFrameData) [(y1 * framePitchBytes) + ((px2 + 1) * ((int32_t) pixWidth))];
    pDst     = &pCurrBuff[(((px2 - x1) + 1) * ((int32_t) pixWidth))];
    bytes    = (x2 - px2) * ((int32_t) pixWidth);

    dmaIndex  = addIdmaRequestInline(pxvTM, pDst, pSrc, (size_t) bytes, (y2 - y1) + 1, framePitchBytes, tp, interruptOnCompletion);

  }

  // Case 2. Only mid part overlaps
  if ((x1 < px1) && (px2 < x2)) {
    pSrc     = &((uint8_t *) pFrame->pFrameData) [(y1 * framePitchBytes) + (x1 * ((int32_t) pixWidth))];
    pDst     = pCurrBuff;
    bytes    = (px1 - x1) * ((int32_t) pixWidth);

    dmaIndex  = addIdmaRequestInline(pxvTM, pDst, pSrc, (size_t) bytes, (y2 - y1) + 1, framePitchBytes, tp, 0);


    pSrcCopy  = pPrevBuff;
    pDstCopy  = &pCurrBuff[((px1 - x1) * ((int32_t) pixWidth))];
    bytesCopy = ((px2 - px1) + 1) * ((int32_t) pixWidth);

    dmaIndex  = addIdmaRequestInline(pxvTM, pDstCopy, pSrcCopy, (size_t) bytesCopy, (y2 - y1) + 1, ptp, tp, 0);


    pSrc     = &((uint8_t *) pFrame->pFrameData) [(y1 * framePitchBytes) + ((px2 + 1) * ((int32_t) pixWidth))];
    pDst     = &pCurrBuff [(((px2 - x1) + 1) * ((int32_t) pixWidth))];
    bytes    = (x2 - px2) * ((int32_t) pixWidth);

    dmaIndex  = addIdmaRequestInline(pxvTM, pDst, pSrc, (size_t) bytes, (y2 - y1) + 1, framePitchBytes, tp, interruptOnCompletion);

  }

  // Case 3. Only right part overlaps
  if ((x1 < px1) && (x2 <= px2)) {
    pSrc     = &((uint8_t *) pFrame->pFrameData) [(y1 * framePitchBytes) + (x1 * ((int32_t) pixWidth))];
    pDst     = pCurrBuff;
    bytes    = (px1 - x1) * ((int32_t) pixWidth);

    dmaIndex  = addIdmaRequestInline(pxvTM, pDst, pSrc, (size_t) bytes, (y2 - y1) + 1, framePitchBytes, tp, 0);


    pSrcCopy  = pPrevBuff;
    pDstCopy  = &pCurrBuff [((px1 - x1) * ((int32_t) pixWidth))];
    bytesCopy = ((x2 - px1) + 1) * ((int32_t) pixWidth);

    dmaIndex  = addIdmaRequestInline(pxvTM, pDstCopy, pSrcCopy, (size_t) bytesCopy, (y2 - y1) + 1, ptp, tp, interruptOnCompletion);

  }

  // Case 4. All three regions overlaps
  if ((px1 <= x1) && (x2 <= px2)) {
    pSrcCopy  = &pPrevBuff [((x1 - px1) * ((int32_t) pixWidth))];
    pDstCopy  = pCurrBuff;
    bytesCopy = ((x2 - x1) + 1) * ((int32_t) pixWidth);

    dmaIndex  = addIdmaRequestInline(pxvTM, pDstCopy, pSrcCopy, (size_t) bytesCopy, (y2 - y1) + 1, ptp, tp, interruptOnCompletion);

  }

  return(dmaIndex);
}

/**********************************************************************************
 * FUNCTION: xvReqTileTransferIn()
 *
 * DESCRIPTION:
 *     Requests data transfer from frame present in system memory to local tile memory.
 *     If there is an overlap between previous tile and current tile, tile reuse
 *     functionality can be used.
 *
 * INPUTS:
 *     xvTileManager *pxvTM                   Tile Manager object
 *     xvTile        *pTile                   Destination tile
 *     xvTile        *pPrevTile               Data is copied from this tile to pTile if the buffer overlaps
 *     int32_t       interruptOnCompletion    If it is set, iDMA will interrupt after completing transfer
 *
 * OUTPUTS:
 *     Returns XVTM_ERROR if it encounters an error, else it returns XVTM_SUCCESS
 *
 ********************************************************************************** */

int32_t xvReqTileTransferIn(xvTileManager *pxvTM, xvTile *pTile, xvTile *pPrevTile, int32_t interruptOnCompletion)
{
  xvFrame *pFrame;
  int32_t frameWidth, frameHeight, framePitchBytes, tileWidth, tilePitchBytes;
  uint32_t tileHeight, statusFlag;
  int32_t x1, y1, x2, y2, dmaHeight, dmaWidthBytes, dmaIndex = 0;
  uint32_t tileIndex;
  uint8_t framePadLeft, framePadRight, framePadTop, framePadBottom;
  uint16_t tileEdgeLeft, tileEdgeRight, tileEdgeTop, tileEdgeBottom;
  int32_t extraEdgeTop, extraEdgeBottom, extraEdgeLeft, extraEdgeRight;
  int32_t pixWidth, pixRes;
  uint8_t *srcPtr, *dstPtr, *pPrevBuff, *pCurrBuff, *edgePtr;
  int32_t px1, px2, py1, py2;

  XV_CHECK_ERROR_NULL(pxvTM == NULL, XVTM_ERROR, "NULL TM Pointer");
  pxvTM->errFlag = XV_ERROR_SUCCESS;
  XV_CHECK_ERROR((interruptOnCompletion < 0), pxvTM->errFlag = XV_ERROR_BAD_ARG, XVTM_ERROR, "XV_ERROR_BAD_ARG");
  XV_CHECK_TILE(pTile, pxvTM);
  pFrame = pTile->pFrame;
  XV_CHECK_ERROR(pFrame == NULL, pxvTM->errFlag = XV_ERROR_FRAME_NULL, XVTM_ERROR, "XV_ERROR_FRAME_NULL");
  XV_CHECK_ERROR(((pFrame->pFrameBuff == NULL) || (pFrame->pFrameData == NULL)), pxvTM->errFlag = XV_ERROR_FRAME_NULL, XVTM_ERROR, "XV_ERROR_FRAME_NULL");

  int32_t channel     = XV_TYPE_CHANNELS(XV_TILE_GET_TYPE(pTile));
  int32_t bytesPerPix = XV_TYPE_ELEMENT_SIZE(XV_TILE_GET_TYPE(pTile));
  int32_t bytePerPel;
  bytePerPel = bytesPerPix / channel;

  XV_CHECK_ERROR((((int32_t) pFrame->pixelRes) != bytePerPel), pxvTM->errFlag = XV_ERROR_BAD_ARG, XVTM_ERROR, "XV_ERROR_BAD_ARG");
  XV_CHECK_ERROR((((int32_t) pFrame->numChannels) != channel), pxvTM->errFlag = XV_ERROR_BAD_ARG, XVTM_ERROR, "XV_ERROR_BAD_ARG");

  pTile->pPrevTile  = NULL;
  pTile->reuseCount = 0;
  frameWidth        = pFrame->frameWidth;
  frameHeight       = pFrame->frameHeight;
  framePadLeft      = pFrame->leftEdgePadWidth;
  framePadRight     = pFrame->rightEdgePadWidth;
  framePadTop       = pFrame->topEdgePadHeight;
  framePadBottom    = pFrame->bottomEdgePadHeight;
  pixRes            = (int32_t) pFrame->pixelRes;
  pixWidth          = ((int32_t) pFrame->pixelRes) * ((int32_t) pFrame->numChannels);
  framePitchBytes   = pFrame->framePitch * pixRes;

  tileWidth      = pTile->width;
  tileHeight     = pTile->height;
  tilePitchBytes = pTile->pitch * pixRes;
  tileEdgeLeft   = pTile->tileEdgeLeft;
  tileEdgeRight  = pTile->tileEdgeRight;
  tileEdgeTop    = pTile->tileEdgeTop;
  tileEdgeBottom = pTile->tileEdgeBottom;

  statusFlag = pTile->status;

  // 1. CHECK IF EXTRA PADDING NEEDED
  // Check top and bottom borders
  y1 = pTile->y - ((int32_t) tileEdgeTop);
  if (y1 > (frameHeight + (int32_t) framePadBottom)) {
    y1 = frameHeight + (int32_t) framePadBottom;
  }
  if (y1 < -(int32_t) framePadTop) {
    extraEdgeTop = -(int32_t) framePadTop - y1;
    y1           = -(int32_t) framePadTop;
    statusFlag  |= XV_TILE_STATUS_TOP_EDGE_PADDING_NEEDED;
  }
  else  {
    extraEdgeTop = 0;
  }

  y2 = (int32_t) (pTile->y + ((int32_t) tileHeight - 1) + (int32_t) tileEdgeBottom);
  if (y2 < -(int32_t) framePadTop) {
    y2 = (int32_t) (-(int32_t) framePadTop - 1);
  }
  if (y2 > (((int32_t) frameHeight - 1) + (int32_t) framePadBottom)) {
    extraEdgeBottom = y2 - (((int32_t) frameHeight - 1) + (int32_t) framePadBottom);
    y2              = ((int32_t) frameHeight - 1) + (int32_t) framePadBottom;
    statusFlag     |= XV_TILE_STATUS_BOTTOM_EDGE_PADDING_NEEDED;
  }
  else  {
    extraEdgeBottom = 0;
  }

  // Check left and right borders
  x1 = pTile->x - ((int32_t) tileEdgeLeft);
  if (x1 > (((int32_t) frameWidth) + ((int32_t) framePadRight))) {
    x1 = ((int32_t) frameWidth) + ((int32_t) framePadRight);
  }
  if (x1 < -((int32_t) framePadLeft)) {
    extraEdgeLeft = (-(int32_t) framePadLeft) - x1;
    x1            = -(int32_t) framePadLeft;
    statusFlag   |= XV_TILE_STATUS_LEFT_EDGE_PADDING_NEEDED;
  }
  else  {
    extraEdgeLeft = 0;
  }

  x2 = pTile->x + ((int32_t) tileWidth - 1) + ((int32_t) tileEdgeRight);
  if (x2 < (-(int32_t) framePadLeft)) {
    x2 = (-(int32_t) framePadLeft - 1);
  }
  if (x2 > (((int32_t) frameWidth - 1) + (int32_t) framePadRight)) {
    extraEdgeRight = x2 - (((int32_t) frameWidth - 1) + (int32_t) framePadRight);
    x2             = ((int32_t) frameWidth - 1) + (int32_t) framePadRight;
    statusFlag    |= XV_TILE_STATUS_RIGHT_EDGE_PADDING_NEEDED;
  }
  else  {
    extraEdgeRight = 0;
  }

  // 2. FILL ALL TILE and DMA RELATED DATA
  // No Need to align srcPtr and dstPtr as DMA does not need aligned start
  srcPtr        = &((uint8_t *) pFrame->pFrameData) [(y1 * framePitchBytes) + (x1 * pixWidth)];
  dmaHeight     = (y2 - y1) + 1;
  dmaWidthBytes = ((x2 - x1) + 1) * pixWidth;
  edgePtr       = &((uint8_t *) pTile->pData)[-(((int32_t) tileEdgeTop * tilePitchBytes) + ((int32_t) tileEdgeLeft * (int32_t) pixWidth))];
  dstPtr        = &edgePtr [(extraEdgeTop * tilePitchBytes) + (extraEdgeLeft * pixWidth)]; // For DMA

  // 3. DATA REUSE FROM PREVIOUS TILE
  if ((dmaHeight > 0) && (dmaWidthBytes > 0)) {
    if (pPrevTile != NULL) {
      XV_CHECK_ERROR(!(XV_TILE_IS_TILE(pPrevTile) > 0), (pxvTM)->errFlag = XV_ERROR_BAD_ARG, XVTM_ERROR, "The argument is not a tile");
      XV_CHECK_ERROR(!(XV_TILE_STARTS_IN_DRAM(pPrevTile)), (pxvTM)->errFlag = XV_ERROR_BAD_ARG, XVTM_ERROR, "Data buffer does not start in DRAM");
      XV_CHECK_ERROR(!(XV_TILE_ENDS_IN_DRAM(pPrevTile)), (pxvTM)->errFlag = XV_ERROR_BAD_ARG, XVTM_ERROR, "Data buffer does not fit in DRAM");
      XV_CHECK_ERROR(!(XV_TILE_GET_TYPE(pTile) == XV_TILE_GET_TYPE(pPrevTile)), (pxvTM)->errFlag = XV_ERROR_BAD_ARG, XVTM_ERROR, "Invalid tile types");
      XV_CHECK_ERROR(!(XV_TILE_IS_CONSISTENT(pPrevTile)), (pxvTM)->errFlag = XV_ERROR_BAD_ARG, XVTM_ERROR, "Invalid buffer");


      py1       = pPrevTile->y - ((int32_t) pPrevTile->tileEdgeTop);
      py2       = ((pPrevTile->y + ((int32_t) pPrevTile->height)) - 1) + ((int32_t) pPrevTile->tileEdgeBottom);
      px1       = pPrevTile->x - ((int32_t) pPrevTile->tileEdgeLeft);
      px2       = ((pPrevTile->x + ((int32_t) pPrevTile->width)) - 1) + ((int32_t) pPrevTile->tileEdgeRight);
      pPrevBuff = &((uint8_t *) pPrevTile->pData)[ -((((int32_t) pPrevTile->tileEdgeTop) * ((int32_t) pPrevTile->pitch) * ((int32_t) pixRes)) + (((int32_t) pPrevTile->tileEdgeLeft) * ((int32_t) pixWidth)))]; // Same pixRes for current and prev tile
      // Case 1. Two tiles are totally independent. DMA entire tile. No copying
      if ((py2 < y1) || (py1 > y2) || (px2 < x1) || (px1 > x2)) {

        dmaIndex = addIdmaRequestInline(pxvTM, dstPtr, srcPtr, (size_t) dmaWidthBytes, dmaHeight, framePitchBytes, tilePitchBytes, interruptOnCompletion);

      }
      else  {
        pTile->pPrevTile = pPrevTile;
        pPrevTile->reuseCount++;

        // Case 2. Only top part overlaps.
        if ((py1 <= y1) && (py2 < y2)) {
          // Top part
          pCurrBuff = dstPtr;
          pPrevBuff = &pPrevBuff[(y1 - py1) * pPrevTile->pitch * pixRes];
          // Bottom part
          srcPtr    = &((uint8_t *) pFrame->pFrameData)[(((py2 + 1) * framePitchBytes) + (x1 * ((int32_t) pixWidth)))];
          dstPtr    = &dstPtr[((py2 + 1) - y1) * tilePitchBytes];
          dmaHeight = y2 - py2;  // (y2+1) - (py2+1)
	  
          dmaIndex  = addIdmaRequestInline(pxvTM, dstPtr, srcPtr, (size_t)  dmaWidthBytes, dmaHeight, framePitchBytes, tilePitchBytes, 0);

          // Top part
		  
          dmaIndex = solveForX(pxvTM, pTile, pCurrBuff, pPrevBuff, y1, py2, x1, x2, px1, px2, tilePitchBytes, pPrevTile->pitch * pixRes, interruptOnCompletion);


        }

        // Case 3. Only mid part overlaps
        if ((y1 < py1) && (py2 < y2)) {
          // Top part
          dmaHeight = (py1 + 1) - y1;
		  
          dmaIndex  = addIdmaRequestInline(pxvTM, dstPtr, srcPtr, (size_t)  dmaWidthBytes, dmaHeight, framePitchBytes, tilePitchBytes, 0);

          // Mid overlapping part
          pCurrBuff = &dstPtr[((py1 - y1) * tilePitchBytes)];
          // Bottom part
          srcPtr    = &((uint8_t *) pFrame->pFrameData) [(((py2 + 1) * framePitchBytes) + (x1 * ((int32_t) pixWidth)))];
          dstPtr    = &dstPtr[((py2 + 1) - y1) * tilePitchBytes];
          dmaHeight = y2 - py2;  // (y2+1) - (py2+1)
		  
          dmaIndex  = addIdmaRequestInline(pxvTM, dstPtr, srcPtr, (size_t)  dmaWidthBytes, dmaHeight, framePitchBytes, tilePitchBytes, 0);
          dmaIndex  = solveForX(pxvTM, pTile, pCurrBuff, pPrevBuff, py1, py2, x1, x2, px1, px2, tilePitchBytes, pPrevTile->pitch * pixRes, interruptOnCompletion);


        }

        // Case 4. Only bottom part overlaps
        if ((y1 < py1) && (y2 <= py2)) {
          dmaHeight = py1 - y1;
		  
          dmaIndex  = addIdmaRequestInline(pxvTM, dstPtr, srcPtr, (size_t)  dmaWidthBytes, dmaHeight, framePitchBytes, tilePitchBytes, 0);

          pCurrBuff = &dstPtr[(dmaHeight * tilePitchBytes)];
		  
          dmaIndex  = solveForX(pxvTM, pTile, pCurrBuff, pPrevBuff, py1, y2, x1, x2, px1, px2, tilePitchBytes, pPrevTile->pitch * pixRes, interruptOnCompletion);

        }

        // Case 5. All the parts overlaps
        if ((py1 <= y1) && (y2 <= py2)) {
          pCurrBuff = dstPtr;
          pPrevBuff = &pPrevBuff[(y1 - py1) * pPrevTile->pitch * pixRes];
		  
          dmaIndex  = solveForX(pxvTM, pTile, pCurrBuff, pPrevBuff, y1, y2, x1, x2, px1, px2, tilePitchBytes, pPrevTile->pitch * pixRes, interruptOnCompletion);

        }
      }
    }
    else  {
		  
      dmaIndex = addIdmaRequestInline(pxvTM, dstPtr, srcPtr, (size_t) dmaWidthBytes, dmaHeight, framePitchBytes, tilePitchBytes, interruptOnCompletion);

    }
  }
  else  {
    dmaIndex    = XVTM_DUMMY_DMA_INDEX;
    statusFlag |= XV_TILE_STATUS_DUMMY_IDMA_INDEX_NEEDED;
  }
  pTile->status = statusFlag | XV_TILE_STATUS_DMA_ONGOING;
  tileIndex     = (((uint32_t) (pxvTM->tileDMAstartIndex)) + (((uint32_t) pxvTM->tileDMApendingCount))) % ((uint32_t) MAX_NUM_DMA_QUEUE_LENGTH);
  pxvTM->tileDMApendingCount++;
  pxvTM->tileProcQueue[tileIndex] = pTile;
  pTile->dmaIndex                 = dmaIndex;
  return(XVTM_SUCCESS);
}

/**********************************************************************************
 * FUNCTION: xvReqTileTransferInFast()
 *
 * DESCRIPTION:
 *     Requests 8b data transfer from frame present in system memory to local tile memory.
 *
 * INPUTS:
 *     xvTileManager *pxvTM                   Tile Manager object
 *     xvTile        *pTile                   Destination tile
 *     int32_t       interruptOnCompletion    If it is set, iDMA will interrupt after completing transfer
 *
 * OUTPUTS:
 *     Returns XVTM_ERROR if it encounters an error, else it returns XVTM_SUCCESS
 *
 ********************************************************************************** */

int32_t xvReqTileTransferInFast(xvTileManager *pxvTM, xvTile *pTile, int32_t interruptOnCompletion)
{
  xvFrame *pFrame;
  int32_t frameWidth, frameHeight, framePitchBytes, tileWidth, tilePitchBytes;
  uint32_t tileHeight, statusFlag;
  int32_t x1, y1, x2, y2, dmaHeight, dmaWidthBytes, dmaIndex, copyRowBytes;
  uint16_t tileEdgeLeft, tileEdgeRight, tileEdgeTop, tileEdgeBottom;
  int32_t extraEdgeTop, extraEdgeBottom, extraEdgeLeft, extraEdgeRight;
  uint8_t *srcPtr, *dstPtr, *edgePtr;
  int32_t interruptOnCompletionLocal = interruptOnCompletion;

  int8_t pixWidth, pixRes;

  pxvTM->errFlag = XV_ERROR_SUCCESS;

  pFrame = pTile->pFrame;

  frameWidth      = pFrame->frameWidth;
  frameHeight     = pFrame->frameHeight;
  pixRes          = (int8_t) pFrame->pixelRes;
  pixWidth        = (int8_t) ((int8_t)pFrame->pixelRes * (int32_t) pFrame->numChannels);
  framePitchBytes = pFrame->framePitch * pixRes;

  tileWidth      = pTile->width;
  tileHeight     = pTile->height;
  tilePitchBytes = pTile->pitch * pixRes;
  tileEdgeLeft   = pTile->tileEdgeLeft;
  tileEdgeRight  = pTile->tileEdgeRight;
  tileEdgeTop    = pTile->tileEdgeTop;
  tileEdgeBottom = pTile->tileEdgeBottom;

  statusFlag      = pTile->status;
  extraEdgeTop    = 0;
  extraEdgeBottom = 0;
  extraEdgeLeft   = 0;
  extraEdgeRight  = 0;
  // 1. CHECK IF EXTRA PADDING NEEDED
  // Check top and bottom borders
  y1 = pTile->y - (int32_t) tileEdgeTop;
  if (y1 > frameHeight) {
    y1 = frameHeight;
  }
  if (y1 < 0) {
    extraEdgeTop = -y1;
    y1           = 0;
    statusFlag  |= XV_TILE_STATUS_TOP_EDGE_PADDING_NEEDED;
  }

  y2 = ((int32_t) pTile->y) + (((int32_t) tileHeight) - 1) + (((int32_t) tileEdgeBottom));
  if (y2 < 0) {
    y2 = -1;
  }
  if (y2 > (frameHeight - 1)) {
    extraEdgeBottom = (y2 - frameHeight) + 1;
    y2              = frameHeight - 1;
    statusFlag     |= XV_TILE_STATUS_BOTTOM_EDGE_PADDING_NEEDED;
  }

  // Check left and right borders
  x1 = pTile->x - (int32_t) tileEdgeLeft;
  if (x1 > frameWidth) {
    x1 = frameWidth;
  }
  if (x1 < 0) {
    extraEdgeLeft = -x1;
    x1            = 0;
    statusFlag   |= XV_TILE_STATUS_LEFT_EDGE_PADDING_NEEDED;
  }

  x2 = pTile->x + ((int32_t) tileWidth - 1) + ((int32_t) tileEdgeRight);
  if (x2 < 0) {
    x2 = -1;
  }
  if (x2 > (frameWidth - 1)) {
    extraEdgeRight = (x2 - frameWidth) + 1;
    x2             = frameWidth - 1;
    statusFlag    |= XV_TILE_STATUS_RIGHT_EDGE_PADDING_NEEDED;
  }

  // 2. FILL ALL TILE and DMA RELATED DATA
  // No Need to align srcPtr as DMA does not need aligned start
  // But dstPtr needs to be aligned
  dmaHeight     = (y2 - y1) + 1;
  dmaWidthBytes = ((x2 - x1) + 1) * pixWidth;

  interruptOnCompletionLocal = (interruptOnCompletionLocal != 0) ? DESC_NOTIFY_W_INT : 0;
  int32_t intrCompletionFlag;

  if ((dmaHeight > 0) && (dmaWidthBytes > 0)) {
    srcPtr  = &((uint8_t *) pFrame->pFrameData)[(y1 * framePitchBytes) + (x1 * pixWidth)];
    edgePtr = &((uint8_t *) pTile->pData)[-((((int32_t) tileEdgeTop) * tilePitchBytes) + ((int32_t) tileEdgeLeft * pixWidth))];
    dstPtr  = &edgePtr[((extraEdgeTop * tilePitchBytes) + ((int32_t) extraEdgeLeft * pixWidth))];         // For DMA

    // One interrupt per tile request. Interrupt only if it is last DMA request for this tile.
    intrCompletionFlag = interruptOnCompletionLocal * ((((statusFlag & (XV_TILE_STATUS_TOP_EDGE_PADDING_NEEDED | XV_TILE_STATUS_BOTTOM_EDGE_PADDING_NEEDED)) != 0u) && (pFrame->paddingType == FRAME_EDGE_PADDING)) ?  0 : 1);
    TM_LOG_PRINT("src: %p, dst, %p, rowsize: %d, numRows: %d, srcPitch: %d, dstPitch: %d, flags: 0x%x, ",
                 (void*)srcPtr, (void*)dstPtr, dmaWidthBytes, dmaHeight, framePitchBytes, tilePitchBytes, 0);

    idma_add_2d_desc(pxvTM->pdmaObj, dstPtr, srcPtr, dmaWidthBytes, (uint32_t) intrCompletionFlag, dmaHeight, framePitchBytes, tilePitchBytes);
	dmaIndex = idma_schedule_desc(1);

    TM_LOG_PRINT("dmaIndex: %d\n", dmaIndex);
    if (((statusFlag & XV_TILE_STATUS_TOP_EDGE_PADDING_NEEDED) != 0u) && (pFrame->paddingType == FRAME_EDGE_PADDING)) {
      srcPtr       = &((uint8_t *) pTile->pData)[ -(((int32_t) tileEdgeTop - (int32_t) extraEdgeTop) * tilePitchBytes) - ((int32_t) tileEdgeLeft * pixWidth)];
      dstPtr       = &srcPtr[ -(extraEdgeTop * tilePitchBytes)];
      copyRowBytes = (((int32_t) tileEdgeLeft) + tileWidth + ((int32_t) tileEdgeRight)) * pixWidth;

      // One interrupt per tile request. Interrupt only if it is last DMA request for this tile.
      intrCompletionFlag = interruptOnCompletionLocal * (((statusFlag & XV_TILE_STATUS_BOTTOM_EDGE_PADDING_NEEDED) == 0u) ? 1 : 0);
      TM_LOG_PRINT("src: %p, dst, %p, rowsize: %d, numRows: %d, srcPitch: %d, dstPitch: %d, flags: 0x%x, ",
                   (void*)srcPtr, (void*)dstPtr, copyRowBytes, extraEdgeTop, 0, tilePitchBytes, 0);

      idma_add_2d_desc(pxvTM->pdmaObj, dstPtr, srcPtr, copyRowBytes, (uint32_t) intrCompletionFlag, extraEdgeTop, 0, tilePitchBytes);
     	dmaIndex = idma_schedule_desc(1);

      TM_LOG_PRINT("dmaIndex: %d\n", dmaIndex);
      statusFlag = statusFlag & ~XV_TILE_STATUS_TOP_EDGE_PADDING_NEEDED;
    }

    if (((statusFlag & XV_TILE_STATUS_BOTTOM_EDGE_PADDING_NEEDED) != 0u) && (pFrame->paddingType == FRAME_EDGE_PADDING)) {
      srcPtr       = &((uint8_t *) pTile->pData)[((((int32_t) tileHeight + (int32_t) tileEdgeBottom) - (int32_t) extraEdgeBottom - 1) * tilePitchBytes) - ((int32_t) tileEdgeLeft * pixWidth)];
      dstPtr       = &srcPtr[tilePitchBytes];
      copyRowBytes = (((int32_t) tileEdgeLeft) + tileWidth + ((int32_t) tileEdgeRight)) * pixWidth;
      TM_LOG_PRINT("src: %p, dst, %p, rowsize: %d, numRows: %d, srcPitch: %d, dstPitch: %d, flags: 0x%x, ",
                   (void*)srcPtr, (void*)dstPtr, copyRowBytes, extraEdgeBottom, 0, tilePitchBytes, 0);

      idma_add_2d_desc(pxvTM->pdmaObj, dstPtr, srcPtr, copyRowBytes, (uint32_t) interruptOnCompletionLocal, extraEdgeBottom, 0, tilePitchBytes);
           	dmaIndex = idma_schedule_desc(1);

      TM_LOG_PRINT("dmaIndex: %d\n", dmaIndex);
      statusFlag = statusFlag & ~XV_TILE_STATUS_BOTTOM_EDGE_PADDING_NEEDED;
    }
  }
  else  {
    dmaIndex    = XVTM_DUMMY_DMA_INDEX;
    statusFlag |= XV_TILE_STATUS_DUMMY_IDMA_INDEX_NEEDED;
  }
  pTile->status   = statusFlag | XV_TILE_STATUS_DMA_ONGOING;
  pTile->dmaIndex = dmaIndex;
  return(XVTM_SUCCESS);
}

/**********************************************************************************
 * FUNCTION: xvReqTileTransferInFast16()
 *
 * DESCRIPTION:
 *     Requests 16b data transfer from frame present in system memory to local tile memory.
 *
 * INPUTS:
 *     xvTileManager *pxvTM                   Tile Manager object
 *     xvTile        *pTile                   Destination tile
 *     int32_t       interruptOnCompletion    If it is set, iDMA will interrupt after completing transfer
 *
 * OUTPUTS:
 *     Returns XVTM_ERROR if it encounters an error, else it returns XVTM_SUCCESS
 *
 ********************************************************************************** */

int32_t xvReqTileTransferInFast16(xvTileManager *pxvTM, xvTile *pTile, int32_t interruptOnCompletion)
{

  return(xvReqTileTransferInFast(pxvTM, pTile, interruptOnCompletion));

}

/**********************************************************************************
 * FUNCTION: xvReqTileTransferOut()
 *
 * DESCRIPTION:
 *     Requests data transfer from tile present in local memory to frame in system memory.
 *
 * INPUTS:
 *     xvTileManager *pxvTM                   Tile Manager object
 *     xvTile        *pTile                   Source tile
 *     int32_t       interruptOnCompletion    If it is set, iDMA will interrupt after completing transfer
 *
 * OUTPUTS:
 *     Returns XVTM_ERROR if it encounters an error, else it returns XVTM_SUCCESS
 *
 ********************************************************************************** */

int32_t xvReqTileTransferOut(xvTileManager *pxvTM, xvTile *pTile, int32_t interruptOnCompletion)
{
  xvFrame *pFrame;
  uint8_t *srcPtr, *dstPtr, pixWidth;
  uint32_t srcHeight;
  int32_t tileIndex, dmaIndex;
  int32_t srcWidth, srcPitchBytes;
  int32_t dstPitchBytes, numRows, rowSize;

  XV_CHECK_ERROR_NULL(pxvTM == NULL, XVTM_ERROR, "NULL TM Pointer");
  pxvTM->errFlag = XV_ERROR_SUCCESS;
  XV_CHECK_ERROR((interruptOnCompletion < 0), pxvTM->errFlag = XV_ERROR_BAD_ARG, XVTM_ERROR, "XV_ERROR_BAD_ARG");
  XV_CHECK_TILE(pTile, pxvTM);
  pFrame = pTile->pFrame;
  XV_CHECK_ERROR(pFrame == NULL, pxvTM->errFlag = XV_ERROR_FRAME_NULL, XVTM_ERROR, "XV_ERROR_FRAME_NULL");
  XV_CHECK_ERROR(((pFrame->pFrameBuff == NULL) || (pFrame->pFrameData == NULL)), pxvTM->errFlag = XV_ERROR_FRAME_NULL, XVTM_ERROR, "XV_ERROR_FRAME_NULL");

  int32_t channel     = XV_TYPE_CHANNELS(XV_TILE_GET_TYPE(pTile));
  int32_t bytesPerPix = XV_TYPE_ELEMENT_SIZE(XV_TILE_GET_TYPE(pTile));
  int32_t bytePerPel;
  bytePerPel = bytesPerPix / channel;

  XV_CHECK_ERROR(((int32_t) pFrame->pixelRes != bytePerPel), pxvTM->errFlag = XV_ERROR_BAD_ARG, XVTM_ERROR, "XV_ERROR_BAD_ARG");
  XV_CHECK_ERROR(((int32_t) pFrame->numChannels != channel), pxvTM->errFlag = XV_ERROR_BAD_ARG, XVTM_ERROR, "XV_ERROR_BAD_ARG");

  pixWidth      = pFrame->pixelRes * pFrame->numChannels;
  srcHeight     = pTile->height;
  srcWidth      = pTile->width;
  srcPitchBytes = (int32_t) pTile->pitch * (int32_t) pFrame->pixelRes;
  dstPitchBytes = (int32_t) pFrame->framePitch * (int32_t) pFrame->pixelRes;
  numRows       = XVTM_MIN((int32_t) srcHeight, pFrame->frameHeight - pTile->y);
  rowSize       = XVTM_MIN(srcWidth, (pFrame->frameWidth - pTile->x));

  srcPtr = (uint8_t *) pTile->pData;
  dstPtr = &((uint8_t *) pFrame->pFrameData) [(pTile->y * dstPitchBytes) + (pTile->x * (int32_t) pixWidth)];

  if (pTile->x < 0) {
    rowSize += pTile->x;       // x is negative;
    srcPtr   = &srcPtr[(-pTile->x * (int32_t) pixWidth)];
    dstPtr   = &dstPtr[(-pTile->x * (int32_t) pixWidth)];
  }

  if (pTile->y < 0) {
    numRows += pTile->y;       // y is negative;
    srcPtr   = &srcPtr[(-pTile->y * srcPitchBytes)];
    dstPtr   = &dstPtr[(-pTile->y * dstPitchBytes)];
  }
  rowSize *= (int32_t) pixWidth;

  if ((rowSize != 0) && (numRows != 0)) {
    pTile->status = pTile->status | XV_TILE_STATUS_DMA_ONGOING;
    tileIndex     = (((int32_t) pxvTM->tileDMAstartIndex) + ((int32_t) pxvTM->tileDMApendingCount)) % ((int32_t) MAX_NUM_DMA_QUEUE_LENGTH);
    pxvTM->tileDMApendingCount++;
    pxvTM->tileProcQueue[tileIndex] = pTile;

    dmaIndex                        = addIdmaRequestInline(pxvTM, dstPtr, srcPtr, (uint32_t) rowSize, numRows, srcPitchBytes, dstPitchBytes, interruptOnCompletion);

    pTile->dmaIndex                 = dmaIndex;
  }
  return(XVTM_SUCCESS);
}

/**********************************************************************************
 * FUNCTION: xvReqTileTransferOutFast()
 *
 * DESCRIPTION:
 *     Requests data transfer from tile present in local memory to frame in system memory.
 *
 * INPUTS:
 *     xvTileManager *pxvTM                   Tile Manager object
 *     xvTile        *pTile                   Source tile
 *     int32_t       interruptOnCompletion    If it is set, iDMA will interrupt after completing transfer
 *
 * OUTPUTS:
 *     Returns XVTM_ERROR if it encounters an error, else it returns XVTM_SUCCESS
 *
 ********************************************************************************** */

int32_t xvReqTileTransferOutFast(xvTileManager *pxvTM, xvTile *pTile, int32_t interruptOnCompletion)
{
  xvFrame *pFrame;
  uint8_t *srcPtr, *dstPtr;
  int32_t dmaIndex, pixWidth;
  uint32_t srcHeight;
  int32_t srcWidth, srcPitchBytes;
  int32_t dstPitchBytes, numRows, rowSize;

  pxvTM->errFlag = XV_ERROR_SUCCESS;

  pFrame        = pTile->pFrame;
  pixWidth      = (int32_t) pFrame->pixelRes * (int32_t) pFrame->numChannels;
  srcHeight     = pTile->height;
  srcWidth      = pTile->width;
  srcPitchBytes = pTile->pitch * (int32_t) pFrame->pixelRes;
  dstPitchBytes = pFrame->framePitch * (int32_t) pFrame->pixelRes;
  numRows       = XVTM_MIN((int32_t) srcHeight, pFrame->frameHeight - pTile->y);
  rowSize       = XVTM_MIN(srcWidth, (pFrame->frameWidth - pTile->x));

  srcPtr = (uint8_t *) pTile->pData;
  dstPtr = &((uint8_t *) pFrame->pFrameData) [(pTile->y * dstPitchBytes) + (pTile->x * pixWidth)];

  if (pTile->x < 0) {
    rowSize += pTile->x;       // x is negative;
    srcPtr   = &srcPtr[(-pTile->x * pixWidth)];
    dstPtr   = &dstPtr[(-pTile->x * pixWidth)];
  }

  if (pTile->y < 0) {
    numRows += pTile->y;       // y is negative;
    srcPtr   = &srcPtr[-pTile->y * srcPitchBytes];
    dstPtr   = &dstPtr[-pTile->y * dstPitchBytes];
  }
  rowSize *= pixWidth;
  int32_t intrCompletionFlag = (interruptOnCompletion != 0) ? DESC_NOTIFY_W_INT : 0;

  if ((rowSize != 0) && (numRows != 0)) {
    pTile->status = pTile->status | XV_TILE_STATUS_DMA_ONGOING;
    TM_LOG_PRINT("src: %p, dst, %p, rowsize: %d, numRows: %d, srcPitch: %d, dstPitch: %d, flags: 0x%x, ",
                 (void*)srcPtr, (void*)dstPtr, rowSize, numRows, srcPitchBytes, dstPitchBytes, 0);

    idma_add_2d_desc(pxvTM->pdmaObj, dstPtr, srcPtr, rowSize, (uint32_t) intrCompletionFlag, numRows, srcPitchBytes, dstPitchBytes);
    dmaIndex = idma_schedule_desc(1);

    TM_LOG_PRINT("dmaIndex: %d\n", dmaIndex);
    pTile->dmaIndex = dmaIndex;
  }
  return(XVTM_SUCCESS);
}

/**********************************************************************************
 * FUNCTION: xvReqTileTransferOutFast16()
 *
 * DESCRIPTION:
 *     Requests 16b data transfer from tile present in local memory to frame in system memory.
 *
 * INPUTS:
 *     xvTileManager *pxvTM                   Tile Manager object
 *     xvTile        *pTile                   Source tile
 *     int32_t       interruptOnCompletion    If it is set, iDMA will interrupt after completing transfer
 *
 * OUTPUTS:
 *     Returns XVTM_ERROR if it encounters an error, else it returns XVTM_SUCCESS
 *
 ********************************************************************************** */

int32_t xvReqTileTransferOutFast16(xvTileManager *pxvTM, xvTile *pTile, int32_t interruptOnCompletion)
{
  
  return(xvReqTileTransferOutFast(pxvTM, pTile, interruptOnCompletion));

}

/**********************************************************************************
 * FUNCTION: xvCheckForIdmaIndex()
 *
 * DESCRIPTION:
 *     Checks if DMA transfer for given index is completed
 *
 * INPUTS:
 *     xvTileManager *pxvTM                   Tile Manager object
 *     int32_t        index                   Index of the dma transfer request, returned by xvAddIdmaRequest()
 *
 * OUTPUTS:
 *     Returns ONE if transfer is complete and ZERO if it is not
 *     Returns XVTM_ERROR if an error occurs
 *
 ********************************************************************************** */

int32_t xvCheckForIdmaIndex(xvTileManager *pxvTM, int32_t index)
{
  int32_t retVal = 1;
  XV_CHECK_ERROR_NULL(pxvTM == NULL, XVTM_ERROR, "NULL TM Pointer");
  pxvTM->errFlag = XV_ERROR_SUCCESS;
  if (index < 0) {
    return(XVTM_ERROR);
  }
  retVal = idma_desc_done(index);

  return(retVal);
}

/**********************************************************************************
 * FUNCTION: xvCheckTileReady()
 *
 * DESCRIPTION:
 *     Checks if DMA transfer for given tile is completed.
 *     It checks all the tile in the transfer request buffer
 *     before the given tile and updates their respective
 *     status. It pads edges wherever required.
 *
 * INPUTS:
 *     xvTileManager *pxvTM                   Tile Manager object
 *     xvTile  const *pTile                   Input tile
 *
 * OUTPUTS:
 *     Returns ONE if dma transfer for input tile is complete and ZERO if it is not
 *     Returns XVTM_ERROR if an error occurs
 *
 ********************************************************************************** */

int32_t xvCheckTileReady(xvTileManager *pxvTM, xvTile const *pTile)
{
  int32_t loopInd, index, retVal, dmaIndex, doneCount;
  uint32_t statusFlag, tileHeight;
  xvTile *pTile1;
  xvFrame *pFrame;
  int32_t frameWidth, frameHeight, framePitchBytes, tileWidth, tilePitchBytes, tileDMApendingCount;
  int32_t x1, y1, x2, y2, copyRowBytes, copyHeight;
  uint8_t framePadLeft, framePadRight, framePadTop, framePadBottom;
  uint16_t tileEdgeLeft, tileEdgeRight, tileEdgeTop, tileEdgeBottom;
  int32_t extraEdgeTop, extraEdgeBottom, extraEdgeLeft, extraEdgeRight;
  uint8_t pixWidth;
  uint8_t *srcPtr, *dstPtr;

  if (pxvTM == NULL) {
    return(XVTM_ERROR);
  }
  pxvTM->errFlag = XV_ERROR_SUCCESS;

  XV_CHECK_TILE(pTile, pxvTM);
  if (pTile->status == 0u)
  {
    return(1);
  }

  doneCount           = 0;
  index               = pxvTM->tileDMAstartIndex;
  tileDMApendingCount = pxvTM->tileDMApendingCount;
  for (loopInd = 0; loopInd < tileDMApendingCount; loopInd++) {
    index    = ((int32_t) pxvTM->tileDMAstartIndex + (int32_t) loopInd) % ((int32_t) MAX_NUM_DMA_QUEUE_LENGTH);
    pTile1   = pxvTM->tileProcQueue[index % ((int32_t) MAX_NUM_DMA_QUEUE_LENGTH)];
    dmaIndex = pTile1->dmaIndex;
    if ((pTile1->status & XV_TILE_STATUS_DUMMY_IDMA_INDEX_NEEDED) == 0u) {
      retVal = xvCheckForIdmaIndex(pxvTM, dmaIndex);
      if (retVal == 1) {
        statusFlag = pTile1->status;
        statusFlag = statusFlag & ~XV_TILE_STATUS_DMA_ONGOING;

        pFrame          = pTile1->pFrame;
        pixWidth        = pFrame->pixelRes * pFrame->numChannels;
        frameWidth      = pFrame->frameWidth;
        frameHeight     = pFrame->frameHeight;
        framePitchBytes = pFrame->framePitch * ((int32_t) pFrame->pixelRes);
        framePadLeft    = pFrame->leftEdgePadWidth;
        framePadRight   = pFrame->rightEdgePadWidth;
        framePadTop     = pFrame->topEdgePadHeight;
        framePadBottom  = pFrame->bottomEdgePadHeight;

        tileWidth      = pTile1->width;
        tileHeight     = pTile1->height;
        tilePitchBytes = pTile1->pitch * ((int32_t) pFrame->pixelRes);
        tileEdgeLeft   = pTile1->tileEdgeLeft;
        tileEdgeRight  = pTile1->tileEdgeRight;
        tileEdgeTop    = pTile1->tileEdgeTop;
        tileEdgeBottom = pTile1->tileEdgeBottom;

        if ((statusFlag & XV_TILE_STATUS_EDGE_PADDING_NEEDED) != 0u) {
          if ((statusFlag & XV_TILE_STATUS_TOP_EDGE_PADDING_NEEDED) != 0u) {
            y1           = pTile1->y - (int32_t) tileEdgeTop;
            extraEdgeTop = -((int32_t) framePadTop) - y1;
            dstPtr       = &((uint8_t *) pTile1->pData)[-(((int32_t) tileEdgeTop * tilePitchBytes) + ((int32_t) tileEdgeLeft * (int32_t) pixWidth))];
            srcPtr       = &dstPtr[extraEdgeTop * tilePitchBytes];
            copyRowBytes = (tileEdgeLeft + tileWidth + tileEdgeRight) * XV_TYPE_ELEMENT_SIZE(pTile1->type);
            xvCopyBufferEdgeDataH(srcPtr, dstPtr, copyRowBytes, (int32_t) pixWidth, extraEdgeTop, tilePitchBytes, pFrame->paddingType, pFrame->paddingVal);
          }

          if ((statusFlag & XV_TILE_STATUS_BOTTOM_EDGE_PADDING_NEEDED) != 0u) {
            y2              = pTile1->y + ((int32_t) tileHeight - 1) + ((int32_t) tileEdgeBottom);
            extraEdgeBottom = y2 - (((int32_t) frameHeight - 1) + (int32_t) framePadBottom);
            y2              = ((int32_t) frameHeight - 1) + (int32_t) framePadBottom;
            dstPtr          = &((uint8_t *) pTile1->pData) [(((y2 - pTile1->y) + 1) * (int32_t) tilePitchBytes) - ((int32_t) tileEdgeLeft * (int32_t) pixWidth)];
            srcPtr          = &dstPtr[-tilePitchBytes];
            copyRowBytes    = (tileEdgeLeft + tileWidth + tileEdgeRight) * XV_TYPE_ELEMENT_SIZE(pTile1->type);
            xvCopyBufferEdgeDataH(srcPtr, dstPtr, copyRowBytes, (int32_t) pixWidth, extraEdgeBottom, tilePitchBytes, pFrame->paddingType, pFrame->paddingVal);
          }

          if ((statusFlag & XV_TILE_STATUS_LEFT_EDGE_PADDING_NEEDED) != 0u) {
            x1            = pTile1->x - ((int32_t) tileEdgeLeft);
            extraEdgeLeft = (-(int32_t) framePadLeft) - x1;
            dstPtr        = &((uint8_t *) pTile1->pData)[-(((int32_t) tileEdgeTop * tilePitchBytes) + (((int32_t) tileEdgeLeft) * ((int32_t) pixWidth)))];
            srcPtr        = &dstPtr[extraEdgeLeft * ((int32_t) pixWidth)];
            copyHeight    = ((int32_t) tileEdgeTop) + ((int32_t) tileHeight) + ((int32_t) tileEdgeBottom);
            xvCopyBufferEdgeDataV(srcPtr, dstPtr, extraEdgeLeft, (int32_t) pixWidth, copyHeight, tilePitchBytes, pFrame->paddingType, pFrame->paddingVal);
          }

          if ((statusFlag & XV_TILE_STATUS_RIGHT_EDGE_PADDING_NEEDED) != 0u) {
            x2             = pTile1->x + (tileWidth - 1) + ((int32_t) tileEdgeRight);
            extraEdgeRight = x2 - (((int32_t) frameWidth - 1) + (int32_t) framePadRight);
            x2             = (((int32_t) frameWidth) - 1) + ((int32_t) framePadRight);
            dstPtr         = &((uint8_t *) pTile1->pData)[-(((int32_t) tileEdgeTop) * tilePitchBytes) + (((x2 - pTile1->x) + 1) * ((int32_t) pixWidth))];
            srcPtr         = &dstPtr[-((int32_t) pixWidth)];
            copyHeight     = ((int32_t) tileEdgeTop) + ((int32_t) tileHeight) + ((int32_t) tileEdgeBottom);
            xvCopyBufferEdgeDataV(srcPtr, dstPtr, extraEdgeRight, (int32_t) pixWidth, copyHeight, tilePitchBytes, pFrame->paddingType, pFrame->paddingVal);
          }
        }

        statusFlag = statusFlag & ~XV_TILE_STATUS_EDGE_PADDING_NEEDED;

        if ((pTile1->pPrevTile) != NULL) {
          pTile1->pPrevTile->reuseCount--;
          pTile1->pPrevTile = NULL;
        }

        pTile1->status = statusFlag;
        doneCount++;
      }
      else    // DMA not done for this tile
      {
        //Need to break the loop but MISRA-C does not allow more than 1
        //break statement. So just do the clean up and return. Also, note that
        // here return will always be 0 i.e. tile not ready.
        pxvTM->tileDMAstartIndex   = index;
        pxvTM->tileDMApendingCount = pxvTM->tileDMApendingCount - doneCount;
        return(0);
      }
    }
    else
    {
      // Tile is not part of frame. Make everything constant
      pFrame         = pTile1->pFrame;
      tileWidth      = pTile1->width;
      tileHeight     = pTile1->height;
      pixWidth       = pFrame->pixelRes * pFrame->numChannels;
      tilePitchBytes = pTile1->pitch * ((int32_t) pFrame->pixelRes);
      tileEdgeLeft   = pTile1->tileEdgeLeft;
      tileEdgeRight  = pTile1->tileEdgeRight;
      tileEdgeTop    = pTile1->tileEdgeTop;
      tileEdgeBottom = pTile1->tileEdgeBottom;

      dstPtr       = &((uint8_t *) pTile1->pData) [-((((int32_t) tileEdgeTop * tilePitchBytes) + ((int32_t) tileEdgeLeft * (int32_t) pixWidth)))];
      srcPtr       = NULL;
      copyRowBytes = (tileEdgeLeft + tileWidth + tileEdgeRight) * XV_TYPE_ELEMENT_SIZE(pTile1->type);
      copyHeight   = ((int32_t) tileHeight) + ((int32_t) tileEdgeTop) + ((int32_t) tileEdgeBottom);
      if (pFrame->paddingType != FRAME_EDGE_PADDING) {
        xvCopyBufferEdgeDataH(srcPtr, dstPtr, copyRowBytes, (int32_t) pixWidth, copyHeight, tilePitchBytes, pFrame->paddingType, pFrame->paddingVal);
      }
      pTile1->status = 0;
      doneCount++;
    }

    // Break if we reached the required tile
    if (pTile1 == pTile)
    {
      index = (index + 1) % ((int32_t) MAX_NUM_DMA_QUEUE_LENGTH);
      break;
    }
  }
  pxvTM->tileDMAstartIndex   = index;
  pxvTM->tileDMApendingCount = pxvTM->tileDMApendingCount - doneCount;
  return((pTile->status == 0u) ? 1 : 0);
}

/**********************************************************************************
 * FUNCTION: xvPadEdges()
 *
 * DESCRIPTION:
 *     Pads edges of the given 8b tile. If FRAME_EDGE_PADDING mode is used,
 *     padding is done using edge values of the frame else if
 *     FRAME_CONSTANT_PADDING or FRAME_ZERO_PADDING mode is used,
 *     constant or zero value is padded
 *		xvPadEdges should be used with Fast functions
 * INPUTS:
 *     xvTileManager *pxvTM                   Tile Manager object
 *     xvTile        *pTile                   Input tile
 *
 * OUTPUTS:
 *     Returns XVTM_ERROR if an error occurs
 *
 ********************************************************************************** */
int32_t xvPadEdges(xvTileManager *pxvTM, xvTile *pTile)
{
  int32_t x1, x2, y1, y2, indy, copyHeight, copyRowBytes, wb, padVal;
  int32_t tileWidth, tilePitchBytes, frameWidth, frameHeight;
  uint32_t tileHeight;


  uint16_t tileEdgeLeft, tileEdgeRight, tileEdgeTop, tileEdgeBottom;
  int32_t extraEdgeLeft, extraEdgeRight, extraEdgeTop, extraEdgeBottom;
  uint16_t * __restrict srcPtr_16b, * __restrict dstPtr_16b;
  uint32_t * __restrict srcPtr_32b, * __restrict dstPtr_32b;
  uint8_t * __restrict srcPtr, * __restrict dstPtr;
  xvFrame *pFrame;
  xb_vecNx16 vec1, * __restrict pvecDst;
  xb_vec2Nx8U dvec1, * __restrict pdvecDst;
  xb_vecN_2x32Uv hvec1, * __restrict phvecDst;
  valign vas1;
  XV_CHECK_ERROR_NULL(pxvTM == NULL, XVTM_ERROR, "NULL TM Pointer");
  pxvTM->errFlag = XV_ERROR_SUCCESS;
  XV_CHECK_TILE(pTile, pxvTM);
  if ((pTile->status & XV_TILE_STATUS_EDGE_PADDING_NEEDED) != 0u) {
    pFrame = pTile->pFrame;
    XV_CHECK_ERROR(pFrame == NULL, pxvTM->errFlag = XV_ERROR_FRAME_NULL, XVTM_ERROR, "XV_ERROR_FRAME_NULL");
    XV_CHECK_ERROR(((pFrame->pFrameBuff == NULL) || (pFrame->pFrameData == NULL)), pxvTM->errFlag = XV_ERROR_FRAME_NULL, XVTM_ERROR, "XV_ERROR_FRAME_NULL");

    int32_t channel     = XV_TYPE_CHANNELS(XV_TILE_GET_TYPE(pTile));
    int32_t bytesPerPix = XV_TYPE_ELEMENT_SIZE(XV_TILE_GET_TYPE(pTile));
    int32_t bytePerPel;
    bytePerPel = bytesPerPix / channel;
    XV_CHECK_ERROR((int32_t) pFrame->pixelRes != bytePerPel, pxvTM->errFlag = XV_ERROR_BAD_ARG, XVTM_ERROR, "XV_ERROR_BAD_ARG");
    XV_CHECK_ERROR((int32_t) pFrame->numChannels != channel, pxvTM->errFlag = XV_ERROR_BAD_ARG, XVTM_ERROR, "XV_ERROR_BAD_ARG");

    tileEdgeTop    = pTile->tileEdgeTop;
    tileEdgeBottom = pTile->tileEdgeBottom;
    tileEdgeLeft   = pTile->tileEdgeLeft;
    tileEdgeRight  = pTile->tileEdgeRight;
    tilePitchBytes = pTile->pitch;
    tileHeight     = pTile->height;
    tileWidth      = pTile->width;
    frameWidth     = pFrame->frameWidth;
    frameHeight    = pFrame->frameHeight;

    if (pFrame->paddingType == FRAME_EDGE_PADDING) {
      if (bytesPerPix == 1)
      {
        if ((pTile->status & XV_TILE_STATUS_LEFT_EDGE_PADDING_NEEDED) != 0u) {
          extraEdgeLeft = ((int32_t) tileEdgeLeft) - pTile->x;
          dstPtr        = &((uint8_t *) pTile->pData)[-((((int32_t) tileEdgeTop) * pTile->pitch) + ((int32_t) tileEdgeLeft))];
          srcPtr        = &dstPtr[extraEdgeLeft];
          copyHeight    = ((int32_t) tileEdgeTop) + ((int32_t) tileHeight) + ((int32_t) tileEdgeBottom);

          vas1 = IVP_ZALIGN();
          for (indy = 0; indy < copyHeight; indy++) {
            dvec1    = *srcPtr;
            pdvecDst = (xb_vec2Nx8U *) dstPtr;
            IVP_SAV2NX8U_XP(dvec1, vas1, pdvecDst, extraEdgeLeft);
            IVP_SAPOS2NX8U_FP(vas1, pdvecDst);
            dstPtr = &dstPtr[pTile->pitch];
            srcPtr = &srcPtr[pTile->pitch];
          }
        }

        if ((pTile->status & XV_TILE_STATUS_RIGHT_EDGE_PADDING_NEEDED) != 0u) {
          x2             = pTile->x + (tileWidth - 1) + ((int32_t) tileEdgeRight);
          extraEdgeRight = x2 - (frameWidth - 1);
          dstPtr         = &((uint8_t *) pTile->pData)[ -((int32_t) tileEdgeTop * pTile->pitch) + (((int32_t) tileWidth + (int32_t) tileEdgeRight) - (int32_t) extraEdgeRight)];
          srcPtr         = &dstPtr[-1];
          copyHeight     = ((int32_t) tileEdgeTop) + ((int32_t) tileHeight) + ((int32_t) tileEdgeBottom);

          vas1 = IVP_ZALIGN();
          for (indy = 0; indy < copyHeight; indy++) {
            dvec1    = *srcPtr;
            pdvecDst = (xb_vec2Nx8U *) dstPtr;
            IVP_SAV2NX8U_XP(dvec1, vas1, pdvecDst, extraEdgeRight);
            IVP_SAPOS2NX8U_FP(vas1, pdvecDst);
            dstPtr = &dstPtr[pTile->pitch];
            srcPtr = &srcPtr[pTile->pitch];
          }
        }
      }
      else if (bytesPerPix == 2)
      {
        if ((pTile->status & XV_TILE_STATUS_LEFT_EDGE_PADDING_NEEDED) != 0u) {
          extraEdgeLeft = (int32_t) tileEdgeLeft - pTile->x;
          dstPtr_16b    = &((uint16_t *) pTile->pData)[ -(((int32_t) tileEdgeTop * pTile->pitch) + (int32_t) tileEdgeLeft)];                                   // No need of multiplying by 2
          srcPtr_16b    = &dstPtr_16b[extraEdgeLeft];                                                                                  // No need of multiplying by 2 as pointers are uint16_t *
          copyHeight    = (int32_t) tileEdgeTop + (int32_t) tileHeight + (int32_t) tileEdgeBottom;
          vas1          = IVP_ZALIGN();
          for (indy = 0; indy < copyHeight; indy++) {
            vec1    = *srcPtr_16b;
            pvecDst = (xb_vecNx16 *) dstPtr_16b;
            IVP_SAVNX16_XP(vec1, vas1, pvecDst, extraEdgeLeft * 2);
            IVP_SAPOSNX16_FP(vas1, pvecDst);
            dstPtr_16b = &dstPtr_16b[pTile->pitch];
            srcPtr_16b = &srcPtr_16b[pTile->pitch];
          }
        }
        if ((pTile->status & XV_TILE_STATUS_RIGHT_EDGE_PADDING_NEEDED) != 0u) {
          x2             = pTile->x + tileWidth + (int32_t) tileEdgeRight;
          extraEdgeRight = x2 - frameWidth;
          dstPtr_16b     = &((uint16_t *) pTile->pData)[(((int32_t) tileWidth + (int32_t) tileEdgeRight) - (int32_t) extraEdgeRight) - ((int32_t) tileEdgeTop * pTile->pitch)];
          srcPtr_16b     = &dstPtr_16b[ -1];
          copyHeight     = (int32_t) tileEdgeTop + (int32_t) tileHeight + (int32_t) tileEdgeBottom;
          vas1           = IVP_ZALIGN();
          for (indy = 0; indy < copyHeight; indy++) {
            vec1    = *srcPtr_16b;
            pvecDst = (xb_vecNx16 *) dstPtr_16b;
            IVP_SAVNX16_XP(vec1, vas1, pvecDst, extraEdgeRight * 2);
            IVP_SAPOSNX16_FP(vas1, pvecDst);
            dstPtr_16b = &dstPtr_16b[pTile->pitch];
            srcPtr_16b = &srcPtr_16b[pTile->pitch];
          }
        }
      }
      else
      {
        if ((pTile->status & XV_TILE_STATUS_LEFT_EDGE_PADDING_NEEDED) != 0u) {
          extraEdgeLeft = (int32_t) tileEdgeLeft - pTile->x;
          dstPtr_32b    = &((uint32_t *) pTile->pData)[ -(((int32_t) tileEdgeTop * pTile->pitch) + (int32_t) tileEdgeLeft)];           // No need of multiplying by 2
          srcPtr_32b    = &dstPtr_32b[extraEdgeLeft];                                                                                  // No need of multiplying by 2 as pointers are uint32_t *
          copyHeight    = (int32_t)tileEdgeTop + (int32_t)tileHeight + (int32_t)tileEdgeBottom;
          vas1          = IVP_ZALIGN();
          for (indy = 0; indy < copyHeight; indy++) {
            hvec1    = *srcPtr_32b;
            phvecDst = (xb_vecN_2x32Uv *) dstPtr_32b;
            IVP_SAVN_2X32_XP(hvec1, vas1, phvecDst, extraEdgeLeft * 4);
            IVP_SAPOSN_2X32_FP(vas1, (xb_vecN_2x32v *) phvecDst);
            dstPtr_32b = &dstPtr_32b[pTile->pitch];
            srcPtr_32b = &srcPtr_32b[pTile->pitch];
          }
        }
        if ((pTile->status & XV_TILE_STATUS_RIGHT_EDGE_PADDING_NEEDED) != 0u) {
          x2             = pTile->x + (int32_t) tileWidth + (int32_t) tileEdgeRight;
          extraEdgeRight = x2 - frameWidth;
          dstPtr_32b     = &((uint32_t *) pTile->pData)[(((int32_t) tileWidth + (int32_t) tileEdgeRight) - (int32_t) extraEdgeRight) - ((int32_t) tileEdgeTop * pTile->pitch)];
          srcPtr_32b     = &dstPtr_32b[ -1];
          copyHeight     = (int32_t)tileEdgeTop + (int32_t)tileHeight + (int32_t)tileEdgeBottom;
          vas1           = IVP_ZALIGN();
          for (indy = 0; indy < copyHeight; indy++) {
            hvec1     = *srcPtr_32b;
            phvecDst = (xb_vecN_2x32Uv *) dstPtr_32b;
            IVP_SAVN_2X32_XP(hvec1, vas1, phvecDst, extraEdgeRight * 4);
            IVP_SAPOSN_2X32_FP(vas1, (xb_vecN_2x32v *) phvecDst);
            dstPtr_32b = &dstPtr_32b[pTile->pitch];
            srcPtr_32b = &srcPtr_32b[pTile->pitch];
          }
        }
      }
    }
    else if(pFrame->paddingType == FRAME_PADDING_REFLECT_101)
    {
    	if (bytesPerPix == 1)
    	{
    		xvExtendEdgesReflect101_I8(pTile, frameWidth, frameHeight);

    	}
    	else if (bytesPerPix == 2)
    	{
    		xvExtendEdgesReflect101_I16(pTile, frameWidth, frameHeight);
    	}
    }
    else  {
      padVal = 0;
      if (pFrame->paddingType == FRAME_CONSTANT_PADDING) {
        padVal = (int32_t) pFrame->paddingVal;
      }
      dvec1 = padVal;
      if (bytesPerPix == 1)
      {
        dvec1 = padVal;
      }
      else if (bytesPerPix == 2)
      {
        xb_vecNx16U vec = padVal;
        dvec1 = IVP_MOV2NX8U_FROMNX16(vec);
      }
      else
      {
        xb_vecN_2x32Uv hvec = padVal;
        dvec1 = IVP_MOV2NX8U_FROMNX16(IVP_MOVNX16_FROMN_2X32U(hvec));
      }

      if ((pTile->status & XV_TILE_STATUS_DUMMY_IDMA_INDEX_NEEDED) == 0u) {
        if ((pTile->status & XV_TILE_STATUS_LEFT_EDGE_PADDING_NEEDED) != 0u) {
          x1            = pTile->x - ((int32_t) tileEdgeLeft);
          extraEdgeLeft = -x1;
          dstPtr        = &((uint8_t *) pTile->pData)[-((((int32_t) tileEdgeTop) * pTile->pitch) + ((int32_t) tileEdgeLeft)) * bytesPerPix];
          copyHeight    = ((int32_t) tileEdgeTop) + ((int32_t) tileHeight) + ((int32_t) tileEdgeBottom);

          pdvecDst = (xb_vec2Nx8U *) dstPtr;
          vas1     = IVP_ZALIGN();
          for (indy = 0; indy < copyHeight; indy++) {
            for (wb = extraEdgeLeft * bytesPerPix; wb > 0; wb -= (2 * IVP_SIMD_WIDTH)) {
              IVP_SAV2NX8U_XP(dvec1, vas1, pdvecDst, wb);
            }
            IVP_SAPOS2NX8U_FP(vas1, pdvecDst);
            dstPtr   = &dstPtr[pTile->pitch * bytesPerPix];
            pdvecDst = (xb_vec2Nx8U *) dstPtr;
          }
        }

        if ((pTile->status & XV_TILE_STATUS_RIGHT_EDGE_PADDING_NEEDED) != 0u) {
          x2             = pTile->x + (int32_t) tileWidth + (int32_t) tileEdgeRight;
          extraEdgeRight = x2 - frameWidth;

          dstPtr     = &((uint8_t *) pTile->pData)[((((tileWidth + (int32_t) tileEdgeRight) - extraEdgeRight) - ((int32_t) tileEdgeTop * pTile->pitch)) * bytesPerPix)];
          copyHeight = ((int32_t) tileEdgeTop) + ((int32_t) tileHeight) + ((int32_t) tileEdgeBottom);

          pdvecDst = (xb_vec2Nx8U *) dstPtr;
          vas1     = IVP_ZALIGN();
          for (indy = 0; indy < copyHeight; indy++) {
            for (wb = extraEdgeRight * bytesPerPix; wb > 0; wb -= (2 * IVP_SIMD_WIDTH)) {
              IVP_SAV2NX8U_XP(dvec1, vas1, pdvecDst, wb);
            }
            IVP_SAPOS2NX8U_FP(vas1, pdvecDst);
            dstPtr   = &dstPtr[pTile->pitch * bytesPerPix];
            pdvecDst = (xb_vec2Nx8U *) dstPtr;
          }
        }

        if ((pTile->status & XV_TILE_STATUS_TOP_EDGE_PADDING_NEEDED) != 0u) {
          y1           = pTile->y - ((int32_t) tileEdgeTop);
          extraEdgeTop = -y1;
          dstPtr       = &((uint8_t *) pTile->pData)[ -(((((int32_t) tileEdgeTop * pTile->pitch) + (int32_t) tileEdgeLeft)) * bytesPerPix)];
          copyRowBytes = (((int32_t) tileEdgeLeft) + ((int32_t) tileWidth) + ((int32_t) tileEdgeRight)) * bytesPerPix;

          pdvecDst = (xb_vec2Nx8U *) dstPtr;
          vas1     = IVP_ZALIGN();
          for (indy = 0; indy < extraEdgeTop; indy++) {
            for (wb = copyRowBytes; wb > 0; wb -= (2 * IVP_SIMD_WIDTH)) {
              IVP_SAV2NX8U_XP(dvec1, vas1, pdvecDst, wb);
            }
            IVP_SAPOS2NX8U_FP(vas1, pdvecDst);
            dstPtr   = &dstPtr[pTile->pitch * bytesPerPix];
            pdvecDst = (xb_vec2Nx8U *) dstPtr;
          }
        }

        if ((pTile->status & XV_TILE_STATUS_BOTTOM_EDGE_PADDING_NEEDED) != 0u) {
          y2              = pTile->y + ((int32_t) tileHeight) + ((int32_t) tileEdgeBottom);
          extraEdgeBottom = y2 - frameHeight;
          dstPtr          = &((uint8_t *) pTile->pData)[ (((((int32_t) frameHeight - pTile->y) * pTile->pitch) - (int32_t) tileEdgeLeft) * bytesPerPix)];
          copyRowBytes    = ((int32_t) tileEdgeLeft + (int32_t) tileWidth + (int32_t) tileEdgeRight) * bytesPerPix;

          pdvecDst = (xb_vec2Nx8U *) dstPtr;
          vas1     = IVP_ZALIGN();
          for (indy = 0; indy < extraEdgeBottom; indy++) {
            for (wb = copyRowBytes; wb > 0; wb -= (2 * IVP_SIMD_WIDTH)) {
              IVP_SAV2NX8U_XP(dvec1, vas1, pdvecDst, wb);
            }
            IVP_SAPOS2NX8U_FP(vas1, pdvecDst);
            dstPtr   = &dstPtr[pTile->pitch * bytesPerPix];
            pdvecDst = (xb_vec2Nx8U *) dstPtr;
          }
        }
      }
      else  {
        // Tile is not part of frame. Make it constant
        dstPtr       = &((uint8_t *) pTile->pData)[ -((((int32_t) tileEdgeTop * pTile->pitch) + (int32_t) tileEdgeLeft) * bytesPerPix)];
        copyHeight   = ((int32_t) tileHeight) + ((int32_t) tileEdgeTop) + ((int32_t) tileEdgeBottom);
        copyRowBytes = ((int32_t) tileEdgeLeft + (int32_t) tileWidth + (int32_t) tileEdgeRight) * bytesPerPix;
        xvCopyBufferEdgeDataH(NULL, dstPtr, copyRowBytes, bytesPerPix, copyHeight, (pTile->pitch * bytesPerPix), pFrame->paddingType, pFrame->paddingVal);
      }
    }

    pTile->status = pTile->status & ~(XV_TILE_STATUS_EDGE_PADDING_NEEDED | XV_TILE_STATUS_DUMMY_IDMA_INDEX_NEEDED);
  }
  return(XVTM_SUCCESS);
}

/**********************************************************************************
 * FUNCTION: xvPadEdges16()
 *
 * DESCRIPTION:
 *     Pads edges of the given 16b tile. If FRAME_EDGE_PADDING mode is used,
 *     padding is done using edge values of the frame else if
 *     FRAME_CONSTANT_PADDING or FRAME_ZERO_PADDING mode is used,
 *     constant or zero value is padded
 *
 * INPUTS:
 *     xvTileManager *pxvTM                   Tile Manager object
 *     xvTile        *pTile                   Input tile
 *
 * OUTPUTS:
 *     Returns XVTM_ERROR if an error occurs
 *
 ********************************************************************************** */
// xvPadEdges should be used with Fast functions
int32_t xvPadEdges16(xvTileManager *pxvTM, xvTile *pTile)
{

  return(xvPadEdges(pxvTM, pTile));

}

/**********************************************************************************
 * FUNCTION: xvCheckInputTileFree()
 *
 * DESCRIPTION:
 *     A tile is said to be free if all data transfers pertaining to data resue
 *     from this tile is completed
 *
 * INPUTS:
 *     xvTileManager       * pxvTM                   Tile Manager object
 *     xvTile        const * pTile                   Input tile
 *
 * OUTPUTS:
 *     Returns ONE if input tile is free and ZERO if it is not
 *     Returns XVTM_ERROR if an error occurs
 *
 ********************************************************************************** */

int32_t xvCheckInputTileFree(xvTileManager *pxvTM, xvTile const * pTile)
{
  XV_CHECK_ERROR_NULL(pxvTM == NULL, XVTM_ERROR, "NULL TM Pointer");
  pxvTM->errFlag = XV_ERROR_SUCCESS;

  XV_CHECK_TILE(pTile, pxvTM);
  if (pTile->reuseCount == 0) {
    return(1);
  }
  else  {
    return(0);
  }
}

/**********************************************************************************
 * FUNCTION: xvWaitForTile()
 *
 * DESCRIPTION:
 *     Waits till DMA transfer for given tile is completed.
 *
 * INPUTS:
 *     xvTileManager *pxvTM                   Tile Manager object
 *     xvTile const  *pTile                   Input tile
 *
 * OUTPUTS:
 *     Returns ONE if dma transfer for input tile is complete and ZERO if it is not
 *     Returns XVTM_ERROR if an error occurs
 *
 ********************************************************************************** */
int32_t xvWaitForTile(xvTileManager *pxvTM, xvTile const *pTile)
{
  XV_CHECK_ERROR_NULL(pxvTM == NULL, XVTM_ERROR, "NULL TM Pointer");
  XV_CHECK_TILE(pTile, pxvTM);
  int32_t status = 0;
  while ((status == 0) && (pxvTM->idmaErrorFlag == XV_ERROR_SUCCESS)) {
    status = xvCheckTileReady(pxvTM, pTile);
  }
  return((int32_t) (pxvTM->idmaErrorFlag));
}

/**********************************************************************************
 * FUNCTION: xvSleepForTile()
 *
 * DESCRIPTION:
 *     Sleeps till DMA transfer for given tile is completed.
 *
 * INPUTS:
 *     xvTileManager *pxvTM                   Tile Manager object
 *     xvTile const  *pTile                   Input tile
 *
 * OUTPUTS:
 *     Returns ONE if dma transfer for input tile is complete and ZERO if it is not
 *     Returns XVTM_ERROR if an error occurs
 *
 ********************************************************************************** */
int32_t xvSleepForTile(xvTileManager *pxvTM, xvTile const *pTile)
{
  XV_CHECK_ERROR_NULL(pxvTM == NULL, XVTM_ERROR, "NULL TM Pointer");
  XV_CHECK_TILE(pTile, pxvTM);
  #if ((XTHAL_RELEASE_MAJOR + XTHAL_RELEASE_MINOR) >= 12009)
  DECLARE_PS();
  #endif
  IDMA_DISABLE_INTS();
  int32_t status = xvCheckTileReady((pxvTM), (pTile));
  while ((status == 0) && (pxvTM->idmaErrorFlag == XV_ERROR_SUCCESS)) {
    idma_sleep();
    status = xvCheckTileReady((pxvTM), (pTile));
  }
  IDMA_ENABLE_INTS();
  return((int32_t) (pxvTM->idmaErrorFlag));
}

/**********************************************************************************
 * FUNCTION: xvWaitForiDMA()
 *
 * DESCRIPTION:
 *     Waits till DMA transfer for given DMA index is completed.
 *
 * INPUTS:
 *     xvTileManager const * pxvTM                   Tile Manager object
 *     int32_t               dmaIndex                DMA index
 *
 * OUTPUTS:
 *     Returns ONE if dma transfer for input tile is complete and ZERO if it is not
 *     Returns XVTM_ERROR if an error occurs
 *
 ********************************************************************************** */
int32_t xvWaitForiDMA(xvTileManager const * pxvTM, int32_t dmaIndex)
{
  XV_CHECK_ERROR_NULL(pxvTM == NULL, XVTM_ERROR, "NULL TM Pointer");

  int32_t status = 0;

  while ((status == 0) && (pxvTM->idmaErrorFlag == XV_ERROR_SUCCESS)) {
    status = idma_desc_done((dmaIndex));
  }
  return((int32_t) (pxvTM->idmaErrorFlag));
}

/**********************************************************************************
 * FUNCTION: xvSleepForiDMA()
 *
 * DESCRIPTION:
 *     Sleeps till DMA transfer for given DMA index is completed.
 *
 * INPUTS:
 *     xvTileManager const * pxvTM                   Tile Manager object
 *     int32_t               dmaIndex                DMA index
 *
 * OUTPUTS:
 *     Returns ONE if dma transfer for input tile is complete and ZERO if it is not
 *     Returns XVTM_ERROR if an error occurs
 *
 ********************************************************************************** */
int32_t xvSleepForiDMA(xvTileManager const * pxvTM, int32_t dmaIndex)
{
  XV_CHECK_ERROR_NULL(pxvTM == NULL, XVTM_ERROR, "NULL TM Pointer");

  #if ((XTHAL_RELEASE_MAJOR + XTHAL_RELEASE_MINOR) >= 12009)
  DECLARE_PS();
  #endif
  IDMA_DISABLE_INTS();
  int32_t status = idma_desc_done((dmaIndex));
  while ((status == 0) && (pxvTM->idmaErrorFlag == XV_ERROR_SUCCESS)) {
    idma_sleep();
    status = idma_desc_done((dmaIndex));
  }
  IDMA_ENABLE_INTS();
  return((int32_t) (pxvTM->idmaErrorFlag));
}

/**********************************************************************************
 * FUNCTION: xvWaitForTileFast()
 *
 * DESCRIPTION:
 *     Waits till DMA transfer for given tile is completed.
 *
 * INPUTS:
 *     xvTileManager const * pxvTM                   Tile Manager object
 *     xvTile              * pTile                   Input tile
 *
 * OUTPUTS:
 *     Returns ONE if dma transfer for input tile is complete and ZERO if it is not
 *     Returns XVTM_ERROR if an error occurs
 *
 ********************************************************************************** */
int32_t xvWaitForTileFast(xvTileManager const * pxvTM, xvTile  *pTile)
{
  int32_t status = 0;

  while ((status == 0) && (pxvTM->idmaErrorFlag == XV_ERROR_SUCCESS)) {
    status = idma_desc_done((pTile)->dmaIndex);
  }
  (pTile)->status = (pTile)->status & ~XV_TILE_STATUS_DMA_ONGOING;
  return((int32_t) (pxvTM->idmaErrorFlag));
}

/**********************************************************************************
 * FUNCTION: xvSleepForTileFast()
 *
 * DESCRIPTION:
 *     Sleeps till DMA transfer for given tile is completed.
 *
 * INPUTS:
 *     xvTileManager const * pxvTM                   Tile Manager object
 *     xvTile              * pTile                   Input tile
 *
 * OUTPUTS:
 *     Returns ONE if dma transfer for input tile is complete and ZERO if it is not
 *     Returns XVTM_ERROR if an error occurs
 *
 ********************************************************************************** */
int32_t xvSleepForTileFast(xvTileManager const * pxvTM, xvTile *pTile)
{
  #if ((XTHAL_RELEASE_MAJOR + XTHAL_RELEASE_MINOR) >= 12009)
  DECLARE_PS();
  #endif
  IDMA_DISABLE_INTS();
  int32_t status = idma_desc_done((pTile)->dmaIndex);


  while ((status == 0) && (pxvTM->idmaErrorFlag == XV_ERROR_SUCCESS)) {
    idma_sleep();
    status = idma_desc_done((pTile)->dmaIndex);
  }
  IDMA_ENABLE_INTS();
  (pTile)->status = (pTile)->status & ~XV_TILE_STATUS_DMA_ONGOING;
  return((int32_t) (pxvTM->idmaErrorFlag));
}

/**********************************************************************************
 * FUNCTION: xvGetErrorInfo()
 *
 * DESCRIPTION:
 *
 *     Prints the most recent error information.
 *
 * INPUTS:
 *     xvTileManager const * pxvTM                   Tile Manager object
 *
 * OUTPUTS:
 *     It returns the most recent error code.
 *
 ********************************************************************************** */

xvError_t xvGetErrorInfo(xvTileManager const * pxvTM)
{
  if (pxvTM == NULL) {
    return(XV_ERROR_TILE_MANAGER_NULL);
  }
  return(pxvTM->errFlag);
}


typedef xb_vecNx16 vsaN;
#define XVTM_IVP_SCATTERNX16T(a, b, c, d) IVP_SCATTERNX16T((a), (short int *)(b), (c), (d))
#define XVTM_IVP_SCATTERNX8UT(a, b, c, d) IVP_SCATTERNX8UT((a), (unsigned char *)(b), (c), (d))
#define XVTM_IVP_GATHERANX16T_V(a, b, c, d) IVP_GATHERANX16T_V((const short int *)(a), (b), (c), (d))
#define XVTM_IVP_GATHERANX8UT_V(a, b, c, d) IVP_GATHERANX8UT_V((const unsigned char *)(a), (b), (c), (d))
#define XVTM_IVP_SAVNX16POS_FP IVP_SAPOSNX16_FP
#define XVTM_IVP_MOVVSV(vr,sa) (vr) // sa is always zero in XI, if not zero -> use IVP_MOVVSELNX16
#define XVTM_OFFSET_PTR_2NX8U(  ptr, nrows, stride, in_row_offset) ((xb_vec2Nx8U*)   ((uint8_t*) (ptr)+(in_row_offset)+((nrows)*(stride))))
#define XVTM_OFFSET_PTR_NX8U(   ptr, nrows, stride, in_row_offset) ((xb_vecNx8U*)    ((uint8_t*) (ptr)+(in_row_offset)+((nrows)*(stride))))
#define XVTM_OFFSET_PTR_NX16(   ptr, nrows, stride, in_row_offset) ((xb_vecNx16*)    ((int16_t*) (ptr)+(in_row_offset)+((nrows)*(stride))))

#define XVTM_EDGE_REFLECT_I8_SCATTER_MIN_WIDTH  5
#define XVTM_EDGE_REFLECT_I16_SCATTER_MIN_WIDTH 8
#define XVTM_IVP_SAV2NX8UPOS_FP IVP_SAPOS2NX8U_FP

static void xvExtendEdgesReflect101_I8(xvTile const * tile, int32_t frame_width, int32_t frame_height)
{

    int32_t w = XV_TILE_GET_EDGE_WIDTH(tile);
    int32_t h = XV_TILE_GET_EDGE_HEIGHT(tile);

    uint8_t* __restrict src = (uint8_t *)XV_TILE_GET_DATA_PTR(tile);
    xb_vec2Nx8U * vpdst;

    int32_t stride = XV_TILE_GET_PITCH(tile);

    int32_t start_x = XV_TILE_GET_X_COORD(tile);
    int32_t start_y = XV_TILE_GET_Y_COORD(tile);

    int32_t W_Local = XV_TILE_GET_WIDTH(tile);
    int32_t H_Local = XV_TILE_GET_HEIGHT(tile);

    // find intersection of tile/frame
    int32_t ixmin = XT_MAX(start_x - w, 0);
    int32_t ixmax = XT_MIN(start_x + W_Local + w - 1, frame_width - 1);
    int32_t iymin = XT_MAX(start_y - h, 0);
    int32_t iymax = XT_MIN(start_y + H_Local + h - 1, frame_height - 1);


    int32_t p0x  = ixmin - start_x;
    int32_t ps0y = iymin - start_y;
    int32_t pd0y = iymin - start_y - 1;
    int32_t p0w  = (ixmax - ixmin) + 1;
    int32_t p0h  = iymin - (start_y - h);

    uint8_t* curr_src = (uint8_t *)XVTM_OFFSET_PTR_2NX8U(src, ps0y, stride, p0x);
    uint8_t* dst      = (uint8_t *)XVTM_OFFSET_PTR_2NX8U(src, pd0y, stride, p0x);
    valign a_store = IVP_ZALIGN();
    valign a_load;

    int32_t p = XT_MAX(1, 2 * (iymax - iymin + 1) - 2);
    int32_t pmod = (p << 16) + 1;

    int32_t x = 0;
    int32_t y = 0;
    if(p0h > 0)
    {
		for (; x < (p0w - (2 * XCHAL_IVPN_SIMD_WIDTH)); x += 4 * XCHAL_IVPN_SIMD_WIDTH)
		{
			int32_t z = 0;

			for ( y = 0; y < p0h; y++)
			{
				z = IVP_ADDMOD16U(z, pmod);
				int32_t k = XT_MIN(z, p - z);

				xb_vec2Nx8U color, color1;
				xb_vec2Nx8U * src_1 = XVTM_OFFSET_PTR_2NX8U(curr_src, k, stride, x);
				a_load = IVP_LA2NX8U_PP(src_1);
				IVP_LA2NX8U_IP (color,  a_load, src_1);
				IVP_LAV2NX8U_XP(color1, a_load, src_1, sizeof(uint8_t) * (p0w - x - 2 * XCHAL_IVPN_SIMD_WIDTH));

				xb_vec2Nx8U * vdst = XVTM_OFFSET_PTR_2NX8U(dst, -y, stride, x);
				IVP_SA2NX8U_IP (color,  a_store, vdst);
				IVP_SAV2NX8U_XP(color1, a_store, vdst, sizeof(uint8_t) * (p0w - x - 2 * XCHAL_IVPN_SIMD_WIDTH));
				XVTM_IVP_SAV2NX8UPOS_FP(a_store, vdst);
			}
		}
		if(x < p0w)
		{
			int32_t z = 0;
			for ( y = 0; y < p0h; y++)
			{
				z = IVP_ADDMOD16U(z, pmod);
				int32_t k = XT_MIN(z, p - z);

				xb_vec2Nx8U color;
				xb_vec2Nx8U * src_1 = XVTM_OFFSET_PTR_2NX8U(curr_src, k, stride, x);
				a_load = IVP_LA2NX8U_PP(src_1);
				IVP_LAV2NX8U_XP(color, a_load, src_1, sizeof(uint8_t) * (p0w - x));

				xb_vec2Nx8U * vdst = XVTM_OFFSET_PTR_2NX8U(dst, -y, stride, x);
				IVP_SAV2NX8U_XP(color, a_store, vdst, sizeof(uint8_t) * (p0w - x));
				XVTM_IVP_SAV2NX8UPOS_FP(a_store, vdst);
			}
		}
    }

    int32_t p1x = ixmin - start_x;
    int32_t ps1y = iymax - start_y;
    int32_t pd1y = (iymax + 1) - start_y;
    int32_t p1w = (ixmax - ixmin) + 1;
    int32_t p1h = ( start_y + H_Local + h ) - 1 - iymax;

    curr_src = (uint8_t *)XVTM_OFFSET_PTR_2NX8U(src, ps1y, stride, p1x);
    dst      = (uint8_t *)XVTM_OFFSET_PTR_2NX8U(src, pd1y, stride, p1x);
    x = 0;
    if(p1h > 0)
    {
		for (; x < (p1w - ( 2 * XCHAL_IVPN_SIMD_WIDTH )); x += 4 * XCHAL_IVPN_SIMD_WIDTH)
		{
			int32_t z = 0;
			for ( y = 0; y < p1h; y++)
			{
				z = IVP_ADDMOD16U(z, pmod);
				int32_t k = XT_MIN(z, p - z);

				xb_vec2Nx8U color, color1;
				xb_vec2Nx8U * src_1 = XVTM_OFFSET_PTR_2NX8U(curr_src, -k, stride, x);
				a_load = IVP_LA2NX8U_PP(src_1);
				IVP_LA2NX8U_IP (color,  a_load, src_1);
				IVP_LAV2NX8U_XP(color1, a_load, src_1, sizeof(uint8_t) * (p1w - x - 2 * XCHAL_IVPN_SIMD_WIDTH));

				xb_vec2Nx8U * vdst = XVTM_OFFSET_PTR_2NX8U(dst, y, stride, x);
				IVP_SA2NX8U_IP (color,  a_store, vdst);
				IVP_SAV2NX8U_XP(color1, a_store, vdst, sizeof(uint8_t) * (p1w - x - 2 * XCHAL_IVPN_SIMD_WIDTH));
				XVTM_IVP_SAV2NX8UPOS_FP(a_store, vdst);
			}
		}
		if(x < p1w)
		{
			int32_t z = 0;
			for ( y = 0; y < p1h; y++)
			{
				z = IVP_ADDMOD16U(z, pmod);
				int32_t k = XT_MIN(z, p - z);

				xb_vec2Nx8U color;
				xb_vec2Nx8U * src_1 = XVTM_OFFSET_PTR_2NX8U(curr_src, -k, stride, x);
				a_load = IVP_LA2NX8U_PP(src_1);
				IVP_LAV2NX8U_XP(color, a_load, src_1, sizeof(uint8_t) * (p1w - x));

				xb_vec2Nx8U * vdst = XVTM_OFFSET_PTR_2NX8U(dst, y, stride, x);
				IVP_SAV2NX8U_XP(color, a_store, vdst, sizeof(uint8_t) * (p1w - x));
				XVTM_IVP_SAV2NX8UPOS_FP(a_store, vdst);
			}
		}
    }
    int32_t ps2x = XT_MIN(ixmin - start_x + 1, ixmax - start_x);
    int32_t pd2x = ixmin - start_x;
    int32_t p2y = -h;
    int32_t p2w = ixmin - (start_x - w);
    int32_t p2h = H_Local + (2 * h);

    curr_src = (uint8_t *)XVTM_OFFSET_PTR_2NX8U(src, p2y, stride, ps2x);
    dst      = (uint8_t *)XVTM_OFFSET_PTR_2NX8U(src, p2y, stride, pd2x);

    x = 0;

    while ( x < p2w - XT_MIN(XVTM_EDGE_REFLECT_I8_SCATTER_MIN_WIDTH, ixmax - ixmin))
    {
    	int32_t wid;
    	xb_vec2Nx8U color, color1, color2, color3;
    	int32_t ytmp = 0;
		int32_t xtmp = 0;

    	int32_t loop_width = XT_MIN(XT_MAX(ixmax,1), p2w-x);
    	while ( xtmp < loop_width )
    	{

    		wid = XT_MIN(p2w - x, 2 * XCHAL_IVPN_SIMD_WIDTH);
    		wid = XT_MAX(XT_MIN(XT_MIN(ixmax-ytmp*2 * XCHAL_IVPN_SIMD_WIDTH, 2 * XCHAL_IVPN_SIMD_WIDTH),wid),1);
    		xb_vec2Nx8U shuffle_vec = IVP_SUB2NX8U(wid - 1, IVP_SEQ2NX8U());

    		xb_vec2Nx8U * vpsrc0 = XVTM_OFFSET_PTR_2NX8U(curr_src, 0, 0, (ytmp)*2 * XCHAL_IVPN_SIMD_WIDTH);
    		xb_vec2Nx8U * vpsrc1 = XVTM_OFFSET_PTR_2NX8U(curr_src, 1, stride, (ytmp)*2 * XCHAL_IVPN_SIMD_WIDTH);
    		vpdst = XVTM_OFFSET_PTR_2NX8U(dst,      0, 0, -x - wid);
    		int32_t y = 0;
    		for (; y < (p2h - 3); y += 4)
    		{
    			a_load = IVP_LA2NX8U_PP(vpsrc0);
    			IVP_LAV2NX8U_XP(color,  a_load, vpsrc0, sizeof(uint8_t) * wid);
    			vpsrc0 = XVTM_OFFSET_PTR_2NX8U(vpsrc0, 1, (2 * stride) - wid, 0);

    			a_load = IVP_LA2NX8U_PP(vpsrc1);
    			IVP_LAV2NX8U_XP(color1, a_load, vpsrc1, sizeof(uint8_t) * wid);
    			vpsrc1 = XVTM_OFFSET_PTR_2NX8U(vpsrc1, 1, (2 * stride) - wid, 0);

    			a_load = IVP_LA2NX8U_PP(vpsrc0);
    			IVP_LAV2NX8U_XP(color2, a_load, vpsrc0, sizeof(uint8_t) * wid);
    			vpsrc0 = XVTM_OFFSET_PTR_2NX8U(vpsrc0, 1, (2 * stride) - wid, 0);

    			a_load = IVP_LA2NX8U_PP(vpsrc1);
    			IVP_LAV2NX8U_XP(color3, a_load, vpsrc1, sizeof(uint8_t) * wid);
    			vpsrc1 = XVTM_OFFSET_PTR_2NX8U(vpsrc1, 1, (2 * stride) - wid, 0);

    			color  = IVP_SHFL2NX8(color,  shuffle_vec);
    			color1 = IVP_SHFL2NX8(color1, shuffle_vec);
    			color2 = IVP_SHFL2NX8(color2, shuffle_vec);
    			color3 = IVP_SHFL2NX8(color3, shuffle_vec);

    			IVP_SAV2NX8U_XP(color,  a_store, vpdst, sizeof(uint8_t) * wid);
    			XVTM_IVP_SAV2NX8UPOS_FP(a_store, vpdst);
    			vpdst = XVTM_OFFSET_PTR_2NX8U(vpdst, 1, stride - wid, 0);

    			IVP_SAV2NX8U_XP(color1, a_store, vpdst, sizeof(uint8_t) * wid);
    			XVTM_IVP_SAV2NX8UPOS_FP(a_store, vpdst);
    			vpdst = XVTM_OFFSET_PTR_2NX8U(vpdst, 1, stride - wid, 0);

    			IVP_SAV2NX8U_XP(color2, a_store, vpdst, sizeof(uint8_t) * wid);
    			XVTM_IVP_SAV2NX8UPOS_FP(a_store, vpdst);
    			vpdst = XVTM_OFFSET_PTR_2NX8U(vpdst, 1, stride - wid, 0);

    			IVP_SAV2NX8U_XP(color3, a_store, vpdst, sizeof(uint8_t) * wid);
    			XVTM_IVP_SAV2NX8UPOS_FP(a_store, vpdst);
    			vpdst = XVTM_OFFSET_PTR_2NX8U(vpdst, 1, stride - wid, 0);
    		}

    		for (; y < p2h; y++)
    		{
    			a_load = IVP_LA2NX8U_PP(vpsrc0);
    			IVP_LAV2NX8U_XP(color, a_load, vpsrc0, sizeof(uint8_t) * wid);
    			vpsrc0 = XVTM_OFFSET_PTR_2NX8U(vpsrc0, 1, stride - wid, 0);

    			color = IVP_SHFL2NX8(color, shuffle_vec);
    			IVP_SAV2NX8U_XP(color, a_store, vpdst, sizeof(uint8_t) * wid);
    			XVTM_IVP_SAV2NX8UPOS_FP(a_store, vpdst);
    			vpdst = XVTM_OFFSET_PTR_2NX8U(vpdst, 1, stride - wid, 0);
    		}
    		x += wid;
    		ytmp+=1;
			xtmp+=wid;

    	}
    	curr_src = curr_src - (ixmax);
    }
    int32_t s = 0;
    if(x < p2w)
    {
    	curr_src = (uint8_t *)XVTM_OFFSET_PTR_2NX8U(src, p2y, stride, ps2x);
    	dst      = (uint8_t *)XVTM_OFFSET_PTR_2NX8U(src, p2y, stride, pd2x);

    	int32_t wid = XT_MIN(p2w - x, 2 * XCHAL_IVPN_SIMD_WIDTH);
    	wid = XT_MAX(XT_MIN(wid, ixmax - ixmin), 1);
    	xb_vecNx8U * lsrc = XVTM_OFFSET_PTR_NX8U(curr_src, 0, 0, -x);
    	xb_vecNx8U * ldst = XVTM_OFFSET_PTR_NX8U(dst,      0, 0, -x - wid);

    	int32_t q15_inv_w = 1 + ((1<<15)/wid);
    	int32_t ystep = q15_inv_w >> 10;

    	xb_vecNx16 shs = IVP_SEQNX16();
    	xb_vecNx16 shd = IVP_SUBNX16(wid - 1, IVP_SEQNX16());
    	xb_vecNx16 shy = IVP_PACKVRNRNX48(IVP_MULUUNX16(shs, q15_inv_w), 15);
    	IVP_MULANX16PACKL(shs, stride - wid, shy);
    	IVP_MULANX16PACKL(shd, stride + wid, shy);

    	for ( s = 0; s < p2h; s += ystep)
    	{
    		vboolN vb = IVP_LTRSN((p2h-s)*wid);
    		uint8_t * src0 = (uint8_t *)XVTM_OFFSET_PTR_NX8U(lsrc, s, stride, 0);
    		uint8_t * dst0 = (uint8_t *)XVTM_OFFSET_PTR_NX8U(ldst, s, stride, 0);

            xb_gsr gr0;
            shs = IVP_MOVNX16T(shs, 0, vb);
            shd = IVP_MOVNX16T(shd, 0, vb);
            gr0 = XVTM_IVP_GATHERANX8UT_V(src0, shs, vb, 1);
            xb_vecNx16U v0 = IVP_GATHERDNX8U(gr0);
            XVTM_IVP_SCATTERNX8UT(v0, dst0, shd, vb);
        }
    }

    int32_t ps3x = ixmax - start_x;
    int32_t pd3x = (ixmax + 1 ) - start_x;
    int32_t p3y = -h;
    int32_t p3w = (start_x + W_Local + w) - 1 - ixmax;
    int32_t p3h = H_Local + (2 * h);

    curr_src = (uint8_t *)XVTM_OFFSET_PTR_2NX8U(src, p3y, stride, ps3x);
    dst      = (uint8_t *)XVTM_OFFSET_PTR_2NX8U(src, p3y, stride, pd3x);
    x = 0;

    while ( x < p3w - XT_MIN(XVTM_EDGE_REFLECT_I8_SCATTER_MIN_WIDTH, ixmax - ixmin))
    {
    	int32_t wid;
    	xb_vec2Nx8U color, color1, color2, color3;
    	int32_t ytmp = 0;
		int32_t xtmp = 0;

    	int32_t loop_width = XT_MIN(XT_MAX(ixmax,1), p3w-x);
    	while ( xtmp < loop_width)
    	{
    		wid = XT_MIN(p3w - x, 2 * XCHAL_IVPN_SIMD_WIDTH);
    		wid = XT_MAX(XT_MIN(XT_MIN(ixmax-(ytmp*2 * XCHAL_IVPN_SIMD_WIDTH), 2 * XCHAL_IVPN_SIMD_WIDTH),wid),1);

    		xb_vec2Nx8U shuffle_vec = IVP_SUB2NX8U(wid - 1, IVP_SEQ2NX8U());

    		xb_vec2Nx8U * vpsrc0 = XVTM_OFFSET_PTR_2NX8U(curr_src, 0, 0, - wid-((ytmp)*2 * XCHAL_IVPN_SIMD_WIDTH));
    		xb_vec2Nx8U * vpsrc1 = XVTM_OFFSET_PTR_2NX8U(curr_src, 1, stride, - wid-((ytmp)*2 * XCHAL_IVPN_SIMD_WIDTH));
    		vpdst  = XVTM_OFFSET_PTR_2NX8U(dst,      0, 0, x);
    		int32_t y = 0;
    		for (; y < (p3h - 3) ; y += 4)
    		{
    			a_load = IVP_LA2NX8U_PP(vpsrc0);
    			IVP_LAV2NX8U_XP(color,  a_load, vpsrc0, sizeof(uint8_t) * wid);
    			vpsrc0 = XVTM_OFFSET_PTR_2NX8U(vpsrc0, 1, (2 * stride) - wid, 0);

    			a_load = IVP_LA2NX8U_PP(vpsrc1);
    			IVP_LAV2NX8U_XP(color1, a_load, vpsrc1, sizeof(uint8_t) * wid);
    			vpsrc1 = XVTM_OFFSET_PTR_2NX8U(vpsrc1, 1, (2 * stride) - wid, 0);

    			a_load = IVP_LA2NX8U_PP(vpsrc0);
    			IVP_LAV2NX8U_XP(color2, a_load, vpsrc0, sizeof(uint8_t) * wid);
    			vpsrc0 = XVTM_OFFSET_PTR_2NX8U(vpsrc0, 1, (2 * stride) - wid, 0);

    			a_load = IVP_LA2NX8U_PP(vpsrc1);
    			IVP_LAV2NX8U_XP(color3, a_load, vpsrc1, sizeof(uint8_t) * wid);
    			vpsrc1 = XVTM_OFFSET_PTR_2NX8U(vpsrc1, 1, (2 * stride) - wid, 0);

    			color  = IVP_SHFL2NX8(color,  shuffle_vec);
    			color1 = IVP_SHFL2NX8(color1, shuffle_vec);
    			color2 = IVP_SHFL2NX8(color2, shuffle_vec);
    			color3 = IVP_SHFL2NX8(color3, shuffle_vec);

    			IVP_SAV2NX8U_XP(color, a_store, vpdst, sizeof(uint8_t) * wid);
    			XVTM_IVP_SAV2NX8UPOS_FP(a_store, vpdst);
    			vpdst = XVTM_OFFSET_PTR_2NX8U(vpdst, 1, stride - wid, 0);

    			IVP_SAV2NX8U_XP(color1, a_store, vpdst, sizeof(uint8_t) * wid);
    			XVTM_IVP_SAV2NX8UPOS_FP(a_store, vpdst);
    			vpdst = XVTM_OFFSET_PTR_2NX8U(vpdst, 1, stride - wid, 0);

    			IVP_SAV2NX8U_XP(color2, a_store, vpdst, sizeof(uint8_t) * wid);
    			XVTM_IVP_SAV2NX8UPOS_FP(a_store, vpdst);
    			vpdst = XVTM_OFFSET_PTR_2NX8U(vpdst, 1, stride - wid, 0);

    			IVP_SAV2NX8U_XP(color3, a_store, vpdst, sizeof(uint8_t) * wid);
    			XVTM_IVP_SAV2NX8UPOS_FP(a_store, vpdst);
    			vpdst = XVTM_OFFSET_PTR_2NX8U(vpdst, 1, stride - wid, 0);
    		}

    			for (; y < p3h; y++)
    			{
    				a_load = IVP_LA2NX8U_PP(vpsrc0);
    				IVP_LAV2NX8U_XP(color, a_load, vpsrc0, sizeof(uint8_t) * wid);
    				vpsrc0 = XVTM_OFFSET_PTR_2NX8U(vpsrc0, 1, stride - wid, 0);

    				color = IVP_SHFL2NX8(color, shuffle_vec);
    				IVP_SAV2NX8U_XP(color, a_store, vpdst, sizeof(uint8_t) * wid);
    				XVTM_IVP_SAV2NX8UPOS_FP(a_store, vpdst);
    				vpdst = XVTM_OFFSET_PTR_2NX8U(vpdst, 1, stride - wid, 0);
    			}
    			x += wid;
    			ytmp+=1;
				xtmp+=wid;
    	}
    	curr_src = curr_src+(ixmax);

    }

    if(x < p3w)
    {
    	curr_src = (uint8_t *)XVTM_OFFSET_PTR_2NX8U(src, p3y, stride, ps3x);
    	dst      = (uint8_t *)XVTM_OFFSET_PTR_2NX8U(src, p3y, stride, pd3x);

    	int32_t wid = XT_MIN(p3w - x, 2 * XCHAL_IVPN_SIMD_WIDTH);
    	wid = XT_MAX(XT_MIN(wid, ixmax - ixmin), 1);
    	xb_vecNx8U * lsrc = XVTM_OFFSET_PTR_NX8U(curr_src, 0, 0, x - wid);
    	xb_vecNx8U * ldst = XVTM_OFFSET_PTR_NX8U(dst,      0, 0, x);

    	int32_t q15_inv_w = 1 + ((1<<15)/wid);
    	int32_t ystep = q15_inv_w >> 10;

        xb_vecNx16 shs = IVP_SEQNX16();
        xb_vecNx16 shd = IVP_SUBNX16(wid - 1, IVP_SEQNX16());
        xb_vecNx16 shy = IVP_PACKVRNRNX48(IVP_MULUUNX16(shs, q15_inv_w), 15);
        IVP_MULANX16PACKL(shs, stride - wid, shy);
        IVP_MULANX16PACKL(shd, stride + wid, shy);

        for ( s = 0; s < p3h; s += ystep)
        {
            vboolN vb = IVP_LTRSN((p3h-s)*wid);
            uint8_t * src0 = (uint8_t *)XVTM_OFFSET_PTR_NX8U(lsrc, s, stride, 0);
            uint8_t * dst0 = (uint8_t *)XVTM_OFFSET_PTR_NX8U(ldst, s, stride, 0);

            xb_gsr gr0;
            shs = IVP_MOVNX16T(shs, 0, vb);
            shd = IVP_MOVNX16T(shd, 0, vb);
            gr0 = XVTM_IVP_GATHERANX8UT_V(src0, shs, vb, 1);
            xb_vecNx16U v0 = IVP_GATHERDNX8U(gr0);
            XVTM_IVP_SCATTERNX8UT(v0, dst0, shd, vb);
        }
    }
    IVP_SCATTERW();
}

static void xvExtendEdgesReflect101_I16(xvTile const * tile, int32_t frame_width, int32_t frame_height)
{

	int32_t w = XV_TILE_GET_EDGE_WIDTH(tile);
	int32_t h = XV_TILE_GET_EDGE_HEIGHT(tile);

	int16_t* __restrict src = (int16_t *)XV_TILE_GET_DATA_PTR(tile);
	int32_t stride = XV_TILE_GET_PITCH(tile);

	int32_t start_x = XV_TILE_GET_X_COORD(tile);
	int32_t start_y = XV_TILE_GET_Y_COORD(tile);

	int32_t W_Local = XV_TILE_GET_WIDTH(tile);
	int32_t H_Local = XV_TILE_GET_HEIGHT(tile);

	int32_t usr_tmp =0;
	// find intersection of tile/frame
	int32_t ixmin = XT_MAX(start_x - w, 0);
	int32_t ixmax = XT_MIN(start_x + W_Local + w - 1, frame_width - 1);
	int32_t iymin = XT_MAX(start_y - h, 0);
	int32_t iymax = XT_MIN(start_y + H_Local + h - 1, frame_height - 1);


	int32_t p0x = ixmin - start_x;
	int32_t ps0y = iymin - start_y;
	int32_t pd0y = iymin - start_y - 1;
	int32_t p0w = (ixmax - ixmin) + 1;
	int32_t p0h = iymin - (start_y - h);

	int16_t* curr_src = (int16_t *)XVTM_OFFSET_PTR_NX16(src, ps0y, stride, p0x);
	int16_t* dst      = (int16_t *)XVTM_OFFSET_PTR_NX16(src, pd0y, stride, p0x);

	xb_vecNx16 *vpdst;

	valign a_store = IVP_ZALIGN();
	valign a_load;

	int32_t p = XT_MAX(1, 2 * (iymax - iymin + 1) - 2);
	int32_t pmod = (p << 16) + 1;
	int32_t x = 0;
	int32_t y = 0;
	if(p0h > 0)
	{
		for (; x < (p0w - (3 * XCHAL_IVPN_SIMD_WIDTH)); x += 4 * XCHAL_IVPN_SIMD_WIDTH)
		{
			int32_t z = 0;
			for ( y = 0; y < p0h; y++)
			{
				z = IVP_ADDMOD16U(z, pmod);
				int32_t k = XT_MIN(z, p - z);

				xb_vecNx16 color, color1, color2, color3;
				xb_vecNx16 *src_1 = XVTM_OFFSET_PTR_NX16(curr_src, k, stride, x);
				a_load = IVP_LANX16_PP(src_1);
				IVP_LANX16_IP (color, a_load, src_1);
				IVP_LANX16_IP (color1, a_load, src_1);
				IVP_LANX16_IP (color2, a_load, src_1);
				IVP_LAVNX16_XP(color3, a_load, src_1, sizeof(int16_t) * (p0w - x - 3 * XCHAL_IVPN_SIMD_WIDTH));

				xb_vecNx16 *vdst = XVTM_OFFSET_PTR_NX16(dst, -y, stride, x);
				IVP_SANX16_IP (color, a_store, vdst);
				IVP_SANX16_IP (color1, a_store, vdst);
				IVP_SANX16_IP (color2, a_store, vdst);
				IVP_SAVNX16_XP(color3, a_store, vdst, sizeof(int16_t) * (p0w - x - 3 * XCHAL_IVPN_SIMD_WIDTH));
				XVTM_IVP_SAVNX16POS_FP(a_store, vdst);
			}
		}
		if(x < (p0w - (2 * XCHAL_IVPN_SIMD_WIDTH)))
		{
			int32_t z = 0;
			for ( y = 0; y < p0h; y++)
			{
				z = IVP_ADDMOD16U(z, pmod);
				int32_t k = XT_MIN(z, p - z);

				xb_vecNx16 color, color1, color2;
				xb_vecNx16 *src_1 = XVTM_OFFSET_PTR_NX16(curr_src, k, stride, x);
				a_load = IVP_LANX16_PP(src_1);
				IVP_LANX16_IP (color,  a_load, src_1);
				IVP_LANX16_IP (color1, a_load, src_1);
				IVP_LAVNX16_XP(color2, a_load, src_1, sizeof(int16_t) * (p0w - x - 2 * XCHAL_IVPN_SIMD_WIDTH));

				xb_vecNx16 *vdst = XVTM_OFFSET_PTR_NX16(dst, -y, stride, x);
				IVP_SANX16_IP (color,  a_store, vdst);
				IVP_SANX16_IP (color1, a_store, vdst);
				IVP_SAVNX16_XP(color2, a_store, vdst, sizeof(int16_t) * (p0w - x - 2 * XCHAL_IVPN_SIMD_WIDTH));
				XVTM_IVP_SAVNX16POS_FP(a_store, vdst);
			}
		}
		else if(x < (p0w - XCHAL_IVPN_SIMD_WIDTH))
		{
			int32_t z = 0;
			for ( y = 0; y < p0h; y++)
			{
				z = IVP_ADDMOD16U(z, pmod);
				int32_t k = XT_MIN(z, p - z);

				xb_vecNx16 color, color1;
				xb_vecNx16 *src_1 = XVTM_OFFSET_PTR_NX16(curr_src, k, stride, x);
				a_load = IVP_LANX16_PP(src_1);
				IVP_LANX16_IP (color, a_load, src_1);
				IVP_LAVNX16_XP(color1, a_load, src_1, sizeof(int16_t) * (p0w - x - XCHAL_IVPN_SIMD_WIDTH));

				xb_vecNx16 *vdst = XVTM_OFFSET_PTR_NX16(dst, -y, stride, x);
				IVP_SANX16_IP (color, a_store, vdst);
				IVP_SAVNX16_XP(color1, a_store, vdst, sizeof(int16_t) * (p0w - x - XCHAL_IVPN_SIMD_WIDTH));
				XVTM_IVP_SAVNX16POS_FP(a_store, vdst);
			}
		}
		else if(x < p0w)
		{
			int32_t z = 0;
			for ( y = 0; y < p0h; y++)
			{
				z = IVP_ADDMOD16U(z, pmod);
				int32_t k = XT_MIN(z, p - z);

				xb_vecNx16 color;
				xb_vecNx16 *src_1 = XVTM_OFFSET_PTR_NX16(curr_src, k, stride, x);
				a_load = IVP_LANX16_PP(src_1);
				IVP_LAVNX16_XP(color, a_load, src_1, sizeof(int16_t) * (p0w - x));

				xb_vecNx16 *vdst = XVTM_OFFSET_PTR_NX16(dst, -y, stride, x);
				IVP_SAVNX16_XP(color, a_store, vdst, sizeof(int16_t) * (p0w - x));
				XVTM_IVP_SAVNX16POS_FP(a_store, vdst);
			}
		}
		else
		{
			//do nothing
		}
	}
	int32_t p1x = ixmin - start_x;
	int32_t ps1y = iymax - start_y;
	int32_t pd1y = (iymax + 1) - start_y;
	int32_t p1w = (ixmax - ixmin) + 1;
	int32_t p1h = (start_y + H_Local + h) - 1 - iymax;

	curr_src = (int16_t *)XVTM_OFFSET_PTR_NX16(src, ps1y, stride, p1x);
	dst      = (int16_t *)XVTM_OFFSET_PTR_NX16(src, pd1y, stride, p1x);
	x = 0;
	if(p1h > 0)
	{
		for (; x < (p1w - (3 * XCHAL_IVPN_SIMD_WIDTH)); x += 4 * XCHAL_IVPN_SIMD_WIDTH)
		{
			int32_t z = 0;
			for ( y = 0; y < p1h; y++)
			{
				z = IVP_ADDMOD16U(z, pmod);
				int32_t k = XT_MIN(z, p - z);

				xb_vecNx16 color, color1, color2, color3;
				xb_vecNx16 * src_1 = XVTM_OFFSET_PTR_NX16(curr_src, -k, stride, x);
				a_load = IVP_LANX16_PP(src_1);
				IVP_LANX16_IP (color,  a_load, src_1);
				IVP_LANX16_IP (color1, a_load, src_1);
				IVP_LANX16_IP (color2, a_load, src_1);
				IVP_LAVNX16_XP(color3, a_load, src_1, sizeof(int16_t) * (p1w - x - 3 * XCHAL_IVPN_SIMD_WIDTH));

				xb_vecNx16 *vdst = XVTM_OFFSET_PTR_NX16(dst, y, stride, x);
				IVP_SANX16_IP (color,  a_store, vdst);
				IVP_SANX16_IP (color1, a_store, vdst);
				IVP_SANX16_IP (color2, a_store, vdst);
				IVP_SAVNX16_XP(color3, a_store, vdst, sizeof(int16_t) * (p1w - x - 3 * XCHAL_IVPN_SIMD_WIDTH));
				XVTM_IVP_SAVNX16POS_FP(a_store, vdst);
			}
		}
		if(x < (p1w - (2 * XCHAL_IVPN_SIMD_WIDTH)))
		{
			int32_t z = 0;
			for ( y = 0; y < p1h; y++)
			{
				z = IVP_ADDMOD16U(z, pmod);
				int32_t k = XT_MIN(z, p - z);

				xb_vecNx16 color, color1, color2;
				xb_vecNx16 * src_1 = XVTM_OFFSET_PTR_NX16(curr_src, -k, stride, x);
				a_load = IVP_LANX16_PP(src_1);
				IVP_LANX16_IP (color,  a_load, src_1);
				IVP_LANX16_IP (color1, a_load, src_1);
				IVP_LAVNX16_XP(color2, a_load, src_1, sizeof(int16_t) * (p1w - x - 2 * XCHAL_IVPN_SIMD_WIDTH));

				xb_vecNx16 *vdst = XVTM_OFFSET_PTR_NX16(dst, y, stride, x);
				IVP_SANX16_IP (color,  a_store, vdst);
				IVP_SANX16_IP (color1, a_store, vdst);
				IVP_SAVNX16_XP(color2, a_store, vdst, sizeof(int16_t) * (p1w - x - 2 * XCHAL_IVPN_SIMD_WIDTH));
				XVTM_IVP_SAVNX16POS_FP(a_store, vdst);
			}
		}
		else if(x < (p1w - XCHAL_IVPN_SIMD_WIDTH))
		{
			int32_t z = 0;
			for ( y = 0; y < p1h; y++)
			{
				z = IVP_ADDMOD16U(z, pmod);
				int32_t k = XT_MIN(z, p - z);

				xb_vecNx16 color, color1;
				xb_vecNx16 * src_0_t = XVTM_OFFSET_PTR_NX16(curr_src, -k, stride, x);
				a_load = IVP_LANX16_PP(src_0_t);
				IVP_LANX16_IP (color,  a_load, src_0_t);
				IVP_LAVNX16_XP(color1, a_load, src_0_t, sizeof(int16_t) * (p1w - x - XCHAL_IVPN_SIMD_WIDTH));

				xb_vecNx16 *vdst = XVTM_OFFSET_PTR_NX16(dst, y, stride, x);
				IVP_SANX16_IP (color,  a_store, vdst);
				IVP_SAVNX16_XP(color1, a_store, vdst, sizeof(int16_t) * (p1w - x - XCHAL_IVPN_SIMD_WIDTH));
				XVTM_IVP_SAVNX16POS_FP(a_store, vdst);
			}
		}
		else if(x < p1w)
		{
			int32_t z = 0;
			for ( y = 0; y < p1h; y++)
			{
				z = IVP_ADDMOD16U(z, pmod);
				int32_t k = XT_MIN(z, p - z);

				xb_vecNx16 color;
				xb_vecNx16 * src_1 = XVTM_OFFSET_PTR_NX16(curr_src, -k, stride, x);
				a_load = IVP_LANX16_PP(src_1);
				IVP_LAVNX16_XP(color, a_load, src_1, sizeof(int16_t) * (p1w - x));

				xb_vecNx16 *vdst = XVTM_OFFSET_PTR_NX16(dst, y, stride, x);
				IVP_SAVNX16_XP(color, a_store, vdst, sizeof(int16_t) * (p1w - x));
				XVTM_IVP_SAVNX16POS_FP(a_store, vdst);
			}
		}
		else
		{
			//do nothing
		}
	}
	int32_t ps2x = XT_MIN(ixmin - start_x + 1, ixmax - start_x);
	int32_t pd2x = ixmin - start_x;
	int32_t p2y = -h;
	int32_t p2w = ixmin - (start_x - w);
	int32_t p2h = H_Local + (2 * h);

	curr_src = (int16_t *)XVTM_OFFSET_PTR_NX16(src, p2y, stride, ps2x);
	dst      = (int16_t *)XVTM_OFFSET_PTR_NX16(src, p2y, stride, pd2x);
	x = 0;


	while (x < p2w- XT_MIN(XVTM_EDGE_REFLECT_I16_SCATTER_MIN_WIDTH, ixmax - ixmin))
	{
		int32_t wid;
		xb_vecNx16 color, color1, color2, color3;

		xb_vecNx16 * vpsrc0;
		xb_vecNx16 * vpsrc1;

		int32_t ytmp = 0;
		int32_t xtmp = 0;

    	int32_t loop_width = XT_MIN(XT_MAX(ixmax,1), p2w-x);
    	while ( xtmp < loop_width)
		{
			wid = XT_MIN(p2w - x, XCHAL_IVPN_SIMD_WIDTH);
			wid = XT_MAX(XT_MIN(XT_MIN(ixmax-ytmp*XCHAL_IVPN_SIMD_WIDTH, XCHAL_IVPN_SIMD_WIDTH),wid),1);
			xb_vecNx16 ind = IVP_SUBNX16(wid - 1, IVP_SEQNX16());
			vsaN index = XVTM_IVP_MOVVSV(ind, 0);
			vpsrc0 = XVTM_OFFSET_PTR_NX16(curr_src, 0, 0, (ytmp)*XCHAL_IVPN_SIMD_WIDTH);
			vpsrc1 = XVTM_OFFSET_PTR_NX16(curr_src, 1, stride, (ytmp)*XCHAL_IVPN_SIMD_WIDTH);
			vpdst  = XVTM_OFFSET_PTR_NX16(dst,      0, 0, -x - wid);

			int32_t y = 0;
			for (; y < (p2h - 3); y += 4)
			{
				a_load = IVP_LANX16_PP(vpsrc0);
				IVP_LAVNX16_XP(color,  a_load, vpsrc0, sizeof(int16_t) * wid);
				vpsrc0 = XVTM_OFFSET_PTR_NX16(vpsrc0, 1, (2 * stride) - wid, 0);

				a_load = IVP_LANX16_PP(vpsrc1);
				IVP_LAVNX16_XP(color1, a_load, vpsrc1, sizeof(int16_t) * wid);
				vpsrc1 = XVTM_OFFSET_PTR_NX16(vpsrc1, 1, (2 * stride) - wid, 0);

				a_load = IVP_LANX16_PP(vpsrc0);
				IVP_LAVNX16_XP(color2, a_load, vpsrc0, sizeof(int16_t) * wid);
				vpsrc0 = XVTM_OFFSET_PTR_NX16(vpsrc0, 1, (2 * stride) - wid, 0);

				a_load = IVP_LANX16_PP(vpsrc1);
				IVP_LAVNX16_XP(color3, a_load, vpsrc1, sizeof(int16_t) * wid);
				vpsrc1 = XVTM_OFFSET_PTR_NX16(vpsrc1, 1, (2 * stride) - wid, 0);

				color  =  IVP_SELNX16(color,  color,  index);
				color1 =  IVP_SELNX16(color1, color1, index);
				color2 =  IVP_SELNX16(color2, color2, index);
				color3 =  IVP_SELNX16(color3, color3, index);

				IVP_SAVNX16_XP(color,  a_store, vpdst, sizeof(int16_t) * wid);
				XVTM_IVP_SAVNX16POS_FP(a_store, vpdst);
				vpdst = XVTM_OFFSET_PTR_NX16(vpdst, 1, stride - wid, 0);

				IVP_SAVNX16_XP(color1, a_store, vpdst, sizeof(int16_t) * wid);
				XVTM_IVP_SAVNX16POS_FP(a_store, vpdst);
				vpdst = XVTM_OFFSET_PTR_NX16(vpdst, 1, stride - wid, 0);

				IVP_SAVNX16_XP(color2, a_store, vpdst, sizeof(int16_t) * wid);
				XVTM_IVP_SAVNX16POS_FP(a_store, vpdst);
				vpdst = XVTM_OFFSET_PTR_NX16(vpdst, 1, stride - wid, 0);

				IVP_SAVNX16_XP(color3, a_store, vpdst, sizeof(int16_t) * wid);
				XVTM_IVP_SAVNX16POS_FP(a_store, vpdst);
				vpdst = XVTM_OFFSET_PTR_NX16(vpdst, 1, stride - wid, 0);
			}
			for (; y < p2h; y++)
			{
				a_load = IVP_LANX16_PP(vpsrc0);
				IVP_LAVNX16_XP(color, a_load, vpsrc0, sizeof(int16_t) * wid);
				vpsrc0 = XVTM_OFFSET_PTR_NX16(vpsrc0, 1, stride - wid, 0);

				color =  IVP_SELNX16(color, color, index);

				IVP_SAVNX16_XP(color, a_store, vpdst, sizeof(int16_t) * wid);
				XVTM_IVP_SAVNX16POS_FP(a_store, vpdst);
				vpdst = XVTM_OFFSET_PTR_NX16(vpdst, 1, stride - wid, 0);
			}
			x += wid;
			ytmp+=1;
			xtmp+=wid;

		}
		curr_src = curr_src - (ixmax);


	}
	int32_t s = 0;
	if(x < p2w)
	{
		curr_src = (int16_t *)XVTM_OFFSET_PTR_NX16(src, p2y, stride, ps2x);
		dst      = (int16_t *)XVTM_OFFSET_PTR_NX16(src, p2y, stride, pd2x);

		int32_t wid = XT_MIN(p2w - x, XCHAL_IVPN_SIMD_WIDTH);
		wid = XT_MAX(XT_MIN(wid, ixmax - ixmin), 1);
		xb_vecNx16 * lsrc = XVTM_OFFSET_PTR_NX16(curr_src, 0, 0, -x + ( XCHAL_IVPN_SIMD_WIDTH * usr_tmp));
		xb_vecNx16 * ldst  = XVTM_OFFSET_PTR_NX16(dst,      0, 0, -x - wid);

		int32_t q15_inv_w = 1 + ((1<<15)/wid);
		int32_t ystep = q15_inv_w >> 10;

		xb_vecNx16 shs = IVP_SEQNX16();
		xb_vecNx16 shd = IVP_SUBNX16(wid - 1, IVP_SEQNX16());
		xb_vecNx16 shy = IVP_PACKVRNRNX48(IVP_MULUUNX16(shs, q15_inv_w), 15);
		IVP_MULANX16PACKL(shs, stride - wid, shy);
		IVP_MULANX16PACKL(shd, stride + wid, shy);

		shd = IVP_SLLINX16(shd, 1);
		shs = IVP_SLLINX16(shs, 1);

		for ( s = 0; s < p2h; s += ystep)
		{
			vboolN vb = IVP_LTRSN((p2h-s)*wid);
			int16_t * src0 = (int16_t *)XVTM_OFFSET_PTR_NX16(lsrc, s, stride, 0);
			int16_t * dst0 = (int16_t *)XVTM_OFFSET_PTR_NX16(ldst, s, stride, 0);

			xb_gsr gr0;
			shs = IVP_MOVNX16T(shs, 0, vb);
			shd = IVP_MOVNX16T(shd, 0, vb);
			gr0 = XVTM_IVP_GATHERANX16T_V(src0, shs, vb, 1);
			xb_vecNx16 v0 = IVP_GATHERDNX16(gr0);
			XVTM_IVP_SCATTERNX16T(v0, dst0, shd, vb);
		}
	}

	int32_t ps3x = ixmax - start_x;
	int32_t pd3x = (ixmax + 1) - start_x;
	int32_t p3y = -h;
	int32_t p3w = (start_x + W_Local + w) - 1 - ixmax;
	int32_t p3h = H_Local + (2 * h);

	curr_src = (int16_t *)XVTM_OFFSET_PTR_NX16(src, p3y, stride, ps3x);
	dst      = (int16_t *)XVTM_OFFSET_PTR_NX16(src, p3y, stride, pd3x);

	x = 0;

	while( x < p3w- XT_MIN(XVTM_EDGE_REFLECT_I16_SCATTER_MIN_WIDTH, ixmax - ixmin))
	{
		int32_t wid;
		xb_vecNx16 color, color1, color2, color3;

		xb_vecNx16 * vpsrc0;
		xb_vecNx16 * vpsrc1;

		int32_t ytmp = 0;
		int32_t xtmp = 0;

    	int32_t loop_width = XT_MIN(XT_MAX(ixmax,1), p3w-x);
    	while ( xtmp < loop_width)
		{
			wid = XT_MIN(p3w - x, XCHAL_IVPN_SIMD_WIDTH);
			wid = XT_MAX(XT_MIN(XT_MIN(ixmax-ytmp*XCHAL_IVPN_SIMD_WIDTH, XCHAL_IVPN_SIMD_WIDTH),wid),1);
			xb_vecNx16 ind = IVP_SUBNX16(wid - 1, IVP_SEQNX16());
			vsaN index = XVTM_IVP_MOVVSV(ind, 0);

			vpdst = XVTM_OFFSET_PTR_NX16(dst, 0, 0, x);
			vpsrc0 = XVTM_OFFSET_PTR_NX16(curr_src, 0, 0, -wid-((ytmp)*XCHAL_IVPN_SIMD_WIDTH));
			vpsrc1 = XVTM_OFFSET_PTR_NX16(curr_src, 1, stride, -wid-((ytmp)*XCHAL_IVPN_SIMD_WIDTH));

			int32_t y = 0;

			for (; y < (p3h - 3); y += 4)
			{
				a_load = IVP_LANX16_PP(vpsrc0);
				IVP_LAVNX16_XP(color,  a_load, vpsrc0, sizeof(int16_t) * wid);
				vpsrc0 = XVTM_OFFSET_PTR_NX16(vpsrc0, 1, (2 * stride) - wid, 0);

				a_load = IVP_LANX16_PP(vpsrc1);
				IVP_LAVNX16_XP(color1, a_load, vpsrc1, sizeof(int16_t) * wid);
				vpsrc1 = XVTM_OFFSET_PTR_NX16(vpsrc1, 1, (2 * stride) - wid, 0);

				a_load = IVP_LANX16_PP(vpsrc0);
				IVP_LAVNX16_XP(color2, a_load, vpsrc0, sizeof(int16_t) * wid);
				vpsrc0 = XVTM_OFFSET_PTR_NX16(vpsrc0, 1, (2 * stride) - wid, 0);

				a_load = IVP_LANX16_PP(vpsrc1);
				IVP_LAVNX16_XP(color3, a_load, vpsrc1, sizeof(int16_t) * wid);
				vpsrc1 = XVTM_OFFSET_PTR_NX16(vpsrc1, 1, (2 * stride) - wid, 0);

				color  =  IVP_SELNX16(color,  color,  index);
				color1 =  IVP_SELNX16(color1, color1, index);
				color2 =  IVP_SELNX16(color2, color2, index);
				color3 =  IVP_SELNX16(color3, color3, index);

				IVP_SAVNX16_XP(color,  a_store, vpdst, sizeof(int16_t) * wid);
				XVTM_IVP_SAVNX16POS_FP(a_store, vpdst);
				vpdst = XVTM_OFFSET_PTR_NX16(vpdst, 1, stride - wid, 0);

				IVP_SAVNX16_XP(color1, a_store, vpdst, sizeof(int16_t) * wid);
				XVTM_IVP_SAVNX16POS_FP(a_store, vpdst);
				vpdst = XVTM_OFFSET_PTR_NX16(vpdst, 1, stride - wid, 0);

				IVP_SAVNX16_XP(color2, a_store, vpdst, sizeof(int16_t) * wid);
				XVTM_IVP_SAVNX16POS_FP(a_store, vpdst);
				vpdst = XVTM_OFFSET_PTR_NX16(vpdst, 1, stride - wid, 0);

				IVP_SAVNX16_XP(color3, a_store, vpdst, sizeof(int16_t) * wid);
				XVTM_IVP_SAVNX16POS_FP(a_store, vpdst);
				vpdst = XVTM_OFFSET_PTR_NX16(vpdst, 1, stride - wid, 0);
			}
			for (; y < p3h; y++)
			{
				a_load = IVP_LANX16_PP(vpsrc0);
				IVP_LAVNX16_XP(color, a_load, vpsrc0, sizeof(int16_t) * wid);
				vpsrc0 = XVTM_OFFSET_PTR_NX16(vpsrc0, 1, stride - wid, 0);

				color =  IVP_SELNX16(color, color, index);

				IVP_SAVNX16_XP(color, a_store, vpdst, sizeof(int16_t) * wid);
				XVTM_IVP_SAVNX16POS_FP(a_store, vpdst);
				vpdst = XVTM_OFFSET_PTR_NX16(vpdst, 1, stride - wid, 0);
			}

			x += wid;
			ytmp+=1;
			xtmp+=wid;
		}
		curr_src = curr_src+(ixmax);
	}

	if(x < p3w)
	{
		curr_src = (int16_t *)XVTM_OFFSET_PTR_NX16(src, p3y, stride, ps3x);
		dst      = (int16_t *)XVTM_OFFSET_PTR_NX16(src, p3y, stride, pd3x);

		int32_t wid = XT_MIN(p3w - x, XCHAL_IVPN_SIMD_WIDTH);
		wid = XT_MAX(XT_MIN(wid, ixmax - ixmin), 1);
		xb_vecNx16 * lsrc = XVTM_OFFSET_PTR_NX16(curr_src, 0, 0, x - wid);
		xb_vecNx16 * ldst = XVTM_OFFSET_PTR_NX16(dst, 0, 0, x);

		int32_t q15_inv_w = 1 + ((1<<15)/wid);
		int32_t ystep = q15_inv_w >> 10;

		xb_vecNx16 shs = IVP_SEQNX16();
		xb_vecNx16 shd = IVP_SUBNX16(wid - 1, IVP_SEQNX16());
		xb_vecNx16 shy = IVP_PACKVRNRNX48(IVP_MULUUNX16(shs, q15_inv_w), 15);
		IVP_MULANX16PACKL(shs, stride - wid, shy);
		IVP_MULANX16PACKL(shd, stride + wid, shy);

		shd = IVP_SLLINX16(shd, 1);
		shs = IVP_SLLINX16(shs, 1);

		for ( s = 0; s < p3h; s += ystep)
		{
			vboolN vb = IVP_LTRSN((p3h-s)*wid);
			int16_t * src0 = (int16_t *)XVTM_OFFSET_PTR_NX16(lsrc, s, stride, 0);
			int16_t * dst0 = (int16_t *)XVTM_OFFSET_PTR_NX16(ldst, s, stride, 0);

			xb_gsr gr0;
			shs = IVP_MOVNX16T(shs, 0, vb);
			shd = IVP_MOVNX16T(shd, 0, vb);
			gr0 = XVTM_IVP_GATHERANX16T_V(src0, shs, vb, 1);
			xb_vecNx16 v0 = IVP_GATHERDNX16(gr0);
			XVTM_IVP_SCATTERNX16T(v0, dst0, shd, vb);
		}
	}
	IVP_SCATTERW();
}

