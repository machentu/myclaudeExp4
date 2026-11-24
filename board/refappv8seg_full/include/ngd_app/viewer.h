#ifndef NGD_APP_VIEWER_H
#define NGD_APP_VIEWER_H

/*
 * ngd_app/viewer.h — PNG frame writer (zero-OpenCV variant).
 *
 * NGD_VIEWER=0 (default): no-op, headless benchmark mode.
 * NGD_VIEWER=1: write each frame as PNG with mask overlay to viewer_frames/.
 *
 * Controlled by compile-time #define NGD_VIEWER in system.cpp.
 */

#include "ngd/math.h"
#include "ngd/se3.h"
#include "ngd/frame.h"
#include "ngd/keyframe.h"
#include "ngd/mappoint.h"
#include "ngd/map.h"
#include <stdint.h>

typedef struct ngd_app_viewer {
    int   enabled;
    int   frame_count;    /* output frame counter (for filename) */
    char  out_dir[512];   /* output directory path */
    /* live camera trajectory: camera centres in the map/world frame. */
    ngd_vec3 *traj;
    int   n_traj, cap_traj;
} ngd_app_viewer;

void ngd_app_viewer_init(ngd_app_viewer *v, int enabled,
                         float vpX, float vpY, float vpZ, float vpF);
void ngd_app_viewer_free(ngd_app_viewer *v);

/* When enabled: writes frame_NNNNNN.png with mask overlay (red tint on dynamic regions).
 * When disabled: no-op. */
void ngd_app_viewer_draw(ngd_app_viewer *v,
                         const ngd_map *map,
                         const ngd_frame *cur,
                         const uint8_t *imRGB, int w, int h, int channels,
                         const uint8_t *mask, int maskW, int maskH,
                         int state, int nInliers, uint64_t frame_num);

#endif /* NGD_APP_VIEWER_H */
