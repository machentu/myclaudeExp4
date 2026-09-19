/* dspcalling_orb.h — ORB feature extraction over the VDSP RPC channel.
 *
 * Same pattern as dspcalling.h (source: vdsp_test_ylm's
 * vdsp_persp_trans_trapezoid_test_ext): one command struct in MMZ holding
 * the image descriptor, scalar parameters, and PHYSICAL addresses of the
 * input/ouput buffers; one blocking dspc_rpc_ext() per frame; results
 * read back through the output buffers' virtual addresses.
 *
 * Wire structs mirror refactor's ngd types (ngd/orb.h) exactly — field
 * order, types and meaning — so the app can overlay them on its own
 * ngd_keypoint / parameter arrays. All members are 4-byte; host (ARM) and
 * DSP (Xtensa fy01) are both 32-bit little-endian, so the layout is
 * identical on both sides with no padding.
 *
 * DSP-side implementation is out of scope here; the contract is this
 * header. Persistent buffers (image/keys/descs/command) are allocated
 * once by dspc_orb_alloc() and reused per frame, like the app's other
 * extractors (orb_tile_manager_init pattern).
 */
#ifndef DSPCALLING_ORB_H
#define DSPCALLING_ORB_H

#include "dspcalling.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Command id sent with every ORB request. The sample firmware dispatches
 * everything with u32CmdId = 0 (the loaded bin determines the op); an
 * ORB-capable bin keeps the same convention. Renumber only together with
 * the DSP-side dispatcher. */
#define DSPC_ORB_CMD_ID 0

/* ORB descriptor size in bytes (256 bits), as everywhere in the project. */
#define DSPC_ORB_DESC_SIZE 32

/* One keypoint — ngd_keypoint verbatim (field-for-field). */
typedef struct {
    FH_FLOAT  x, y;       /* pixel coords, level-0 frame */
    FH_FLOAT  angle;      /* orientation, degrees [0,360) */
    FH_FLOAT  response;   /* FAST corner score */
    FH_SINT32 octave;     /* pyramid level */
    FH_FLOAT  size;       /* PATCH_SIZE * scaleFactor^octave */
} DSPC_ORB_KEYPOINT_S;

/* The command block the CPU fills (via vir) and the DSP reads (via phy).
 * Every buffer reference is a physical address of an MMZ allocation. */
typedef struct {
    /* input grayscale image, tight rows (stride == width) */
    FH_UINT32 u32Width;
    FH_UINT32 u32Height;
    FH_UINT32 u32Stride;
    FH_UINT32 u32GrayPhyAddr;        /* width * height bytes */

    /* extractor parameters (ngd_orb_init semantics) */
    FH_SINT32 s32NFeatures;
    FH_FLOAT  fScaleFactor;
    FH_SINT32 s32NLevels;
    FH_SINT32 s32IniThFAST;
    FH_SINT32 s32MinThFAST;

    /* output buffers + capacity; DSP writes u32NKps on completion */
    FH_UINT32 u32MaxKps;
    FH_UINT32 u32KpPhyAddr;          /* max_kps * sizeof(DSPC_ORB_KEYPOINT_S) */
    FH_UINT32 u32DescPhyAddr;        /* max_kps * DSPC_ORB_DESC_SIZE */
    FH_UINT32 u32NKps;               /* out: actual count, <= u32MaxKps */
} DSPC_ORB_CMD_S;

/* Persistent per-instance buffers. Allocate once, extract per frame. */
typedef struct {
    dspc_buf_t in;      /* grayscale image, max_w * max_h */
    dspc_buf_t kp;      /* DSPC_ORB_KEYPOINT_S[max_kps] */
    dspc_buf_t desc;    /* uint8[max_kps][DSPC_ORB_DESC_SIZE] */
    dspc_buf_t cmd;     /* DSPC_ORB_CMD_S */
    FH_UINT32  max_w, max_h, max_kps;
} dspc_orb_t;

/* Allocate the four MMZ buffers for the given worst-case frame size and
 * keypoint capacity. Returns 0 on success. Call dspc_init() first. */
int  dspc_orb_alloc(dspc_orb_t *o, FH_UINT32 max_w, FH_UINT32 max_h,
                    FH_UINT32 max_kps);
void dspc_orb_free(dspc_orb_t *o);

/* Extract ORB features from one grayscale frame (tight rows).
 *   gray      : CPU pointer, w*h bytes, copied into the input MMZ buffer
 *   params    : ORB-SLAM semantics (app uses 1000, 1.2f, 8, 20, 7)
 *   out_kps   : caller buffer, capacity max_kps keypoints
 *   out_desc  : caller buffer, capacity max_kps * 32 bytes (row i = kp i)
 *   out_n     : number of keypoints/descriptor rows written
 * Blocking; returns 0 on success. */
int dspc_orb_extract(dspc_orb_t *o,
                     const FH_UINT8 *gray, FH_UINT32 w, FH_UINT32 h,
                     int nfeatures, float scaleFactor, int nlevels,
                     int iniThFAST, int minThFAST,
                     DSPC_ORB_KEYPOINT_S *out_kps, FH_UINT8 *out_desc,
                     FH_UINT32 *out_n);

#ifdef __cplusplus
}
#endif

#endif /* DSPCALLING_ORB_H */
