#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int http_stream_start(uint16_t port);
void http_stream_publish_jpeg(const void *data, uint32_t size);
void http_stream_stop(void);

#ifdef __cplusplus
}
#endif
