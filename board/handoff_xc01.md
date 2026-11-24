# HANDOFF xc01 — ORB-SLAM3 + YOLOv8-seg 板端部署 (XC01/NNP310/VDSP)

**日期**: 2026-10-10
**状态**: 代码集成全部完成（nnp310 yolo 宏 + VDSP ORB 宏）；PC baseline 定档（827/827，ATE 1.41cm）；
板端全链唯一成功形态 = 10-08 E2（词袋版，100 帧）；后续所有免词袋/布局实验（E13-E32、v21）均死于
NPU create 野写 × 大进程驻留的物理层问题，**待下一步部署分析**。

---

## 1. 任务定义

原始任务（`slampart/prompt2.txt`）：
1. YOLOv8-seg 从 nnp318 迁到 **nnp310**，用 `NGD_DNN_BACKEND=3` 宏切换 —— ✅ 已完成
2. ORB 特征计算从 CPU 切到 **VDSP**，在 refappv8seg_full 里用宏切换 —— ✅ 已完成（`NGD_ORB_DSP=1`）
3. 用 `rgbd_tum_app_v8seg_full` 的结果文件分析 ATE 是否达成，对比运行指标 —— 🔄 PC 基准已定档，
   板端被内核 panic 问题卡住

**代码仓库**: `/home/frank/workspace/tianxuan/myProject/minicpm/slampart/`
**板端 NFS**: `/home/frank/nfs/xc01/use_sdkv2/`（板端挂 `/mnt/xc01/use_sdkv2`）
**板卡接入**: telnet 10.0.34.100 root/空密码（`/DEngine/projects/qwen_xc01_mul/board_telnet.py`）
**权威排错记录**: `/DEngine/projects/xc01_slam_panic_investigation.md`（E1-E32 全矩阵，本文只摘要）

---

## 2. PC baseline（2026-10-10 定档）

构建：`bash refappv8seg_full/build_pc.sh`
配置：`NGD_BOW=1`（词袋 1082074 节点全加载）+ `NGD_DNN_BACKEND=1`（MNN CPU）+ CPU ORB
输入与板端完全同口径：`slamres/Vocabulary/ORBvoc.bin` + `slamres/TUM3.yaml` +
`rgbd_dataset_freiburg3_walking_xyz`（NFS 直读）

### ATE（vs groundtruth.txt，Umeyama 对齐，`python3 slampart/ate.py <traj> <gt>`）

| 口径 | 帧数 | 跟踪 | ATE RMSE | ATE mean | ATE median | ATE max | scale |
|------|------|------|----------|----------|------------|---------|-------|
| head100 (`asso_head100.txt`) | 100 | 100/100 | **1.02 cm** | 0.91 cm | 0.87 cm | 2.47 cm | 0.877 |
| **全量 (`asso_full_clean.txt`)** | **827** | **827/827** | **1.41 cm** | 1.20 cm | 1.06 cm | 6.05 cm | 0.972 |

注意：全量口径是 `asso_full_clean.txt`（827 行）；`asso.txt` 4135 行是带空洞的原始关联，不用。

### 耗时（PC，全量，ms/帧）

| 模块 | 均值 | 占比 |
|------|------|------|
| yolo (MNN CPU) | 48.6 | 22.6% |
| orb (CPU) | 106.2 | 49.4% |
| track | 4.7 | 2.2% |
| sum | 214.9 | |

### 板端对照（head100 同口径）

| 运行 | 配置 | ATE mean | 备注 |
|------|------|----------|------|
| PC baseline | 词袋 + MNN + CPU ORB | **0.91 cm** | 参考线 |
| 板 run_npu (10-08 E2) | 词袋 + nnp310 yolo + VDSP ORB | 3.33 cm | 板端唯一全量成功，≈PC 的 3.7 倍 |

产物：`slampart/build_pc/run_pc_base/`（head100）、`build_pc/run_pc_full827/`（全量）、
`slampart/baseline_pc_ate.md`（基准记录）。

---

## 3. 代码集成现状（全部完成，可编译可运行）

### 3.1 宏一览

| 宏 | 值 | 作用 |
|----|----|------|
| `NGD_DNN_BACKEND` | 0/1/2/3 | yolo 后端：0=onnxruntime，1=MNN(CPU)，2=fy01p NPU(nnp318)，**3=xc01 NPU(nnp310, TY_NPU 运行时)** |
| `NGD_ORB_DSP` | 0/1 | **1 = ORB 提取走 VDSP 固件**（`src/orb_dsp.c`），0 = CPU |
| `NGD_BOW` | 0/1 | 0 = 词袋三件（bow/kfdb/loopclosing）不编译不加载（xc01 内存救命宏） |
| `NGD_VIEWER` | 0/1 | PNG 输出开关（板端 0） |
| `NGD_UNBUF` | env | stdout 无缓冲（panic 现场定位） |
| `NGD_ORB_DSP_FW` | env | VDSP 固件路径（默认 `/mnt/xc01/use_sdkv2/orb_dsp_test/sram_all.bin`） |

### 3.2 关键改动文件

| 文件 | 内容 |
|------|------|
| `refappv8seg_full/src/yolov8_seg.cpp` | `NGD_DNN_BACKEND==3` 分支：TY_NPU + uint8 NCHW tensor 输入（沿 318 的无 AIPP 方案），lazy create 于首帧 |
| `refappv8seg_full/src/orb_dsp.c`（新） | VDSP ORB 后端：dspc_init(3 次重试抗 E20 安顿竞态) + 3×buf_alloc + RPC（5s 有限超时抗 E12 挂死，超时自动电源循环重试一次）+ 输出布局 {int32 n; n×24B kp; n×32B desc@+48004}（MAXK=2000，固件 v1015 板端 8/8 golden PASS 契约） |
| `refappv8seg_full/src/dsp/dspcalling.c/.h` | 从 orb_dsp_test 移植的 RPC 封装 |
| `refappv8seg_full/src/system.cpp` | `NGD_ORB_DSP=1` 时 `orb_dsp_init()` + `ngd_orb_set_backend(orb_dsp_extract)`；`NGD_BOW=0` 时跳过词袋 |
| `refappv8seg_full/src/main.cpp` | mallopt 封 mmap churn（E25，虽然最终证明无效但无害保留） |

固件参数硬编码 (1000, 1.2f, 8, 20, 7) == TUM3 默认值；宽度上限 640（DSP_ORB_VP6_MAX_W）。

### 3.3 构建脚本（refappv8seg_full/ 下）

| 脚本 | 配置 | 产物 |
|------|------|------|
| `build_pc.sh` | PC + BOW=1 + MNN + CPU ORB | `build_pc/rgbd_tum_app_v8seg_full` |
| `build_arm_xc01_novocab_dsp.sh` | 板 + BOW=0 + nnp310 + **DSP ORB** | `..._xc01vE` |
| `build_arm_xc01_novocab_cpu.sh` | 板 + BOW=0 + nnp310 + CPU ORB | `..._xc01vD` |
| `build_arm_xc01.sh` / `build_arm_xc01_cpuorb.sh` | 板 + BOW=1 + nnp310 + DSP/CPU ORB | `..._xc01v7`/`vC`（词袋版，需 E2 布局） |
| `build_arm_xc01_mnn.sh` | 板 + MNN yolo + CPU ORB | `..._xc01mnn`（无 NPU 对照臂） |

板构建要点：工具链 `slampart/arm-fhva12c-.../bin/arm-fhva12c-linux-uclibcgnueabihf-`；
FPU `-mcpu=cortex-a7 -mfpu=neon-vfpv4`（xc01 有 NEON，与 FY01P 不同）；NPU 运行时源只读引用
`/DEngine/projects/qwen_xc01_mul/board_qrun`；12×npu .a + libmpi.a `--start-group`；
`-static-libstdc++`（SDK .a 内嵌老 libstdc++ 与板端 .so 混绑会废掉全部 istream，见记忆
xc01-sdk-a-embeds-libstdcxx）；每次部署**必须换新文件名**（NFS 同名覆盖→exec 页陈旧→假 SIGILL）。

### 3.4 板端模型

- yolo nnp310: `slamres/models/yolov8n-seg-320x448-nnp310.bin`（TyTVM 产物，容器格式同 netbin，
  偏移 160 intellif+nnp310；运行时 staging 到 /tmp）
- VDSP 固件: `orb_dsp_test/sram_all.bin`（v1015，md5 a88283fe，6291488B）

---

## 4. 板端部署现状 —— 唯一成功形态与当前卡点

### 4.1 唯一全量成功：E2 布局（10-08，run_npu/）

**配置**：mem=160M（MemTotal≈152840K）+ `loadko-slam.sh -i`（mmz 96M@0x4A000000）+ MODULE-FREE
干净 boot + 词袋版 v2 二进制（BOW=1 + nnp310 yolo + VDSP ORB）
**结果**：100 帧（head100）全部跑完，轨迹完整，ATE 3.33cm；E7 曾以 240M 布局跑完过 100 帧
CPU-ORB 基线（rc=0）。**此后再无任何全量成功**——同配置复跑随机死（E4/E5/E10 等）。

### 4.2 当前卡点（E26-E30 定案的机制级根因）

**NPU create（`TY_NPU_CreateModel`）窗口触发 DMA 物理寻址错乱**：
- E27 哨兵抓到现行：野写把**进程自己的页内容拷贝到别的物理页**（src→dst 页戳铁证，68 页损坏），
  同时命中内核结构 → oops #1 六次命中六个不同调度器/slab 函数（非定向 bulk 拷贝，踩中哪算哪）。
- 触发条件 = **大驻留分配（≥32MB）在场 × NPU create**。受害方零调用、纯睡眠在场即死（E26）；
  探针小进程 ×300 迭代 ×5 boot 全活（E1/E2/E3/E9）；变输入/malloc churn/NFS 读全部排除（E30
  静态对照 A 即死）。
- nnp 内存**不走 mmz 池**（E28 create 后池占用仍 4KB）：来自 vmalloc 物理散页任务池
  （57 页 ≈ 损坏量级）+ dma-coherent 块 → 指向 NPU DMA 把 vmalloc 虚拟地址当物理地址的
  virt/phys 混淆。
- "延迟地雷"形态：野写总在 create 窗口发生，物理落点随 boot 漂移——落空闲页时当场无症状，
  后续内核/app 复用毒页才爆（E28 +27s；解释了"探针后同 boot SLAM 必死"与"探针后板活数小时"
  两种表象）。
- 词袋（65MB 堆）是 create 窗口的放大器；免词袋（vD/vE，~15-40MB）救了 create 但救不了全链
  （E29 第 9 帧异步爆；v21 第 4 帧左右死）。

### 4.3 已排除清单（详见调查记录）

内存压力/低水位（E8/E23 高内存照样死）、SMP 竞态（E17 单核死）、vdsp 路径（E24 无 vdsp 也死）、
mmap churn（E25 mallopt 无效）、加载方式/加载者（E11-E15 全覆盖）、模块集（E22 摄像头链 7 模块
洗清）、mmz 池布局（E24/25 池基址影响死亡速度但非根因）、**厂商原始布局也不救**（E31 48M+
mmz208M@0x43000000 探针死在 create ~16s；E32 阶梯相1 也死）。

### 4.4 板卡谱系（E31 副产物定案）

本板 = **256M DDR**（0x40000000-0x4FFFFFFF；0x50000000+ 是 bit28 镜像）。loadko-dsp.sh 的
"208M (512M-48M)" 注释面向 512M 板；qwen 板 = 128M DDR 不同板型，其实证不能直接移植。
布局铁律：Linux+mmz 总和 ≤256M；mmz 严禁声明到镜像区。

### 4.5 最新运行（v22/v23，10-10；用户任务"04c 原布局 + 免词袋 vD"两次尝试均板死）

- **v23 = 厂商默认 48M + loadko-04c.sh 原样 + 免词袋 vD**（`run_v23/`）：NPU create ✓ →
  帧 1-3 track ✓ → **第 3 帧板死**（无 rc/轨迹）。另有独立发现：**48M 内存墙** —— 16 模块
  吃 ~21M 后 MemFree 仅 17.7M，app 启动 15s 内崩到 1.9M（MemAvailable 1.8M）；全量 827 帧
  在该布局本就不可行，100 帧也过不去。→ E33。
- **v22 = 160M + loadko-slam.sh（=04c 模块集, mmz 96M@0x4A000000）+ 免词袋 vD**（`run_v22/`）：
  内存全程健康（MemFree 108-113M），loadko ✓，app 启动 ~10-12s 后板死 ≈ 首帧 yolo lazy create
  窗口；**oops 全文到手**（用户串口）：`clear_buddies+0x8 ← dequeue_task_fair ← __schedule`，
  kworker/0:1 events，受害指针被灌 0x6040 —— 第 7 个不同调度器函数，与 E16/E22 同族
  （内核数据非定向灌垃圾后异步撞雷）。→ **E34，判决性：E2 精确布局 + 最小进程也过不去，
  10-08 的两次 100 帧成功是毒页落点运气，E2 布局不再是可复现安全形态**。
- 两次均需物理加电救板。**全布局×全模块集×全进程尺寸矩阵至此穷尽，无一稳定。**

### 4.6 板卡当前状态（10-10 01:30）

v22 板死后需物理加电。当前布局未知（加电后看 MemTotal），无模块加载（MODULE-FREE）。

### 4.7 无 NPU 支线定案（10-10 下午）：零泄漏 + 板端全量 827 首通

- **三处内存泄漏全清**（ASAN 全量 827 rc=0 / 0 byte leaked）：
  ① octree `vKeys`（orb.c 栈子节点两处 free，1.49MB/帧，板端 OOM 根因）
  ② mappoint retired（set_bad 清 KF 槽 + localmapping retired + driver 清 owned 帧槽后 free，3.4KB/帧）
  ③ keyframe retired（`map.retired_kfs` 剔除挂账、`ngd_map_free` shutdown 释放，4.9KB/帧，行为零变化）
- **PC 基线更正**：全量 827 ATE 1.41→**1.50cm**（mappoint 修复的语义效应：bad-MP 槽位
  未清→NULL，Fuse 走 AddObservation 分支，对齐上游 SetBadFlag 语义；ASAN 零 UAF 证明
  1.41 非悬垂读产物）。head100 1.02cm 不变。
- **板端 v28 全量首通**（`run_v28/`，240M 布局无 ko，`_xc01mnnF`，106 分钟）：
  827/827 帧 rc=0，**ATE 1.44cm ≈ PC 1.50cm**（数值对齐，scale 0.9739 vs 0.9740），
  终点 MemAvailable 35.8MB（真实增长 63→41KB/帧，无泄漏）。
  耗时 7.7s/帧：yolo MNN CPU 3333ms（45%）> orb 2121ms（28%）> lm 1220ms/KF（16%）
  > of 825ms（11%）；median tracking 6.65s/帧。
- **结论**：全 CPU 支线（MNN yolo + CPU ORB + 词袋）在 240M 布局下功能、精度、内存
  三达标，是 NPU 致死矩阵（E1-E34）穷尽后的**可交付形态**；性能优化空间在 yolo/orb
  （如需提速再议 NPU 时机与隔离方案）。

### 4.8 yolo-NPU 支线（10-10 晚，v29→v30）：布局通、NPU 稳、模型坏→已修

- **新 cell 首通**：240M 布局不动 + `loadko-slam-n16m.sh`（nnp-only 6 模块, mmz **16M@0x4F000000**
  = mem=240M 之上的顶部 16M, 零重叠, MemTotal 不变, **无需重启/动 uboot**）。NPU create
  窗口过、100/100 帧 rc=0、板活——**E26-E34 致死机制在此物理页图未触发**（E35 阴性）。
- **MNN 链接已去**：`build_arm_xc01_npu2.sh` → `_xc01npuA` 2.04MB（MNN 版 4.09MB）。
  yolo NPU **210.9ms/帧（4.7%）vs MNN CPU 3333ms = 15.8×**，全程稳定。
- **但 ATE 8.01cm/scale 0.173 崩**。三对照定案病灶在 **nnp310 netbin 数值错误**（与 ORB
  后端无关）：v26 MNN scale 0.88✓ / E2 nnp310 scale 0.345✗（其 3.33cm 旧归因作废, 同病灶）
  / v29 nnp310 scale 0.173✗。
- **离线铁证**（`/tmp/nnp310_verify/cmp.py`）：同一校准图，fp32 参考 top anchor
  person score 0.906 紧致框；定点仿真（netbin 实际行为）score 崩 0.087、框退化全图、
  box absdiff max 352。**转换脚本 quantize_yolov8seg310.py 的"阶段 D 校验"只存不定比对，
  netbin 从未验证就上板**。
- **根因深挖（晚段）**：手搓量化脚本全变体皆坏（310 l2norm / 310b min_max / 310c float32
  输入契约）且 **nnp318 血统仿真同病（score 0.039）**→ 非工具链专属，是手搓管线缺 tyassist
  内建配置（opt 前置/global_scale 8.0/bias_method 等）；fp32 IR 过同一仿真 == ORT 证明仿真器
  忠实、病在 quantize()。
- **修复定案（离线验证通过）**：改走 **TyAssist 工具链**（`/DEngine/tyassist/tyassist.py
  build --config`，本地直跑免 docker，配方参照 yolov8npose）+ **用户自训 clip(0,6) 激活模型**
  `minicpm/yolov8nseg/yolov8n-seg-clip-opset10.onnx`（256×480，有界激活量化友好）：
  **cos 0.999636/0.997798；锚点级对拍 score 0.892 vs fp32 0.874、框漂 ~5px、proto 保持**。
  SiLU 原模型走 tyassist 亦可用（0.999/0.9895，备胎 `yolov8nseg/outputs/nnp310/`）。
  onnx 坑：version_converter 17→10 段错误；Softmax-1 coercion 语义（opset≤12 前端 `_impl_v1`
  非末维 axis 必坏，pose 的 axis=3 末维幸免）。
- **v30 板端冒烟（10-10 深夜）——链路通、数值仍崩，病灶定案到 NPU 运行时执行层**：
  100/100 帧 rc=0 板活（E35 阴性维持），yolo NPU forward 仅 **31.5ms/帧**（detect 全程 200.6ms，
  其中 postproc 155.7ms 是 CPU 瓶颈）；但 ATE **scale 0.228** ≈ 无 yolo 水平。
  - 鉴别矩阵：PC+MNN+256×480 clip fp32 = **0.89cm/scale 0.872**（app 解码/分辨率/clip 模型全无罪，
    PC 无 yolo 臂 = 5.35cm/0.26）；netbin md5 与构建产物一致。
  - **板端探针铁证**（`slamres/probe_src/yprobe_v6` + tv_in_u8.bin 喂 tyassist 同源输入）：
    ① 同输入同 netbin 同进程连跑 hash 皆不同——**输出非确定**；② 读取稳定（200ms 后重读不变）
    → 非 DMA 未完成竞态；③ task scratch 清零 2.28MB 无效；④ 损坏结构：out0 的 **cls 通道(ch5-83)
    98.8% 与 sim 定点完全一致、box(ch0-4)+mcoeff(ch84-115) 大面积坏且每次漂移**，坏值落在量化格点上；
    out1(proto) 呈**瓦片状** 100%对/0%错且 23% 总一致率。
  - 推断：tytvm 310 codegen 对本输出形状（116×2520 concat / 32×64×120）的 multi-pass 调度存在
    交叉写入（last-writer-wins → 非确定），qwen 小子图（单 task/单 cube 工作负载）从未踩到。
  - **消融实验（进行中）**：winograd1d off（outputs10b）/ param_spilt off（outputs10c）/
    num_cube 1（outputs10d），构建完成后探针复测哪个消掉非确定性。

- **v30 首跑未成（非模型问题）**：16M 顶区被 v29 残留 **MMZ 孤儿块占 9.2MB**
  （`/proc/driver/media-mem` kvirt=0x0，NPU runtime 进程退出不释放）→ create 分配失败；
  rmmod 被 fyosal EBUSY 卡住 → 软 reboot 后 telnet 10+min 未回（本机无串口）→ **待物理加电**。
  加电后 240M 布局 module-free 直接跑 v30 脚本即可。
- 陷阱备忘：v29 首跑 staging 漏 `touch /tmp/yolov8n-seg-320x448.onnx` → system.cpp:314
  fopen 探测失败 → detector 未建 → NPU 静默不跑（board.log "model not found" 即此）。
- **厂商驱动模式阴性对照（10-12，yprobe_g1，判决性）**：应用户要求用 gesture_recog 活体
  驱动模式（`npu_comm.c`+`gesture_rec_npu.c` 序列忠实复刻：输出 alloc memset(0)+flush、
  每 rep forward 前 flush 出→flush 入、forward 后**不 flush 直接读**、IO align=8）重测 r2 netbin。
  - 注：gesture 工程的 `npucommon/` 是死代码（BUILD_FIXES.md §0），活体参考=gesture_rec_npu.c；
    探针源 `yolov8nseg/probe_gest/yprobe_g1.c`（纯 C，三种模式单变量对照：
    A=vendor / B=SubGraph 复刻 / C=vendor+post-flush）。
  - **结果：三模式全坏且剖面一致**——当时定案"病在 NPU 执行层对该图的多 pass 调度"。
  - **★ 真根因定案（10-12 深夜，时钟扫描，推翻上面所有"损坏"判读）：NNP 667MHz 时钟边际**。
    - 实验链：pose_v0（厂商 2025-02 旧模）在 240M cell 与 04c 布局（48M+72M@0x43000000，
      16 模块）下**皆非确定** → 布局/模块集洗清 → 唯一剩余共因 = 所有 loadko 都设
      `clock_nnp_667000000` → **时钟扫描（`run_clkscan_v1.log`）：667/550 非确定，
      ≤500MHz（500/444/400/333.5/297/250/200/100）全部确定且 hash 全同**。
    - **500MHz 数值验证（`run_clk500_v1.log` + `probe_gest/cmp_clk.py`）**：yolo r2 vs 仿真
      cls 79 行 + mcoeff **逐元素 100% 精确**、proto cos **1.0000**、活跃锚点分数与 sim 全同
      （0.700/0.700/0.604/0.553/0.553；667 对照全灭 1.0/0.0）；DFL rows2-3 raw logit 有差但
      为 softmax 饱和平顶 + **跨频率 hash 恒定**（500/444/400/333.5 全等 e96698af）→ HW 固定
      舍入特性，解码级 ltrb max 漂 1.27px / mean 0.0013px（量化本身 vs fp32 ~5px，可忽略）。
    - 解释了全部悬案：qwen 单 dense 0.7ms 短管线 667 下侥幸正确（首 token==fp32）；v29/v30
      ATE 崩（scale 0.173/0.228）= 好 netbin 跑在边际时钟；"图特异性/执行层损坏/布局毒性"
      全是 667MHz 伪影。
    - **修复**：loadko 后必须 `echo clock_nnp_500000000 > /proc/driver/sys_config`
      （dmesg 确认 "clock set rate"）。500MHz 下 yolo forward 24ms/帧（vs 667 的 19ms）。
      谨慎项：550 坏 500 好，余量一档，产线建议 444-。E26-E34 create 窗口 panic 是否同为
      667MHz DMA 边际→未测（若被 500MHz 治愈则 240M hack cell 也可废）。
  - 探针清理完美（退出后 media-mem used=0KB，无 MMZ 孤儿），cell 稳定可复用。
  - **★ v31/v32 终验通过（10-12，NPU@500MHz 全链达标）**：
    - v31 head100：**ATE 1.03cm / scale 0.886**（PC 基线 1.02/0.877），100/100 rc=0 板活。
    - **v32 全量 827：827/827 rc=0 板活，ATE 1.45cm / scale 0.9710 ≈ 板 MNN v28 1.44/0.9739
      ≈ PC 1.50/0.9740** —— 三方数值对齐，NPU 部署定案。
    - 耗时（827 帧均值）：yolo detect 190ms/帧（pre 5.2 + **NPU forward 36.2@500MHz** + decode
      10.8 + postproc 137.7=72.5%）；ORB CPU 2175ms/帧 = 新瓶颈（71.6%）；median tracking 3.35s。
      全程 ~65min（MNN 版 106min）。终点 MemFree 88M 无泄漏迹象。
    - 脚本：`run_slam_xc01_v31.sh`（head100）/`run_slam_xc01_v32.sh`（全量，RUN=run_v32）；
      与 v30 唯一差异 = loadko 后 `echo clock_nnp_500000000 > /proc/driver/sys_config` + dmesg 校验。
  - **★ v33/v34 = VDSP ORB 上板（10-12，全形态最优收官）**：
    - 布局回 m208（mem=208M + mmz 48M@0x4D000000，7 模块含 vdsp；16M cell 装不下 vdsp 固件 6.3M）。
    - 构建 `build_arm_xc01_npu4.sh`（npu3c + `NGD_ORB_DSP=1`）→ `_xc01npuD`；脚本
      `run_slam_xc01_v33.sh`（head100）/`v34`（全量，RUN=run_v34），208M 专属门禁（240M 拒跑防重叠）。
    - **v34 全量 827：827/827 rc=0 板活，ATE 1.43cm / scale 0.9766 = 全形态最优**；
      orb **464.5ms/帧**（CPU 2175ms 的 4.7×，比 golden 541ms 还快）；median tracking
      **1.38s/帧**；全程 38.6min（v32 65min / v28 106min）。终点 MemAvailable 72M。
      v33 head100：1.13cm/0.895（DSP 定点关键点 ~0.1cm 正常差异）。
    - **E23 时代 m208 布局的 create 窗口死亡在 500MHz + 词袋 65M 大进程下未复现**（全量 827
      过 create 窗口）→ 进一步支持 E26-E34 panic 本质也是 667MHz 时钟边际 DMA（未单独定案）。
    - 耗时结构（v34）：orb 464ms + yolo 190ms（NPU fwd 35.9ms@500MHz + postproc 137.6ms）；
      下一优化点 = yolo postproc（72.7% of detect）与 lm/of。
  - **★ 1b BA/LocalMapping 提速（10-12，v39 探测→v40/v41 交付）**：
    - v39 探针（NGD_LM_PROBE）定案：晚段 LBA NP=84/NE=16822/dim_p=498 单次 24s，
      其中未计时稠密 LU ~9.8s（979ms/trial，n³ 拟合）；iters 恒 10。
    - **v41 = v37 + `NGD_BA_FAST=1`**（新文件 `src/ngd_opt_lmneon.c`，钩子 ba.c 4 行，
      构建 `build_arm_xc01_npu11.sh` → `_xc01npuK`，脚本 `run_slam_xc01_v41.sh`）：
      A1 缓冲提升/A2 BD hoist/A4 R 缓存/A5 chi-only/A7 分块 LU(NB=32) 全部位级一致
      （PC 在环 A/B dim_p=498 逐位全等 + 板端 probe maxdx=0）；A3 对称 Schur 镜像
      唯一 ulp 源，LM 吸收。**全量 827：21.53min（v37 29.4），lm 948.8→547.2s
      (−42%)，BA 总量 940→538s (1.75×)，ATE 1.46/scale 0.975696 与 v37 逐位打平，
      KF 107/84 全同**。head100 门禁 1.13/0.895288 全同基线。
    - probe_lutime_b1 定案：dim 498 LU 稠密 976→分块 756ms 仅 1.29× —— **A7 上该
      LU 是 f64 标量 VFP 算力瓶颈（~18 周期/fma），非带宽**；f32 NEON 是唯一进一步
      杠杆但精度受限，未做（估计余量 ~100s wall）。
    - 教训：板端长跑脚本必须 setsid 脱离 telnet 会话，否则掉线 SIGHUP 全组死
  - **★ ① lm 线程化（10-13，v42-v45：速度档成立 / 质量未收敛）**：
    - 形态：新文件 `src/ngd_opt_lmthread.c/.h`（宏 `NGD_LM_THREAD`，worker + 地图
      rwlock，BA solve 相位无锁），钩子 system.cpp×7/tracking.c×2/ba.c×1；宏关
      .o md5 与 v41 全等。构建 npu12-15 → `_xc01npuL~O`，脚本 run_v42-v45。
    - 三个实测坑：v42 post 阻塞×worker 取件互等死锁（frame 153）；v43 无节流
      KF 洪泛螺旋（1.3 KF/帧=10×，160 背压 444s）；v44 硬刹车地图过稀（KF70，
      ATE 4.01）。v45 折中（忙时 KF 间隔≥8 帧）：**11.55min / KF 101≈v41 / MPs
      2463 / ATE 4.24cm(max 24)** —— 密度回到 v41 仍 4cm。
    - 定性：误差主源是**陈旧性**非密度（v38 稀疏新鲜 3.29 < v44/v45 富而陈旧
      ~4.1-4.2）—— OF 链在未精化地图跑 10-15 帧，writeback 落地位姿跳变尖峰。
      ORB-SLAM2 每帧重投影匹配对此免疫，本库 OF 链没有。板端物理约束：KF 率
      ≤ worker 吞吐（LBA mean 4.9s），超出需求要么拉长链（漂移）要么跳过（稀疏）。
    - 交付口径：① = 速度档（v44 7.9min/4.01 · v45 11.6min/4.24），**质量口径
      默认关**；质量收敛需配套 ② BA 协视窗帽（LBA 缩 3× 让 worker 跟上自然 KF
      需求）或 OF 链再锚定，均已列入后续方向待定夺。head100 v42 1.17cm/0.8935
      （v41 1.13/0.8953，异步窗口小故接近）。
  - **★ ② 协视窗帽（10-13，v46-v48：帽零代价 / 质量口径换挡 −29%）**：
    - 形态：宏 `NGD_BA_COVIS_CAP=24`，ba.c gather 单钩子（connKFs 按权重降序截
      前 24 协视 KF，被丢者降级固定位姿、边保留 = 窗口化 BA）；宏关 md5 全等。
      构建 npu16-18 → `_xc01npuP/Q/R`，脚本 run_v46-48。晚段 LBA 窗 NP=84/
      dim_p=498 → 自由位姿恒 25（dim_p=150），LBA mean 5.17→**1.30s（4.0×）**、
      max 14.2→1.77s（8.1×）。注意 `[bafast]` NP=62 是 local+fixed 总数，
      dim_p/6 才是自由位姿。
    - **v46 = inline+帽 = 新质量口径默认：15.22min（−29%）/ ATE 1.49cm**
      （vs v41 21.53/1.46，+0.03 噪声带内）；KF 115；head100 逐位全同 v41。
    - v47/v48 = 帽+线程（gap 8/4）：10.1min，ATE 2.52→**2.22cm**（KF 88→116
      补回密度；worker 1.38-1.63s/KF 完全跟上，背压 0-8 次）。同密度下与 v46
      差 0.73cm → 残余 = 异步固有延迟（精化晚 2-3 帧 + writeback 跳变），再收
      敛需 worker 抢占或 OF 再锚定。**速度档配方 = run_slam_xc01_v48.sh**。
    - 状态速览已迁移：`status_xc01_v46.md`（status_xc01_v41.md 留指针）。
      （v41 首跑即此误杀，dmesg MMB LEAK + 无 rc 行为特征）。
  - 后续方向：① 性能：完整耗时分布与优化排序见 **`slampart/perf_analysis_v34.md`**
    （含 §3R 实施结果：②LK-NEON of 470→94ms/5.0×、③postproc 137.6→5.6ms、
    1b BA 940→538s/1.75×（A1-A7），均宏隔离新文件 + 逐位一致/ATE 不变；
    ①KF稀疏@12 有 +1.8cm ATE 代价已全量定案默认关；v41 后剩余大头 = orb
    375s/33.8% 与 lm 残余 547s 中 schur ~250s）② create-panic 与 667MHz 的因果单独定案
    （若坐实可废所有布局 hack）③ 厂商工单（667MHz 时钟规格）。

---

## 5. 实验矩阵速查（致死条件）

| 组合 | 结果 | 实验号 |
|------|------|--------|
| NPU/DSP 探针（小进程）单独跑 | ✅ ×300 活 | E1/E2/E3/E9 |
| NPU+DSP 交替探针 | ✅ 活 | E3 |
| 词袋版全链 SLAM（75-90MB 进程） | ❌ 帧 0-31 随机死 ×8 | E4/E5/E10/E22/E23/E24/E25/E29 |
| 模块 idle × 词袋装载期 | ❌ ×7 | E13-E19 |
| 大进程驻留睡眠 + 他进程 NPU create | ❌ 探针期死 | E26 |
| 哨兵 64MB + 单次 create | ❌ 哨兵 68 页损坏+板死 / 延迟爆 | E27/E28 |
| stress3 金丝雀 32MB + 静态 forward | ❌ i25-50 死，金丝雀净 | E30 |
| 厂商 48M 原始布局 + 探针 | ❌ create ~16s 死 | E31/E32 相1 |
| 厂商 48M + 04c + 免词袋 vD 全链 | ❌ 帧 3 死（另: 48M 内存墙 1.8M） | E33 |
| **E2 精确布局 + 免词袋 vD 全链** | ❌ create 窗口死（oops: clear_buddies） | E34 |
| **240M 布局 + 无 NPU（MNN/CPU）** | ✅ 100 帧跑完 | E6/E7 |
| **240M 布局 + 无 NPU + 零泄漏 mnnF** | ✅ **全量 827 帧, ATE 1.44cm ≈ PC 1.50** | v28 |
| **240M+16M 顶区 mmz + nnp-only + 词袋** | ✅ 100 帧 rc=0 板活（E35 阴性; ATE 崩 = netbin 坏, 非板问题） | v29 |
| **240M+16M cell + tyassist clip netbin (256x480)** | ⏳ 离线对拍过关 (cos 0.9996), 板端 v30 待物理加电后冒烟 | v30 |
| **240M+16M cell + NNP 500MHz（时钟修复）** | ✅ **全量 827, ATE 1.45cm/scale 0.971 ≈ MNN ≈ PC** | **v32** |
| **m208 布局 + NPU 500MHz + VDSP ORB** | ✅ **全量 827, ATE 1.43cm/scale 0.977, 全形态最优, 38.6min** | **v34** |
| **E2 布局 + 词袋版全链** | ✅ 100 帧（仅 10-08 两次） | run_npu |

**生产规则（现行）**：NPU 使用时同 boot 不得有 ≥65MB（后收紧到 ≥32MB）大驻留进程。

---

## 6. 下一步方向（部署分析候选）

按调查记录收束状态排列：

1. **E32 三阶梯未完成**（v20 只跑了相1 就死）：相2（厂商池 208M@0x43000000）相3（全 16 模块）
   未及执行。但相1 已死 + E31 已死 → 简单固定窗假说基本出局。
2. **mem= carve 物理窗**：若能定位 NPU DMA 固定落点（E27 方法：哨兵+单 create+损坏地图多 boot
   采样），把该物理窗从 Linux carve 掉 = 无需厂商的生产级修复。工作量在多次 boot 采样定位窗口边界。
3. **转厂商案例**：vdsp.ko/nnp.ko 均为 SDK 预编译 blob 无源码；E27 页戳铁证 + E1-E32 矩阵 +
   oops 全文已是完整证据包。带着证据要补丁/勘误。
4. **无 NPU 交付臂（保底）**：`build_arm_xc01_mnn.sh`（MNN yolo + CPU ORB）在 240M 布局已实证
   能跑完 100 帧（E6/E7 同构）→ 可先拿板端 MNN+CPU 全量 827 帧 ATE，与 PC baseline 对比，
   作为"硬件加速不可用时的板端基线"；再在其上单独引入 NPU/DSP 做增量验证。
5. **DSP ORB 单独验证臂**：VDSP 路径已被 E24 洗清（无 vdsp 也死）→ `orb_dsp_test` golden
   契约稳定（8/8 PASS，541ms/帧 vs CPU 1663ms/帧）→ MNN yolo + DSP ORB 组合理论上可行，
   未在板端全链实测过。

---

## 7. 关键路径

```
代码
  /home/frank/workspace/tianxuan/myProject/minicpm/slampart/
    refappv8seg_full/            应用工程（源码+构建脚本，见 §3.3）
    refactor/                    SLAM 核心（bow/kfdb/loopclosing 可被 NGD_BOW=0 裁掉）
    build_pc/                    PC 构建产物 + run_pc_base/ + run_pc_full827/
    ate.py / compare_traj.py     ATE 评估
    baseline_pc_ate.md           PC 基准记录
    HANDOFF.md                   FY01P 阶段交接（nnp318，历史参考）

板端 NFS（/home/frank/nfs/xc01/use_sdkv2/）
  slamres/                       所有板端产物与运行脚本
    rgbd_tum_app_v8seg_full_xc01v{2,3,5,6,7,A,C,D,E}   各版本二进制
    run_slam_xc01_v{1..21}.sh    各实验运行脚本（v21=最新）
    run_npu/                     ★ 唯一全量成功（E2, 100 帧, ATE 3.33cm）
    run_v21/                     最新失败现场（第 4 帧死, 取证快照在）
    models/yolov8n-seg-320x448-nnp310.bin
    rgbd_dataset_freiburg3_walking_xyz/   数据集+GT+asso（全量=asso_full_clean.txt 827 帧）
    Vocabulary/ORBvoc.bin
    probe_src/                   探针源码（yolo_npu_stress3 / dummy_resident2 / ilv_dsp）
  orb_dsp_test/                  VDSP golden 测试（sram_all.bin v1015 固件）
  va12c-sun-04k/ko/              模块加载脚本族（loadko-slam/min/m208/nnp/dsp/e32-*）

调查与手册
  /DEngine/projects/xc01_slam_panic_investigation.md    E1-E32 权威记录
  /DEngine/projects/xc01_layout_switch_manual.html      布局切换手册（串口步骤/铁律）
```

## 8. 常用命令

```bash
# PC baseline（重跑）
cd ~/workspace/tianxuan/myProject/minicpm/slampart/refappv8seg_full && bash build_pc.sh
cd ../build_pc && mkdir -p run_X && cd run_X && NGD_VIEWER=0 ../rgbd_tum_app_v8seg_full \
  /home/frank/nfs/xc01/use_sdkv2/slamres/Vocabulary/ORBvoc.bin \
  /home/frank/nfs/xc01/use_sdkv2/slamres/TUM3.yaml \
  /home/frank/nfs/xc01/use_sdkv2/slamres/rgbd_dataset_freiburg3_walking_xyz \
  /home/frank/nfs/xc01/use_sdkv2/slamres/rgbd_dataset_freiburg3_walking_xyz/asso_full_clean.txt \
  ~/workspace/tianxuan/myProject/minicpm/slampart/refappv8seg_full/models

# ATE
python3 ~/workspace/tianxuan/myProject/minicpm/slampart/ate.py <CameraTrajectory.txt> \
  /home/frank/nfs/xc01/use_sdkv2/slamres/rgbd_dataset_freiburg3_walking_xyz/groundtruth.txt

# 板端（160M 布局 + 干净 boot 前置，见手册）
# 板上: sh /mnt/xc01/use_sdkv2/slamres/run_slam_xc01_v21.sh [asso]
# 板死检测: python3 socket connect 10.0.34.100:23/111
# 板侧长命令: setsid sh -c 'cmd > /tmp/x.log 2>&1; echo rc=$? >> /tmp/x.log' &
```

## 9. 坑清单（拿血换的，勿重蹈）

1. NFS 同名覆盖可执行文件 → exec 页陈旧 → 假 SIGILL。**部署必须换新文件名**。
2. telnet 前台跑长命令断开即 SIGHUP 被杀（曾误判自崩）→ 必须 setsid 脱离 + NFS 轮询。
3. 板上 busybox **无 timeout 命令**（E21 乌龙：探针从未真正跑过）。
4. 板侧脚本运行中绝不可编辑（NFS 增量读取字节偏移错位）。
5. NFS 侧日志板 panic 时丢尾部脏页 → 帧数以趋势判断；关键结论要有 /tmp+串口双源。
6. 探针必须先 `qb::npu_system_init()` 再 create，否则 100001 伪装成"DSP 跑完后 NPU 坏了"。
7. qwen 运行时 C 源必须 gcc 单独编 .o。
8. 工作过程禁用 rm（用户指令）；临时目录用新名。
9. E12 DSP 首 RPC 挂死：timeout=-1 无限等待 → D-state 不可杀，只能断电；orb_dsp.c 已带 5s
   超时+电源循环重试。
10. 词袋装载期死（E13-E19）与 create 窗口死（E24+）是两个窗口，共同条件 = 模块 × 大进程。
11. 板死取证：串口日志 `~/Documents/Serial-ttyUSB0-*.log` **最后一段**（`8<--- cut here ---`
    起）；每个 boot 一段。
