/*
 * Copyright (c) 2016-2018 by Cadence Design Systems, Inc.  ALL RIGHTS RESERVED.
 * These coded instructions, statements, and computer programs are the
 * copyrighted works and confidential proprietary information of
 * Cadence Design Systems Inc.  They may be adapted and modified by bona fide
 * purchasers for internal use, but neither the original nor any adapted
 * or modified version may be disclosed or distributed to third parties
 * in any manner, medium, or form, in whole or in part, without the prior
 * written consent of Cadence Design Systems Inc.  This software and its
 * derivatives are to be executed solely on products incorporating a Cadence
 * Design Systems processor.
 */
//Fix for xtensa RI-2020.4 build error.
//Tool needs something to compiles, else it generates error.
#include <stdio.h>
#if !defined(__XTENSA__)
#include <stdlib.h>
#include <string.h>
#include <xtensa/tie/xt_ivpn.h>
#include "tileManager.h"

idma_cntrl_t idma_cntrl;
idma_buf_t* _idma_buf_ptr;

void
_idma_reset()
{
  idma_cntrl.num_outstanding = 0;
  idma_cntrl.newest_task     = 0;
  idma_cntrl.oldest_task     = 0;
}

idma_status_t
idma_init(unsigned init_flags,
          idma_max_block_t block_sz,
          unsigned max_pif_req,
          idma_ticks_cyc_t ticks_per_cyc,
          unsigned timeout_ticks,
          idma_err_callback_fn err_cb_func)
{
  unsigned _haltable;

  if (max_pif_req > 64 || max_pif_req < 1) {
    return(IDMA_ERR_BAD_INIT);
  }

  IDMA_DISABLE_INTS();

  _idma_reset();

  _haltable = (init_flags & IDMA_OCD_HALT_ON) ? 1 : 0;

  if (max_pif_req == 64) {
    max_pif_req = 0;
  }

  idma_cntrl.settings  = (block_sz & IDMA_MAX_BLOCK_SIZE_MASK) << IDMA_MAX_BLOCK_SIZE_SHIFT;
  idma_cntrl.settings |= (max_pif_req & IDMA_OUTSTANDING_REG_MASK) << IDMA_OUTSTANDING_REG_SHIFT;
  idma_cntrl.settings |= (_haltable & IDMA_HALT_ON_OCD_MASK) << IDMA_HALT_ON_OCD_SHIFT;

  idma_cntrl.timeout  = (ticks_per_cyc & IDMA_TIMEOUT_CLOCK_MASK) << IDMA_TIMEOUT_CLOCK_SHIFT;
  idma_cntrl.timeout |= (timeout_ticks & IDMA_TIMEOUT_THRESHOLD_MASK) << IDMA_TIMEOUT_THRESHOLD_SHIFT;

  idma_cntrl.reg_base        = idmareg_base;
  idma_cntrl.num_outstanding = 0;

  idma_cntrl.err_cb_func = err_cb_func;

  IDMA_WER(idma_cntrl.settings, idmareg_base + IDMA_REG_SETTINGS);
  IDMA_WER(idma_cntrl.timeout, idmareg_base + IDMA_REG_TIMEOUT);

  /* Enable the uDMA */
  //_idma_enable();

#if 0 //XCHAL_HAVE_INTERRUPTS
  idma_register_interrupts(idma_sync_intr_handler, idma_sync_err_handler);
#endif


  idma_cntrl.oldest_task = NULL;
  idma_cntrl.newest_task = NULL;
  idma_cntrl.initialized = 1;

  IDMA_ENABLE_INTS();
  return((idma_status_t) IDMA_OK);
}

idma_status_t
idma_init_loop(idma_buffer_t *bufh,
               idma_type_t type,
               int32_t num_descs,
               void *cb_data,
               idma_callback_fn cb_func)
{
  idma_buf_t *buf   = (idma_buf_t *) bufh;
  idma_desc_t* desc = (idma_desc_t *) &buf->desc;
  buf->last_desc = (desc + num_descs * type);

  *(unsigned *) (buf->last_desc) = (unsigned) desc; //JUMP
  //_idma_disable();
  //_idma_set_start_address(desc);

  buf->cur_desc_i    = 0;
  buf->num_descs     = num_descs;
  buf->next_desc     = desc; //FIXME - CAN'T HAVE ALL THESE !!!
  buf->next_add_desc = desc;
  buf->type          = type;

  _idma_buf_ptr = buf;

  /* Num outstanding descs */
  buf->status = IDMA_TASK_EMPTY; // FIXME - out !!



#if 0 //XCHAL_HAVE_INTERRUPTS
  if (cb_func) {
    buf->cb_data = cb_data;
    buf->cb_func = cb_func;
  }

  idma_register_interrupts(idma_intr_loop_handler, idma_err_loop_handler);
#endif

  return((idma_status_t) IDMA_OK);
}

int32_t copy2d(void *dst,
               void *src,
               size_t row_size,
               unsigned flags,
               unsigned num_rows,
               unsigned src_pitch,
               unsigned dst_pitch)
{
  int32_t indx;
  for (indx = 0; indx < num_rows; indx++)
  {
    memcpy(dst, src, row_size);
    dst = (uint8_t *) dst + dst_pitch;
    src = (uint8_t *) src + src_pitch;
  }

  return(0);
}

idma_status_t add_and_schedule2d(void* pbuf,
	void *dst,
	void *src,
	size_t row_size,
	unsigned flags,
	unsigned num_rows,
	unsigned src_pitch,
	unsigned dst_pitch)
{
	int32_t indx;
	for (indx = 0; indx < num_rows; indx++)
	{
		memcpy(dst, src, row_size);
		dst = (uint8_t *)dst + dst_pitch;
		src = (uint8_t *)src + src_pitch;
	}

	return(IDMA_OK);
}

void dma_sleep()
{
}

int32_t idma_desc_done(int32_t index)
{
  //To improbe coverage. alternately return 0/1
  static int32_t Flag = 1;
  Flag = Flag ^ 1;
  return(Flag);
}

#endif //#if !defined(__XTENSA__ )

