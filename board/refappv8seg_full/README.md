# refappv8seg_full — 零 OpenCV 依赖的 NGD-SLAM 测试工程

`refappv8seg` 的完全无 OpenCV 变体。动态 mask 由 YOLOv8-seg 端到端产生，
支持 **ONNX Runtime** 和 **MNN** 双 DNN 推理后端，通过编译宏一键切换。

## 与 refappv8seg 的差异

| | refappv8seg | refappv8seg_full |
|---|---|---|
| 图像 I/O | `cv::imread` | `stb_image`（单头文件） |
| YAML 解析 | `cv::FileStorage` | 纯 C 行解析器 |
| DNN 推理 | `cv::dnn::Net`（OpenCV） | ONNX Runtime 或 MNN（宏切换） |
| 颜色转换 | `cv::cvtColor` | `bgr_to_gray`（纯 C BT.601） |
| 可视化 | `cv::imshow`（交互窗口） | PNG 帧输出到 `viewer_frames/` |
| 光流 / PnP | OpenCV `solvePnPRansac` | 纯 C EPnP + LM refine |
| OpenCV 依赖 | 10 个 DLL | **0** |

## 目录结构

```
refappv8seg_full/
├── CMakeLists.txt
├── README.md
├── include/ngd_app/
│   ├── system.h, viewer.h, yaml.h, yolov8_seg.h, ...
│   └── image_types.h          # 纯 C 图像描述符
├── src/
│   ├── main.cpp                # 入口（stb_image 加载 PNG）
│   ├── system.cpp              # SLAM 驱动（零 OpenCV）
│   ├── yolov8_seg.cpp          # YOLO 检测器（双后端）
│   ├── optflow_track.cpp       # 光流跟踪（纯 C LK+EPnP）
│   ├── viewer.cpp              # PNG 帧输出
│   ├── color_convert.c         # BGR→Gray
│   ├── image_proc.c            # resize/threshold/blobFromImage/letterbox
│   ├── imio_pure.c             # stb_image 封装
│   ├── yaml_pure.c             # 纯 C YAML 解析
│   ├── stb_image_impl.c        # stb 实现
│   ├── yolov8seg/              # C 解码器（testblurDnn 逐字复制）
│   └── stb/                    # stb_image.h + stb_image_write.h
├── models/
│   ├── yolov8n-seg-320x448.onnx # YOLOv8-seg ONNX 模型
│   └── yolov8n-seg-320x448.mnn  # MNN 转换后的模型
└── thirdparty/
    └── MNN310/                  # MNN v3.1.0（仅 MNN 后端需要）
```

## 构建

### 前置条件

- Visual Studio 2022 x64
- CMake 3.15+（vcpkg 自带）
- `refactor/myBuild/Release/ngd_core.lib`（纯 C SLAM 核心）
- ONNX Runtime（从 GitHub 下载预编译包）或 MNN v3.1.0

### ONNX Runtime 后端（默认）

```bash
# 1. 下载 onnxruntime（若未安装）
# https://github.com/microsoft/onnxruntime/releases/download/v1.20.1/onnxruntime-win-x64-1.20.1.zip
# 解压到 C:/src/onnxruntime/v1.20.1/

# 2. 编译
cmake -B build -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_TOOLCHAIN_FILE=C:/src/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

### MNN 后端

```bash
# 1. 编译 MNN（已在 thirdparty/MNN310/，已预编译）
# 若需重新编译：
cd thirdparty/MNN310
cmake -B build -G "Visual Studio 17 2022" -A x64 ^
    -DMNN_BUILD_SHARED_LIBS=ON -DMNN_OPENCL=OFF -DMNN_OPENGL=OFF ^
    -DMNN_VULKAN=OFF -DMNN_CUDA=OFF
cmake --build build --config Release -- /m:1

# 2. 转换 ONNX → MNN 模型（见下方"模型转换"）

# 3. 编译（指定 MNN 后端）
cmake -B build -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_TOOLCHAIN_FILE=C:/src/vcpkg/scripts/buildsystems/vcpkg.cmake ^
    -DNGD_DNN_BACKEND=1
cmake --build build --config Release
```

### Linux ARM 交叉编译（fhva12c）

工具链路径：`/opt/fullhan/toolchain/arm-fhva12c-linux-uclibcgnueabihf-b5/`

**1. 交叉编译 MNN v3.1.0**

```bash
cd thirdparty/MNN310
rm -rf build_arm
cmake -B build_arm \
    -DCMAKE_SYSTEM_NAME=Linux \
    -DCMAKE_SYSTEM_PROCESSOR=arm \
    -DCMAKE_C_COMPILER=/opt/fullhan/toolchain/arm-fhva12c-linux-uclibcgnueabihf-b5/bin/arm-linux-uclibcgnueabihf-gcc \
    -DCMAKE_CXX_COMPILER=/opt/fullhan/toolchain/arm-fhva12c-linux-uclibcgnueabihf-b5/bin/arm-linux-uclibcgnueabihf-g++ \
    -DCMAKE_BUILD_TYPE=Release \
    -DMNN_BUILD_SHARED_LIBS=ON \
    -DMNN_OPENCL=OFF -DMNN_OPENGL=OFF -DMNN_VULKAN=OFF -DMNN_CUDA=OFF
cmake --build build_arm -- -j$(nproc)
# 产出: build_arm/libMNN.so
```

**2. 交叉编译 refappv8seg_full**

```bash
cd refappv8seg_full
rm -rf build_arm

# 写 toolchain 文件
cat > arm_fhva12c.cmake << 'EOF'
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(TOOLCHAIN_ROOT /opt/fullhan/toolchain/arm-fhva12c-linux-uclibcgnueabihf-b5)
set(CMAKE_C_COMPILER   ${TOOLCHAIN_ROOT}/bin/arm-linux-uclibcgnueabihf-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_ROOT}/bin/arm-linux-uclibcgnueabihf-g++)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
EOF

cmake -B build_arm \
    -DCMAKE_TOOLCHAIN_FILE=arm_fhva12c.cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DNGD_DNN_BACKEND=1 \
    -DNGD_CORE_INCLUDE=../refactor/include \
    -DNGD_CORE_LIB=../refactor/myBuild_arm/Release/libngd_core.a
cmake --build build_arm -- -j$(nproc)
# 产出: build_arm/rgbd_tum_app_v8seg_full
```

**3. 部署到 ARM 设备**

```bash
# 拷贝到目标设备
scp build_arm/rgbd_tum_app_v8seg_full           target:/app/
scp thirdparty/MNN310/build_arm/libMNN.so       target:/app/
scp models/yolov8n-seg-320x448.onnx              target:/app/models/
scp Vocabulary/ORBvoc.txt                        target:/app/

# 目标设备上运行
export LD_LIBRARY_PATH=/app:$LD_LIBRARY_PATH
cd /app
./rgbd_tum_app_v8seg_full ORBvoc.txt TUM3.yaml /data/sequence /data/assoc.txt models/
```

> **注：** `ngd_core` 库也需要用同一工具链交叉编译（`refactor/myBuild_arm/`），
> 并改 CMake 中 `NGD_CORE_LIB` 指向 ARM 版本。ONNX 模型文件与架构无关，可直接复用。

```

## 模型转换（ONNX → MNN）

使用 Python MNN 包转换：

```bash
pip install mnn aliyun-log-python-sdk

python -m MNN.tools.mnnconvert ^
    -f ONNX ^
    --modelFile models/yolov8n-seg-320x448.onnx ^
    --MNNModel models/yolov8n-seg-320x448.mnn ^
    --bizCode test
```

输出 `models/yolov8n-seg-320x448.mnn`。MNN 后端会自动尝试 `.mnn` 文件（先试 `.onnx`，失败则回退 `.mnn`）。

## 运行

```bash
cd build
Release\rgbd_tum_app_v8seg_full.exe ^
    ..\..\..\Vocabulary\ORBvoc.txt ^
    ..\..\..\Examples\RGB-D\TUM3.yaml ^
    C:\Users\frank.tu\Downloads\rgbd_dataset_freiburg3_walking_xyz ^
    ..\..\..\Examples\RGB-D\associations\fr3_walk_xyz.txt ^
    ..\models
```

输出：`CameraTrajectory.txt`、`KeyFrameTrajectory.txt`、`PoseCovariance.txt`

## 编译选项

| 宏 | 位置 | 默认值 | 说明 |
|---|---|---|---|
| `NGD_DNN_BACKEND` | CMakeLists.txt / cmake -D | 0 | 0=ONNX Runtime, 1=MNN |
| `NGD_VIEWER` | src/system.cpp 顶部 | 0 | 0=headless, 1=输出 PNG 帧到 `viewer_frames/` |
| `NGD_PHASE` | src/system.cpp 顶部 | 3 | 1=静态, 2=masked-ORB, 3=OF 模式 |

### 切换后端

```bash
# ONNX Runtime
cmake -B build -DNGD_DNN_BACKEND=0 ...

# MNN
cmake -B build -DNGD_DNN_BACKEND=1 ...
```

切换后需 `cmake --build build --config Release` 重新编译。

## ATE 结果（fr3_walking_xyz, 827 帧）

| 后端 | ATE RMSE | KFs | ms/帧 |
|---|---|---|---|
| ONNX Runtime v1.20.1 | **1.43 cm** | 98 | 252 |
| MNN v3.1.0 | **1.43 cm** | 98 | 245 |

两个后端 ATE 完全一致，MNN 略快 3%。

### 耗时分解（per-frame）

| 阶段 | ORT | MNN | 说明 |
|---|---|---|---|
| yolo | 29.5ms | 26.2ms | YOLOv8-seg 推理+后处理 |
| of | 33.5ms | 28.8ms | 光流跟踪 |
| orb | 170ms | 171ms | masked-ORB（KF 分支） |
| track | 3.6ms | 3.6ms | 跟踪 |
| lm | 15.1ms | 15.5ms | 局部建图 |
| **median total** | **252ms** | **245ms** | |

## MNN 编译踩坑备忘

- MNN v2.9.0 有 session 重用 bug（`Can't run session because not resized`），**必须用 v3.1.0**
- MSBuild 多线程编译有 PDB 锁竞争，用 `/m:1` 单线程编译
- MNN 的 `copyFromHostTensor` + `copyToHostTensor` 是标准输入输出方式（不要直接写 `host<float>()`）
- MNN DLL 会自动拷贝到 exe 目录（CMake POST_BUILD）

## 依赖关系

```
refappv8seg_full.exe
├── ngd_core.lib            # 纯 C SLAM 核心（refactor/）
├── stb_image.h             # 图像 I/O（header-only, 已内置 src/stb/）
├── ONNX Runtime DLL        # 仅 ORT 后端
│   └── onnxruntime.dll     # C:/src/onnxruntime/... (用户自行下载)
└── MNN DLL                 # 仅 MNN 后端
    └── MNN.dll             # thirdparty/MNN310/build/Release/ (项目自带, 已预编译)

## 零依赖：
- OpenCV ❌
- Eigen ❌
- g2o ❌
- Pangolin ❌
- protobuf ❌
```
