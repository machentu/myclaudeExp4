# xc01 SLAM 当前状态速览（v46 交付态, 2026-10-13）

数据源：v46 全量 827（run_v46/，板端 m208 布局，NPU@500MHz，`_xc01npuP`）。
配置 = **②OF_NEON + ③POST_NEON + BA_FAST(A1-A7) + ②协视窗帽24**（`build_arm_xc01_npu16.sh`，质量口径默认）。
帽 = `-DNGD_BA_COVIS_CAP=24`：LBA 只优化当前 KF + 权重前 24 个协视 KF（自由位姿 25，
被丢 KF 降级固定位姿、边保留 = 标准窗口化 BA）。**实测零精度代价**（ATE 1.49 vs 无帽 1.46，
噪声带内），LBA mean 5.17s→1.30s（4.0×）、max 14.2s→1.77s（8.1×）。

## 1. 总览

| 指标 | 数值 |
|---|---|
| 全量 wall | **15.22 min**（v41 21.53 → **−29%**；v34 基线 38.6 → −61%） |
| 吞吐率 | **0.90 fps**（827 帧 / 913s） |
| 帧内跟踪 median/mean | 0.7280 / 0.8910 s（KF 帧长尾已缩短：v41 mean 1.354） |
| ATE RMSE | **1.49 cm** / scale 0.9762（v41 1.46 同带；v34 1.43 / v37 1.46） |
| KF 建立 | 115 / 827（帽后精化更局部→KF 触发略增 107→115，无碍） |
| LBA（帽后） | 112 次，med 1.43s / max 1.77s（无帽 med 4.30s / max 14.23s） |
| 阶段计时总和 | 727.2 s（wall 差 ~186s = 词袋装载 + IO + 间隙，同历版） |
| 板端稳定性 | rc=0 板活；宏关 .o md5 与 v41 逐位全等（含新帽钩子复验） |

## 2. 模块耗时分解（stage 表，share = 占计时总和）

| 模块 | 总量 s | 每次调用 ms | 调用数 | share | 说明 |
|---|---|---|---|---|---|
| **orb** | **378.3** | 457.4 /次 | 784 | **52.0%** | VDSP ORB（CPU 2175ms 的 4.7×）；帽后成第一大头 |
| **lm** (LocalMapping+BA) | **155.5** | 1352 /KF | 115 | 21.4% | 帽后（v41 547s 的 28%）；LBA 本体 med 1.43s |
| **of** (LK 光流) | 108.5 | 131.2 /帧 | 824 | 14.9% | ②NEON 后 5.0× |
| **track** | 37.2 | 45.0 /帧 | 784 | 5.1% | |
| **yolo** | 47.8 | 57.7 /帧 | 827 | 6.6% | forward 35.9 + decode 10.6 + pre 5.1 + post 5.7 |
| lc / view / mask | ~0 | — | — | 0% | |

**速度档（默认关）**：帽+①线程化（v48，`_xc01npuR`）= **10.1 min / 2.22 cm**（KF 116，
worker 1.63s/KF 跟得上；mean tracking 0.52s < median 0.69s = KF 长尾消失）。异步残余
~0.7cm（新 KF 精化延迟 2-3 帧 + writeback 跳变，inline 是阻塞立即精化）。

## 3. 版本沿革（全量 827，同一数据/布局）

| 版本 | 配置 | wall | ATE |
|---|---|---|---|
| v34 | 基线（VDSP ORB + NPU yolo） | 38.6 min | 1.43 cm |
| v37 | +②③ | 29.4 min | 1.46 cm |
| v40 | +BA_FAST A1-A6 | 22.47 min | 1.46 cm |
| v41 | +A7 分块 LU | 21.53 min | 1.46 cm |
| **v46** | **+②协视窗帽24（质量口径交付）** | **15.22 min** | **1.49 cm** |
| v38 | ①KF稀疏@12（速度档） | 10.0 min | 3.29 cm |
| v44 | ①b lm线程+硬刹车 | 7.88 min | 4.01 cm |
| v45 | ①b lm线程+KF间隔≥8 | 11.55 min | 4.24 cm |
| v47 | ①b+帽24+线程+gap8 | 10.1 min | 2.52 cm |
| v48 | ①b+帽24+线程+gap4 | 10.1 min | 2.22 cm |

### ② 协视窗帽（v46-v48, 10-13）：帽本身零代价，质量口径换挡

机理：本数据集晚段协视图全连通，LBA 窗涨到 NP=84/dim_p=498（单次 14.2s）。帽取权重
前 24 协视 KF（connKFs 天然按权重降序，keyframe.c kf_conn_sort），其余降级固定（边保留）。
注意 `[bafast]` 打印的 NP=62 是 local+fixed 总数，**dim_p/6=25 才是自由位姿数**。
三点定案：(a) 帽 inline 零代价（1.49≈1.46）；(b) 线程化 4.24→2.52（worker 4.9→1.4s/KF
跟上需求，陈旧窗口 10-15 帧→2-3 帧）；(c) gap 8→4 补回 KF 密度（88→116）再收 2.52→2.22，
残余 ~0.7cm 为异步固有（精化延迟+writeback 跳变），再收敛需 worker 抢占/OF 再锚定。

### ①b lm 线程化（v42-v45, 10-13）：三坑两修（详见 perf_analysis §①c）

死锁环→post 非阻塞；KF 洪泛螺旋→忙时 KF 最小间隔（`NGD_LM_KF_MIN_GAP`，默认 8，
配帽后建议 4）；硬刹车→过稀。无帽时线程档 7.9-11.6min/4.0-4.2cm（陈旧性主导），
配帽后 10.1min/2.22cm。

## 4. 下一步优化方向（按 收益×可行性）

| # | 方向 | 可回收 | 性质/风险 | 依据 |
|---|---|---|---|---|
| A | **orb 378s（52%）**：帽后成第一大头；调用数随 KF 门槛（784 次），调用内 457ms 已 4.7× | 调用数减半即省 ~190s | ①拨盘有代价；独立降调用需解 system.cpp ORB/KF 耦合 | §2 |
| B | **lm 残余 155s 中 schur ~半**：f64 标量 VFP 算力墙 | f32 NEON+迭代精化估 ~60-80s | 精度受限，需 ATE 门禁，工程量大 | probe_lutime_b1 |
| C | ~~启动+间隙~ **已定案 (§6 预取)**：实测词袋仅 4.4s（v31+ tmpfs 暂存），真驱动 = img_load 200.7ms/帧=166s | **已收割 −158s** | 预取生产线程 NGD_IMG_PREFETCH，轨迹逐位同 | §6 |
| D | decode 10.6ms/帧 | ~8ms/帧 | 局部 NEON，收益 ~7s，低 | §2 |
| E | 速度档再收敛 2.2→~1.5：worker 抢占新 KF / OF 链再锚定 | 质量 | 前端改动，风险近跟踪核心 | §3 ② (c) |

**不建议重做（已证伪/到顶）**：NPU 下沉 mask matmul；LU/带宽归因（实为算力墙）；A6 早停
（永不触发）；无帽线程化纯调参（v44/v45 已证不收敛）。

## 5. 复现（质量口径 = v46 形态）

```bash
# 构建 (PC):  bash slampart/refappv8seg_full/build_arm_xc01_npu16.sh   # 产物 _xc01npuP
# 板端 (m208: MemTotal 201608):
setsid sh /mnt/xc01/use_sdkv2/slamres/run_slam_xc01_v46.sh asso_full_clean.txt   # 全量 (须 setsid!)
# head100 冒烟: 省略 asso 参数 (帽不触发, 应得 1.13/0.895288 与 v41 全同)
# 速度档: run_slam_xc01_v48.sh (_xc01npuR, 10.1min/2.22cm)
# ATE: python3 slampart/ate.py run_v46/CameraTrajectory.txt <SEQ>/groundtruth.txt
```

宏清单（npu16 全集）：`NGD_OF_NEON` / `NGD_POST_NEON` / `NGD_BA_FAST`(A1-A7) /
**`NGD_BA_COVIS_CAP=24`**（新，质量口径默认开；16/32 可调，不定义=还原） /
`NGD_LM_THREAD`（速度档，默认关；`NGD_LM_KF_MIN_GAP` 默认 8，配帽建议 4）/
`NGD_KF_SPARSE`（默认关）。宏关 = 与基线位级一致（.o md5 验证）。
详见 `perf_analysis_v34.md` §3R/§①c/§②、`handoff_xc01.md` §4.8。

## 6. 增补（2026-11-24, v46 定档后两条线收官）

**BA 稀疏化 A/B — 负结果, 默认关**（详 `ba_sparse_ab_test.md`）：`ba_sparse.c`
（block-sparse Schur+Cholesky）两轮板端 A/B 输给 dense fast — r1 1637ms/KF·1.72cm、
r2 热路径优化后 1602ms/KF·1.72cm vs v46 1352ms/KF·1.49cm。剩余 +18.5%/call 是
体系结构级（6×6 块散射 vs dense 分块流式, 帽24 规模 nb≈60 下 dense 碾压）。
`NGD_BA_SPARSE` 默认 0（ba_sparse.h, -D=1 启用）; 主场 = GBA 级/解帽大窗口。
HEAD 默认构建 = `_xc01npuU`（npu21, objdump 证编译级等价 v46）。

**-O3 备选速度档 — 等价门破, 未翻默认**（详 `ba_sparse_ab_test.md` §五）：
`_xc01npuV`（npu22 = v46 源码 + sed -O3）: **14.25min / 1.44cm / 113KF**（lm
155.5→119.6s −23%, track −33%/call, orb per-call 持平）。GCC10 -O3 向量化/FMA
组合 ulp 级差 → KF 决策链 115→113 → 轨迹非逐位（ATE 1.44 属决策链噪声非质量
信号）。定位 = v46(15.2/1.49)↔v48(10.1/2.22) 之间的质量同档速度选择; 官方定档
产物仍 npuP/-O2（bit-stable 参考是实验线地基）。复现: `build_arm_xc01_npu22.sh`。

通用教训（两线共同）：ulp×KF决策链 = 无界轨迹差 — solver 重构（r1）和编译档
（-O3）任何 ulp 级扰动都能摆 ±0.2cm; 板端逐位复现可作重构正确性铁证（r2）。

**f32 混合精度 BA — 质量档新默认, 源码树默认已翻 (2026-11-24, 详 `ba_sparse_ab_test.md` §六)**：
`refappv8seg_full/src/ngd_opt_lmf32.c`（宏 `NGD_BA_F32`，**ba_f32.h 默认 1 = 源码树默认开**，
-DNGD_BA_F32=0 回滚 v46 精确路径）— 线性代数全 f32 NEON
（H 组装/Schur/分块 LU/回代）+ f64 收口（线性化/chi2/Huber/lambda/Nielsen/se3
更新/写回/outlier）。板端 `_xc01npuW`（npu23, -O2）全量 827 四门全过且**决策链
零翻转**：KF 115 / orb 784 / of 824 / track 784 / yolo 827 全同 v46，ATE 全统计量
同数（1.49/1.35/1.23/0.64/5.31）；**wall 14.22 min（−6.4%），lm 101.4s（−35%，
882ms/KF），纯 BA 815ms/call（−37%），晚段 max 1129ms（−44%）**。
-O3 叠加档 `_xc01npuX`（npu24）：13.60min / lm 86.0s / 纯 BA 703ms / 113KF /
1.52cm — 四门亦过但决策链翻（-O3 ulp，同 npuV 机制），留作中间档备选不翻默认。
MemFree 谷：W 2.2MB / X 5.3MB（均活）。验收方法论：质量带门 + stage 调用数对表；
"零翻转+统计量同数"是 f32 级改动能拿到的最强等价证据。
默认翻转三重验证：PC test_ba/test_ba_sparse PASS；npu25（无 -D 纯头默认）与
npu23 产物 md5 三全同；F32=0 下 ba.o 前后恒等（29 个历史脚本已补 -D=0 保语义）。

**图像预取生产线程 — 质量口径新默认 (2026-11-24, 详 `ba_sparse_ab_test.md` §七)**：
`NGD_IMG_PREFETCH`（main.cpp，**默认 1 已翻**，-D=0 回滚串行装载）— 2 槽环形
生产线程，read+stb 解码+深度转换提前一帧后台发生，藏进 track 的 VDSP/NPU/IO
等待窗（单核亦有效）。板端全量 827 **轨迹/关键帧与 W 逐字节同**、stage 调用数
全同、ATE 全统计量同数 (1.49/1.35/1.23/0.64/5.31)；**wall 14.22 → 11.59 min
(−18.5%)**，img_load 166→0.5s，MemFree 谷 2.3→6.8MB 改善。间隙定案：词袋仅
4.4s（旧 "~100s" 前提作废，v31+ 已 tmpfs 暂存），真驱动 = img_load 200.7ms/帧
(read 65.9 NFS + decode 129.6 标量 PNG)。复现：`build_arm_xc01_npu28.sh` →
`_xc01npuZA`，`run_slam_xc01_v46ZA.sh`；npu23 已钉 -DNGD_IMG_PREFETCH=0 保
W bit 重建。质量口径当前形态 = **W+预取 11.59min/1.49cm/零翻转**。
