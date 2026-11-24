# NGD-SLAM 纯 C 重构 — 工作记录 (WORKLOG)

> 本文档记录 NGD-SLAM 工程基于 `NGD_SLAM_TECHNICAL_DOC.md` 第 6 章「移植到纯 C / 嵌入式环境」重构建议，在 `NGD-SLAM/refactor/` 下开展纯 C 核心重构的过程、决策与验证。
> 模块级说明见 [`README.md`](README.md)；本文侧重过程（决策依据、忠实性核对、踩坑、验证）。
> 最后更新：2026-07-01（第十一期）

---

## 目录

1. [项目背景与目标](#1-项目背景与目标)
2. [路线决策](#2-路线决策)
3. [整体架构与目录结构](#3-整体架构与目录结构)
4. [第一期：数学库 + PoseOptimization](#4-第一期数学库--poseoptimization)
5. [第二期：ORB 提取 + Frame + StereoInitialization](#5-第二期orb-提取--frame--stereoinitialization)
6. [第三期：投影匹配 + 运动模型 + 简化局部地图](#6-第三期投影匹配--运动模型--简化局部地图)
7. [第四期：DBoW2 词表 + SearchByBoW + TrackReferenceKeyFrame](#7-第四期dbow2-词表--searchbybow--trackreferencekeyframe)
8. [第五期：重定位 score + KeyFrameDB + EPnP + Relocalization](#8-第五期重定位-score--keyframedb--epnp--relocalization)
9. [第六期：LocalMapping 简化 KF 插入 + MapPointCulling](#9-第六期localmapping-简化-kf-插入--mappointculling)
10. [第七期：LocalBundleAdjustment Schur 补 BA](#10-第七期localbundleadjustment-schur-补-ba)
11. [第八期：CreateNewMapPoints 三角化](#11-第八期createnewmappoints-三角化)
12. [第九期：SearchInNeighbors Fuse + KeyFrameCulling](#12-第九期searchinneighbors-fuse--keyframeculling)
13. [第十期：NGD 动态 mask 链（纯 C 核心）](#13-第十期ngd-动态-mask-链纯-c-核心)
14. [第十一期：MLPnP 求解器（纯 C 核心）](#14-第十一期mlpnp-求解器纯-c-核心)
15. [忠实性核对（逐行对照原码）](#15-忠实性核对逐行对照原码)
16. [调试踩坑记录](#16-调试踩坑记录)
17. [测试验证](#17-测试验证)
18. [构建方法](#18-构建方法)
19. [遗留问题与下一步路线图](#19-遗留问题与下一步路线图)

---

## 1. 项目背景与目标

NGD-SLAM 是在 ORB-SLAM3 基础上集成 YOLO 动态物体检测 + 光流跟踪的版本（项目根：`NGD-SLAM/`）。技术文档 `NGD_SLAM_TECHNICAL_DOC.md` 第 6 章给出移植到纯 C / 嵌入式环境的重构建议，第 7 章给出已完成的 Windows 移植经验。

**重构目标**：新建独立目录，按文档建议把工程核心算法移植到纯 C，使其可在资源受限/嵌入式环境运行，同时保持与原 C++ 实现的数值一致性。

**前置分析**（通过三个并行 Explore agent 完成代码调研）：

- **NGD 专有代码面**：YOLO.cc（`cv::dnn`，无 C API，最难）、mask 链函数（中等）、`TrackWithOpticalFlow`（中-难，`solvePnPRansac` 无 C API）、`SearchByOpticalFlow`（易-中）、双 ORB/c5（易）。发现潜在 bug：`ComputePixelPotential` 未初始化计数器、死枚举 `OK_KLT`。
- **数学层**：g2o 最难（`Optimizer.cc` 5590 行）；自定义视觉边在 `OptimizableTypes.{h,cpp}`（雅可比简单）；Sophus 普遍但浅；Eigen 主要是小固定尺寸。文档建议首选 `PoseOptimization`（P0）。
- **构建面**：27 源文件 SHARED 库，OpenCV 重度依赖，g2o 内建，ceres/SuiteSparse 是死重。

---

## 2. 路线决策

通过 `AskUserQuestion` 与用户确认两个关键决策：

### 决策一：移植策略

**采用「纯 C 核心 + OpenCV 薄壳」**（非严格纯 C）。
- 核心算法（数学库/李群/优化器/NGD 动态 mask 链）用纯 C 重写、可独立编译与单元测试。
- 图像 IO / 光流 / PnP / YOLO 推理暂经薄 OpenCV 封装调用（后续增量）。
- **理由**：YOLO 的 `cv::dnn` 无 C API、g2o 稀疏 BA（LocalBA/EssentialGraph）在合理时间内无法用纯 C 完整复现；严格纯 C 会把首期周期拉到不可控。文档 §6.7 本就建议此方向。

### 决策二：首期交付范围

**首期只交付数学库 + PoseOptimization**（P0 基础层 + 第一个优化器）。
- 选 `PoseOptimization` 的依据：文档 §6.3(1)/§6.8 指定其为首个 P0 目标；且原 `PoseOptimization` 是纯 g2o+Eigen+Sophus+相机模型（无 OpenCV 调用），自包含、可独立测试。

### 第二期范围扩展

首期完成后，用户要求继续推进 P0 剩余项。研究后发现 `TrackReferenceKeyFrame` 依赖 DBoW2（工作量巨大），故第二期落地为：**ORB 提取 + Frame(RGBD) + StereoInitialization**，TrackRefKF 留待 DBoW2 决策后推进。

---

## 3. 整体架构与目录结构

```
NGD-SLAM/refactor/
├─ CMakeLists.txt            C11, MSVC + GCC 双兼容，无外部依赖
├─ README.md                 模块级说明
├─ WORKLOG.md                本文件（过程记录）
├─ include/ngd/
│  ├─ math.h                 Vec3 / Mat3 / Quat / 6×6 线性求解
│  ├─ so3.h                  SO3 exp/hat/log
│  ├─ se3.h                  SE3 完整 exp/log/map/inverse/SE3deriv
│  ├─ pinhole.h              mono+stereo 投影及雅可比
│  ├─ pose_opt.h             PoseOptimization
│  ├─ orb.h                  ORB 提取器
│  ├─ matcher.h              DescriptorDistance
│  ├─ calib.h                相机标定上下文
│  ├─ mappoint.h             MapPoint（最小）
│  ├─ keyframe.h             KeyFrame（最小）
│  ├─ frame.h                Frame（RGBD）
│  ├─ tracking.h             StereoInitialization + TrackWithMotionModel + TrackLocalMap + TrackReferenceKeyFrame
│  └─ bow.h                  DBoW2 词表（load/transform/BowVec/FeatVec）
├─ src/                      与 include 一一对应
└─ tests/
   ├─ test_math.c
   ├─ test_se3.c
   ├─ test_pose_opt.c
   ├─ test_orb.c
   ├─ test_frame.c
   └─ test_stereo_init.c
```

**设计原则**（文档 §6.4 / §7.3，与量产扫地机芯片嵌入式图优化同思路）：
- 放弃 g2o 通用图结构，按**固定维度硬编码 Hessian**（位姿 6 维 → `float H[36]` 栈上开）。
- **固定索引关系硬编码填矩阵**（`H += ws·AᵀA`、`b −= ws·Aᵀr`）。
- 全部结构体显式零初始化，杜绝未初始化隐患。
- 无 `cv::Mat::at` 无边界访问；所有像素索引带钳位/范围/深度/NaN 守护。
- 大对象（网格、金字塔、MapPoint 观测数组）堆分配；Frame/KeyFrame 网格为指针字段（避免 150KB 栈溢出，§7.3）。
- 纯值类型，热路径无隐式分配，便于后续对象池化。

---

## 4. 第一期：数学库 + PoseOptimization

完成日期：2026-06-29/30。无任何外部依赖（仅 `<math.h>`）。

| 模块 | 文件 | 对应原码 | 核心内容 |
|---|---|---|---|
| 固定尺寸线代 | `math.c/h` | 文档 §6.9 | Vec3、Mat3(闭式逆)、Quat(Hamilton w-first)、6×6 高斯消元线性求解(部分主元) |
| SO(3) | `so3.c/h` | Sophus::SO3f | exp(Rodrigues→Quat)、hat(反对称)、log |
| SE(3) | `se3.c/h` | g2o::SE3Quat | 完整 exp(左雅可比 V)、log、map(R·p+t)、inverse、compose、SE3deriv(3×6) |
| 针孔相机 | `pinhole.c/h` | `Pinhole::project/projectJac` | mono + stereo(uR=fx·X/Z+cx−bf/Z) 投影及雅可比 |
| 位姿优化 | `pose_opt.c/h` | `Optimizer.cc:814-1114` | 单 SE3 顶点 + LM(H+λI) + Huber + 4 轮 chi2 内外点分类 |

### PoseOptimization 实现要点

逐行核对 g2o 源码后忠实移植：
- 单 SE3 顶点，左扰动 oplus `T_new = exp(ξ)·T`（`VertexSE3Expmap::oplusImpl`）。
- 残差 `r = obs − project(map(T, Xw))`；雅可比 `A = −projectJac(Xc)·SE3deriv(Xc)`，`SE3deriv = [−skew(Xc) | I]`（`OptimizableTypes.cpp:49-63`）。
- 信息矩阵 `Ω = invSigma2(octave)·I`；Huber 核 `δ=√5.991`(mono)/`√7.815`(stereo)。
- IRLS 鲁棒权重 `w = rho[1] = min(1, δ/√χ²)`；`H += w·AᵀΩA`、`b −= w·AᵀΩr`（`base_unary_edge.hpp:56-67`、`base_edge.h:96`）。
- LM 求解器：阻尼 `H+λI`，`λ_init = 1e-5·maxdiag`，Nielsen 增益比，α∈[1/3,2/3]，连续 3 步无改善即停（`optimization_algorithm_levenberg.cpp`）。
- 4 轮外迭代：0–2 轮用 Huber 核，第 3 轮摘核；每轮按 χ² 阈值重分类内外点，边数 <10 提前停。

### 第一期测试结果

- `test_math`: PASS（Mat3 逆、四元数旋转、线性求解）
- `test_se3`: PASS（SO3/SE3 exp-log 往返、逆、左作用）
- `test_pose_opt`: PASS — mono 位姿误差 **2.31e-07**，stereo **1.16e-07**，内点 RMS **0.0000 px**，5/5 注入粗差全剔除

---

## 5. 第二期：ORB 提取 + Frame + StereoInitialization

完成日期：2026-06-30。新增 7 个模块。

| 模块 | 文件 | 对应原码 | 核心内容 |
|---|---|---|---|
| ORB 提取 | `orb.c/h` | `ORBextractor.cc` | 纯C FAST-9_16+NMS / 双线性金字塔 / IC_Angle / rBRIEF / 四叉树 |
| Hamming 距离 | `matcher.c/h` | `ORBmatcher.cc:2131` | 8×uint32 并行 popcount |
| 相机标定上下文 | `calib.c/h` | Frame 静态内参+尺度金字塔 | fx/fy/cx/cy/mbf/mb/thDepth + 尺度金字塔 |
| MapPoint(最小) | `mappoint.c/h` | `MapPoint.h` | worldPos/descriptor/normal/obs记录/nObs + ComputeDistinctiveDescriptors/UpdateNormalAndDepth |
| KeyFrame(最小) | `keyframe.c/h` | `KeyFrame.h` | id/位姿/相机中心/逐关键点数组 + 64×48 堆网格(PosInGrid/GetFeaturesInArea) |
| Frame(RGBD) | `frame.c/h` | RGBD Frame 构造 `Frame.cc:222-309` | ORB→去畸变(TUM3零畸变no-op)→ComputeStereoFromRGBD→网格 |
| 立体初始化 | `tracking.c/h` | `StereoInitialization` `Tracking.cc:2458-2492` | 首帧 N>500→单位位姿→建KF→逐关键点 UnprojectStereo 建 MapPoint |

### ORB 提取忠实性说明

`bit_pattern_31_`(256 对)、`umax`(16)、IC_Angle 公式、rBRIEF 旋转/位打包均逐行复制原码 → 描述子词袋兼容。以下为**重新实现**（非逐位等同 OpenCV），角点集合在边缘可能略有差异，但每个描述子仍是合法 ORB 描述子：
- FAST-9_16 检测 + NMS + 角点分数（自写，基数加速测试用正确的 `≥2` 必要条件）。
- 金字塔缩放（双线性，OpenCV `INTER_LINEAR` 像素中心约定）。
- 7×7 σ=2 高斯模糊（描述子用，reflect-101 边界）。
- `atan2f` 替代 `cv::fastAtan2`。
- 关键点内缩 ≥16px，最大采样半径 15 → 原 19px reflect-101 边界数值上不可达，存层不带边框 + 采样钳位（§7.3）。

### 第二期测试结果

- `test_orb`: PASS — **1009 特征 / 9 象限全覆盖 / 90° 旋转描述子 Hamming = 0**（rBRIEF 旋转不变性完美）
- `test_frame`: PASS — stereo-from-depth(`uR=u−mbf/Z`) + 网格分箱 + UnprojectStereo 全验
- `test_stereo_init`: PASS — **1009 MapPoints**，pos/nObs/descriptor/normal 逐项验证

---

## 6. 第三期：投影匹配 + 运动模型 + 简化局部地图

完成日期：2026-06-30。推进**不依赖 DBoW2 的 P0/P1 项**：`SearchByProjection`（两个重载）+ `isInFrustum` + `TrackWithMotionModel` + 简化 `TrackLocalMap`。`TrackReferenceKeyFrame`/重定位依赖 DBoW2，仍延后。

### 6.1 模块清单

| 模块 | 文件 | 对应原码 | 核心内容 |
|---|---|---|---|
| 投影匹配 #1 | `matcher.c` | `ORBmatcher.cc:47-146` | `ngd_search_by_projection_local`：依赖 isInFrustum 缓存投影、`RadiusByViewingCos·th`、ratio(0.8) 同 octave 才用、无方向直方图 |
| 投影匹配 #2 | `matcher.c` | `ORBmatcher.cc:1680-1860` | `ngd_search_by_projection_motion`：从零投影 `Tcw·worldPos`、`th·mvScaleFactors[lastOctave]`、无 ratio、bForward/bBackward、方向直方图 |
| 视锥判定 | `frame.c` | `Frame.cc:651-716` | `ngd_frame_is_in_frustum`：单目/RGBD 路径，填 track 缓存 |
| 尺度预测 | `mappoint.c` | `MapPoint.cc:539-554` | `ngd_mappoint_predict_scale`：`ratio=mfMaxDistance/dist`、`ceil(log/logScaleFactor)`、clamp |
| 跟踪上下文 | `tracking.h` | `Tracking` 类字段 | `ngd_tracking`：current/last/refKF/velocity/sensor/mState + localKFs/localMPs 堆列表 + PoseObs 暂存 |
| 运动模型 | `tracking.c` | `Tracking.cc:2901-2994` | `ngd_tracking_track_with_motion_model`：V·last.pose→#2→PoseOpt→外点剔除→nmatchesMap≥10 |
| 局部地图 | `tracking.c` | `Tracking.cc:3104-3204,3491-3563,3565-3704` | `ngd_tracking_track_local_map`：UpdateLocalMap(简化)+SearchLocalPoints+PoseOpt→mnMatchesInliers≥30 |

### 6.2 两处忠实性修正（原码与第二期偏差）

1. **`UpdateNormalAndDepth` 尺度不变量**：第二期用 `mvScaleFactor[nlevels-1]` 同时算 max/min，原码（`MapPoint.cc:476-499`）用 **refKF 观测 octave 的 scaleFactor** 算 max、`max/mvScaleFactor[nlevels-1]` 算 min。修正前 `mfMaxDistance` 被高估 ~3.58×（8 级@1.2），`isInFrustum` 距离门限失效。修正后用 `level = refKF->keys[leftIdx].octave`。`test_stereo_init` 不校验此量，无回归；`test_frustum` 显式覆盖。
2. **匿名 struct typedef 与前向声明不兼容**：`calib.h`/`frame.h` 的 `ngd_cam_ctx`/`ngd_frame` 原为匿名 struct typedef，与 matcher.h 中 `struct ngd_cam_ctx`/`struct ngd_frame` 前向声明是不同类型，导致 MSVC C4133/C4028/C2037。修正：给两个 struct 加标签 `typedef struct ngd_cam_ctx {...} ngd_cam_ctx;`。

### 6.3 简化与桩化

- **协视图未移植**：`UpdateLocalKeyFrames` 仅保留"当前帧已匹配 MP 的观测 KF 去重集合 + 投票最多者为 refKF"，**跳过** `GetBestCovisibilityKeyFrames(10)` 邻居扩展与 spanning-tree 父子扩展（`Tracking.cc:3679-3704`）。单/少 KF 场景无影响，多 KF 后需补协视图（P1）。
- **LocalMapping 线程未移植**：纯 C 单线程，`NeedNewKeyFrame`/`CreateNewKeyFrame` 桩化（`ngd_tracking_need_new_keyframe` 恒返 0），`mbFarPoints`/`mThFarPoints` 走桩字段（默认关闭）。
- **fisheye 右相机分支不做**（Nleft 恒 -1）：MapPoint 不加 `mbTrackInViewR`/`mTrackProjYR` 等右相机字段。

### 6.4 第三期测试结果

- `test_frustum`: PASS — isInFrustum 前/后/越界/角度>60°/距离越界/stereo uR 全验 + `update_normal_and_depth` 修正验证（mfMaxDistance=dist·sf[level] 而非 dist·sf[top]）+ `mfLogScaleFactor=log(sf[1])`。
- `test_projmatch`: PASS — #1（匹配/ratio 剪枝/Hamming>100 拒）、#2（恒等匹配/外点跳过/方向直方图 top-3 剪枝：20 对 bin0 + 1 对 bin6 → 保留 20）。
- `test_motion`: PASS — TWM 端到端，位姿恢复至真值 (0.00934,0.00556,0) 附近（实测 t=(0.0104,0.0079,0.0005), rot=0.0024rad），339 内点。
- `test_localmap`: PASS — TWM + TrackLocalMap，mnMatchesInliers≥30，localKFs={KF0}，localMPs>0。

---

## 7. 第四期：DBoW2 词表 + SearchByBoW + TrackReferenceKeyFrame

完成日期：2026-07-01。按用户确认的**路线 B（移植 DBoW2）**推进，范围 **Stage 1：到 TrackRefKF**（重定位的 score+倒排索引留 Stage 2），词表加载 **文本 + 二进制双格式**。落地 `TrackReferenceKeyFrame`，纯 C 前端在运动模型之外可走参考帧跟踪。

### 7.1 路线决策

`TrackReferenceKeyFrame`(`Tracking.cc:2767`) 用 `SearchByBoW`(DBoW2 词袋匹配) 做初始匹配——**不是**投影匹配。第三期因此延后。本期与用户确认走路线 B（移植 DBoW2）而非路线 A（投影匹配替代）。范围决策：Stage 1（到 TrackRefKF）+ 文本/二进制双加载，重定位（score+倒排索引+MLPnPsolver）留 Stage 2。

### 7.2 模块清单

| 模块 | 文件 | 对应原码 | 核心内容 |
|---|---|---|---|
| DBoW2 词表 | `bow.c/h` | `TemplatedVocabulary.h` | `load_text`(ORBvoc.txt)/`load_binary`(DBoW2 .bin,41B/节点)/`save_binary`；扁平 SoA 节点 + CSR 子节点；`transform`(levelsup=4)→BowVector(L1归一化)+FeatureVector |
| BowVector/FeatureVector | `bow.c` | `BowVector`/`FeatureVector` | 有序数组（collect→sort→merge，复用 `ngd_descriptor_distance`） |
| Frame/KF BoW | `frame.c`/`keyframe.c` | `Frame::ComputeBoW`/`KeyFrame::ComputeBoW` | `ngd_frame_compute_bow`/`ngd_keyframe_compute_bow`(lazy,levelsup=4)+bow/feat 指针字段 |
| SearchByBoW | `matcher.c` | `ORBmatcher.cc:227-429` | `ngd_search_by_bow`：两 FeatureVector 有序归并+同节点暴力+TH_LOW=50+ratio+30-bin方向直方图(左分支) |
| 参考帧跟踪 | `tracking.c` | `Tracking.cc:2767-2826` | `ngd_tracking_track_reference_keyframe` |

### 7.3 关键设计

- **词表内存布局**：ORB-SLAM3 大词表 k=10/L=6，1,082,074 节点 / 971,815 词。节点扁平 SoA（`parent`/`child_off`(CSR)/`child_idx`/`desc`(32B)/`weight`(double)/`word_id`），子节点 CSR。`n_children[i] = child_off[i+1]-child_off[i]`，不单独存字段（避免 k>255 截断隐患）。
- **transform**：root 逐层选 Hamming 最近子节点下推至叶子（`TemplatedVocabulary.h:1224-1265`）；`nid_level=m_L-levelsup`，ComputeBoW 用 levelsup=4 → level-2 节点。多描述子 collect `(wid,w)` 与 `(nid,i)` → 排序 → 合并去重（addWeight/addFeature）；TF_IDF+L1 → L1 归一化。
- **二进制格式**：核对 `TemplatedVocabulary.h:1458-1529`，DBoW2 .bin 头 `nb_nodes(u32) size_node(u32=41) k L scoring weighting`，节点 41B=`parent(i32)+desc[32]+weight(float)+is_leaf(bool)`。**weight 二进制为 float、文本为 double**——照搬此差异（内部统一 double，二进制读写时截断）。`save_binary` 写出与 DBoW2 兼容的 .bin。
- **SearchByBoW 左分支**：两 FeatureVector 有序归并（`lower_bound` 跳不重叠节点）；同 NodeId 内对每个有 MP 的 KF 特征扫 Frame 特征找 best1/best2；TH_LOW=50 + ratio(TrackRefKF=0.7)；30-bin 方向直方图 + ComputeThreeMaxima。**仅左相机分支**，右分支及其 `|| true` 调试残留不做。
- **scope 裁剪**：`score()`(L1 评分) Stage 1 不需要（TrackRefKF 不打分），留 Stage 2；BowVector 仍按 transform 忠实 L1 归一化。k-means 训练不做（只 load）。

### 7.4 第四期测试结果

- `test_bow`: PASS — 微词表(k=2/L=2) load_text 结构/transform(word/nid)/L1归一化/二进制往返 + 真实 ORBvoc.bin 校验(**1,082,074 节点 / 971,815 词**，transform 命中合法词)。
- `test_searchbow`: PASS — 干净匹配(2 跨节点匹配)/ratio 剪枝(best1=best2=0 拒)/节点分组(跨节点不匹配)。
- `test_trackrefkf`: PASS — 端到端，真实 ORBvoc，**198 内点**，**RMS 0.55px**（BoW 描述子精确匹配，远优于 TWM 的 ~1.3px），位姿恢复 t=(0.00898,0.00668,0.00036) 近真值 (0.00934,0.00556,0)，rot=0.00098rad。

---

## 8. 第五期：重定位 score + KeyFrameDB + EPnP + Relocalization

完成日期：2026-07-01。落地 Stage 2 重定位：`score()`(L1) + KeyFrameDatabase 倒排索引 + `DetectRelocalizationCandidates` + EPnP(RANSAC) + `Relocalization()`。第四期 BowVector 已 L1 归一化，直接接 score。**PnP 用 EPnP**（非原 MLPnPsolver）—— doc §6.5 已定方向，用户确认。同时补建 KeyFrame 共视图并顺手补回第三期简化掉的 UpdateLocalKeyFrames 协视图邻居扩展。

### 8.1 路线决策

PnP 求解器选择：原码用 MLPnPsolver（ML PnP，bearing-vector + nullspace + 稀疏 Gauss-Newton，依赖 Eigen，~600 行），纯 C 复现代价高且易错。EPnP 简化版（单最小特征向量）在 6 点最小集失败（零空间 4D 退化，smallest eigenvector 非真解）。故忠实移植 OpenCV `epnp.cpp` 完整版（4 控制点 + L6x10 + β 三近似法 + Gauss-Newton + Kabsch）。PoseOptimization 后续精修，EPnP 仅需给种子位姿 + RANSAC 内点分类。

共视图：`DetectRelocalizationCandidates` 依赖 `GetBestCovisibilityKeyFrames(10)` 做共视累积打分，第三期简化跳过。本期补建最小共视图（`AddConnection`/`UpdateConnections`/`GetBestCovisibilityKeyFrames`），spanning-tree parent/child 不做（reloc 不需要）。

### 8.2 模块清单

| 模块 | 文件 | 对应原码 | 核心内容 |
|---|---|---|---|
| L1 评分 | `bow.c` | `L1Scoring::score` `ScoringObject.cpp:23-68` | 双指针归并，同 word `|vi−wi|−|vi|−|wi|`，`−sum/2`，[0,1] |
| 共视图 | `keyframe.c` | `KeyFrame.cc:189-251,379-475` | `connKFs`/`connWeights` 降序；`UpdateConnections` 从 MP 观测重建（th=15），对称 `AddConnection` |
| 倒排索引 | `kfdb.c` | `KeyFrameDatabase.cc:39-98` | per-word KF 动态数组；`add`/`erase` |
| 候选检测 | `kfdb.c` | `KeyFrameDatabase.cc:733-845` | 共享词去重→0.8 maxWords→L1 score→共视累积(10)→0.75 bestAcc 保留 |
| EPnP | `pnp.c` | OpenCV `epnp.cpp` | 4 控制点(质心+PCA)/α/M(2N×12)/MtM 4 最小特征向量/L6x10+ρ/β 三法+GN/Kabsch；自写 Jacobi(3×3/12×12) |
| RANSAC | `pnp.c` | `MLPnPsolver` 接口 | `0.99,10,300,6,0.5,5.991`；6 点抽样→χ2 分类→全 inlier 精修 |
| 重定位 | `tracking.c` | `Tracking.cc:3757-3925` | ComputeBoW→Detect→SearchByBoW(0.75)→EPnP RANSAC→PoseOpt→nGood≥50 |

### 8.3 关键设计

- **EPnP 内部 double 精度**：MtM 特征值跨度大（1e8~1e10），float 精度下 4 个零特征值会被噪声淹没；全程 double，结果窄化为 float SE3。自写 `jacobi_sym`（循环 Jacobi，3×3 与 12×12 通用）求对称特征分解；`svd3`（一般 3×3 SVD，经 A^T A 特征分解 + U=AV/S）用于 Kabsch 与 PCA。
- **β 三近似法**：method 1(cols 0,1,3,6→6×4)、2(cols 0,1,2→6×3)、3(cols 0,1,2,3,4→6×5)，各配 5 次 Gauss-Newton 精修 β，取重投影误差最小者。最小二乘走正规方程（pnp.c 内自写 `lstsq_d`+`solve_linear_d`，因 math.h 的 `ngd_solve_linear` 是 float）。
- **solve_for_sign**：零空间特征向量符号任意，按首点相机 z>0 翻转（OpenCV 一致）。
- **Kabsch**：`ABt = Σ(pc−pc0)(pw−pw0)^T`，`R = U V^T`（det<0 翻第 3 行），`t = pc0 − R·pw0`。
- **RANSAC 简化**：原 `iterate(5,...)` 每轮 5 次迭代 + 外层 while 循环；`ngd_pnp_solve_ransac` 单次跑满 nIters（≈293），故 reloc 的外层 while 塌缩为单候选单次调用。固定 `srand(12345)` 确定性。
- **reloc <50 补匹配链简化**：原码 `nGood<50` 时两轮 `SearchByProjection`(10,100)/(3,64)（第三重载，未移植）；简化为单次 `ngd_search_by_projection_local`(th=3,ratio 0.9) over 候选 KF 的 MP（已匹配排除），再 PoseOpt。test_reloc 主路径已达 203 内点 ≥50，此分支不触发。
- **UpdateLocalKeyFrames 协视图扩展**：第三期简化版补回——观测 KF 集合基础上，对每个 KF `GetBestCovisibilityKeyFrames(10)` 扩展（`Tracking.cc:3679-3704` 协视图部分）；spanning-tree 父子扩展仍跳过。

### 8.4 第五期测试结果

- `test_score`: PASS — 相同=1.0/不相交=0.0/部分=0.8/单共享=0.5/对称/[0,1]。
- `test_kfdb`: PASS — 3 KF 倒排索引（word1 在 3 KF）；候选筛选 kf0(10 词,score1.0) 保留、kf2(6 词≤8) 排除；kf1(9 词,score0.9)；reloc 字段 stamp 正确。手工 BoW，无需词表。
- `test_pnp`: PASS — 60 点非共面，clean EPnP **rot_err=0.0000°, dt=0**；30% 外点 RANSAC 内点 46（expect ~42），位姿精确。
- `test_reloc`: PASS — 端到端，真实 ORBvoc，**203 内点**，**RMS 0.67px**，位姿 t=(0.00998,0.00415,−0.00497) 近真值 (0.00934,0.00556,0)，rot=0.004rad。

---

## 9. 第六期：LocalMapping 简化 KF 插入 + MapPointCulling

完成日期：2026-07-01。落地 P1 路线图「简化 LocalMapping（KF 插入 + 地图点剔除）」。此前 `ngd_tracking_need_new_keyframe` 是桩（恒返 0），无 Map 容器、无 KF 插入、无 MP 生命周期管理。本期补齐：Map 容器 + NeedNewKeyFrame + CreateNewKeyFrame + MapPointCulling。

**用户确认两个决策**：① 范围 = KF 插入 + MapPointCulling（KeyFrameCulling/CreateNewMapPoints 三角化/SearchInNeighbors Fuse/LocalBA 全延后 P2）；② 子函数暴露、测试手接（不补 Track() 总调度器，沿用前期风格）。

### 9.1 模块清单

| 模块 | 文件 | 对应原码 | 核心内容 |
|---|---|---|---|
| Map 容器 | `map.c` | `Map.cc:60-134` | KF/MP 动态数组(set 去重)+mnInitKFid/mnMaxKFid/mnLastKeyFrameId；add/erase |
| MP 字段 | `mappoint.c` | `MapPoint::mnFirstKFid`/`GetFoundRatio`/`SetBadFlag` | mnFirstKFid(new 设自 refKF)+get_found_ratio+set_bad(简化：标记+释放 obs，不清理共视图反向) |
| KF 计数 | `keyframe.c` | `KeyFrame::TrackedMapPoints` | tracked_map_points(nMinObs) |
| LocalMapping | `localmapping.c` | `LocalMapping.cc:64-290,354-393` | recent_mps+currentKF+push_recent+map_point_culling+run(culling+P2 桩) |
| NeedNewKeyFrame | `tracking.c` | `Tracking.cc:3220-3360` | 非 IMU：c1a/c1b/c1c/c2 + NGD c5(惰性)；reloc 冷却；thRefRatio 0.75/0.9/0.4 |
| CreateNewKeyFrame | `tracking.c` | `Tracking.cc:3363-3489` | 深拷贝 frame→KF；RGBD 深度点建 MP(depth 升序,maxPoint=100)；ComputeBoW+KFDB+update_connections+map+recent |

### 9.2 关键设计

- **Map 容器**：原码 `std::set<KeyFrame*>`/`std::set<MapPoint*>`；纯 C 用动态数组+线性去重（P1 量级可接受）。erase 只删指针不 free 对象（与 Map.cc 一致，对象生命周期由 KF/MP owner 管）。多地图/Atlas/IMU 标志不做。
- **NeedNewKeyFrame 简化**：去 IMU 分支（c3/c4/VIBA）；LocalMapping idle 恒真（单线程内联，LM 在 create 后同步跑完 culling）；`mbOnlyTracking` 无此模式；近距离点统计 `bNeedToInsertClose`（RGBD：`depth>0&&<thDepth` 的 tracked/nontracked，<100 tracked && >70 nontracked）。c5（NGD 光流触发）保留但 `mbStartOpticalFlow` 默认 0 → 惰性，待光流移植激活。
- **CreateNewKeyFrame RGBD 深度点建 MP**：忠实 `Tracking.cc:3395-3480`——按 depth 升序排，对 `z>0` 且 (`!mp || Observations()<1`) 的点 `UnprojectStereo`→`mappoint_new`(mnFirstKFid=kf->mnId)→`add_observation`→`compute_distinctive_descriptors`→`update_normal_and_depth`→`map_add_mappoint`→`push_recent`；`maxPoint=100`，`z>mThDepth && nPoints>100` break。鱼眼右图分支不做（Nleft 恒 -1）。
- **单线程 LM**：原码 LM 是独立线程（Run 循环 + 队列）；纯 C 塌缩为 `ngd_local_mapping_run`（culling + P2 桩），由调用者在 `create_new_key_frame` 后显式调。`ProcessNewKeyFrame` 的职责（ComputeBoW/关联 MP/UpdateConnections/AddKeyFrame）合并进 `create_new_key_frame`，`run` 只做 culling。
- **MapPointCulling**：忠实 `LocalMapping.cc:354-393`。cnThObs=3（RGBD/stereo）。age=`currentKF->mnId - mp->mnFirstKFid`。found_ratio<0.25 或 (age≥2 & obs≤3) → set_bad+移除；age≥3 → 仅移除（成年出列，MP 存活）；已 bad → 移除。
- **set_bad 简化**：原码 `SetBadFlag` 还要清理共视图反向引用 + 从 KF mvpMapPoints 擦除 + map erase。本期简化版只标记 mbBad + 释放 obs 数组（P1 culling 后 MP 不再被访问，足够；完整清理留 P2 Fuse/KeyFrameCulling 时补）。

### 9.3 第六期测试结果

- `test_map`: PASS — add 3 KF(dedup/mnInitKFid=0/mnMaxKFid=9)+2 MP(dedup)+erase(swap-pop)+erase absent no-op。
- `test_mappointculling`: PASS — 7 MP 覆盖各分支：bad(移除)/lowfound(ratio0.1<0.25→bad+移除)/young_lowobs(age2,obs2≤3→bad+移除)/young_ok(age1→保留)/mature(age4≥3→移除存活)/healthy(age2,obs5>3→保留)/age2_obs4(age2,obs4>3→保留)；recent 7→3。
- `test_needkf`: PASS — 5 用例：强跟踪近帧→0；弱跟踪+c1a→1；reloc 冷却(nKFs>mMaxFrames)→0；c1a 但强跟踪(c2假)→0；弱+c1a 过冷却→1。
- `test_kfinsert`: PASS — 端到端 5 帧(mMaxFrames=2)，frame1/2 各插一 KF(map.nKFs 1→2→3)，culling recent 1444→772(剔除近半)，kf0 共视图度数=2(与两新 KF 协视)，refKF/mnLastKeyFrameId 推进。frame3-5 因 mnMatchesInliers 仍高(>nRefMatches*0.75)未触发 c2，符合 NeedNewKeyFrame。

### 9.4 第六期踩坑

- **test_needkf CHECK 宏缺 `}`**：`#define CHECK(...) do { if(){...} } while(0)` 误写成 `... } while(0)`（少 do 闭花括号）→ MSVC C1075 EOF 未匹配。对照 test_localmap.c 的 `} } while(0)` 修复。非算法缺陷。

---

## 10. 第七期：LocalBundleAdjustment Schur 补 BA

完成日期：2026-07-01。落地 P2 路线图「LocalBundleAdjustment（稀疏 Schur，g2o 最难，可降级）」。本期用纯 C 忠实复现 g2o `BlockSolver_6_3` + `OptimizationAlgorithmLevenberg` 的 Schur 补 BA，替代 g2o，可独立单元测试，并接入 `ngd_local_mapping_run()` 取代第六期 P2 桩。

**用户确认方向**：四个 P2/P3 候选（LBA / YOLO+mask / TrackWithOpticalFlow / Sim3+LoopClosing）中选 LocalBundleAdjustment，延续前六期纯 C g2o 替代路线。

### 10.1 模块清单

| 模块 | 文件 | 对应原码 | 核心内容 |
|---|---|---|---|
| 局部 BA 核心 | `ba.c/h` `ngd_ba_optimize` | `Optimizer.cc:1116-1500` + g2o `BlockSolver_6_3`/`OptimizationAlgorithmLevenberg`/`base_binary_edge.hpp`/`robust_kernel_impl.cpp` | 双精度内部 Schur 补 BA：边残差 `r=obs−project(Tcw·Xw)`；雅可比 `J_point=−Pjac·R`、`J_pose=−Pjac·SE3deriv(Xc)`（与 `OptimizableTypes.cpp:139-160` 一致）；Huber IRLS ρ₁=`min(1,δ/√χ²)`；块结构 `Hpp/Hll/Hpl/bp/bl`（固定位姿不进 Hpp/Hpl/bp，但贡献 Hll/bl）；Schur `Hschur=Hpp+λI − Σ_j Hpl_{·j}·(Hll_jj+λI)⁻¹·Hpl_{·j}ᵀ`、`bschur=bp − Σ_j Hpl_{·j}·Dinv·bl_j`；稠密 LU 解位姿系统；回代 `dxl_j=Dinv_j·(bl_j−Hpl_{·j}ᵀ·dxp)`；Nielsen LM（λ_init=1e-5·maxdiag，增益比 `ρ=(χ_old−χ_new)/(dxᵀ(λdx+b)+1e-3)`，α∈[1/3,2/3]，内层 10 试步）；oplus 左扰动 `T=exp(ξ)·T`+点 `p+=dx`；外点分类 χ²>5.991/7.815 或深度≤0 |
| 局部 BA 收集 | `ba.c` `ngd_local_ba` | `Optimizer::LocalBundleAdjustment` `:1116-1500` | 收集 lLocalKFs(pKF+共视邻居)/lLocalMPs(局部 KF 的 MP 去重)/lFixedKFs(观测局部 MP 但非局部)；0 固定则中止（init KF 计固定）；建扁平 problem（obs 来自 `keys[i].x/y`+`uRight`，invSigma2 来自 `mvInvLevelSigma2[octave]`，stereo 当 `uRight≥0`）；10 轮 LM；外点解绑观测（`mp_erase_observation`：swap-pop obs + 减 nObs + 清 KF mvpMapPoints 槽，忠实 `EraseObservation`+`EraseMapPointMatch`）；写回非固定局部 KF 位姿+局部 MP 位置 |
| BA 戳字段 | `keyframe.h`/`mappoint.h` | `KeyFrame.h:338-340`/`MapPoint.h` | `mnBALocalForKF`/`mnBAFixedForKF`（集合去重，零初始化增量） |
| LocalMapping 接线 | `localmapping.c` `run` | `LocalMapping.cc:64-290` | P2 LBA 桩→`ngd_local_ba(currentKF,map,NULL)`（守卫 `nKFs≥4`） |

### 10.2 关键设计

- **双精度内部**：g2o BA 全程 double（`SE3Quat`/`VertexSBAPointXYZ` 均 double，原码 `Tcw.cast<double>()`）。`ba.c` 自带 double 镜像 `se3_exp_d`/`quat_to_R_d`/`se3_deriv_d`/`inv3_d`/`solve_dense_d`，位姿/点在边界 float↔double 转换（`pnp.c` 先例）。不复用 `se3.c` 的 float 版。
- **Schur 而非稠密**：位姿(6·n_free)+点(3·n_MP) 全 Hessian 稠密解对真实规模（百点）不可行。Schur 把点边际化掉，只解 6·n_free 位姿系统（n_free<20），再逐点 3×3 回代。每 LM 外层迭代重线性化一次（g2o `buildSystem` 一次/iter），内层试步仅变 λ（复用线性化，g2o `setLambda`/`restoreDiagonal`）。
- **Hpl CSR**：每点存其 free-pose 观测边的 (pose_idx, 6×3 块) 列表；Schur 双循环 `(i1,i2)∈free obs of j` 做 `Hschur_{i1,i2} −= Hpl_{i1,j}·Dinv·Hpl_{i2,j}ᵀ`（k_j 通常 2-4，k_j² 小）。
- **固定位姿处理**：忠实 g2o `toNotFixed` 守卫——固定位姿不进 Hpp/Hpl/bp，但其观测边仍贡献 `Hll_jj`/`bl_j`（点块）。故 `Dinv` 含所有观测姿态的贡献，位姿系统只含 free 姿态。`n_free` 用 packed index 0..n_free−1。
- **外点剔除**：LM 结束后单次分类（非 4 轮，与 `PoseOptimization` 不同——LBA 不做轮间 chi2 重分类，仅 Huber 降权 + 事后剔除）。wrapper 中 `mp_erase_observation` 用边构建时捕获的 `edge_kf/edge_left/edge_mp` 直接定位（避免 swap-pop 后重扫描失配，见 10.4 踩坑）。
- **g2o 细节忠实**：ρ₁-only（ρ₂ 二阶项被注掉，`base_edge.h:96-102`）；λ 同时阻尼 Hpp 与 Hll 对角（`setLambda` 双循环）；b 带负号 `b=−JᵀΩr`，dx 直接 oplus；`computeScale` 对全变量 `[dx_p|dx_l]` 求和。

### 10.3 第七期测试结果（共 22 个 PASS）

- `test_ba` T1 无扰不自漂移：3 位姿(2 固定)+40 点 mono，10 轮 LM 后 max rot=0、max pt=5.1e-6（float 往返噪声）。
- `test_ba` T2 mono 恢复：3 位姿(2 固定,固定 2 个锚定尺度避免单目尺度退化)+50 点，扰动 0.015m/0.004rad，恢复 rot=0、t=1.4e-7、pt=8.7e-6，150/150 全内点。
- `test_ba` T3 stereo 恢复：3 位姿(1 固定,基线锚定尺度)+50 点，恢复 rot=0、t=1.2e-7、pt=3.1e-6，150/150 全内点。
- `test_ba` T4 外点剔除：注入 3 粗差(±60px)，标出 6 外点(3 注入+3 边界)，内点位姿 rot=0 恢复。
- `test_ba` T5 端到端 `ngd_local_ba`：3 KF(kf0 init 固定/kf1/kf2)+24 共享 MP+共视图，rc=0，kf0 不变(固定✓)，kf1 移动 0.0083，kf1/kf2 收敛至 GT(误差<5e-5)，nMP=24。
- 旧 21 测试无回归。

### 10.4 第七期踩坑

- **`ngd_keypoint` 字段名**：原码 `cv::KeyPoint.pt.x`，refactor 的 `ngd_keypoint` 是扁平 `x/y`（非 `pt.x`）。wrapper 初版写 `k->keys[i].pt.x` 编译失败，对照 `frame.c:38` 改 `.x`。
- **`ngd_keyframe` 无 `mbBad`**：refactor 不建模 KF bad-flag（仅 MP 有 `mbBad`）。wrapper 初版 `k->mbBad` 检查编译失败（C2039），改为 `if(!k)` 跳过。
- **匿名 struct 前向声明不兼容（同 9.6）**：`ba.h` 前向声明 `struct ngd_map;`，但 `map.h` 是匿名 `typedef struct {...} ngd_map;` → C4133/C4028。给 `map.h` 加标签 `typedef struct ngd_map {...}` 修复（与 9.6 对 `ngd_cam_ctx`/`ngd_frame` 的修法一致）。
- **残差数组未初始化**：核心初版「initial chi2」pass 把 `edge_linearize` 的 `r` 写进局部变量而非 `&er[e*3]`，导致 assembly 用到未初始化 `er`。改为直接传 `&er[e*3]`。
- **外点解绑重扫描失配**：初版事后重扫 `mp->obs[]` 重映射 edge→(kf,leftIdx)，但 `mp_erase_observation` 的 swap-pop 改变 obs 数组 → `e` 计数器错位。改为边构建时存 `edge_kf/edge_left/edge_mp`，事后直接定位。
- **单目尺度退化**：测试初版 mono 用 1 固定位姿，纯 2D 单目 BA 尺度不可观 → 恢复差。改用 2 固定位姿锚定尺度（stereo 靠基线锚定）。

---

## 11. 第八期：CreateNewMapPoints 三角化

完成日期：2026-07-01。落地 P2 路线图 LocalMapping 余项的「CreateNewMapPoints 三角化」。补齐 `LocalMapping::Run` 的 culling→**CreateNewMapPoints**→LBA 顺序（SearchInNeighbors Fuse / KeyFrameCulling 仍留）。忠实移植 `LocalMapping.cc:396-720` + `ORBmatcher::SearchForTriangulation:911-1150` + `GeometricTools::Triangulate` + `Pinhole::epipolarConstrain`。

### 11.1 模块清单

| 模块 | 文件 | 对应原码 | 核心内容 |
|---|---|---|---|
| 三角化 DLT | `matcher.c/h` `ngd_triangulate` | `GeometricTools.cc:47-66` | 4×4 A（行 `x_ci[k]·Tc_i.row2 − Tc_i.row(k)`，k=0,1；i=1,2），x3D = AᵀA 最小特征值特征向量（4×4 循环 Jacobi，镜像 pnp.c::jacobi_sym 两通道旋转）去齐次。w≈0（无穷远点）返回 0 |
| 三角化匹配 | `matcher.c/h` `ngd_search_for_triangulation` | `ORBmatcher.cc:911-1150` | FeatureVector 有序归并；跳过 kf1 有 MP 的特征 + kf2 已匹配/有 MP；极点距离拒（`|ep−kp2|²<100·sf²[kp2.oct]`，仅 !stereo1&&!stereo2）；F12 基础矩阵极线约束（`Pinhole.cpp:107-129`：`F12=K1⁻ᵀ·[t12]×·R12·K2⁻¹`，`l=F12ᵀ·kp1h`，`dsqr=(l·kp2h)²/(a²+b²)<3.84·σ²[kp2.oct]`）；TH_LOW=50 best-only（无 ratio）；30-bin 方向直方图+ComputeThreeMaxima 剪枝。`bCoarse` 跳过极线门（IMU RECENTLY_LOST 用，refactor 非 IMU 恒 false） |
| KF 反投影 | `keyframe.c/h` `ngd_keyframe_unproject_stereo` | `Frame.cc:1043+` | `Xc=(u−cx)·z/fx, (v−cy)·z/fy, z`；`Xw=Twc·Xc`（低视差 stereo 分支用，深度直接反投影） |
| 新建地图点 | `localmapping.c/h` `ngd_local_mapping_create_new_map_points` | `LocalMapping.cc:396-720` | nn=10 共视邻居（RGBD；mono 30 + medianDepth 未做）；基线<mb 跳过；SearchForTriangulation→逐匹配视差分支：`cosParallaxRays<cosParallaxStereo && >0 && (stereo‖<0.9998)` → DLT Triangulate；`stereo1 && cosParStereo1<cosParStereo2` → UnprojectStereo(kf1)；stereo2 对称；else 跳过（低视差无 stereo）。cosParStereo=cos(2·atan2(mb/2, depth))。前向 z>0 双 KF；重投影 mono `err²<5.991·σ²`/stereo `+errXr²<7.8·σ²` 双 KF（uR 用 currentKF mbf，忠实原码 cc:673）；尺度 `ratioDist·ratioFactor(1.5·sf) vs ratioOctave(sf1[kp1.oct]/sf2[kp2.oct])` 双侧拒；建 MP+AddObservation 双 KF（stereo rightIdx=i 使 nObs+2，Stage 6 约定）+mvpMapPoints+map+recent。`run()` 接线 culling→create_mps→LBA |

### 11.2 关键设计

- **4×4 Jacobi 必须两通道**：初版自写 Jacobi 用特殊情形公式（手算 app'/aqq' + r≠p,q 行更新），结果特征向量错乱（triangulate(0,0,2) 返回 (0.18,0,−2.84)）。改镜像 pnp.c::jacobi_sym 的两通道旋转（先列更新 `A[i][p]=c·aip+s·aiq; A[i][q]=−s·aip+c·aiq` 全 i，再行更新全 i，再 V 更新），J=[[c,−s],[s,c]]，`phi=0.5·atan2(2·apq, app−aqq)`（pnp.c 第五期已验证的符号约定）。修复后 triangulate(0,0,2)=(0,0,1.99)。
- **nObs 立体计数**：原 `AddObservation` 对 `!mpCamera2 && mvuRight[idx]>=0` 计 nObs+=2。refactor `add_observation(kf,leftIdx,rightIdx)` 用 `rightIdx>=0?2:1`。为让三角化 MP（2 KF 观测）在 culling(cnThObs=3 RGBD) 存活，stereo 观测传 `rightIdx=i`（Stage 6 CreateNewKeyFrame 同约定）→nObs=4>3。rightIdx 仅用于计数，compute_distinctive_descriptors/update_normal_and_depth/LBA wrapper 都只用 leftIdx，无副作用。
- **covisibility 鸡生蛋**：CreateNewMapPoints 靠共视图找邻居 KF，而共视图由共享 MP 建立——但被三角化的 MP 还不存在。原系统靠 tracking（TWM/TrackRefKF 把 kf0 的 MP 匹配给 kf1）先建立共享 MP→共视。测试照此：先 seed NTRACK 个共享 MP（点 0..19，双 KF 各 add_observation）建协视，留 NTRACK..NPTS−1 未匹配给三角化。
- **极线约束 F12**：`T12=Tcw1·Twc2`（Twc2=kf2 pose 逆），`R12=T12.R, t12=T12.t`。极点 `C2=Tcw2·Cw`（kf1 中心在 kf2 系），`ep=project(C2)`（C2.z≤0 时置远避免除零）。`K1invT=(K1⁻¹)ᵀ`、`K2inv=K2⁻¹`，Pinhole K=[[fx,0,cx],[0,fy,cy],[0,0,1]]。
- **简化/桩化**：mbMonocular medianDepth 分支未做（refactor RGBD-only）；mbFarPoints/mThFarPoints 远点剔除未建模；IMU/dual-cam-right 分支不做（Nleft 恒 -1）；bCoarse 恒 false。

### 11.3 第八期测试结果（共 23 个 PASS）

- `test_createmps` T1：2 KF（kf0 init/kf1 平移 0.15m）+40 点（20 共享 MP 协视+20 未匹配，真实 ORBvoc，跨 KF 同点描述子相同），create_new_map_points 三角化 20 新 MP，位置误差<1e-4 m，双 KF 观测+map+recent 齐全。
- T2：kf2 平移 0.02m（baseline<mb=0.0747）→0 新 MP。
- T3：点 NTRACK 的 kf1 描述子篡改（0xFF）→该点不三角化，其余 ≥10 仍三角化。
- 旧 22 测试无回归（test_kfinsert 的 run() 现含 create_new_map_points，仍 PASS）。

### 11.4 第八期踩坑

- **Jacobi 特殊情形公式错**（见 11.2）：自写 4×4 Jacobi 用手算对角更新+排除 p,q 的行更新，特征向量错（DLT 解漂到 (-0.18,0,2.84) 方向）。根因是手算的 `app'=c²app−2sc·apq+s²aqq` 等与两通道隐式旋转的更新顺序不一致（混了对称保持假设与逐行更新）。改两通道全 i 旋转（pnp.c 范式）后正确。**教训：小矩阵 Jacobi 直接照搬已验证实现，勿手算特殊情形。**
- **`mvScaleFactors` 拼写**：calib.h 字段是 `mvScaleFactor`（无 s），初版误写 `mvScaleFactors` → C2039。
- **`featvec_lower_bound` 前向声明**：ngd_search_for_triangulation 被插在 motion 与 featvec_lower_bound 定义之间（replace_all 替换两处 `free(matchBin)...return nmatches;}` 末尾），导致调用先于定义 → C4013/C4211。加 static 前向声明修复。

---

## 12. 第九期：SearchInNeighbors Fuse + KeyFrameCulling

完成日期：2026-07-01。落地 LocalMapping P2 余项的最后两步：`SearchInNeighbors/Fuse`（cc:722-831）与 `KeyFrameCulling`（cc:910-1062），补入 `run()` 的 `culling→create_mps→search_in_neighbors→keyframe_culling→LBA` 顺序（原 Run 全顺序）。至此 LocalMapping 的 P2 项（LBA + CreateNewMapPoints + Fuse + KeyFrameCulling）全部移植完毕。

**用户确认方向**：四个 P2/P3 候选（LBA / YOLO+mask / TrackWithOpticalFlow / Sim3+LoopClosing）已在第七/八期选 LBA+三角化；本期为 LocalMapping P2 余项收尾，延续纯 C 忠实移植路线。

### 12.1 模块清单

| 模块 | 文件 | 对应原码 | 核心内容 |
|---|---|---|---|
| Fuse 原语去重 | `matcher.c/h` | `ORBmatcher::Fuse` `cc:1152-1342` | 修复 `ngd_fuse` 在 matcher.c 中**重复定义两次**(:222 与 :646，byte-identical)且未在 header 声明的遗留问题——删第二份、加 matcher.h 原型。body 不改（已忠实：投影+距离带 0.8×min/1.2×max+视角 PO·normal<0.5·dist(cos60°)+尺度门 kpLevel∈[nPred−1,nPred]+stereo 7.8/mono 5.99 χ²+TH_LOW=50+按 Observations() 多寡 Replace，**无方向直方图/ComputeThreeMaxima**——那些仅用于 SearchByBoW/SearchByProjection） |
| EraseObservation | `mappoint.c/h` `ngd_mappoint_erase_observation` | `MapPoint::EraseObservation` `cc:168-201` | 线性找 kf 的 obs 记录→`nObs −= (rightIdx≥0)?2:1`→swap-pop obs+`nObsRec−−`→清 KF `mvpMapPoints[leftIdx]` 槽(EraseMapPointMatch)→若 `refKF==kf` 重选 `obs[0].kf`→`nObs<=2` 则 `set_bad`("only 2 observations or less") |
| KF bad 标志 | `keyframe.c/h` | `KeyFrame::mbBad`/`isBad`/`EraseConnection`/`SetBadFlag` `cc:573-701` | 加 `mbBad` 字段；`is_bad` 访问器；`erase_connection`（线性找 target + shift 删保降序，不用 swap-pop 破序）；`set_bad` 非惯性简化版：守卫 init KF(`mnId==map->mnInitKFid`)→快照 connKFs→逐邻居 `erase_connection(nb,this)` 对称剪边→逐 `mvpMapPoints[i]` `erase_observation(mp,this)`(可能触发 MP set_bad)→清自身 connKFs/connWeights→`mbBad=1`→`map_erase_keyframe`+可选 `kfdb_erase` |
| SearchInNeighbors | `localmapping.c/h` `ngd_local_mapping_search_in_neighbors` | `LocalMapping::SearchInNeighbors` `cc:722-831` | `nn=mbMonocular?30:10`；1 级 target=当前 KF 协视邻居(`mnFuseTargetForKF` stamp 去重)；2 级=各 target 的 `GetBestCovisibilityKeyFrames(20)`（`imax` 取快照不递归新 appended）；惯性时间链跳过。**快照当前 KF mvpMapPoints 一次**（GetMapPointMatches 返回 copy 的忠实——Fuse 中 Replace 会改 mvpMapPoints，迭 live 数组跨多 target 会偏离原码）→逐 target `ngd_fuse(target, vpMPmatches, N, 3.0)`；收集 targets 的非 bad MP(`mnFuseCandidateForKF` stamp)→`ngd_fuse(currentKF, cand, nC, 3.0)`；重算 currentKF 存活 MP descriptor/normal+`update_connections`；**事后扫除 map 中 bad(replaced) MP**（忠实 `Replace` 的 `mpMap->EraseMapPoint`——MP 无 map 指针故由驱动承担） |
| KeyFrameCulling | `localmapping.c/h` `ngd_local_mapping_keyframe_culling` | `LocalMapping::KeyFrameCulling` `cc:910-1062` | 非惯性 RGBD：`redundant_th=0.9`、`thObs=3`、仅近 stereo 点(`depth>mThDepth||depth<0` 跳过)；逐 currentKF 协视 KF（**快照 connKFs**，set_bad 会改邻居表）：跳过 init/bad；对每个非 bad close MP：`nMPs++`，若 `observations()>3` 则遍历 `mp->obs[]`(排除自身) 统 `scaleLeveli<=scaleLevel+1` 的他 KF 数，`>3` 则 `nRedundantObservations++`；`nRedundant>0.9·nMPs`→`set_bad(k,map,kfdb)`；`count>100` 安全 break(`mbAbortBA` 不建模) |
| LocalMapping 接线 | `localmapping.c` `run` | `LocalMapping::Run` `cc:64-290` | `culling→create_mps→search_in_neighbors→keyframe_culling→LBA`(nKFs≥4 守卫)；删 P2 stub 注释 |
| kfdb 字段 | `localmapping.h` `ngd_local_mapping` | `LocalMapping::mpKeyFrameDB` | 加 `kfdb` 指针(caller-owned,可 NULL)+`set_kfdb` setter（不动 `init` 签名，避免改 5 处现有调用）；`set_bad` 据此调 `kfdb_erase` |

### 12.2 关键设计

- **KF mbBad 补建**：第七期 WORKLOG §10.4 已记"refactor 不建模 KF bad-flag"。本期 KeyFrameCulling 非惯性分支正是 `pKF->SetBadFlag()`(cc:1054) 剔 KF，必须补。`set_bad` 忠实非惯性简化：spanning-tree 父子重挂不做（refactor 未建模，reloc/协视不需要）、`mbNotErase`/`mbToBeErased`（回环预留）不建模、`UpdateBestCovisibles` 显式重排省略（`connKFs` 由 `add_connection`/`update_connections` 保降序不变式）。
- **EraseObservation vs ba.c::mp_erase_observation**：第七期 ba.c 内 static `mp_erase_observation`（LBA 外点解绑用）是简化版——**不**做 `nObs<=2→set_bad` 与 refKF 重选。本期 KeyFrameCulling 的 KF set_bad 需要"剔 KF 后其独占 MP 也变 bad"的完整语义，故在 mappoint.c 加**忠实版** `ngd_mappoint_erase_observation`（含 nObs≤2→set_bad + refKF 重选）。ba.c 的 static 版**不动**（surgical，不同路径，LBA 外点解绑不需要 set_bad 副作用）。
- **map erase 责任归属**：原 `MapPoint::Replace`/`SetBadFlag` 自带 `mpMap->EraseMapPoint(this)`/`mpMap->EraseKeyFrame(this)`。refactor 的 MP/KF **无 map 指针**（前八期约定）：`ngd_mappoint_replace`/`set_bad` 不 erase map。KF 的 map erase 由 `set_bad` 直接调（有 map 参）；MP 的 map erase 由 `SearchInNeighbors` 驱动事后扫除承担（`Replace` 在 Fuse 内被调，Fuse 无 map 参）。`MapPointCulling` 的 set_bad 同样不 erase map（前八期既有简化，bad MP 留 map.mps 被 mbBad 跳过）。
- **快照防迭代失效**：两处关键快照——(1) `search_in_neighbors` 调 Fuse 前快照当前 KF mvpMapPoints（原码 GetMapPointMatches 返回 copy）；(2) `keyframe_culling` 快照 currentKF->connKFs（set_bad 会 erase_connection 改邻居表）。二者皆防"迭 live 容器同时被改"。
- **`ngd_fuse` 重复定义修复**：matcher.c 中 `ngd_fuse` 被贴了两份 byte-identical（:222/:646），未在 header 声明。第二份用 Python 按行删 638-725（两份含 em-dash 注释完全相同，Edit 无法唯一定位，故用行范围删），加 matcher.h 原型。

### 12.3 第九期测试结果（共 25 个 PASS）

- `test_fuse` PASS — 端到端 search_in_neighbors：2 KF+30 点(10 共享协视+20 重复 MP 同位同描述子)。融合 20 重复：**mpA 全 bad(20/20)、mpB 全存活(20/20)、mpB 双 KF 观测(20/20)、kf0/kf1 槽均转 mpB(20/20)、map 无 bad MP(扫除)、nMPs 50→30**。无 BoW 依赖（Fuse 用 Hamming）。
- `test_keyframeculling` PASS — 端到端 keyframe_culling：5 KF(init+3 观测+1 冗余 kfR)、20 共享 MP(5 KF 观测)+1 kfR 独占。**kfR 剔除(mbBad=1)、map.nKFs 5→4、独占 MP bad(nObs 2→0≤2)、20 共享 MP 存活且 4 KF 观测、kfR 从 kf3 协视表剪除**；3 观测各 3 unique MP 使冗余率 20/23=0.87<0.9 不被剔。无 BoW 依赖。
- 旧 23 测试无回归（test_kfinsert 的 run() 现含 search_in_neighbors+keyframe_culling，仍 PASS）。

### 12.4 第九期踩坑

- **`ngd_kfdb` 匿名 struct**（同 9.6/10.4 模式）：keyframe.h 前向声明 `struct ngd_kfdb;`，但 kfdb.h 是 `typedef struct {...} ngd_kfdb;`（匿名，无 tag）→ keyframe.c `set_bad` 形参 `ngd_kfdb*` 与 header `struct ngd_kfdb*` 是不同类型 → MSVC C4028。修：kfdb.h 加 tag `typedef struct ngd_kfdb {...}`（与 9.6 对 `ngd_cam_ctx`/`ngd_frame`、10.4 对 `ngd_map` 的修法一致）。
- **`ngd_keyframe_get_best_covisibility_keyframes` 参数数**：该函数签名是 3 参 `(kf,N,out)`（非 4 参，无 max_out——与 `ngd_frame_get_features_in_area` 不同）。初版两处调用都误传 4 参 → C2197。修：删第 4 参，调用方按 `min(N,nConn)` 静态保证 out 缓冲足够（nn≤30→buf[64]，20→buf[32]）。
- **test_keyframeculling 双重 free**：`mp_excl`（kfR 独占 MP）被 `set_bad`→`erase_observation` 标 bad 但**未出 map.mps**（KeyFrameCulling 不扫除 bad MP，只有 SearchInNeighbors 扫），仍由 `map.mps` 循环 free；初版又单独 `ngd_mappoint_free(mp_excl)` → 双重 free。修：删单独 free，注释说明。

---

## 13. 第十期：NGD 动态 mask 链（纯 C 核心）

路线图 P2 的 NGD 专有部分——动态 mask 预测链（`Tracking::PredictCurrentMask` 及四个 helper，`src/Tracking.cc:4249-4587`）。本期交付 **mask 生成核心**；mask 消费侧（Frame 过滤 / `SearchByOpticalFlow` / `TrackWithOpticalFlow` 复用 EPnP）留第十一期，YOLO `cv::dnn` 薄壳 + 线程模型留第十二期。

**关键架构发现**：整条链唯一的硬 OpenCV 依赖是 `cv::calcOpticalFlowPyrLK`。其余（腐蚀/膨胀/连通域/DBSCAN/FAST 角点势/bbox+深度门）全部纯 C 重写，故核心保持 0 依赖、可单测；光流作为**可注入回调** `ngd_lk_flow_fn`（薄壳侧绑 LK 默认参 winSize21/maxLevel3/COUNT+EPS 30·0.01，测试侧注入合成流）。与 doc §6.7「光流=薄壳」、第五期 EPnP 替代 MLPnP 同思路。

### 13.1 模块清单

新模块 `include/ngd/mask.h` + `src/mask.c`：

| 函数 | 对应原码 | 说明 |
|---|---|---|
| `ngd_mask_erode/dilate` | `cv::erode/dilate`(MORPH_RECT) | 二值形态学(2·half+1 核)；越界邻点忽略(erode=窗口内 AND，dilate=OR)，等价 min/max 滤波，与 OpenCV BORDER_CONSTANT 边界一致；支持 in==out 别名(内部 scratch 副本) |
| `ngd_mask_pixel_potential` | `ComputePixelPotential` `:4308-4352` | 4 十字(±3)角点响应；`isBrighter/isDarker>=3`→200+minDiff，否则 max(bright,dark) |
| `ngd_mask_extract_dyna_points` | `ExtractDynaPoints` `:4355-4399` | cellSize=15 网格、threshold=250，mask==1 区取最大势像素、`>250` 早退，`bestPixel>0&&<w/h` 入选 |
| `ngd_mask_cluster_dbscan` | `ClusterWithDBSCAN` `:4402-4483` | 两阶段：深度预分段(sort by z，gap>0.1 或窗>0.5 切段，子段>20→z 改写 200·level；全空回退用原点)+3D DBSCAN(eps/minPts)；`labels[n]` 映射回原点(>=1 簇号/-1 噪声) |
| `ngd_mask_connected_components` | `cv::connectedComponentsWithStats`(4 连通) | BFS 队列；返回标签数(含背景 0) |
| `ngd_mask_create_from_clusters` | `CreateMaskFromClusters` `:4486-4587` | 每簇 bbox+扩15/`width或height<50` 跳过/深度门±0.3/最大连通域写1/末尾 dilate 7×7；中位深度(pointsDepth 排序取 [n/2]) |
| `ngd_mask_predict_current` | `PredictCurrentMask` `:4249-4305` | 编排：腐蚀 last_mask 副本(11×11 half=5)→取种子→LK(cb)→深度过滤(NaN/越界/depth>=0.05)→DBSCAN(50,15)→create；不改调用方 last_mask |

### 13.2 忠实性修正（潜伏 bug 不传入 C）

- **`ComputePixelPotential` 未初始化**：原码 `isBrighter`/`isDarker`(`:4325`)未初始化→C 版置 0。
- **死边界检查**：原码 `potential=-1`(`:4319`)紧接被 `potential=0`(`:4322`)覆盖，近边像素越界读 `imGray.at`→C 版任一十字邻点越界即返回 -1（原意图，`ExtractDynaPoints` 近边格子因此不出种子）。
- **DBSCAN 预分段尾段不冲刷**：原码循环结束不 flush 最后 tempPoints（只有被违反点关闭的段才入 segmentedPoints）→C 版原样保留；常见单深度带场景走「segmentedPoints 空→回退用全部原点」分支。
- **`CreateMaskFromClusters` 的 `hullMask`**：原码画了但后续未用（死代码）→C 版不建。
- **就地腐蚀**：原码 `erode(mImMaskLastKey, mImMaskLastKey, ...)` 就地改 `mImMaskLastKey`，但帧末被覆盖无持久副作用→C 版腐蚀 scratch 副本，不改调用方 `last_mask`。

### 13.3 第十期踩坑

- **DBSCAN 邻居缓冲溢出 → 0xc0000374(堆损坏)**：初版 `nbr` 按 `nseg` 定长 malloc，但 DBSCAN expand 把每个核心点的 newNeighbors 追加进邻居集（**含重复**，忠实原码 `neighbors.insert(end, newNeighbors)` 的 vector 语义），总量可超 `nseg`（test_dbscan 20 点簇：regionQuery 返 20，expand 每点再 append 20 → 20+20×20=420 ≫ 41）→ 堆溢出。修：`nbr` 改可增长 `intvec`(realloc 倍增)；`newnbr` 仍定长 `nseg`(单次 regionQuery ≤nseg)。**教训：移植 std::vector 动态增长语义时 C 缓冲必须动态分配，勿按单次查询容量定长。**
- **MSVC C4244**：test 里 `pts[k].x = 100 + 5*(i%5)`(int→float) 6 处警告，加 `(float)` 强转清零（`/W4` 0 warning 标准）。

### 13.4 第十期测试结果（共 26 个 PASS）

`test_mask` 7 用例（纯 assert，合成 LK 回调注入纯平移）：
- morphology(腐蚀缩/膨胀涨/别名安全)
- pixel_potential(3 邻点亮=327/平坦全等等=-1/近边越界=-1)
- extract_dyna_points(单 mask 格 1 种子/空 mask→0)
- cluster_dbscan(两紧簇+孤立噪声，同 z 走回退分支)
- connected_components(两块+背景=3 标签)
- create_from_clusters(大簇 bbox≥50 填充/深度门±0.3 清零 off-median 块/bbox<50 跳过)
- **端到端 predict_current**：3px 棋盘灰度(每内点 4 十字反相→势 455>250)，mask 70×70 种子区(质心 ~45,45)，合成 LK shift(20,10)→预测 mask 质心 **(65.0,55.0)** 精确匹配 种子质心+shift；空 last_mask→空输出。

构建干净：**0 error / 0 warning**。

---

## 14. 第十一期：MLPnP 求解器（纯 C 核心）

补回原 NGD-SLAM 重定位实际使用的 **MLPnP**（Maximum-Likelihood PnP，`src/MLPnPsolver.cpp`，Penate-Sanchez et al.），作为 EPnP 之外第二个 PnP 求解器。第五期曾因 "MLPnP 依赖 Eigen、纯 C 复现代价高" 改用 EPnP；本期补齐，与 EPnP 并存，后续再定取舍（用户明示"后续再看怎么使用"）。API 形状对齐 EPnP，输出 `ngd_se3`，核心 0 依赖。

### 14.1 模块清单

新模块 `include/ngd/mlpnp.h` + `src/mlpnp.c`（约 760 行）。`computePose`（`MLPnPsolver.cpp:356-658`）七阶段：

| 阶段 | 原码行 | 说明 |
|---|---|---|
| A 零空间 | 367-375 | 每点 bearing f 的 2-D 右零空间（3×2）。原 `JacobiSVD(f^T)` 取 V 后两列；C 版 eigendecomp `f f^T`（3×3 对称，`jacobi_sym` 升序）取两最小特征值向量（`f f^T` 秩 1：特征值 `|f|²,0,0`） |
| B 平面检测 | 377-399 | `planarTest=points3·points3^T`，rank==2→平面（9 列，否则 12 列）。rank = 特征值 > 1e-12·max\|λ\| 的个数（Eigen dummy_precision） |
| C 设计矩阵 A | 425-512 | 2N×12 / 2N×9，逐项 `ns(k,·)·pt_coord`，**逐行照搬** |
| D 最小二乘 | 514-524 | `AtPA=A^T·A`，`JacobiSVD` 取最后右奇异向量 = 最小特征向量；C 版 `jacobi_sym` 对称 eigendecomp 取 col 0 |
| E 位姿恢复+符号判定 | 526-637 | 非平面：reshape→tmp、scale=`1/(\|col0·col1·col2\|^(1/3))`、Frobenius `U V^T`(`svd3`)、det<0 翻号、±t 重投影 `1-v·f` 判定；平面：4 组合（R1/R2×±t）。4×4 逆用闭式 `[[R^T,-R^T·t],[0,1]]` 免通用求逆 |
| F Gauss-Newton | 639-658, 694-758 | `rot2rodrigues`→x=[ω;t]→5 轮 GN（epsP=1e-5，发散守卫 max\|dx\|>5\|\|min\|dx\|>1，收敛 max\|Jac·dx\|<epsP），正规方程 `Jac^T·Jac·dx=Jac^T·r` 用 `solve_linear_d` 替 LDLT→`rodrigues2rot` |
| 残差+雅可比 | 760-806, 808-1055 | `mlpnp_residuals_and_jacs`（ptCam 归一化，r=ns^T·ptCam）+ `mlpnpJacs`（~200 行 t# 手写符号雅可比，**逐字转录**） |
| G RANSAC | iterate 100-223 | 采样 6→computePose→CheckInliers(`err²<sigma2·th2`)→≥minInliers 记 best→Refine（全 inliers computePose+CheckInliers） |

**Rodrigues**：`rodrigues2rot`(:660)/`rot2rodrigues`(:677) 逐字转录。

### 14.2 忠实性修正

- **丢弃 ML 协方差路径**（死代码）：原码 `iterate`/`Refine` 恒传 `covs(1)`（size 1），`computePose` 守卫 `covMats.size()==N`（N>5）永假→`use_cov=false`、P=I、`AtPA=A^T·A`、`Jac^T·Kll` 退化为 `Jac^T`。整条稀疏 linalg（SparseMatrix、2×2 逆）从未启用→C 版不移植（忠实于实际行为，非源码死分支），核心保持 0 依赖。`sigma2` 仅用于 RANSAC 内点门限。
- **Bearing 形式**：原码 `unproject→/=z` 得 `(x/z,y/z,1)`（**非单位**）；refactor 仅 pinhole，故 `((u-cx)/fx,(v-cy)/fy,1)`。符号判定的 `1-v·f` 依赖此非单位形式→保留。GN 残差里 ptCam 另归一化为单位（照原码不一致处照搬）。
- **输出 Tcw**（world→camera），与 EPnP 一致；原码 `Matrix4f`，Tracking 亦按 Tcw 用。**实证**：clean/RANSAC 恢复位姿与真值 Tcw 完全吻合（rot=0/dt=0），Twc 不吻合（rot=30°）。
- **线性代数自包含**（refactor 惯例，同 `pnp.c`/`ba.c` 各带 double 镜像）：自带 `jacobi_sym`(3/9/12)/`svd3`/`solve_linear_d`，不改 `pnp.c`（零风险）。
- **RANSAC 形状**：原码 `iterate(5,...)` 增量模型，循环条件 `mnIterations<maxIts \|\| nCurrentIterations<nIterations` 实际跑到 maxIts（nIterations=5 被 OR 短路）→C 版批量 `ngd_mlpnp_solve_ransac`（跑满 maxIts、记 best、Refine、耗尽返 best），净行为等价，与 EPnP 同形状。`srand(12345)` 确定性。
- **`SetRansacParameters` 指数硬编码 3**（:253 `pow(eps,3)` 而非 minSet=6）原样保留（疑似原码 latent bug，忠实照搬）。

### 14.3 第十一期踩坑

- **GN 段 use-after-free**：初版 `compute_pose` 在符号判定后 `free(ns)`，但 GN 准备段又 `NS(gns,...)=NS(ns,...)` 从已释放的 `ns` 拷贝→改为 GN 段从 ctx bearings 重新算 nullspace（不依赖 `ns`）。
- **MSVC C4189**：平面分支声明 `double X=p3[i*3+0]` 但平面设计矩阵只用 `Y/Z`（无 r11/r21/r31）→删 X。C4244 在 test_pnp 已清，本测试无。

### 14.4 第十一期测试结果（共 27 个 PASS）

`test_mlpnp`（60 点非共面，TUM3 内参，对齐 `test_pnp`）：
- **clean** `ngd_mlpnp_solve`：rot_err=0.0000°、dt=0.00000（机器精度恢复，GN 在无噪数据上收敛到位姿精确解）。
- **RANSAC 30% 外点**：46 内点（期望~42，+4 在 sigma2·th2=5.99px 门限内）、位姿 rot=0/dt=0。
- **N<6 守卫**：返 0。

构建干净：**0 error / 0 warning**。

---

## 15. 忠实性核对（逐行对照原码）

所有关键数值约定均直接核对源码，非凭记忆：

| 约定 | 核对来源 | 一致性 |
|---|---|---|
| 四元数 Hamilton w-first `{w,x,y,z}` | Sophus/Eigen | ✅ |
| Mat3 行主序 `m[row*3+col]` | Eigen `Matrix3f` | ✅ |
| SE3 左扰动 `T_new=exp(ξ)·T`，exp 用完整 SE3 指数(含左雅可比 V) | `se3quat.h:223`、`types_six_dof_expmap.h:73` | ✅ |
| 位姿雅可比 `A=−projectJac·SE3deriv`，`SE3deriv=[−skew(Xc)\|I]` | `OptimizableTypes.cpp:49-63` | ✅ |
| IRLS 权重 `rho[1]=min(1,δ/√χ²)` | `base_unary_edge.hpp:56-67`、`base_edge.h:96` | ✅ |
| LM 阻尼 `H+λI`、`λ_init=1e-5·maxdiag`、Nielsen、α∈[1/3,2/3] | `optimization_algorithm_levenberg.cpp` | ✅ |
| 4 轮结构 0-2 用核/3 摘核，chi2 阈值 5.991/7.815 | `Optimizer.cc:852,1001-1002` | ✅ |
| ORB `bit_pattern_31_`/`umax` 逐字复制 | `ORBextractor.cc:149-407,451-468` | ✅ |
| rBRIEF 旋转 `row=px·sin+py·cos`、`col=px·cos−py·sin`、bit0=LSB | `ORBextractor.cc:117-143` | ✅ |
| RGBD 立体 `uR=u−mbf/Z`，深度已按 DepthMapFactor 缩放为米 | `Frame.cc:1126-1147` | ✅ |
| 网格 64×48，`GetFeaturesInArea` 用 `|dx|<r && |dy|<r` 方框 | `Frame.cc:799-865` | ✅ |
| TUM3 零畸变 → 去畸变 no-op | `TUM3.yaml:16-19`、`Frame.cc:891-894` | ✅ |
| StereoInit N>500 门限 | `Tracking.cc:2460` | ✅ |
| `RadiusByViewingCos` cos>0.998→2.5 else 4.0 | `ORBmatcher.cc:219-225` | ✅ |
| `ComputeThreeMaxima` top-3 + `0.1·max1` 剪枝 | `ORBmatcher.cc:2085-2126` | ✅ |
| SearchByProjection #1 缓存投影+ratio(0.8)同 octave+无直方图 | `ORBmatcher.cc:47-146` | ✅ |
| SearchByProjection #1 stereo `er=\|mTrackProjXR-mvuRight\|>r·sf` 跳过 | `ORBmatcher.cc:96-100` | ✅ |
| SearchByProjection #2 现算投影 `Tcw·worldPos`、`th·mvScaleFactors[lastOctave]`、无 ratio | `ORBmatcher.cc:1680-1771` | ✅ |
| SearchByProjection #2 bForward/bBackward 由 `tlc.z` vs `mb` | `ORBmatcher.cc:1696-1737` | ✅ |
| 方向直方图 `factor=1/30`、`bin=round((angLF-angCF)/30)%30`、撤销非 top-3 | `ORBmatcher.cc:1688,1788-1795,1869-1888` | ✅ |
| isInFrustum 单目路径填 track 缓存（Nleft==-1） | `Frame.cc:651-716` | ✅ |
| `mTrackProjXR=uv.x−mbf/PcZ`、`mTrackDepth=\|Pc\|` | `Frame.cc:705,707` | ✅ |
| `PredictScale` `ratio=mfMaxDistance/dist`、`ceil(log/logScaleFactor)`、clamp | `MapPoint.cc:539-554` | ✅ |
| `mfLogScaleFactor=log(mvScaleFactor[1])`（level1，非 level0） | `Frame` 静态 | ✅ |
| `UpdateNormalAndDepth` 用观测 octave（修正第二期 max-level bug） | `MapPoint.cc:476-499` | ✅ |
| `GetMax/MinDistanceInvariance` = 1.2·max / 0.8·min | `MapPoint.cc:510-519` | ✅ |
| TrackWithMotionModel V·last.pose、th=7/15、<20 重试 2th、nmatchesMap≥10 | `Tracking.cc:2901-2994` | ✅ |
| TrackLocalMap ≥30(非IMU)、SearchLocalPoints th=3(RGBD)/1、isInFrustum 0.5 | `Tracking.cc:3104-3204,3491-3563` | ✅ |
| UpdateLocalKeyFrames 简化（跳过协视图邻居扩展 cc:3679-3704） | `Tracking.cc:3605-3704` | ⚠️ 简化 |
| UpdateLocalPoints 逆序+`mnTrackReferenceForFrame` 去重 | `Tracking.cc:3575-3602` | ✅ |
| NeedNewKeyFrame/CreateNewKeyFrame 桩化（LocalMapping 线程未移植） | `Tracking.cc:3220-3489` | ⚠️ 桩 |
| ORBvoc.txt 文本格式 头`k L scoring weighting`+节点行`parent isLeaf d[32] weight(double)` | `TemplatedVocabulary.h:1344-1430` | ✅ |
| ORBvoc.bin 二进制 41B/节点 `parent(i32)+desc[32]+weight(float)+is_leaf(bool)` | `TemplatedVocabulary.h:1458-1529` | ✅ |
| 词表节点扁平 SoA + CSR 子节点；叶子按文件序赋 word_id | `TemplatedVocabulary.h:1382-1426` | ✅ |
| transform root 逐层选 Hamming 最近子节点；`nid_level=m_L-levelsup` | `TemplatedVocabulary.h:1224-1265` | ✅ |
| ComputeBoW `levelsup=4` → nid_level=2(level-2 节点) | `KeyFrame.cc:105`/`Frame.cc:885` | ✅ |
| TF_IDF+L1 scoring → BowVector L1 归一化(`sum|v_i|=1`) | `TemplatedVocabulary.h:1117-1128`、`BowVector.cpp:62-84` | ✅ |
| FeatureVector 同节点特征索引升序（addFeature 按 i_feature 追加） | `FeatureVector.cpp:31-45` | ✅ |
| SearchByBoW 两 FeatureVector 有序归并+同节点暴力+TH_LOW=50+ratio | `ORBmatcher.cc:227-429` | ✅(左分支) |
| SearchByBoW 方向直方图 30-bin `factor=1/30`、`bin=round((angKF-angF)/30)%30` | `ORBmatcher.cc:240,342-357` | ✅(左分支) |
| SearchByBoW 右分支及 `|| true` 调试残留 | `ORBmatcher.cc:300-327,361-389` | ⚠️ 不做(Nleft==-1) |
| TrackRefKF ComputeBoW→SearchByBoW(0.7)→<15 失败→SetPose(last)→PoseOpt→nmatchesMap≥10 | `Tracking.cc:2767-2826` | ✅ |
| Fuse 投影+距离带 0.8×min/1.2×max+视角 cos60°(0.5·dist)+尺度门+stereo 7.8/mono 5.99 χ²+TH_LOW=50+Replace(无 Merge/无直方图) | `ORBmatcher.cc:1152-1342` | ✅(左分支) |
| SearchInNeighbors nn=10/30+1st/2nd 阶协视 stamp+Fuse 双向+重算 desc/normal+UpdateConnections | `LocalMapping.cc:722-831` | ✅(非IMU) |
| MapPoint::Replace 转移观测+继承 found/visible+重算 desc/normal+mbBad+map erase | `MapPoint.cc:248-300` | ✅(map erase 由驱动扫除) |
| EraseObservation 减 nObs(stereo×2)+删记录+清槽+refKF 重选+nObs≤2→set_bad | `MapPoint.cc:168-201` | ✅ |
| SetBadFlag 守卫 init KF+对称 EraseConnection+逐 MP EraseObservation+清表+mbBad+map/kfdb erase | `KeyFrame.cc:573-679` | ✅(非惯性,无 spanning tree) |
| KeyFrameCulling redundant_th=0.9+thObs=3+近 stereo 点+scaleLeveli≤scaleLevel+1+nRedundant>0.9·nMPs→SetBadFlag | `LocalMapping.cc:910-1062` | ✅(非IMU) |

---

## 16. 调试踩坑记录

第二期调试中发现并修复 4 个关键问题（已记入 memory `refactor-c-core-design.md`）：

### 9.1 FAST 基数测试必要条件错误

- **现象**：干净的角点图像提取出 0 个特征。
- **根因**：FAST-9 的 9-contiguous 弧线最多只跨 4 个基数点中的 2 个，我误写必要条件为 `hi<3 && lo<3`（要求 ≥3），过于激进。
- **修复**：改为 `hi<2 && lo<2`（9-contiguous 必跨 ≥2 基数点）。

### 9.2 四叉树坐标系不一致

- **现象**：`test_stereo_init` segfault，`test_frame` 网格分箱失败（关键点偏移到 x≈640 越界）。
- **根因**：`ngd_fast_detect` 写入绝对金字塔坐标，但 `DistributeOctTree`（及随后的 `+minBorder` 步骤）期望 minBorder-相对坐标，导致关键点最终被 +16 双重偏移。
- **修复**：分发前先 `raw[k].x −= minBorderX`，分发后保留原码的 `kept[i] += minBorder` 加回。

### 9.3 四叉树 `kept` 缓冲容量不足

- **现象**：`test_orb` 提取 1008/1000（quadtree overshoot），`kept` 缓冲按 `nfeatures+8` 分配导致溢出 segfault。
- **根因**：四叉树节点数可能略超预算。
- **修复**：缓冲改为 `nfeatures*2 + 64`。

### 9.4 MapPoint `nObs` 与观测记录数混淆

- **现象**：`test_stereo_init` segfault（读 `obs[1]` 野指针）。
- **根因**：stereo 点 `nObs=2`（立体计 2）但只存 1 条观测记录；`ComputeDistinctiveDescriptors`/`UpdateNormalAndDepth` 误用 `i < nObs` 遍历，对 stereo 点读到未初始化的 `obs[1].kf`。
- **修复**：新增 `nObsRec`（观测记录数）字段，遍历用 `nObsRec`，`nObs` 仅作语义计数（与原码 `MapPoint::nObs` 含义一致）。

### 9.5 test_se3 断言写错（非代码缺陷）

- **现象**：`test_se3` 一项失败。
- **根因**：断言 `exp(ξ)·T == exp(log(T))` 仅在 T 为单位时成立，而 T 是任意位姿。
- **修复**：改为断言优化器真正依赖的左作用不变式 `map(exp(dx)·T, p) == map(exp(dx), map(T, p))`。

### 9.6 匿名 struct typedef 与前向声明不兼容（第三期）

- **现象**：matcher.c 编译报 `C2037 "cam"的左侧指定未定义的 struct/union "ngd_frame"`、`C4133`/`C4028` 类型不兼容。
- **根因**：`calib.h`/`frame.h` 的 `ngd_cam_ctx`/`ngd_frame` 是匿名 struct typedef（`typedef struct {...} T;`），而 matcher.h 前向声明 `struct ngd_frame;`/`struct ngd_cam_ctx;` 指向另一个**永不定义**的 tagged 类型，二者不兼容；matcher.c 解引用 `F->cam` 时该 tagged 类型仍不完整。
- **修复**：给两个 struct 加标签 `typedef struct ngd_cam_ctx {...} ngd_cam_ctx;` / `typedef struct ngd_frame {...} ngd_frame;`，使前向声明与 typedef 同型。

### 9.7 UpdateNormalAndDepth 尺度不变量用错 octave（第三期）

- **现象**：`test_frustum` 距离越界用例假阳（点本应被距离门限拒绝却通过）。
- **根因**：第二期 `ngd_mappoint_update_normal_and_depth` 用 `mvScaleFactor[nlevels-1]`（max level）算 `mfMaxDistance`，高估 ~3.58×（8 级@1.2），`isInFrustum` 距离门限 `[0.8·minD, 1.2·maxD]` 失效。原码（`MapPoint.cc:476-499`）用 **refKF 观测 octave 的 scaleFactor**。
- **修复**：查 obs[] 找 refKF 的 leftIdx → `level=refKF->keys[leftIdx].octave` → `mfMaxDistance=dist·mvScaleFactor[level]`、`mfMinDistance=mfMaxDistance/mvScaleFactor[nlevels-1]`。重跑 6 旧测试无回归。

### 9.8 load_binary is_leaf 偏移错误（第四期）

- **现象**：`test_bow` 二进制往返 `n_words=0`（所有节点判为非叶）；真实 ORBvoc.bin 加载出 880704 词（错误计数）。
- **根因**：`ngd_bow_vocab_load_binary` 读 `is_leaf` 用了 `buf[4+NGD_BOW_DESC_LEN]`=`buf[36]`（weight float 首字节），应为 `buf[4+32+4]`=`buf[40]`。DBoW2 节点 41B 布局 `parent(i32) + desc[32] + weight(float) + is_leaf(bool)`，is_leaf 在 offset 40。错读成 weight 首字节，恰好对部分 IDF 权重为非零 → 假叶子。
- **修复**：改为 `buf[4 + NGD_BOW_DESC_LEN + 4]`。修复后真实 ORBvoc.bin 词数 = 971,815（与文本 grep 估计一致），微词表二进制往返 n_words=4 正确。

### 9.9 FeatureVector 同节点特征顺序（第四期，qsort 非稳定）

- **现象**：`test_bow` 3 描述子 transform 断言 `featvec node1 idx={0,1}` 偶发失败（顺序变 `{1,0}`）。
- **根因**：`ngd_bow_transform` 用 collect→`qsort`→group 构建 FeatureVector；`cmp_featkey` 仅按 `nid` 排序，同 nid 内 qsort 非稳定 → 特征索引顺序不定。DBoW2 `FeatureVector::addFeature` 按 `i_feature` 升序追加，同节点内索引应升序。
- **修复**：`cmp_featkey` 增加 `feat` 次级升序排序（同 nid 内按特征索引升序），与 DBoW2 一致。

### 9.10 test_motion RMS 断言过严（第三期，非代码缺陷）

- **现象**：`test_motion` 初版断言重投影 RMS<0.5px 失败（实测 1.30px，339/1009 内点）。
- **根因**：合成图硬整像素平移在 ORB 金字塔下非平移不变（边界 + 级联下采样效应），frame1 特征不严格等于 frame0+(5,3)，导致约 2/3 错配被 PoseOpt 的 Huber 剔除；剩余内点带 ~1px 残差。这是场景伪影，非移植 bug。
- **修复**：断言改为恢复位姿近真值（`|tx-0.00934|<1e-2` 等，特征铺开 X/Y 已打破恒定深度的 tx↔yaw 退化）+ 宽松 RMS<5px 上界。

### 9.11 EPnP alphas 栈溢出（第五期）

- **现象**：`test_pnp` 退出码 `0xc0000409`（栈饼干破坏）；`test_reloc` SegFault。
- **根因**：`epnp_ctx.alphas` 误声明为固定 `double alphas[4]`，但 `compute_barycentric` 写 `alphas[i*4+...]` for i in 0..N−1（N=60 → 写 240 个 double 进 4 元素数组），冲垮栈。
- **修复**：改为 `double *alphas`，`epnp_compute_pose` 内 `malloc(N*4*sizeof(double))`，用完释放。

### 9.12 Jacobi 旋转角符号错（第五期，EPnP 零空间丢失）

- **现象**：EPnP clean 位姿 rot_err=47.7°（错）；调试发现 MtM（M^T M，M 为 EPnP 投影矩阵）的 4 个最小特征值应为 ~0（零空间），实际 6e8~1.5e9（无零空间）。自写 `jacobi_sym` 在已知 rank-2 的 B^T B（4×4，应有两个 0 特征值）上返回全非零 → jacobi 本身坏。
- **根因**：循环 Jacobi 旋转角公式写成 `phi = 0.5·atan2(2·apq, aqq−app)`，而按本代码的 J 约定（`J=[[c,−s],[s,c]]`，更新 `A'=J^T·A·J`）零化 A[p][q] 需 `tan(2θ)=2·apq/(app−aqq)`，即 `phi = 0.5·atan2(2·apq, app−aqq)`。符号反了 → 旋转不零化 off-diagonal → 不收敛到真特征分解（trace 守恒但个别特征值错）。
- **修复**：改为 `app−aqq`。自测恢复两个 ~0 特征值（off_resid 2e-29），EPnP clean rot_err=0。
- **教训**：Jacobi 角公式必须与所用 J 约定（J 还是 J^T 作用方向、s 符号）一致，照搬教材公式而不核对自己的更新方向会出此错。

### 9.13 test_kfdb float 精度断言（第五期，非代码缺陷）

- **现象**：`kf1->mRelocScore == 0.9` 断言失败（实际 0.89999997）。
- **根因**：`mRelocScore` 是 float，`(float)0.9 ≈ 0.89999997`，与 double `0.9` 差 2.4e-8 > 1e-9 容差。kf0=1.0 因 1.0 精确可表示故通过。
- **修复**：容差放宽到 1e-6。

---

## 16. 第十二期 a–g：P3 Sim3 + LoopClosing + OptimizeEssentialGraph（2026-07-02）

路线图 P3 收尾。NGD-SLAM 剩余纯算法模块，无 OpenCV 依赖（符合 YOLO-dnn/光流/主函数不重构约束）。单地图/非 IMU/非 merge 简化口径。

### 16.1 Sim3 李群（12a）
- `sim3.c`：exp/log 逐字照搬 g2o::Sim3（`sim3.h:70-229`）含 σ≈0 / θ≈0 四分支；`W=A·Ω+B·Ω²+C·I`；`map=s·(R·p)+t`；`inverse/multiply/to_se3=[R|t/s]`。复用 `ngd_math`/`ngd_so3`/`ngd_solve_linear`（log 中 `υ=W⁻¹·t` 用线性求解替代 g2o 的 LU）。
- `test_sim3`：4 分支 exp/log 往返 + map 公式 + inverse∘map=identity + s=1 退化为 SE3 + compose 结合律。

### 16.2 Sim3Solver（12b）
- `sim3solver.c`：Horn 1987 闭式（质心→M=Pr2·Pr1ᵀ→4×4 N→最大特征值特征向量→axis-angle→SO3::exp→尺度 nom/den→平移）+ 3 点 RANSAC。自含 `jacobi_sym`（4×4 对称，升序，复用 pnp.c 约定）。内点门 `9.210·σ²`。确定性 LCG（替 DUtils::Random）。
- **关键点**：API 用扁平数组（X3Dc1/X3Dc2/P1im1/P2im2/maxErr1/maxErr2 + origIdx 映射），驱动从 KF/MP 抽取，沿用 pose_opt/pnp 风格。`check_inliers` 双向投影 χ²（sR12·X2c+t12→cam1，sR21·X1c+t21→cam2）。
- **踩坑**：初版把 mBest* 同时当迭代槽 + 最优槽（CUR_* 宏）。分析后确认逻辑正确——只在 `mnInliersi>=mnBestInliers` 分支返回，且返回时 mBest*=当前=新最优；clobber 仅在不达 minInliers 时发生，而该路径返 identity（与原码 `iterate` 非 bConverge 重载一致）。保留但加注释。
- `test_sim3solver`：60 点 s=1.2+R+Δt，clean rot=0/dt=0/s≈1.2；30% 外点 RANSAC ≥38 内点位姿精确；bFixScale 强制 s=1。

### 16.3 OptimizeSim3（12c）
- `sim3opt.c`：单 Sim3 顶点 + 双向重投影边，点固定。**数值雅可比**（中心差分，h=1e-4）——g2o 的 `EdgeSim3ProjectXYZ::linearizeOplus` 被注释掉，运行时用数值微分（与 SE3 路径不同），故忠实复现用数值而非转录 g2o 都不用的解析式。LM(H+λI, Nielsen) + Huber(δ=√th2) IRLS。5 轮剔外点→摘核→5/10 轮（对齐 `Optimizer.cc:2306-2353`）。
- **踩坑 1**：h=1e-6（初版）对 float 太小（残差差 ~1e-6 被 ~1e-4 投影舍入淹没）→ 雅可比噪声 → 不收敛。改 h=1e-4（√eps）后 SNR~10，收敛到机器精度。
- **踩坑 2（致命）**：初版在函数入口缓存 `S21=inverse(*S12)`，Phase 1 LM 更新 *S12 后用**过期** S21 做外点分类 → 全标外点 → `nCorr-nBad<10` 返 0（但优化已收敛 err=1.7e-7）。修复：分类前重算 `S21=inverse(*S12)`。
- `test_sim3opt`：扰动 rot0.05/dt0.1/s1.05 恢复 <1e-3；bFixScale 下 s 锁定（init s=1）；粗差注入剔除且 GT 仍恢复。

### 16.4 SearchBySim3（12d）
- `matcher.c::ngd_search_by_sim3`：忠实移植 `ORBmatcher.cc:1461-1678`。**计划备注有误**：实际用 TH_HIGH=100（非 TH_LOW=50）、**无**方向直方图（非 top-3 剪枝）、best-only、octave 门[nL-1,nL]、双向一致性。加 `mp_index_in_kf` 静态助手（扫描 obs）替 GetIndexInKeyFrame。
- `test_searchbysim3`：2 KF+共享 MP+精确 S12，40/40 匹配且全部同世界点；描述子篡改→该点不匹配。

### 16.5 KF-KF SearchByBoW（12d 附带，LoopClosing 检测需要）
- 抽取 `search_by_bow_core`（原 ngd_search_by_bow 函数体参数化为原始数组：featvec/desc/mp/keys），DRY 复用于 KF-vs-Frame（`ngd_search_by_bow`）与 KF-vs-KF（`ngd_search_by_bow_kf`）。原码两重载都有，忠实。重跑 test_searchbow/trackrefkf/reloc 指标完全不变。

### 16.6 OptimizeEssentialGraph（12e）
- `essgraph.c`：全 KF Sim3 顶点（init 固定）+ 共视图边(weight≥100, id<j, dedup) + 回环边。EdgeSim3 残差 `err=log(meas·Siw·Sjw⁻¹)`（7-D，info=I_7，无 Huber），**数值雅可比**（同 sim3opt，g2o EdgeSim3 无 linearizeOplus 重载→运行时数值）。稠密 LM 20 轮（H=(7N)²，λ_init=1e-16 对齐 setUserLambdaInit；固定顶点行列清零+对角置1）。写回 `[R|t/s]` + MP 纠正（`P'=(post Twc_ref)·(pre Tcw_ref·P)`）。
- **简化**：spanning-tree parent 边 + KF mLoopEdges 不建模（refactor KeyFrame 无此字段），共视图边承载主干。
- **踩坑**：初版用 GCC 语句表达式宏 `IDXOf({...})`（MSVC 不支持）→ 改静态函数 `idx_of`。
- `test_essgraph`：3 KF 链+共视边(真值测量)+漂移初始化，优化后 kf1/kf2 收敛真值 t_err=0/r_err=0。

### 16.7 GlobalBA（12g）
- `ba.c::ngd_global_ba`：复用 `ngd_ba_optimize` 全 KF(init 固定)+全 MP+全观测，薄包装。无独立测试（test_ba 已覆盖核心），test_loopclosing 内部调用验证。

### 16.8 LoopClosing 编排（12f）
- `loopclosing.c`：`detect_common_regions`（BoW L1 候选扫描→KF-KF SearchByBoW≥20→Sim3Solver RANSAC≥15→SearchBySim3≥50→OptimizeSim3≥20，保留最优）+ `correct_loop`（传播 loopScw 沿共视图→MP 纠正(mnBALocalForKF 戳去重)→loop fusion(Replace/Add)→SearchAndFuse(ngd_fuse)→建回环边→EssentialGraph→GBA）+ 单线程 `run`（队列→detect→correct）。
- **简化**：候选检测用 BoW L1 扫描 map KFs（替 DetectNBestCandidates，避免 KF 版 reloc detect）；跳过时序传播(DetectAndReffineSim3FromLastKF)/多 KF 一致性(nNumKFs≥3)/SearchByProjection-Sim3 变体（未移植），用 SearchBySim3 替代。
- **踩坑**：`ngd_bow_vocab` 在 bow.h 是**匿名** typedef（`typedef struct {...} ngd_bow_vocab;` 无 tag），loopclosing.h 初版前向声明 `struct ngd_bow_vocab` 制造了不同类型（§9.6 同类问题）→ MSVC C4028/C4133 警告。修复：loopclosing.h include `kfdb.h`(→bow.h) 拿到 typedef，字段/参数改用 `ngd_bow_vocab`（非 `struct ngd_bow_vocab`）。
- `test_loopclosing`：3 KF 共享 MP，**一致刚体漂移 D 注入全部 3 KF**（保证共视边局部一致），注入 loopScw=true 当前位姿，correct_loop 后 kf0/kf1/kf2 全部恢复真值（kf2 0.992≈1.0，kf1 0.496≈0.5）。**关键洞察**：漂移必须一致作用所有 KF（含锚点 kf0），否则共视边编码错误几何会与回环纠正对抗；kf0 在 currentConnectedKFs 内被传播纠正回 T0，恰好是其 init 固定位姿。

---

## 17. 测试验证

33 个测试，纯 assert 无外部依赖，全部 PASS（MSVC 19.44，2026-07-02 验证）：


| 测试 | 结果 | 关键指标 |
|---|---|---|
| `test_math` | PASS | Mat3 逆、四元数旋转、线性求解 |
| `test_se3` | PASS | SO3/SE3 exp-log 往返、逆、左作用 |
| `test_pose_opt` | PASS | mono 位姿误差 2.31e-07，stereo 1.16e-07，内点 RMS 0.0000 px，粗差全剔除 |
| `test_orb` | PASS | 1009 特征，9 象限全覆盖，90° 旋转 Hamming=0 |
| `test_frame` | PASS | stereo-from-depth + 网格分箱 + UnprojectStereo |
| `test_stereo_init` | PASS | 1009 MapPoints，pos/nObs/descriptor/normal 全验 |
| `test_frustum` | PASS | isInFrustum 前/后/越界/角度>60°/距离越界/stereo uR + `update_normal_and_depth` 修正 + `mfLogScaleFactor=log(sf[1])` |
| `test_projmatch` | PASS | #1 匹配/ratio 剪枝/Hamming>100 拒；#2 恒等匹配/外点跳过/方向直方图 top-3 剪枝(20 保 1 剪) |
| `test_motion` | PASS | TWM 端到端，位姿恢复 t=(0.0104,0.0079,0.0005) 近真值 (0.00934,0.00556,0)，rot=0.0024rad，339 内点 |
| `test_localmap` | PASS | TWM + TrackLocalMap，mnMatchesInliers≥30，localKFs={KF0}，localMPs>0 |
| `test_bow` | PASS | 微词表 load_text/transform/L1归一化/二进制往返 + 真实 ORBvoc.bin(1,082,074 节点/971,815 词) 校验 |
| `test_searchbow` | PASS | SearchByBoW 干净匹配(2 跨节点)/ratio 剪枝(best1=best2=0 拒)/节点分组(跨节点不匹配) |
| `test_trackrefkf` | PASS | 端到端 TrackRefKF，真实 ORBvoc，198 内点，RMS 0.55px，位姿 t=(0.00898,0.00668,0.00036) 近真值，rot=0.00098rad |
| `test_score` | PASS | L1 score：相同=1/不相交=0/部分=0.8/单共享=0.5/对称/[0,1] |
| `test_kfdb` | PASS | 3 KF 倒排索引 + 候选筛选(0.8 maxWords/0.75 bestAcc) + 共视累积，手工 BoW 无需词表 |
| `test_pnp` | PASS | 60 点非共面，clean EPnP rot_err=0/dt=0；30% 外点 RANSAC 内点 46 位姿精确 |
| `test_reloc` | PASS | 端到端 Relocalization，真实 ORBvoc，203 内点，RMS 0.67px，位姿 t=(0.00998,0.00415,−0.00497) 近真值，rot=0.004rad |
| `test_map` | PASS | Map add/erase/dedup，mnInitKFid/mnMaxKFid |
| `test_mappointculling` | PASS | 7 MP 覆盖各剔除分支（bad/lowfound/young_lowobs/young_ok/mature/healthy/age2_obs4），recent 7→3 |
| `test_needkf` | PASS | NeedNewKeyFrame 5 用例：强跟踪近帧→0/弱+c1a→1/reloc冷却→0/c1a但强跟踪→0/弱+c1a过冷却→1 |
| `test_kfinsert` | PASS | 端到端 5 帧序列，2 KF 插入(map.nKFs 1→3)，culling recent 1444→772，kf0 共视图度数=2 |
| `test_ba` | PASS | Schur BA：无扰不自漂移(pt 5.1e-6)/mono 恢复(rot=0 t=1.4e-7 pt=8.7e-6 150/150 内点)/stereo 恢复(rot=0 pt=3.1e-6)/3 粗差标外点/`ngd_local_ba` 端到端 3 KF+24 MP kf0 固定 kf1/kf2 收敛至 GT |
| `test_createmps` | PASS | CreateNewMapPoints：2 KF+40 点(20 共享 MP 协视+20 未匹配)，真实 ORBvoc，三角化 20 新 MP 误差<1e-4 双 KF 观测+map+recent；基线<mb→0 新；描述子篡改→不三角化 |
| `test_fuse` | PASS | SearchInNeighbors/Fuse：2 KF+30 点(10 共享协视+20 重复 MP 同位同描述子)，融合 20 重复→mpA bad/mpB 存活双 KF 观测+槽转移+bad MP 扫出 map，无 BoW 依赖 |
| `test_keyframeculling` | PASS | KeyFrameCulling：5 KF(init+3 观测+1 冗余)+20 共享 MP+1 kfR 独占，kfR 剔除(mbBad/map.nKFs−1/独占 MP bad/共享 MP 存活 4 观测/协视表剪枝)，3 观测冗余率<0.9 不被剔 |
| `test_mask` | PASS | NGD mask 链：morphology(腐蚀/膨胀/别名)/pixel_potential(327/平坦-1/越界-1)/extract(单格1种子)/dbscan(2簇+噪声)/CC(2块+背景)/create(bbox+深度门清零/bbox<50跳过)/端到端 predict_current(合成 LK shift(20,10)→质心(65,55)精确) |
| `test_mlpnp` | PASS | MLPnP：60 点非共面 clean 恢复 rot=0/dt=0(机器精度)；RANSAC 30% 外点 46 内点(期望~42)、位姿 rot=0/dt=0；N<6 守卫返 0 |
| `test_sim3` | PASS | Sim3 李群：4 分支(σ/θ≈0)exp/log 往返 + map=s·(R·p)+t + inverse∘map=identity + s=1 退化 SE3 + compose 结合律 |
| `test_sim3solver` | PASS | Horn Sim3+RANSAC：60 点 s=1.2+R+Δt clean rot=0/dt=0/s≈1.2；30% 外点 ≥38 内点位姿精确；bFixScale 强制 s=1 |
| `test_sim3opt` | PASS | OptimizeSim3：扰动 rot0.05/dt0.1/s1.05 恢复 <1e-3(数值雅可比 h=1e-4)；粗差剔除且 GT 仍恢复；bFixScale 下 s 锁定 |
| `test_searchbysim3` | PASS | SearchBySim3：2 KF+共享 MP+精确 S12，40/40 匹配全同世界点(TH_HIGH=100/无方向直方图)；描述子篡改不匹配 |
| `test_essgraph` | PASS | OptimizeEssentialGraph：3 KF 链+共视边(真值测量)+漂移初始化，优化后 kf1/kf2 收敛真值 t_err=0/r_err=0 |
| `test_loopclosing` | PASS | LoopClosing::CorrectLoop：3 KF 共享 MP+一致刚体漂移 D+注入 loopScw，correct_loop(传播+MP纠正+fuse+essgraph+GBA)后 kf0/kf1/kf2 全部恢复真值(kf2 0.992≈1.0) |

构建干净：**0 error / 0 warning**。

---

## 18. 构建方法

本机 cmake 不在默认 PATH，需先运行 `c:\Users\frank.tu\CMakePath.bat`（提供 cmake 3.31.10）。纯 C，无外部依赖。

```bat
cd NGD-SLAM\refactor
c:\Users\frank.tu\CMakePath.bat
cmake -B myBuild -G "Visual Studio 17 2022" -A x64
cmake --build myBuild --config Release
ctest --test-dir myBuild -C Release --output-on-failure
```

- cmake 路径：`C:/src/vcpkg/downloads/tools/cmake-3.31.10-windows/cmake-3.31.10-windows-x86_64/bin/cmake.exe`
- 编译器：MSVC 19.44（VS2022 Community），编译器 ABI 检测通过
- 工具链注意：本机无独立 gcc/make（Git mingw64 不含 gcc）；VS2022 生成器自动发现 MSVC
- GCC/Linux 同样适用（CMake 自动链 `libm`）

实际命令（Bash 工具中）：
```bash
CMAKE="C:/src/vcpkg/downloads/tools/cmake-3.31.10-windows/cmake-3.31.10-windows-x86_64/bin/cmake.exe"
"$CMAKE" -B myBuild -G "Visual Studio 17 2022" -A x64
"$CMAKE" --build myBuild --config Release
```

---

## 19. 遗留问题与下一步路线图

### 19.1 P0 → P4 路线图（文档 §6.8）

| 优先级 | 模块 | 状态 |
|---|---|---|
| P0 | 数学库 + SE3/SO3 + PoseOptimization | ✅ 第一期 |
| P0 | ORB 提取 + Frame(RGBD) + StereoInitialization | ✅ 第二期 |
| P0/P1 | SearchByProjection(两个重载) + isInFrustum + TrackWithMotionModel + 简化 TrackLocalMap | ✅ 第三期 |
| P0 | DBoW2 词表 + SearchByBoW + TrackReferenceKeyFrame | ✅ 第四期 |
| P0 | 重定位（score + KeyFrameDB 倒排索引 + EPnP + Relocalization） | ✅ 第五期 |
| P1 | 简化 LocalMapping（KF 插入 + MapPointCulling） | ✅ 第六期（CreateNewMapPoints 三角化/SearchInNeighbors Fuse/KeyFrameCulling 仍留 P2） |
| P2 | LocalBundleAdjustment（稀疏 Schur，g2o 最难，可降级） | ✅ 第七期（Schur 补 LM BA） |
| P2 | CreateNewMapPoints 三角化 | ✅ 第八期（SearchInNeighbors Fuse / KeyFrameCulling ✅ 第九期，LocalMapping P2 余项收尾） |
| P2 | YOLO + 动态 mask 链（`PredictCurrentMask` 等）+ `SearchByOpticalFlow` | ⏳ 第十期(mask 生成核心纯 C + LK 回调)；`SearchByOpticalFlow`/`TrackWithOpticalFlow` 待第十一期；YOLO `cv::dnn` 薄壳待第十二期 |
| P3 | TrackWithOpticalFlow（PnP+RANSAC，`solvePnPRansac` 无 C API → 自实现 EPnP） | EPnP ✅ 第五期；MLPnP ✅ 第十一期（与 EPnP 并存，后续定取舍） |
| P3 | Sim3 + LoopClosing + OptimizeEssentialGraph | ✅ 第十二期 a–g（Sim3/Sim3Solver/OptimizeSim3/SearchBySim3/KF-KF SearchByBoW/OptimizeEssentialGraph/GlobalBA/LoopClosing 编排，单地图非IMU非merge） |
| P4 | Viewer | 嵌入式通常省略 |

### 19.2 DBoW2 路线决策（已落地路线 B / Stage 1 + Stage 2）

第五期完成 Stage 2 重定位：`score()`(L1) + KeyFrameDatabase 倒排索引 + `DetectRelocalizationCandidates` + EPnP(RANSAC) + `Relocalization`。PnP 用 EPnP（OpenCV epnp.cpp 忠实移植）替代 MLPnPsolver（doc §6.5 方向）。补建 KeyFrame 共视图并补回 UpdateLocalKeyFrames 协视图扩展。`test_reloc` 端到端：203 内点、RMS 0.67px、位姿近真值。

**仍待办**：mask 消费侧(`SearchByOpticalFlow`/`TrackWithOpticalFlow`)、YOLO `cv::dnn` 薄壳——按用户约束这两块直接用 OpenCV，不重构。第十二期完成 Sim3+LoopClosing+OptimizeEssentialGraph（P3 收尾，纯算法核心全部移植完毕）。

### 19.3 关键阈值速查（供下一期参考）

- TrackRefKF: BoW 匹配 `<15` 失败(`Tracking.cc:2779`)；ratio 0.7；单次 PoseOpt；最终 `nmatchesMap>=10`(`:2825`)（**第四期已移植**）
- TrackWithMotionModel: `th=15` mono/`7` stereo，`<20` 重试 `2·th`，仍 `<20` 失败，PoseOpt，`nmatchesMap>=10`（**第三期已移植**）
- TrackLocalMap: `mnMatchesInliers<30`(非IMU)/`<15`(IMU stereo-rgb)/`<50`(IMU mono 未初始化)/`<50`(近期重定位) 失败（**第三期已移植非IMU ≥30**）
- SearchByBoW: `TH_LOW=50`，ratio 0.7(TrackRefKF)/0.75(重定位)/0.9(LoopClosing)，30-bin 直方图 `factor=1/30`，top-3+0.1·max1 剪枝（**第四期已移植左分支**）
- DBoW2 transform: `levelsup=4` → `nid_level=2`（level-2 节点）；TF_IDF+L1 → BowVector L1 归一化（**第四期已移植**）
- SearchByProjection `:47`(#1): `RadiusByViewingCos` 2.5/4.0，`TH_HIGH=100`，ratio 0.8(SearchLocalPoints)，无方向直方图（**第三期已移植**）
- SearchByProjection `:1680`(#2): `th·mvScaleFactors[lastOctave]`，无 ratio，方向直方图 30-bin `factor=1/30`，top-3+0.1·max1 剪枝（**第三期已移植**）
- Fuse: `TH_LOW=50`，th=3(SearchInNeighbors)，距离带 0.8×min/1.2×max，视角 cos60°(PO·normal<0.5·dist)，尺度门 kpLevel∈[nPred−1,nPred]，stereo 7.8/mono 5.99 χ²，按 Observations() 多寡 Replace，**无方向直方图**（**第九期已移植左分支**）
- KeyFrameCulling: `redundant_th=0.9`(非IMU)，`thObs=3`(冗余需 >3 他 KF)，仅近 stereo 点(depth≤mThDepth)，`scaleLeveli≤scaleLevel+1`，`nRedundant>0.9·nMPs`→SetBadFlag（**第九期已移植非IMU**）
- Grid: 64×48，`mfGridElementWidthInv=64/imgW`，`GetFeaturesInArea` 方框 `|dx|<r && |dy|<r`
- isInFrustum: `viewCos<0.5`(SearchLocalPoints) 拒；距离带[0.8·minD, 1.2·maxD]；`PredictScale ratio=mfMaxDistance/dist`（**第三期已移植**）
- Sim3Solver: 内点门 `9.210·σ²`(χ²99%)，3 点 RANSAC，`prob=0.99,minInliers=20,maxIts=300`（**第十二期已移植**）
- OptimizeSim3: `th2=5.991`(mono)/`7.815`(stereo)，Huber δ=√th2，5 轮剔外点→摘核→5/10 轮，**数值雅可比**(g2o linearizeOplus 注释掉)（**第十二期已移植**）
- SearchBySim3: `TH_HIGH=100`，octave 门[nL-1,nL]，**无 ratio/无方向直方图**，双向一致性，th=3（**第十二期已移植**）
- OptimizeEssentialGraph: 共视边 weight≥`100`(minFeat)，EdgeSim3 残差 `log(meas·Siw·Sjw⁻¹)` info=I_7 无 Huber，LM 20 轮 λ_init=1e-16，恢复 `[R|t/s]`（**第十二期已移植**）

---

## 20. refapp 实时 Viewer：Pangolin MapDrawer → OpenCV 对照（2026-07-03）

refapp 驱动层新增纯 OpenCV 实时 3D viewer（`refapp/include/ngd_app/viewer.h` + `src/viewer.cpp`），替代原版 Pangolin 3D 地图窗口。本机 vcpkg OpenCV 4.12 **未带 viz/VTK**，故用 `cv::projectPoints` 手动 3D→2D 投影 + `cv::Mat` 渲染。画法逐项对照 `src/MapDrawer.cc`：

| MapDrawer（`src/MapDrawer.cc`） | OpenCV（`refapp/src/viewer.cpp`） |
|---|---|
| `DrawMapPoints`（:135） | 点云 `project_with`+`cv::circle`，按高度着色 |
| `DrawKeyFrames` bDrawKF（:194-263） | 每个关键帧画蓝色线框 frustum（init KF 红色），`draw_frustum` |
| `DrawKeyFrames` bDrawGraph（:265-311） | 共视图绿色连线，`weight≥100`、`mnId<j` 去重 |
| `DrawCurrentCamera`（:398-438） | 当前相机绿色 frustum，`draw_frustum` |

frustum 几何逐字同 MapDrawer：`w=size, h=w·0.75, z=w·0.6`，8 线（apex→4 角 + 图像矩形），相机系构建经 `Twc=inverse(pose)` 变换（对应原码 `GetPoseInverse()`，:199）。尺寸按场景对角线缩放（`auto_fit`：`cam_size=diag·0.085`）使屏幕大小与场景尺度无关。两窗口：3D Map + Frame（masked-ORB 关键点 + mask 叠加）。运行开关 `NGD_VIEWER`/`NGD_PHASE`。**完整对照表 + 行号 + 运行命令见 `refapp/BENCHMARK.md` 附录 A**（本节为镜像摘要）。

---

## 附：工程统计

- 源文件：26 个 `.c` + 26 个 `.h`（含 `calib`/`bow`/`kfdb`/`pnp`/`map`/`localmapping`/`ba`/`mask`/`mlpnp`/`sim3`/`sim3solver`/`sim3opt`/`essgraph`/`loopclosing`）；第十二期新增 `sim3/sim3solver/sim3opt/essgraph/loopclosing` 5 对 + `ba.c::ngd_global_ba` + `matcher.c::ngd_search_by_sim3/ngd_search_by_bow_kf`
- 测试：33 个，纯 assert 无框架（第十二期新增 `test_sim3/test_sim3solver/test_sim3opt/test_searchbysim3/test_essgraph/test_loopclosing` 6 个）
- 最大单文件：`orb.c`（约 620 行，含 256 对 BRIEF 模式表）；`ba.c` 约 760 行(含 global_ba)；`matcher.c` 约 800 行(含 search_by_sim3/kf + core 抽取)；`pnp.c` 约 530 行；`mlpnp.c` 约 850 行；`sim3opt.c` 约 220 行；`essgraph.c` 约 280 行；`loopclosing.c` 约 280 行
- 外部依赖：无（仅 `<math.h>`/`<stdlib.h>`/`<string.h>`/`<stdio.h>`）
- 构建产物：`ngd_core.lib`（静态库）+ 33 个测试 exe
- 已验证平台：Windows 10 / MSVC 19.44 / VS2022 / cmake 3.31.10

---

**文档完。** 后续迭代请同步更新本文与 `README.md`。
