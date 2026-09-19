/* dspcalling_orb.c — ORB feature extraction over the VDSP RPC channel.
 *
 * Four moves per frame, exactly the dspcalling_example.c pattern:
 *   CPU memcpy the image into the input MMZ buffer (vir)
 *   → fill DSPC_ORB_CMD_S via cmd.vir, buffers referenced by phy
 *   → dspc_rpc_ext(DSPC_ORB_CMD_ID, cmd, -1) and block
 *   → copy keypoints/descriptors out of the output MMZ buffers (vir)
 */
#include "dspcalling_orb.h"

#include <stdio.h>
#include <string.h>

#define DSPC_ORB_ERR(fmt, ...) \
    do { printf("[dspc_orb] %s:%d: " fmt "\n", __FUNCTION__, __LINE__, ## __VA_ARGS__); } while (0)

int dspc_orb_alloc(dspc_orb_t *o, FH_UINT32 max_w, FH_UINT32 max_h,
                   FH_UINT32 max_kps)
{
    if (!o || max_w == 0 || max_h == 0 || max_kps == 0) return -1;
    o->max_w = max_w; o->max_h = max_h; o->max_kps = max_kps;

    if (dspc_buf_alloc(&o->in,  "vdsp_orb_gray", max_w * max_h) != 0 ||
        dspc_buf_alloc(&o->kp,   "vdsp_orb_kp",
                       max_kps * sizeof(DSPC_ORB_KEYPOINT_S)) != 0 ||
        dspc_buf_alloc(&o->desc, "vdsp_orb_desc",
                       max_kps * DSPC_ORB_DESC_SIZE) != 0 ||
        dspc_buf_alloc(&o->cmd,  "vdsp_orb_cmd",
                       sizeof(DSPC_ORB_CMD_S)) != 0) {
        DSPC_ORB_ERR("MMZ allocation failed");
        dspc_orb_free(o);
        return -1;
    }
    return 0;
}

void dspc_orb_free(dspc_orb_t *o)
{
    if (!o) return;
    dspc_buf_free(&o->cmd);
    dspc_buf_free(&o->desc);
    dspc_buf_free(&o->kp);
    dspc_buf_free(&o->in);
}

int dspc_orb_extract(dspc_orb_t *o,
                     const FH_UINT8 *gray, FH_UINT32 w, FH_UINT32 h,
                     int nfeatures, float scaleFactor, int nlevels,
                     int iniThFAST, int minThFAST,
                     DSPC_ORB_KEYPOINT_S *out_kps, FH_UINT8 *out_desc,
                     FH_UINT32 *out_n)
{
    DSPC_ORB_CMD_S *cmd;

    if (!o || !gray || !out_kps || !out_desc || !out_n) return -1;
    if (w > o->max_w || h > o->max_h || w == 0 || h == 0) {
        DSPC_ORB_ERR("frame %ux%u exceeds capacity %ux%u", w, h, o->max_w, o->max_h);
        return -1;
    }

    /* 1. image bytes in (CPU virtual address) */
    memcpy(o->in.vir, gray, (size_t)w * h);

    /* 2. command block: image geometry + params, buffers by PHY address */
    cmd = (DSPC_ORB_CMD_S *)o->cmd.vir;
    memset(cmd, 0, sizeof(*cmd));
    cmd->u32Width       = w;
    cmd->u32Height      = h;
    cmd->u32Stride      = w;              /* tight rows */
    cmd->u32GrayPhyAddr = o->in.phy;
    cmd->s32NFeatures   = nfeatures;
    cmd->fScaleFactor   = scaleFactor;
    cmd->s32NLevels     = nlevels;
    cmd->s32IniThFAST   = iniThFAST;
    cmd->s32MinThFAST   = minThFAST;
    cmd->u32MaxKps      = o->max_kps;
    cmd->u32KpPhyAddr   = o->kp.phy;
    cmd->u32DescPhyAddr = o->desc.phy;

    /* 3. send and block */
    if (dspc_rpc_ext(DSPC_ORB_CMD_ID, &o->cmd, -1) != 0)
        return -1;

    /* 4. results out (trust but cap: a bad DSP build must not overrun) */
    FH_UINT32 n = cmd->u32NKps;
    if (n > o->max_kps) {
        DSPC_ORB_ERR("DSP returned n=%u > max=%u, clamping", n, o->max_kps);
        n = o->max_kps;
    }
    memcpy(out_kps,  o->kp.vir,   (size_t)n * sizeof(DSPC_ORB_KEYPOINT_S));
    memcpy(out_desc, o->desc.vir, (size_t)n * DSPC_ORB_DESC_SIZE);
    *out_n = n;
    return 0;
}
