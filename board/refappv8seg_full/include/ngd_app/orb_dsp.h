#ifndef NGD_APP_ORB_DSP_H
#define NGD_APP_ORB_DSP_H

/*
 * ngd_app/orb_dsp.h — VDSP ORB extraction backend (xc01), enabled by
 * NGD_ORB_DSP=1 (see system.cpp; board toolchain only).
 *
 * Wraps the board-verified orb_dsp_test RPC contract (firmware sram_all.bin
 * v1015, 8/8 golden PASS):
 *   dspc_init(fw) → 3×dspc_buf_alloc → per frame: gray→MMZ, RPC_Ext,
 *   out layout { int32 n; n×24B kp; n×32B desc @ +48004 } (fixed offsets,
 *   MAXK=2000). Keypoint/descriptor layout is byte-identical to the
 * ngd_orb_extract() contract, so orb_dsp_extract() installs directly as the
 * ngd_orb_set_backend() hook; mask filtering stays on the CPU side (frame.c).
 *
 * Firmware parameters are hardcoded (1000, 1.2f, 8, 20, 7 == TUM3 defaults):
 * the `ex` argument is ignored, and the ex_dyna (1.5×) variant therefore
 * yields the same extraction. Width limit 640 (DSP_ORB_VP6_MAX_W).
 *
 * Firmware path: env NGD_ORB_DSP_FW, default
 * /mnt/xc01/use_sdkv2/orb_dsp_test/sram_all.bin
 */
#include "ngd/orb.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Load the firmware and allocate the RPC buffers. w/h must match the frames
 * later passed to orb_dsp_extract(). Returns 0 on success. */
int  orb_dsp_init(int w, int h);

/* Release the RPC buffers and power the DSP core down (no-op if not inited). */
void orb_dsp_exit(void);

/* ngd_orb_extract_fn_t-compatible extraction; `ex` ignored (see above).
 * Returns the keypoint count, or -1 on error. */
int  orb_dsp_extract(const ngd_orb_extractor *ex,
                     const uint8_t *gray, int w, int h, int stride,
                     ngd_keypoint *out_kps, int max_kps,
                     uint8_t *out_desc);

#ifdef __cplusplus
}
#endif
#endif /* NGD_APP_ORB_DSP_H */
