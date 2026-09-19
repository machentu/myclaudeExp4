/* dspcalling_orb_example.c — run one ORB extraction through the real DSP.
 *
 * Load a raw grayscale image (w*h bytes), send it through dspc_orb_extract
 * with the ORB-SLAM parameters the app uses (1000, 1.2, 8, 20, 7), print
 * the result summary and dump keypoints + descriptors to files so they can
 * be compared offline against the pure-C / cstub outputs.
 *
 * Build (device box, SDK headers + libmpi on the include/link path):
 *   $(CC) dspcalling.c dspcalling_orb.c dspcalling_orb_example.c \
 *         -o dspcalling_orb_example -lmpi -lpthread -lm
 * Run:
 *   ./dspcalling_orb_example -s orb_sram.bin -r 640 480 -i frame.bin \
 *                            -o orb_out
 * Outputs orb_out.kps (n * sizeof(DSPC_ORB_KEYPOINT_S), n from the first
 * 4 bytes... see dump format below) and orb_out.desc (n * 32).
 */
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "dspcalling_orb.h"

static int load_file(FH_UINT8 *dst, const char *path, FH_UINT32 cap)
{
    struct stat st;
    FILE *f;

    if (stat(path, &st) != 0 || !(f = fopen(path, "rb"))) {
        printf("open %s failed\n", path);
        return -1;
    }
    if ((FH_UINT32)st.st_size > cap) {
        printf("%s (%lld bytes) larger than buffer (%u)\n",
               path, (long long)st.st_size, cap);
        fclose(f);
        return -1;
    }
    fread(dst, st.st_size, 1, f);
    fclose(f);
    return 0;
}

static void usage(const char *prog)
{
    printf("Usage: %s -s sram.bin -r width height -i gray.bin [-o out_prefix]\n", prog);
}

int main(int argc, char *argv[])
{
    const char *sram = NULL, *input = NULL, *prefix = "orb_out";
    FH_UINT32 w = 0, h = 0;
    int ch;

    const struct option opts[] = {
        {"sram",  required_argument, NULL, 's'},
        {"res",   required_argument, NULL, 'r'},
        {"image", required_argument, NULL, 'i'},
        {"out",   required_argument, NULL, 'o'},
        {0, 0, 0, 0},
    };
    while ((ch = getopt_long(argc, argv, "s:r:i:o:", opts, NULL)) != -1) {
        switch (ch) {
        case 's': sram  = optarg; break;
        case 'r': w = (FH_UINT32)atoi(optarg);
                  if (optind < argc) h = (FH_UINT32)atoi(argv[optind]); break;
        case 'i': input = optarg; break;
        case 'o': prefix = optarg; break;
        default:  usage(argv[0]); return 1;
        }
    }
    if (!sram || !input || !w || !h) { usage(argv[0]); return 1; }

    /* ORB-SLAM / TUM3.yaml parameters, same as the app's extractor */
    enum { MAX_KPS = 2000 };

    if (dspc_init(sram) != 0)
        return 1;

    dspc_orb_t orb;
    if (dspc_orb_alloc(&orb, w, h, MAX_KPS) != 0)
        goto exit;

    FH_UINT8 *img = (FH_UINT8 *)malloc((size_t)w * h);
    if (!img) {
        printf("malloc %ux%u failed\n", w, h);
        goto exit_free;
    }
    if (load_file(img, input, w * h) != 0) {
        free(img);
        goto exit_free;
    }

    static DSPC_ORB_KEYPOINT_S kps[MAX_KPS];
    static FH_UINT8 desc[MAX_KPS * DSPC_ORB_DESC_SIZE];
    FH_UINT32 n = 0;

    FH_UINT64 t0 = dspc_pts_us();
    int rc = dspc_orb_extract(&orb, img, w, h,
                              1000, 1.2f, 8, 20, 7, kps, desc, &n);
    FH_UINT64 t1 = dspc_pts_us();

    if (rc != 0) {
        printf("dspc_orb_extract failed\n");
        free(img);
        goto exit_free;
    }

    printf("orb: %u keypoints, RPC round trip %llu us\n",
           n, (unsigned long long)(t1 - t0));
    for (FH_UINT32 i = 0; i < n && i < 5; ++i)
        printf("  kp[%u]: x=%.2f y=%.2f ang=%.1f resp=%.0f oct=%d size=%.1f\n",
               i, kps[i].x, kps[i].y, kps[i].angle, kps[i].response,
               kps[i].octave, kps[i].size);
    if (n > 0) {
        printf("  desc[0] first 16 bytes:");
        for (int b = 0; b < 16; ++b) printf(" %02x", desc[b]);
        printf("\n");
    }

    /* dump for offline compare against the pure-C / cstub outputs:
     * .kps = FH_UINT32 n, then n * DSPC_ORB_KEYPOINT_S
     * .desc = n * 32 raw bytes */
    char path[512];
    FILE *f;
    snprintf(path, sizeof(path), "%s.kps", prefix);
    if ((f = fopen(path, "wb")) != NULL) {
        fwrite(&n, sizeof(n), 1, f);
        fwrite(kps, sizeof(kps[0]), n, f);
        fclose(f);
    }
    snprintf(path, sizeof(path), "%s.desc", prefix);
    if ((f = fopen(path, "wb")) != NULL) {
        fwrite(desc, DSPC_ORB_DESC_SIZE, n, f);
        fclose(f);
    }
    printf("dumped %s.kps / %s.desc\n", prefix, prefix);
    free(img);

exit_free:
    dspc_orb_free(&orb);
exit:
    dspc_exit();
    return 0;
}
