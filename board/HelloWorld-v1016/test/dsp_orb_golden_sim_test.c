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
//#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <sys/stat.h>



#include "ngd/orb.h"
#include "ngd_app/dsp_orb.h"
#include "dsp/dsp_orb_vp6.h"
#include "dsp/brief_vp6.h"      /* brief sub-stage counters (timing report) */
#include "dsp/orb_tile_manager.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//#define MAXK 2000

int InitTM();

//#include "dspcalling_orb.h"
extern xvTileManager g_tm;

static int load_file(unsigned char *dst, const char *path, int cap)
{
    FILE *f = fopen(path, "rb");
    if (0 == f) {
        printf("open %s failed\n", path);
        return -1;
    }
    fread(dst, cap, 1, f);
    fclose(f);

    return 0;
}

static void usage(const char *prog)
{
    printf("Usage: %s -s sram.bin -r width height -i gray.bin [-o out_prefix]\n", prog);
}

int loadgolden(char* path, char* prefix, uint8_t* img, int w, int h,
		ngd_keypoint** kps0, uint8_t **desc0, int* Nraw0)
{
	char fullname[1024];
//	char* path = "../golden";
//	char* filename1 = "f000_gray.bin";
	sprintf(fullname, "%s/%s_gray.bin", path, prefix);

	printf("fullname: %s\n", fullname);

	int ret = load_file(img, fullname, w*h);
	if(ret != 0)
	{
		printf("load image failed\n");
		return 1;
	}
	printf("load image succees\n");

	int Nraw = 0;
//	char* filename2 = "f000.kps";
	sprintf(fullname, "%s/%s.kps", path, prefix);
	FILE* fp = fopen(fullname, "rb");
	fread(&Nraw, sizeof(int),1, fp);
	ngd_keypoint* kps = malloc(sizeof(ngd_keypoint)*(size_t)Nraw);
	fread(kps, sizeof(ngd_keypoint)*(size_t)Nraw,1 , fp);
	fclose(fp);

	printf("load kps %d\n", Nraw);

//	char* filename3 = "f000.desc";
	sprintf(fullname, "%s/%s.desc", path, prefix);
	uint8_t *desc = (uint8_t*)malloc((size_t)Nraw * 32);
	load_file(desc, fullname, (size_t)Nraw * 32);

	*kps0 = kps;
	*desc0 = desc;
	*Nraw0 = Nraw;

	return 0;
}
int testgolden(int argc, char *argv[])
{
	printf("start\n");

	int ret = InitTM();
	if(ret != 0)
	{
		printf("orb_tile_manager_init failed\n");
		return -1;
	}

	printf("loading image\n");

	int w = 640, h = 480;
	uint8_t *img = (uint8_t*)malloc((size_t)w * h);

	ngd_keypoint* kps = 0;
	uint8_t *desc = 0;
	int Nraw = 0;

	char* goldenPath = "../golden";///prev_pureC
	ret = loadgolden(goldenPath, "f000", img, w, h, &kps, &desc,&Nraw);
	if(0 != ret)
	{
		printf("golden load failed\n");
		return -1;
	}
	printf("golden load finished\n");

	ngd_orb_extractor exB;//exA,//	ngd_orb_init(&exA, 1000, 1.2f, 8, 20, 7);
	ngd_orb_init(&exB, 1000, 1.2f, 8, 20, 7);
	ngd_keypoint *kB = (ngd_keypoint*)malloc(sizeof(ngd_keypoint) * (size_t)MAXK);//	ngd_keypoint *kA = (ngd_keypoint*)malloc(sizeof(ngd_keypoint) * (size_t)MAXK);
	uint8_t *dB = (uint8_t*)malloc((size_t)MAXK * 32);//	uint8_t *dA = (uint8_t*)malloc((size_t)MAXK * 32);

	brief_vp6_kps = 0;

	printf("start dsp_orb_vp6_extract\n");

	int nB = dsp_orb_vp6_extract(&g_tm, &exB, img, w, h, w, kB, MAXK, dB);

	{
		printf("saving my result\n");
		FILE* fp = fopen("mykps.bin", "wb");
		fwrite(&nB, sizeof(int),1,fp);
		fwrite(kB, sizeof(ngd_keypoint)*(size_t)nB,1,fp);
		fclose(fp);

		FILE* fp2 = fopen("mydesc.bin", "wb");
		fwrite(dB, (size_t)nB * 32, 1, fp2);
		fclose(fp2);
	}

	dsp_orb_vp6_timing_report();
	//bit-excapt

	if(nB != Nraw)
	{
		printf("FAIL (keypoint count differs)\n");
		free(kps);
		free(desc);
		free(kB);
		free(dB);
		free(img);
		return 1;
	}
	if(nB > 0)
	{
		if(memcmp(kps, kB, sizeof(ngd_keypoint)*(size_t)Nraw) != 0)
		{
			printf(" FAIL keypoint byte differ\n");
			for(int i = 0; i < Nraw; ++i)
			{
				if(memcmp(&kps[i], &kB[i], sizeof(ngd_keypoint)) != 0)
				{
					printf(" first diff at %d\n", i);
					break;
				}
			}

			free(kps);
			free(desc);
			free(kB);
			free(dB);
			free(img);
			return 1;
		}

		if(memcmp(desc, dB, (size_t)Nraw * 32) != 0)
		{
			printf("FAIL descriptor bytes differ\n");

			free(kps);
			free(desc);
			free(kB);
			free(dB);
			free(img);
			return 1;
		}
	}

	printf("  PASS(bit-exact)\n");

	free(kps);
	free(desc);
	free(kB);
	free(dB);
	free(img);

    return 0;
}
//	if (orb_tile_manager_init(&g_tm) != 0) {
//		printf("orb_tile_manager_init failed\n");
//		return 1;
//	}
//    const char *sram = NULL, *input = NULL, *prefix = "orb_out";
//    FH_UINT32 w = 0, h = 0;
//    int ch;
//
//    const struct option opts[] = {
//        {"sram",  required_argument, NULL, 's'},
//        {"res",   required_argument, NULL, 'r'},
//        {"image", required_argument, NULL, 'i'},
//        {"out",   required_argument, NULL, 'o'},
//        {0, 0, 0, 0},
//    };
//    while ((ch = getopt_long(argc, argv, "s:r:i:o:", opts, NULL)) != -1) {
//        switch (ch) {
//        case 's': sram  = optarg; break;
//        case 'r': w = (FH_UINT32)atoi(optarg);
//                  if (optind < argc) h = (FH_UINT32)atoi(argv[optind]); break;
//        case 'i': input = optarg; break;
//        case 'o': prefix = optarg; break;
//        default:  usage(argv[0]); return 1;
//        }
//    }
//    if (!sram || !input || !w || !h) { usage(argv[0]); return 1; }
//
//    /* ORB-SLAM / TUM3.yaml parameters, same as the app's extractor */
//    enum { MAX_KPS = 2000 };
//
//    if (dspc_init(sram) != 0)
//        return 1;

//    dspc_orb_t orb;
//    if (dspc_orb_alloc(&orb, w, h, MAX_KPS) != 0)
//        goto exit;
//
//    FH_UINT8 *img = (FH_UINT8 *)malloc((size_t)w * h);
//    if (!img) {
//        printf("malloc %ux%u failed\n", w, h);
//        goto exit_free;
//    }
//    if (load_file(img, input, w * h) != 0) {
//        free(img);
//        goto exit_free;
//    }
//
//    static DSPC_ORB_KEYPOINT_S kps[MAX_KPS];
//    static FH_UINT8 desc[MAX_KPS * DSPC_ORB_DESC_SIZE];
//    FH_UINT32 n = 0;
//
//    FH_UINT64 t0 = dspc_pts_us();
//    int rc = dspc_orb_extract(&orb, img, w, h,
//                              1000, 1.2f, 8, 20, 7, kps, desc, &n);
//    FH_UINT64 t1 = dspc_pts_us();
//
//    if (rc != 0) {
//        printf("dspc_orb_extract failed\n");
//        free(img);
//        goto exit_free;
//    }
//
//    printf("orb: %u keypoints, RPC round trip %llu us\n",
//           n, (unsigned long long)(t1 - t0));
//    for (FH_UINT32 i = 0; i < n && i < 5; ++i)
//        printf("  kp[%u]: x=%.2f y=%.2f ang=%.1f resp=%.0f oct=%d size=%.1f\n",
//               i, kps[i].x, kps[i].y, kps[i].angle, kps[i].response,
//               kps[i].octave, kps[i].size);
//    if (n > 0) {
//        printf("  desc[0] first 16 bytes:");
//        for (int b = 0; b < 16; ++b) printf(" %02x", desc[b]);
//        printf("\n");
//    }
//
//    /* dump for offline compare against the pure-C / cstub outputs:
//     * .kps = FH_UINT32 n, then n * DSPC_ORB_KEYPOINT_S
//     * .desc = n * 32 raw bytes */
//    char path[512];
//    FILE *f;
//    snprintf(path, sizeof(path), "%s.kps", prefix);
//    if ((f = fopen(path, "wb")) != NULL) {
//        fwrite(&n, sizeof(n), 1, f);
//        fwrite(kps, sizeof(kps[0]), n, f);
//        fclose(f);
//    }
//    snprintf(path, sizeof(path), "%s.desc", prefix);
//    if ((f = fopen(path, "wb")) != NULL) {
//        fwrite(desc, DSPC_ORB_DESC_SIZE, n, f);
//        fclose(f);
//    }
//    printf("dumped %s.kps / %s.desc\n", prefix, prefix);
//    free(img);
//
//exit_free:
//    dspc_orb_free(&orb);
//exit:
//    dspc_exit();


