# refmain/ — OpenCV 薄壳 (YOLO DNN + 光流跟踪)

`refactor/` 纯 C 核心已完成 P0–P3 全部算法（33 测试 PASS），架构为「纯C核心 + OpenCV 薄壳」。本目录是 **OpenCV 薄壳**，按用户约束把 YOLO DNN / 光流 / PnP 直接交给 OpenCV，**绑定 refactor 核心**（链接 `ngd_core.lib`），不复用核心的纯 C EPnP/MLPnP。

## 与 MaskTracker / refactor 的关系

- **YOLO 检测 + mask 预测链**（YOLO DNN → 深度一致性 mask；LK+DBSCAN mask 传播）：**本地自含拷贝** `include/ngd_shell/{YoloDetector,MaskPredictor,MaskTracker}.h` + `src/*.cpp`，源自用户先前从 NGD-SLAM 剥离重构的 [`../../MaskTracker/`](../../MaskTracker/)（仅拷入 refmain 实际用到的 3 个模块，未拷 DatasetLoader/MaskOutput/main）。**不依赖** `../../MaskTracker` 目录。
- **光流 SLAM 跟踪**（MaskTracker 明确不含位姿估计）：refmain 新增 `optflow` — `SearchByOpticalFlow`（LK 点传播 + mask 过滤）+ `cv::solvePnPRansac`（TrackWithOpticalFlow 的 PnP）。
- **LK 回调桥** `lk_shell`：把核心 `refactor/include/ngd/mask.h` 的 `ngd_lk_flow_fn` 回调绑到 `cv::calcOpticalFlowPyrLK`（默认参 winSize21/maxLevel3/COUNT+EPS 30·0.01，对应 `Tracking.cc:4271`），使核心纯 C mask 链 `ngd_mask_predict_current` 可用真实 OpenCV LK。

> 即：动态 mask 可由两条等价链产生 —— 本地 `MaskPredictor`（OpenCV），或 refactor 的纯 C `mask.c`（经 `lk_shell` 注入 LK）。两者均测。

## 目录结构

```
refmain/
  include/ngd_shell/
    lk_shell.h        — ngd_lk_flow_fn 回调（绑 cv::calcOpticalFlowPyrLK）
    optflow.h         — SearchByOpticalFlow + cv::solvePnPRansac（C API，扁平数组）
    YoloDetector.h    } 本地自含拷贝（源自 MaskTracker，剥离自 NGD-SLAM YOLO.cc）
    MaskPredictor.h   }
    MaskTracker.h     }
  src/
    lk_shell.cpp / optflow.cpp          — refmain 原创
    YoloDetector.cpp / MaskPredictor.cpp / MaskTracker.cpp   — 本地自含拷贝
  tests/
    test_lk_shell.cpp   — LK 恢复已知位移 + 真实 OpenCV LK 驱动核心 mask 链
    test_optflow.cpp    — SearchByOpticalFlow + PnP 恢复已知位姿
    test_yolo_mask.cpp  — 本地 YoloDetector 跑 yolo-fastest-xl on bus.jpg
  CMakeLists.txt
```

全部对外 API `extern "C"`（C 核心可取函数指针 / C demo 可调）。`optflow` 不依赖 `ngd_core`（扁平数组）；`lk_shell` 仅 include `ngd/mask.h` 取 typedef；仅 `test_lk_shell` 链 `ngd_core`（驱动 mask 链）。

## API 速查

```c
/* lk_shell.h —— 核心 mask 链的 LK 注入点 */
int ngd_shell_lk_default(const uint8_t *prev, const uint8_t *curr,
                         int w, int h, int stride,
                         const float *pts_in, int n,
                         float *pts_out, uint8_t *status, void *user);

/* optflow.h —— 光流 SLAM 跟踪 */
int ngd_shell_search_by_optical_flow(const uint8_t *last_gray, const uint8_t *curr_gray,
                                     const uint8_t *mask, int w, int h, int stride,
                                     const float *last_keys, const int *last_mp_valid, int n_last,
                                     float *out_keys, int *out_mp_idx);
int ngd_shell_pnp_ransac(const float *mapPoints, const float *imgPoints, int n,
                         const float K[9], const float *dist,
                         const float *init_Tcw, int iter, float reprojErr, float conf,
                         int *inliers_out, float out_Tcw[16]);
```
YOLO 走 C++ 类 `YoloDetector`（MaskTracker），`detect(imRGB, imDepthMeters) -> cv::Mat mask`。

## 忠实性

- **SearchByOpticalFlow** 逐行对应 `ORBmatcher.cc:2016-2082`：取 last 的 keypoint+MP(Observations≥3)→`calcOpticalFlowPyrLK`→`status`+边界 `[0,w)×[0,h)`+`mask==0` 门→写 out。
- **PnP** 对应 `Tracking.cc:3025-3053`：`cv::solvePnPRansac`（SOLVEPNP_ITERATIVE, 100/5/0.99），`init_Tcw` 可选种子（Rodrigues→rvec/tvec）。**注意**：solvePnPRansac 输出 rvec/tvec 可能是 CV_64F，须 `convertTo(CV_32F)` 后读，否则 `.at<float>` 读 double 得乱码平移（本工程已修）。
- **YOLO** 对应 `YOLO.cc`（class 0=person，深度一致性 ±0.33m，最大连通域，dilate，80% 清零）—— 见 MaskTracker。
- **光流走 OpenCV**（用户约束）：`calcOpticalFlowPyrLK` + `solvePnPRansac` 均用 cv，**不**复用核心纯 C EPnP/MLPnP。

## 构建（Windows / MSVC）

先确保 `refactor/myBuild/Release/ngd_core.lib` 存在（refactor 已构建），再：

```bat
cd NGD-SLAM\refmain
c:\Users\frank.tu\CMakePath.bat
set CMAKE=C:/src/vcpkg/downloads/tools/cmake-3.31.10-windows/cmake-3.31.10-windows-x86_64/bin/cmake.exe
%CMAKE% -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=C:/src/vcpkg/scripts/buildsystems/vcpkg.cmake
%CMAKE% --build build --config Release
```

需 vcpkg OpenCV4（含 dnn/video/calib3d/imgproc/imgcodecs/highgui）+ Protobuf（dnn 依赖）。`-DCMAKE_TOOLCHAIN_FILE=.../vcpkg.cmake` 一次性解析全部依赖（`-DOpenCV_DIR` 不够，会漏 Protobuf）。

运行 exe 需 vcpkg OpenCV DLL 在 PATH：`set PATH=C:\src\vcpkg\installed\x64-windows\bin;%PATH%`。

CMake 变量：`NGD_CORE_INCLUDE`（默认 `../refactor/include`）、`NGD_CORE_LIB`（默认 `../refactor/myBuild/Release/ngd_core.lib`，内部转绝对路径避免 MSVC `.lib.lib` 重名）。无任何 `../../MaskTracker` 依赖。

## 验证（3 demo，全 PASS）

| 测试 | 结果 | 关键指标 |
|---|---|---|
| `test_lk_shell` | PASS | LK 恢复位移 (5.00,3.00) 96/96 点；真实 OpenCV LK 驱动核心 mask 链 → 13673px mask，质心 (105,87)≈seed(100,90)+shift(8,5) |
| `test_optflow` | PASS | SearchByOpticalFlow 40/40 + solvePnPRansac 40 内点，rot_err=0.000°, \|dt\|=0.0086 |
| `test_yolo_mask` | PASS | yolo-fastest-xl 跑通 bus.jpg，person mask 292623 px（人检测） |

构建干净：**0 error / 0 warning**（本地 MaskTracker-源文件的 C4244 int↔float 已 target 级 `/wd4244` 抑制）。
