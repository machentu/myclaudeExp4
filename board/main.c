#define _POSIX_C_SOURCE 199309L

#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// #include "camera_capture.h"
// #include "http_stream.h"

// #define DEFAULT_STREAM_WIDTH 960
// #define DEFAULT_STREAM_HEIGHT 512
// #define DEFAULT_HTTP_PORT 1111
// #define OVOS02K_ISP_PARAM "hex/ovos02k_mipi_attr.hex"
// #define OVOS04C10_ISP_PARAM "hex/ovos04c10_mipi_attr.hex"

// static volatile sig_atomic_t g_stop;
// static volatile sig_atomic_t g_jpeg_count;

// static void on_signal(int signal_number) {
//   (void)signal_number;
//   g_stop = 1;
// }

// static int parse_int(const char *text, int min_value, int max_value, int *out) {
//   char *end = NULL;
//   long value;

//   if (!text || !out) {
//     return -1;
//   }
//   errno = 0;
//   value = strtol(text, &end, 10);
//   if (errno != 0 || text[0] == '\0' || !end || end[0] != '\0' ||
//       value < min_value || value > max_value) {
//     return -1;
//   }
//   *out = (int)value;
//   return 0;
// }

// static void print_usage(const char *program) {
//   printf("usage: %s [options]\n"
//          "  --sensor NAME       ovos02k|ovos04c10 (required)\n"
//          "  --stream-width N    HTTP JPEG width (default: %d)\n"
//          "  --stream-height N   HTTP JPEG height (default: %d)\n"
//          "  --http-port N       HTTP port (default: %d)\n"
//          "  --frames N          stop after N JPEG frames (default: 0)\n"
//          "  --isp-param PATH    ISP parameter file (selected by sensor)\n",
//          program, DEFAULT_STREAM_WIDTH, DEFAULT_STREAM_HEIGHT,
//          DEFAULT_HTTP_PORT);
// }

// static void publish_jpeg(const void *data, uint32_t size, void *user) {
//   (void)user;
//   http_stream_publish_jpeg(data, size);
//   ++g_jpeg_count;
//   if (g_jpeg_count == 1 || g_jpeg_count % 300 == 0) {
//     printf("encoded JPEG frame=%d size=%u\n", (int)g_jpeg_count,
//            (unsigned)size);
//   }
// }

// int main(int argc, char **argv) {
//   camera_capture *camera = NULL;
//   camera_config config;
//   int http_port = DEFAULT_HTTP_PORT;
//   int frame_limit = 0;
//   int isp_param_overridden = 0;
//   int result = 1;
//   int i;

//   memset(&config, 0, sizeof(config));
//   config.stream_width = DEFAULT_STREAM_WIDTH;
//   config.stream_height = DEFAULT_STREAM_HEIGHT;
//   config.on_jpeg = publish_jpeg;

//   for (i = 1; i < argc; ++i) {
//     if (strcmp(argv[i], "--sensor") == 0 && i + 1 < argc) {
//       config.sensor = argv[++i];
//       if (strcmp(config.sensor, "ovos02k") != 0 &&
//           strcmp(config.sensor, "ovos04c10") != 0) {
//         goto invalid_args;
//       }
//     } else if (strcmp(argv[i], "--stream-width") == 0 && i + 1 < argc) {
//       if (parse_int(argv[++i], 2, 4096, &config.stream_width) != 0) {
//         goto invalid_args;
//       }
//     } else if (strcmp(argv[i], "--stream-height") == 0 && i + 1 < argc) {
//       if (parse_int(argv[++i], 2, 4096, &config.stream_height) != 0) {
//         goto invalid_args;
//       }
//     } else if (strcmp(argv[i], "--http-port") == 0 && i + 1 < argc) {
//       if (parse_int(argv[++i], 1, 65535, &http_port) != 0) {
//         goto invalid_args;
//       }
//     } else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
//       if (parse_int(argv[++i], 0, 1000000, &frame_limit) != 0) {
//         goto invalid_args;
//       }
//     } else if (strcmp(argv[i], "--isp-param") == 0 && i + 1 < argc) {
//       config.isp_param_path = argv[++i];
//       isp_param_overridden = 1;
//     } else if (strcmp(argv[i], "--help") == 0 ||
//                strcmp(argv[i], "-h") == 0) {
//       print_usage(argv[0]);
//       return 0;
//     } else {
//       goto invalid_args;
//     }
//   }

//   if (!config.sensor) {
//     goto invalid_args;
//   }
//   if (!isp_param_overridden) {
//     config.isp_param_path = strcmp(config.sensor, "ovos02k") == 0
//                                 ? OVOS02K_ISP_PARAM
//                                 : OVOS04C10_ISP_PARAM;
//   }
//   if ((config.stream_width & 1) || (config.stream_height & 1)) {
//     fprintf(stderr, "stream width and height must be even\n");
//     return 2;
//   }

//   signal(SIGINT, on_signal);
//   signal(SIGTERM, on_signal);

//   if (http_stream_start((uint16_t)http_port) != 0) {
//     fprintf(stderr, "failed to start HTTP server on port %d\n", http_port);
//     goto done;
//   }
//   if (camera_capture_open(&camera, &config) != 0) {
//     fprintf(stderr, "failed to open camera pipeline\n");
//     goto done;
//   }

//   printf("camera ready: sensor=%s JPEG=%dx%d\n", config.sensor,
//          config.stream_width, config.stream_height);
//   printf("open http://<board-ip>:%d/\n", http_port);

//   while (!g_stop && (frame_limit == 0 || g_jpeg_count < frame_limit)) {
//     struct timespec delay = {0, 20 * 1000 * 1000};
//     nanosleep(&delay, NULL);
//   }
//   result = g_jpeg_count > 0 ? 0 : 1;
//   goto done;

// invalid_args:
//   print_usage(argv[0]);
//   return 2;

// done:
//   camera_capture_close(camera);
//   http_stream_stop();
//   return result;
// }


#include "ngd_app/maintest.h"


int main(int argc, char **argv) 
{

  maintest(argc, argv);
}