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

#ifndef TILEMANAGER_FIK_API_H__
#define TILEMANAGER_FIK_API_H__


#ifdef FIK_FRAMEWORK

// Allocates single frame. It does not allocate buffer required for frame data.
// Initializes the frame elements
// pxvTM - Tile Manager object
// imgBuff - Pointer to image buffer
// width - Width of image
// height - Height of image
// pitch - Pitch of image
// pixRes - Pixel resolution of image in bytes
// numChannels - Number of channels in the image
// paddingtype - Supported padding type
// paddingVal - Padding value if padding type is edge extension
// Returns the pointer to allocated frame.
// Returns ((XVTM_ERROR)) if it encounters an error.
// Does not allocate frame data buffer.
int32_t xvSetupFrame(xvTileManager *pxvTM, xvFrame * pFrame, void *imgBuff, int32_t width, int32_t height, int32_t pitch, uint8_t pixRes, uint8_t numChannels, uint8_t paddingtype, uint32_t paddingVal);

int32_t xvSetupTile(xvTileManager *pxvTM, xvTile *pTile, int32_t tileBuffSize, int32_t width, uint16_t height, int32_t pitch, uint16_t edgeWidth, uint16_t edgeHeight, int32_t color, xvFrame *pFrame, uint16_t xvTileType, int32_t alignType);

int32_t xvRegisterTile(xvTileManager *pxvTM, xvTile* pTile, void *pBuff, xvFrame *pFrame, uint32_t DMAInOut);

int32_t xvBufferCheckPointSave(xvTileManager* pxvTM);

int32_t xvBufferCheckPointRestore(xvTileManager* pxvTM, uint32_t Idx);

int32_t xvTileManagerContextSave(xvTileManager * pxvTM);

int32_t xvTileManagerContextRestore(xvTileManager * pxvTM);

int32_t xvCheckPointSave(xvTileManager *pxvTM);

int32_t xvCheckPointRestore(xvTileManager *pxvTM);

int32_t xvGetArgParamsContext(xvTileManager *pxvTM, RefTile *pRefTile, uint32_t ArgSize,void* pComArgs,
		 void (*SetupUpdatesTiles)(xvTileManager *pxvTM, RefTile *pRefTile, void* CommonArgs, void* TileArgs, int updateOnlyFlag),
		 int32_t (*ProcessKernel)(void* CommonArgs, void* TileArgs),
		uint16_t CoreID, uint16_t NumCores, uint32_t Flags, FIK_Context_t** dpWorkPacket);

int32_t xvProcessTileWiseFast(FIK_Context_t *WPacket, xvTileManager *pxvTM);

int32_t xvExecuteFullImageKernel(xvTileManager *pxvTM, RefTile *pRefTile, uint32_t ArgSize,void* pComArgs,
		 	 	 void (*SetupUpdatesTiles)(xvTileManager *pxvTM, RefTile *pRefTile, void* CommonArgs, void* TileArgs, int updateOnlyFlag),
		 	 	int32_t (*ProcessKernel)(void* CommonArgs, void* TileArgs),
		 		uint16_t CoreID, uint16_t NumCores, uint32_t Flags);

int32_t xvMemCpy(xvTileManager *pxvTM, void * destination, void * source, int32_t num );
#endif

#endif /* TILEMANAGER_FIK_API_H__ */
