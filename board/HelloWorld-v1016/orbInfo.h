#ifndef ORB_INFO_DESCRIPTION_HEADER
#define ORB_INFO_DESCRIPTION_HEADER


/* One keypoint — ngd_keypoint verbatim (field-for-field). */
typedef struct {
    float  x, y;       /* pixel coords, level-0 frame */
    float  angle;      /* orientation, degrees [0,360) */
    float  response;   /* FAST corner score */
    int octave;     /* pyramid level */
    float  size;       /* PATCH_SIZE * scaleFactor^octave */
} DSPC_ORB_KEYPOINT_S;

/* The command block the CPU fills (via vir) and the DSP reads (via phy).
 * Every buffer reference is a physical address of an MMZ allocation. */
typedef struct {
    /* input grayscale image, tight rows (stride == width) */
	int u32Width;
    int u32Height;
    int u32Stride;
    int u32GrayPhyAddr;        /* width * height bytes */

    /* extractor parameters (ngd_orb_init semantics) */
    int s32NFeatures;
    float  fScaleFactor;
    int s32NLevels;
    int s32IniThFAST;
    int s32MinThFAST;

    /* output buffers + capacity; DSP writes u32NKps on completion */
    int u32MaxKps;
    int u32KpPhyAddr;          /* max_kps * sizeof(DSPC_ORB_KEYPOINT_S) */
    int u32DescPhyAddr;        /* max_kps * DSPC_ORB_DESC_SIZE */
    int u32NKps;               /* out: actual count, <= u32MaxKps */
} DSPC_ORB_CMD_S;

/* One MMZ allocation: the DSP dereferences `phy`, the CPU dereferences
 * `vir` — both point at the same bytes. */
typedef struct {
    int phy;
    uint8_t *vir;
    int size;
} dspc_buf_t;

/* Persistent per-instance buffers. Allocate once, extract per frame. */
typedef struct {
    dspc_buf_t in;      /* grayscale image, max_w * max_h */
    dspc_buf_t kp;      /* DSPC_ORB_KEYPOINT_S[max_kps] */
    dspc_buf_t desc;    /* uint8[max_kps][DSPC_ORB_DESC_SIZE] */
    dspc_buf_t cmd;     /* DSPC_ORB_CMD_S */
    int  max_w, max_h, max_kps;
} dspc_orb_t;


#endif
