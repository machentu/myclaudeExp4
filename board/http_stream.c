#include "http_stream.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_JPEG_SIZE (16U * 1024U * 1024U)

static const char k_index_page[] =
    "<!doctype html><html><head><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>MC635X Camera</title>"
    "<style>html,body{margin:0;height:100%;background:#111}"
    "body{display:grid;place-items:center}"
    "img{max-width:100vw;max-height:100vh}</style></head>"
    "<body><img src=\"/stream.mjpg\" alt=\"camera preview\"></body></html>";

static const char k_mjpeg_header[] =
    "HTTP/1.0 200 OK\r\n"
    "Connection: close\r\n"
    "Cache-Control: no-store, no-cache, must-revalidate\r\n"
    "Pragma: no-cache\r\n"
    "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";

typedef struct client_context {
  int fd;
} client_context;

typedef struct http_context {
  pthread_mutex_t lock;
  pthread_cond_t frame_ready;
  pthread_cond_t clients_done;
  int running;
  int thread_started;
  int stop_requested;
  int active_clients;
  int server_fd;
  uint16_t port;
  pthread_t thread;
  unsigned char *jpeg;
  uint32_t jpeg_size;
  uint64_t jpeg_sequence;
} http_context;

static http_context g_http = {
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .frame_ready = PTHREAD_COND_INITIALIZER,
    .clients_done = PTHREAD_COND_INITIALIZER,
    .server_fd = -1,
};

static int send_all(int fd, const void *data, size_t size) {
  const unsigned char *bytes = (const unsigned char *)data;
  size_t offset = 0;

  while (offset < size) {
    ssize_t sent = send(fd, bytes + offset, size - offset, MSG_NOSIGNAL);
    if (sent > 0) {
      offset += (size_t)sent;
    } else if (sent < 0 && errno == EINTR) {
      continue;
    } else {
      return -1;
    }
  }
  return 0;
}

static void send_response(int fd, int status, const char *status_text,
                          const char *content_type, const void *body,
                          size_t body_size) {
  char header[512];
  int length;

  length = snprintf(header, sizeof(header),
                    "HTTP/1.0 %d %s\r\n"
                    "Connection: close\r\n"
                    "Cache-Control: no-store, no-cache, must-revalidate\r\n"
                    "Pragma: no-cache\r\n"
                    "Content-Type: %s\r\n"
                    "Content-Length: %u\r\n\r\n",
                    status, status_text, content_type, (unsigned)body_size);
  if (length <= 0 || length >= (int)sizeof(header)) {
    return;
  }
  if (send_all(fd, header, (size_t)length) == 0 && body && body_size > 0) {
    (void)send_all(fd, body, body_size);
  }
}

static int read_path(int fd, char *path, size_t path_size) {
  char request[512];
  char method[16];
  char target[256];
  char *query;
  ssize_t length;

  length = recv(fd, request, sizeof(request) - 1U, 0);
  if (length <= 0) {
    return -1;
  }
  request[length] = '\0';
  if (sscanf(request, "%15s %255s", method, target) != 2 ||
      strcmp(method, "GET") != 0) {
    return -1;
  }
  query = strchr(target, '?');
  if (query) {
    query[0] = '\0';
  }
  snprintf(path, path_size, "%s", target);
  return 0;
}

static void send_latest_jpeg(int fd) {
  unsigned char *copy = NULL;
  uint32_t size = 0;

  pthread_mutex_lock(&g_http.lock);
  if (g_http.jpeg && g_http.jpeg_size > 0) {
    copy = (unsigned char *)malloc(g_http.jpeg_size);
    if (copy) {
      memcpy(copy, g_http.jpeg, g_http.jpeg_size);
      size = g_http.jpeg_size;
    }
  }
  pthread_mutex_unlock(&g_http.lock);

  if (copy) {
    send_response(fd, 200, "OK", "image/jpeg", copy, size);
  } else {
    static const char message[] = "no frame\n";
    send_response(fd, 503, "Service Unavailable", "text/plain", message,
                  sizeof(message) - 1U);
  }
  free(copy);
}

static void send_mjpeg_stream(int fd) {
  uint64_t sequence = 0;

  if (send_all(fd, k_mjpeg_header, sizeof(k_mjpeg_header) - 1U) != 0) {
    return;
  }

  for (;;) {
    unsigned char *copy;
    uint32_t size;
    char part_header[256];
    int header_length;

    pthread_mutex_lock(&g_http.lock);
    while (!g_http.stop_requested &&
           (!g_http.jpeg || g_http.jpeg_sequence == sequence)) {
      pthread_cond_wait(&g_http.frame_ready, &g_http.lock);
    }
    if (g_http.stop_requested) {
      pthread_mutex_unlock(&g_http.lock);
      break;
    }

    size = g_http.jpeg_size;
    copy = (unsigned char *)malloc(size);
    if (copy) {
      memcpy(copy, g_http.jpeg, size);
      sequence = g_http.jpeg_sequence;
    }
    pthread_mutex_unlock(&g_http.lock);
    if (!copy) {
      break;
    }

    header_length = snprintf(part_header, sizeof(part_header),
                             "--frame\r\n"
                             "Content-Type: image/jpeg\r\n"
                             "Content-Length: %u\r\n\r\n",
                             (unsigned)size);
    if (header_length <= 0 || header_length >= (int)sizeof(part_header) ||
        send_all(fd, part_header, (size_t)header_length) != 0 ||
        send_all(fd, copy, size) != 0 ||
        send_all(fd, "\r\n", 2) != 0) {
      free(copy);
      break;
    }
    free(copy);
  }
}

static void *client_thread(void *argument) {
  client_context *client = (client_context *)argument;
  char path[256];

  if (read_path(client->fd, path, sizeof(path)) == 0) {
    if (strcmp(path, "/stream.mjpg") == 0) {
      send_mjpeg_stream(client->fd);
    } else if (strcmp(path, "/frame.jpg") == 0 ||
               strcmp(path, "/snapshot.jpg") == 0) {
      send_latest_jpeg(client->fd);
    } else {
      send_response(client->fd, 200, "OK", "text/html", k_index_page,
                    sizeof(k_index_page) - 1U);
    }
  }

  close(client->fd);
  free(client);

  pthread_mutex_lock(&g_http.lock);
  --g_http.active_clients;
  if (g_http.active_clients == 0) {
    pthread_cond_broadcast(&g_http.clients_done);
  }
  pthread_mutex_unlock(&g_http.lock);
  return NULL;
}

static int create_server(uint16_t port) {
  struct sockaddr_in address;
  int reuse = 1;
  int fd = socket(AF_INET, SOCK_STREAM, 0);

  if (fd < 0) {
    return -1;
  }
  (void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
  memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port);
  if (bind(fd, (struct sockaddr *)&address, sizeof(address)) != 0 ||
      listen(fd, 8) != 0) {
    close(fd);
    return -1;
  }
  return fd;
}

static void *server_thread(void *unused) {
  int server;

  (void)unused;
  server = create_server(g_http.port);
  pthread_mutex_lock(&g_http.lock);
  g_http.server_fd = server;
  if (server < 0) {
    g_http.running = 0;
  }
  pthread_mutex_unlock(&g_http.lock);
  if (server < 0) {
    return NULL;
  }

  for (;;) {
    fd_set read_fds;
    struct timeval timeout;
    int stop_requested;
    int ready;

    pthread_mutex_lock(&g_http.lock);
    stop_requested = g_http.stop_requested;
    pthread_mutex_unlock(&g_http.lock);
    if (stop_requested) {
      break;
    }

    FD_ZERO(&read_fds);
    FD_SET(server, &read_fds);
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;
    ready = select(server + 1, &read_fds, NULL, NULL, &timeout);
    if (ready > 0 && FD_ISSET(server, &read_fds)) {
      int client_fd = accept(server, NULL, NULL);
      if (client_fd >= 0) {
        client_context *client;
        struct timeval io_timeout = {2, 0};
        pthread_t thread;
        int result;

        (void)setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &io_timeout,
                         sizeof(io_timeout));
        (void)setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, &io_timeout,
                         sizeof(io_timeout));

        client = (client_context *)malloc(sizeof(*client));
        if (!client) {
          close(client_fd);
          continue;
        }
        client->fd = client_fd;

        pthread_mutex_lock(&g_http.lock);
        ++g_http.active_clients;
        pthread_mutex_unlock(&g_http.lock);
        result = pthread_create(&thread, NULL, client_thread, client);
        if (result == 0) {
          pthread_detach(thread);
        } else {
          close(client_fd);
          free(client);
          pthread_mutex_lock(&g_http.lock);
          --g_http.active_clients;
          if (g_http.active_clients == 0) {
            pthread_cond_broadcast(&g_http.clients_done);
          }
          pthread_mutex_unlock(&g_http.lock);
        }
      }
    }
  }

  pthread_mutex_lock(&g_http.lock);
  if (g_http.server_fd >= 0) {
    close(g_http.server_fd);
    g_http.server_fd = -1;
  }
  g_http.running = 0;
  pthread_mutex_unlock(&g_http.lock);
  return NULL;
}

int http_stream_start(uint16_t port) {
  int result;

  pthread_mutex_lock(&g_http.lock);
  if (g_http.thread_started) {
    pthread_mutex_unlock(&g_http.lock);
    return -1;
  }
  g_http.port = port;
  g_http.server_fd = -1;
  g_http.stop_requested = 0;
  g_http.running = 1;
  result = pthread_create(&g_http.thread, NULL, server_thread, NULL);
  if (result != 0) {
    g_http.running = 0;
  } else {
    g_http.thread_started = 1;
  }
  pthread_mutex_unlock(&g_http.lock);
  return result == 0 ? 0 : -1;
}

void http_stream_publish_jpeg(const void *data, uint32_t size) {
  unsigned char *copy;

  if (!data || size == 0 || size > MAX_JPEG_SIZE) {
    return;
  }
  copy = (unsigned char *)malloc(size);
  if (!copy) {
    return;
  }
  memcpy(copy, data, size);

  pthread_mutex_lock(&g_http.lock);
  free(g_http.jpeg);
  g_http.jpeg = copy;
  g_http.jpeg_size = size;
  ++g_http.jpeg_sequence;
  pthread_cond_broadcast(&g_http.frame_ready);
  pthread_mutex_unlock(&g_http.lock);
}

void http_stream_stop(void) {
  pthread_t thread;
  int join_thread;

  pthread_mutex_lock(&g_http.lock);
  join_thread = g_http.thread_started;
  thread = g_http.thread;
  g_http.stop_requested = 1;
  pthread_cond_broadcast(&g_http.frame_ready);
  if (g_http.server_fd >= 0) {
    shutdown(g_http.server_fd, SHUT_RDWR);
  }
  pthread_mutex_unlock(&g_http.lock);

  if (join_thread) {
    pthread_join(thread, NULL);
  }

  pthread_mutex_lock(&g_http.lock);
  while (g_http.active_clients > 0) {
    pthread_cond_wait(&g_http.clients_done, &g_http.lock);
  }
  free(g_http.jpeg);
  g_http.jpeg = NULL;
  g_http.jpeg_size = 0;
  g_http.jpeg_sequence = 0;
  g_http.thread_started = 0;
  g_http.running = 0;
  pthread_mutex_unlock(&g_http.lock);
}
