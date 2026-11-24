/* orb_dsp.c — VDSP ORB backend for NGD_ORB_DSP=1 (see ngd_app/orb_dsp.h).
 *
 * Command struct + output layout mirror orb_dsp_test/main.c, which is the
 * board-verified reference for firmware HelloWorld/main.c v1015.
 */
#include "ngd_app/orb_dsp.h"
#include "dsp/dspcalling.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>       /* sleep — LoadBin 安顿竞态重试 */

/* ---- firmware-side command struct = imgInfo.h fhTRANS_TRAPEZOID_S,
 *      field order must not change ---- */
typedef struct {
    SVP_DSP_IMAGE_EXT_S stInputImage;
    SVP_DSP_IMAGE_EXT_S stOutPutImage;
    SVP_DSP_VERTEX_S    stSrcPos[4];
    SVP_DSP_VERTEX_S    stDstPos[4];
    FH_SINT32           bUpdateFlg;
} orb_dsp_cmd_t;

/* ---- firmware output layout constants (MAXK=2000, offsets fixed) ---- */
#define ORB_DSP_MAXK      2000
#define ORB_DSP_KP_BYTES  24                      /* sizeof(ngd_keypoint) */
#define ORB_DSP_DESC_OFF  (4 + ORB_DSP_MAXK * ORB_DSP_KP_BYTES)
#define ORB_DSP_OUT_BYTES 112532                  /* desc area + telemetry (v1015) */
#define ORB_DSP_MAX_W     640                     /* DSP_ORB_VP6_MAX_W */
#define ORB_DSP_RPC_TIMEOUT_MS 5000               /* ~10x normal 460ms/frame; E12 hang was -1=forever */

static dspc_buf_t g_in, g_out, g_cmd;
static char g_fwpath[256];                        /* kept for E12 timeout retry (dspc re-init) */
static int g_w = 0, g_h = 0;
static int g_ready = 0;

int orb_dsp_init(int w, int h)
{
    const char *fw = getenv("NGD_ORB_DSP_FW");
    if (!fw || !fw[0]) fw = "/mnt/xc01/use_sdkv2/orb_dsp_test/sram_all.bin";
    snprintf(g_fwpath, sizeof(g_fwpath), "%s", fw);

    if (w <= 0 || w > ORB_DSP_MAX_W || h <= 0) {
        printf("[orb_dsp] bad frame size %dx%d (width limit %d)\n", w, h, ORB_DSP_MAX_W);
        return -1;
    }
    memset(&g_in, 0, sizeof(g_in));
    memset(&g_out, 0, sizeof(g_out));
    memset(&g_cmd, 0, sizeof(g_cmd));

    /* E20: LoadBin 在 insmod 后过早调用会失败 (2s=失败, 分钟级=正常) —— 驱动
     * 安顿竞态。重试 3 次 (间隔 10s), 每次完整重走 dspc_init 电源循环。 */
    {
        int rc = -1;
        for (int i = 0; i < 3; ++i) {
            rc = dspc_init(fw);
            if (rc == 0) break;
            printf("[orb_dsp] dspc_init attempt %d failed (rc=%d)%s\n",
                   i + 1, rc, i < 2 ? ", retry in 10s" : "");
            if (i < 2) sleep(10);
        }
        if (rc != 0) return -1;
    }
    {
        FH_UINT32 ver = 0;
        if (FH_MPI_SVP_DSP_GetModelVersion(&ver) == 0)
            printf("[orb_dsp] firmware version: 0x%08x (%s)\n", ver, fw);
    }
    if (dspc_buf_alloc(&g_in,  "vdsp_orb_gray", (FH_UINT32)(w * h)) != 0) goto fail;
    if (dspc_buf_alloc(&g_out, "vdsp_orb_out",  ORB_DSP_OUT_BYTES)   != 0) goto fail;
    if (dspc_buf_alloc(&g_cmd, "vdsp_orb_cmd",  sizeof(orb_dsp_cmd_t)) != 0) goto fail;

    g_w = w; g_h = h; g_ready = 1;
    return 0;
fail:
    dspc_buf_free(&g_in); dspc_buf_free(&g_out); dspc_buf_free(&g_cmd);
    dspc_exit();
    return -1;
}

int orb_dsp_extract(const ngd_orb_extractor *ex,
                    const uint8_t *gray, int w, int h, int stride,
                    ngd_keypoint *out_kps, int max_kps, uint8_t *out_desc)
{
    (void)ex;   /* firmware params fixed (1000, 1.2f, 8, 20, 7) */
    if (!g_ready || !gray || w != g_w || h != g_h) return -1;

    /* firmware requires stride == w: compact rows if the caller strided */
    if (stride == w) {
        memcpy(g_in.vir, gray, (size_t)w * h);
    } else {
        for (int y = 0; y < h; ++y)
            memcpy(g_in.vir + (size_t)y * w, gray + (size_t)y * stride, (size_t)w);
    }

    orb_dsp_cmd_t *c = (orb_dsp_cmd_t *)g_cmd.vir;
    memset(c, 0, sizeof(*c));
    c->stInputImage.enType          = SVP_IMAGE_TYPE_YUV400;
    c->stInputImage.u32Width        = (FH_UINT32)w;
    c->stInputImage.u32Height       = (FH_UINT32)h;
    c->stInputImage.au32PhyAddr[0]  = g_in.phy;
    c->stInputImage.au32Stride[0]   = (FH_UINT32)w;
    c->stOutPutImage.u32Width       = (FH_UINT32)w;
    c->stOutPutImage.u32Height      = (FH_UINT32)h;
    c->stOutPutImage.au32PhyAddr[0] = g_out.phy;
    c->stOutPutImage.au32Stride[0]  = (FH_UINT32)w;
    c->bUpdateFlg                   = 1;

    if (dspc_rpc_ext(0, &g_cmd, ORB_DSP_RPC_TIMEOUT_MS) != 0) {
        /* E12 (2/3 fresh boots): first RPC after EnableCore sometimes never
         * completes — driver state=busy, runtimes=0, mc_vdsp IRQ never fires;
         * with timeout=-1 the process wedges in D-state, unkillable. Power-cycle
         * the DSP core + reload firmware, retry once before giving up on the frame. */
        printf("[orb_dsp] RPC timeout after %dms — power-cycling DSP, retry once\n",
               ORB_DSP_RPC_TIMEOUT_MS);
        if (dspc_init(g_fwpath) != 0) return -1;
        if (dspc_rpc_ext(0, &g_cmd, ORB_DSP_RPC_TIMEOUT_MS) != 0) return -1;
    }

    unsigned int n = *(const unsigned int *)g_out.vir;
    if (n > ORB_DSP_MAXK)  n = ORB_DSP_MAXK;
    if ((int)n > max_kps)  n = (unsigned int)max_kps;
    if (out_kps)  memcpy(out_kps,  g_out.vir + 4,               (size_t)n * ORB_DSP_KP_BYTES);
    if (out_desc) memcpy(out_desc, g_out.vir + ORB_DSP_DESC_OFF, (size_t)n * 32);
    return (int)n;
}

void orb_dsp_exit(void)
{
    if (!g_ready) return;
    dspc_buf_free(&g_in);
    dspc_buf_free(&g_out);
    dspc_buf_free(&g_cmd);
    dspc_exit();
    g_ready = 0; g_w = g_h = 0;
}
