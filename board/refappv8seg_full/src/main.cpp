/* main.cpp — rgbd_tum-equivalent driver (zero-OpenCV variant).
 *
 * Usage: rgbd_tum_app_v8seg_full <vocab> <settings> <sequence> <association> [yolo_dir]
 *   yolo_dir defaults to ../../models (relative to build/Release).
 * Single-threaded. Uses stb_image for PNG loading, ONNX Runtime for YOLO inference.
 */

#include "ngd_app/maintest.h"
#include "ngd_app/system.h"
#include "stb_image.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#endif

static void LoadImages(const std::string &strAssociationFilename,
                       std::vector<std::string> &vstrImageFilenamesRGB,
                       std::vector<std::string> &vstrImageFilenamesD,
                       std::vector<double> &vTimestamps)
{
    std::ifstream fAssociation(strAssociationFilename.c_str());
    std::string s;
    while (std::getline(fAssociation, s)) {
        if (s.empty()) continue;
        std::stringstream ss(s);
        double t; std::string sRGB, sD;
        ss >> t; vTimestamps.push_back(t);
        ss >> sRGB; vstrImageFilenamesRGB.push_back(sRGB);
        ss >> t; ss >> sD; vstrImageFilenamesD.push_back(sD);
    }
}

// int maintest(int argc, char **argv)

/* [gap-opt-b] 图像预取生产线程 (NGD_IMG_PREFETCH, 板端验证后翻默认):
 * read+decode+深度转换提前一帧在后台发生, 藏进 track 的 DSP/NPU/IO 等待窗
 * (单核亦有效 — orb 457ms VDSP 等待 + NFS 读等待, 等待窗 >> 200ms 装载)。
 * 解码/转换算术与串行版逐行同 → 送 track 的字节逐位同 → 结果 bit 等价。
 * 板端验证 (2026-11-24, _xc01npuZA/npu28): 全量 827 轨迹/关键帧与 W 逐字节同,
 * stage 调用数全同 (orb784/of824/track784/yolo827/KF115), ATE 全统计量同数
 * (1.49/1.35/1.23/0.64/5.31); wall 14.22→11.59min (−18.5%), img_load 166→0.5s,
 * MemFree 谷 2.3→6.8MB 改善。冒烟 head100 亦逐位同 (1.13/0.895288/KF15)。
 * -DNGD_IMG_PREFETCH=0 = 回滚串行装载路径。 */
#ifndef NGD_IMG_PREFETCH
#define NGD_IMG_PREFETCH 1
#endif

#if NGD_IMG_PREFETCH
#include <pthread.h>
#endif

/* [gap-opt] 整文件一次读: 原 stbi_load(path) 走 stdio 小缓冲分块读, 板端 NFS
 * 每块一次 RPC; 一次 read + from_memory 解码既省 IO 开销又可分开计时。
 * 解码输出逐位相同 (同一 stb 解码路径, 只换输入来源)。 */
static uint8_t *read_file_whole(const char *path, size_t *out_len)
{
    FILE *f = fopen(path, "rb");
    if (!f) return nullptr;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return nullptr; }
    uint8_t *buf = (uint8_t*)malloc((size_t)sz);
    if (!buf) { fclose(f); return nullptr; }
    size_t got = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    if (got != (size_t)sz) { free(buf); return nullptr; }
    *out_len = (size_t)sz;
    return buf;
}

#if NGD_IMG_PREFETCH
struct pf_slot {
    uint8_t *rgb;      /* stb 输出; 消费后 stbi_image_free */
    float   *depth;    /* 槽位私有, 按需增长复用 */
    int      depth_sz;
    int      w, h;
    int      ok;       /* 1=好帧; 0=装载失败 (复刻串行版 skip 语义) */
};
struct pf_ctx {
    const std::vector<std::string> *rgbNames, *dNames;
    const std::string *seq;
    int n; float inv;
    pf_slot slots[2];
    int filled[2];
    pthread_mutex_t mu;
    pthread_cond_t cv_full, cv_empty;
    double read_ms, dec_ms;
};

static void *pf_thread(void *arg)
{
    pf_ctx *c = (pf_ctx*)arg;
    for (int i = 0; i < c->n; ++i) {
        const int s = i & 1;
        pthread_mutex_lock(&c->mu);
        while (c->filled[s]) pthread_cond_wait(&c->cv_empty, &c->mu);
        pthread_mutex_unlock(&c->mu);

        const std::string pathRGB = *c->seq + "/" + (*c->rgbNames)[i];
        const std::string pathD   = *c->seq + "/" + (*c->dNames)[i];
        const auto t0 = std::chrono::steady_clock::now();
        size_t rgbLen = 0, dLen = 0;
        uint8_t *rgbFile = read_file_whole(pathRGB.c_str(), &rgbLen);
        uint8_t *dFile   = read_file_whole(pathD.c_str(), &dLen);
        const auto t1 = std::chrono::steady_clock::now();

        pf_slot *sl = &c->slots[s];
        int ch = 0, dw = 0, dh = 0, dch = 0;
        sl->rgb = rgbFile ? stbi_load_from_memory(rgbFile, (int)rgbLen, &sl->w, &sl->h, &ch, 3) : nullptr;
        unsigned short *raw16 = (dFile && stbi_is_16_bit_from_memory(dFile, (int)dLen))
                              ? stbi_load_16_from_memory(dFile, (int)dLen, &dw, &dh, &dch, 1)
                              : nullptr;
        const auto t2 = std::chrono::steady_clock::now();   /* 解码计时止于 stb 输出, 深度转换不计 (同串行口径) */
        free(rgbFile); free(dFile);
        c->read_ms += std::chrono::duration<double, std::milli>(t1 - t0).count();
        c->dec_ms  += std::chrono::duration<double, std::milli>(t2 - t1).count();

        const int have_d = (raw16 != nullptr);
        if (raw16) {
            int n = dw * dh;
            if (n > sl->depth_sz) {
                free(sl->depth);
                sl->depth = (float*)malloc((size_t)n * sizeof(float));
                sl->depth_sz = n;
            }
            for (int k = 0; k < n; ++k)
                sl->depth[k] = (float)raw16[k] * c->inv;
            stbi_image_free(raw16);
        }
        sl->ok = (sl->rgb && have_d) ? 1 : 0;
        if (!sl->ok) {
            std::fprintf(stderr, "Failed to load %s\n", (!sl->rgb ? pathRGB : pathD).c_str());
            stbi_image_free(sl->rgb);   /* 半载 rgb 释放, 槽位干净复用 */
            sl->rgb = nullptr;
        }

        pthread_mutex_lock(&c->mu);
        c->filled[s] = 1;
        pthread_cond_signal(&c->cv_full);
        pthread_mutex_unlock(&c->mu);
    }
    return NULL;
}

/* 阻塞取第 idx 帧的满槽; 返回槽下标 */
static int pf_take(pf_ctx *c, int idx)
{
    const int s = idx & 1;
    pthread_mutex_lock(&c->mu);
    while (!c->filled[s]) pthread_cond_wait(&c->cv_full, &c->mu);
    pthread_mutex_unlock(&c->mu);
    return s;
}
static void pf_release(pf_ctx *c, int s)
{
    pthread_mutex_lock(&c->mu);
    c->filled[s] = 0;
    pthread_cond_signal(&c->cv_empty);
    pthread_mutex_unlock(&c->mu);
}
#endif /* NGD_IMG_PREFETCH */

int main(int argc, char **argv)
{
    if (argc < 5 || argc > 6) {
        std::fprintf(stderr, "Usage: %s path_to_vocabulary path_to_settings path_to_sequence path_to_association [yolo_dir]\n", argv[0]);
        return 1;
    }
    const std::string strVoc = argv[1];
    const std::string strSettings = argv[2];
    const std::string strSequence = argv[3];
    const std::string strAssociation = argv[4];
    const std::string strYoloDir = (argc >= 6) ? argv[5] : "../../models";

    /* [gap-probe] wall-sum gap 分解诊断 (2026-11-24): init / 每帧图像装载 / track / other */
    const auto tApp0 = std::chrono::steady_clock::now();
    double imgMsTot = 0.0, trackS = 0.0, initMs = 0.0, readMsTot = 0.0, decMsTot = 0.0;
    long imgN = 0;

    /* NGD_UNBUF=1: stdout 无缓冲 (板端 panic 时 stdout 缓冲全丢, 无缓冲可让 NFS 侧日志精确定位死亡步骤) */
    if (getenv("NGD_UNBUF")) setvbuf(stdout, NULL, _IONBF, 0);
    /* (E25 mallopt M_MMAP_MAX=0 已删: 对 panic 无效且在 uclibc 上致长跑 brk 碎片化 —
     *   v24 板端帧 34 OOM, anon-rss 216M, 每帧 ~1.3MB 单调涨; 大块恢复默认 mmap 路径) */

    printf("1\n");

    std::vector<std::string> vstrImageFilenamesRGB, vstrImageFilenamesD;
    std::vector<double> vTimestamps;
    LoadImages(strAssociation, vstrImageFilenamesRGB, vstrImageFilenamesD, vTimestamps);
    
    printf("2\n");
    
    const int nImages = (int)vstrImageFilenamesRGB.size();
    if (nImages == 0) { std::fprintf(stderr, "No images found in %s\n", strAssociation.c_str()); return 1; }
    if ((int)vstrImageFilenamesD.size() != nImages) { std::fprintf(stderr, "RGB/depth count mismatch\n"); return 1; }

    printf("3\n");

    ngd_app_system SLAM;
    const auto tInit0 = std::chrono::steady_clock::now();
    if (!ngd_app_system_init(&SLAM, strVoc.c_str(), strSettings.c_str(), strYoloDir.c_str())) {
        std::fprintf(stderr, "System init failed\n");
        return 1;
    }
    initMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tInit0).count();
    std::fprintf(stderr, "[gap] init %.0fms (vocab+yaml+dsp+yolo)\n", initMs);
    const float depthMapFactor = SLAM.cfg.depthMapFactor;

    std::vector<float> vTimesTrack(nImages, 0.0f);
    std::fprintf(stderr, "-------\nStart processing sequence ... %d images\n", nImages);

#if NGD_IMG_PREFETCH
    /* [gap-opt-b] 预取生产线程: 2 槽环形, 生产者 read+decode+深度转换,
     * 消费者循环体只剩取槽等待; 解码/转换算术与串行版逐行同 → bit 等价 */
    pf_ctx pf = {};
    pf.rgbNames = &vstrImageFilenamesRGB;
    pf.dNames = &vstrImageFilenamesD;
    pf.seq = &strSequence;
    pf.n = nImages;
    pf.inv = 1.0f / depthMapFactor;
    pthread_mutex_init(&pf.mu, NULL);
    pthread_cond_init(&pf.cv_full, NULL);
    pthread_cond_init(&pf.cv_empty, NULL);
    pthread_t pf_tid;
    if (pthread_create(&pf_tid, NULL, pf_thread, &pf) != 0) {
        std::fprintf(stderr, "prefetch thread create failed\n");
        return 1;
    }
#else
    /* pre-allocate depth buffer (reused per frame to avoid malloc/free churn) */
    float *depth_buf = nullptr;
    int depth_buf_sz = 0;
#endif

    int nTracked = 0;
    for (int ni = 0; ni < nImages; ++ni) {
        printf("cur image:%d\n", ni);

        const double tframe = vTimestamps[ni];

        uint8_t *rgb = nullptr;
        float *depth_m = nullptr;
        int w = 0, h = 0;
#if NGD_IMG_PREFETCH
        /* [gap-probe] 消费侧 img_load 计时 = 槽等待 (生产者领先时≈0) */
        const auto tImg0 = std::chrono::steady_clock::now();
        const int s = pf_take(&pf, ni);
        rgb = pf.slots[s].rgb;
        depth_m = pf.slots[s].depth;
        w = pf.slots[s].w;
        h = pf.slots[s].h;
        if (!pf.slots[s].ok) {
            pf_release(&pf, s);   /* 失败帧: 生产者已打印 Failed to load */
            continue;
        }
        imgMsTot += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tImg0).count();
        ++imgN;
        printf("image loaded\n");
#else
        const std::string pathRGB = strSequence + "/" + vstrImageFilenamesRGB[ni];
        const std::string pathD   = strSequence + "/" + vstrImageFilenamesD[ni];

        /* [gap-probe] 图像装载段 (NFS 读 + png 解码 + 深度转换, 均在 stage 表外) */
        const auto tImg0 = std::chrono::steady_clock::now();
        /* [gap-opt] 整文件一次读 (read), 再内存解码 (decode) — 分开计时 */
        size_t rgbLen = 0, dLen = 0;
        uint8_t *rgbFile = read_file_whole(pathRGB.c_str(), &rgbLen);
        uint8_t *dFile   = read_file_whole(pathD.c_str(), &dLen);
        const auto tRd1 = std::chrono::steady_clock::now();

        /* load RGB (forced to 3-channel BGR) */
        int ch;
        rgb = rgbFile ? stbi_load_from_memory(rgbFile, (int)rgbLen, &w, &h, &ch, 3) : nullptr;

        /* load depth (16-bit PNG → float metres) */
        int dw, dh, dch;
        unsigned short *raw16 = (dFile && stbi_is_16_bit_from_memory(dFile, (int)dLen))
                              ? stbi_load_16_from_memory(dFile, (int)dLen, &dw, &dh, &dch, 1)
                              : nullptr;
        const auto tDc1 = std::chrono::steady_clock::now();
        readMsTot += std::chrono::duration<double, std::milli>(tRd1 - tImg0).count();
        decMsTot  += std::chrono::duration<double, std::milli>(tDc1 - tRd1).count();
        free(rgbFile); free(dFile);   /* stb 解码输出自持缓冲, 输入可释放 */

        if (raw16) {
            int n = dw * dh;
            if (n > depth_buf_sz) {
                free(depth_buf);
                depth_buf = (float*)malloc((size_t)n * sizeof(float));
                depth_buf_sz = n;
            }
            float inv = 1.0f / depthMapFactor;
            for (int i = 0; i < n; ++i)
                depth_buf[i] = (float)raw16[i] * inv;
            stbi_image_free(raw16);
            depth_m = depth_buf;
        }
        if (!rgb || !depth_m) {
            stbi_image_free(rgb);
            std::fprintf(stderr, "Failed to load %s\n", (!rgb ? pathRGB : pathD).c_str());
            continue;
        }

        imgMsTot += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tImg0).count();
        ++imgN;
        printf("image loaded\n");
#endif

        auto t1 = std::chrono::steady_clock::now();
        int ok = ngd_app_system_track_rgbd(&SLAM, rgb, depth_m, w, h, tframe);
        auto t2 = std::chrono::steady_clock::now();
        const double ttrack = std::chrono::duration<double>(t2 - t1).count();
        trackS += ttrack;

        printf("track finished\n");

        vTimesTrack[ni] = (float)ttrack;
        if (ok) ++nTracked;

        /* free frame data */
        stbi_image_free(rgb);
#if NGD_IMG_PREFETCH
        pf_release(&pf, s);   /* depth 是槽位缓冲, 归生产者复用不释放 */
#endif
        /* depth_m is reused, not freed */

        // double T = 0;
        // if (ni < nImages - 1) T = vTimestamps[ni + 1] - tframe;
        // else if (ni > 0)      T = tframe - vTimestamps[ni - 1];
//         if (ttrack < T) {
// #ifdef _WIN32
//             Sleep((DWORD)((T - ttrack) * 1e3));
// #else
//             usleep((useconds_t)((T - ttrack) * 1e6));
// #endif
//         }
    }

#if NGD_IMG_PREFETCH
    pthread_join(pf_tid, NULL);   /* 生产者恰填 n 槽, 与消费 n 取平衡, 自然结束 */
    readMsTot += pf.read_ms;      /* [gap-probe] read/dec 计数来自生产者线程 */
    decMsTot  += pf.dec_ms;
    free(pf.slots[0].depth);
    free(pf.slots[1].depth);
    pthread_cond_destroy(&pf.cv_empty);
    pthread_cond_destroy(&pf.cv_full);
    pthread_mutex_destroy(&pf.mu);
#else
    free(depth_buf);
#endif

    ngd_app_system_save_trajectory_tum(&SLAM, "CameraTrajectory.txt");
    ngd_app_system_save_keyframe_trajectory_tum(&SLAM, "KeyFrameTrajectory.txt");
    ngd_app_system_save_pose_covariance(&SLAM, "PoseCovariance.txt");

    std::sort(vTimesTrack.begin(), vTimesTrack.end());
    double total = 0; for (int i = 0; i < nImages; ++i) total += vTimesTrack[i];
    std::fprintf(stderr, "-------\nmedian tracking time: %.4f s\nmean tracking time: %.4f s\n",
                 vTimesTrack[nImages / 2], total / nImages);
    std::fprintf(stderr, "tracked %d/%d frames\n", nTracked, nImages);
    {
        const double appS = std::chrono::duration<double>(std::chrono::steady_clock::now() - tApp0).count();
        const double imgS = imgMsTot / 1e3;
        std::fprintf(stderr, "[gap] app %.1fs | init %.1fs | img_load %.1fs (%ld fr, avg %.1fms = read %.1f + dec %.1f) | track %.1fs | other %.1fs\n",
                     appS, initMs / 1e3, imgS, imgN, imgN ? imgMsTot / imgN : 0.0,
                     imgN ? readMsTot / imgN : 0.0, imgN ? decMsTot / imgN : 0.0, trackS,
                     appS - initMs / 1e3 - imgS - trackS);
    }

    ngd_app_system_print_timing(&SLAM, nImages);
    ngd_app_system_shutdown(&SLAM);
    return 0;
}
