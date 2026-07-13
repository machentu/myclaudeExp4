# NGD-SLAM 纯C核心 (refactor/)

基于 [`NGD_SLAM_TECHNICAL_DOC.md`](../NGD_SLAM_TECHNICAL_DOC.md) 第 6 章「移植到纯 C / 嵌入式环境」的重构建议，在 `NGD-SLAM/refactor/` 下新建的**纯 C 核心**。

> 本文件为**模块级说明**；决策依据、忠实性核对、调试踩坑与进度验证等过程记录见 [`WORKLOG.md`](WORKLOG.md)。

本目录采用 **纯C核心 + OpenCV 薄壳** 路线：核心算法（数学库 / 李群 / 优化器）用纯 C 重写、可独立编译与单元测试；后续增量再以薄封装调用 OpenCV（图像 IO / 光流 / PnP / YOLO 推理）。

## 当前交付

### 首期：P0 数学库 + PoseOptimization

| 模块 | 文件 | 对应原码 | 说明 |
|---|---|---|---|
| 固定尺寸线代 | `include/ngd/math.h` `src/math.c` | 文档 §6.9 | Vec3 / Mat3(闭式逆) / Quat(Hamilton w-first) / 6×6 线性求解(高斯消元) |
| SO(3) | `include/ngd/so3.h` `src/so3.c` | Sophus::SO3f | exp(Rodrigues) / hat / log |
| SE(3) | `include/ngd/se3.h` `src/se3.c` | g2o::SE3Quat | 完整 exp(左雅可比 V) / log / map / inverse / compose / SE3deriv |
| 针孔相机 | `include/ngd/pinhole.h` `src/pinhole.c` | `Pinhole::project/projectJac` | mono + stereo(uR=fx·X/Z+cx−bf/Z) 投影及雅可比 |
| 位姿优化 | `include/ngd/pose_opt.h` `src/pose_opt.c` | `Optimizer::PoseOptimization` `src/Optimizer.cc:814-1114` | 单 SE3 顶点 + LM(H+λI) + Huber + 4 轮 chi2 内外点分类 |

### 第二期：P0 ORB 提取 + Frame + StereoInitialization

| 模块 | 文件 | 对应原码 | 说明 |
|---|---|---|---|
| ORB 提取 | `include/ngd/orb.h` `src/orb.c` | `ORBextractor` `src/ORBextractor.cc` | 纯C：双线性金字塔/FAST-9_16+NMS/IC_Angle/rBRIEF/四叉树。`bit_pattern_31_`、`umax` 逐字复制，描述子词袋兼容 |
| Hamming 距离 | `include/ngd/matcher.h` `src/matcher.c` | `ORBmatcher::DescriptorDistance` `:2131` | 8×uint32 并行 popcount |
| 相机标定上下文 | `include/ngd/calib.h` `src/calib.c` | Frame 静态内参 + 尺度金字塔 | fx/fy/cx/cy/mbf/mb/thDepth + 尺度金字塔 + `mfLogScaleFactor` |
| MapPoint(最小) | `include/ngd/mappoint.h` `src/mappoint.c` | `MapPoint` `include/MapPoint.h` | worldPos/descriptor/normal/nObs + 观测记录 + ComputeDistinctiveDescriptors/UpdateNormalAndDepth/PredictScale + track 投影缓存 |
| KeyFrame(最小) | `include/ngd/keyframe.h` `src/keyframe.c` | `KeyFrame` `include/KeyFrame.h` | id/位姿/相机中心/逐关键点数组 + 64×48 堆网格(PosInGrid/GetFeaturesInArea) |
| Frame(RGBD) | `include/ngd/frame.h` `src/frame.c` | RGBD Frame 构造 `Frame.cc:222-309` + `Frame::isInFrustum` `:651` | ORB→去畸变→ComputeStereoFromRGBD→网格 + isInFrustum 视锥判定 |
| 立体初始化 | `include/ngd/tracking.h` `src/tracking.c` | `StereoInitialization` `Tracking.cc:2458-2492` | 首帧 N>500→单位位姿→建 KF→逐关键点 UnprojectStereo 建 MapPoint |

### 第三期：P0/P1 投影匹配 + 运动模型跟踪 + 简化局部地图

| 模块 | 文件 | 对应原码 | 说明 |
|---|---|---|---|
| 投影匹配 #1 | `matcher.c` `ngd_search_by_projection_local` | `ORBmatcher::SearchByProjection` `:47` | SearchLocalPoints 用：依赖 isInFrustum 缓存投影、RadiusByViewingCos·th、ratio(0.8) 同 octave 才用、无方向直方图 |
| 投影匹配 #2 | `matcher.c` `ngd_search_by_projection_motion` | `ORBmatcher::SearchByProjection` `:1680` | TrackWithMotionModel 用：从零投影、th·mvScaleFactors[lastOctave]、无 ratio、bForward/bBackward、方向直方图(ComputeThreeMaxima 剪枝) |
| 视锥/尺度辅助 | `frame.c`/`mappoint.c`/`matcher.c` | `Frame::isInFrustum` `:651`、`MapPoint::PredictScale` `:539`、`RadiusByViewingCos` `:219`、`ComputeThreeMaxima` `:2085` | isInFrustum 单目/RGBD 路径填 track 缓存；PredictScale `ratio=mfMaxDistance/dist`；2.5/4.0 半径；top-3 + 0.1·max1 剪枝 |
| 运动模型跟踪 | `tracking.c` `ngd_tracking_track_with_motion_model` | `Tracking::TrackWithMotionModel` `:2901` | V·last.pose→#2(th=7stereo/15，<20 重试 2th)→PoseOpt→外点剔除→nmatchesMap≥10 |
| 局部地图跟踪 | `tracking.c` `ngd_tracking_track_local_map` | `Tracking::TrackLocalMap` `:3104` + `SearchLocalPoints` `:3491` + `UpdateLocalMap` `:3565` | UpdateLocalMap(协视图邻居扩展简化为观测 KF 集合)+SearchLocalPoints(isInFrustum 0.5+#1 th=3 RGBD)+PoseOpt→mnMatchesInliers≥30(非IMU) |
| 跟踪上下文 | `tracking.h` `ngd_tracking` | `Tracking` 类相关字段 | current/last/refKF/velocity/sensor/mState + localKFs/localMPs 堆列表 + PoseObs 抓取暂存；LocalMapping/NeedNewKeyFrame 桩化 |

**测试**（10 个，纯 assert 无外部依赖，全部 PASS）：
`test_math`、`test_se3`、`test_pose_opt`、`test_orb`、`test_frame`、`test_stereo_init`、`test_frustum`(isInFrustum 各门限+update_normal_and_depth 修正)、`test_projmatch`(#1/#2+方向直方图剪枝)、`test_motion`(TWM 端到端位姿恢复至真值)、`test_localmap`(TWM+TrackLocalMap，mnMatchesInliers≥30)。

### 第四期：P0 DBoW2 词表 + SearchByBoW + TrackReferenceKeyFrame

| 模块 | 文件 | 对应原码 | 说明 |
|---|---|---|---|
| DBoW2 词表 | `include/ngd/bow.h` `src/bow.c` | `TemplatedVocabulary.h` `Thirdparty/DBoW2/` | 纯C：load_text(ORBvoc.txt)/load_binary(ORBvoc.bin,DBoW2二进制41B/节点)/save_binary；扁平 SoA 节点 + CSR 子节点；transform(levelsup=4)→BowVector(L1归一化)+FeatureVector |
| BowVector/FeatureVector | `bow.c` | `BowVector`/`FeatureVector` | 有序数组（collect→sort→merge）；Hamming 复用 `ngd_descriptor_distance` |
| Frame/KF BoW | `frame.c`/`keyframe.c` | `Frame::ComputeBoW`/`KeyFrame::ComputeBoW` | `ngd_frame_compute_bow`/`ngd_keyframe_compute_bow`(lazy,levelsup=4)+bow/feat 指针字段+free 释放 |
| SearchByBoW | `matcher.c` `ngd_search_by_bow` | `ORBmatcher::SearchByBoW` `:227-429` | 两 FeatureVector 有序归并+同节点暴力 Hamming+TH_LOW=50+ratio+30-bin方向直方图(左相机分支) |
| 参考帧跟踪 | `tracking.c` `ngd_tracking_track_reference_keyframe` | `Tracking::TrackReferenceKeyFrame` `:2767-2826` | ComputeBoW→SearchByBoW(ratio=0.7)→<15失败→SetPose(last)→PoseOpt→外点剔除→nmatchesMap≥10 |

**测试**（新增 3 个，共 13 个全部 PASS）：
`test_bow`(微词表 load_text/transform/L1归一化/二进制往返 + 真实 ORBvoc.bin 校验:1,082,074节点/971,815词)、`test_searchbow`(干净匹配/ratio剪枝/节点分组)、`test_trackrefkf`(端到端:真实 ORBvoc,198 内点,RMS 0.55px,位姿恢复近真值 t≈(0.009,0.006,0))。

### 第五期：P0 重定位（Stage 2 — score + KeyFrameDB + EPnP + Relocalization）

| 模块 | 文件 | 对应原码 | 说明 |
|---|---|---|---|
| L1 评分 | `bow.c` `ngd_bow_score_l1` | `L1Scoring::score` `ScoringObject.cpp:23-68` | 双指针归并两 sorted BowVector，同 word 累加 `|vi−wi|−|vi|−|wi|`，`−sum/2`；BowVector 已 L1 归一化，直接可评 |
| 共视图 | `keyframe.c/h` `ngd_keyframe_add_connection`/`update_connections`/`get_best_covisibility_keyframes` | `KeyFrame::AddConnection`/`UpdateConnections`/`GetBestCovisibilityKeyFrames` `KeyFrame.cc:189-251,379-475` | 并行数组 `connKFs`/`connWeights`（权重降序）；`UpdateConnections` 从 MP 观测重建（th=15，无则取最大），对称 `AddConnection`；spanning-tree parent/child 不做（reloc 不需要） |
| KF reloc 字段 | `keyframe.h` | `KeyFrame.h:338-340` | `mnRelocQuery`/`mnRelocWords`/`mRelocScore`（DetectRelocalizationCandidates 读写） |
| 倒排索引 | `kfdb.c/h` `ngd_kfdb` | `KeyFrameDatabase` `KeyFrameDatabase.cc:39-98` | per-word KF 动态数组（`mvInvertedFile`）；`add`/`erase` |
| 候选检测 | `kfdb.c` `ngd_kfdb_detect_reloc_candidates` | `DetectRelocalizationCandidates` `KeyFrameDatabase.cc:733-845` | 共享词去重(`mnRelocQuery`)→`maxCommonWords*0.8`过滤→L1 score→共视累积(`GetBestCovisibilityKeyFrames(10)`)→`0.75*bestAccScore`保留；多地图`GetMap()`过滤跳过（单地图） |
| EPnP | `pnp.c/h` `ngd_epnp_solve` | OpenCV `epnp.cpp`（NGD 原用 MLPnPsolver，此处按 doc §6.5 改用 EPnP） | 4 控制点(质心+PCA)/barycentric α/M 矩阵(2N×12)/MtM 4 最小特征向量/L6x10+ρ/β 三近似法+Gauss-Newton/Kabsch；内部 Jacobi 对称特征分解(3×3 与 12×12)；double 精度 |
| RANSAC | `pnp.c` `ngd_pnp_solve_ransac` | `MLPnPsolver::iterate`/`SetRansacParameters` | `prob=0.99,minInliers=10,maxIts=300,minSet=6,ε=0.5,χ²=5.991`；每轮抽 6 点 EPnP→全体重投影 χ2 分类→最优 inlier 集→全 inlier 精修；固定 srand 确定性 |
| 重定位 | `tracking.c` `ngd_tracking_relocalization` | `Tracking::Relocalization` `Tracking.cc:3757-3925` | ComputeBoW→DetectCandidates→每候选 SearchByBoW(0.75,<15弃)→EPnP RANSAC→SetPose→填 inlier→PoseOpt→`nGood<10` continue/`nGood≥50` 成功；`<50` 补匹配链简化为单次 `search_by_projection_local`(th=3,ratio 0.9)→再 PoseOpt |

**测试**（新增 4 个，共 17 个全部 PASS）：
`test_score`(相同=1/不相交=0/部分=0.8/对称/[0,1])、`test_kfdb`(3 KF 倒排索引+候选筛选 0.8 maxWords/0.75 bestAcc+共视累积，手工 BoW 无需词表)、`test_pnp`(合成 60 点非共面,clean EPnP rot_err=0/dt=0,30% 外点 RANSAC 内点~42 位姿精确)、`test_reloc`(端到端:真实 ORBvoc,203 内点,RMS 0.67px,位姿 t=(0.010,0.004,−0.005) 近真值,rot=0.004rad)。

### 第六期：P1 LocalMapping 简化（KF 插入 + MapPointCulling）

| 模块 | 文件 | 对应原码 | 说明 |
|---|---|---|---|
| Map 容器 | `map.c/h` `ngd_map` | `Map` `Map.cc:60-134` | KF/MP 动态数组(set 语义去重)+`mnInitKFid`/`mnMaxKFid`/`mnLastKeyFrameId`；add/erase；多地图/IMU 不做 |
| MP 生命周期字段 | `mappoint.c/h` | `MapPoint::mnFirstKFid`/`GetFoundRatio`/`SetBadFlag` | 加 `mnFirstKFid`(`new` 设自 refKF)+`get_found_ratio`(`mnFound/mnVisible`)+`set_bad`(简化版 SetBadFlag，标记+释放 obs，不清理共视图反向引用) |
| KF 跟踪点计数 | `keyframe.c/h` `ngd_keyframe_tracked_map_points` | `KeyFrame::TrackedMapPoints` | NeedNewKeyFrame 的 `nRefMatches` |
| LocalMapping 上下文 | `localmapping.c/h` `ngd_local_mapping` | `LocalMapping` `LocalMapping.cc:64-290,354-393` | `recent_mps`(mlpRecentAddedMapPoints)+`currentKF`+`push_recent`+`map_point_culling`(cnThObs=3 RGBD/2 mono；foundRatio<0.25/age≥2&obs≤cnThObs 剔除/age≥3 出列)+`run`(culling+P2 桩) |
| 关键帧判定 | `tracking.c` `ngd_tracking_need_new_keyframe` | `Tracking::NeedNewKeyFrame` `Tracking.cc:3220-3360` | 非 IMU 核心：c1a/c1b/c1c/c2 + NGD c5(c5 惰性，`mbStartOpticalFlow` 默认 0)；LocalMapping idle 恒真(单线程)；reloc 冷却；thRefRatio 0.75 RGBD/0.9 mono/0.4(nKFs<2) |
| 关键帧创建 | `tracking.c` `ngd_tracking_create_new_key_frame` | `Tracking::CreateNewKeyFrame` `Tracking.cc:3363-3489` | 深拷贝 frame→KF；RGBD 深度点建新 MP(depth 升序,maxPoint=100,mThDepth 截断)；ComputeBoW+KFDB+`update_connections`+map；push 新 MP 到 recent；设 refKF/mnLastKeyFrameId/lm->currentKF |

**测试**（新增 4 个，共 21 个全部 PASS）：
`test_map`(add/erase/dedup/mnInitKFid/mnMaxKFid)、`test_mappointculling`(7 MP 覆盖各剔除分支：bad/lowfound/young_lowobs/young_ok/mature/healthy/age2_obs4)、`test_needkf`(强跟踪近帧→0/弱跟踪+c1a→1/reloc冷却→0/c1a但强跟踪→0)、`test_kfinsert`(端到端:5 帧序列 mMaxFrames=2,2 KF 插入,map.nKFs 1→3,culling 1444→772,kf0 共视图度数=2)。

### 第七期：P2 LocalBundleAdjustment（Schur 补 BA）

| 模块 | 文件 | 对应原码 | 说明 |
|---|---|---|---|
| 局部 BA | `include/ngd/ba.h` `src/ba.c` | `Optimizer::LocalBundleAdjustment` `src/Optimizer.cc:1116-1500` + g2o `BlockSolver_6_3`/`OptimizationAlgorithmLevenberg` | 纯 C：双精度内部 Schur 补 BA。两层 API：`ngd_ba_optimize`(扁平数组核心：边残差/雅可比 `J_point=−Pjac·R`、`J_pose=−Pjac·SE3deriv`/Huber IRLS ρ₁/Schur `Hschur=Hpp−Σ Hpl·Dinv·Hplᵀ`/稠密 LU 解位姿系统/回代 `dxl=Dinv(bl−Hplᵀ·dxp)`/Nielsen LM λ/OPLUS 左扰动 `exp(ξ)·T`+点 `p+=dx`) + `ngd_local_ba`(收集局部 KF+共视邻居/局部 MP/固定 KF，0 固定则中止，10 轮 LM，外点剔除 χ²>5.991/7.815 或深度≤0 解绑观测，写回非固定位姿+MP) |
| BA 戳字段 | `keyframe.h`/`mappoint.h` | `KeyFrame.h:338-340`/`MapPoint.h` | `mnBALocalForKF`/`mnBAFixedForKF`（局部/固定集合去重） |
| LocalMapping 接线 | `localmapping.c` `ngd_local_mapping_run` | `LocalMapping::Run` `LocalMapping.cc:64-290` | P2 LBA 桩替换为 `ngd_local_ba` 调用（守卫 `nKFs≥4`）；CreateNewMapPoints/SearchInNeighbors/KeyFrameCulling 仍留后续 |

**测试**（新增 1 个，共 22 个全部 PASS）：
`test_ba`(无扰不自漂移/mono BA 扰动恢复 rot=0 t=1.4e-7 pt=8.7e-6 全内点/stereo BA 恢复/3 粗差注入被标外点且内点仍收敛/`ngd_local_ba` 端到端 3 KF+24 MP 小地图 kf0 固定不变 kf1/kf2 收敛至 GT)。

### 第七期补：P2 CreateNewMapPoints 三角化

| 模块 | 文件 | 对应原码 | 说明 |
|---|---|---|---|
| 三角化 DLT | `matcher.c/h` `ngd_triangulate` | `GeometricTools::Triangulate` `GeometricTools.cc:47-66` | 4×4 A（行 `x_ci[k]·Tc_i.row2 − Tc_i.row(k)`），x3D = AᵀA 最小特征值特征向量（4×4 循环 Jacobi，镜像 pnp.c::jacobi_sym 两通道旋转）去齐次 |
| 三角化匹配 | `matcher.c/h` `ngd_search_for_triangulation` | `ORBmatcher::SearchForTriangulation` `:911-1150` | FeatureVector 有序归并，跳过有 MP 的特征/已匹配，极点距离拒（100·sf²），F12 基础矩阵极线约束（`Pinhole::epipolarConstrain` `Pinhole.cpp:107-129`，`F12=K1⁻ᵀ·[t12]×·R12·K2⁻¹`，点到线距²<3.84·σ²），TH_LOW=50 best-only，30-bin 方向直方图+ComputeThreeMaxima |
| KF 反投影 | `keyframe.c/h` `ngd_keyframe_unproject_stereo` | `Frame::UnprojectStereo` `Frame.cc:1043+` | 深度→世界点（低视差 stereo 分支用） |
| 新建地图点 | `localmapping.c/h` `ngd_local_mapping_create_new_map_points` | `LocalMapping::CreateNewMapPoints` `LocalMapping.cc:396-720` | 10 共视邻居（RGBD），基线<mb 跳过，SearchForTriangulation→逐匹配：视差（cosParallaxRays vs stereo cos(2·atan2(mb/2,depth))）选 Triangulate/UnprojectStereo 分支→前后向(z>0)+双 KF 重投影(5.991/7.8·σ²)+尺度一致性(ratioDist·1.5·sf vs ratioOctave)→建 MP+双观测(stereo nObs+2)+map+recent。接入 `run()` culling→create_mps→LBA 顺序 |

**测试**（新增 1 个，共 23 个全部 PASS）：
`test_createmps`(端到端:2 KF+40 点[20 共享 MP 协视+20 未匹配]，真实 ORBvoc，三角化 20 新 MP 位置误差<1e-4，双 KF 观测+map+recent；基线<mb→0 新；描述子篡改→该点不三角化)。

### 第九期：P2 SearchInNeighbors/Fuse + KeyFrameCulling

| 模块 | 文件 | 对应原码 | 说明 |
|---|---|---|---|
| Fuse 原语去重 | `matcher.c/h` `ngd_fuse` | `ORBmatcher::Fuse` `:1152-1342` | 修重复定义(:646 删)+header 声明；body 不动（已忠实：投影+距离带+视角 cos60°+尺度门+stereo 7.8/mono 5.99 χ²+TH_LOW=50+按 Observations() 多寡 Replace，无方向直方图） |
| EraseObservation | `mappoint.c/h` `ngd_mappoint_erase_observation` | `MapPoint::EraseObservation` `:168-201` | 减 nObs(stereo rightIdx≥0→2)+swap-pop obs+清 KF 槽+refKF 重选+`nObs<=2`→set_bad |
| KF bad 标志 | `keyframe.c/h` `mbBad`/`is_bad`/`erase_connection`/`set_bad` | `KeyFrame::mbBad`/`isBad`/`EraseConnection`/`SetBadFlag` `:573-701` | 加 `mbBad` 字段；`erase_connection` shift 删保降序；`set_bad` 非惯性简化：守卫 init KF→对称 EraseConnection→逐 MP EraseObservation→清自身共视→mbBad+map erase+kfdb erase；spanning tree 不做 |
| SearchInNeighbors | `localmapping.c/h` `ngd_local_mapping_search_in_neighbors` | `LocalMapping::SearchInNeighbors` `:722-831` | nn=10/30+1st/2nd 阶协视(mnFuseTargetForKF stamp)→Fuse current MPs 入 targets+targets MPs 入 current(mnFuseCandidateForKF stamp)→重算 descriptor/normal+UpdateConnections+扫除 bad MP 出 map(MP 无 map 指针，驱动承担 Replace 的 map erase) |
| KeyFrameCulling | `localmapping.c/h` `ngd_local_mapping_keyframe_culling` | `LocalMapping::KeyFrameCulling` `:910-1062` | 非惯性 RGBD：redundant_th=0.9、thObs=3、仅近 stereo 点(depth≤thDepth)、`nRedundant>0.9·nMPs`→set_bad；scaleLeveli≤scaleLevel+1；count>100 安全 |
| LocalMapping 接线 | `localmapping.c` `run` | `LocalMapping::Run` `:64-290` | culling→create_mps→**search_in_neighbors→keyframe_culling**→LBA（原 Run 顺序） |

**测试**（新增 2 个，共 25 个全部 PASS）：
`test_fuse`(端到端:2 KF+30 点[10 共享协视+20 重复 MP 同位同描述子]，search_in_neighbors 融合 20 重复→mpA bad/mpB 存活双 KF 观测+槽转移+bad MP 扫出 map，无 BoW 依赖)、`test_keyframeculling`(5 KF[init+3 观测+1 冗余]，20 共享 MP 5 KF 观测+1 kfR 独占，culling 剔 kfR(mbBad/map.nKFs−1/独占 MP bad/共享 MP 存活 4 观测/协视表剪枝)，3 观测因 3 unique MP 使冗余率<0.9 不被剔)。

### 第十期：NGD 动态 mask 链（mask 生成核心，纯 C）

路线图 P2 的 NGD 专有部分。本期交付动态 mask **生成**核心（`Tracking::PredictCurrentMask` 及四 helper，`src/Tracking.cc:4249-4587`）；mask 消费侧（Frame 过滤 / `SearchByOpticalFlow` / `TrackWithOpticalFlow`）留第十一期，YOLO `cv::dnn` 薄壳留第十二期。

**关键架构**：整条链唯一硬 OpenCV 依赖是 `cv::calcOpticalFlowPyrLK`，其余（腐蚀/膨胀/连通域/DBSCAN/角点势/bbox+深度门）全部纯 C → 核心 0 依赖、可单测；光流作**可注入回调** `ngd_lk_flow_fn`（薄壳绑 LK 默认参 winSize21/maxLevel3/COUNT+EPS 30·0.01，测试注入合成流）。

| 模块 | 文件 | 对应原码 | 说明 |
|---|---|---|---|
| 形态学 | `mask.c/h` `ngd_mask_erode/dilate` | `cv::erode/dilate`(MORPH_RECT) | 二值(2·half+1)；越界邻点忽略(erode=AND/dilate=OR)；支持 in==out 别名 |
| 角点势 | `mask.c/h` `ngd_mask_pixel_potential` | `ComputePixelPotential` `:4308-4352` | 4 十字(±3)；>=3 邻点亮/暗→200+minDiff。**修潜伏 bug**：`isBrighter/isDarker` 未初始化→置 0；死边界检查→越界返 -1 |
| 取种子 | `mask.c/h` `ngd_mask_extract_dyna_points` | `ExtractDynaPoints` `:4355-4399` | cellSize=15 网格、threshold=250、mask==1 区最大势像素、`>250` 早退 |
| DBSCAN | `mask.c/h` `ngd_mask_cluster_dbscan` | `ClusterWithDBSCAN` `:4402-4483` | 深度预分段(gap>0.1/窗>0.5/子段>20→z=200·level；空回退原点)+3D DBSCAN(eps/minPts)；labels 映射回原点 |
| 连通域 | `mask.c/h` `ngd_mask_connected_components` | `cv::connectedComponentsWithStats`(4 连通) | BFS 队列 |
| 生成 mask | `mask.c/h` `ngd_mask_create_from_clusters` | `CreateMaskFromClusters` `:4486-4587` | bbox+扩15/`w或h<50`跳过/深度门±0.3/最大连通域/末尾 dilate 7×7；中位深度 |
| 编排 | `mask.c/h` `ngd_mask_predict_current` | `PredictCurrentMask` `:4249-4305` | 腐蚀副本→取种子→LK(cb)→深度过滤→DBSCAN(50,15)→create；不改 last_mask |

**测试**（新增 1 个，共 26 个全部 PASS）：
`test_mask`(7 用例：morphology/pixel_potential/extract/dbscan/CC/create/端到端 predict_current——合成 LK 回调注入 shift(20,10)，3px 棋盘灰度保证每内点势 455>250，预测 mask 质心 (65.0,55.0) 精确匹配种子质心(45,45)+shift；空 mask→空输出)。

### 第十一期：MLPnP 求解器（纯 C 核心）

补回原 NGD-SLAM 重定位实际使用的 **MLPnP**（Maximum-Likelihood PnP，`src/MLPnPsolver.cpp`），作为 EPnP 之外第二个 PnP 求解器。第五期曾因 "MLPnP 依赖 Eigen、纯 C 复现代价高" 改用 EPnP；本期补齐，与 EPnP 并存，后续再定取舍。API 形状对齐 EPnP（`ngd_mlpnp_solve` + `ngd_mlpnp_solve_ransac`），输出 `ngd_se3`，核心 0 依赖。

**忠实移植** `MLPnPsolver.cpp:356-658`（`computePose` 七阶段）：bearing 零空间（3×2，eigendecomp `f f^T`）→ 平面检测（rank of `points3·points3^T`，9/12 列分支）→ 设计矩阵 A → 最小特征向量解（`A^T A` 对称 eigendecomp）→ R/t 恢复 + 符号判定（Frobenius `U V^T`、±t 重投影 `1-v·f`）→ Gauss-Newton 精修（Rodrigues，5 轮）+ 200 行手写符号雅可比 `mlpnpJacs` 逐字转录。RANSAC 包装对齐 EPnP。

| 模块 | 文件 | 对应原码 | 说明 |
|---|---|---|---|
| MLPnP 核心 | `mlpnp.c/h` `ngd_mlpnp_solve` | `computePose` `:356-658` | 无 RANSAC，N≥6；pinhole bearing `((u-cx)/fx,(v-cy)/fy,1)` |
| MLPnP+RANSAC | `mlpnp.c/h` `ngd_mlpnp_solve_ransac` | `iterate` `:100-223` | 采样 6→computePose→CheckInliers(`err²<sigma2·th2`)→best→Refine；`srand(12345)` |

**关键忠实性决策**：
- **丢弃 ML 协方差路径**（死代码）：原码 `iterate`/`Refine` 恒传 `covs(size=1)`，守卫 `covMats.size()==N`（N>5）永假 → `use_cov=false`、P=I、`AtPA=A^T A`、`Jac^T·Kll` 退化为 `Jac^T`。整条稀疏 linalg 从未启用 → C 版不移植（忠实于实际行为），核心保持 0 依赖。`sigma2` 仅用于 RANSAC 内点门限。
- **输出 Tcw**（world→camera），与 EPnP 一致；原码 `Eigen::Matrix4f`，Tracking 亦按 Tcw 用。已实证：clean/RANSAC 恢复位姿与真值 Tcw 完全吻合（rot=0、dt=0），Twc 不吻合。
- **线性代数自包含**（refactor 惯例，同 `pnp.c`/`ba.c`）：自带 `jacobi_sym`/`svd3`/`solve_linear_d`，不改 `pnp.c`。
- **4×4 逆用闭式** `[[R^T,-R^T·t],[0,1]]`（符号判定，免通用求逆）；`solve_linear_d` 替 LDLT（6×6 GN 步）。

**测试**（新增 1 个，共 27 个全部 PASS）：
`test_mlpnp`(60 点非共面，TUM3 内参)：clean `ngd_mlpnp_solve` 恢复 rot=0.0000°/dt=0；RANSAC 30% 外点 46 内点(期望~42)、位姿 rot=0/dt=0；N<6 守卫返 0。精度对标 `test_pnp`(EPnP)。

### 第十二期 a–g：P3 Sim3 + LoopClosing + OptimizeEssentialGraph

路线图 P3 的 NGD-SLAM 剩余纯算法模块（无 OpenCV 依赖，符合 YOLO-dnn/光流/主函数不重构的约束）。单地图、非 IMU、非 merge 简化口径（与前 11 期一致）。

| 模块 | 文件 | 对应原码 | 说明 |
|---|---|---|---|
| Sim3 李群 | `include/ngd/sim3.h` `src/sim3.c` | g2o::Sim3 `Thirdparty/g2o/g2o/types/sim3.h` | `{q,t,s}`；exp/log 逐字照搬 g2o 含 σ≈0/θ≈0 各分支；`map=s·(R·p)+t`；`inverse`/`multiply`/`to_se3=[R|t/s]` |
| Sim3Solver | `include/ngd/sim3solver.h` `src/sim3solver.c` | `Sim3Solver` `src/Sim3Solver.cc` | Horn 1987 闭式（质心→M→4×4 N→最大特征向量→SO3::exp→尺度 nom/den→平移）+ 3 点 RANSAC；内点门 `9.210·σ²`；自含 `jacobi_sym` 4×4 |
| OptimizeSim3 | `include/ngd/sim3opt.h` `src/sim3opt.c` | `Optimizer::OptimizeSim3` `src/Optimizer.cc:2115-2381` | 单 Sim3 顶点 + 双向重投影边(`EdgeSim3ProjectXYZ`/`EdgeInverseSim3ProjectXYZ`)，点固定。**数值雅可比**（g2o `linearizeOplus` 注释掉→运行时数值微分，与 SE3 路径不同）+ LM+Huber(δ=√th2) + 5 轮剔外点→摘核再 5/10 轮 |
| SearchBySim3 | `src/matcher.c` `ngd_search_by_sim3` | `ORBmatcher::SearchBySim3` `:1461-1678` | S12/S21 双向投影 + 半径匹配(TH_HIGH=100，octave 门[nL-1,nL]，无 ratio/方向直方图) + 双向一致性 |
| KF-KF SearchByBoW | `src/matcher.c` `ngd_search_by_bow_kf` | `ORBmatcher::SearchByBoW` `:769-907` overload | 抽取 `search_by_bow_core` 共享核（DRY），KF-vs-Frame / KF-vs-KF 两封装；LoopClosing 检测用 |
| OptimizeEssentialGraph | `include/ngd/essgraph.h` `src/essgraph.c` | `Optimizer::OptimizeEssentialGraph` `src/Optimizer.cc:1501-1783` | 全 KF Sim3 顶点(init 固定) + 共视图边(weight≥100) + 回环边；EdgeSim3 残差 `log(meas·Siw·Sjw⁻¹)`，**数值雅可比**，稠密 LM 20 轮(λ_init=1e-16)；写回 `[R|t/s]` + MP 二次变换纠正 |
| GlobalBA | `src/ba.c` `ngd_global_ba` | `Optimizer::GlobalBundleAdjustment` | 复用 `ngd_ba_optimize` 全 KF(init 固定)+全 MP，薄包装 |
| LoopClosing 编排 | `include/ngd/loopclosing.h` `src/loopclosing.c` | `LoopClosing::Run/NewDetectCommonRegions/CorrectLoop` `src/LoopClosing.cc` | `detect_common_regions`(BoW L1 候选→KF-KF SearchByBoW≥20→Sim3Solver RANSAC≥15→SearchBySim3≥50→OptimizeSim3≥20) + `correct_loop`(传播 loopScw 沿共视图→MP 纠正→fuse→建回环边→EssentialGraph→GBA)；线程桩化单线程 `run` |

**测试**（新增 6 个，共 33 个全部 PASS）：
`test_sim3`(exp/log 4 分支往返/map/inverse/compose 结合律/s=1 退化为 SE3)、`test_sim3solver`(60 点 s=1.2+R+Δt clean rot=0/dt=0/s=1.2；30% 外点 RANSAC ≥38 内点位姿精确；bFixScale 强制 s=1)、`test_sim3opt`(扰动 rot0.05/dt0.1/s1.05 恢复 <1e-3；粗差剔除；bFixScale 下 s 锁定)、`test_searchbysim3`(2 KF+共享 MP+精确 S12，40/40 匹配且全部同世界点；描述子篡改不匹配)、`test_essgraph`(3 KF 链+共视边(真值测量)+漂移初始化，优化后 kf1/kf2 收敛真值 t_err=0/r_err=0)、`test_loopclosing`(3 KF 共享 MP+一致刚体漂移 D+注入 loopScw，correct_loop 后 kf0/kf1/kf2 全部恢复真值)。

## 关键约定（已逐行核对 g2o/ORB-SLAM3 源码）

- **四元数**：Hamilton、w-first `{w,x,y,z}`，与 Sophus/Eigen 一致。
- **Mat3**：行主序 `m[row*3+col]`，与 Eigen `Matrix3f` 一致。
- **SE3 更新**：左扰动 `T_new = exp(ξ)·T`，`exp` 用 g2o 的**完整** SE3 指数（含左雅可比 V），与 `VertexSE3Expmap::oplusImpl` 一致（`Thirdparty/g2o/g2o/types/se3quat.h:223`、`types_six_dof_expmap.h:73`）。
- **位姿雅可比**：`A = −projectJac(Xc)·SE3deriv(Xc)`，`SE3deriv = [−skew(Xc) | I]`（`src/OptimizableTypes.cpp:49-63`）。
- **IRLS 鲁棒权重**：`w = rho[1] = min(1, δ/√χ²)`，`H += w·AᵀΩA`、`b −= w·AᵀΩr`（`base_unary_edge.hpp:56-67`、`base_edge.h:96`）。
- **LM 求解器**：阻尼 `H+λI`，`λ_init = 1e-5·maxdiag`，Nielsen 增益比，α∈[1/3,2/3]，连续 3 步无改善即停（`optimization_algorithm_levenberg.cpp`）。
- **4 轮结构**：0–2 轮用 Huber 核（`δ=√5.991` mono / `√7.815` stereo），第 3 轮摘核；每轮按 `χ²` 阈值重分类内外点，边数 <10 提前停。
- **ORB 描述子**：`bit_pattern_31_`(256 对) + `umax`(16) 逐字复制；旋转约定 `row_off=round(px·sin+py·cos)`、`col_off=round(px·cos−py·sin)`；bit0=LSB。给定相同角点+方向，描述子与 OpenCV 一致，词袋兼容。
- **RGBD 立体**：`uR = u − mbf/Z`，深度在 `GrabImageRGBD` 已按 `DepthMapFactor` 缩放为米（`Frame.cc:1126-1147`）。TUM3 零畸变 → 去畸变 no-op。
- **网格**：64×48，`mfGridElementWidthInv=64/imgW`，`GetFeaturesInArea` 用 `|dx|<r && |dy|<r` 方框（`Frame.cc:799-865`）。堆分配（避免 150KB 栈溢出，§7.3）。
- **isInFrustum**（`Frame.cc:651-716`，Nleft==-1）：`Pc=Rcw·P+tcw`→`PcZ<0` 拒→project→边界[0,imgW]/[0,imgH]→尺度不变距离带[0.8·minD, 1.2·maxD]→`viewCos=PO·normal/dist`<limit 拒→PredictScale→填 track 缓存。视角阈值是**参数**（SearchLocalPoints 传 0.5=cos60°），非 0.000174。
- **PredictScale**（`MapPoint.cc:539`）：`ratio=mfMaxDistance/currentDist`（**方向**），`nScale=ceil(log(ratio)/mfLogScaleFactor)`，clamp[0,nlevels-1]。`mfLogScaleFactor=log(mvScaleFactor[1])`（必用 level1，因 `log(mvScaleFactor[0]=1)=0`）。
- **UpdateNormalAndDepth**（`MapPoint.cc:476-499`）：`mfMaxDistance=dist·mvScaleFactor[level]`，**level=refKF 观测 octave**（非 max level）；`mfMinDistance=mfMaxDistance/mvScaleFactor[nlevels-1]`。第二期曾误用 max level 算 max（高估 ~3.58×），第三期修正。
- **SearchByProjection #1/#2 差异**（`ORBmatcher.cc:47` vs `:1680`）：#1 用缓存投影+RadiusByViewingCos·th+ratio(0.8) 同 octave 才用+无方向直方图；#2 现算投影+th·mvScaleFactors[lastOctave]+无 ratio+方向直方图。`RadiusByViewingCos`：cos>0.998→2.5 else 4.0。
- **方向直方图**：`factor=1/HISTO_LENGTH=1/30`（每 bin 30°宽，照搬勿改 1/12），`bin=round((angLF-angCF)/30)%30`，`ComputeThreeMaxima` top-3 + `0.1·max1` 剪枝，非 top-3 bin 匹配撤销。
- **TrackWithMotionModel/TrackLocalMap**：th=7(stereo)/15(else)，<20 重试 2th；TrackLocalMap th=3(RGBD)/1(mono)；非 IMU 成功门限 nmatchesMap≥10 / mnMatchesInliers≥30。
- **DBoW2 词表**（`TemplatedVocabulary.h`）：ORB-SLAM3 大词表 k=10/L=6，1,082,074 节点 / 971,815 词。文本格式头 `k L scoring weighting`、节点行 `parent isLeaf d[0..31] weight(double)`；二进制 41B/节点=`parent(i32)+desc[32]+weight(float)+is_leaf(bool)`（weight 二进制为 float、文本为 double，照搬 DBoW2）。
- **transform**（`:1224-1265`）：root 逐层选 Hamming 最近子节点下推至叶子；`nid_level=m_L-levelsup`，`ComputeBoW` 用 levelsup=4 → 记 level-2 节点（≤100 bin，粗粒度但忠实）。TF_IDF+L1 scoring → BowVector L1 归一化（`sum|v_i|=1`）。FeatureVector 同节点内特征索引按升序（DBoW2 `addFeature` 按 i_feature 追加）。
- **SearchByBoW**（`ORBmatcher.cc:227-429`）：两 FeatureVector 有序归并（lower_bound 跳过不重叠节点）；同 NodeId 内 KF×Frame 暴力 Hamming；TH_LOW=50 门限 + ratio(TrackRefKF=0.7)；30-bin 方向直方图 `bin=round((angKF-angF)/30)%30` + ComputeThreeMaxima top-3 剪枝。**仅左相机分支（Nleft==-1）**，右分支及其 `|| true` 调试残留不做。
- **TrackReferenceKeyFrame**（`Tracking.cc:2767-2826`）：ComputeBoW→SearchByBoW(ratio=0.7)→`<15` 失败→`SetPose(lastPose)`→PoseOptimization→外点剔除(`mbTrackInView=0`、`mnLastFrameSeen=mnId`、`nmatches--`)→`nmatchesMap≥10`（非 IMU）。`score()`/倒排索引/MLPnPsolver 重定位留 Stage 2。

## ORB 提取的忠实性说明

`bit_pattern_31_`、`umax`、IC_Angle 公式、rBRIEF 旋转/位打包均逐行复制原码 → 描述子词袋兼容。以下为**重新实现**（非逐位等同 OpenCV），角点集合在边缘可能略有差异，但每个描述子仍是合法 ORB 描述子：
- FAST-9_16 检测 + NMS + 角点分数（自写，基数加速测试用正确的 `≥2` 必要条件）。
- 金字塔缩放（双线性，OpenCV `INTER_LINEAR` 像素中心约定）。
- 7×7 σ=2 高斯模糊（描述子用，reflect-101 边界）。
- `atan2f` 替代 `cv::fastAtan2`。
- 关键点内缩 ≥16px，最大采样半径 15 → 原 19px reflect-101 边界数值上不可达，存层不带边框 + 采样钳位（§7.3）。

## 构建

需要本机 CMake 环境（先运行 `c:\Users\frank.tu\CMakePath.bat`）。纯 C，无任何外部依赖（仅 `<math.h>`）。

```bat
cd NGD-SLAM\refactor
cmake -B myBuild -G "Visual Studio 17 2022" -A x64
cmake --build myBuild --config Release
ctest --test-dir myBuild -C Release --output-on-failure
```

GCC/Linux 同样适用（自动链 `libm`）。预期：33 个测试全部 `PASS`。

## 后续路线图（P0 → P4，文档 §6.8）

| 优先级 | 模块 | 状态 |
|---|---|---|
| P0 | 数学库 + SE3/SO3 + PoseOptimization | ✅ 首期 |
| P0 | ORB 提取 + Frame(RGBD) + StereoInitialization | ✅ 第二期 |
| P0/P1 | SearchByProjection(两个重载) + isInFrustum + TrackWithMotionModel + 简化 TrackLocalMap | ✅ 第三期 |
| P0 | DBoW2 词表 + SearchByBoW + TrackReferenceKeyFrame | ✅ 第四期 |
| P0 | 重定位（score + KeyFrameDB 倒排索引 + EPnP + Relocalization） | ✅ 第五期 |
| P1 | 简化 LocalMapping（KF 插入 + MapPointCulling） | ✅ 第六期（CreateNewMapPoints 三角化/SearchInNeighbors Fuse/KeyFrameCulling 仍留 P2） |
| P2 | LocalBundleAdjustment（稀疏 Schur，g2o 最难，可降级） | ✅ 第七期（Schur 补 LM BA；CreateNewMapPoints ✅ 第八期；SearchInNeighbors Fuse/KeyFrameCulling ✅ 第九期） |
| P2 | YOLO + 动态 mask 链（`PredictCurrentMask` 等）+ `SearchByOpticalFlow` | ⏳ 第十期(mask 生成核心纯 C + LK 回调)；`SearchByOpticalFlow`/`TrackWithOpticalFlow` 待第十一期；YOLO `cv::dnn` 薄壳待第十二期 |
| P3 | TrackWithOpticalFlow（PnP+RANSAC，`solvePnPRansac` 无 C API → 自实现 EPnP） | EPnP ✅ 第五期；MLPnP ✅ 第十一期（与 EPnP 并存，后续定取舍） |
| P3 | Sim3 + LoopClosing + OptimizeEssentialGraph | ✅ 第十二期 a–g（单地图/非IMU/非merge；Sim3/Sim3Solver/OptimizeSim3/SearchBySim3/OptimizeEssentialGraph/GlobalBA/LoopClosing 编排） |
| P4 | Viewer | 嵌入式通常省略 |

### TrackReferenceKeyFrame 的 DBoW2 决策（已落地：路线 B）

原 `TrackReferenceKeyFrame`(`Tracking.cc:2767`) 用 `SearchByBoW`(DBoW2 词袋匹配) 做初始匹配，依赖 DBoW2 词表树 + `BowVector`/`FeatureVector`。第四期按**路线 B（移植 DBoW2）**落地：纯 C 词表（load ORBvoc.txt/bin + transform levelsup=4 + L1 归一化 BowVector + FeatureVector）+ `SearchByBoW`（同节点 Hamming + ratio + 方向直方图）+ `TrackReferenceKeyFrame`。词表为 ORB-SLAM3 大词表（k=10, L=6, 1,082,074 节点 / 971,815 词），二进制 41B/节点（weight 为 float，文本为 double，照搬 DBoW2）。

**Stage 2（重定位）已落地**：`score()`(L1 评分) + KeyFrameDatabase 倒排索引 + `DetectRelocalizationCandidates` + EPnP(RANSAC) + `Relocalization`。第五期按 doc §6.5 用 **EPnP**（自实现，OpenCV epnp.cpp 忠实移植）替代原 MLPnPsolver（ML PnP 依赖 Eigen，纯 C 复现代价高；EPnP 给种子位姿后 PoseOptimization 精修，等价）。同时补建 KeyFrame 共视图（`AddConnection`/`UpdateConnections`/`GetBestCovisibilityKeyFrames`），并顺手补回第三期简化掉的 UpdateLocalKeyFrames 协视图邻居扩展。`test_reloc` 端到端：203 内点、RMS 0.67px、位姿近真值。

## 设计原则（文档 §6.4 / §7.3）

- 全部结构体显式零初始化，杜绝未初始化隐患。
- 无 `cv::Mat::at` 无边界访问；所有像素索引带钳位/范围/深度/NaN 守护。
- 大对象（网格、金字塔、MapPoint 观测数组）堆分配；Frame/KeyFrame 网格为指针字段。
- 纯值类型，热路径无隐式分配，便于后续对象池化。
