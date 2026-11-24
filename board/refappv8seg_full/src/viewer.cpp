/* ngd_app/viewer.cpp — PNG frame writer (zero-OpenCV).
 *
 * NGD_VIEWER=0: no-op.
 * NGD_VIEWER=1: writes viewer_frames/frame_NNNNNN.png with mask overlay.
 */
#include "ngd_app/viewer.h"
#include "stb_image_write.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#endif

//tzg
#define UNUSED(x) (void)(x)


static void ensure_dir(const char *dir) {
#ifdef _WIN32
    CreateDirectoryA(dir, NULL);
#else
    /* best-effort: ignore failure */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "mkdir -p \"%s\" 2>/dev/null", dir);
    system(cmd);
#endif
}

void ngd_app_viewer_init(ngd_app_viewer *v, int enabled,
                         float /*vpX*/, float /*vpY*/, float /*vpZ*/, float /*vpF*/)
{
    memset(v, 0, sizeof(*v));
    v->enabled = enabled;
    if (enabled) {
        ensure_dir("viewer_frames");
        fprintf(stderr, "viewer: PNG frames -> viewer_frames/ (NGD_VIEWER=1)\n");
    }
}

void ngd_app_viewer_free(ngd_app_viewer *v)
{
    free(v->traj);
    v->traj = NULL;
    v->n_traj = v->cap_traj = 0;
}

void ngd_app_viewer_draw(ngd_app_viewer *v,
                         const ngd_map * /*map*/,
                         const ngd_frame * /*cur*/,
                         const uint8_t *imRGB, int w, int h, int channels,
                         const uint8_t *mask, int maskW, int maskH,
                         int /*state*/, int /*nInliers*/, uint64_t /*frame_num*/)
{
    if (!v->enabled || !imRGB) return;

    UNUSED(channels);

    /* copy BGR image with mask overlay (red tint on dynamic regions) */
    int npix = w * h;
    uint8_t *out = (uint8_t *)malloc((size_t)npix * 3);
    const uint8_t redB = 0, redG = 0, redR = 180;  /* BGR dark red */

    if (mask && maskW == w && maskH == h) {
        for (int i = 0; i < npix; ++i) {
            int off = i * 3;
            if (mask[i]) {
                /* blend: 65% original + 35% red */
                out[off]   = (uint8_t)(((int)imRGB[off]   * 65 + (int)redB * 35) / 100);
                out[off+1] = (uint8_t)(((int)imRGB[off+1] * 65 + (int)redG * 35) / 100);
                out[off+2] = (uint8_t)(((int)imRGB[off+2] * 65 + (int)redR * 35) / 100);
            } else {
                out[off]   = imRGB[off];
                out[off+1] = imRGB[off+1];
                out[off+2] = imRGB[off+2];
            }
        }
    } else {
        memcpy(out, imRGB, (size_t)npix * 3);
    }

    /* write PNG */
    char path[640];
    snprintf(path, sizeof(path), "viewer_frames/frame_%06d.png", v->frame_count);
    stbi_write_png(path, w, h, 3, out, w * 3);
    free(out);

    /* only log every 50 frames to avoid I/O spam */
    if ((v->frame_count % 50) == 0)
        fprintf(stderr, "viewer: wrote %s\n", path);
    v->frame_count++;
}
