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

#ifndef TILE_MANAGER_H__
#define TILE_MANAGER_H__

#if defined (__cplusplus)
extern "C"
{
#endif
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>

//#define XVTM_MULTITHREADING_SUPPORT
#ifdef __XTENSA__
//#define XVTM_USE_XMEM
#endif
#include "tmUtils.h"

#ifdef XVTM_USE_XMEM
#include <xtensa/xmem_bank.h>
#endif
#if !defined(__XTENSA__)
#include <xtensa/config/core-isa.h>
#include "dummy.h"
#else
#ifndef XVTM_MULTITHREADING_SUPPORT
#define IDMA_APP_USE_XTOS
#else
#include <xtensa/xos.h>
#endif
#include <xtensa/idma.h>
#include <xtensa/tie/xt_misc.h>
#endif //#if !defined(__XTENSA__ )

// #define TM_LOG
#include "tileManager_api.h"
#ifdef TM_LOG
#define TILE_MANAGER_LOG_FILE_NAME  "tileManager.log"
#define TM_LOG_PRINT(fmt, ...)  do { fprintf(pxvTM->tm_log_fp, (fmt), __VA_ARGS__); } while (0)
#else
#define TM_LOG_PRINT(fmt, ...)  do {} while (0)
#endif

#ifndef IVP_SIMD_WIDTH
#define IVP_SIMD_WIDTH  XCHAL_IVPN_SIMD_WIDTH
#endif
#define IVP_ALIGNMENT   0x1F
#define XVTM_MIN(a, b)  (((a) < (b)) ? (a) : (b))

#define XVTM_DUMMY_DMA_INDEX  -2

#define XVTM_ERROR_MEMALLOC   -2
#define XVTM_ERROR            -1
#define XVTM_SUCCESS          0


/***********************************
*    Other Marcos
***********************************/

#define XV_TILE_CHECK_VIRTUAL_FRAME(pTile)    ((pTile)->pFrame->pFrameBuff == NULL)
#define XV_FRAME_CHECK_VIRTUAL_FRAME(pFrame)  ((pFrame)->pFrameBuff == NULL)

#define SETUP_TILE(pTile, pBuf, bufSize, pFrame, width, height, pitch, type, edgeWidth, edgeHeight, x, y, alignType)              \
  {                                                                                                                               \
    int32_t tileType, bytesPerPixel, channels, bytesPerPel;                                                                       \
    uint8_t *edgePtr  = (uint8_t *) (pBuf), *dataPtr;                                                                             \
    int32_t alignment = 63;                                                                                                       \
    if (((alignType) == XVTM_EDGE_ALIGNED_N) || ((alignType) == XVTM_DATA_ALIGNED_N)) { (alignment) = 31; }                               \
    tileType      = (type);                                                                                                         \
    bytesPerPixel = XV_TYPE_ELEMENT_SIZE(tileType);                                                                               \
    channels      = XV_TYPE_CHANNELS(tileType);                                                                                   \
    bytesPerPel   = bytesPerPixel / channels;                                                                                     \
    XV_TILE_SET_FRAME_PTR((xvTile *) (pTile), ((xvFrame *) (pFrame)));                                                            \
    XV_TILE_SET_BUFF_PTR((xvTile *) (pTile), (pBuf));                                                                             \
    XV_TILE_SET_BUFF_SIZE((xvTile *) (pTile), (bufSize));                                                                         \
    if (((alignType) == XVTM_EDGE_ALIGNED_N) || ((alignType) == XVTM_EDGE_ALIGNED_2N))                                                     \
    {                                                                                                                             \
      edgePtr = &((uint8_t *) (pBuf))[alignment - (((int32_t) (pBuf) + alignment) & (alignment))];                                \
    }                                                                                                                             \
    XV_TILE_SET_DATA_PTR((xvTile *) (pTile), edgePtr + (((edgeHeight) * (pitch) * bytesPerPel)) + ((edgeWidth) * bytesPerPixel)); \
    if (((alignType) == XVTM_DATA_ALIGNED_N) || ((alignType) == XVTM_DATA_ALIGNED_2N))                                                     \
    {                                                                                                                             \
      dataPtr = (uint8_t *) XV_TILE_GET_DATA_PTR((xvTile *) (pTile));                                                             \
      dataPtr = (uint8_t *) (void *)(((long) (void *)(dataPtr) + alignment) & (~alignment));                                                      \
      XV_TILE_SET_DATA_PTR((xvTile *) (pTile), dataPtr);                                                                          \
    }                                                                                                                             \
    XV_TILE_SET_WIDTH((xvTile *) (pTile), (width));                                                                               \
    XV_TILE_SET_HEIGHT((xvTile *) (pTile), (height));                                                                             \
    XV_TILE_SET_PITCH((xvTile *) (pTile), (pitch));                                                                               \
    XV_TILE_SET_TYPE((xvTile *) (pTile), (tileType | XV_TYPE_TILE_BIT));                                                          \
    XV_TILE_SET_EDGE_WIDTH((xvTile *) (pTile), (edgeWidth));                                                                      \
    XV_TILE_SET_EDGE_HEIGHT((xvTile *) (pTile), (edgeHeight));                                                                    \
    XV_TILE_SET_X_COORD((xvTile *) (pTile), (x));                                                                                 \
    XV_TILE_SET_Y_COORD((xvTile *) (pTile), (y));                                                                                 \
    XV_TILE_SET_STATUS_FLAGS((xvTile *) (pTile), 0);                                                                              \
    XV_TILE_RESET_DMA_INDEX((xvTile *) (pTile));                                                                                  \
    XV_TILE_RESET_PREVIOUS_TILE((xvTile *) (pTile));                                                                              \
    XV_TILE_RESET_REUSE_COUNT((xvTile *) (pTile));                                                                                \
  }

#define SETUP_FRAME(pFrame, pFrameBuffer, buffSize, width, height, pitch, padWidth, padHeight, pixRes, numCh, paddingType, paddingVal)           \
  {                                                                                                                                              \
    XV_FRAME_SET_BUFF_PTR((xvFrame *) (pFrame), (pFrameBuffer));                                                                                 \
    XV_FRAME_SET_BUFF_SIZE((xvFrame *) (pFrame), (buffSize));                                                                                    \
    XV_FRAME_SET_WIDTH((xvFrame *) (pFrame), (width));                                                                                           \
    XV_FRAME_SET_HEIGHT((xvFrame *) (pFrame), (height));                                                                                         \
    XV_FRAME_SET_PITCH((xvFrame *) (pFrame), (pitch));                                                                                           \
    XV_FRAME_SET_PIXEL_RES((xvFrame *) (pFrame), (pixRes));                                                                                      \
    XV_FRAME_SET_DATA_PTR((xvFrame *) (pFrame), ((uint8_t *) (pFrameBuffer)) + ((((pitch) * (padHeight)) + ((padWidth) * (numCh))) * (pixRes))); \
    XV_FRAME_SET_EDGE_WIDTH((xvFrame *) (pFrame), (padWidth));                                                                                   \
    XV_FRAME_SET_EDGE_HEIGHT((xvFrame *) (pFrame), (padHeight));                                                                                 \
    XV_FRAME_SET_NUM_CHANNELS((xvFrame *) (pFrame), (numCh));                                                                                    \
    XV_FRAME_SET_PADDING_TYPE((xvFrame *) (pFrame), (paddingType));                                                                              \
    XV_FRAME_SET_PADDING_VALUE((xvFrame *) (pFrame), (paddingVal));                                                                              \
  }


// Assumes both top and bottom edges are equal
#define XV_TILE_UPDATE_EDGE_HEIGHT(pTile, newEdgeHeight)                              \
  {                                                                                   \
    uint32_t tileType       = XV_TILE_GET_TYPE(pTile);                                \
    uint32_t bytesPerPixel  = XV_TYPE_ELEMENT_SIZE(tileType);                         \
    uint32_t channels       = XV_TYPE_CHANNELS(tileType);                             \
    uint32_t bytesPerPel    = bytesPerPixel / channels;                               \
    uint16_t currEdgeHeight = (uint16_t) XV_TILE_GET_EDGE_HEIGHT(pTile);              \
    uint32_t tilePitch      = (uint32_t) XV_TILE_GET_PITCH(pTile);                    \
    uint32_t tileHeight     = (uint32_t) XV_TILE_GET_HEIGHT(pTile);                   \
    uint32_t dataU32        = (uint32_t) XV_TILE_GET_DATA_PTR(pTile);                 \
    dataU32 = dataU32 + tilePitch * bytesPerPel * ((newEdgeHeight) - currEdgeHeight); \
    XV_TILE_SET_DATA_PTR((pTile), (void *) dataU32);                                  \
    XV_TILE_SET_EDGE_HEIGHT((pTile), (newEdgeHeight));                                \
    XV_TILE_SET_HEIGHT((pTile), tileHeight + 2 * (currEdgeHeight - (newEdgeHeight))); \
  }

// Assumes both left and right edges are equal
#define XV_TILE_UPDATE_EDGE_WIDTH(pTile, newEdgeWidth)                            \
  {                                                                               \
    uint32_t tileType      = (pTile)->type;                                       \
    uint32_t bytesPerPixel = XV_TYPE_ELEMENT_SIZE(tileType);                      \
    uint16_t currEdgeWidth = (uint16_t) XV_TILE_GET_EDGE_WIDTH(pTile);            \
    uint32_t tileWidth     = (uint32_t) XV_TILE_GET_WIDTH(pTile);                 \
    uint32_t dataU32       = (uint32_t) XV_TILE_GET_DATA_PTR(pTile);              \
    dataU32 = dataU32 + ((newEdgeWidth) - currEdgeWidth) * bytesPerPixel;         \
    XV_TILE_SET_DATA_PTR((pTile), (void *) dataU32);                              \
    XV_TILE_SET_EDGE_WIDTH((pTile), (newEdgeWidth));                              \
    XV_TILE_SET_WIDTH((pTile), tileWidth + 2 * (currEdgeWidth - (newEdgeWidth))); \
  }

#define XV_TILE_UPDATE_DIMENSIONS(pTile, x, y, w, h, p) \
  {                                                     \
    XV_TILE_SET_X_COORD((pTile), (x));                  \
    XV_TILE_SET_Y_COORD((pTile), (y));                  \
    XV_TILE_SET_WIDTH((pTile), (w));                    \
    XV_TILE_SET_HEIGHT((pTile), (h));                   \
    XV_TILE_SET_PITCH((pTile), (p));                    \
  }

#define XV_TILE_GET_CHANNEL(t)  XV_TYPE_CHANNELS(XV_TILE_GET_TYPE(t))
#define XV_TILE_PEL_SIZE(t)     (XV_TILE_GET_ELEMENT_SIZE(t) / XV_TILE_GET_CHANNEL(t))


#define XV_ARRAY_STARTS_IN_DRAM(t) \
  (((uint32_t) XV_ARRAY_GET_BUFF_PTR(t)) >= ((uint32_t) XVTM_DRAM_LOWER_ADDR))
#define XV_ARRAY_ENDS_IN_DRAM(t) \
  ((((uint64_t) (uint32_t) XV_ARRAY_GET_BUFF_PTR(t)) + ((uint64_t) XV_ARRAY_GET_BUFF_SIZE(t))) <= ((uint64_t) XVTM_DRAM_HIGHER_ADDR))
#define XV_TILE_STARTS_IN_DRAM(t) \
  (((uint32_t) XV_TILE_GET_BUFF_PTR(t)) >= ((uint32_t) XVTM_DRAM_LOWER_ADDR))
#define XV_TILE_ENDS_IN_DRAM(t) \
  ((((uint64_t) (uint32_t) XV_TILE_GET_BUFF_PTR(t)) + ((uint64_t) XV_TILE_GET_BUFF_SIZE(t))) <= ((uint64_t) XVTM_DRAM_HIGHER_ADDR))
#define XV_PTR_START_IN_DRAM(t) \
  (((uint32_t) (t)) >= (((uint32_t) XVTM_DRAM_LOWER_ADDR)))
#define XV_PTR_END_IN_DRAM(t) \
  (((uint64_t) (t)) <= (((uint32_t) XVTM_DRAM_HIGHER_ADDR)))


#define XV_TILE_IS_CONSISTENT(t)                                                                                                                                                                             \
  (((XV_TILE_GET_PITCH(t)) >= ((XV_TILE_GET_WIDTH(t) + XV_TILE_GET_EDGE_LEFT(t) + XV_TILE_GET_EDGE_RIGHT(t)) * XV_TILE_GET_CHANNEL(t))) &&                                                                   \
   (((uint8_t *) XV_TILE_GET_DATA_PTR(t) - ((XV_TILE_GET_EDGE_LEFT(t) * XV_TILE_GET_ELEMENT_SIZE(t)) + (XV_TILE_GET_PITCH(t) * XV_TILE_GET_EDGE_TOP(t) * XV_TILE_PEL_SIZE(t))))                              \
    >= ((uint8_t *) XV_TILE_GET_BUFF_PTR(t))) &&                                                                                                                                                             \
   ((((uint8_t *) XV_TILE_GET_DATA_PTR(t) - (XV_TILE_GET_EDGE_LEFT(t) * XV_TILE_GET_ELEMENT_SIZE(t))) + (XV_TILE_GET_PITCH(t) * (XV_TILE_GET_HEIGHT(t) + XV_TILE_GET_EDGE_BOTTOM(t)) * XV_TILE_PEL_SIZE(t))) \
    <= ((uint8_t *) XV_TILE_GET_BUFF_PTR(t) + XV_TILE_GET_BUFF_SIZE(t))))

#define XV_CHECK_POINTER(pointer, statement) \
  XV_CHECK_ERROR((pointer) == (void *) NULL, (statement), XVTM_ERROR, " NULL pointer error")

#define XV_CHECK_TILE(tile, TileMgr)                                                                                                          \
  XV_CHECK_POINTER((tile), (TileMgr)->errFlag = XV_ERROR_BAD_ARG );                                                                          \
  XV_CHECK_ERROR(!(XV_TILE_IS_TILE(tile) > 0), (TileMgr)->errFlag = XV_ERROR_BAD_ARG, XVTM_ERROR, "The argument is not a tile");            \
  XV_CHECK_ERROR(!(XV_TILE_STARTS_IN_DRAM(tile)), (TileMgr)->errFlag = XV_ERROR_BAD_ARG, XVTM_ERROR, "Data buffer does not start in DRAM"); \
  XV_CHECK_ERROR(!(XV_TILE_ENDS_IN_DRAM(tile)), (TileMgr)->errFlag = XV_ERROR_BAD_ARG, XVTM_ERROR, "Data buffer does not fit in DRAM");     \
  XV_CHECK_ERROR(!(XV_TILE_IS_CONSISTENT(tile)), (TileMgr)->errFlag = XV_ERROR_BAD_ARG, XVTM_ERROR, "Invalid buffer")


/* do not check arguments for errors */
#define XV_ERROR_LEVEL_NO_ERROR                     0
/* call exit(-1) in case of error */
#define XV_ERROR_LEVEL_TERMINATE_ON_ERROR           1
/* return corresponding error code on error without any processing (recommended)*/
#define XV_ERROR_LEVEL_RETURN_ON_ERROR              2
/* capture error but attempt continue processing (dangerous!) */
#define XV_ERROR_LEVEL_CONTINUE_ON_ERROR            3
/* print error message to stdout and return without any processing */
#define XV_ERROR_LEVEL_PRINT_ON_ERROR               4
/* print error message but attempt continue processing (dangerous!) */
#define XV_ERROR_LEVEL_PRINT_AND_CONTINUE_ON_ERROR  5

#ifndef XV_ERROR_LEVEL
#define XV_ERROR_LEVEL  XV_ERROR_LEVEL_RETURN_ON_ERROR
#endif

#if XV_ERROR_LEVEL == XV_ERROR_LEVEL_TERMINATE_ON_ERROR
#  define XV_CHECK_ERROR(condition, statement, code, wide_description) \
  if (!(condition)) {} else { exit(-1); }
#  define XV_CHECK_ERROR_NULL(condition, code, wide_description) \
  if (!(condition)) {} else { exit(-1); }

#elif XV_ERROR_LEVEL == XV_ERROR_LEVEL_RETURN_ON_ERROR
#  define XV_CHECK_ERROR(condition, statement, code, wide_description) \
  if (!(condition)) {} else { (statement); return (code); }
#  define XV_CHECK_ERROR_NULL(condition, code, wide_description) \
  if (!(condition)) {} else { return (code); }

#elif XV_ERROR_LEVEL == XV_ERROR_LEVEL_CONTINUE_ON_ERROR
#  define XV_CHECK_ERROR(condition, statement, code, wide_description) \
  if (!(condition)) {} else { (statement); }
#  define XV_CHECK_ERROR_NULL(condition, code, wide_description)

#elif XV_ERROR_LEVEL == XV_ERROR_LEVEL_PRINT_ON_ERROR
#  define XV_CHECK_ERROR(condition, statement, code, wide_description)          \
  do { if (condition) { (statement); printf("%s:%d: Error # in function %s: %s\n", \
                                        __FILE__, __LINE__, __func__, (wide_description)); fflush(stdout); return (code); } } while (0)
#  define XV_CHECK_ERROR_NULL(condition, code, wide_description)          \
  do { if (condition) { printf("%s:%d: Error # in function %s: %s\n", \
                                        __FILE__, __LINE__, __func__, (wide_description)); fflush(stdout); return (code); } } while (0)

#elif XV_ERROR_LEVEL == XV_ERROR_LEVEL_PRINT_AND_CONTINUE_ON_ERROR
#  define XV_CHECK_ERROR(condition, statement, code, wide_description)  \
  do { if (condition) { printf("%s:%d: Error #  in function %s: %s\n", \
                               __FILE__, __LINE__, __func__, (wide_description)); fflush(stdout); } } while (0)
#  define XV_CHECK_ERROR_NULL(condition, code, wide_description)  \
  do { if (condition) { printf("%s:%d: Error #  in function %s: %s\n", \
                               __FILE__, __LINE__, __func__, (wide_description)); fflush(stdout); } } while (0)

#else
#  define XV_CHECK_ERROR(condition, statment, code, wide_description)
#  define XV_CHECK_ERROR_NULL(condition, code, wide_description)
#endif


#if defined (__cplusplus)
}
#endif
#endif

