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

#ifndef TILE_MANAGER_API_H__
#define TILE_MANAGER_API_H__

#if defined (__cplusplus)
extern "C"
{
#endif

/*****************************************
*   Tile Manager Version
*****************************************/
#define XVTM_VERSION_MAJOR    6
#define XVTM_VERSION_MINOR    1
#define XVTM_VERSION_PATCH    0

#define XVTM_MAKE_LIBRARY_VERSION(major, minor, patch) ((major)*100000 + (minor)*1000 + (patch)*10)
#define XVTM_AUX_STR_EXP(__A) #__A
#define XVTM_AUX_STR(__A) XVTM_AUX_STR_EXP(__A)

#define XVTM_VERSION     (XVTM_MAKE_LIBRARY_VERSION(XVTM_VERSION_MAJOR, XVTM_VERSION_MINOR, XVTM_VERSION_PATCH))
#define XVTM_VERSION_STR XVTM_AUX_STR(XVTM_VERSION_MAJOR) "." XVTM_AUX_STR(XVTM_VERSION_MINOR) "." XVTM_AUX_STR(XVTM_VERSION_PATCH) 


#if defined(__XTENSA__)
#if defined(XCHAL_DATARAM1_VADDR)

#if (XCHAL_DATARAM1_VADDR < XCHAL_DATARAM0_VADDR)
#define XVTM_DRAM_LOWER_ADDR   XCHAL_DATARAM1_VADDR
#define XVTM_DRAM_HIGHER_ADDR  (((uint64_t) (XCHAL_DATARAM0_VADDR)) + ((uint64_t) (XCHAL_DATARAM0_SIZE)))
#else // (XCHAL_DATARAM1_VADDR < XCHAL_DATARAM0_VADDR)
#define XVTM_DRAM_LOWER_ADDR   XCHAL_DATARAM0_VADDR
#define XVTM_DRAM_HIGHER_ADDR  (((uint64_t) (XCHAL_DATARAM1_VADDR)) + ((uint64_t) (XCHAL_DATARAM1_SIZE)))
#endif // (XCHAL_DATARAM1_VADDR < XCHAL_DATARAM0_VADDR)

#else // defined(XCHAL_DATARAM1_VADDR)

#define XVTM_DRAM_LOWER_ADDR   XCHAL_DATARAM0_VADDR
#define XVTM_DRAM_HIGHER_ADDR  (((uint64_t) (XCHAL_DATARAM0_VADDR)) + ((uint64_t) (XCHAL_DATARAM0_SIZE)))
#endif // defined(XCHAL_DATARAM1_VADDR)

#else // defined(__XTENSA__)
#define XVTM_DRAM_LOWER_ADDR   ((uint64_t) 0x00001000)
#define XVTM_DRAM_HIGHER_ADDR  ((uint64_t) 0x00001000 + ((uint64_t) (0xFFFFFFFF - 0x2000)))
#endif // defined(__XTENSA__)


#ifndef FIK_FRAMEWORK
#define FIK_FRAMEWORK                    /*FIK framework */
#endif
// MAX limits for number of tiles, frames memory banks and dma queue length
// Allow user to change the values build time.

#if !defined(MAX_NUM_MEM_BANKS)
#define MAX_NUM_MEM_BANKS         8u
#endif
#if !defined(MAX_NUM_TILES)
#define MAX_NUM_TILES             52u
#endif
#if !defined(MAX_NUM_FRAMES)
#define MAX_NUM_FRAMES            16u
#endif
#if !defined(MAX_NUM_DMA_QUEUE_LENGTH)
#define MAX_NUM_DMA_QUEUE_LENGTH  32u // Optimization, multiple of 2
#endif
#if !defined(MAX_NUM_CHANNEL)
#define MAX_NUM_CHANNEL           4u
#endif

#if !defined(MAX_TM_CONTEXT_SIZE)
#define MAX_TM_CONTEXT_SIZE           4u
#endif

// Bank colors. XV_MEM_BANK_COLOR_ANY is an unlikely enum value
#define XV_MEM_BANK_COLOR_0    0x0u
#define XV_MEM_BANK_COLOR_1    0x1u
#define XV_MEM_BANK_COLOR_2    0x2u
#define XV_MEM_BANK_COLOR_3    0x3u
#define XV_MEM_BANK_COLOR_4    0x4u
#define XV_MEM_BANK_COLOR_5    0x5u
#define XV_MEM_BANK_COLOR_6    0x6u
#define XV_MEM_BANK_COLOR_7    0x7u
#define XV_MEM_BANK_COLOR_ANY  0xBEEDDEAFU

// Edge padding format
#define FRAME_ZERO_PADDING          0u
#define FRAME_CONSTANT_PADDING      1u
#define FRAME_EDGE_PADDING          2u
#define FRAME_PADDING_REFLECT_101   3u
#define FRAME_PADDING_MAX           4u

#define XVTM_DUMMY_DMA_INDEX    -2
#define XVTM_ERROR              -1
#define XVTM_SUCCESS            0


/*****************************************
*   Individual status flags definitions
*****************************************/

#define XV_TILE_STATUS_DMA_ONGOING                 (0x01u)
#define XV_TILE_STATUS_LEFT_EDGE_PADDING_NEEDED    (0x02u)
#define XV_TILE_STATUS_RIGHT_EDGE_PADDING_NEEDED   (0x04u)
#define XV_TILE_STATUS_TOP_EDGE_PADDING_NEEDED     (0x08u)
#define XV_TILE_STATUS_BOTTOM_EDGE_PADDING_NEEDED  (0x10u)
#define XV_TILE_STATUS_DUMMY_IDMA_INDEX_NEEDED     (0x8000u)

#define XV_TILE_STATUS_EDGE_PADDING_NEEDED    \
  (XV_TILE_STATUS_LEFT_EDGE_PADDING_NEEDED |  \
   XV_TILE_STATUS_RIGHT_EDGE_PADDING_NEEDED | \
   XV_TILE_STATUS_TOP_EDGE_PADDING_NEEDED |   \
   XV_TILE_STATUS_BOTTOM_EDGE_PADDING_NEEDED)


/*****************************************
*   Data type definitions
*****************************************/

#define XV_TYPE_SIGNED_BIT         (0x8000u)
#define XV_TYPE_TILE_BIT           (0x4000u)

#define XV_TYPE_ELEMENT_SIZE_BITS  10
#define XV_TYPE_ELEMENT_SIZE_MASK  ((1 << XV_TYPE_ELEMENT_SIZE_BITS) - 1)
#define XV_TYPE_CHANNELS_BITS      2
#define XV_TYPE_CHANNELS_MASK      (((1 << XV_TYPE_CHANNELS_BITS) - 1) << XV_TYPE_ELEMENT_SIZE_BITS)

// XV_MAKETYPE accepts 3 parameters
// 1: flag: Denotes whether the entity is a tile (XV_TYPE_TILE_BIT is set) or an array (XV_TYPE_TILE_BIT is not set),
//    and also if the data is a signed(XV_TYPE_SIGNED_BIT is set) or unsigned(XV_TYPE_SIGNED_BIT is not set).
// 2: depth: Denotes number of bytes per pel.
//    1 implies the data is 8bit, 2 implies the data is 16bit and 4 implies the data is 32bit.
// 3: Denotes number of channels.
//    1 implies gray scale, 3 implies RGB

#define XV_MAKETYPE(flags, depth, channels)  (((depth) * (channels)) | (((channels) - 1) << XV_TYPE_ELEMENT_SIZE_BITS) | (flags))
#define XV_CUSTOMTYPE(type)                  XV_MAKETYPE(0, sizeof(type), 1)

#define XV_TYPE_ELEMENT_SIZE(type)           ((type) & (XV_TYPE_ELEMENT_SIZE_MASK))
#define XV_TYPE_ELEMENT_TYPE(type)           ((type) & (XV_TYPE_SIGNED_BIT | XV_TYPE_CHANNELS_MASK | XV_TYPE_ELEMENT_SIZE_MASK))
#define XV_TYPE_IS_TILE(type)                ((type) & (XV_TYPE_TILE_BIT))
#define XV_TYPE_IS_SIGNED(type)              ((type) & (XV_TYPE_SIGNED_BIT))
#define XV_TYPE_CHANNELS(type)               ((((type) & (XV_TYPE_CHANNELS_MASK)) >> (XV_TYPE_ELEMENT_SIZE_BITS)) + 1)

// Common XV_MAKETYPEs
#define XV_U8            XV_MAKETYPE(0, 1, 1)
#define XV_U16           XV_MAKETYPE(0, 2, 1)
#define XV_U32           XV_MAKETYPE(0, 4, 1)

#define XV_S8            XV_MAKETYPE(XV_TYPE_SIGNED_BIT, 1, 1)
#define XV_S16           XV_MAKETYPE(XV_TYPE_SIGNED_BIT, 2, 1)
#define XV_S32           XV_MAKETYPE(XV_TYPE_SIGNED_BIT, 4, 1)
#define XV_F32           XV_MAKETYPE(XV_TYPE_SIGNED_BIT, 4, 1)

#define XV_ARRAY_U8      XV_U8
#define XV_ARRAY_S8      XV_S8
#define XV_ARRAY_U16     XV_U16
#define XV_ARRAY_S16     XV_S16
#define XV_ARRAY_U32     XV_U32
#define XV_ARRAY_S32     XV_S32

#define XV_TILE_U8       (XV_U8 | XV_TYPE_TILE_BIT)
#define XV_TILE_S8       (XV_S8 | XV_TYPE_TILE_BIT)
#define XV_TILE_U16      (XV_U16 | XV_TYPE_TILE_BIT)
#define XV_TILE_S16      (XV_S16 | XV_TYPE_TILE_BIT)
#define XV_TILE_U32      (XV_U32 | XV_TYPE_TILE_BIT)
#define XV_TILE_S32      (XV_S32 | XV_TYPE_TILE_BIT)
#define XV_TILE_F32      (XV_F32 | XV_TYPE_TILE_BIT)

#define XV_TILE_RGB_U8   XV_MAKETYPE(XV_TYPE_TILE_BIT, 1, 3)
#define XV_TILE_RGB_U16  XV_MAKETYPE(XV_TYPE_TILE_BIT, 2, 3)
#define XV_TILE_RGB_U32  XV_MAKETYPE(XV_TYPE_TILE_BIT, 4, 3)
  
#define XV_FLOAT         XV_MAKETYPE(0, 4, 1)
#define XV_TILE_FLOAT 	 (XV_FLOAT | XV_TYPE_TILE_BIT)

/*****************************************
*    Frame Access Macros
*****************************************/

#define XV_FRAME_GET_BUFF_PTR(pFrame)                   ((pFrame)->pFrameBuff)
#define XV_FRAME_SET_BUFF_PTR(pFrame, pBuff)            (pFrame)->pFrameBuff = ((void *) (pBuff))

#define XV_FRAME_GET_BUFF_SIZE(pFrame)                  ((pFrame)->frameBuffSize)
#define XV_FRAME_SET_BUFF_SIZE(pFrame, buffSize)        (pFrame)->frameBuffSize = ((uint32_t) (buffSize))

#define XV_FRAME_GET_DATA_PTR(pFrame)                   ((pFrame)->pFrameData)
#define XV_FRAME_SET_DATA_PTR(pFrame, pData)            (pFrame)->pFrameData = ((void *) (pData))

#define XV_FRAME_GET_WIDTH(pFrame)                      ((pFrame)->frameWidth)
#define XV_FRAME_SET_WIDTH(pFrame, width)               (pFrame)->frameWidth = ((int32_t) (width))

#define XV_FRAME_GET_HEIGHT(pFrame)                     ((pFrame)->frameHeight)
#define XV_FRAME_SET_HEIGHT(pFrame, height)             (pFrame)->frameHeight = ((int32_t) (height))

#define XV_FRAME_GET_PITCH(pFrame)                      ((pFrame)->framePitch)
#define XV_FRAME_SET_PITCH(pFrame, pitch)               (pFrame)->framePitch = ((int32_t) (pitch))
#define XV_FRAME_GET_PITCH_IN_BYTES(pFrame)             ((pFrame)->framePitch * (pFrame)->pixelRes)

#define XV_FRAME_GET_PIXEL_RES(pFrame)                  ((pFrame)->pixelRes)
#define XV_FRAME_SET_PIXEL_RES(pFrame, pixRes)          (pFrame)->pixelRes = ((uint8_t) (pixRes))

#define XV_FRAME_GET_NUM_CHANNELS(pFrame)               ((pFrame)->numChannels)
#define XV_FRAME_SET_NUM_CHANNELS(pFrame, pixelFormat)  (pFrame)->numChannels = ((uint8_t) (pixelFormat))

#define XV_FRAME_GET_EDGE_WIDTH(pFrame)                 ((pFrame)->leftEdgePadWidth < (pFrame)->rightEdgePadWidth ? (pFrame)->leftEdgePadWidth : (pFrame)->rightEdgePadWidth)
#define XV_FRAME_SET_EDGE_WIDTH(pFrame, padWidth)         \
  {                                                       \
    (pFrame)->leftEdgePadWidth  = ((uint8_t) (padWidth)); \
    (pFrame)->rightEdgePadWidth = ((uint8_t) (padWidth)); \
  }

#define XV_FRAME_GET_EDGE_HEIGHT(pFrame)  ((pFrame)->topEdgePadHeight < (pFrame)->bottomEdgePadHeight ? (pFrame)->topEdgePadHeight : (pFrame)->bottomEdgePadHeight)
#define XV_FRAME_SET_EDGE_HEIGHT(pFrame, padHeight)          \
  {                                                          \
    (pFrame)->topEdgePadHeight    = ((uint8_t) (padHeight)); \
    (pFrame)->bottomEdgePadHeight = ((uint8_t) (padHeight)); \
  }

#define XV_FRAME_GET_EDGE_LEFT(pFrame)               ((pFrame)->leftEdgePadWidth)
#define XV_FRAME_SET_EDGE_LEFT(pFrame, padWidth)     (pFrame)->leftEdgePadWidth = ((uint8_t) (padWidth))

#define XV_FRAME_GET_EDGE_RIGHT(pFrame)              ((pFrame)->rightEdgePadWidth)
#define XV_FRAME_SET_EDGE_RIGHT(pFrame, padWidth)    (pFrame)->rightEdgePadWidth = ((uint8_t) (padWidth))

#define XV_FRAME_GET_EDGE_TOP(pFrame)                ((pFrame)->topEdgePadHeight)
#define XV_FRAME_SET_EDGE_TOP(pFrame, padHeight)     (pFrame)->topEdgePadHeight = ((uint8_t) (padHeight))

#define XV_FRAME_GET_EDGE_BOTTOM(pFrame)             ((pFrame)->bottomEdgePadHeight)
#define XV_FRAME_SET_EDGE_BOTTOM(pFrame, padHeight)  (pFrame)->bottomEdgePadHeight = ((uint8_t) (padHeight))

#define XV_FRAME_GET_PADDING_TYPE(pFrame)            ((pFrame)->paddingType)
#define XV_FRAME_SET_PADDING_TYPE(pFrame, padType)   (pFrame)->paddingType = (padType)

#define XV_FRAME_GET_PADDING_VALUE(pFrame)           ((pFrame)->paddingVal)
#define XV_FRAME_SET_PADDING_VALUE(pFrame, padVal)   (pFrame)->paddingVal = (padVal)

/*****************************************
*    Array Access Macros
*****************************************/

#define XV_ARRAY_GET_BUFF_PTR(pArray)              ((pArray)->pBuffer)
#define XV_ARRAY_SET_BUFF_PTR(pArray, pBuff)       (pArray)->pBuffer = ((void *) (pBuff))

#define XV_ARRAY_GET_BUFF_SIZE(pArray)             ((pArray)->bufferSize)
#define XV_ARRAY_SET_BUFF_SIZE(pArray, buffSize)   (pArray)->bufferSize = ((uint32_t) (buffSize))

#define XV_ARRAY_GET_DATA_PTR(pArray)              ((pArray)->pData)
#define XV_ARRAY_SET_DATA_PTR(pArray, pArrayData)  (pArray)->pData = ((void *) (pArrayData))

#define XV_ARRAY_GET_WIDTH(pArray)                 ((pArray)->width)
#define XV_ARRAY_SET_WIDTH(pArray, value)          (pArray)->width = ((int32_t) (value))

#define XV_ARRAY_GET_PITCH(pArray)                 ((pArray)->pitch)
#define XV_ARRAY_SET_PITCH(pArray, value)          (pArray)->pitch = ((int32_t) (value))

#define XV_ARRAY_GET_HEIGHT(pArray)                ((pArray)->height)
#define XV_ARRAY_SET_HEIGHT(pArray, value)         (pArray)->height = ((uint16_t) (value))

#define XV_ARRAY_GET_STATUS_FLAGS(pArray)          ((pArray)->status)
#define XV_ARRAY_SET_STATUS_FLAGS(pArray, value)   (pArray)->status = ((uint8_t) (value))

#define XV_ARRAY_GET_TYPE(pArray)                  ((pArray)->type)
#define XV_ARRAY_SET_TYPE(pArray, value)           (pArray)->type = ((uint16_t) (value))

#define XV_ARRAY_GET_CAPACITY(pArray)              XV_ARRAY_GET_PITCH(pArray)
#define XV_ARRAY_SET_CAPACITY(pArray, value)       XV_ARRAY_SET_PITCH((pArray), (value))

#define XV_ARRAY_GET_ELEMENT_TYPE(pArray)          XV_TYPE_ELEMENT_TYPE(XV_ARRAY_GET_TYPE(pArray))
#define XV_ARRAY_GET_ELEMENT_SIZE(pArray)          XV_TYPE_ELEMENT_SIZE(XV_ARRAY_GET_TYPE(pArray))
#define XV_ARRAY_IS_TILE(pArray)                   XV_TYPE_IS_TILE(XV_ARRAY_GET_TYPE(pArray) & (XV_TYPE_TILE_BIT))

#define XV_ARRAY_GET_AREA(pArray)                  (((pArray)->width) * ((int32_t) (pArray)->height))

/*****************************************
*    Tile Access Macros
*****************************************/

#define XV_TILE_GET_BUFF_PTR   XV_ARRAY_GET_BUFF_PTR
#define XV_TILE_SET_BUFF_PTR   XV_ARRAY_SET_BUFF_PTR

#define XV_TILE_GET_BUFF_SIZE  XV_ARRAY_GET_BUFF_SIZE
#define XV_TILE_SET_BUFF_SIZE  XV_ARRAY_SET_BUFF_SIZE

#define XV_TILE_GET_DATA_PTR   XV_ARRAY_GET_DATA_PTR
#define XV_TILE_SET_DATA_PTR   XV_ARRAY_SET_DATA_PTR

#define XV_TILE_GET_WIDTH      XV_ARRAY_GET_WIDTH
#define XV_TILE_SET_WIDTH      XV_ARRAY_SET_WIDTH

#define XV_TILE_GET_PITCH      XV_ARRAY_GET_PITCH
#define XV_TILE_SET_PITCH      XV_ARRAY_SET_PITCH
#define XV_TILE_GET_PITCH_IN_BYTES(pTile)  ((pTile)->pitch * (int32_t) ((pTile)->pFrame->pixelRes))

#define XV_TILE_GET_HEIGHT        XV_ARRAY_GET_HEIGHT
#define XV_TILE_SET_HEIGHT        XV_ARRAY_SET_HEIGHT

#define XV_TILE_GET_STATUS_FLAGS  XV_ARRAY_GET_STATUS_FLAGS
#define XV_TILE_SET_STATUS_FLAGS  XV_ARRAY_SET_STATUS_FLAGS

#define XV_TILE_GET_TYPE          XV_ARRAY_GET_TYPE
#define XV_TILE_SET_TYPE          XV_ARRAY_SET_TYPE

#define XV_TILE_GET_ELEMENT_TYPE  XV_ARRAY_GET_ELEMENT_TYPE
#define XV_TILE_GET_ELEMENT_SIZE  XV_ARRAY_GET_ELEMENT_SIZE
#define XV_TILE_IS_TILE           XV_ARRAY_IS_TILE

#define XV_TILE_RESET_DMA_INDEX(pTile)              ((pTile)->dmaIndex = 0)
#define XV_TILE_RESET_PREVIOUS_TILE(pTile)          (pTile)->pPrevTile = ((xvTile *) (NULL))
#define XV_TILE_RESET_REUSE_COUNT(pTile)            ((pTile)->reuseCount = 0)

#define XV_TILE_GET_FRAME_PTR(pTile)                ((pTile)->pFrame)
#define XV_TILE_SET_FRAME_PTR(pTile, ptrFrame)      (pTile)->pFrame = ((xvFrame *) (ptrFrame))

#define XV_TILE_GET_X_COORD(pTile)                  ((pTile)->x)
#define XV_TILE_SET_X_COORD(pTile, xcoord)          (pTile)->x = ((int32_t) (xcoord))

#define XV_TILE_GET_Y_COORD(pTile)                  ((pTile)->y)
#define XV_TILE_SET_Y_COORD(pTile, ycoord)          (pTile)->y = ((int32_t) (ycoord))

#define XV_TILE_GET_EDGE_LEFT(pTile)                ((pTile)->tileEdgeLeft)
#define XV_TILE_SET_EDGE_LEFT(pTile, edgeWidth)     (pTile)->tileEdgeLeft = ((uint16_t) (edgeWidth))

#define XV_TILE_GET_EDGE_RIGHT(pTile)               ((pTile)->tileEdgeRight)
#define XV_TILE_SET_EDGE_RIGHT(pTile, edgeWidth)    (pTile)->tileEdgeRight = ((uint16_t) (edgeWidth))

#define XV_TILE_GET_EDGE_TOP(pTile)                 ((pTile)->tileEdgeTop)
#define XV_TILE_SET_EDGE_TOP(pTile, edgeHeight)     (pTile)->tileEdgeTop = ((uint16_t) (edgeHeight))

#define XV_TILE_GET_EDGE_BOTTOM(pTile)              ((pTile)->tileEdgeBottom)
#define XV_TILE_SET_EDGE_BOTTOM(pTile, edgeHeight)  (pTile)->tileEdgeBottom = ((uint16_t) (edgeHeight))

#define XV_TILE_GET_EDGE_WIDTH(pTile)               (((pTile)->tileEdgeLeft < (pTile)->tileEdgeRight) ? (pTile)->tileEdgeLeft : (pTile)->tileEdgeRight)
#define XV_TILE_SET_EDGE_WIDTH(pTile, edgeWidth)       \
  {                                                    \
    (pTile)->tileEdgeLeft  = ((uint16_t) (edgeWidth)); \
    (pTile)->tileEdgeRight = ((uint16_t) (edgeWidth)); \
  }

#define XV_TILE_GET_EDGE_HEIGHT(pTile)  (((pTile)->tileEdgeTop < (pTile)->tileEdgeBottom) ? (pTile)->tileEdgeTop : (pTile)->tileEdgeBottom)
#define XV_TILE_SET_EDGE_HEIGHT(pTile, edgeHeight)       \
  {                                                      \
    (pTile)->tileEdgeTop    = ((uint16_t) (edgeHeight)); \
    (pTile)->tileEdgeBottom = ((uint16_t) (edgeHeight)); \
  }

#define XV_TILE_CHECK_STATUS_FLAGS_DMA_ONGOING(pTile)          (((pTile)->status & XV_TILE_STATUS_DMA_ONGOING) > 0)
#define XV_TILE_CHECK_STATUS_FLAGS_EDGE_PADDING_NEEDED(pTile)  (((pTile)->status & XV_TILE_STATUS_EDGE_PADDING_NEEDED) > 0)

#ifdef FIK_FRAMEWORK
#define XV_INTERIM_TILE 0
#define XV_INPUT_TILE 1
#define XV_OUTPUT_TILE 2

#define INTERRUPT_ON_COMPLETION  0

#define XV_AUTOSIZE_FLAG (1)
#define XV_SEQ_FLAG (1<<10)
#define XV_ALLOCATE_SINGLE_BANK (1<<11)
#define XV_DMA_OVERLAP (1<<12)

#define XV_MAX_ALLOCATIONS 64

#define MAX_TILES 20
#define INTERIM_MAX_TILES 16

#define PING 0
#define PONG 1
#define DBG_PROFILE 0
#endif


/*Structures and enums*/
#if 1
typedef enum
{
  /* CRITICAL ORDER, DO NOT CHANGE. */
  XVTM_TILE_UNALIGNED = 0,
  XVTM_EDGE_ALIGNED_N,
  XVTM_DATA_ALIGNED_N,
  XVTM_EDGE_ALIGNED_2N,
  XVTM_DATA_ALIGNED_2N,
} buffer_align_type_t;
#endif

#define TILE_UNALIGNED  XVTM_TILE_UNALIGNED  
#define EDGE_ALIGNED_32 XVTM_EDGE_ALIGNED_N  
#define DATA_ALIGNED_32 XVTM_DATA_ALIGNED_N  
#define EDGE_ALIGNED_64 XVTM_EDGE_ALIGNED_2N 
#define DATA_ALIGNED_64 XVTM_DATA_ALIGNED_2N 

typedef enum
{
  XV_ERROR_SUCCESS            = 0,
  XV_ERROR_TILE_MANAGER_NULL  = 1,
  XV_ERROR_POINTER_NULL       = 2,
  XV_ERROR_FRAME_NULL         = 3,
  XV_ERROR_TILE_NULL          = 4,
  XV_ERROR_BUFFER_NULL        = 5,
  XV_ERROR_ALLOC_FAILED       = 6,
  XV_ERROR_FRAME_BUFFER_FULL  = 7,
  XV_ERROR_TILE_BUFFER_FULL   = 8,
  XV_ERROR_DIMENSION_MISMATCH = 9,
  XV_ERROR_BUFFER_OVERFLOW    = 10,
  XV_ERROR_BAD_ARG            = 11,
  XV_ERROR_FILE_OPEN          = 12,
  XV_ERROR_DMA_INIT           = 13,
  XV_ERROR_XVMEM_INIT         = 14,
  XV_ERROR_IDMA               = 15
}xvError_t;


typedef struct  xvFrameStruct
{
  void     *pFrameBuff;
  uint32_t frameBuffSize;
  void     *pFrameData;
  int32_t  frameWidth;
  int32_t  frameHeight;
  int32_t  framePitch;
  uint8_t  pixelRes;
  uint8_t  numChannels;
  uint8_t  leftEdgePadWidth;
  uint8_t  topEdgePadHeight;
  uint8_t  rightEdgePadWidth;
  uint8_t  bottomEdgePadHeight;
  uint8_t  paddingType;
  uint32_t paddingVal;
} xvFrame, *xvpFrame;


#define XV_ARRAY_FIELDS \
  void     *pBuffer;    \
  uint32_t bufferSize;  \
  void *pData;          \
  int32_t width;        \
  int32_t pitch;        \
  uint32_t status;      \
  uint16_t type;        \
  uint16_t height;


typedef struct xvArrayStruct
{
  XV_ARRAY_FIELDS
} xvArray, *xvpArray;


typedef struct  xvTileStruct
{
  XV_ARRAY_FIELDS
  xvFrame             *pFrame;
  int32_t             x;
  int32_t             y;
  uint16_t            tileEdgeLeft;
  uint16_t            tileEdgeTop;
  uint16_t            tileEdgeRight;
  uint16_t            tileEdgeBottom;
  int32_t             dmaIndex;
  int32_t             reuseCount;
  struct xvTileStruct *pPrevTile;
} xvTile, *xvpTile;

#ifdef FIK_FRAMEWORK
typedef struct
{
  int32_t  frameWidth;
  int32_t  frameHeight;
  int32_t  tileWidth;
  uint16_t tileHeight;
  int32_t  x;
  int32_t  y;

} RefTile;

/*Structure holds overall tile based processing requirements*/
typedef struct
{
	int32_t numInTiles[2];					/*Number of input tiles for DMA in configuration*/
	xvTile* InTiles[2][MAX_TILES]; 	/* Ping/Pong input tile pointers for DMA in configuration*/
	int32_t numOutTiles[2];				/*Number of output tiles for DMA in configuration*/
	xvTile* OutTiles[2][MAX_TILES];	/* Ping/Pong output tile pointers for DMA in configuration*/
	void *KernelArgs[2];		/* Pointer to kernel arguments struct for each kernel ping/pong */
	void *CommonArgs;
	int32_t (*ProcessKernel)(void* CommonArgs_, void* TileArgs);		/* pointer to kernel fucntion provided by used inside which user can call kernel[s] for every tile */
	void (*SetupUpdatesTiles)(void* pxvTM, RefTile *pRefTile, void* _CommonArgs, void* TileArgs, int32_t updateOnlyFlag);/*Coordinate map function, Need to pass a function pointer incase of constom input to output tile mapping e.g. Transpose*/
	int32_t CoreID;					/* ID of current core should start from 0 to numCores -1 */
	int32_t numCores;				/* Total number of cores */
	uint32_t numTiles;
	uint32_t allocationStartIdx;
	RefTile refTile;
	uint32_t flags;
#if DBG_PROFILE
	uint32_t Cycles[5];
	/*uint32_t TotalCycles;
	uint32_t KernelCycles;
	uint32_t DMAWaitCycles;
	uint32_t MapFunCycles;
	uint32_t PadEdgesCycles;*/
#endif
}FIK_Context_t;

typedef struct xvTileManagerContextStruct
{
	uint32_t  tileAllocFlags[(MAX_NUM_TILES + 31) / 32];      // Each bit of tileAllocFlags and frameAllocFlags
	uint32_t  frameAllocFlags[(MAX_NUM_FRAMES + 31) / 32];    // indicates if a particular tile/frame is allocated
	int32_t   tileCount;
	int32_t   frameCount;
	uint32_t  allocationsIdx;
}xvTileManagerContext;
#endif

typedef struct xvTileManagerStruct
{
  // iDMA related
  void    *pdmaObj;
  int32_t tileDMApendingCount;       // Incremented when new request is added. Decremented when request is completed.
  int32_t tileDMAstartIndex;         // Incremented when request is completed
  xvTile  *tileProcQueue[MAX_NUM_DMA_QUEUE_LENGTH];
#ifndef XVTM_USE_XMEM
  // Mem Banks
  int32_t     numMemBanks;                       // Number of memory banks/pools
  xvmem_mgr_t memBankMgr[MAX_NUM_MEM_BANKS];     // xvmem memory manager, one for each bank
  void        *pMemBankStart[MAX_NUM_MEM_BANKS]; // Start address of bank
  int32_t     memBankSize[MAX_NUM_MEM_BANKS];    // size of each bank
#endif
#ifdef TM_LOG
  FILE *tm_log_fp;
#endif
  // Tiles and frame allocation
  xvTile    tileArray[MAX_NUM_TILES];
  xvFrame   frameArray[MAX_NUM_FRAMES];
  uint32_t  tileAllocFlags[(MAX_NUM_TILES + 31) / 32];      // Each bit of tileAllocFlags and frameAllocFlags
  uint32_t  frameAllocFlags[(MAX_NUM_FRAMES + 31) / 32];    // indicates if a particular tile/frame is allocated
  int32_t   tileCount;
  int32_t   frameCount;
  xvError_t errFlag;
  xvError_t idmaErrorFlag;

#ifdef FIK_FRAMEWORK
  xvTileManagerContext tmContext[MAX_TM_CONTEXT_SIZE];
  uint32_t             tmContextSize;
  void                 * allocatedList[XV_MAX_ALLOCATIONS];
  uint32_t             allocationsIdx;
  int32_t              AllocateErrorState;
  int32_t              AllocateColor;
  uint32_t             PingPongState;
  void                 * InterimTileList[INTERIM_MAX_TILES];
  uint32_t             InterimTileCounter;
  FIK_Context_t        * pWorkPacket;
#endif
} xvTileManager;


/***********************************
*    Function  Prototypes
***********************************/

// Initializes iDMA
// pxvTM            - Tile Manager object
// buf              - iDMA object and buffer
// numDesc          - Number of descriptors
// maxBlock         - Maximum block size
// maxPifReq        - Maximum pif request
// errCallbackFunc  - Callback function for iDMA interrupt error handling
// cbFunc           - Callback function for iDMA interrupt handling
// cbData           - Data required for interrupt callback function, cbFunc
// Returns XVTM_ERROR if it encounters an error, else returns XVTM_SUCCESS
int32_t  xvInitIdma(xvTileManager *pxvTM, idma_buffer_t *buf, int32_t numDescs, int32_t maxBlock,
                    int32_t maxPifReq, idma_err_callback_fn errCallbackFunc, idma_callback_fn cbFunc, void * cbData);

// Initializes Tile Manager
// pxvTM        - Tile Manager object
// pdmaObj      - iDMA buffer
// Returns XVTM_ERROR if it encounters an error, else returns XVTM_SUCCESS
int32_t xvInitTileManager(xvTileManager *pxvTM, void *pdmaObj);

// Resets Tile Manager
// pxvTM        - Tile Manager object
// Returns XVTM_ERROR if it encounters an error, else returns XVTM_SUCCESS
int32_t xvResetTileManager(xvTileManager *pxvTM);

// Initializes memory manager
// pxvTM         - Tile Manager object
// numMemBanks   - Number of memory pools
// pBankBuffPool - Array of start addresses of memory bank
// buffPoolSize  - Array of sizes of memory bank
// Returns XVTM_ERROR if it encounters an error, else returns XVTM_SUCCESS
int32_t xvInitMemAllocator(xvTileManager *pxvTM, int32_t numMemBanks, void **pBankBuffPool, int32_t* buffPoolSize);


// Allocates buffer from the pool
// pxvTM         - Tile Manager object
// buffSize      - size of requested buffer
// buffColor     - color/index of requested bufffer
// buffAlignment - Alignment of requested buffer
// Returns the buffer with requested parameters. If an error occurs, returns ((void *)(XVTM_ERROR))
void *xvAllocateBuffer(xvTileManager * pxvTM, int32_t buffSize, int32_t buffColor, int32_t buffAlignment);


// Releases the given buffer
// pxvTM - Tile Manager object
// pBuff - Pointer to buffer that needs to be released
// Returns XVTM_ERROR if it encounters an error, else returns XVTM_SUCCESS
int32_t xvFreeBuffer(xvTileManager  *pxvTM, void * pBuff);


// Releases all buffers. Reinitializes the memory allocator
// pxvTM - Tile Manager object
// Returns XVTM_ERROR if it encounters an error, else returns XVTM_SUCCESS
int32_t xvFreeAllBuffers(xvTileManager *pxvTM);


// Allocates single frame
// pxvTM - Tile Manager object
// Returns the pointer to allocated frame. Does not allocate frame data buffer.
// Returns ((xvFrame *)(XVTM_ERROR)) if it encounters an error.
xvFrame *xvAllocateFrame(xvTileManager *pxvTM);


// Releases given frame
// pxvTM  - Tile Manager object
// pFrame - Frame that needs to be released. Does not release buffer
// Returns XVTM_ERROR if it encounters an error, else returns XVTM_SUCCESS
int32_t xvFreeFrame(xvTileManager *pxvTM, xvFrame const *pFrame);


// Releases all allocated frames
// pxvTM  - Tile Manager object
// Returns XVTM_ERROR if it encounters an error, else returns XVTM_SUCCESS
int32_t  xvFreeAllFrames(xvTileManager *pxvTM);


// Allocates single tile
// pxvTM - Tile Manager object
// Returns the pointer to allocated tile. Does not allocate tile data buffer
// Returns ((xvTile *)(XVTM_ERROR)) if it encounters an error.
xvTile *xvAllocateTile(xvTileManager *pxvTM);


// Releases given tile
// pxvTM  - Tile Manager object
// pFrame - Tile that needs to be released. Does not release buffer
// Returns XVTM_ERROR if it encounters an error, else returns XVTM_SUCCESS
int32_t xvFreeTile(xvTileManager *pxvTM, xvTile const *pTile);


// Releases all allocated tiles
// pxvTM  - Tile Manager object
// Returns XVTM_ERROR if it encounters an error, else returns XVTM_SUCCESS
int32_t xvFreeAllTiles(xvTileManager *pxvTM);


// Add iDMA transfer request
// pxvTM                 - Tile Manager object
// dst                   - pointer to destination buffer
// src                   - pointer to source buffer
// rowSize               - number of bytes to transfer in a row
// numRows               - number of rows to transfer
// srcPitch              - source buffer's pitch in bytes
// dstPitch              - destination buffer's pitch in bytes
// interruptOnCompletion - if it is set, iDMA will interrupt after completing transfer
// Returns dmaIndex for this request. It returns -1 if it encounters an error
int32_t xvAddIdmaRequest(xvTileManager *pxvTM, void *dst, void *src, size_t rowSize,
                         int32_t numRows, int32_t srcPitch, int32_t dstPitch, int32_t interruptOnCompletion);


// Requests data transfer from frame present in system memory to local tile memory
// pxvTM          - Tile Manager object
// pTile          - destination tile
// pPrevTile - data is copied from this tile to pTile if the buffer overlaps
// interruptOnCompletion - if it is set, iDMA will interrupt after completing transfer
// Returns XVTM_ERROR if it encounters an error, else it returns XVTM_SUCCESS
int32_t xvReqTileTransferIn(xvTileManager *pxvTM, xvTile *pTile, xvTile *pPrevTile, int32_t interruptOnCompletion);


// Requests 8b, 16b or 32b data transfer from frame present in system memory to local tile memory
// pxvTM          - Tile Manager object
// pTile          - destination tile
// interruptOnCompletion - if it is set, iDMA will interrupt after completing transfer
// Returns XVTM_ERROR if it encounters an error, else it returns XVTM_SUCCESS
int32_t xvReqTileTransferInFast(xvTileManager *pxvTM, xvTile *pTile, int32_t interruptOnCompletion);


// Requests 16b data transfer from frame present in system memory to local tile memory
// pxvTM          - Tile Manager object
// pTile          - destination tile
// interruptOnCompletion - if it is set, iDMA will interrupt after completing transfer
// Returns XVTM_ERROR if it encounters an error, else it returns XVTM_SUCCESS
int32_t xvReqTileTransferInFast16(xvTileManager *pxvTM, xvTile *pTile, int32_t interruptOnCompletion);


// Requests data transfer from tile present in local memory to frame in system memory
// pxvTM - Tile Manager object
// pTile - source tile
// interruptOnCompletion - if it is set, iDMA will interrupt after completing transfer
// Returns XVTM_ERROR if it encounters an error, else it returns XVTM_SUCCESS
int32_t xvReqTileTransferOut(xvTileManager *pxvTM, xvTile *pTile, int32_t interruptOnCompletion);


// Requests 8b, 16b or 32b data transfer from tile present in local memory to frame in system memory
// pxvTM - Tile Manager object
// pTile - source tile
// interruptOnCompletion - if it is set, iDMA will interrupt after completing transfer
// Returns XVTM_ERROR if it encounters an error, else it returns XVTM_SUCCESS
int32_t xvReqTileTransferOutFast(xvTileManager *pxvTM, xvTile *pTile, int32_t interruptOnCompletion);


// Requests 16b data transfer from tile present in local memory to frame in system memory
// pxvTM - Tile Manager object
// pTile - source tile
// interruptOnCompletion - if it is set, iDMA will interrupt after completing transfer
// Returns XVTM_ERROR if it encounters an error, else it returns XVTM_SUCCESS
int32_t xvReqTileTransferOutFast16(xvTileManager *pxvTM, xvTile *pTile, int32_t interruptOnCompletion);


//Pads 8b, 16b or 32b edge of the given tile
// pxvTM - Tile Manager object
// pTile - tile
int32_t xvPadEdges(xvTileManager *pxvTM, xvTile *pTile);


//Pads 16b edge of the given tile
// pxvTM - Tile Manager object
// pTile - tile
int32_t xvPadEdges16(xvTileManager *pxvTM, xvTile *pTile);


// Check if dma transfer is done
// pxvTM - Tile Manager object
// index - index for dma transfer request
// Returns XVTM_ERROR if an error occurs
int32_t xvCheckForIdmaIndex(xvTileManager *pxvTM, int32_t index);


// Check if tile is ready
// pxvTM - Tile Manager object
// pTile - input tile
// Takes care of padding, tile reuse
// Completes all tile transfers before the input tile
// Returns 1 if tile is ready, else returns 0
// Returns XVTM_ERROR if an error occurs
int32_t xvCheckTileReady(xvTileManager *pxvTM, xvTile const *pTile);


// Check if input tile is free.
// A tile is said to be free if all data transfers pertaining to data resue from this tile is completed
// pxvTM - Tile Manager object
// pTile - output tile
// Returns 1 if tile is free, else returns 0
// Returns -1 if an error occurs
int32_t xvCheckInputTileFree(xvTileManager *pxvTM, xvTile const *pTile);


// Prints the most recent error information.
// pxvTm - Tile Manager object
// It returns the most recent error code.
xvError_t xvGetErrorInfo(xvTileManager const *pxvTM);

// Creates and initializes Tile Manager, Memory Allocator and iDMA.
// Returns XVTM_ERROR if it encounters an error, else returns XVTM_SUCCESS
// pxvTM - Tile Manager object
// buf0 - iDMA object. It should be initialized before calling this function. Contains descriptors and idma library object
// numMemBanks - Number of memory pools
// pBankBuffPool - Array of memory pool start address
// buffPoolSize - Array of memory pool sizes
// errCallbackFunc - Callback for dma transfer error
// intrCallbackFunc - Callback function executed when DMA transfer is complete
// cbData - Data needed for completion callback function
// descCount - Number of descriptors that can be added in buffer
// maxBlock - Maximum block size allowed
// numOutReq - Maximum number of outstanding pif requests
// Returns XVTM_SUCCESS on successful initializations,
// else returns XVTM_ERROR in case of error
int32_t xvCreateTileManager(xvTileManager *pxvTM, void *buf0, int32_t numMemBanks, void **pBankBuffPool, int32_t* buffPoolSize,
                            idma_err_callback_fn errCallbackFunc, idma_callback_fn intrCallbackFunc, void *cbData, int32_t descCount, int32_t maxBlock, int32_t numOutReq);

// Allocates single frame. It does not allocate buffer required for frame data.
// Initializes the frame elements
// pxvTM - Tile Manager object
// imgBuff - Pointer to image buffer
// frameBuffSize - Size of allocated image buffer
// width - Width of image
// height - Height of image
// pitch - Pitch of image
// pixRes - Pixel resolution of image in bytes
// numChannels - Number of channels in the image
// paddingtype - Supported padding type
// paddingVal - Padding value if padding type is edge extension
// Returns the pointer to allocated frame.
// Returns ((xvFrame *)(XVTM_ERROR)) if it encounters an error.
// Does not allocate frame data buffer.
xvFrame *xvCreateFrame(xvTileManager *pxvTM, void *imgBuff, uint32_t frameBuffSize, int32_t width, int32_t height, int32_t pitch, uint8_t pixRes, uint8_t numChannels, uint8_t paddingtype, uint32_t paddingVal);

// Allocates single tile and associated buffer data.
// Initializes the elements in tile
// pxvTM - Tile Manager object
// tileBuffSize - Size of allocated tile buffer
// width - Width of tile
// height - Height of tile
// pitch - Pitch of tile
// edgeWidth - Edge width of tile
// edgeHeight - Edge height of tile
// color - Memory pool from which the buffer should be allocated
// pFrame - Frame associated with the tile
// xvTileType - Type of tile
// alignType - Alignment tpye of tile. could be edge aligned of data aligned
// Returns the pointer to allocated tile.
// Returns ((xvTile *)(XVTM_ERROR)) if it encounters an error.
xvTile *xvCreateTile(xvTileManager *pxvTM, int32_t tileBuffSize, int32_t width, uint16_t height, int32_t pitch, uint16_t edgeWidth, uint16_t edgeHeight, int32_t color, xvFrame *pFrame, uint16_t xvTileType, int32_t alignType);

// Waits till DMA transfer is done for the given tile
// pxvTM - Tile Manager object
// pTile - input tile
// Takes care of padding, tile reuse
// Returns 1 if tile is ready, else returns 0
// Returns XVTM_ERROR if an error occurs
int32_t xvWaitForTile(xvTileManager *pxvTM, xvTile const *pTile);

// Sleeps till DMA transfer is done for the given tile
// pxvTM - Tile Manager object
// pTile - input tile
// Takes care of padding, tile reuse
// Returns 1 if tile is ready, else returns 0
// Returns XVTM_ERROR if an error occurs
int32_t xvSleepForTile(xvTileManager *pxvTM, xvTile const *pTile);

// Waitss till DMA transfer is done for the given dma index
// pxvTM - Tile Manager object
// dmaIndex - input dma index
// Returns 1 if tile is ready, else returns 0
// Returns XVTM_ERROR if an error occurs
int32_t xvWaitForiDMA(xvTileManager const *pxvTM, int32_t dmaIndex);

// Sleeps till DMA transfer is done for the given dma index
// pxvTM - Tile Manager object
// dmaIndex - input dma index
// Returns 1 if tile is ready, else returns 0
// Returns XVTM_ERROR if an error occurs
int32_t xvSleepForiDMA(xvTileManager const *pxvTM, int32_t dmaIndex);

// Waits till DMA transfer is done for the given tile
// pxvTM - Tile Manager object
// pTile - input tile
// Returns 1 if tile is ready, else returns 0
// Returns XVTM_ERROR if an error occurs
int32_t xvWaitForTileFast(xvTileManager const *pxvTM, xvTile  *pTile);

// Sleeps till DMA transfer is done for the given tile
// pxvTM - Tile Manager object
// pTile - input tile
// Returns 1 if tile is ready, else returns 0
// Returns XVTM_ERROR if an error occurs
int32_t xvSleepForTileFast(xvTileManager const *pxvTM, xvTile *pTile);

#if defined (__cplusplus)
}
#endif
#endif


