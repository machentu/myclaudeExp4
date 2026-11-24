# HANDOFF — YOLOv8-seg + ORB-SLAM 板端部署 (FY01P/NNP318)

**日期**: 2026-08-27  
**状态**: YOLOv8-seg NPU 推理通过，25 帧完整 SLAM 跑通。ORB 瓶颈待优化，100 帧待测。

---

## 1. 项目概述

将 ORB-SLAM3 + YOLOv8-seg 动态 mask 系统部署到云天 FY01P 板端 (NNP318 NPU, Cortex-A7, 256MB RAM + 256MB MMZ)。

**代码仓库**: `/DEngine/projects/slampart/refappv8seg_full/`  
**板端 NFS**: `/home/frank/nfs/fy01p/slampart/` (板端挂载 `/mnt/fy01p/slampart/`)  
**模型转换**: `/home/frank/workspace/tianxuan/myProject/yolov8segrelay/`  
**NPU 运行时**: `/DEngine/projects/qwen_board/`  

---

## 2. 已完成

### 2.1 YOLOv8-seg NPU 推理 (uint8 tensor, 无 AIPP)

**问题**: NNP318 AIPP 硬件不支持 BGR/RGB_888_PACKED + resize letterbox。两次板端测试均 `IOC_CMD_AIPP_NNP err 19403817` 崩溃。

**方案**: 关闭 AIPP，走 uint8 NCHW tensor 输入：
- 软件 letterbox + BGR→RGB NCHW uint8 blob (0-255)
- 模型量化: `get_quantize_config(TARGET, {"images": "uint8"})`，内置 dequantize op (scale=1/255)
- `make_netbin` 输入: `{"images": {"layout": "NCHW"}}`（关闭 AIPP/CR 模块）
- `SubGraph::create()` + `run_u8()` 直接喂 uint8 tensor

**板端实测** (25 帧):
| 指标 | 数值 |
|------|------|
| NPU forward | **29.0ms/帧** |
| 预处理 | 5.2ms/帧 |
| 解码 | 9.7ms/帧 |
| 后处理 | 84.0ms/帧 |
| YOLO 总计 | 128ms/帧 |

### 2.2 完整 SLAM 管线

25 帧序列板端通过 (rc=0, 25/25 跟踪成功):

| 模块 | 每帧 | 占比 |
|------|------|------|
| ORB | 1455ms | 79.5% |
| Optical Flow | 209ms | 11.4% |
| YOLO (NPU) | 128ms | 7.0% |
| Tracking | 33ms | 1.8% |
| Local Mapping | 5.5ms | 0.3% |
| **总计** | **1.83s/帧** | |

- 轨迹: `CameraTrajectory.txt` (25 帧), `KeyFrameTrajectory.txt` (4 KFs)
- Pose covariance: 0/25 有效（正常，仅特定帧计算）

### 2.3 代码改动总结

| 文件 | 改动 |
|------|------|
| `yolov8_seg.cpp` | NPU 后端重写: `DnnSession` 去掉 AIPP 字段, `dnn_run_image` → `dnn_run_u8`, `detect()` 统一软件 letterbox |
| `yolov8_seg.h` | 新增 `uint8_t *u8_blob_` 成员 |
| `image_proc.c` | 新增 `ngd_blob_from_image_u8()` — BGR→RGB NCHW uint8 blob |
| `subgraph_pool.h/cpp` | 新增 `run_u8()` + `input_bytes()` — uint8 tensor MMZ 拷贝+flush+forward |
| `quantize_yolov8seg.py` | `MAKE_NETBIN_INPUT_INFO = {"layout": "NCHW"}` — 关闭 AIPP |

---

## 3. 卡住的问题

### 3.1 串口 Console Zombie (硬件问题)

**现象**: 板端 NPU 程序退出后，串口在数分钟内变哑（零回显），`run_board.py` 超时。

**已知特征**:
- 内核仍活着（TCP RST 正常），但串口全哑
- 触发模式: NPU 程序正常退出后 ≤24min
- 恢复方法: **物理加电重启**（唯一方法）

**缓解**: 已在 `run_board.py` 中实现重试+恢复流程，但效果有限。建议未来用 telnet 替代串口。

### 3.2 串口命令截断

`run_board.py` 限制命令长度 ~150 字符。长命令需写成 NFS 脚本再 `sh` 执行。

### 3.3 NFS 同名覆盖可执行文件陷阱

NFS 上 `cp` 覆盖同名可执行文件后，exec 可能加载旧页 → 假 SIGILL。**部署必须换新文件名**（当前用 `_v3` 后缀）。

---

## 4. 下一步计划

### 4.1 短期
1. **跑 100 帧全序列** — 验证长序列稳定性（预计 ~3 分钟）
2. **评估轨迹质量** — 与 ground truth 对比 ATE/RPE

### 4.2 性能优化
1. **ORB 优化** (最大瓶颈, 1455ms/帧, 79.5%):
   - 考虑移植到 NPU 或使用 SIMD (但 A7 无 NEON, 只有 vfpv4-d16)
   - 降低分辨率或减少特征点数
2. **YOLO 后处理优化** (84ms/帧):
   - `ngd_resize_nearest_f32` + threshold + crop + dilate 链可融合
3. **光流优化** (209ms/帧):
   - 当前纯软实现，可考虑降分辨率

### 4.3 长期
- 实时管线（当前 1.83s/帧，远未实时）
- 多线程/流水线（ORB + YOLO 并行）
- Telnet 替代串口调试

---

## 5. 踩过的坑

### 5.1 AIPP 不支持 packed RGB + resize
**症状**: `IOC_CMD_AIPP_NNP err 19403817`  
**根因**: NNP318 AIPP 硬件仅支持 NV12/YUV 格式 + resize，不支持 BGR/RGB_888_PACKED  
**方案**: 放弃 AIPP，走 uint8 tensor 输入

### 5.2 NFS 同名覆盖 exec 页陈旧
**症状**: 二进制 `cp` 覆盖后运行正常，再改再覆盖 → SIGILL  
**根因**: NFS client cache 持有旧页  
**方案**: 每次部署换新文件名 (`_v2`, `_v3`...)

### 5.3 A7 无 NEON
**症状**: 任何 NEON 指令 → SIGILL  
**方案**: 编译必须 `-mcpu=cortex-a7 -mfloat-abi=hard -mfpu=vfpv4-d16 -mno-unaligned-access`

### 5.4 串口命令截断
**症状**: 板端 `__CLI_ERROR__ 命令过长`  
**方案**: 写成 NFS 脚本再 `sh` 执行

### 5.5 Console Zombie
**症状**: NPU 程序退出后串口全哑  
**方案**: 物理加电重启（唯一恢复方法）

---

## 6. 关键路径

### 代码
```
/DEngine/projects/slampart/refappv8seg_full/
  src/yolov8_seg.cpp          ← NPU 后端 (NGD_DNN_BACKEND=2)
  src/image_proc.c            ← 图像处理 (letterbox, blob)
  include/ngd_app/yolov8_seg.h ← 检测器接口
  build_arm_npu.sh            ← 交叉编译脚本

/DEngine/projects/qwen_board/
  backend/subgraph_pool.h/cpp ← NPU 子图运行时 (run_u8)
  backend/npu_runtime.c       ← NPU 底层运行时
  common/mmz.c                ← MMZ 内存分配
  run_board.py                ← 板端部署/串口控制

/home/frank/workspace/tianxuan/myProject/yolov8segrelay/
  quantize_yolov8seg.py       ← 模型量化+make_netbin
```

### 模型
```
/home/frank/workspace/tianxuan/myProject/yolov8segrelay/
  export/result/quantized.json        ← 量化后的 Relay IR
  export/nnp318/net_combine_no_list.bin ← NPU 最终产物 (3.83 MB)

/home/frank/nfs/fy01p/slampart/slamres/models/
  yolov8n-seg-320x448.bin    ← 板端 .bin (从上面 cp 过来)
```

### 板端二进制
```
/home/frank/nfs/fy01p/slampart/bin/
  rgbd_tum_app_v8seg_full_fy01p_npu_v3  ← 当前版本 (uint8 tensor, 无 AIPP)
```

### 板端数据
```
/home/frank/nfs/fy01p/slampart/slamres/
  seq100/                    ← TUM 序列 (100 帧 RGB-D)
    asso5.txt                ← 5 帧关联
    asso25.txt               ← 25 帧关联
    asso.txt                 ← 100 帧关联
  TUM3.yaml                  ← ORB-SLAM 配置
  Vocabulary/ORBvoc.bin      ← DBoW 词袋
```

---

## 7. 构建 & 部署命令

### 模型转换 (仅 make_netbin, 无需重跑量化)
```bash
source /DEngine/tytvm/env_nnp318.sh
cd /home/frank/workspace/tianxuan/myProject/yolov8segrelay
python3 quantize_yolov8seg.py --mode build-relay
```

### 交叉编译
```bash
cd /DEngine/projects/slampart/refappv8seg_full
bash build_arm_npu.sh
```

### 部署到 NFS
```bash
# 模型
cp export/nnp318/net_combine_no_list.bin /home/frank/nfs/fy01p/slampart/slamres/models/yolov8n-seg-320x448.bin
# 二进制 (每次换新名)
cp /DEngine/projects/slampart/build_arm_npu/rgbd_tum_app_v8seg_full_fy01p_npu \
   /home/frank/nfs/fy01p/slampart/bin/rgbd_tum_app_v8seg_full_fy01p_npu_vN
```

### 板端测试
```bash
# 准备运行脚本 (NFS 上)
# 编辑 run_slam_npuXX.sh, 改 B= 路径指向新二进制

# 串口执行
cd /DEngine/projects/qwen_board
python3 run_board.py --board-cmd "sh /mnt/fy01p/slampart/run/run_slam_npu25.sh"
```

### 查看结果 (串口哑时直接用 NFS)
```bash
cat /home/frank/nfs/fy01p/slampart/run/rc_npu25.txt
tail -60 /home/frank/nfs/fy01p/slampart/run/board_npu25.log
```

---

## 8. 环境变量

| 变量 | 值 | 说明 |
|------|-----|------|
| `NGD_DNN_BACKEND` | `2` | NPU 后端 (编译时 `-DNGD_DNN_BACKEND=2`) |
| `NGD_VIEWER` | `0` | 关闭 Pangolin viewer |
| 工具链 | `arm-mol-linux-uclibcgnueabihf-` | `/home/frank/workspace/toochain/arm-molchip-linux/bin/` |
| SDK | FY01P_SDK_V0.9.5_RC1 | `/home/frank/workspace/github/fy01p/FY01P_SDK_V0.9.5_RC1_202608031855/` |
| TVM env | `env_nnp318.sh` | `source /DEngine/tytvm/env_nnp318.sh` |