
#include <xtensa/config/core.h>

#include<stdio.h>
#include<math.h>
#include<string.h>
#include<stdint.h>
#include<stdlib.h>
#include "print_dsp.h"

#include "ngd/orb.h"
#include "ngd_app/dsp_orb.h"
#include "dsp/dsp_orb_vp6.h"
#include "dsp/fast9_scoremap.h" /* collect scan byte/candidate counts (v1015) */
#include "dsp/brief_vp6.h"      /* brief sub-stage counters (timing report) */
#include "dsp/orb_blur_vp6.h"   /* blur sub-stage counters (v1003 export) */
#include "dsp/dsp_calib_vp6.h"  /* on-chip cost calibration (v1005 export) */
#include "dsp/dsp_orb_octtree.h" /* quadtree sub-phase counters (v1006)   */
#include "dsp/orb_tile_manager.h"


xvTileManager g_tm;

int InitTM()
{
	if (orb_tile_manager_init(&g_tm) != 0)
	{
		printf("orb_tile_manager_init failed\n");
		return 1;
	}
	return 0;
}

int testgolden(int argc, char *argv[]);


int main(int argc, char **argv)
{
#if 0
	testgolden(argc, argv);
	return 0;
#endif

	DspInit();

	if (InitTM() != 0)
	{
		printf("orb_tile_manager_init failed\n");
		return 1;
	}

	const int versionID = 1016;	/* 1002: + per-stage cycles block; 1003: + sub-stage
	                                 block + collect band staging; 1004: collect band
	                                 y-offset fix + blur H/V & collect dma/scan slots;
	                                 1005: + CAL1 on-chip cost calibration block;
	                                 1006: collect chunk-hit prefilter + CAL2 blur
	                                 decomposition + blur vec/scalar counters + qt
	                                 init/rounds/qsort/emit slots (CYB1 16); 1007:
	                                 blur counters -> register locals (hot-loop
	                                 DDR ++ cost 98ms), prefilter reverted (hit
	                                 rate ~100%), + CAL3 production-shape block;
	                                 1008: blur scalar border weights moved to the
	                                 caller's stack gk[] (global orb_blur_gauss7
	                                 read per tap = 7x84 DDR cyc/pixel, ~93% of
	                                 the 472ms blur stage); 1009: quadtree pool
	                                 rewrite (flat node pool + single key arena +
	                                 stable in-place 4-way partition, no per-node
	                                 malloc: 2129 cyc/pair x ~10k pairs was the
	                                 307ms stage) + scoremap ring table to stack
	                                 (32 global reads/row); 1010: quadtree
	                                 partitions staged into bank SRAM (octtree
	                                 scratch + iDMA mover, oversized spans fall
	                                 back to the in-place path; every DDR arena
	                                 touch was 83.9 cyc), nIni==1 skips the
	                                 initial partition, float compares as
	                                 order-preserving int compares; 1011:
	                                 scratch budget 41 -> 28 B/record, fallback
	                                 classify also int-compare, 16KB move
	                                 pieces, + qt.scmax/scmin/stg/fbk slots
	                                 (CYB1 20); 1012: every partition is a
	                                 single-pass stable scatter through
	                                 running cursors (qtmp/pos/cycle-walk
	                                 deleted — telemetry showed the walk
	                                 shape at ~1150/visit, not the DDR
	                                 medium; staged path scatters straight
	                                 into the arena kid slots, no move-back,
	                                 scratch = pure 24 B/record records);
	                                 1013: oversized spans stage in chunks
	                                 too (verbatim temp copy + count /
	                                 scatter sweeps through the bank — the
	                                 scatter shape has no write-before-read
	                                 hazard; v1012 telemetry: DDR scatter
	                                 ~800/visit vs ~200 staged); 1014:
	                                 1013 reverted — its 64-chunk partial
	                                 table put ~1KB on the divide stack
	                                 frame and overflowed the DSP stack
	                                 (deterministic first-RPC wedge on
	                                 the board, vdsp_do_request D-state;
	                                 the host differential cannot see it
	                                 — host stack is MBs). Staged path
	                                 serves spans within the scratch
	                                 again; oversized spans back to the
	                                 v1012 DDR scatter; 1015:
	                                 measurement-only round for the
	                                 collect scan (188.95ms, 35% of
	                                 TOTAL): CAL4 splits the 39.5
	                                 cyc/byte into base loop (all-NONE
	                                 band) vs candidate tail and prices
	                                 a vector threshold-skip prototype
	                                 (per 64B chunk: streaming load +
	                                 skip-lane masks + select/MIN +
	                                 scratch store + scalar-OR all-skip
	                                 test); CYB1 gains
	                                 collect.bytes/collect.cands
	                                 production counts (20 -> 22 slots)
	                                 so the real byte-visit count and
	                                 candidate density close the cost
	                                 equation. No behavior change; 1016:
	                                 octtree OOM hygiene — oct_make_kids
	                                 clears kids[] to -1 when the node-pool
	                                 grow fails (callers read uninitialized
	                                 stack indices otherwise), the strips
	                                 start-malloc failure path frees cnt,
	                                 octtree header contract corrected
	                                 (nout can overshoot N by up to 3).
	                                 No success-path change */

	ngd_orb_extractor exB;
	ngd_orb_init(&exB, 1000, 1.2f, 8, 20, 7);

//	ngd_keypoint *kB = (ngd_keypoint*)malloc(sizeof(ngd_keypoint) * (size_t)MAXK);
//	uint8_t *dB = (uint8_t*)malloc((size_t)MAXK * 32);

	int headsize = sizeof(int);
	int kBsize = sizeof(ngd_keypoint) * (size_t)MAXK;
	int dBsize = (size_t)MAXK * 32;


	int val;
	while(1)
	{
		val = reg32(CMD_REG);

		if(!(val == 1 || val== 2 || val==3)){
			continue;
		}

		reg32(CMD_REG) = 0x10;
//		printf_dsp("dsp work once: %d\n", val);

		if(val == 2)//set version
		{
			reg32(OUT_INFO) = versionID;
		}
		else if(val == 1)//workflow
		{
//			printf_dsp("orb feature extraction\n");
			deviceInit();

//			uint8_t* pInput = (uint8_t*)graphic_address.u32SrcLeftAddr;
//			uint8_t* pOutput = (uint8_t*)result_address;
//	        float* pParam = (float*)info_address;

			uint8_t* pInput = (uint8_t*)pInputInfoWarp->stInputImage.au32PhyAddr[0];
			uint8_t* pOutput = (uint8_t*)pInputInfoWarp->stOutPutImage.au32PhyAddr[0];

			int imgWidthOut = pInputInfoWarp->stOutPutImage.u32Width;
			int imgHeightOut = pInputInfoWarp->stOutPutImage.u32Height;

			int imgWidthIn = pInputInfoWarp->stInputImage.u32Width;
			int imgHeightIn = pInputInfoWarp->stInputImage.u32Height;

			SVP_DSP_VERTEX_S* pSrcPos = pInputInfoWarp->stSrcPos;
			SVP_DSP_VERTEX_S* pDstPos = pInputInfoWarp->stDstPos;
			int reserved2 = pInputInfoWarp->bUpdateFlg;

	        int *num = (int*)pOutput;
	        ngd_keypoint *kB = (ngd_keypoint*)(pOutput + headsize);
	        uint8_t *dB = pOutput + headsize + kBsize;
	        /* per-stage cycle block appended after the fixed desc region
	         * (offset headsize+kBsize+dBsize = 112004): "CYC1" magic, stage
	         * count, then each u64 as lo/hi u32 pair (offset only 4-aligned).
	         * Host must size the output buffer accordingly (v1002). */
	        unsigned int *pTim = (unsigned int*)(pOutput + headsize + kBsize + dBsize);

	        *num = 0;
	        /* one-shot on-chip calibration (v1005): run BEFORE extract so
	         * its bank allocations never coexist with the drivers' */
	        dsp_calib_vp6_run(&g_tm);
	        /* counters accumulate (+=) inside extract: clear so the block
	         * reports THIS call only */
	        memset(dsp_orb_vp6_cycles, 0, sizeof(dsp_orb_vp6_cycles));
	        memset(dsp_orb_vp6_qt, 0, sizeof(dsp_orb_vp6_qt));
	        memset(dsp_orb_vp6_cs, 0, sizeof(dsp_orb_vp6_cs));
	        memset(orb_blur_vp6_sub, 0, sizeof(orb_blur_vp6_sub));
	        memset(dsp_octtree_cs, 0, sizeof(dsp_octtree_cs));
	        memset(dsp_octtree_qcnt, 0, sizeof(dsp_octtree_qcnt));
	        orb_blur_vp6_vec = 0;
	        orb_blur_vp6_scol = 0;
	        fast9_vp6_scan_bytes = 0;          /* v1015 collect telemetry */
	        fast9_vp6_scan_cands = 0;

	        *num = 0;

	        int w = imgWidthIn;
	        int h = imgHeightIn;

	        unsigned char* img = pInput;
			// do something
	        int nB = dsp_orb_vp6_extract(&g_tm, &exB, img, w, h, w, kB, MAXK, dB);

	        *num = nB;

	        /* export this call's per-stage cycles (board DSP has no printf);
	         * written before INTERAPT so the host sees them on completion */
	        pTim[0] = 0x31435943u;	/* "CYC1" */
	        pTim[1] = (unsigned int)DSP_ORB_VP6_T_COUNT;
	        for (int i = 0; i < DSP_ORB_VP6_T_COUNT; ++i) {
	            pTim[2 + 2 * i] = (unsigned int)(dsp_orb_vp6_cycles[i] & 0xffffffffu);
	            pTim[3 + 2 * i] = (unsigned int)(dsp_orb_vp6_cycles[i] >> 32);
	        }

	        /* sub-stage block right behind (v1003): blur submit/wait/kernel,
	         * quadtree prep/tree/post. v1004 appends blur hpass/vpass and
	         * collect dma/scan; v1006 appends the blur kernel structure
	         * counters and the quadtree sub-phases — 16 slots total */
	        unsigned int *pSub = pTim + 2 + 2 * DSP_ORB_VP6_T_COUNT;
	        const unsigned long long subv[] = {
	            orb_blur_vp6_sub[ORB_BLUR_VP6_S_SUBMIT],
	            orb_blur_vp6_sub[ORB_BLUR_VP6_S_WAIT],
	            orb_blur_vp6_sub[ORB_BLUR_VP6_S_KERNEL],
	            dsp_orb_vp6_qt[DSP_ORB_VP6_Q_PREP],
	            dsp_orb_vp6_qt[DSP_ORB_VP6_Q_TREE],
	            dsp_orb_vp6_qt[DSP_ORB_VP6_Q_POST],
	            orb_blur_vp6_sub[ORB_BLUR_VP6_S_HPASS],
	            orb_blur_vp6_sub[ORB_BLUR_VP6_S_VPASS],
	            dsp_orb_vp6_cs[DSP_ORB_VP6_C_DMA],
	            dsp_orb_vp6_cs[DSP_ORB_VP6_C_SCAN],
	            orb_blur_vp6_vec,                            /* count, not cyc */
	            orb_blur_vp6_scol,                           /* count, not cyc */
	            dsp_octtree_cs[DSP_OCT_INIT],
	            dsp_octtree_cs[DSP_OCT_ROUNDS],
	            dsp_octtree_cs[DSP_OCT_QSORT],
	            dsp_octtree_cs[DSP_OCT_EMIT],
	            dsp_octtree_qcnt[0],                        /* counts, not cyc */
	            dsp_octtree_qcnt[1],
	            dsp_octtree_qcnt[2],
	            dsp_octtree_qcnt[3],
	            fast9_vp6_scan_bytes,                      /* count, not cyc */
	            fast9_vp6_scan_cands,                      /* count, not cyc */
	        };
	        pSub[0] = 0x31425943u;	/* "CYB1" */
	        pSub[1] = sizeof(subv) / sizeof(subv[0]);
	        for (unsigned int i = 0; i < sizeof(subv) / sizeof(subv[0]); ++i) {
	            pSub[2 + 2 * i] = (unsigned int)(subv[i] & 0xffffffffu);
	            pSub[3 + 2 * i] = (unsigned int)(subv[i] >> 32);
	        }

	        /* calibration block right behind (v1005): "CAL1", count, then
	         * DSP_CALIB_COUNT u64 (cycles/op x100, same lo/hi layout) */
	        unsigned int *pCal = pSub + 2 + 2 * (sizeof(subv) / sizeof(subv[0]));
	        pCal[0] = 0x314C4143u;	/* "CAL1" */
	        pCal[1] = DSP_CALIB_COUNT;
	        for (int i = 0; i < DSP_CALIB_COUNT; ++i) {
	            pCal[2 + 2 * i] = (unsigned int)(dsp_calib_vp6_res[i] & 0xffffffffu);
	            pCal[3 + 2 * i] = (unsigned int)(dsp_calib_vp6_res[i] >> 32);
	        }

	        /* second calibration block right behind (v1006): "CAL2" blur
	         * composites + blind-spot unit costs, same lo/hi layout */
	        unsigned int *pCal2 = pCal + 2 + 2 * DSP_CALIB_COUNT;
	        pCal2[0] = 0x324C4143u;	/* "CAL2" */
	        pCal2[1] = DSP_CALIB2_COUNT;
	        for (int i = 0; i < DSP_CALIB2_COUNT; ++i) {
	            pCal2[2 + 2 * i] = (unsigned int)(dsp_calib2_vp6_res[i] & 0xffffffffu);
	            pCal2[3 + 2 * i] = (unsigned int)(dsp_calib2_vp6_res[i] >> 32);
	        }

	        /* third calibration block right behind (v1007): "CAL3"
	         * production-shaped kernel/scan measurements */
	        unsigned int *pCal3 = pCal2 + 2 + 2 * DSP_CALIB2_COUNT;
	        pCal3[0] = 0x334C4143u;	/* "CAL3" */
	        pCal3[1] = DSP_CALIB3_COUNT;
	        for (int i = 0; i < DSP_CALIB3_COUNT; ++i) {
	            pCal3[2 + 2 * i] = (unsigned int)(dsp_calib3_vp6_res[i] & 0xffffffffu);
	            pCal3[3 + 2 * i] = (unsigned int)(dsp_calib3_vp6_res[i] >> 32);
	        }

	        /* fourth calibration block right behind (v1015): "CAL4"
	         * collect scan decomposition (base loop vs vector skip) */
	        unsigned int *pCal4 = pCal3 + 2 + 2 * DSP_CALIB3_COUNT;
	        pCal4[0] = 0x344C4143u;	/* "CAL4" */
	        pCal4[1] = DSP_CALIB4_COUNT;
	        for (int i = 0; i < DSP_CALIB4_COUNT; ++i) {
	            pCal4[2 + 2 * i] = (unsigned int)(dsp_calib4_vp6_res[i] & 0xffffffffu);
	            pCal4[3 + 2 * i] = (unsigned int)(dsp_calib4_vp6_res[i] >> 32);
	        }
		}

		reg32(INTERAPT) = 1;

	}

//	free(kB);
//	free(dB);

	return 0;
}
