/* dspcalling.h — host-side wrapper for the real VDSP RPC interface.
 *
 * Distilled from the FH sample tree vdsp_test_ylm (dsp_init.c +
 * vdsp_persp_trans_trapezoid_test_ext in persp_trans.c): the canonical way
 * to invoke the VDSP is NOT a typed per-op API but the generic command
 * channel FH_MPI_SVP_DSP_RPC_Ext — the caller builds a command struct in
 * MMZ memory (CPU fills it via the virtual address, the DSP reads it via
 * the physical address), sends it, and the DSP writes results back into
 * MMZ buffers referenced by physical addresses inside that struct.
 *
 * The four-step pattern this wrapper captures:
 *   1. dspc_init(sram_bin)   — DisableCore → PowerOff → LoadBin →
 *                              PowerOn → EnableCore (loads the DSP firmware)
 *   2. dspc_buf_alloc()      — FH_SYS_VmmAlloc: one MMZ block, PHY addr for
 *                              the DSP + virtual addr for the CPU
 *   3. fill the command struct inside a cmd buffer (PHY addrs of the image /
 *      output / parameter buffers), then dspc_rpc_ext()
 *   4. read results via the output buffer's virtual address, dspc_buf_free()
 *
 * Linux-only (FH MPI headers + /dev/mc_vdsp backend); not part of the
 * Windows cstub build. DSP-side implementation is out of scope.
 */
#ifndef DSPCALLING_H
#define DSPCALLING_H

#include "fh_vdsp_mpi.h"    /* SVP_DSP_* types, FH_MPI_SVP_DSP_* */
#include "fh_system_mpi.h"  /* FH_SYS_VmmAlloc/VmmFree, FH_SYS_GetCurPts */

#ifdef __cplusplus
extern "C" {
#endif

/* One MMZ allocation: the DSP dereferences `phy`, the CPU dereferences
 * `vir` — both point at the same bytes. */
typedef struct {
    FH_UINT32 phy;
    FH_UINT8 *vir;
    FH_UINT32 size;
} dspc_buf_t;

/* Load the DSP firmware (sram bin, e.g. sram-83b0-1.6.2.5-tile16-h.bin)
 * and bring core 0 up. Returns 0 on success. Safe to call once at boot. */
int dspc_init(const char *sram_bin_path);

/* Tear core 0 down again (inverse of the first two init steps). */
void dspc_exit(void);

/* Allocate / free an MMZ buffer. `name` shows up in driver dumps
 * (e.g. "vdsp_cmdpara"); may be NULL. Returns 0 on success. */
int  dspc_buf_alloc(dspc_buf_t *b, const char *name, FH_UINT32 size);
void dspc_buf_free(dspc_buf_t *b);

/* Send one command and wait for it to finish (timeout_ms == -1 blocks
 * forever, matching the sample). `cmd` is a buffer previously filled by
 * the CPU through cmd->vir; its physical address and size go into the
 * request. Returns 0 on success. */
int dspc_rpc_ext(FH_UINT32 cmd_id, const dspc_buf_t *cmd, int timeout_ms);

/* Monotonic timestamp in us (wraps FH_SYS_GetCurPts) — for measuring
 * individual RPC costs, as the sample does around RPC_Ext. */
FH_UINT64 dspc_pts_us(void);

#ifdef __cplusplus
}
#endif

#endif /* DSPCALLING_H */
