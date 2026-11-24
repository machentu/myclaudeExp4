/* dspcalling_example.c — runnable template for the RPC_Ext calling style.
 *
 * Reimplements the sample's vdsp_persp_trans_trapezoid_test_ext (persp_trans.c
 * case -c 6) on top of the dspc_* wrapper: an NV12 input goes in, a
 * perspective transform defined by 4 src/dst vertex pairs comes back out.
 * It is the pattern to copy when wiring a new DSP op (e.g. the ORB
 * extractor): define the command struct, put image/output buffers in MMZ,
 * reference them by physical address inside the command, send via
 * dspc_rpc_ext, harvest the results through the virtual addresses.
 *
 * Build (on the device box, SDK headers + libmpi on the include/link path):
 *   $(CC) dspcalling.c dspcalling_example.c -o dspcalling_example -lmpi -lpthread -lm
 * Run:
 *   ./dspcalling_example -s trapezoid/sram-83b0-1.6.2.5-tile16-h.bin \
 *                        -r 864 480 -i trapezoid/in_864x480_nv12.yuv
 */
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "dspcalling.h"

/* ---- command struct, verbatim from persp_trans.h: image descriptors by
 * physical address + op parameters. The DSP-side handler reads exactly
 * this layout (SVP_DSP_IMAGE_EXT_S / SVP_DSP_VERTEX_S come from
 * fh_vdsp_mpi.h), so do not reorder fields. ---- */
typedef struct {
    SVP_DSP_IMAGE_EXT_S stInputImage;
    SVP_DSP_IMAGE_EXT_S stOutPutImage;
    SVP_DSP_VERTEX_S    stSrcPos[4];
    SVP_DSP_VERTEX_S    stDstPos[4];
    FH_BOOL             bUpdateFlg;
} TRANS_TRAPEZOID_S;

/* ---- file helpers copied from the sample ---- */
static int load_file(FH_UINT8 *dst, const char *path)
{
    struct stat st;
    FILE *f;

    if (stat(path, &st) != 0 || !(f = fopen(path, "rb"))) {
        printf("open %s failed\n", path);
        return -1;
    }
    fread(dst, st.st_size, 1, f);
    fclose(f);
    return 0;
}

static void save_output(const FH_UINT8 *src, const char *path, FH_UINT32 size)
{
    FILE *f = fopen(path, "wb");
    if (!f) { printf("save %s failed\n", path); return; }
    fwrite(src, size, 1, f);
    fclose(f);
    printf("saved %u bytes to %s\n", size, path);
}

static void usage(const char *prog)
{
    printf("Usage: %s -s sram.bin -r width height -i input.nv12 [-o out.yuv]\n", prog);
}

int main(int argc, char *argv[])
{
    const char *sram = NULL, *input = NULL, *output = "out_dspcalling.yuv";
    FH_UINT32 w = 0, h = 0;
    int ch;

    const struct option opts[] = {
        {"sram", required_argument, NULL, 's'},
        {"res",  required_argument, NULL, 'r'},
        {"image", required_argument, NULL, 'i'},
        {"out",  required_argument, NULL, 'o'},
        {0, 0, 0, 0},
    };
    while ((ch = getopt_long(argc, argv, "s:r:i:o:", opts, NULL)) != -1) {
        switch (ch) {
        case 's': sram  = optarg; break;
        case 'r': w = (FH_UINT32)atoi(optarg);
                  if (optind < argc) h = (FH_UINT32)atoi(argv[optind]); break;
        case 'i': input = optarg; break;
        case 'o': output = optarg; break;
        default:  usage(argv[0]); return 1;
        }
    }
    if (!sram || !input || !w || !h) { usage(argv[0]); return 1; }

    const FH_UINT32 img_size = w * h * 3 / 2;   /* NV12 */

    if (dspc_init(sram) != 0)
        return 1;

    /* step 1: MMZ buffers — input image, output image, command block */
    dspc_buf_t buf_in = {0}, buf_out = {0}, buf_cmd = {0};
    if (dspc_buf_alloc(&buf_in,  "vdsp_input",   img_size) != 0) goto exit;
    if (dspc_buf_alloc(&buf_out, "vdsp_result",  img_size) != 0) goto exit;
    if (dspc_buf_alloc(&buf_cmd, "vdsp_cmdpara", sizeof(TRANS_TRAPEZOID_S)) != 0) goto exit;

    if (load_file(buf_in.vir, input) != 0)
        goto exit;

    /* step 2: fill the command struct via the CPU virtual address; every
     * buffer reference inside it is a PHYSICAL address */
    TRANS_TRAPEZOID_S *cmd = (TRANS_TRAPEZOID_S *)buf_cmd.vir;
    memset(cmd, 0, sizeof(*cmd));

    cmd->stInputImage.enType        = SVP_IMAGE_TYPE_YUV420SP;
    cmd->stInputImage.u32Width      = w;
    cmd->stInputImage.u32Height     = h;
    cmd->stInputImage.au32PhyAddr[0] = buf_in.phy;              /* Y  */
    cmd->stInputImage.au32Stride[0]  = w;
    cmd->stInputImage.au32PhyAddr[1] = buf_in.phy + w * h;      /* UV */

    cmd->stOutPutImage.u32Width      = w;
    cmd->stOutPutImage.u32Height     = h;
    cmd->stOutPutImage.au32PhyAddr[0] = buf_out.phy;
    cmd->stOutPutImage.au32Stride[0]  = w;
    cmd->stOutPutImage.au32PhyAddr[1] = buf_out.phy + w * h;

    cmd->bUpdateFlg = 1;

    /* the sample's src/dst quads */
    {
        SVP_DSP_VERTEX_S src[4] = {{689, 81}, {1314, 68}, {1372, 1018}, {669, 1017}};
        SVP_DSP_VERTEX_S dst[4] = {{1071, 508}, {1069, 1207}, {8, 1210}, {22, 499}};
        memcpy(cmd->stSrcPos, src, sizeof(src));
        memcpy(cmd->stDstPos, dst, sizeof(dst));
    }

    /* step 3: send + block (-1), measuring the round trip like the sample */
    FH_UINT64 t0 = dspc_pts_us();
    if (dspc_rpc_ext(0, &buf_cmd, -1) != 0)
        goto exit;
    FH_UINT64 t1 = dspc_pts_us();
    printf("RPC_Ext round trip: %llu us\n", (unsigned long long)(t1 - t0));

    /* step 4: results land in buf_out via DMA; read them through vir */
    save_output(buf_out.vir, output, img_size);

exit:
    dspc_buf_free(&buf_cmd);
    dspc_buf_free(&buf_out);
    dspc_buf_free(&buf_in);
    dspc_exit();
    return 0;
}
