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

#include <stdint.h>
#include <xtensa/tie/xt_ivpn.h>
#include "tileManager.h"
#ifndef XVTM_USE_XMEM

/****************************************************************************************
 *
 *	XVMEM allocation utility
 *
 *****************************************************************************************/

/**********************************************************************************
 * FUNCTION: xvmem_init()
 *
 * DESCRIPTION:
 *     Initializes memory manager object.
 *
 * INPUTS:
 *     xvmem_mgr_t   *mgr                     Memory manager object pointer
 *     void          *buf                     Memory pool buffer pointer
 *     int32_t       size                     Memory pool buffer size in bytes
 *     uint32_t      num_blocks               Number of memory blocks
 *     void          *header                  External memory block descriptor header pointer
 *
 * OUTPUTS:
 *     Returns XVMEM_OK on success, else it returns appropriate error code.
 *
 ********************************************************************************** */

xvmem_status_t xvmem_init(xvmem_mgr_t *mgr, void *buf, uint32_t size, uint32_t num_blocks, void *header)
{
  if (buf == NULL) {
    return(XVMEM_ERROR_POOL_NULL);
  }

  mgr->_buffer      = buf;
  mgr->_buffer_size = size;

  uint32_t num_words_in_block_bitvec;
  /* If there is an externally provided header, the blocks and the bitvector
   * are allocated from the header, else they are from the user provided pool */
  if (header != NULL) {
    mgr->_num_blocks          = num_blocks;
    num_words_in_block_bitvec = (mgr->_num_blocks + ((uint32_t) 31u)) >> 5;
    mgr->_blocks              = (xvmem_block_t *) header;
    mgr->_has_header          = 1;
  }
  else  {
    mgr->_num_blocks          = XVMEM_HEAP3_DEFAULT_NUM_BLOCKS;
    num_words_in_block_bitvec = (mgr->_num_blocks + ((uint32_t) 31u)) >> 5;
    mgr->_blocks              = (xvmem_block_t *) buf;
    mgr->_has_header          = 0;
  }
  mgr->_block_free_bitvec =
    (uint32_t *) (&(mgr->_blocks[mgr->_num_blocks]));

  mgr->_header_size = (uint32_t) (sizeof(xvmem_block_t) * mgr->_num_blocks) +
                      (uint32_t) (num_words_in_block_bitvec * ((uint32_t) 4u));

  /* Check if there is the minimum required buffer size and number of blocks */
  if (header == NULL) {
    if (size < (mgr->_header_size + ((uint32_t) XVMEM_HEAP3_MIN_ALLOC_SIZE))) {
      return(XVMEM_ERROR_POOL_SIZE);
    }
  }
  else  {
  }

  /* Initialize free and allocated list */
  mgr->_free_list_head._block_size     = 0;
  mgr->_free_list_head._buffer         = NULL;
  mgr->_free_list_head._aligned_buffer = NULL;
  mgr->_free_list_tail._block_size     = 0xffffffffU;
  mgr->_free_list_tail._buffer         = (void *) 0xffffffffU;
  mgr->_free_list_tail._aligned_buffer = (void *) 0xffffffffU;

  mgr->_alloc_list_head._block_size     = 0;
  mgr->_alloc_list_head._buffer         = NULL;
  mgr->_alloc_list_head._aligned_buffer = NULL;

  mgr->_allocated_bytes = 0;

  if (header != NULL) {
    /* Initialize the first block with the start address of the buffer and
     * size as the whole buffer and add to the free list */
    mgr->_blocks[0]._block_size     = size;
    mgr->_blocks[0]._buffer         = buf;
    mgr->_blocks[0]._aligned_buffer = buf;

    mgr->_free_list_head._next_block = &mgr->_blocks[0];
    mgr->_free_list_tail._next_block = NULL;
    mgr->_blocks[0]._next_block      = &mgr->_free_list_tail;

    mgr->_alloc_list_head._next_block = NULL;

    /* Set the first block as in use (set to 0) and rest as
     * available (set to 1) */
    mgr->_block_free_bitvec[0] = 0xfffffffeU;

    mgr->_free_bytes   = size;
    mgr->_unused_bytes = 0;
  }
  else  {
    /* The header (array of xvmem_block_t and the bitvector) is placed at the
     * start of buf. Data allocation begins after that. Initialize the first
     * block with the header address (start of buf) and header size. */
    mgr->_blocks[0]._block_size     = mgr->_header_size;
    mgr->_blocks[0]._buffer         = buf;
    mgr->_blocks[0]._aligned_buffer = buf;

    mgr->_alloc_list_head._next_block = &mgr->_blocks[0];
    mgr->_blocks[0]._next_block       = NULL;

    /* Initialize the second block with the address of the area in buffer
     * following the xvmem_block_t array and bitvector that is available for
     * user allocation. Size is set as the buffer size minus the space required
     * for the block array and bitvector */
    mgr->_blocks[1]._block_size = size - mgr->_blocks[0]._block_size;
    mgr->_blocks[1]._buffer     = (void *) ((uintptr_t) mgr->_blocks[0]._buffer +
                                            mgr->_blocks[0]._block_size);
    mgr->_blocks[1]._aligned_buffer = mgr->_blocks[1]._buffer;

    mgr->_free_list_head._next_block = &mgr->_blocks[1];
    mgr->_free_list_tail._next_block = NULL;
    mgr->_blocks[1]._next_block      = &mgr->_free_list_tail;

    /* Set the first two blocks as in use (set to 0) and rest as
     * available (set to 1) */
    mgr->_block_free_bitvec[0] = 0xfffffffcU;

    mgr->_free_bytes      = mgr->_blocks[1]._block_size;
    mgr->_unused_bytes    = mgr->_blocks[0]._block_size;
    mgr->_allocated_bytes = mgr->_unused_bytes;
  }

  mgr->_initialized = XVMEM_INITIALIZED;

  return(XVMEM_OK);
}

/**********************************************************************************
 * FUNCTION: xvmem_find_msbit()
 *
 * DESCRIPTION:
 *     Finds index of most significant non zero bit.
 *
 * INPUTS:
 *     uint32_t   n                     Input word
 *
 * OUTPUTS:
 *     Returns ndex of most significant non zero bit.
 *
 ********************************************************************************** */
uint32_t xvmem_find_msbit(uint32_t n)
{
#if defined(XCHAL_HAVE_NSA) && (XCHAL_HAVE_NSA)
  return(31 - XT_NSAU(n));
#else
  uint32_t nbits = 0;
  uint32_t n1    = n;
  while ((n1 != ((uint32_t) 0u)) && ((n1 & ((uint32_t) 1u)) == ((uint32_t) 0u))) {
    n1 >>= 1;
    nbits++;
  }
  return(nbits);
#endif
}

/**********************************************************************************
 * FUNCTION: xvmem_compute_new_size()
 *
 * DESCRIPTION:
 *     Align the block buffer address to the specified alignment
 *
 * INPUTS:
 *     xvmem_block_t   *block                 Memory block object pointer
 *     size_t          size                   Size of the buffer in bytes
 *     uint32_t        align                  Buffer  alignment in bytes
 *
 * OUTPUTS:
 *     Returns new size of buffer after alignment
 *
 ********************************************************************************** */
uint32_t xvmem_compute_new_size(xvmem_block_t const *block, size_t size, uint32_t align)
{
  /* Align the block buffer address to the specified alignment */
  uint32_t p              = xvmem_find_msbit(align);
  uintptr_t aligned_block = (uintptr_t) block->_buffer;
  aligned_block = (((aligned_block + align) - ((uint32_t) 1u)) >> p) << p;
  /* compute extra space required for alignment */
  uint32_t extra_size = (uint32_t) (aligned_block - (uintptr_t) block->_buffer);
  uint32_t new_size   = (uint32_t) (size + extra_size);
  return(new_size);
}

/**********************************************************************************
 * FUNCTION: leading_zero_count()
 *
 * DESCRIPTION:
 *     Computes number of leading zeros for a given position
 *
 * INPUTS:
 *     uint32_t       *bitvec                 Bitvector pointer
 *     uint32_t       num_bits                Number of bits
 *     uint32_t       pos                     Bit position
 *     uint32_t       w                       Data word
 *     uint32_t       zero_count              Current  zero count
 *
 * OUTPUTS:
 *     Returns number of leading zeros for pos
 *
 ********************************************************************************** */
uint32_t leading_zero_count(const uint32_t *bitvec, uint32_t num_bits, uint32_t pos,
                            uint32_t w, uint32_t zero_count)
{
  /* Find the word corresponding to bit position pos */
  uint32_t word_idx = pos >> 5;
  /* Find bit offset within this word */
  uint32_t word_off = pos & ((uint32_t) 31u);
  /* Not all of the word will be in use. If num_bits is < 32, then only
   * num_bits are used else all 32-bits are used */
  uint32_t num_bits_used_in_word = 32;
  XT_MOVLTZ(num_bits_used_in_word, num_bits, (int32_t) (num_bits - 32));
  /* Find remaining bits in word starting from off to end of word */
  uint32_t num_rem_bits_in_word = num_bits_used_in_word - word_off;
  uint32_t num_words_in_bitvec  = (num_bits + ((uint32_t) 31u)) >> 5;
  uint32_t w1                   = w ^ zero_count;
  uint32_t ws                   = w1 >> word_off;
  /* Find position of the least signficant 1b */
  uint32_t ls1b     = ws & ((uint32_t) (-(int32_t) ws));
  uint32_t ls1b_pos = xvmem_find_msbit(ls1b);
  /* If there are no 1s in the word then the leading count is the remaining
   * bits in the word */
  uint32_t lzc       = ls1b_pos;
  uint32_t num_bits1 = num_bits;
  XT_MOVEQZ(lzc, num_rem_bits_in_word, ls1b);
  uint32_t num_rem_words = num_words_in_bitvec - (word_idx + ((uint32_t) 1u));
  /* While there is no 1 in the word and there are words remaining, continue */
  while ((ls1b == ((uint32_t) 0u)) && (num_rem_words != ((uint32_t) 0u))) {
    num_bits1 -= 32u;
    word_idx++;
    num_rem_words--;
    w1                    = bitvec[word_idx];
    w1                   ^= zero_count;
    ls1b                  = w1 & ((uint32_t) (-(int32_t) w1));
    ls1b_pos              = xvmem_find_msbit(ls1b);
    num_bits_used_in_word = 32;
    XT_MOVLTZ(num_bits_used_in_word, num_bits1, (int32_t) (num_bits1 - 32));
    uint32_t l = ls1b_pos;
    XT_MOVEQZ(l, num_bits_used_in_word, ls1b);
    lzc += l;
  }
  return(lzc);
}

/**********************************************************************************
 * FUNCTION: xvmem_toggle_bitvec()
 *
 * DESCRIPTION:
 *     Toggle bit in given bit position
 *
 * INPUTS:
 *     uint32_t       *bitvec                 Bitvector pointer
 *     uint32_t       pos                     Bit position
 *     uint32_t       num_bits                Number of bits to end of word
 *
 * OUTPUTS:
 *
 *
 ********************************************************************************** */
void xvmem_toggle_bitvec(uint32_t *bitvec, uint32_t pos, uint32_t num_bits)
{
  /* Find the word corresponding to bit position pos */
  uint32_t word_idx = pos >> 5;
  /* Find bit offset within this word */
  uint32_t word_off = pos & ((uint32_t) 31u);
  /* Find remaining bits in word starting from off to end of word */
  uint32_t rem_bits_in_word = ((uint32_t) 32u) - word_off;
  uint32_t nbits            = rem_bits_in_word;
  /* If num_bits is greater than the remaining number of bits to end of word
   * toggle the remaining number of bits in word, else only toggle num_bits */
  XT_MOVLTZ(nbits, num_bits, num_bits - rem_bits_in_word);
  /* Form bit mask of the number of bits to toggle */
  uint32_t n = ((uint32_t) 1u) << nbits;
  XT_MOVEQZ(n, 0, nbits - 32);
  /* Align the mask to the word offset */
  uint32_t bmask = (n - ((uint32_t) 1u)) << word_off;
  /* Negate the bits in the word */
  bitvec[word_idx] ^= bmask;
  uint32_t num_bits1 = num_bits - nbits;
  word_idx++;
  /* If there are more bits remaining, continue with the next word */
  while (num_bits1 > (uint32_t) 0u) {
    nbits = 32;
    /* Form bit mask of either 32 1s or num_bits 1s if num_bits < 32 */
    XT_MOVLTZ(nbits, num_bits1, num_bits1 - 32);
    n = ((uint32_t) 1u) << nbits;
    XT_MOVEQZ(n, 0, nbits - 32);
    bmask = n - ((uint32_t) 1u);
    /* Negate the bits in the word */
    bitvec[word_idx] ^= bmask;
    num_bits1        -= nbits;
    word_idx++;
  }
}

/**********************************************************************************
 * FUNCTION: xvmem_alloc()
 *
 * DESCRIPTION:
 *     Allocates a buffer of requested size and alignment.
 *
 * INPUTS:
 *     xvmem_mgr_t       *mgr                 Memory management object pointer
 *     size_t            size                 Size of the buffer in bytes
 *     uint32_t          align                Alignment of buffer in bytes
 *     xvmem_status_t    *err_code            Return error code pointer
 *
 * OUTPUTS:
 *     Returns allocated buffer pointer (void*)
 *
 ********************************************************************************** */

void *
xvmem_alloc(xvmem_mgr_t *mgr, size_t size, uint32_t align, xvmem_status_t *err_code)
{
  void *r;

  if ((align & (align - ((uint32_t) 1u))) != ((uint32_t) 0u)) {
    *err_code = XVMEM_ERROR_ILLEGAL_ALIGN;
    return(NULL);
  }

  xvmem_block_t *prev_block = &mgr->_free_list_head;
  xvmem_block_t *curr_block = mgr->_free_list_head._next_block;


  /* Compute required size based on alignment */
  uint32_t new_size = xvmem_compute_new_size(curr_block, size, align);

  /* Check for the first available block in the free list that satisfies
   * the required size */
  while (curr_block->_block_size < new_size) {
    prev_block = curr_block;
    curr_block = curr_block->_next_block;
    new_size   = xvmem_compute_new_size(curr_block, size, align);
  }

  if (curr_block != &mgr->_free_list_tail) {
    curr_block->_aligned_buffer = (void *) ((uintptr_t) curr_block->_buffer +
                                            (new_size - size));
    /* The aligned buffer address gets returned to the user */
    r                   = curr_block->_aligned_buffer;
    mgr->_unused_bytes += (uint32_t) (new_size - size);

    /* If the remaining space in the block is < than the minimum alloc size,
     * just allocate the whole block to this request, else split the block
     * and add the remaining to the free list */
    if ((curr_block->_block_size - new_size) > XVMEM_HEAP3_MIN_ALLOC_SIZE) {
      /* Find a free block to hold the remaining of the current block */
      uint32_t avail_buf_idx = leading_zero_count(mgr->_block_free_bitvec, mgr->_num_blocks,
                                                  0, mgr->_block_free_bitvec[0], 0);

      /* Run out of block headers.
       * TODO: Do reallocate and expand the block header */
      if (avail_buf_idx >= mgr->_num_blocks) {
        *err_code = XVMEM_ERROR_ALLOC_FAILED;
        return(NULL);
      }
      /* Mark the new created block as allocated */
      xvmem_toggle_bitvec(mgr->_block_free_bitvec, avail_buf_idx, 1);
      xvmem_block_t *new_block = &mgr->_blocks[avail_buf_idx];
      new_block->_block_size     = curr_block->_block_size - new_size;
      new_block->_buffer         = (void *) ((uintptr_t) curr_block->_buffer + new_size);
      new_block->_aligned_buffer = new_block->_buffer;
      curr_block->_block_size    = new_size;
      /* Add to free list */
      new_block->_next_block  = curr_block->_next_block;
      prev_block->_next_block = new_block;
    }
    else  {
      new_size                = curr_block->_block_size;
      prev_block->_next_block = curr_block->_next_block;
    }
    /* Add the allocated block to beginning of the allocated list */
    curr_block->_next_block           = mgr->_alloc_list_head._next_block;
    mgr->_alloc_list_head._next_block = curr_block;
    mgr->_free_bytes                 -= curr_block->_block_size;
    mgr->_allocated_bytes            += curr_block->_block_size;
    *err_code                         = XVMEM_OK;
  }
  else  {
    *err_code = XVMEM_ERROR_ALLOC_FAILED;
    r         = NULL;
  }

  return(r);
}

/**********************************************************************************
 * FUNCTION: xvmem_add_block_to_free_list()
 *
 * DESCRIPTION:
 *     Adds a block to list of free blocks.
 *
 * INPUTS:
 *     xvmem_block_t     *new_block           Memory block object pointer
 *     xvmem_mgr_t       *mgr                 Memory management object pointer
 *
 * OUTPUTS:
 *     None
 *
 ********************************************************************************** */

void xvmem_add_block_to_free_list(xvmem_block_t *new_block, xvmem_mgr_t *mgr)
{
  xvmem_block_t *block;
  xvmem_block_t *new_block_local = new_block;
  /* Find block whose next free block's buffer address is >= buffer address of
   * the new block to be added */
  for (block = &mgr->_free_list_head;
       (uintptr_t) block->_next_block->_buffer < (uintptr_t) new_block_local->_buffer;
       block = block->_next_block) {
    ;
  }
  int32_t merge_with_prev = 0;
  int32_t merge_with_next = 0;
  /* Check if the previous block can be merged with the new block */
  if (block != &mgr->_free_list_head) {
    if (((uintptr_t) block->_buffer + block->_block_size) ==
        (uintptr_t) new_block_local->_buffer) {
      block->_block_size += new_block_local->_block_size;
      uint32_t new_block_bv_idx =
        ((uint32_t) (void *) new_block_local - ((uint32_t ) (void *) &mgr->_blocks[0])) >>
        XVMEM_HEAP3_BLOCK_STRUCT_SIZE_LOG2;
      xvmem_toggle_bitvec(mgr->_block_free_bitvec,
                          new_block_bv_idx, 1);
      new_block_local = block;
      merge_with_prev = 1;
    }
  }

  /* Check if new_block (or new_block that is merged with previous from above)
   * can be merged with the next */
  if (block->_next_block != &mgr->_free_list_tail) {
    if (((uintptr_t) new_block_local->_buffer + new_block_local->_block_size) ==
        (uintptr_t) block->_next_block->_buffer) {
      uint32_t next_block_bv_idx =
        ((uint32_t ) (void *) block->_next_block - ((uint32_t ) (void *) (&mgr->_blocks[0]))) >>
        XVMEM_HEAP3_BLOCK_STRUCT_SIZE_LOG2;
      xvmem_toggle_bitvec(mgr->_block_free_bitvec, next_block_bv_idx, 1);
      new_block_local->_block_size += block->_next_block->_block_size;
      new_block_local->_next_block  = block->_next_block->_next_block;
      if (merge_with_prev == 0) {
        block->_next_block = new_block_local;
      }
      merge_with_next = 1;
    }
  }

  if ((merge_with_prev == 0) && (merge_with_next == 0)) {
    new_block_local->_next_block = block->_next_block;
    block->_next_block           = new_block_local;
  }
}

/**********************************************************************************
 * FUNCTION: xvmem_free()
 *
 * DESCRIPTION:
 *     Frees given buffer.
 *
 * INPUTS:
 *     xvmem_mgr_t       *mgr                 Memory management object pointer
 *     void              *p                   Pointer to be freed
 * OUTPUTS:
 *     None
 *
 ***********************************************************************************/
void  xvmem_free(xvmem_mgr_t *mgr, void const *p)
{
  if (p == NULL) {
    return;
  }

  xvmem_block_t *block;
  xvmem_block_t *prev_block;

  /* Check the allocated list to find the matching block that holds p */
  prev_block = &mgr->_alloc_list_head;
  for (block = mgr->_alloc_list_head._next_block;
       block != NULL;
       block = block->_next_block) {
    if (block->_aligned_buffer == p) {
      break;
    }
    prev_block = block;
  }

  if (block == NULL) {
    return;
  }

  /* Remove from allocated list and add to free list */
  prev_block->_next_block = block->_next_block;
  mgr->_free_bytes       += block->_block_size;
  mgr->_allocated_bytes  -= block->_block_size;
  mgr->_unused_bytes     -= (uint32_t) ((uintptr_t) block->_buffer - (uintptr_t) block->_aligned_buffer);
  xvmem_add_block_to_free_list(block, mgr);
}
#endif

/****************************************************************************************
 *
 *	Tile Manager internal functions
 *
 *****************************************************************************************/
/**********************************************************************************
 * FUNCTION: copyBufferEdgeDataH()
 *
 * DESCRIPTION:
 *     Used for padding horizontal rows. Supports both zero padding and edge padding.
 *
 * INPUTS:
 *     uint8_t       *srcPtr                 Source buffer pointer
 *     uint8_t       *dstPtr                 Destination buffer pointer
 *     int32_t       widthBytes              Width of row in bytes
 *     int32_t       height                  Number of rows to copy
 *     int32_t       pitchBytes              Row pitch in bytes
 *     uint8_t       paddingType             Padding type
 *     uint8_t       paddingVal              Padding value
 *
 * OUTPUTS:
 *     None
 *
 ***********************************************************************************/
// Used for padding horizontal rows. Supports both zero padding and edge padding.
void xvCopyBufferEdgeDataH(uint8_t * __restrict srcPtr, uint8_t * __restrict dstPtr, int32_t widthBytes, int32_t pixWidth, int32_t height, int32_t pitchBytes, uint8_t paddingType, uint32_t paddingVal)
{
  int32_t indy, wb;
  xb_vec2Nx8U dvec1, * __restrict pdvecSrc, * __restrict pdvecDst;
  valign vas1, val1;
  uint8_t * __restrict dstPtrLocal = dstPtr;
  pdvecSrc = (xb_vec2Nx8U *) srcPtr;
  pdvecDst = (xb_vec2Nx8U *) dstPtr;

  if (paddingType == FRAME_EDGE_PADDING) {
    for (indy = 0; indy < height; indy++) {
      val1 = IVP_LA2NX8U_PP(pdvecSrc);
      vas1 = IVP_ZALIGN();
      for (wb = widthBytes; wb > 0; wb -= (2 * IVP_SIMD_WIDTH)) {
        IVP_LAV2NX8U_XP(dvec1, val1, pdvecSrc, wb);
        IVP_SAV2NX8U_XP(dvec1, vas1, pdvecDst, wb);
      }
      IVP_SAPOS2NX8U_FP(vas1, pdvecDst);
      dstPtrLocal = &dstPtrLocal[pitchBytes];
      pdvecSrc    = (xb_vec2Nx8U *) srcPtr;
      pdvecDst    = (xb_vec2Nx8U *) dstPtrLocal;
    }
  }
  else  {
    dvec1 = 0;
    if (paddingType == FRAME_CONSTANT_PADDING) {
      if ((((uint32_t) pixWidth) & ((uint32_t) 1u)) == ((uint32_t) 0u)) {
        xb_vecNx16U vec1 = paddingVal;
        dvec1 = IVP_MOV2NX8_FROMNX16(vec1);
      }
      else{
        dvec1 = paddingVal;
      }
    }

    vas1 = IVP_ZALIGN();
    for (indy = 0; indy < height; indy++) {
      for (wb = widthBytes; wb > 0; wb -= (2 * IVP_SIMD_WIDTH)) {
        IVP_SAV2NX8U_XP(dvec1, vas1, pdvecDst, wb);
      }
      IVP_SAPOS2NX8U_FP(vas1, pdvecDst);
      dstPtrLocal = &dstPtrLocal[pitchBytes];
      pdvecDst    = (xb_vec2Nx8U *) dstPtrLocal;
    }
  }
}

/**********************************************************************************
 * FUNCTION: copyBufferEdgeDataV()
 *
 * DESCRIPTION:
 *     Can be used for padding vertical columns. Supports both zero padding and edge padding .
 *
 * INPUTS:
 *     uint8_t       *srcPtr                 Source buffer pointer
 *     uint8_t       *dstPtr                 Destination buffer pointer
 *     int32_t       width                   Width of row in bytes
 *     int32_t       pixWidth                Pixel width in bytes
 *     int32_t       height                  Number of rows
 *     int32_t       pitchBytes              Row pitch in bytes
 *     uint8_t       paddingType             Padding type
 *     uint8_t       paddingVal              Padding value
 *
 * OUTPUTS:
 *     None
 *
 ***********************************************************************************/
// Can be used for padding vertical columns. Supports both zero padding and edge padding .
void  xvCopyBufferEdgeDataV(uint8_t const *__restrict srcPtr, uint8_t * __restrict dstPtr, int32_t width, int32_t pixWidth, int32_t height, int32_t pitchBytes, uint8_t paddingType, uint32_t paddingVal)
{
  int32_t indy, wb, widthBytes;
  xb_vec2Nx8U dvec1, * __restrict pdvecDst;
  xb_vecNx16U vec1, * __restrict pvecDst;
  valign vas1;
  uint8_t * __restrict dstPtrLocal       = dstPtr;
  uint8_t const * __restrict srcPtrLocal = srcPtr;

  widthBytes = width * pixWidth;

  if (paddingType == FRAME_EDGE_PADDING) {
    for (indy = 0; indy < height; indy++) {
      if (pixWidth == 1) {
        dvec1    = *srcPtrLocal;
        vas1     = IVP_ZALIGN();
        pdvecDst = (xb_vec2Nx8U *) dstPtrLocal;
        for (wb = widthBytes; wb > 0; wb -= (2 * IVP_SIMD_WIDTH)) {
          IVP_SAV2NX8U_XP(dvec1, vas1, pdvecDst, wb);
        }
        IVP_SAPOS2NX8U_FP(vas1, pdvecDst);
        dstPtrLocal = &dstPtrLocal[pitchBytes];
        srcPtrLocal = &srcPtrLocal[pitchBytes];
      }
      else  {
        if (pixWidth == 2) {
          vec1    = *((uint16_t const *) srcPtrLocal);
          vas1    = IVP_ZALIGN();
          pvecDst = (xb_vecNx16U *) dstPtrLocal;
          for (wb = widthBytes; wb > 0; wb -= (2 * IVP_SIMD_WIDTH)) {
            IVP_SAVNX16U_XP(vec1, vas1, pvecDst, wb);
          }
          IVP_SAPOSNX16U_FP(vas1, pvecDst);
          dstPtrLocal = &dstPtrLocal[pitchBytes];
          srcPtrLocal = &srcPtrLocal[pitchBytes];
        }
        else  {
          // TODO: Vectorize it
          for (wb = 0; wb < widthBytes; wb += pixWidth) {
            int32_t index;
            for (index = 0; index < pixWidth; index++) {
              dstPtrLocal[wb + index] = srcPtrLocal[index];
            }
          }
          dstPtrLocal = &dstPtrLocal[pitchBytes];
          srcPtrLocal = &srcPtrLocal[pitchBytes];
        }
      }
    }
  }
  else  {
    dvec1 = 0;

    if (paddingType == FRAME_CONSTANT_PADDING) {
      if ((((uint32_t) pixWidth) & ((uint32_t) 1u)) == ((uint32_t) 0u)) {
        vec1  = paddingVal;
        dvec1 = IVP_MOV2NX8_FROMNX16(vec1);
      }
      else {
        dvec1 = paddingVal;
      }
    }

    pdvecDst = (xb_vec2Nx8U *) dstPtrLocal;
    vas1     = IVP_ZALIGN();
    for (indy = 0; indy < height; indy++) {
      for (wb = widthBytes; wb > 0; wb -= (2 * IVP_SIMD_WIDTH)) {
        IVP_SAV2NX8U_XP(dvec1, vas1, pdvecDst, wb);
      }
      IVP_SAPOS2NX8U_FP(vas1, pdvecDst);
      dstPtrLocal = &dstPtrLocal[pitchBytes];
      pdvecDst    = (xb_vec2Nx8U *) dstPtrLocal;
    }
  }
}

