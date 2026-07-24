# refappv8seg — refapp 的 YOLOv8-seg 端到端 mask 变体

把 refapp 的动态 mask 计算从 **yolo-fastest-xl 检测 + 深度一致性分割 + `ngd_mask_predict_current` LK 传播**
两阶段,替换为 **YOLOv8-seg 端到端逐帧直接出 mask**。mask 下游消费(masked-ORB / OF-pose 关键点过滤)
与 CLI 完全不变,便于和 refapp 做 ATE / KF / 耗时对比。**不动任何已有工程**(refactor / refmain / refapp /
主工程 / Thirdparty),本目录自包含。

## 与 refapp 的差异(只有 mask 来源不同)

| | refapp | refappv8seg |
|---|---|---|
| 检测器 | `YoloDetector`(yolo-fastest-xl Darknet, 320×320)+ 深度一致性 person 分割 | `Yolov8SegDetector`(yolov8n-seg ONNX, 448×320)|
| mask 取得 | YOLO 出 seed → `ngd_mask_predict_current` LK 传播到当前帧 | YOLOv8-seg **直接**输出当前帧 mask(端到端,无 LK)|
| mask 阶段耗时 | ~21% (LK + DBSCAN 等)| **0 ms**(整段链移除)|
| LK 调用 | mask 传播 LK + pose 跟踪 LK(两个)| 仅 pose 跟踪 LK(mask 传播 LK 删除)|
| 深度图用途 | 深度一致性分割 + LK 深度门 | **不用于 mask**(端到端分割不需要);仅 ORB/三角化用 |

**注**:refapp 里有两个独立 LK —— (1) mask 传播 `ngd_mask_predict_current`(本变体删除);(2) pose 跟踪
`ngd_app_track_with_optical_flow`(SLAM 位姿管线,**保留不动**)。

## 端到端 mask 流程(`src/yolov8_seg.cpp`)

复刻 testblurDnn(`oldworkspace/study/gesture/testblurDnn`)的 `Yolov8Seg::Detect` 活跃解码路径,再加 letterbox
逆变换(SLAM 传全分辨率 640×480,testblurDnn 输入已预 resize,故需逆变换):

1. `LetterBox(imRGB → 448×320)` + `blobFromImage`(/255, BGR→RGB)
2. `net.forward([output0, output1])` — output0 `[1,116,N]`(4 box + 80 cls + 32 mask 系数),output1 `[1,32,80,112]`(mask proto)
3. `decodePosition`(C,person-only,conf 0.25,NMS 0.7,MAX_DET 10)→ 逐检测记录 + 32 mask 系数
4. `decodeMask`(C,系数·proto,Q15 sigmoid,按 person 合并)→ float mask 80×112
5. **mask remap**:`resize→(448,320)` → `*255`+`threshold 127`(与 testblurDnn 逐位一致)→ 裁 letterbox 内容区 → `resize` 回 640×480 → `>0` 二值
6. 后处理:`dilate 7×7` + `>80% 覆盖安全阀清零`(对齐 `YoloDetector::postprocess`)

C 解码器(`src/yolov8seg/`:`libkptdecode`/`libnms`/`libobjectpool`/`sigmoidTable_q15.h`/`outsize.h`)
逐字复制自 testblurDnn,不改。

## 目录结构

```
refappv8seg/
  CMakeLists.txt              # 仿 refapp; 编译 yolov8seg/*.c
  README.md                   # 本文件
  models/yolov8n-seg-320x448.onnx   # 从 testblurDnn/weights/ 复制
  include/ngd_app/
    system.h                  # 复制 refapp; 成员改 Yolov8SegDetector*, 删 mask_pred
    imio.h yaml.h optflow_track.h viewer.h   # 复制 refapp 不改
    yolov8_seg.h              # 新: Yolov8SegDetector 类
  src/
    imio.cpp yaml.cpp optflow_track.cpp viewer.cpp   # 复制 refapp 不改
    main.cpp                  # 复制 refapp; 默认 yolo_dir 改 ../../models
    system.cpp                # 复制 refapp; 删 LK 传播块, detect 直出 mask
    yolov8_seg.cpp            # 新: 检测 + LetterBox + decode + remap + 后处理
  src/yolov8seg/              # C 解码器(逐字复制自 testblurDnn)
```

## 构建(Windows, VS2022 x64, vcpkg OpenCV4)

前置:`refactor/myBuild/Release/ngd_core.lib` 与 `refmain/build/Release/ngd_shell.lib` 已存在
(先按 refapp 流程构建过 refactor → refmain)。

```
c:\Users\frank.tu\CMakePath.bat
set CMAKE=C:/src/vcpkg/downloads/tools/cmake-3.31.10-windows/cmake-3.31.10-windows-x86_64/bin/cmake.exe
%CMAKE% -B refappv8seg/build -S refappv8seg -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_TOOLCHAIN_FILE=C:/src/vcpkg/scripts/buildsystems/vcpkg.cmake
%CMAKE% --build refappv8seg/build --config Release
```

## 运行(CLI 与 refapp 一致,ate.py / plot_traj.py 直接可用)

```
cd refappv8seg\build\Release
set PATH=C:\src\vcpkg\installed\x64-windows\bin;%PATH%   # opencv_*.dll
rgbd_tum_app_v8seg.exe ^
  ..\..\..\Vocabulary\ORBvoc.bin ^
  ..\..\..\Examples\RGB-D\TUM3.yaml ^
  C:\Users\frank.tu\Downloads\rgbd_dataset_freiburg3_walking_xyz ^
  ..\..\..\Examples\RGB-D\associations\fr3_walk_xyz.txt ^
  ..\..\models                                          # 含 yolov8n-seg-320x448.onnx
```
输出:`CameraTrajectory.txt` / `KeyFrameTrajectory.txt` / `PoseCovariance.txt`(写在本目录)。

> **⚠️ `yolo_dir` 必须指向含 `yolov8n-seg-320x448.onnx` 的目录(= `refappv8seg/models`)。
> **切勿**传 `Thirdparty/YOLO` —— 那是旧 yolo-fastest-xl Darknet 模型目录,refappv8seg 找不到
> ONNX 会**静默关闭 mask**(stderr:`system: YOLOv8-seg model not found ...`),人体不过滤,轨迹
> 退化到 ~12 cm ATE。验证 mask 已加载:stderr 应见 `system: Yolov8SegDetector loaded (...onnx)`。
> 相对路径深度(从 `build/Release` 起):repo 根文件用 `../../../`(如 Vocabulary),`models` 用 `../../`。

## 结果(fr3_walking_xyz, 827 帧, NGD_VIEWER=0)

ATE(`refapp/ate.py`,Umeyama 对齐):

| 变体 | ATE RMSE | mean | median | max |
|---|---|---|---|---|
| **refappv8seg**(YOLOv8-seg 端到端)| **1.58 cm** | 1.37 | 1.18 | 5.50 |
| refapp(yolo-fastest + LK)| 1.53 cm | 1.31 | 1.16 | 5.74 |

**结论**:端到端 YOLOv8-seg mask 与两阶段 yolo-fastest+LK **精度等价**(1.58 vs 1.53 cm,差 0.05 cm,
小于 refapp 残余 ~1.5 cm 的确定性编排抖动),同时移除整条 mask LK 链(mask 阶段 0 ms),概念更简洁
(一个模型直接出 mask,无光流漂移补偿)。

耗时(per-frame):

| stage | per-frm | share | 说明 |
|---|---|---|---|
| yolo | 50.4 ms | 49.8% | YOLOv8-seg CPU 推理(端到端,每帧)|
| mask | **0 ms** | 0.0% | LK 传播链已删除(refapp 约 21%)|
| orb | 42.7 ms | 42.2% | masked-ORB(KF 分支 185 次)|
| of | 2.8 ms | 2.8% | OF-pose 跟踪 LK |
| lm | 4.5 ms | 4.4% | LocalMapping |
| median / mean per-frame | 53 / 101 ms | | |

52 KF 保留(60 创建)。`t_yolo` 占比从 refapp 的 ~34% 升到 ~50%(模型更大);这是端到端精度的代价。

`detect()` 内部还有逐子阶段计时(分析用,`print_timing` 输出在主表之后):

| sub-stage | per-call | share(of detect) | 说明 |
|---|---|---|---|
| preprocess | ~0.8 ms | ~1.7% | LetterBox + blobFromImage |
| **forward** | **~43.6 ms** | **~96.4%** | `net.forward`(双头 DNN 推理)|
| decode | ~0.6 ms | ~1.3% | decodePosition + decodeMask(C)|
| postproc | ~0.3 ms | ~0.6% | remap + dilate + 安全阀 |

**结论**:端到端分割的耗时几乎全在 DNN 推理(forward ~96%);C 解码/remap/dilate 合计 < 1.6 ms。提速只能靠
推理本身(CUDA 后端 `init(onnx, useCuda=true)` 或更小输入/模型),后处理无可优化。

## 已知差异 / 权衡

- **CPU 推理慢**:yolov8n-seg 320×448 OpenCV CPU ~50 ms/帧,827 帧纯推理约 35 s。如需加速:`init(onnx, useCuda=true)`
  走 CUDA 后端(需 OpenCV CUDA build);或后续上 YOLO 线程化(仿 refappthread2)。
- **无 mask LK 漂移补偿**:refapp 的 LK 传播在 YOLO 漏检帧能给一个预测 mask;本变体每帧独立分割,YOLO 漏检帧
  mask 直接为空(见运行日志 maskYolo=0 的帧,如 frame 250-300 人物出画)。ATE 表明这不影响整体精度。
- **person-only**:`JUST_KEEP_PERSON_LABEL=1`(libkptdecode.c 编译期开关)。若要扩到其它动态类,改该宏并按需放开
  `nc`/类别过滤。

## 可切换项

- **viewer**:`src/system.cpp` 顶部 `#define NGD_VIEWER 0`(headless 默认)。改 `1` 重编可看轨迹 + mask 叠加。
- **phase**:`#define NGD_PHASE 3`(OF 模式默认)。`2`=masked-ORB(无 OF),`1`=静态(无 YOLO)。
- **模型分辨率**:`src/yolov8seg/outsize.h` 的 `INPUT_WIDTH/HEIGHT` + 换对应 `.onnx`(testblurDnn 另有 640×640)。
