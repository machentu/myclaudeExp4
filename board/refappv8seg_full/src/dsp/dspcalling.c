/* dspcalling.c — host-side wrapper for the real VDSP RPC interface.
 *
 * Line-for-line mirror of the FH sample usage (vdsp_test_ylm): every FH_*
 * call here appears, with the same argument shapes, in dsp_init.c or
 * vdsp_persp_trans_trapezoid_test_ext. Nothing DSP-side is assumed.
 */
#include "dspcalling.h"

#include <stdio.h>

#define DSPC_ERR(fmt, ...) \
    do { printf("[dspc] %s:%d: " fmt "\n", __FUNCTION__, __LINE__, ## __VA_ARGS__); } while (0)

/* Firmware load sequence, order preserved from vdsp_test_init():
 * power-cycle first, then LoadBin into SVP_DSP_MEM_TYPE_SYS_DDR_DSP_0,
 * then power/enable. */
int dspc_init(const char *sram_bin_path)
{
    int err;

    err = FH_MPI_SVP_DSP_DisableCore(0);
    if (err) { DSPC_ERR("DisableCore failed: %d", err); return err; }

    err = FH_MPI_SVP_DSP_PowerOff(0);
    if (err) { DSPC_ERR("PowerOff failed: %d", err); return err; }

    printf("[dspc] loading DSP bin: %s\n", sram_bin_path);
    err = FH_MPI_SVP_DSP_LoadBin((char *)sram_bin_path, SVP_DSP_MEM_TYPE_SYS_DDR_DSP_0);
    if (err) { DSPC_ERR("LoadBin failed: %d", err); return err; }

    err = FH_MPI_SVP_DSP_PowerOn(0);
    if (err) { DSPC_ERR("PowerOn failed: %d", err); return err; }

    err = FH_MPI_SVP_DSP_EnableCore(0);
    if (err) { DSPC_ERR("EnableCore failed: %d", err); return err; }

    return 0;
}

void dspc_exit(void)
{
    FH_MPI_SVP_DSP_DisableCore(0);
    FH_MPI_SVP_DSP_PowerOff(0);
}

int dspc_buf_alloc(dspc_buf_t *b, const char *name, FH_UINT32 size)
{
    if (!b || size == 0) return -1;
    if (FH_SYS_VmmAlloc(&b->phy, (void **)&b->vir, (char *)name, NULL, size)
        != FH_SUCCESS) {
        DSPC_ERR("VmmAlloc failed (name=%s size=%u)", name ? name : "(null)", size);
        return -1;
    }
    b->size = size;
    return 0;
}

void dspc_buf_free(dspc_buf_t *b)
{
    if (!b || !b->phy) return;
    FH_SYS_VmmFree(b->phy);
    b->phy = 0; b->vir = NULL; b->size = 0;
}

int dspc_rpc_ext(FH_UINT32 cmd_id, const dspc_buf_t *cmd, int timeout_ms)
{
    SVP_DSP_HANDLE h = 0;
    SVP_DSP_REQ_PARAMS_S req = {0};

    if (!cmd || !cmd->phy) return -1;

    req.u32CmdId       = cmd_id;
    req.u32ParamPhyAddr = cmd->phy;   /* DSP reads the struct by PHY addr */
    req.u32ParamLen    = cmd->size;
    req.u32TimeoutMs   = (FH_UINT32)timeout_ms;  /* -1 → block until done */

    if (FH_MPI_SVP_DSP_RPC_Ext(&h, &req, 0) != FH_SUCCESS) {
        DSPC_ERR("RPC_Ext failed (cmd=%u)", cmd_id);
        return -1;
    }
    return 0;
}

FH_UINT64 dspc_pts_us(void)
{
    FH_UINT64 pts = 0;
    FH_SYS_GetCurPts(&pts);
    return pts;
}
