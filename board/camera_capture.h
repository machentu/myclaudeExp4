#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct camera_capture camera_capture;

typedef struct camera_config {
  const char *sensor;
  int stream_width;
  int stream_height;
  const char *isp_param_path;
  void (*on_jpeg)(const void *data, uint32_t size, void *user);
  void *on_jpeg_user;
} camera_config;

int camera_capture_open(camera_capture **out, const camera_config *config);
void camera_capture_close(camera_capture *camera);

#ifdef __cplusplus
}
#endif
