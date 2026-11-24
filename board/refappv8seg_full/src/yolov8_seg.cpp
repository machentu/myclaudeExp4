/* ngd_app/yolov8_seg.cpp — YOLOv8-seg detector (multi DNN backend, zero OpenCV).
 *
 * Compile-time backend selection:
 *   NGD_DNN_BACKEND=0 (default) — ONNX Runtime (onnxruntime_c_api.h)
 *   NGD_DNN_BACKEND=1            — MNN (Alibaba, MNN/Interpreter.hpp)
 *   NGD_DNN_BACKEND=2            — fy01p NPU (NNP318, uint8 NCHW tensor 输入, 无 AIPP)
 *   NGD_DNN_BACKEND=3            — xc01 NPU (NNP310, TY_NPU 运行时, uint8 NCHW tensor 输入, 无 AIPP)
 *
 * The detect() pipeline (LetterBox→blob→dnn_run→C-decoders→postproc) is
 * shared between backends; only init/run/free differ.
 * NPU(==2/3) 走 uint8 tensor: 软件 letterbox+blob(μ8 NCHW RGB) → SubGraph::run_u8;
 * 不走 AIPP (NNP318/310 AIPP 不支持 BGR/RGB packed + resize)。
 * ==2 与 ==3 的 SubGraph 接口同名同构 (qwen_board 与 qwen_xc01_mul/board_qrun
 * 符号级移植), 仅 -I 指向各自的运行时源码树。
 */
#include "ngd_app/yolov8_seg.h"

#include "libkptdecode.h"
#include "outsize.h"
#include "ngd/mask.h"        /* ngd_mask_dilate */

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <memory>

/* ---- pure-C image processing functions (from image_proc.c) ---- */
extern "C" {
void ngd_resize_nearest_u8(const uint8_t *src, int sw, int sh, int sstride,
                           uint8_t *dst, int dw, int dh);
void ngd_resize_nearest_f32(const float *src, int sw, int sh, int sstride,
                            float *dst, int dw, int dh);
int  ngd_count_nonzero_u8(const uint8_t *src, int n);
#if defined(NGD_POST_NEON) && defined(__ARM_NEON__)
/* [perf ③] 后处理 NEON (实现: src/ngd_opt_postneon.c, 宏关时零影响) */
extern "C" {
void ngd_post_resize_f32_bin_neon(const float *src, int sw, int sh, int sstride,
                                  uint8_t *dst, int dw, int dh);
void ngd_post_resize_u8_neon(const uint8_t *src, int sw, int sh, int sstride,
                             uint8_t *dst, int dw, int dh);
int  ngd_post_count_nonzero_neon(const uint8_t *src, int n);
}
#endif
void ngd_letterbox_u8(const uint8_t *src, int sw, int sh, int sstride,
                      uint8_t *dst, int netW, int netH,
                      float *out_params, uint8_t fill);
void ngd_letterbox_params(int sw, int sh, int netW, int netH, float *out_params);
void ngd_blob_from_image(const uint8_t *bgr, int sw, int sh, int sstride,
                         float *blob, int netW, int netH);
void ngd_blob_from_image_u8(const uint8_t *bgr, int sw, int sh, int sstride,
                            uint8_t *blob, int netW, int netH);
}

/* ===================================================================
 *  BACKEND SELECTION
 * =================================================================== */
#ifndef NGD_DNN_BACKEND
#define NGD_DNN_BACKEND 1   /* 0=onnxruntime, 1=mnn(CPU), 2=fy01p NPU(NNP318), 3=xc01 NPU(NNP310) */
#endif

/* ===================================================================
 *  BACKEND IMPLEMENTATIONS
 * =================================================================== */

#if NGD_DNN_BACKEND == 0
/* -------------------- ONNX Runtime backend ------------------------ */
#include <onnxruntime_c_api.h>

struct DnnSession {
    OrtEnv        *env = nullptr;
    OrtSession    *session = nullptr;
    OrtMemoryInfo *memInfo = nullptr;
    OrtAllocator  *allocator = nullptr;
};

static const OrtApi *g_ort = nullptr;
static const OrtApi* ort_api() {
    if (!g_ort) g_ort = OrtGetApiBase()->GetApi(ORT_API_VERSION);
    return g_ort;
}

static bool ort_ok(OrtStatus *st) {
    if (!st) return true;
    const char *msg = ort_api()->GetErrorMessage(st);
    fprintf(stderr, "Yolov8Seg ORT: %s\n", msg ? msg : "(null)");
    ort_api()->ReleaseStatus(st);
    return false;
}

static DnnSession* dnn_load(const char *path, int netW, int netH) {
    (void)netW; (void)netH;
    const OrtApi *api = ort_api();
    OrtEnv *env = nullptr;
    if (!ort_ok(api->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "ngd", &env))) return nullptr;
    OrtSessionOptions *sopt = nullptr;
    if (!ort_ok(api->CreateSessionOptions(&sopt))) { api->ReleaseEnv(env); return nullptr; }
    api->SetSessionGraphOptimizationLevel(sopt, ORT_ENABLE_ALL);
    std::wstring wpath(path, path + strlen(path));
    OrtSession *sess = nullptr;
    OrtStatus *st = api->CreateSession(env, wpath.c_str(), sopt, &sess);
    api->ReleaseSessionOptions(sopt);
    if (!ort_ok(st)) { api->ReleaseEnv(env); return nullptr; }
    OrtMemoryInfo *mi = nullptr;
    if (!ort_ok(api->CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault, &mi))) {
        api->ReleaseSession(sess); api->ReleaseEnv(env); return nullptr;
    }
    auto *s = new DnnSession();
    s->env = env; s->session = sess; s->memInfo = mi;
    api->GetAllocatorWithDefaultOptions(&s->allocator);
    return s;
}

static int dnn_run(DnnSession *s, const float *input, int netW, int netH,
                   float **out0, int *out0_nd, int64_t *out0_dim,
                   float **out1, int *out1_nd, int64_t *out1_dim) {
    const OrtApi *api = ort_api();
    int64_t shape[] = {1, 3, (int64_t)netH, (int64_t)netW};
    size_t nbytes = sizeof(float) * 3 * netW * netH;
    OrtValue *in_t = nullptr, *o0 = nullptr, *o1 = nullptr;
    if (!ort_ok(api->CreateTensorWithDataAsOrtValue(s->memInfo, (void*)input, nbytes,
            shape, 4, ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, &in_t))) return 0;

    const char *in_n[]  = {"images"};
    const char *out_n[] = {"output0", "output1"};
    OrtValue *out_vals[2] = {nullptr, nullptr};
    OrtRunOptions *rop = nullptr;
    api->CreateRunOptions(&rop);
    OrtStatus *st = api->Run(s->session, rop, in_n, (const OrtValue*const*)&in_t, 1,
                              out_n, 2, out_vals);
    api->ReleaseValue(in_t);
    api->ReleaseRunOptions(rop);
    if (!ort_ok(st)) { api->ReleaseValue(out_vals[0]); api->ReleaseValue(out_vals[1]); return 0; }

    api->GetTensorMutableData(out_vals[0], (void**)out0);
    api->GetTensorMutableData(out_vals[1], (void**)out1);
    for (int k = 0; k < 2; ++k) {
        OrtTensorTypeAndShapeInfo *info = nullptr;
        api->GetTensorTypeAndShape(out_vals[k], &info);
        size_t nd = 0; api->GetDimensionsCount(info, &nd);
        api->GetDimensions(info, k==0?out0_dim:out1_dim, nd);
        *(k==0?out0_nd:out1_nd) = (int)nd;
        api->ReleaseTensorTypeAndShapeInfo(info);
        api->ReleaseValue(out_vals[k]);
    }
    return 1;
}

static void dnn_free(DnnSession *s) {
    if (!s) return;
    const OrtApi *api = ort_api();
    if (s->session) api->ReleaseSession(s->session);
    if (s->memInfo) api->ReleaseMemoryInfo(s->memInfo);
    if (s->env)     api->ReleaseEnv(s->env);
    delete s;
}

#elif NGD_DNN_BACKEND == 1
/* -------------------- MNN backend (clipsrc v3.1.0 pattern) --------- */
#include <MNN/Interpreter.hpp>
#include <MNN/Tensor.hpp>

struct DnnSession {
    std::shared_ptr<MNN::Interpreter> net;
    MNN::Session *session = nullptr;
    std::vector<float> out0_buf, out1_buf;  /* persistent output copies */
    int64_t out0_dim[4], out1_dim[4];
    int out0_nd, out1_nd;
};

static DnnSession* dnn_load(const char *path, int /*netW*/, int /*netH*/) {
    std::string load_path(path);
    fprintf(stderr, "Yolov8Seg MNN: loading %s ...\n", load_path.c_str());

    auto net = std::shared_ptr<MNN::Interpreter>(
        MNN::Interpreter::createFromFile(load_path.c_str()));
    if (!net && load_path.find(".onnx") != std::string::npos) {
        std::string mnn_path = load_path;
        size_t pos = mnn_path.rfind(".onnx");
        if (pos != std::string::npos) mnn_path.replace(pos, 5, ".mnn");
        fprintf(stderr, "Yolov8Seg MNN: retry with %s ...\n", mnn_path.c_str());
        net.reset(MNN::Interpreter::createFromFile(mnn_path.c_str()));
    }
    if (!net) {
        fprintf(stderr, "Yolov8Seg MNN: createFromFile NULL\n");
        return nullptr;
    }

    MNN::ScheduleConfig cfg;
    cfg.type      = MNN_FORWARD_CPU;
    /* 板端 (uclibc malloc 线程安全性未知): 默认单线程, NGD_MNN_THREADS 可调.
     * 多线程曾致堆损坏 (ngd_node_push realloc 后 vKeys 垃圾 -> SIGSEGV, PC glibc 不复现). */
    {
        const char *nt = getenv("NGD_MNN_THREADS");
        cfg.numThread = nt ? atoi(nt) : 1;
        if (cfg.numThread < 1) cfg.numThread = 1;
    }
    MNN::Session *sess = net->createSession(cfg);
    if (!sess) {
        fprintf(stderr, "Yolov8Seg MNN: createSession NULL\n");
        return nullptr;
    }

    auto *s = new DnnSession();
    s->net = net; s->session = sess;
    fprintf(stderr, "Yolov8Seg MNN: model loaded successfully\n");
    return s;
}

static int dnn_run(DnnSession *s, const float *input, int netW, int netH,
                   float **out0, int *out0_nd, int64_t *out0_dim,
                   float **out1, int *out1_nd, int64_t *out1_dim) {
    /* --- input (clipsrc pattern: setType + copyFromHostTensor) --- */
    auto in_t = s->net->getSessionInput(s->session, nullptr);
    if (!in_t) { fprintf(stderr, "MNN: getSessionInput NULL\n"); return 0; }

    /* Only resize if shape changed (clipsrc lines 67-81) */
    std::vector<int> target = {1, 3, netH, netW};
    bool need_resize = (in_t->dimensions() != (int)target.size());
    if (!need_resize) {
        for (int i = 0; i < (int)target.size(); ++i)
            if (in_t->length(i) != target[i]) { need_resize = true; break; }
    }
    if (need_resize) {
        s->net->resizeTensor(in_t, target);
        s->net->resizeSession(s->session);
    }

    MNN::Tensor host_in(in_t, MNN::Tensor::CAFFE);
    halide_type_t f32; f32.code = halide_type_float; f32.bits = 32; f32.lanes = 1;
    host_in.setType(static_cast<int>(f32.code));
    size_t nbytes = sizeof(float) * 3 * netH * netW;
    ::memcpy(host_in.host<void>(), input, nbytes);
    in_t->copyFromHostTensor(&host_in);

    /* --- run --- */
    s->net->runSession(s->session);

    /* --- output (clipsrc pattern: copyToHostTensor) --- */
    MNN::Tensor *o0 = s->net->getSessionOutput(s->session, "output0");
    if (!o0) o0 = s->net->getSessionOutput(s->session, nullptr);
    MNN::Tensor *o1 = s->net->getSessionOutput(s->session, "output1");
    if (!o0 || !o1) {
        fprintf(stderr, "MNN: output NULL\n"); return 0;
    }

    MNN::Tensor host_o0(o0, MNN::Tensor::CAFFE);
    MNN::Tensor host_o1(o1, MNN::Tensor::CAFFE);
    o0->copyToHostTensor(&host_o0);
    o1->copyToHostTensor(&host_o1);

    /* Copy to persistent buffers (host_o0/o1 are stack locals, destroyed on return) */
    size_t n0 = (size_t)host_o0.elementSize();
    size_t n1 = (size_t)host_o1.elementSize();
    s->out0_buf.assign(host_o0.host<float>(), host_o0.host<float>() + n0);
    s->out1_buf.assign(host_o1.host<float>(), host_o1.host<float>() + n1);

    *out0 = s->out0_buf.data();
    *out1 = s->out1_buf.data();
    auto s0 = host_o0.shape();  *out0_nd = (int)s0.size();
    auto s1 = host_o1.shape();  *out1_nd = (int)s1.size();
    for (size_t i = 0; i < s0.size(); ++i) out0_dim[i] = s->out0_dim[i] = s0[i];
    for (size_t i = 0; i < s1.size(); ++i) out1_dim[i] = s->out1_dim[i] = s1[i];
    s->out0_nd = *out0_nd; s->out1_nd = *out1_nd;
    { static int once=0; if(!once){ once=1;
        fprintf(stderr, "MNN out0: ["); for(size_t i=0;i<s0.size();++i) fprintf(stderr,"%lld ",(long long)s0[i]);
        fprintf(stderr,"] out1: ["); for(size_t i=0;i<s1.size();++i) fprintf(stderr,"%lld ",(long long)s1[i]);
        fprintf(stderr,"]\n");
    }}
    return 1;
}

static void dnn_free(DnnSession *s) {
    if (!s) return;
    if (s->net && s->session) s->net->releaseSession(s->session);
    delete s;
}

#elif NGD_DNN_BACKEND == 2
/* -------------------- fy01p NPU backend (NNP318, uint8 NCHW tensor, 无 AIPP) -- */
#include "backend/subgraph_pool.h"

struct DnnSession {
    qb::SubGraph *graph = nullptr;
    std::string   bin;               /* net_combine_no_list.bin 路径 */
    std::vector<float> out0_buf, out1_buf;  /* 持久化输出副本 (MMZ 视图下次 run 失效) */
};

static DnnSession* dnn_load(const char *path, int /*netW*/, int /*netH*/) {
    if (!qb::npu_system_init()) {
        fprintf(stderr, "Yolov8Seg NPU: FY_NPU_SysInit failed\n");
        return nullptr;
    }
    auto *s = new DnnSession();
    /* 模型: path 的同名 .bin (make_netbin 产物 net_combine_no_list.bin) */
    s->bin = path;
    size_t dot = s->bin.rfind('.');
    if (dot != std::string::npos) s->bin.resize(dot);
    s->bin += ".bin";
    s->graph = new qb::SubGraph();
    fprintf(stderr, "Yolov8Seg NPU: model=%s (lazy create on first frame, uint8 tensor)\n",
            s->bin.c_str());
    return s;
}

/* 喂 uint8 NCHW RGB tensor (软件 letterbox+blob 已做好), 无 AIPP */
static int dnn_run_u8(DnnSession *s, const uint8_t *u8_blob,
                      float **out0, int *out0_nd, int64_t *out0_dim,
                      float **out1, int *out1_nd, int64_t *out1_dim) {
    if (!s || !s->graph) return 0;
    if (!s->graph->ready()) {
        if (!s->graph->create(s->bin)) {
            fprintf(stderr, "Yolov8Seg NPU: create failed\n");
            return 0;
        }
    }
    if (!s->graph->run_u8({u8_blob})) {
        fprintf(stderr, "Yolov8Seg NPU: run_u8 failed\n");
        return 0;
    }

    /* 拷贝输出到持久化缓冲 (output() 零拷贝指针在下一次 run 失效) */
    size_t n0 = s->graph->output_size(0);
    size_t n1 = s->graph->output_size(1);
    s->out0_buf.assign(s->graph->output(0), s->graph->output(0) + n0);
    s->out1_buf.assign(s->graph->output(1), s->graph->output(1) + n1);
    *out0 = s->out0_buf.data();
    *out1 = s->out1_buf.data();

    *out0_nd = s->graph->output_nd(0);
    *out1_nd = s->graph->output_nd(1);
    s->graph->output_dims(0, out0_dim, 4);
    s->graph->output_dims(1, out1_dim, 4);
    return 1;
}

static void dnn_free(DnnSession *s) {
    if (!s) return;
    delete s->graph;
    delete s;
}

#elif NGD_DNN_BACKEND == 3
/* -------------------- xc01 NPU backend (NNP310, TY_NPU runtime, uint8 NCHW tensor, 无 AIPP) -- */
/* 运行时源: /DEngine/projects/qwen_xc01_mul/board_qrun (backend/subgraph_pool.cpp +
 * npu_runtime.c + common/mmz.c, 经 -I 解析 include "backend/subgraph_pool.h").
 * SubGraph 接口与 backend==2 (fy01p/qwen_board) 同名同构, 仅实现换成
 * TY_NPU 系 API 与 FH_SYS_Vmm64 系 MMZ 调用. */
#include "backend/subgraph_pool.h"

struct DnnSession {
    qb::SubGraph *graph = nullptr;
    std::string   bin;               /* net_combine_no_list.bin 路径 */
    std::vector<float> out0_buf, out1_buf;  /* 持久化输出副本 (MMZ 视图下次 run 失效) */
    size_t u8_blob_bytes = 0;        /* 3*netH*netW: uint8 契约基准 (float 契约 = 4x) */
    bool   float_input = false;      /* 首帧 create 后按 input_bytes 判定 */
};

static DnnSession* dnn_load(const char *path, int netW, int netH) {
    if (!qb::npu_system_init()) {
        fprintf(stderr, "Yolov8Seg NPU: TY_NPU_SysInit failed\n");
        return nullptr;
    }
    auto *s = new DnnSession();
    s->u8_blob_bytes = (size_t)3 * netW * netH;
    /* 模型: path 的同名 .bin (make_netbin 产物 net_combine_no_list.bin) */
    s->bin = path;
    size_t dot = s->bin.rfind('.');
    if (dot != std::string::npos) s->bin.resize(dot);
    s->bin += ".bin";
    s->graph = new qb::SubGraph();
    fprintf(stderr, "Yolov8Seg NPU(nnp310): model=%s (lazy create on first frame, uint8/float32 auto)\n",
            s->bin.c_str());
    return s;
}

/* 喂 uint8 NCHW RGB tensor (软件 letterbox+blob 已做好), 无 AIPP */
static int dnn_run_u8(DnnSession *s, const uint8_t *u8_blob,
                      float **out0, int *out0_nd, int64_t *out0_dim,
                      float **out1, int *out1_nd, int64_t *out1_dim) {
    if (!s || !s->graph) return 0;
    if (!s->graph->ready()) {
        if (!s->graph->create(s->bin)) {
            fprintf(stderr, "Yolov8Seg NPU(nnp310): create failed\n");
            return 0;
        }
        /* 模型输入 dtype 由 netbin 自带 blob desc 决定; 非 uint8 则 input_bytes 偏大,
         * run_u8 的 memcpy 会越界读 u8 blob — 在此精确拦截 */
        if (s->graph->input_bytes(0) != s->u8_blob_bytes) {
            fprintf(stderr, "Yolov8Seg NPU(nnp310): input bytes %zu != u8 blob %zu "
                            "(model input not uint8 tensor?)\n",
                    s->graph->input_bytes(0), s->u8_blob_bytes);
            return 0;
        }
    }
    if (!s->graph->run_u8({u8_blob})) {
        fprintf(stderr, "Yolov8Seg NPU(nnp310): run_u8 failed\n");
        return 0;
    }

    /* 拷贝输出到持久化缓冲 (output() 零拷贝指针在下一次 run 失效) */
    size_t n0 = s->graph->output_size(0);
    size_t n1 = s->graph->output_size(1);
    s->out0_buf.assign(s->graph->output(0), s->graph->output(0) + n0);
    s->out1_buf.assign(s->graph->output(1), s->graph->output(1) + n1);
    *out0 = s->out0_buf.data();
    *out1 = s->out1_buf.data();

    *out0_nd = s->graph->output_nd(0);
    *out1_nd = s->graph->output_nd(1);
    s->graph->output_dims(0, out0_dim, 4);
    s->graph->output_dims(1, out1_dim, 4);
    return 1;
}

/* nnp310 双契约: netbin 输入可为 uint8 tensor (make_netbin 路线) 或 float32 norm
 * (tyassist enable_aipp=false 路线, /255 进图)。首帧 create 后按 input_bytes 判定,
 * 两种 blob 由调用方备好, 此处择一喂入。 */
static int dnn_run_npu(DnnSession *s, const uint8_t *u8_blob, const float *f32_blob,
                       float **out0, int *out0_nd, int64_t *out0_dim,
                       float **out1, int *out1_nd, int64_t *out1_dim) {
    if (!s || !s->graph) return 0;
    if (!s->graph->ready()) {
        if (!s->graph->create(s->bin)) {
            fprintf(stderr, "Yolov8Seg NPU(nnp310): create failed\n");
            return 0;
        }
        size_t ib = s->graph->input_bytes(0);
        if (ib == s->u8_blob_bytes)            s->float_input = false;
        else if (ib == s->u8_blob_bytes * 4)   s->float_input = true;
        else {
            fprintf(stderr, "Yolov8Seg NPU(nnp310): input bytes %zu matches neither "
                            "uint8 (%zu) nor float32 (%zu)\n",
                    ib, s->u8_blob_bytes, s->u8_blob_bytes * 4);
            return 0;
        }
        fprintf(stderr, "Yolov8Seg NPU(nnp310): input contract %s (%zu bytes)\n",
                s->float_input ? "float32 norm" : "uint8 tensor", ib);
    }
    bool ok = s->float_input ? s->graph->run({f32_blob})
                             : s->graph->run_u8({u8_blob});
    if (!ok) {
        fprintf(stderr, "Yolov8Seg NPU(nnp310): run%s failed\n", s->float_input ? "" : "_u8");
        return 0;
    }

    size_t n0 = s->graph->output_size(0);
    size_t n1 = s->graph->output_size(1);
    s->out0_buf.assign(s->graph->output(0), s->graph->output(0) + n0);
    s->out1_buf.assign(s->graph->output(1), s->graph->output(1) + n1);
    *out0 = s->out0_buf.data();
    *out1 = s->out1_buf.data();

    *out0_nd = s->graph->output_nd(0);
    *out1_nd = s->graph->output_nd(1);
    s->graph->output_dims(0, out0_dim, 4);
    s->graph->output_dims(1, out1_dim, 4);
    return 1;
}

static void dnn_free(DnnSession *s) {
    if (!s) return;
    delete s->graph;
    delete s;
}

#else
#error "NGD_DNN_BACKEND must be 0 (onnxruntime), 1 (mnn), 2 (fy01p NPU) or 3 (xc01 NPU)"
#endif

/* ===================================================================
 *  Yolov8SegDetector (common code, backend-agnostic)
 * =================================================================== */

Yolov8SegDetector::Yolov8SegDetector() {}
Yolov8SegDetector::~Yolov8SegDetector()
{
    dnn_free(dnn_);
    dnn_ = nullptr;
    free(blob_); free(u8_blob_); free(letterbox_buf_); free(resize_tmp_);
    free(ffull_buf_); free(u8_buf_); free(crop_buf_);
    free(dilate_tmp_);
    free(out0_); free(out1_);
}

bool Yolov8SegDetector::init(const std::string &onnxPath, bool /*useCuda*/)
{
    ok_ = false;

    dnn_ = dnn_load(onnxPath.c_str(), netW_, netH_);
    if (!dnn_) return false;

    int npix_net = netW_ * netH_;
    blob_          = (float*)malloc(sizeof(float) * 3 * npix_net);
    u8_blob_       = (uint8_t*)malloc(sizeof(uint8_t) * 3 * npix_net);
    letterbox_buf_ = (uint8_t*)malloc(3 * npix_net);
    ffull_buf_     = (float*)malloc(sizeof(float) * npix_net);
    u8_buf_        = (uint8_t*)malloc(npix_net);
    crop_buf_      = (uint8_t*)malloc(npix_net);
    dilate_tmp_    = nullptr;
    dilate_sz_     = 0;
    /* output buffers: allocated but dnn_run returns ORT-managed pointers.
     * For MNN, output data lives inside MNN tensors (don't free). */
    out0_ = out1_ = nullptr;

    ok_ = true;
#if NGD_DNN_BACKEND == 0
    fprintf(stderr, "Yolov8SegDetector: ONNX Runtime loaded (%s)\n", onnxPath.c_str());
#elif NGD_DNN_BACKEND == 2
    fprintf(stderr, "Yolov8SegDetector: NPU loaded (%s)\n", onnxPath.c_str());
#elif NGD_DNN_BACKEND == 3
    fprintf(stderr, "Yolov8SegDetector: NPU nnp310 loaded (%s)\n", onnxPath.c_str());
#else
    fprintf(stderr, "Yolov8SegDetector: MNN loaded (%s)\n", onnxPath.c_str());
#endif
    return true;
}

int Yolov8SegDetector::detect(const uint8_t *imBGR, const float * /*imDepthMeters*/,
                               int W, int H, uint8_t *mask_out)
{
    if (!ok_ || !dnn_) return 0;
    if (!blob_ || !letterbox_buf_ || !ffull_buf_ || !u8_buf_ || !crop_buf_) {
        fprintf(stderr, "Yolov8Seg detect: NULL buffer\n"); return 0;
    }
#if NGD_DNN_BACKEND == 2 || NGD_DNN_BACKEND == 3
    if (!u8_blob_) {
        fprintf(stderr, "Yolov8Seg detect: NULL u8_blob_\n"); return 0;
    }
#endif
    auto t0 = std::chrono::steady_clock::now();

    /* 1. LetterBox (software, 所有后端统一) */
    float params[4];
    ngd_letterbox_u8(imBGR, W, H, 3*W, letterbox_buf_, netW_, netH_, params, 114);
    auto t1 = std::chrono::steady_clock::now();

    /* 2. blobFromImage */
#if NGD_DNN_BACKEND == 3
    /* nnp310 双契约: 两个 blob 都备好, dnn_run_npu 按 netbin input_bytes 择一 */
    ngd_blob_from_image_u8(letterbox_buf_, netW_, netH_, 3*netW_, u8_blob_, netW_, netH_);
    ngd_blob_from_image(letterbox_buf_, netW_, netH_, 3*netW_, blob_, netW_, netH_);
#elif NGD_DNN_BACKEND == 2
    ngd_blob_from_image_u8(letterbox_buf_, netW_, netH_, 3*netW_, u8_blob_, netW_, netH_);
#else
    ngd_blob_from_image(letterbox_buf_, netW_, netH_, 3*netW_, blob_, netW_, netH_);
#endif

    /* 3. DNN inference (backend-agnostic) */
    float *pOut = nullptr, *pOut1 = nullptr;
    int nd0 = 0, nd1 = 0;
    int64_t dims0[4] = {0}, dims1[4] = {0};
#if NGD_DNN_BACKEND == 3
    if (!dnn_run_npu(dnn_, u8_blob_, blob_, &pOut, &nd0, dims0, &pOut1, &nd1, dims1)) {
        fprintf(stderr, "Yolov8Seg detect: dnn_run_npu failed\n"); return 0;
    }
#elif NGD_DNN_BACKEND == 2
    if (!dnn_run_u8(dnn_, u8_blob_, &pOut, &nd0, dims0, &pOut1, &nd1, dims1)) {
        fprintf(stderr, "Yolov8Seg detect: dnn_run_u8 failed\n"); return 0;
    }
#else
    if (!dnn_run(dnn_, blob_, netW_, netH_, &pOut, &nd0, dims0, &pOut1, &nd1, dims1)) {
        fprintf(stderr, "Yolov8Seg detect: dnn_run failed\n"); return 0;
    }
#endif
    int outh0 = (int)dims0[1], outw0 = (int)dims0[2];
    int outc1 = (int)dims1[1], outh1 = (int)dims1[2], outw1 = (int)dims1[3];
    auto t2 = std::chrono::steady_clock::now();

    /* diagnostic (first frame) */
    { static int once=0; if(!once){ once=1;
        fprintf(stderr,"Yolov8Seg output0:[%lld,%lld,%lld] output1:[%lld,%lld,%lld,%lld]\n",
                (long long)dims0[0],(long long)dims0[1],(long long)dims0[2],
                (long long)dims1[0],(long long)dims1[1],(long long)dims1[2],(long long)dims1[3]);
    }}

    /* 4. decode detections + merge mask (C decoders, unchanged) */
    float outputDet[MAX_OUT_SIZE] = {0};
    /* MNN output is read-only; for ORT we already released the OrtValue.
     * The merged mask is written to a heap buffer allocated on the fly. */
    float *pOutMask = (float*)malloc(sizeof(float) * outw1 * outh1);
    int numDet = decodePosition(pOut, pOut1, outw0, outh0, outputDet, netW_, netH_, MAX_DET);
    decodeMask(pOut1, outw1, outh1, outc1, outputDet, netW_, netH_, numDet, pOutMask);
    auto t3 = std::chrono::steady_clock::now();

    /* 5. remap mask to original size */
#if defined(NGD_POST_NEON) && defined(__ARM_NEON__)
    /* 优化③ (ngd_opt_postneon.c): rs1+bin 融合 NEON, 输出逐字节一致 */
    ngd_post_resize_f32_bin_neon(pOutMask, outw1, outh1, outw1, u8_buf_, netW_, netH_);
    free(pOutMask);
#else
    ngd_resize_nearest_f32(pOutMask, outw1, outh1, outw1, ffull_buf_, netW_, netH_);
    free(pOutMask);
    {   int np = netW_ * netH_;
        for (int i = 0; i < np; ++i) {
            int v = (int)(ffull_buf_[i] * 255.0f + 0.5f);
            u8_buf_[i] = (v > 127) ? (uint8_t)255 : (uint8_t)0;
        }
    }
#endif
    /* crop letterbox content + resize to original */
    float ratio_x = params[0], ratio_y = params[1];
    int left = (int)params[2], top = (int)params[3];
    int content_w = (int)lroundf((float)W * ratio_x);
    int content_h = (int)lroundf((float)H * ratio_y);
    if (content_w > netW_ - left) content_w = netW_ - left;
    if (content_h > netH_ - top)  content_h = netH_ - top;
    memset(mask_out, 0, (size_t)W * H);
    if (content_w > 0 && content_h > 0) {
        for (int y = 0; y < content_h; ++y)
            memcpy(crop_buf_ + (size_t)y * content_w,
                   u8_buf_ + ((size_t)(y + top) * netW_ + left), (size_t)content_w);
#if defined(NGD_POST_NEON) && defined(__ARM_NEON__)
    /* 优化③ (ngd_opt_postneon.c): rs2 索引表 + 行重复 memcpy, 输出逐字节一致 */
        ngd_post_resize_u8_neon(crop_buf_, content_w, content_h, content_w, mask_out, W, H);
#else
        ngd_resize_nearest_u8(crop_buf_, content_w, content_h, content_w, mask_out, W, H);
#endif
    }

    /* 6. dilate + safety valve */
#if defined(NGD_POST_NEON) && defined(__ARM_NEON__)
    /* 优化③: 板端勘察 (probe_neon/postprof_b1) — dilate 输出 dilate_tmp_ 在
     * detect() 内无任何后继读取 (下方 count 统计的是 mask_out), 纯死计算
     * 68.5ms/帧, 宏开时跳过; mask_out 位级不变。 */
    if (ngd_post_count_nonzero_neon(mask_out, W * H) > (int)(0.8 * (double)W * (double)H))
        memset(mask_out, 0, (size_t)W * H);
#else
    if (W * H > dilate_sz_) {
        free(dilate_tmp_);
        dilate_tmp_ = (uint8_t*)malloc((size_t)W * H);
        dilate_sz_ = W * H;
    }
    ngd_mask_dilate(mask_out, dilate_tmp_, W, H, 3);
    if (ngd_count_nonzero_u8(mask_out, W * H) > (int)(0.8 * (double)W * (double)H))
        memset(mask_out, 0, (size_t)W * H);
#endif
    auto t4 = std::chrono::steady_clock::now();

    using ms = std::chrono::duration<double, std::milli>;
    t_pre_  += ms(t1 - t0).count();
    t_fwd_  += ms(t2 - t1).count();
    t_dec_  += ms(t3 - t2).count();
    t_post_ += ms(t4 - t3).count();
    n_det_++;
    return 1;
}

void Yolov8SegDetector::print_timing(int /*nFrames*/) const
{
    const double total = t_pre_ + t_fwd_ + t_dec_ + t_post_;
    const long long n = n_det_;
    fprintf(stderr,
        "------- YOLOv8-seg detect() breakdown (calls=%lld, sum=%.1f ms) -------\n"
        "%-12s %12s %14s %8s\n", n, total, "stage", "total(ms)", "per-call(ms)", "share");
    auto row = [&](const char *name, double t) {
        double pc = (n > 0) ? t / (double)n : 0.0;
        double sh = (total > 1e-9) ? 100.0 * t / total : 0.0;
        fprintf(stderr, "%-12s %12.1f %14.3f %7.1f%%\n", name, t, pc, sh);
    };
    row("preprocess", t_pre_);
    row("forward",    t_fwd_);
    row("decode",     t_dec_);
    row("postproc",   t_post_);
    fprintf(stderr, "(per-call = total/calls; share = fraction of detect() time.)\n");
}
