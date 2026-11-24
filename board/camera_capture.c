#define _POSIX_C_SOURCE 199309L

#include "camera_capture.h"

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <time.h>

#ifdef __cplusplus
#define private private_
#endif
#include "dsp/fh_jpege_mpi.h"
#include "dsp/fh_system_mpi.h"
#include "dsp/fh_vb_mpi.h"
#include "dsp/fh_vpu_mpi.h"
#include "isp/isp_api.h"
#include "isp/isp_common.h"
#include "isp/isp_enum.h"
#include "isp/isp_sensor_if.h"
#include "types/vmm_api.h"
#include "vicap/fh_vicap_mpi.h"
#ifdef __cplusplus
#undef private
#endif

#define ALIGN_UP(value, alignment) \
  (((value) + (alignment)-1) & (~((alignment)-1)))

#define MIPI_LANE_COUNT 2
#define SENSOR_CSI_DEVICE VICAP_PIPELINE_MIPI1
#define SENSOR_I2C_DEVICE 2
#define ISP_PIPE 0
#define VPSS_GROUP 0
#define STREAM_VPSS_CHANNEL 0
#define JPEGE_CHANNEL 0
#define VPSS_OUTPUT_FORMAT VPU_VOMODE_SCAN

#define LOG(fmt, ...) printf("[camera] " fmt "\n", ##__VA_ARGS__)

#define SENSOR_CREATE(name) Sensor_Create##_##name
extern struct isp_sensor_if *SENSOR_CREATE(ovos02k_mipi)(void);
extern struct isp_sensor_if *SENSOR_CREATE(ovos04c10_mipi)(void);
FH_SINT32 fh_vpu_init(FH_VOID);
FH_SINT32 fh_vpu_close(FH_VOID);

typedef struct sensor_profile {
  const char *name;
  int width;
  int height;
  int format;
  struct isp_sensor_if *(*create)(void);
} sensor_profile;

static const sensor_profile k_sensor_profiles[] = {
    {"ovos02k", 1920, 1080, FORMAT_1080P25,
     SENSOR_CREATE(ovos02k_mipi)},
    {"ovos04c10", 2688, 1520, FORMAT_400WP30,
     SENSOR_CREATE(ovos04c10_mipi)},
};

struct camera_capture {
  camera_config config;
  const sensor_profile *sensor;
  int sys_ready;
  int vb_ready;
  int vicap_ready;
  int isp_ready;
  int vpu_ready;
  int vpu_group_ready;
  int stream_channel_ready;
  int isp_bound;
  int jpege_created;
  int jpege_started;
  int jpege_bound;
  volatile int isp_running;
  volatile int jpege_running;
  pthread_t isp_thread;
  pthread_t jpege_thread;
};

static const sensor_profile *find_sensor_profile(const char *name) {
  size_t i;

  if (!name) {
    return NULL;
  }
  for (i = 0; i < sizeof(k_sensor_profiles) / sizeof(k_sensor_profiles[0]);
       ++i) {
    if (strcmp(name, k_sensor_profiles[i].name) == 0) {
      return &k_sensor_profiles[i];
    }
  }
  return NULL;
}

static int check_result(int result, const char *operation) {
  if (result != FH_SUCCESS) {
    LOG("%s failed: 0x%x", operation, result);
    return -EIO;
  }
  return 0;
}

static void reset_media_system(void) {
  int i;

  FH_SYS_Exit();
  for (i = 0; i < VB_MAX_USER; ++i) {
    FH_VB_ExitModCommPool((VB_UID_E)i);
  }
  for (i = 0; i < VB_MAX_POOLS; ++i) {
    FH_VB_DestroyPool((VB_POOL)i);
  }
  FH_VB_Exit();
}

static int init_system(camera_capture *camera) {
  VB_CONF_S vb_config;
  uint32_t stream_block;
  int result;

  reset_media_system();
  memset(&vb_config, 0, sizeof(vb_config));
  stream_block = (uint32_t)(ALIGN_UP(camera->config.stream_width, 32) *
                            ALIGN_UP(camera->config.stream_height, 32) * 2);
  vb_config.astCommPool[0].u32BlkSize = stream_block;
  vb_config.astCommPool[0].u32BlkCnt = 3;
  vb_config.u32MaxPoolCnt = 1;

  result = FH_VB_SetConf(&vb_config);
  if (check_result(result, "FH_VB_SetConf") != 0) {
    return -EIO;
  }
  result = FH_VB_Init();
  if (check_result(result, "FH_VB_Init") != 0) {
    return -EIO;
  }
  camera->vb_ready = 1;

  result = FH_SYS_Init();
  if (check_result(result, "FH_SYS_Init") != 0) {
    return -EIO;
  }
  camera->sys_ready = 1;
  return 0;
}

static void configure_sensor_registers(void) {
  FH_UINT32 value;

  FH_SYS_SetReg(0x10000954, 0xF);
  FH_SYS_SetReg(0x10000188, 0x09);
  FH_SYS_SetReg(0x2d000094, 0x01);
  FH_SYS_SetReg(0x10200014, 0x00);
  FH_SYS_SetReg(0x10200018, 0x00);
  FH_SYS_SetReg(0x1020000C, 0x00);
  FH_SYS_SetReg(0x10200010, 0x00);
  FH_SYS_SetReg(0x10200024, 0x00);
  FH_SYS_SetReg(0x10200028, 0x00);
  FH_SYS_SetReg(0x1020001C, 0x00);
  FH_SYS_SetReg(0x10200020, 0x00);
  FH_SYS_SetReg(0x102001b8, 0x02);
  FH_SYS_SetReg(0x102001bc, 0x02);
  FH_SYS_SetReg(0x102001C4, 0x07);
  FH_SYS_SetReg(0x102001C8, 0x07);

  FH_SYS_GetReg(0x10000204, &value);
  FH_SYS_SetReg(0x10000204, value | 0x7);
  FH_SYS_GetReg(0x20100000, &value);
  FH_SYS_SetReg(0x20100000, value | 0x4000);
  FH_SYS_SetReg(0x2c100094, 0x222);

  FH_SYS_GetReg(0x10300260, &value);
  FH_SYS_SetReg(0x10300260, value & 0xfffff0f8);
  FH_SYS_GetReg(0x10300264, &value);
  FH_SYS_SetReg(0x10300264, value & 0xfffff0f8);
  FH_SYS_GetReg(0x10300268, &value);
  FH_SYS_SetReg(0x10300268, value & 0xfffff0f8);

  FH_SYS_GetReg(0x1010033C, &value);
  FH_SYS_SetReg(0x1010033C, value & (~0x4));
  FH_SYS_GetReg(0x10100014, &value);
  FH_SYS_SetReg(0x10100014, value | 0x4);
  FH_SYS_GetReg(0x1C0001BC, &value);
  FH_SYS_SetReg(0x1C0001BC, (value & (~0x4)) | 0x8);

  FH_SYS_SetReg(0x2c100094, 0x222);
  FH_SYS_GetReg(0x1010033C, &value);
  FH_SYS_SetReg(0x1010033C, value & (~0x4));
  FH_SYS_GetReg(0x10100014, &value);
  FH_SYS_SetReg(0x10100014, value | 0x4);
  FH_SYS_GetReg(0x1C0001BC, &value);
  FH_SYS_SetReg(0x1C0001BC, (value & (~0x4)) | 0x8);
}

static int init_vicap(camera_capture *camera) {
  FH_VICAP_DEV_ATTR_S device_attr;
  FH_VICAP_CROP_CFG_S crop;
  FH_VICAP_VI_ATTR_S vi_attr;
  FH_VICAP_PEAKLOAD_INFO_S peak_load;
  int result;

  memset(&device_attr, 0, sizeof(device_attr));
  memset(&crop, 0, sizeof(crop));
  memset(&vi_attr, 0, sizeof(vi_attr));
  memset(&peak_load, 0, sizeof(peak_load));

  result = FH_VICAP_Set_Phy_Lane(MIPI_LANE_COUNT);
  if (check_result(result, "FH_VICAP_Set_Phy_Lane") != 0) {
    return -EIO;
  }
  peak_load.sizeFpsInfo[0].width = (FH_UINT32)camera->sensor->width;
  peak_load.sizeFpsInfo[0].height = (FH_UINT32)camera->sensor->height;
  peak_load.sizeFpsInfo[0].fps = 30;
  peak_load.sizeFpsInfo[0].pclk = 600000000;
  peak_load.pclkSet = 0;
  result = FH_VICAP_SetPeakLoad(&peak_load);
  if (check_result(result, "FH_VICAP_SetPeakLoad") != 0) {
    return -EIO;
  }

  device_attr.bUsingVb = FH_FALSE;
  device_attr.enDataTypeIn = VICAP_DATA_IN_RAW12;
  device_attr.enWorkMode = VICAP_WORK_MODE_ONLINE;
  device_attr.stSize.u16Width = (FH_UINT16)camera->sensor->width;
  device_attr.stSize.u16Height = (FH_UINT16)camera->sensor->height;
  result = FH_VICAP_InitViDev(SENSOR_CSI_DEVICE, &device_attr);
  if (check_result(result, "FH_VICAP_InitViDev") != 0) {
    return -EIO;
  }
  camera->vicap_ready = 1;

  crop.bCutEnable = FH_FALSE;
  result = FH_VICAP_SetPipeCrop(SENSOR_CSI_DEVICE, VICAP_LFRAME, &crop);
  if (check_result(result, "FH_VICAP_SetPipeCrop") != 0) {
    return -EIO;
  }

  vi_attr.enBayerType = VICAP_BAYER_RGGB;
  vi_attr.enWorkMode = VICAP_WORK_MODE_ONLINE;
  vi_attr.u8WdrMode = 0;
  vi_attr.stInSize.u16Width = (FH_UINT16)camera->sensor->width;
  vi_attr.stInSize.u16Height = (FH_UINT16)camera->sensor->height;
  vi_attr.stOfflineCfg.u8Priority = 0;
  vi_attr.stCropSize.bCutEnable = FH_FALSE;
  result = FH_VICAP_SetViAttr(SENSOR_CSI_DEVICE, &vi_attr);
  if (check_result(result, "FH_VICAP_SetViAttr") != 0) {
    return -EIO;
  }
  return 0;
}

static int load_isp_parameters(const char *path) {
  ISP_PARAM_CONFIG parameters;
  FH_UINT32 phy_addr = 0;
  FH_VOID *vir_addr = NULL;
  FILE *file;
  size_t bytes_read;
  int result;

  memset(&parameters, 0, sizeof(parameters));
  API_ISP_GetBinAddr(ISP_PIPE, &parameters);
  file = fopen(path, "rb");
  if (!file) {
    LOG("open ISP parameter failed: %s", path);
    return -errno;
  }

  result = FH_SYS_VmmAlloc(&phy_addr, &vir_addr, "ispParam", "anonymous",
                           (FH_UINT32)(sizeof(unsigned int) *
                                       parameters.u32BinSize));
  if (result != FH_SUCCESS) {
    fclose(file);
    return -EIO;
  }
  memset(vir_addr, 0, sizeof(unsigned int) * parameters.u32BinSize);
  bytes_read = fread(vir_addr, 1, parameters.u32BinSize, file);
  fclose(file);
  if (bytes_read == 0) {
    FH_SYS_VmmFree(phy_addr);
    return -EIO;
  }

  result = API_ISP_LoadIspParam(ISP_PIPE, (char *)vir_addr);
  FH_SYS_VmmFree(phy_addr);
  if (check_result(result, "API_ISP_LoadIspParam") != 0) {
    return -EIO;
  }
  LOG("loaded ISP parameter: %s", path);
  return 0;
}

static void *isp_thread_main(void *argument) {
  camera_capture *camera = (camera_capture *)argument;
  struct timespec delay = {0, 10 * 1000 * 1000};

  prctl(PR_SET_NAME, "camera_isp");
  while (camera->isp_running) {
    API_ISP_Run(ISP_PIPE);
    nanosleep(&delay, NULL);
  }
  return NULL;
}

static int init_isp(camera_capture *camera) {
  ISP_MEM_INIT memory;
  Sensor_Init_t sensor_init;
  ISP_VI_ATTR_S vi_attr;
  struct isp_sensor_if *sensor;
  int result;

  memset(&memory, 0, sizeof(memory));
  memset(&sensor_init, 0, sizeof(sensor_init));
  memset(&vi_attr, 0, sizeof(vi_attr));

  memory.enOfflineWorkMode = ISP_OFFLINE_MODE_DISABLE;
  memory.enIspOutMode = ISP_OUT_TO_VPU;
  memory.stPicConf.u32Width = (FH_UINT32)camera->sensor->width;
  memory.stPicConf.u32Height = (FH_UINT32)camera->sensor->height;
  memory.enLut2dWorkMode = ISP_LUT2D_BYPASS;
  result = API_ISP_MemInit(ISP_PIPE, &memory);
  if (check_result(result, "API_ISP_MemInit") != 0) {
    API_ISP_Exit(ISP_PIPE);
    return -EIO;
  }

  sensor = camera->sensor->create();
  if (!sensor) {
    API_ISP_Exit(ISP_PIPE);
    LOG("create %s sensor failed", camera->sensor->name);
    return -EIO;
  }
  result = API_ISP_SensorRegCb(ISP_PIPE, 0, sensor);
  if (check_result(result, "API_ISP_SensorRegCb") != 0) {
    API_ISP_Exit(ISP_PIPE);
    return -EIO;
  }

  sensor_init.u8CsiDeviceId = SENSOR_CSI_DEVICE;
  sensor_init.u8CciDeviceId = SENSOR_I2C_DEVICE;
  sensor_init.bGrpSync = FH_FALSE;
  result = API_ISP_SensorInit(ISP_PIPE, &sensor_init);
  if (check_result(result, "API_ISP_SensorInit") != 0) {
    API_ISP_Exit(ISP_PIPE);
    return -EIO;
  }
  result = API_ISP_SetSensorFmt(ISP_PIPE, camera->sensor->format);
  if (check_result(result, "API_ISP_SetSensorFmt") != 0) {
    API_ISP_Exit(ISP_PIPE);
    return -EIO;
  }

  vi_attr.u16InputWidth = (FH_UINT16)camera->sensor->width;
  vi_attr.u16InputHeight = (FH_UINT16)camera->sensor->height;
  vi_attr.u16PicWidth = (FH_UINT16)camera->sensor->width;
  vi_attr.u16PicHeight = (FH_UINT16)camera->sensor->height;
  vi_attr.u16OffsetX = 0;
  vi_attr.u16OffsetY = 0;
  vi_attr.enBayerType = BAYER_BGGR;
  result = API_ISP_SetViAttr(ISP_PIPE, &vi_attr);
  if (check_result(result, "API_ISP_SetViAttr") != 0) {
    API_ISP_Exit(ISP_PIPE);
    return -EIO;
  }
  result = API_ISP_SensorKick(ISP_PIPE);
  if (check_result(result, "API_ISP_SensorKick") != 0) {
    API_ISP_Exit(ISP_PIPE);
    return -EIO;
  }
  result = API_ISP_Init(ISP_PIPE);
  if (check_result(result, "API_ISP_Init") != 0) {
    return -EIO;
  }
  camera->isp_ready = 1;

  result = load_isp_parameters(camera->config.isp_param_path);
  if (result != 0) {
    return result;
  }

  camera->isp_running = 1;
  result = pthread_create(&camera->isp_thread, NULL, isp_thread_main, camera);
  if (result != 0) {
    camera->isp_running = 0;
    return -result;
  }
  return 0;
}

static int create_vpss_channel(int channel, int width, int height,
                               int buffer_count, const char *label) {
  FH_VPU_CHN_INFO channel_info;
  FH_VPU_CHN_CONFIG channel_config;
  int result;

  memset(&channel_info, 0, sizeof(channel_info));
  channel_info.bgm_enable = FH_FALSE;
  channel_info.cpy_enable = FH_FALSE;
  channel_info.sad_enable = FH_FALSE;
  channel_info.out_mode = VPSS_OUTPUT_FORMAT;
  channel_info.support_mode = 1 << VPSS_OUTPUT_FORMAT;
  channel_info.bufnum = (FH_UINT32)buffer_count;
  channel_info.chn_max_size.u32Width = (FH_UINT32)width;
  channel_info.chn_max_size.u32Height = (FH_UINT32)height;
  channel_info.max_stride = (FH_UINT32)width;
  result = FH_VPSS_CreateChn(VPSS_GROUP, channel, &channel_info);
  if (result != FH_SUCCESS) {
    LOG("FH_VPSS_CreateChn %s failed: 0x%x", label, result);
    return -EIO;
  }

  memset(&channel_config, 0, sizeof(channel_config));
  channel_config.vpu_chn_size.u32Width = (FH_UINT32)width;
  channel_config.vpu_chn_size.u32Height = (FH_UINT32)height;
  channel_config.stride = (FH_UINT32)width;
  channel_config.offset = 0;
  channel_config.depth = 1;
  channel_config.crop_area.crop_en = 0;
  result = FH_VPSS_SetChnAttr(VPSS_GROUP, channel, &channel_config);
  if (result != FH_SUCCESS) {
    FH_VPSS_DestroyChn(VPSS_GROUP, channel);
    LOG("FH_VPSS_SetChnAttr %s failed: 0x%x", label, result);
    return -EIO;
  }
  result = FH_VPSS_SetVOMode(VPSS_GROUP, channel,
                             (FH_VPU_VO_MODE)VPSS_OUTPUT_FORMAT);
  if (result != FH_SUCCESS) {
    FH_VPSS_DestroyChn(VPSS_GROUP, channel);
    LOG("FH_VPSS_SetVOMode %s failed: 0x%x", label, result);
    return -EIO;
  }
  result = FH_VPSS_OpenChn(VPSS_GROUP, channel);
  if (result != FH_SUCCESS) {
    FH_VPSS_DestroyChn(VPSS_GROUP, channel);
    LOG("FH_VPSS_OpenChn %s failed: 0x%x", label, result);
    return -EIO;
  }
  return 0;
}

static int init_vpss(camera_capture *camera) {
  FH_VPU_SET_GRP_INFO group_info;
  FH_VPU_SIZE input_size;
  int result;

  result = fh_vpu_init();
  if (check_result(result, "fh_vpu_init") != 0) {
    return -EIO;
  }
  camera->vpu_ready = 1;

  memset(&group_info, 0, sizeof(group_info));
  group_info.vi_max_size.u32Width =
      (FH_UINT32)ALIGN_UP(camera->sensor->width, 16);
  group_info.vi_max_size.u32Height =
      (FH_UINT32)ALIGN_UP(camera->sensor->height, 16);
  group_info.ycmean_en = FH_FALSE;
  group_info.ycmean_ds = 16;
  result = FH_VPSS_CreateGrp(VPSS_GROUP, &group_info);
  if (check_result(result, "FH_VPSS_CreateGrp") != 0) {
    return -EIO;
  }
  camera->vpu_group_ready = 1;

  memset(&input_size, 0, sizeof(input_size));
  input_size.vi_size.u32Width = (FH_UINT32)camera->sensor->width;
  input_size.vi_size.u32Height = (FH_UINT32)camera->sensor->height;
  input_size.crop_area.crop_en = 0;
  result = FH_VPSS_SetViAttr(VPSS_GROUP, &input_size);
  if (check_result(result, "FH_VPSS_SetViAttr") != 0) {
    return -EIO;
  }
  result = FH_VPSS_Enable(VPSS_GROUP, VPU_MODE_ISP);
  if (check_result(result, "FH_VPSS_Enable") != 0) {
    return -EIO;
  }

  result = create_vpss_channel(
      STREAM_VPSS_CHANNEL, camera->config.stream_width,
      camera->config.stream_height, 0, "stream");
  if (result != 0) {
    return result;
  }
  camera->stream_channel_ready = 1;
  return 0;
}

static int bind_isp_to_vpss(camera_capture *camera) {
  FH_BIND_INFO source;
  FH_BIND_INFO destination;
  int result;

  memset(&source, 0, sizeof(source));
  memset(&destination, 0, sizeof(destination));
  source.obj_id = FH_OBJ_ISP;
  source.dev_id = ISP_PIPE;
  source.chn_id = 0;
  destination.obj_id = FH_OBJ_VPU_VI;
  destination.dev_id = VPSS_GROUP;
  destination.chn_id = 0;
  result = FH_SYS_Bind(source, destination);
  if (check_result(result, "FH_SYS_Bind ISP->VPSS") != 0) {
    return -EIO;
  }
  camera->isp_bound = 1;
  return 0;
}

static void *jpege_thread_main(void *argument) {
  camera_capture *camera = (camera_capture *)argument;

  prctl(PR_SET_NAME, "camera_jpege");
  while (camera->jpege_running) {
    JPEGE_STREAM_S stream;
    int result;

    memset(&stream, 0, sizeof(stream));
    stream.s32MilliSec = 1000;
    result = FH_JPEGE_GetStream(JPEGE_CHANNEL, &stream);
    if (result == FH_SUCCESS) {
      if (camera->config.on_jpeg) {
        camera->config.on_jpeg(stream.pu8Addr, stream.u32Len,
                               camera->config.on_jpeg_user);
      }
      result = FH_JPEGE_ReleaseStream(JPEGE_CHANNEL, &stream);
      if (result != FH_SUCCESS) {
        LOG("FH_JPEGE_ReleaseStream failed: 0x%x", result);
      }
    } else {
      struct timespec delay = {0, 1000 * 1000};
      nanosleep(&delay, NULL);
    }
  }
  return NULL;
}

static int init_jpege(camera_capture *camera) {
  JPEGE_CHN_ATTR_S attributes;
  FH_BIND_INFO source;
  FH_BIND_INFO destination;
  int result;

  memset(&attributes, 0, sizeof(attributes));
  attributes.u32MaxPicWidth = (FH_UINT32)camera->config.stream_width;
  attributes.u32MaxPicHeight = (FH_UINT32)camera->config.stream_height;
  attributes.u32PicWidth = (FH_UINT32)camera->config.stream_width;
  attributes.u32PicHeight = (FH_UINT32)camera->config.stream_height;
  attributes.u32BufSize =
      (FH_UINT32)(camera->config.stream_width * camera->config.stream_height);
  result = FH_JPEGE_CreateChn(JPEGE_CHANNEL, &attributes);
  if (check_result(result, "FH_JPEGE_CreateChn") != 0) {
    return -EIO;
  }
  camera->jpege_created = 1;

  result = FH_JPEGE_StartRecvPic(JPEGE_CHANNEL);
  if (check_result(result, "FH_JPEGE_StartRecvPic") != 0) {
    return -EIO;
  }
  camera->jpege_started = 1;

  memset(&source, 0, sizeof(source));
  memset(&destination, 0, sizeof(destination));
  source.obj_id = FH_OBJ_VPU_VO;
  source.dev_id = VPSS_GROUP;
  source.chn_id = STREAM_VPSS_CHANNEL;
  destination.obj_id = FH_OBJ_JPEG;
  destination.dev_id = 0;
  destination.chn_id = JPEGE_CHANNEL;
  result = FH_SYS_Bind(source, destination);
  if (check_result(result, "FH_SYS_Bind VPSS->JPEGE") != 0) {
    return -EIO;
  }
  camera->jpege_bound = 1;

  return 0;
}

static int start_jpege_thread(camera_capture *camera) {
  int result;

  camera->jpege_running = 1;
  result =
      pthread_create(&camera->jpege_thread, NULL, jpege_thread_main, camera);
  if (result != 0) {
    camera->jpege_running = 0;
    return -result;
  }
  return 0;
}

static void deinit_jpege(camera_capture *camera) {
  FH_BIND_INFO source;
  FH_BIND_INFO destination;

  if (camera->jpege_running) {
    camera->jpege_running = 0;
    pthread_join(camera->jpege_thread, NULL);
    camera->jpege_thread = 0;
  }
  if (camera->jpege_bound) {
    memset(&source, 0, sizeof(source));
    memset(&destination, 0, sizeof(destination));
    source.obj_id = FH_OBJ_VPU_VO;
    source.dev_id = VPSS_GROUP;
    source.chn_id = STREAM_VPSS_CHANNEL;
    destination.obj_id = FH_OBJ_JPEG;
    destination.dev_id = 0;
    destination.chn_id = JPEGE_CHANNEL;
    (void)FH_SYS_UnBind(source, destination);
    camera->jpege_bound = 0;
  }
  if (camera->jpege_started) {
    (void)FH_JPEGE_StopRecvPic(JPEGE_CHANNEL);
    camera->jpege_started = 0;
  }
  if (camera->jpege_created) {
    (void)FH_JPEGE_DestroyChn(JPEGE_CHANNEL);
    camera->jpege_created = 0;
  }
}

int camera_capture_open(camera_capture **out, const camera_config *config) {
  camera_capture *camera;
  int result;

  if (!out || !config || !config->sensor || config->stream_width <= 0 ||
      config->stream_height <= 0 || config->stream_width > 8192 ||
      config->stream_height > 8192 || (config->stream_width & 1) ||
      (config->stream_height & 1) || !config->isp_param_path) {
    return -EINVAL;
  }
  *out = NULL;
  camera = (camera_capture *)calloc(1, sizeof(*camera));
  if (!camera) {
    return -ENOMEM;
  }
  camera->config = *config;
  camera->sensor = find_sensor_profile(config->sensor);
  if (!camera->sensor) {
    free(camera);
    return -EINVAL;
  }
  result = init_system(camera);
  if (result == 0) {
    configure_sensor_registers();
    result = init_vicap(camera);
  }
  if (result == 0) {
    result = init_isp(camera);
  }
  if (result == 0) {
    result = init_vpss(camera);
  }
  if (result == 0) {
    result = init_jpege(camera);
  }
  if (result == 0) {
    result = bind_isp_to_vpss(camera);
  }
  if (result == 0) {
    result = start_jpege_thread(camera);
  }
  if (result != 0) {
    camera_capture_close(camera);
    return result;
  }

  *out = camera;
  LOG("sensor=%s input=%dx%d", camera->sensor->name, camera->sensor->width,
      camera->sensor->height);
  return 0;
}

void camera_capture_close(camera_capture *camera) {
  FH_BIND_INFO source;
  FH_BIND_INFO destination;
  int i;

  if (!camera) {
    return;
  }
  if (camera->isp_running) {
    camera->isp_running = 0;
    pthread_join(camera->isp_thread, NULL);
    camera->isp_thread = 0;
  }
  deinit_jpege(camera);

  if (camera->isp_bound) {
    memset(&source, 0, sizeof(source));
    memset(&destination, 0, sizeof(destination));
    source.obj_id = FH_OBJ_ISP;
    source.dev_id = ISP_PIPE;
    source.chn_id = 0;
    destination.obj_id = FH_OBJ_VPU_VI;
    destination.dev_id = VPSS_GROUP;
    destination.chn_id = 0;
    (void)FH_SYS_UnBind(source, destination);
    camera->isp_bound = 0;
  }
  if (camera->stream_channel_ready) {
    (void)FH_VPSS_CloseChn(VPSS_GROUP, STREAM_VPSS_CHANNEL);
    (void)FH_VPSS_DestroyChn(VPSS_GROUP, STREAM_VPSS_CHANNEL);
    camera->stream_channel_ready = 0;
  }
  if (camera->vpu_group_ready) {
    (void)FH_VPSS_DestroyGrp(VPSS_GROUP);
    camera->vpu_group_ready = 0;
  }
  if (camera->vpu_ready) {
    (void)fh_vpu_close();
    camera->vpu_ready = 0;
  }
  if (camera->vicap_ready) {
    (void)FH_VICAP_Exit(SENSOR_CSI_DEVICE);
    camera->vicap_ready = 0;
  }
  if (camera->isp_ready) {
    (void)API_ISP_Exit(ISP_PIPE);
    camera->isp_ready = 0;
  }
  if (camera->sys_ready) {
    (void)FH_SYS_Exit();
    camera->sys_ready = 0;
  }
  if (camera->vb_ready) {
    for (i = 0; i < VB_MAX_USER; ++i) {
      (void)FH_VB_ExitModCommPool((VB_UID_E)i);
    }
    for (i = 0; i < VB_MAX_POOLS; ++i) {
      (void)FH_VB_DestroyPool((VB_POOL)i);
    }
    (void)FH_VB_Exit();
    camera->vb_ready = 0;
  }
  free(camera);
}
