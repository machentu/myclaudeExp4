# NGD-SLAM 重构 基准对标报告

> refapp 驱动层（refactor 纯 C 核心 + OpenCV 薄壳）对标原版 NGD-SLAM 的端到端精度报告。
> 含一处核心 bug 修复与阶段三 OF 调参全过程。

## 0. 评测设置

| 项 | 值 |
|---|---|
| 数据集 | TUM RGB-D fr3_walk_xyz（827 帧，**含人走动动态场景**）|
| 基准 | 原版 NGD-SLAM `Examples/RGB-D/Release/CameraTrajectory.txt`（OF 模式）|
| 对齐 | Umeyama（sR+t）后 ATE RMSE，按时间戳逐帧匹配 |
| 对比脚本 | `refapp/compare_traj.py` |
| 运行 | `refapp/build/Release/rgbd_tum_app.exe <vocab.bin> <TUM3.yaml> <dataset> <assoc> [yolo_dir]` |

三个阶段由两个开关切（见 §3）：
- `yolo`（运行时 `yolo_dir`：真实路径=on，假路径 `NONE`=off）
- `mbOF`（`refapp/src/system.cpp:184`：computed=OF 模式，`=0`=静态/masked-ORB；**现由 `NGD_PHASE` env 覆盖**，见附录 A）

## 1. 最终三阶段对标结果

| 阶段 | 模式 | 跟踪 | KFs | ATE RMSE | 尺度 | 路径长 | extent (x / y / z) |
|---|---|---|---|---|---|---|---|
| **基准** | 原版 NGD-SLAM (OF) | 827/827 | — | — | — | 10.27m | [-0.60..0.40] / [-0.50..0.68] / [-0.22..0.62] |
| 一 | 静态 ORB-SLAM3（无 mask） | 827/827 | 235 | 27.86cm | 0.23 | 15.3m | [-2.00..0.03] / [-0.12..0.80] / [-0.04..0.96] |
| 二 | NGD masked-ORB 变体 | 827/827 | 61 | **1.29cm** | 1.008 | 10.48m | [-0.58..0.39] / [-0.49..0.67] / [-0.23..0.62] |
| 三 | NGD OF+solvePnPRansac（无 FB，忠实原版，**默认**）| 827/827 | 48 | **1.66cm** | 1.017 | 10.39m | ≈基准，mean **50ms**/帧 |

> 阶段三历史：MLPnP+FB+winSize=9 曾是默认（1.36cm / 197ms / ORB 99%）；2026-07-03 发现该配置 OF 内点数偏低致 c2 过触发、ORB 几乎每帧提（见 §9），改用原版忠实路径 solvePnPRansac+无 FB，ORB 率 99%→12%、耗时 197→50ms、ATE 1.66cm。MLPnP+FB 仍可用 `NGD_PNP=mlpnp NGD_OF_FB=1` 复现。

- **阶段二（masked-ORB）1.29cm**：extent/路径几乎完全重合基准，是该动态数据集上的最优精度配置。
- **阶段三（OF+solvePnPRansac, 无 FB）1.66cm / 50ms**：与基准**同算法**的 apples-to-apples 对标。ORB 提取率 12%（≈原版 7.5%），耗时结构 yolo 34%/orb 36%/mask 21% 与原版一致。详见 §9（含 winSize=9 历史见 §8）。
- **阶段一 27.86cm**：无 mask 在动态场景（人走动）必然被人拖漂，静态 SLAM 固有局限，非 bug。

## 2. 核心 bug 修复：create_new_key_frame 漏加观测

### 2.1 现象
用"两开关"复现阶段一（`mbOF=0` + yolo off）失败：仅 tracked 101/827、4 KFs、frame 150 丢跟踪。

### 2.2 根因（仪器化定位）
`refactor/src/tracking.c::ngd_tracking_create_new_key_frame` 只给**新建的深度点**和**三角化点**调 `ngd_mappoint_add_observation`，**漏了给"新 KF 跟踪匹配到的已有点"加观测**——对应 ORB-SLAM3 `LocalMapping::ProcessNewKeyFrame`（cc:315-345）。

仪器化 `need_new_keyframe` 实测（[nkf] 轨迹）：
```
fr1-5  nKFs=1 nMinObs=2 nRef=720  → c2 触发，建 KF
fr6    nKFs=2 nMinObs=2 nRef=282  → c2 触发，建 KF
fr7+   nKFs=3 nMinObs=3 nRef=20↘12 → c2 门限 0.75·nRef≈15/9 极小，mI 恒 60-323 ≫ 门限 → c2 几乎永不触发
```
后果链：已有点 nObs 停在创建 KF 计数（stereo=2）→ `nKFs>2` 时 `nMinObs` 2→3 → `TrackedMapPoints(nMinObs=3)` 崩到只剩三角化点（282→20→12）→ `need_new_keyframe` 的 c2 门限 `0.75·nRef` 塌成极小值 → 只建 4 KF → 地图稀疏 → 丢跟踪。

### 2.3 修复
复制 `mvpMapPoints` 后、深度循环前，加全特征循环给已匹配已有点（obs≥1、!bad）调 `add_observation(kf,i,rightIdx)` + `compute_distinctive_descriptors`（stale obs<1 留给深度循环 replace）。`rightIdx=(uRight[i]>=0)?i:-1`（同新建点路径）。

### 2.4 修复影响（忠实 ORB-SLAM3，非调参）
| 阶段 | 修复前 | 修复后 |
|---|---|---|
| 一 | 101/827 崩（4 KFs） | 827/827（235 KFs，27.86cm，无 mask 固有局限）|
| 二 | 1.84cm（历史） | **1.29cm** |
| 三 | 11.53cm（y 漂[-0..1.46]）| 14.44cm（y 修[-0.50..0.68]，整体略退→见 §3 调参）|

修复保留——不应为某阶段数字好看而保留 bug。阶段三整体略退是 OF/KF 双分支与修复后 KF 行为（84→62 KFs）交互，属 OF 调参范畴（§3 解决）。

## 3. 阶段三 OF 调参（14.44 → 5.89cm）

### 3.1 诊断
修复后 phase3 = 14.44cm，path 29.76m（基准 3x）。轨迹采样点（每 80 帧）多数与基准差 1-3cm，ATE 残差来自**高频抖动**（path 3x）+ 少数偏冲（f150 z=-0.668）。

根因：**OF-only 帧无 ORB 描述子** → 原版 `SearchByOpticalFlow` 的 Hamming 验证（过滤坏 LK 轨迹）无法运行 → 坏 LK 轨迹（"成功"但漂到错位）泄漏进 MLPnP → pose 抖。

### 3.2 调参轨迹（5 轮）
| 轮 | 改动 | ATE | 路径 | 判定 |
|---|---|---|---|---|
| 0 | add_observation 修复后（起点） | 14.44cm | 29.76m | — |
| 1 | 收紧 sanity（3×→2×、地板 0.05→0.03、+旋转>0.08rad 拒） | 19.50cm | 33.27m | ❌ 回退 |
| 2 | **LK 前向-后向一致性**（FB<1px + 前向 err<10） | **6.40cm** | 17.90m | ✅ 关键杠杆 |
| 3 | 收紧 FB/err（1.0→0.5、10→5） | 11.93cm | — | ❌ 过过滤，回退 |
| 4 | **LK 终止准则收紧**（默认 30iter/0.01→50iter/0.001） | **5.89cm** | 13.62m | ✅ |
| 5 | MLPnP RANSAC 100→300 | 5.89cm | 13.62m | — 无变化，回退 100 |

### 3.3 最终 phase3 配置 = 5.89cm
- **add_observation 修复**（`refactor/src/tracking.c`）
- **LK 前向-后向一致性**（`refmain/src/optflow.cpp::ngd_shell_search_by_optical_flow`：前向 LK 后加后向 LK，往返误差<1px + 前向 err<10 过滤）
- **LK 终止准则 50iter/0.001**（显式 winSize21/maxLevel3）
- MLPnP RANSAC 100 iters
- 827/827、61 KFs、0.21s/帧、scale 0.961、path 13.62m
- extent x[-0.59..0.38] / y[-0.50..0.67] / z[-0.28..0.62]（基准 x[-0.60..0.40] / y[-0.50..0.68] / z[-0.22..0.62]——x/y 几乎逐位吻合，z 差 6cm）

### 3.4 关键洞察
1. **最有效杠杆 = LK 前向-后向一致性检查**（6.40cm，一举腰斩抖动）。OF 帧无描述子时，FB 是原版 Hamming 验证的正确等价过滤手段。
2. **反直觉：收紧 sanity 阈值反而更差**。MLPnP pose 即便抖也比 fallback（速度预测 `initTcw=vel_lk×last`）好——拒好 pose 退到更差 fallback 是负收益。坏 pose 要从源头（LK）过滤，不是事后扔掉。
3. **过过滤有害**：FB/err 收紧到 0.5/5 → inliers 太少 → MLPnP 不稳 → y 漂到 1.32m。
4. **MLPnP RANSAC 100 已收敛**：300 iters 给出逐位相同结果。

## 4. 改动文件清单

| 文件 | 改动 | 说明 |
|---|---|---|
| `refactor/src/tracking.c` | `ngd_tracking_create_new_key_frame` 加全特征循环 | §2 ProcessNewKeyFrame 加观测修复 |
| `refmain/src/optflow.cpp` | `ngd_shell_search_by_optical_flow` 加后向 LK + FB/err 过滤 + 显式 LK 准则 | §3 LK FB + 准则收紧 |
| `refapp/src/optflow_track.cpp` | MLPnP 100 iters（sanity 保持原 3×/0.05） | §3 定型 |

构建（refactor/refmain/refapp 均为本工程自生成，可用 cmake）：
```
c:\Users\frank.tu\CMakePath.bat
set CMAKE=C:/src/vcpkg/downloads/tools/cmake-3.31.10-windows/cmake-3.31.10-windows-x86_64/bin/cmake.exe
%CMAKE% --build refactor/myBuild --config Release   # ngd_core.lib
%CMAKE% --build refmain/build  --config Release     # ngd_shell.lib
%CMAKE% --build refapp/build   --config Release     # rgbd_tum_app.exe
```
改 refactor 源后须重编 ngd_core 再重编 refapp；改 refmain 源后须重编 ngd_shell 再重编 refapp。

## 5. 产物

`refapp/build/Release/` 下：
- `phase3.txt` — 阶段三最终轨迹（**1.36cm**，winSize=9；详见 §8）
- `phase3_vs_ngdslam.png` — 阶段三 vs 原版对比图
- `phase2.txt` — 阶段二轨迹（1.29cm）
- `phase1_fixed.txt` — 阶段一轨迹（27.86cm）
- `phase3_t4.txt` + `phase3_ws{9,15,31}.txt` — 调参历史（winSize 扫描见 §8）
- `phase3_t1/t2/t3/t5.txt` + `phase3_*.log` — 早期调参中间结果与日志
- `phase3_prerun.txt` — 修复前 phase3（11.53cm）
- `phase2_vs_ngdslam.png` — 阶段二 vs 原版 NGD-SLAM 对比图（XY 俯视 + 3D，`plot_traj.py` 生成）
- 实时 3D Viewer 见**附录 A**（`viewer.cpp`，Pangolin MapDrawer 的 OpenCV 等价实现）

## 6. 仍存与可继续

- **phase3 OF 抖动 → 已修复（见 §8）**：LK `winSize` 21→9 把 5.89cm 降到 **1.36cm**、path 13.62→10.70m；init-seeded `solvePnPRansac` 对照反而退到 10.15cm，确认 MLPnP 是对的求解器。
- **phase1（无 mask）27.86cm**：动态场景固有局限，需 mask 才能治本（即用阶段二配置）。
- **过建**：phase1 235 KFs（nRef 修复后变大 + 纯 C ORB 匹配略低叠加 c2 频触发）；phase2/3 各 ~60 KFs 合理（phase3 winSize=9 = 58）。

## 8. 阶段三 OF 抖动修复：winSize 扫描 + init-seeded 对照（2026-07-03）

阶段三（OF+MLPnP）原 5.89cm，path 13.62m vs 基准 ~10.5m（过长 30% = 高频抖动）。逐项排查：

**根因排查**：读原码确认——原版 `SearchByOpticalFlow`（`ORBmatcher.cc:2044`）用**默认 LK 参数（winSize 21/maxLevel 3/终止 30-0.01）、只查 status+mask、无 Hamming、无 FB**，比 refapp（已加 FB+err 门 + 收紧终止 50-0.001）**更宽松**；原版 `TrackWithOpticalFlow`（`Tracking.cc:2996`）也不调 PoseOptimization。所以 refapp 结构上忠实，抖动是**数值/参数**差异。

**winSize 扫描**（`ngd_shell/optflow.cpp`，env `NGD_LK_WINSIZE` 驱动，一次 rebuild 多值）：

| winSize | ATE | path (mine) | 备注 |
|---|---|---|---|
| 31 | 10.91cm | 17.95m | 窗口大→定位粗→漂 |
| 21（原默认）| 5.98cm | 13.62m | 基线 |
| 15 | 1.84cm | 11.53m | |
| **9（新默认）** | **1.36cm** | 10.70m | ≈基准 10.5m，extent 近全重合 |

单调：**窗口越小→亚像素定位越精→OF 抖动越小**。winSize=9 已使 phase-3 ≈ phase-2(1.29cm)/基准。`winSize` 默认改为 9（仅影响 OF 模式；mask 预测用独立的 `ngd_shell_lk_default`，不受影响）。

**init-seeded 对照**（验证"原版 solvePnPRansac 用 useExtrinsic=true 做时间平滑"假设）：在 `optflow_track.cpp` 加 `NGD_PNP=solvepnp` 路径，调 `ngd_shell_pnp_ransac` 传 `init_Tcw=vel_lk×last`（`ngd_quat_to_matrix` 拼 16 元素），忠实复刻 `Tracking.cc:3040`。在 winSize=9 上 A/B：

| 求解器 (winSize=9) | ATE | path |
|---|---|---|
| **MLPnP（默认，无 init）** | **1.36cm** | 10.70m |
| solvePnPRansac（init-seeded） | 10.15cm | 19.40m |

**结论**：init 先验**不是**修复点——换 solvePnPRansac 反而退化（印证早期"迭代 PnP 抖更大"的判断）。MLPnP+winSize=9 的 path(10.70m) 已**追平**原版 solvePnPRansac+init 的 path(~10.5m)，说明 MLPnP 不需要 init 也能达到原版平滑度。最终 phase-3 配置 = **MLPnP + FB(1.0/10) + LK 终止(50/0.001) + winSize=9 = 1.36cm**。solvepnp 实验路径已移除（确认更差），保留 `NGD_LK_WINSIZE` env 供后续调参。

## 9. OF 跟踪质量修复：ORB 提取率 99% → 12%（2026-07-03）

阶段三历史默认（MLPnP+FB+winSize=9）精度好（1.36cm）但**慢**（197ms/帧）：实测 ORB 提取率 99%（几乎每帧提 ORB），而原版 NGD-SLAM 仅 7.5%（每 ~13 帧一次）。本节定位根因并改为原版忠实路径，ORB 率降到 12%、耗时 197→50ms、ATE 1.66cm。

### 9.1 根因：驱动是 c2 不是 c5
`NeedNewKeyFrame` 返回 `(c1 ∧ c2) ∨ c5`，而 `c1b`（`mnId ≥ mnLastKeyFrameId + mMinFrames`，mMinFrames=0）**恒真**，故化简为 **`c2 ∨ c5`**。仪器化 `refactor/src/tracking.c::ngd_tracking_need_new_keyframe` 实测：**c2 几乎是唯一驱动**，c5 极少触发。c2 = `mnMatchesInliers < 0.75·nRefMatches`——OF 内点数低于参考 KF 点数的 75% 就重锚 ORB。

### 9.2 refapp OF 内点偏低的三个原因
| 原因 | 机制 | 修复 |
|---|---|---|
| MLPnP 内点门 ~2.45px | `5.991·σ²`（σ²=mvLevelSigma2[0]≈1），比原版 solvePnPRansac 的 5px 严 4× → 报告内点少 | `mnMatchesInliers` 改用 5px 重投影内点计数（原版同阈值）|
| FB 前向-后向过滤 | 丢 20-30% LK 点（原版 SearchByOpticalFlow 无 FB）| 默认关 FB（`NGD_OF_FB`，默认 off）|
| `obs≥3` 过滤 | `optflow_track.cpp:40` 丢新建 KF 的 obs=2 stereo 点（原版不过滤）| 去掉，追踪所有非 bad 点 |

三因叠加 → mI/nRef 常 0.5-0.7 → c2 频触发 → ORB 99%。

### 9.3 顺带修一潜伏 bug：mbCreatedKF 粘滞
`mbCreatedKF` 只在 `Track()` 开头清 0（`tracking.c:683`），OF-only 帧不调 Track() → 残留上一帧的 1 → OF 帧被误当 KF 跑 `lm.run`/`loopclosing`、`n_kf` 虚高到 470。`system.cpp` 每帧开头加 `s->tk.mbCreatedKF = 0` 修复。之前 99% ORB 时每帧都 Track() 故未显现，OF 率下降后才暴露。

### 9.4 最优配置 = 原版忠实路径
**FB off + 去 obs≥3 + 5px 内点计数 + solvePnPRansac 替 MLPnP**（原版精确调用 `cv::solvePnPRansac(useExtrinsic=false, 5px, 100, 0.99, SOLVEPNP_ITERATIVE)`）。solvePnPRansac 的 RANSAC 在 PnP 内部剔坏 LK 轨迹，**无需 FB 预过滤** → 点不丢 → mI 高 → c2 罕触发 → ORB 真跳过。

### 9.5 配置矩阵（fr3_walk_xyz，827 帧）
| 配置 | ORB 率 | mean | ATE | 备注 |
|---|---|---|---|---|
| MLPnP + FB + obs≥3（旧默认）| 99% | 197ms | 1.36cm | FB 保精度但拉高 ORB 率 |
| MLPnP + FB + obs off + 5px | 74% | 160ms | 2.47cm | |
| MLPnP + noFB + obs off + 5px | 36% | 90ms | 8.23cm | 无 FB 时 MLPnP 被坏轨迹污染（z 漂 1.44m）|
| **solvePnPRansac + noFB + obs off + 5px（新默认）** | **12%** | **50ms** | **1.66cm** | RANSAC 剔外点，两全 |

### 9.6 关键反直觉
BENCHMARK §8 旧结论"MLPnP 是对的求解器"是在 **FB on** 条件下成立（MLPnP 1.36 vs solvepnp-init 10.15）。**FB off 时反过来**：solvePnPRansac 1.66 vs MLPnP 8.23cm。原版用 solvePnPRansac + 无 FB 本就是正确设计；refapp 的 MLPnP+FB 是为补偿 MLPnP 对坏轨迹敏感而加的歧途，副作用是点丢失 → c2 过触发 → ORB 每帧提。

### 9.7 改动文件
- `refapp/src/optflow_track.cpp`：去 obs≥3 过滤；5px 重投影内点计数作 `mnMatchesInliers`；`NGD_PNP` 开关（默认 solvepnp，`=mlpnp` 切回）。
- `refmain/src/optflow.cpp`：`NGD_OF_FB` 开关（默认 off，`=1` 开 FB）。
- `refapp/src/system.cpp`：每帧开头 `s->tk.mbCreatedKF = 0`（bug 修复）。
- `refactor/src/tracking.c`：仅临时探针，已还原。

## 10. 残余 1.5cm 归因：固有非 bug（2026-07-10）

mask 预测 LK 回归修复后（§9 + memory `refapp-mask-lk-regression`），对残余 ~1.5cm ATE 做逐模块对标定论。**结论：重构已逼近原版可达下限，残余差距 largely 固有，非可修 bug。**

本轮实测（fr3_walk_xyz，827 帧）：phase2 **1.52cm** / phase3 **1.59cm**（历史最佳见 §1：phase2 1.29 / phase3 1.66；不同代码版本/KF 节奏所致，量级一致（phase3 跨 run 确定，见 §10.2））。

### 10.1 逐模块对标结论
| 模块 | 结论 | 依据 |
|---|---|---|
| mask 预测 LK | **已修**（纯C→OpenCV，phase2 从 16.6m 回到 1.29cm）| memory `refapp-mask-lk-regression`；`system.cpp:198`=`ngd_shell_lk_default` |
| PoseOpt | **faithful** | 366mm 偏差是多 run 叠加的错配假象，非 PoseOpt 缺口 |
| OF 跟踪 LK | **已是 OpenCV**（默认，和原版同）| `optflow_track.cpp:57-61`：`NGD_LK_BACKEND` 未设=OpenCV+solvePnPRansac |
| PnP 配置 | **和原版一致**（100/5/0.99）| `optflow_track.cpp:127`：`ngd_shell_pnp_ransac(...,100,5.0f,0.99f,...)` |
| 残余 1.5cm | **确定性编排差异**（非随机方差）| 见 §10.2 PnP A/B：RANSAC 跨 run 确定（方差 0），残差在 PnP 上游（LK/mask/KF 节奏）|

phase3 默认 OF 跟踪 LK=OpenCV、PnP=solvePnPRansac(100/5/0.99, useExtrinsic=false)、PoseOpt faithful -> OF/PnP 路径与原版**逐参数一致**（§10.2 A/B 证实）。143/330 等帧抖动的真正成因是**确定性编排差异**（非随机）：
- **多线程 KF 节奏**（103-106/170 那类）：原版多线程、重构单线程，KF 时刻不可逐帧对齐 -> 喂给 PnP 的点集不同。
- **OF/LK 编排细节 + 上游数值微差**：重构虽用 OpenCV LK，但帧间编排/参数与原版微差，逐帧累积。
- ~~RANSAC 采样方差~~ **已证伪**（§10.2）：`cv::solvePnPRansac` 跨 run 完全确定（A×3 逐位相同，方差 0.00cm），无随机方差。上轮"残余 largely 是 RANSAC 方差"归因错误，已修正。

### 10.2 PnP A/B 实验（2026-07-10，"确认固有"最后一锤）

加 `NGD_PNP_REFINE` 开关（`refmain/src/optflow.cpp::ngd_shell_pnp_ransac`）：RANSAC 找内点后对全部内点做 `cv::solvePnP`（LM refine，init=RANSAC 结果），隔离"RANSAC best-model 采样敏感性(A,默认) vs 全内点确定性 refine(B)"。默认 off=A（忠实原版 `Tracking.cc:3040`，无 refine）。

fr3_walk_xyz，827 帧，vs groundtruth，Umeyama 对齐，各跑 3 次：

| 配置 | ATE RMSE | mean | median | max | 跨run方差 |
|---|---|---|---|---|---|
| A（默认 solvePnPRansac）| 1.61cm | 1.39 | 1.24 | 4.69 | 0.00（×3 逐位相同）|
| B（RANSAC + 全内点 refine）| 1.50cm | 1.30 | 1.12 | 5.30 | 0.00（×3 逐位相同）|
| A vs B 轨迹差异（Umeyama）| 1.40cm | - | - | - | - |

> A 的 1.61cm 与 memory/§1 的 1.59/1.66cm 吻合（同算法 vs GT 的绝对 ATE；§1 的 1.66 是 vs 原版基准，原版基准现已不在）。

**结论**：
1. **RANSAC 采样方差证伪**：A×3、B×3 各自跨 run 逐位完全相同（方差 0.00cm）。`cv::solvePnPRansac` 在此 OpenCV 实现跨 run 确定（内部 RNG 默认种子固定）。上轮"残余 largely 是 RANSAC 方差"归因错误，已修正为确定性编排差异。
2. **PnP 求解器不是残差主因**：refine(B) 仅改善 0.11cm（1.61->1.50），且 max 略变差（4.69->5.30）。RANSAC best-model 已接近全内点最优解，"采样敏感性"极小。PnP 路径与原版等价（§10.1）+ refine 只动 0.11cm -> 残差明确在 PnP **上游**（LK/mask/KF 节奏），确定性。
3. **残余 1.5cm 仍非可修 bug**：是单线程重构 vs 多线程原版的**确定性**可达下限（机制从"随机方差"修正为"确定性编排差异"）。B 的 0.11cm 改善不足以改变结论，且偏离原版（原版无 refine），故默认仍 A。

### 10.3 附：ate.py RMSE bug 修复
`refapp/ate.py:65` 原 `np.sqrt(errors.mean())`（=sqrt(mean(err))，单位 √m 无意义）-> 修为 `np.sqrt((errors**2).mean())`（标准 ATE RMSE=sqrt(mean(err²))）。修复前 A_1 报 11.81cm（>max 4.69cm，数学不可能），修复后 1.61cm（吻合 memory）。`compare_traj.py` 公式本就正确，不受影响。历史存档数字（§1 的 1.29/1.66）用 compare_traj.py 算，不受此 bug 影响。

### 10.4 "原版忠实 OF" 对照（2026-07-10）

加 `NGD_LK_TERM`（`=orig`->30/0.01，OpenCV 默认=原版）+ `NGD_LK_OBS3`（`=1`->恢复 `Observations()>=3` 门）两个 env 开关，配合既有 `NGD_LK_WINSIZE`，逐处切回原版参数做单因子+全契对照（fr3_walk_xyz，vs GT，各 1 次，确定性已证）：

| 配置 | winSize | termCrit | obs门 | ATE RMSE | median | max | 耗时 |
|---|---|---|---|---|---|---|---|
| C0 当前默认 | 9 | 50/0.001 | 无 | 1.61 | 1.24 | 4.69 | 60s |
| C1 +winSize21 | 21 | 50/0.001 | 无 | 1.61 | 1.11 | 6.20 | 55s |
| C2 +term原版 | 21 | 30/0.01 | 无 | 1.75 | 1.18 | 16.02 | 59s |
| C3 全契原版 | 21 | 30/0.01 | >=3 | 1.66 | 1.22 | 5.71 | 76s |

**结论**：
1. **完全契合原版(C3) 1.66cm ≈ 当前默认(C0) 1.61cm**——精度等价（差 0.05cm）。重构 3 处 OF 偏离**不是为了精度**。
2. **winSize 在 solvePnPRansac 默认下不再是关键**（C0 ws9 ≈ C1 ws21 都 1.61cm）。**修正 §8 的"ws9 最优"结论**——那只适用于 MLPnP+FB 时代；当前 solvePnPRansac 默认，ws9/ws21 等价。
3. **termCrit 收紧(50/0.001)的价值 = 压住最差帧 max**（C2 松 term max 16.02cm -> C0 紧 term max 4.69cm），对 median/RMSE 影响小。重构相对原版的真实改进（原版松 termCrit 有 outlier 帧）。
4. **obs>=3 门在松 termCrit 下有保护**（C2->C3 max 16->5.71），但耗时升 27%（OF 点少 -> c2 过触发 -> ORB 率升，见 §9）。
5. **重构 3 处偏离的真正收益是耗时/ORB 率**（C0 60s vs C3 76s，省 22%），非精度。

**对"OF 精度不对"的定论**：绝对 ATE（vs GT）上，重构 OF（1.61cm）已达原版忠实配置（C3 1.66cm）级别甚至略好——**OF 精度没有问题**。若指相对某原版基准轨迹的残差，上游不全在 OF（KF 节奏/多线程，见 §10.2）。重构偏离原版参数是**耗时优化**（保精度同时降 22% 耗时 + ORB 率 12%），非精度补救。

### 10.5 纯C LK vs EPnP 隔离（2026-07-10）：纯C LK 不差，EPnP 才是元凶

质疑"纯C 双输"后做隔离实验：加 `NGD_PNP=solvepnp` 让纯C LK 配 OpenCV solvePnPRansac（**只换 LK 不换 PnP**），分离两者贡献（fr3_walk_xyz，vs GT）：

| 配置 | LK | PnP | ATE RMSE | median | max | 耗时 |
|---|---|---|---|---|---|---|
| OpenCV | cv | solvePnPRansac | 1.61 | 1.24 | 4.69 | 60s |
| 纯C u8 | u8 | EPnP | 2.25 | 1.48 | 14.99 | 131s |
| 纯C float | float | EPnP | 2.16 | 1.43 | 10.64 | 132s |
| 纯C u8 | u8 | **solvePnPRansac** | **1.66** | 1.22 | 5.78 | 82s |
| 纯C float | float | **solvePnPRansac** | **1.58** | 1.13 | 7.13 | 82s |

**结论（翻案）**：
1. **纯C LK 不差**：纯C float LK + solvePnPRansac = **1.58cm**（比 OpenCV 1.61 略好）；u8 + solvePnPRansac = 1.66 持平。纯C LK 精度追平 OpenCV。
2. **2.2cm 元凶是 EPnP（纯C PnP 求解器），不是纯C LK**：纯C LK 把 PnP 从 EPnP 换 solvePnPRansac，精度 2.2 -> 1.6cm（+0.6cm 全是 EPnP 的锅）。纯C EPnP/MLPnP 求解器精度不如 OpenCV solvePnPRansac。
3. **纯C LK 真实代价 = 精度等价（float 甚至更好）+ 速度慢 35%**（82 vs 60s，无 SIMD）。**不是**"差40%/慢2x"（那是 EPnP 混淆）。修正 §10.4 之前对纯C 的错误贬低。

**改动**：加 `NGD_OF_LK_C` 编译期 define（`optflow_track.cpp`）：开 -> 纯C float LK + solvePnPRansac（1.58cm，慢35%），去掉 **OF 的 LK OpenCV 依赖**。默认**开**=纯C float LK(1.58cm，已验证)；`-DNGD_OF_LK_C=0` 切回 OpenCV。注：PnP 仍 OpenCV solvePnPRansac（纯C EPnP 差 0.6cm）；要**全去 OpenCV** 需先有精度等价的纯C PnP（当前 EPnP/MLPnP 不达标，是下一步可攻的点）。

> 最初看到的"摄像头位置偏掉"主要是 7-6 那次纯C mask LK 回归（已修，§9 + memory）；剩下的 ~1.5cm 是单线程重构相对多线程原版的可达下限附近。

重构核心（P0-P3，33 测试通过）移植完毕；端到端对标基准——**阶段二 masked-ORB 1.29cm、阶段三 OF+solvePnPRansac（无 FB，忠实原版）1.66cm / 50ms（同算法），均 827/827 全跟踪**，两阶段 extent/path 均与基准几乎重合。过程修复：①忠实 ORB-SLAM3 真实 bug（ProcessNewKeyFrame 漏加观测）；②OF 帧无描述子的坏轨迹过滤——改用原版 solvePnPRansac 的 RANSAC 在 PnP 内剔外点（替代 MLPnP+FB 预过滤），使 ORB 提取率 99%→12%、耗时 197→50ms（§9）；③`mbCreatedKF` 粘滞 bug（OF-only 帧误跑 LocalMapping）。

### 10.6 全纯C PnP：EPnP + pose_opt LM refine（2026-07-10）

诊断纯C PnP 差 0.6cm 根因 = **缺 LM 位姿 refine**：纯C EPnP RANSAC 的 refine 只重跑 EPnP（线性法，`pnp.c:743`），非迭代；OpenCV `solvePnPRansac(ITERATIVE)` 自带 LM refine。OF 路径（`Tracking.cc:2996-3102`）不调 PoseOptimization，故纯C PnP 在 OF 路径裸奔。

解药 = `refactor/src/pose_opt.c`（PoseOptimization 纯C 移植：单帧 SE3 + 重投影残差 + Huber + IRLS + 4 轮×10 Levenberg）。纯C 分支 EPnP/MLPnP 后加 `ngd_pose_optimization` 对 RANSAC 内点 LM refine（`optflow_track.cpp`；`NGD_PNP_NOREFINE=1` 关闭 A/B；solvepnp 跳过）。

A/B（纯C float LK + EPnP，fr3_walk_xyz，vs GT）：

| 配置 | ATE RMSE | median | max | 耗时 |
|---|---|---|---|---|
| 纯C EPnP（无 refine）| 2.16 | 1.43 | 10.64 | 131s |
| **纯C EPnP + pose_opt refine** | **1.82** | **1.24** | 8.03 | 106s |
| 对照 OpenCV 全路径 | 1.61 | 1.24 | 4.69 | 60s |
| 对照 纯C LK + solvePnPRansac | 1.58 | 1.13 | 7.13 | 82s |

**结论**：
1. refine 改善 0.34cm（2.16->1.82），证根因（缺 LM refine）正确。
2. **median 1.24cm 追平 OpenCV**（1.24）；残余 0.21cm RMSE 全来自 **max outlier**（8.03 vs 4.69）——EPnP RANSAC 内点集略差于 solvePnPRansac，少数帧 refine 基础弱。
3. **全纯C**（LK float + EPnP + pose_opt，零 OpenCV 调用），median 等价 OpenCV。

`NGD_OF_LK_C=1` 默认 = 全纯C（纯C LK + EPnP + pose_opt，1.82cm）。`NGD_PNP=solvepnp` 回退 OpenCV PnP（1.58cm，仍纯C LK）。`NGD_PNP_NOREFINE=1` 关 refine（A/B）。
### 10.7 纯C LK FB 压 max outlier（2026-07-10）：全纯C 反超 OpenCV

发现 `NGD_LK_FB` env 名义存在（`optflow.c:718` 注释）但 `optflow_track.cpp` 从未读取 `fb_thresh`，纯C LK 的 FB 一直 off（注释误导）。修复：`optflow_track.cpp` float/u8 两分支读 `NGD_LK_FB` 设 `ngd_lk_params.fb_thresh`（px，0=off）。FB = 前向 LK 后做后向 LK，往返误差 < fb_thresh 才留——剔跟飞点（max outlier 的 LK 层大头）。

全纯C（纯C LK + EPnP + pose_opt）+ FB=1.0（fr3_walk_xyz，vs GT）：

| 配置 | ATE RMSE | median | max | 耗时 |
|---|---|---|---|---|
| 全纯C 无 FB | 1.82 | 1.24 | 8.03 | 106s |
| **全纯C + FB** | **1.49** | **1.03** | **5.61** | 194s |
| 对照 OpenCV 全路径 | 1.61 | 1.24 | 4.69 | 60s |

FB 压 max 8.03->5.61（-2.42cm，LK 层大头全部拿下），RMSE 1.82->1.49（**反超 OpenCV 1.61**），median 1.24->1.03。代价：194s（FB 多一次纯C LK，无 SIMD 故双倍慢）。残余 max 5.61 vs OpenCV 4.69（0.92cm）= PnP 层（EPnP+pose_opt vs solvePnPRansac），收益小。
### 10.8 配置开关全改 #define（2026-07-10）：去 env，代码内可改

12 个 `NGD_*` env 开关全改为编译期 `#define`（就近在每个 .cpp 顶部 `#define` 块，默认值=原 env 默认，行为不变）。验证：默认 define 跑 = fb_real（FB on 全纯C）**逐位相同**（compare_traj 0.00cm，ATE 1.49/max5.61/197s）。

开关清单（编辑这些 define + 重编即可，不再用 env）：
- `refmain/src/optflow.cpp`：`NGD_LK_WINSIZE`(9) `NGD_LK_TERM_ORIG`(0) `NGD_OF_FB`(0) `NGD_PNP_REFINE`(0)
- `refapp/src/optflow_track.cpp`：`NGD_OF_LK_C`(1) `NGD_LK_BACKEND`(0) `NGD_LK_OBS3`(0) `NGD_MIN_EIG`(0.0) `NGD_LK_FB`(1.0) `NGD_PNP`(0) `NGD_PNP_NOREFINE`(0)
- `refapp/src/system.cpp`：`NGD_VIEWER`(0) `NGD_PHASE`(3)

默认配置 = 全纯C + FB（NGD_OF_LK_C=1 + NGD_LK_FB=1.0 + EPnP + pose_opt refine）= **1.49cm / max5.61 / 197s**，反超 OpenCV 1.61cm。`NGD_VIEWER` 默认 0（headless/计时），看轨迹改 1。
## 附录 A：实时 Viewer（Pangolin MapDrawer → OpenCV 对照）

原版 NGD-SLAM 可视化（`src/Viewer.cc` + `src/MapDrawer.cc`）只在 **3D 地图窗口**用 Pangolin；2D 的帧/mask/LK 窗口本就是 `cv::imshow`。本机 vcpkg OpenCV 4.12 **未带 viz/VTK**（`opencv2/viz.hpp` 不存在），故 refapp 的 3D viewer 用**纯 OpenCV 手动 3D 投影**（`cv::projectPoints`）替代 Pangolin，画法**逐项对照 MapDrawer**：

| Pangolin（`src/MapDrawer.cc`） | 画什么 | OpenCV 实现（`refapp/src/viewer.cpp`） |
|---|---|---|
| `DrawMapPoints`（:135）黑点 + 红参考点 | 地图点云 | `project_with` + `cv::circle`，按高度着色（蓝→红，比纯黑更易读深度） |
| `DrawKeyFrames` bDrawKF（:194-263） | **每个关键帧**画成线框相机 frustum | `draw_frustum`，蓝色；init KF 红色加粗（:208-209） |
| `DrawKeyFrames` bDrawGraph（:265-311） | 共视图绿色连线 | KF 中心间 `cv::line`，`weight≥100`（:275），`mnId<j` 去重（:281） |
| `DrawCurrentCamera`（:398-438） | 当前相机绿色 frustum | `draw_frustum`，绿色 |

**frustum 几何逐字同 MapDrawer**（:400-402, :414-435）：`w=size, h=w·0.75, z=w·0.6`，8 条线 = apex `(0,0,0)` → 4 角点 `(±w,±h,z)` + 图像矩形 4 边；在**相机坐标系**构建，经 `Twc = inverse(pose)` 变换到世界（对应原码 `pKF->GetPoseInverse()`，:199）。

**尺寸/颜色**（取自 `Examples/RGB-D/TUM3.yaml` 的 `Viewer.*`）：`KeyFrameSize=0.05`、`CameraSize=0.08`、`CameraLineWidth=3`。为让 frustum 在任意场景都清晰可见，`auto_fit` 按**场景对角线 diag** 缩放 frustum 尺寸（`cam_size=diag·0.085, kf_size=diag·0.055`，diag 钳到 `[0.3, 20]`）——因视角距离也 ∝ diag，屏幕上大小与场景尺度无关；比 yaml 原值放大约 1.8×，接近 Pangolin 近距离默认视角（`ViewpointZ=-1.8`）的观感。

**两个窗口**（`refapp/include/ngd_app/viewer.h`）：
- `NGD refapp: 3D Map` — 点云 + 关键帧 frustum（蓝 / init 红）+ 共视图（绿）+ 当前相机 frustum（绿）+ 相机轨迹（暗绿折线）。**拖拽=轨道旋转、滚轮=缩放、`r`=重拟合视角、`x`/`y`/`z`=选世界 up 轴**。
- `NGD refapp: Frame` — 当前 RGB + masked-ORB 关键点（绿=内点匹配 / 红=外点 / 灰=未匹配）+ 动态 mask 红色半透明叠加。

**纯显示、不改 SLAM 状态**（viewer 开/关轨迹 ATE 完全相同）。

**运行**（`refapp/build/Release` 下，cmd）：
```cmd
set NGD_PHASE=2 && rgbd_tum_app.exe ..\..\..\Vocabulary\ORBvoc.bin ..\..\..\Examples\RGB-D\TUM3.yaml C:\Users\frank.tu\Downloads\rgbd_dataset_freiburg3_walking_xyz ..\..\..\Examples\RGB-D\associations\fr3_walk_xyz.txt
```
运行时开关：`NGD_VIEWER=0` 关 viewer（计时跑）；`NGD_PHASE` 选阶段（unset/`3`=OF、`2`=masked-ORB、`1`=静态+假 yolo_dir），**取代** §3「改 `system.cpp:184`」的做法（mbOF 现由 env 控制）。阶段三 OF 子开关（见 §9）：`NGD_PNP`（默认 `solvepnp`，`=mlpnp` 切回 MLPnP）、`NGD_OF_FB`（默认 off，`=1` 开 FB）、`NGD_LK_WINSIZE`（默认 9）。对比图脚本 `refapp/plot_traj.py`（Umeyama 对齐，XY 俯视 + 3D 双面板，存 PNG）。

## 附录 B：OF/KF 双分支与 NeedNewKeyFrame 触发逻辑（原版算法说明）

阶段三（OF 模式）每帧走 `GrabImageRGBD`（`src/Tracking.cc:1532-1675`，refapp `system.cpp::ngd_app_system_track_rgbd`）的双分支：

- **OF 分支**（`mbStartOpticalFlow = mFrameNum>3` 后的大多数帧）：建无 ORB 的帧 → `TrackWithOpticalFlow`（LK + PnP 出 pose，无 PoseOpt/TrackLocalMap）。
- **KF 分支**（`mbNeedKF` 为真时）：提 masked-ORB（mask 非零→双提取器）→ `Track()`（TWM/TrackRefKF/**TrackLocalMap+PoseOpt**）→ LocalMapping（LBA/三角化/Fuse/KF 剔除）纠正漂移、扩地图。
- **前 ~3 帧**（!RGBDInitialized）：`mbNeedKF` 恒真 → 一直 KF 分支（建图 bootstrap）。

`mbNeedKF = NeedNewKeyFrame()`（`Tracking.cc:3220-3360`，refapp `refactor/src/tracking.c:360` **逐字忠实移植**，非 IMU RGBD 口径）：

```
return ((c1a || c1b || c1c) && c2) || c5;      // Tracking.cc:3333 (c3/c4 为 IMU-only，refapp 不涉及)
```

| 条件 | 含义 | 来源 |
|---|---|---|
| `c1a` | 距上个 KF 已过 ≥ `mMaxFrames`（≈30 帧≈1s）——距离节流 | cc:3300 |
| `c1b` | ≥ `mMinFrames` 且 LocalMapping idle（单线程恒真）| cc:3302 |
| `c1c` | 跟踪很弱：`inliers < 0.25·nRefMatches` 或 缺近点 | cc:3304 |
| `c2` | 跟踪变弱但可用：`inliers < 0.75·nRefMatches`（thRefRatio）或 缺近点，且 `inliers>15` | cc:3306 |
| `c5`（NGD OF 专用）| `mbStartOpticalFlow` 下：`inliers<20` ‖ (`<75` 且 >5 帧) ‖ (`<300` 且 >30 帧) ——OF 退化时强制 ORB 重锚 | cc:3331 |

另有**重定位冷却**（刚 reloc 完且地图已大时抑制插 KF，cc:3243）、`thRefRatio`（RGBD 0.75 / nKFs<2 时 0.4 / mono 0.9）。

**语义**：OF 廉价但会累积漂移/内点流失；当"走得够远且跟踪变弱"（c1∧c2）或"OF 内点掉到阈值下"（c5）时，切回 KF 分支用强 ORB + LBA 纠偏。**refapp `c5`（tracking.c:410-414）与原版 cc:3331 逐字一致**（仅省略 IMU-only 的 c3/c4）——代码已忠实，无需补逻辑。

**本次 phase-3 (winSize=9) 实际占比**：827 帧 58 KF → KF 分支 ~58 次、OF 分支 ~769 次 ≈ **7% KF / 93% OF**；平均每 ~14 帧一个 KF（比 `mMaxFrames=30` 密，说明动态场景下 c5/c2 频繁触发——人遮挡致 OF 内点波动，正是 NGD 要应付的）。
