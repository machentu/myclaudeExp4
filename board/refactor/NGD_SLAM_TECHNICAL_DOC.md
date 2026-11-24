# NGD-SLAM 技术文档：架构 / 原理 / 代码分析 / 移植

> 本文档基于 NGD-SLAM（在 ORB-SLAM3 基础上集成 YOLO 动态物体检测 + 光流跟踪的版本）的实际源码分析。
> 项目根目录：`NGD-SLAM/`
> 所有代码位置以 `file:line` 标注。

---

## 目录

1. [系统架构与线程模型](#1-系统架构与线程模型)
2. [核心算法原理](#2-核心算法原理)
3. [动态物体剔除机制（NGD 核心）](#3-动态物体剔除机制ngd-核心)
4. [详细代码分析](#4-详细代码分析)
5. [数据流总览](#5-数据流总览)
6. [移植到纯 C / 嵌入式环境注意事项](#6-移植到纯-c--嵌入式环境注意事项)
7. [Windows 移植经验（已完成）](#7-windows-移植经验已完成)

---

## 1. 系统架构与线程模型

### 1.1 整体架构

NGD-SLAM 是一个**多线程**系统，由 5 个线程组成（`src/System.cc:205-254`）：

```
┌─────────────────────────────────────────────────────────────────┐
│                      System (主控制类)                           │
├─────────────────────────────────────────────────────────────────┤
│  主线程          │ Tracking          │ 前端跟踪（同步，在 TrackRGBD 调用线程） │
│  线程1 →         │ LocalMapping      │ 局部地图管理、局部 BA              │
│  线程2 →         │ LoopClosing       │ 回环检测、位姿图优化、全局 BA       │
│  线程3 →(NGD新增)│ YOLO              │ 动态目标检测，输出动态 mask         │
│  线程4 →         │ Viewer            │ Pangolin 可视化                    │
└─────────────────────────────────────────────────────────────────┘
```

**关键设计**：Tracking **不是**独立线程，而是同步运行在调用 `System::TrackRGBD` 的线程中（主线程，`src/System.cc:417`）。其他四个才是后台线程。

### 1.2 线程启动（`src/System.cc:205-254`）

```cpp
// src/System.cc:211
mptLocalMapping = new thread(&LocalMapping::Run, mpLocalMapper);
// src/System.cc:228
mptLoopClosing  = new thread(&LoopClosing::Run,   mpLoopCloser);
// src/System.cc:232  ← NGD 新增
mptYOLO         = new thread(&YOLO::Run,          mpYOLO);
// src/System.cc:252  (仅 bUseViewer=true)
mptViewer       = new thread(&Viewer::Run,         mpViewer);
```

### 1.3 线程间指针互连（`src/System.cc:235-243`）

线程间通过指针共享对象：
```cpp
mpTracker->SetLocalMapper(mpLocalMapper);
mpTracker->SetLoopClosing(mpLoopCloser);
mpTracker->SetYOLO(mpYOLO);              // NGD 新增，SetYOLO 定义于 Tracking.cc:1444
mpLocalMapper->SetTracker(mpTracker);
mpLocalMapper->SetLoopCloser(mpLoopCloser);
mpLoopCloser->SetTracker(mpTracker);
mpLoopCloser->SetLocalMapper(mpLocalMapper);
```

### 1.4 线程间数据流

```
        ┌─────────┐  InsertKeyFrame(KF)   ┌──────────────┐
图像 →  │ Tracking │ ───────────────────→ │ LocalMapping │ → 局部 BA
        │  (前端)  │ ←─────────────────── │   (后端)     │
        └────┬─────┘  mpLocalMapper 状态    └──────┬───────┘
             │ InsertKeyFrame(KF)                  │
             ▼                                    ▼
        ┌─────────┐                        ┌──────────────┐
        │  YOLO   │ ← InsertInput(帧)      │ LoopClosing  │ → 回环 + 全局 BA
        │(动态检测)│ ─→ GetOutput(mask)     │   (回环)     │
        └─────────┘                        └──────────────┘
                                                    │ 地图更新
                                                    ▼
                                              ┌──────────┐
                                              │  Viewer  │ 可视化
                                              └──────────┘
```

- **Tracking → LocalMapping**：通过 `InsertKeyFrame` 队列传递关键帧
- **Tracking ↔ YOLO**：`InsertInput`（投递帧）/ `GetOutput`（取 mask）双缓冲
- **LocalMapping → LoopClosing**：通过 `InsertKeyFrame` 队列
- **所有线程 → Viewer**：通过 `FrameDrawer`/`MapDrawer` 读取共享状态

---

## 2. 核心算法原理

### 2.1 ORB 特征提取（`src/ORBextractor.cc`）

**ORB = FAST 角点 + rBRIEF 描述子**，通过图像金字塔获得尺度不变性。

**关键参数**（构造函数 `ORBextractor.cc:409-469`）：
- `nfeatures`：期望特征总数
- `scaleFactor`：金字塔缩放比（典型 1.2）
- `nlevels`：金字塔层数（典型 8）
- `iniThFAST`/`minThFAST`：FAST 阈值，先用高阈值，提不到时降级

**提取流程**（`operator()` `ORBextractor.cc:1086-1168`）：

1. **图像金字塔**（`ComputePyramid` `:1170-1195`）：每层 resize 上一层，补 `EDGE_THRESHOLD=19` 边界
2. **四叉树均匀分布**（`ComputeKeyPointsOctTree` `:781-896` + `DistributeOctTree` `:555-779`）：
   - 按 35 像素网格分 cell，每 cell 跑 `FAST`
   - 递归四等分含 >1 角点的节点，直到节点数 ≥ 目标
   - **每个叶子节点只保留 response 最大的角点**
3. **灰度质心主方向**（`IC_Angle` `:76-103`）：半径 15 圆形 patch 累加图像矩 `m_10/m_01`，`angle = atan2(m_01, m_01)`
4. **rBRIEF 描述子**（`computeOrbDescriptor` `:107-146`）：按主方向旋转 256 对采样点，256 次像素对比较 → 32 字节描述子

### 2.2 特征匹配（`src/ORBmatcher.cc`）

#### 描述子距离（`DescriptorDistance` `ORBmatcher.cc:2131-2147`）

ORB 描述子 32 字节 = 8 个 int32。汉明距离用**并行 popcount**：
```cpp
v = v - ((v >> 1) & 0x55555555);
v = (v & 0x33333333) + ((v >> 2) & 0x33333333);
dist += (((v + (v >> 4)) & 0xF0F0F0F) * 0x1010101) >> 24;
```
阈值：`TH_HIGH=100`、`TH_LOW=50`（`ORBmatcher.cc:39-41`）

#### 主要匹配方法

| 方法 | 位置 | 用途 | 原理 |
|---|---|---|---|
| `SearchByOpticalFlow` | `:2016` | **NGD 新增**，光流跟踪 | LK 光流搬运 3D-2D 对应，mask 过滤动态点 |
| `SearchByProjection` | `:47`/`:1680` | TrackLocalMap / 帧间 | 投影 + 比率测试 + 旋转直方图 |
| `SearchForInitialization` | `:652` | 单目初始化 | octave 0 邻域搜索 + 双向唯一 |
| `SearchByBoW` | `:227`/`:769` | 重定位 / 回环 | 词袋节点分组加速匹配 |
| `SearchBySim3` | `:1461` | 回环几何验证 | Sim3 投影匹配 |
| `Fuse` | `:1152`/`:1344` | LocalMapping | 融合重复地图点 |

### 2.3 位姿估计与优化

#### TrackWithOpticalFlow（NGD 新增，PnP+RANSAC）（`Tracking.cc:2996-3102`）

```cpp
// 1. 光流匹配得 2D-3D 对应
int nmatches = matcher.SearchByOpticalFlow(mCurrentFrame, mLastFrameLK, ...);

// 2. 收集 2D 点 (currKeyPoints) 和 3D 点 (currMapPoints, GetWorldPos())

// 3. 恒速模型初值
Sophus::SE3f initTcw = mVelocityLK * mLastFrameLK.GetPose();

// 4. PnP + RANSAC：最小化重投影误差 ||π(K·(R·X+t)) − u||²
cv::solvePnPRansac(currMapPoints, currKeyPoints, mK, mDistCoef,
                  R_vector, T, false, 100, 5, 0.99, ransacInlier, cv::SOLVEPNP_ITERATIVE);

// 5. 速度合理性检查：若发散回退初值
if(currVeloMagnitude > 3*lastVeloMagnitude && currVeloMagnitude > 0.05) {
    mCurrentFrame.SetPose(initTcw);  // 回退
}
```

#### 位姿优化 PoseOptimization（`Optimizer.cc:814-1114`）

g2o 单帧位姿优化，**4 轮迭代**：
- 顶点：`VertexSE3Expmap`（当前帧位姿，李代数 se3）
- 边：`EdgeSE3ProjectXYZOnlyPose`（每个 MapPoint 一条）
- 鲁棒核：Huber，`deltaMono=√5.991`、`deltaStereo=√7.815`
- 每轮按 chi2 阈值重新分类内外点，第 3 轮起去掉鲁棒核

#### 局部 BA LocalBundleAdjustment（`Optimizer.cc:1116`）

同时优化位姿与地图点：
- **局部关键帧**：当前 KF + 共视 KF（广度遍历）
- **局部地图点**：这些 KF 看到的 MP
- **固定关键帧**：看到局部 MP 但非局部 KF 的 KF（固定约束）
- 顶点：`VertexSE3Expmap`（KF）+ `VertexSBAPointXYZ`（MP），Levenberg 求解器

### 2.4 地图管理

#### Atlas 多地图（`src/Atlas.cc`）

```cpp
std::set<Map*> mspMaps;    // 所有地图
Map* mpCurrentMap;         // 当前地图
```

**为什么需要多地图**：当跟踪彻底丢失（`Tracking.cc:2134` LOST）且当前地图 KF>10 时，`CreateMapInAtlas()` 开新地图而非清零，从而支持**地图复用/重定位回到旧地图**；IMU 场景也便于按段初始化。

#### 关键数据结构

**MapPoint**（`include/MapPoint.h:44-253`）：
- `mWorldPos`（Eigen::Vector3f）：世界坐标
- `mObservations`（map<KeyFrame*, tuple<leftIdx,rightIdx>>）：被哪些 KF 观测
- `mNormalVector`：平均观测方向
- `mDescriptor`：最具区分度描述子
- `mnVisible/mnFound`：可见/匹配计数，用于剔除

**KeyFrame**（`include/KeyFrame.h:195-259`）：
- 位姿 `mTcw`（SE3f）、BoW 向量
- **共视图**（covisibility graph）：`mConnectedKeyFrameWeights`（权重=共视 MP 数）
- **生成树**（spanning tree）：`parent`/`mChildrens`
- **回环边**`mLoopEdges`

#### 关键帧选取 NeedNewKeyFrame（`Tracking.cc:3220-3360`）

逻辑 `((c1a||c1b||c1c)&&c2)||c3||c4||c5`：
- `c1a`：距上一 KF 已过 `mMaxFrames` 帧
- `c1b`：过 `mMinFrames` 且 LocalMapping 空闲
- `c1c`：跟踪弱
- `c2`：跟踪点数 < 参考帧的 `thRefRatio`（单目 0.9，双目 0.75）且 >15
- **`c5`（NGD 新增，`:3331`）**：`mbStartOpticalFlow` 且匹配数 <20 或衰减，触发更激进的关键帧插入

#### 地图点剔除 MapPointCulling（`LocalMapping.cc:354-393`）

对近 3 个 KF 内新建的点：
1. `GetFoundRatio() < 0.25` → 坏点
2. 观测数 ≤ `cnThObs`（单目 2，双目/RGBD 3）→ 坏点
3. 存在 ≥3 个 KF → 移出"近期"列表（已稳定）

### 2.5 回环检测与闭环（`src/LoopClosing.cc`）

#### 主流程（`LoopClosing::Run` `:90-312`）

```
KF 入队 → NewDetectCommonRegions()
  ├─ 时序传播验证（用上帧 Sim3 预测）
  └─ DBoW2 词袋查询 → DetectNBestCandidates (取 N-best 候选)
       └─ DetectCommonRegionsFromBoW 几何验证
            ├─ SearchByBoW (BoW 匹配 ≥20)
            ├─ Sim3Solver RANSAC (≥15 内点)
            ├─ SearchByProjection (Sim3 投影, ≥50)
            ├─ OptimizeSim3 精化
            └─ SearchByProjection 验证 (≥80)
需要连续 3 次重合 → mbLoopDetected
       ↓
CorrectLoop()
  ├─ 停 LocalMapping + 中止 GBA
  ├─ 沿共视图传播纠正位姿
  ├─ 纠正地图点坐标
  ├─ SearchAndFuse (投影融合)
  ├─ 构建 LoopConnections
  ├─ OptimizeEssentialGraph (位姿图优化)
  └─ RunGlobalBundleAdjustment (全局 BA)
```

#### Sim3 优化（`Optimizer.cc:2115`）

优化 7 自由度相似变换 `[sR|t]`，最小化两视图间重投影误差。单目可变尺度，双目/RGBD 固定尺度=1。

#### 位姿图优化 OptimizeEssentialGraph（`Optimizer.cc:1501`）

所有 KF 作 `VertexSim3Expmap` 顶点（初始 KF fixed），边包括回环边、共视图边、生成树边，最小化 Sim3 上的位姿图能量，把回环纠正传播到全图。

### 2.6 IMU 集成（`src/ImuTypes.cc`）

#### 预积分 Preintegrated（`ImuTypes.h:143-251`）

将两帧间 IMU 测量积分成相对约束，使后端优化时不必随 bias 变化重新积分：
- 状态量：`dR`（旋转）、`dV`（速度）、`dP`（位置）增量
- 雅可比：`JRg/JVg/JVa/JPg/JPa`（对 bias 的一阶更新）
- 协方差：`C`（15×15）

#### 中点法积分（`IntegrateNewMeasurement` `ImuTypes.cc:177-235`）

```cpp
dP = dP + dV*dt + 0.5*dR*acc*dt²;
dV = dV + dR*acc*dt;
dR = NormalizeRotation(dR*dRi.deltaR);
// 同步更新协方差 C = A·C·Aᵀ + B·Nga·Bᵀ
```

IMU 初始化分两阶段（`LocalMapping.cc:204-228`）：`mTinit>5s` 触发 VIBA1，`>15s` 触发 VIBA2。

---

## 3. 动态物体剔除机制（NGD 核心）

这是 NGD-SLAM 区别于原版 ORB-SLAM3 的核心增量，用于在动态环境（行人/车辆）下剔除移动物体上的特征点。

### 3.1 数据流总览

```
上一帧 mask (mImMaskLastKey)
  │
  ▼
ExtractDynaPoints（在 mImGrayLastKey 上挑种子点，FAST 角点响应）
  │
  ▼
cv::calcOpticalFlowPyrLK (mImGrayLastKey → mImGray)  传播到当前帧
  │
  ▼
深度过滤 → trackedDynaPoints (x,y,depth)
  │
  ▼
ClusterWithDBSCAN（深度分段 + 3D DBSCAN）
  │
  ▼
CreateMaskFromClusters（外接矩形 + 深度门 + 最大连通域 + 膨胀）
  │
  ▼
当前帧 mask mImMask
  │
  ├─────────────────────────────────────┐
  ▼                                     ▼
SearchByOpticalFlow 用 mImMask 过滤    Frame 构造用 mImMask 过滤 ORB 特征
LK 匹配（动态点丢弃）                   （动态点丢弃，仅静态特征进 Frame）
```

### 3.2 YOLO 动态检测（`src/YOLO.cc`）

**模型**：yolo-fastest-xl（`src/System.cc:231`），通过 `cv::dnn::readNetFromDarknet` 加载。

**生产者-消费者模型**（双互斥锁 + 双缓冲 `cv::Mat`）：
- `InsertInput`（`YOLO.cc:197-203`）：锁 `mInputMutex`，`clear()` 后压入 `[imRGB, imDepth]`（仅保留最新输入）
- `GetOutput`（`YOLO.cc:205-215`）：锁 `mOutputMutex`，`std::move` 取出 `[imGray, imMask]`
- `Run`（`YOLO.cc:217-250`）：`while(1)` 取输入 → `Detect` → 写输出，否则 `sleep_for(10ms)`

**Detect + postprocess**（`YOLO.cc:25-166`）：
1. `blobFromImage(320×320, 1/255)` → `forward`
2. NMS 去重
3. 对每个 `classIds==0`（person）的框：
   - 宽框收紧到 70%
   - 计算框内有效深度像素（≥0.05），要求有效率>70%
   - `value = (median + third)/2`
   - 扩展框内，深度与 `value` 差 ≤0.33 的像素置 `localMask=1`（**深度一致性分割**）
   - `connectedComponentsWithStats` 取最大连通域加入 `globalMask`
4. 膨胀 `globalMask`
5. 安全阀：mask 覆盖 >80% 图像则清零

输出：单通道 `CV_8UC1` mask，**1=动态，0=静态**。

### 3.3 PredictCurrentMask（`Tracking.cc:4249-4305`）

光流预测当前帧动态 mask：

```cpp
void Tracking::PredictCurrentMask() {
    mImMask = cv::Mat::zeros(mImMaskLastKey.size(), CV_8UC1);  // 静态背景=0

    // 1. 腐蚀上一关键帧 mask
    erode(mImMaskLastKey, mImMaskLastKey, erosionElement);  // erosionSize=5

    // 2. 从上一 mask 区域挑种子点（FAST 风格角点响应）
    ExtractDynaPoints(lastDynaPoints, mImGrayLastKey, mImMaskLastKey, 15);

    if(!lastDynaPoints.empty()) {
        // 3. LK 光流传播到当前帧
        cv::calcOpticalFlowPyrLK(mImGrayLastKey, mImGray, lastDynaPoints, currDynaPoints, ...);

        // 4. 深度过滤 → trackedDynaPoints (带边界+NaN保护)
        for(...) {
            // 越界/NaN 跳过
            float depth = mImDepth2.at<float>(py, px);
            if(depth >= 0.05f) trackedDynaPoints.push_back(Point3f(px,py,depth));
        }

        // 5. DBSCAN 聚类
        ClusterWithDBSCAN(clusters, trackedDynaPoints, 50.0f, 15);

        // 6. 生成 mask
        CreateMaskFromClusters(clusters);
    }
}
```

### 3.4 关键函数说明

| 函数 | 位置 | 作用 |
|---|---|---|
| `PredictCurrentMask` | `Tracking.cc:4249` | 光流预测当前帧 mask |
| `ExtractDynaPoints` | `Tracking.cc:4355` | 在 mask 区域网格挑角点响应最大的种子点（cellSize=15） |
| `ComputePixelPotential` | `Tracking.cc:4308` | FAST 风格角点响应（Bresenham 圆半径 3） |
| `ClusterWithDBSCAN` | `Tracking.cc:4402` | 两阶段：深度分段 + 3D DBSCAN（eps=50, minPts=15） |
| `CreateMaskFromClusters` | `Tracking.cc:4486` | 外接矩形 + 深度门（±0.3）+ 最大连通域 + 膨胀 |
| `SearchByOpticalFlow` | `ORBmatcher.cc:2016` | LK 光流 + mask 过滤动态点 |

### 3.5 双 ORB 提取器（NGD 新增）

```cpp
// 初始化（Tracking.cc:601 / 1289-1290）
mpORBextractorLeft = new ORBextractor(nFeatures, ...);
mpORBextractorDyna = new ORBextractor(nFeatures*1.5, ...);  // 多 50% 特征
```

**设计意图**：当检测到动态物体（mask 非空）时，部分特征被 mask 过滤掉，用更多特征数补偿。

**使用**（`GrabImageRGBD` `Tracking.cc:1637-1638`）：
```cpp
if(cv::countNonZero(mImMask)!=0 || cv::countNonZero(mImMaskLastKey)!=0)
    mpORBextractorTmp = mpORBextractorDyna;   // 有动态物体
else
    mpORBextractorTmp = mpORBextractorLeft;     // 静态场景
```

---

## 4. 详细代码分析

### 4.1 GrabImageRGBD 完整流程（`Tracking.cc:1532-1675`）

这是 NGD-SLAM 前端的核心状态机，每帧执行：

```cpp
Sophus::SE3f Tracking::GrabImageRGBD(const cv::Mat &imRGB, const cv::Mat &imD,
                                     const double &timestamp, string filename)
{
    // (1) 图像预处理
    mImGray = imRGB;  // RGB → Gray
    mImRGB = imRGB.clone();
    mImDepth2 = imD.clone();
    mImDepth.convertTo(mImDepth, CV_32F, mDepthMapFactor);  // 深度缩放

    // (2) 决定跟踪模式
    bool RGBDInitialized = (mFrameNum > 3);
    mbStartOpticalFlow = RGBDInitialized;

    // (3) YOLO 投递当前帧
    mpYOLO->InsertInput(mImRGB, mImDepth2);

    // (4) YOLO 输出轮询
    while(true) {
        auto output = mpYOLO->GetOutput();
        if(mFrameNum == 1) {           // 首帧：阻塞等 mask，10s 超时
            mImMask = output[1];
            break;
        } else if(mFrameNum > 1) {     // 后续帧：取 [imGray, imMask]
            // 大旋转 + mask 空判断
            mImGrayLastKey = output[0];
            mImMaskLastKey = output[1];
            break;
        }
        if(mFrameNum == 1 && elapsed > 10.0) {  // 超时保护
            cerr << "Warning: YOLO timeout...";
            break;
        }
    }

    // (5) 光流预测当前 mask
    if(mFrameNum > 1) PredictCurrentMask();

    // (6) 动态跟踪（光流）
    if(mbStartOpticalFlow) {
        mCurrentFrame = Frame(mbStartOpticalFlow);  // 无 ORB
        mCurrentFrame.mTimeStamp = timestamp;
        TrackWithOpticalFlow();                      // PnP 估计位姿
    }

    // (7) 关键帧判定
    if(!RGBDInitialized) mbNeedKF = true;
    else mbNeedKF = NeedNewKeyFrame();

    // (8) 关键帧/ORB 静态跟踪
    if(mbNeedKF) {
        Sophus::SE3f Tcw;
        if(mbStartOpticalFlow) Tcw = mCurrentFrame.GetPose();  // 保留光流位姿

        // 选 ORB 提取器（有动态用 Dyna）
        if(cv::countNonZero(mImMask)!=0 || ...)
            mpORBextractorTmp = mpORBextractorDyna;
        else
            mpORBextractorTmp = mpORBextractorLeft;

        // 构造带 mask 的 Frame（ORB 提取后按 mask 过滤）
        mCurrentFrame = Frame(mImGray, mImDepth, mImMask, timestamp,
                              mpORBextractorTmp, ...);

        if(mbStartOpticalFlow) mCurrentFrame.SetPose(Tcw);  // 恢复光流位姿
        Track();  // 标准 ORB-SLAM3 跟踪
    } else {
        // 非关键帧：仅保存轨迹
        ...
    }

    // (9) LK 状态更新
    mVelocityLK = mCurrentFrame.GetPose() * mLastFrameLK.GetPose().inverse();
    mImGrayLastKey = mImGray.clone();
    mImMaskLastKey = mImMask.clone();
    mImGrayLastLK = mImGray;
    mLastFrameLK = Frame(mCurrentFrame);
    mFrameNum++;

    return mCurrentFrame.GetPose();
}
```

### 4.2 关键成员变量（`Tracking.h:175-192`）

| 变量 | 作用 | 更新时机 |
|---|---|---|
| `mbStartOpticalFlow` | 是否启用光流跟踪 | 每帧由 `mFrameNum>3` 决定 |
| `mbNeedKF` | 本帧是否建关键帧 | 前 3 帧恒 true；之后 `NeedNewKeyFrame()` |
| `mFrameNum` | 帧计数，驱动初始化阶段 | 每帧末 `++` |
| `mLastFrameLK` | 光流跟踪的"上一帧" Frame | 每帧末 `Frame(mCurrentFrame)` |
| `mImGrayLastLK` | 光流源灰度图（LK 的 LastImg） | 每帧末 `mImGray` |
| `mVelocityLK` | 光流运动模型 | 每帧末 `curr*last.inverse()` |
| `mImMask` | 当前帧动态 mask（1=动态） | 首帧 YOLO；后续 `PredictCurrentMask` |
| `mImMaskLastKey` | 上一帧 mask，预测源 | YOLO 写入；每帧末覆盖 |
| `mImGrayLastKey` | 上一帧灰度图，LK 源 | YOLO 写入；每帧末覆盖 |
| `mImDepth2` | 深度克隆，供 YOLO 与 mask 预测查深度 | 帧首 clone + 缩放 |

**注意**：`mImGrayLastKey`/`mImMaskLastKey` 名义是"上一关键帧"，但实际每帧末都被更新，语义上更接近"上一帧的灰度/mask"。

### 4.3 与原版 ORB-SLAM3 的差异汇总

| 模块 | 原版 | NGD 新增 |
|---|---|---|
| 线程 | 4 个 | +YOLO 线程 |
| 跟踪 | TrackReferenceKeyFrame/TrackWithMotionModel | +TrackWithOpticalFlow（PnP+RANSAC） |
| 匹配 | SearchByProjection/BoW | +SearchByOpticalFlow（LK 光流） |
| mask | 无 | YOLO + PredictCurrentMask 动态 mask 链 |
| ORB 提取器 | 单一 | 双提取器（Left + Dyna×1.5） |
| 状态 | OK/LOST 等 | +OK_KLT=5（`Tracking.h:131`） |
| 关键帧判定 | c1-c4 | +c5（光流跟踪不稳时强制建 KF） |

---

## 5. 数据流总览

### 5.1 单帧完整处理流

```
图像输入
  │
  ▼
GrabImageRGBD
  ├─ 图像预处理（RGB→Gray, 深度缩放）
  ├─ YOLO InsertInput（异步投递）
  ├─ YOLO GetOutput（轮询取 mask）
  ├─ PredictCurrentMask（光流预测当前 mask）         ← NGD
  │
  ├─ [if mbStartOpticalFlow]
  │   └─ TrackWithOpticalFlow
  │       ├─ SearchByOpticalFlow（LK + mask 过滤）  ← NGD
  │       ├─ solvePnPRansac（PnP 位姿估计）
  │       └─ 速度合理性检查
  │
  ├─ NeedNewKeyFrame 判定
  │
  ├─ [if mbNeedKF]
  │   ├─ 选 ORB 提取器（Dyna/Left）                ← NGD
  │   ├─ 构造 Frame（ORB 提取 + mask 过滤）         ← NGD
  │   └─ Track()
  │       ├─ StereoInitialization（首帧）
  │       ├─ TrackReferenceKeyFrame / TrackWithMotionModel
  │       ├─ TrackLocalMap
  │       │   ├─ UpdateLocalMap
  │       │   ├─ SearchLocalPoints
  │       │   └─ PoseOptimization
  │       └─ CreateNewKeyFrame（入队 LocalMapping）
  │
  └─ LK 状态更新（mLastFrameLK, mVelocityLK, ...）
```

### 5.2 关键代码位置速查表

| 主题 | 文件:行 |
|---|---|
| 线程启动 | `src/System.cc:211,228,232,252` |
| Tracking 入口 | `src/Tracking.cc:1532` |
| TrackWithOpticalFlow（NGD） | `src/Tracking.cc:2996` |
| SearchByOpticalFlow（NGD） | `src/ORBmatcher.cc:2016` |
| PredictCurrentMask（NGD） | `src/Tracking.cc:4249` |
| ExtractDynaPoints（NGD） | `src/Tracking.cc:4355` |
| ClusterWithDBSCAN（NGD） | `src/Tracking.cc:4402` |
| CreateMaskFromClusters（NGD） | `src/Tracking.cc:4486` |
| StereoInitialization | `src/Tracking.cc:2458` |
| Track 状态机 | `src/Tracking.cc:1910` |
| NeedNewKeyFrame | `src/Tracking.cc:3220` |
| PoseOptimization | `src/Optimizer.cc:814` |
| LocalBundleAdjustment | `src/Optimizer.cc:1116` |
| OptimizeSim3 | `src/Optimizer.cc:2115` |
| OptimizeEssentialGraph | `src/Optimizer.cc:1501` |
| Atlas 多地图 | `src/Atlas.cc:58` |
| MapPointCulling | `src/LocalMapping.cc:354` |
| LoopClosing::Run | `src/LoopClosing.cc:90` |
| CorrectLoop | `src/LoopClosing.cc:972` |
| YOLO::Run | `src/YOLO.cc:217` |
| YOLO::Detect | `src/YOLO.cc:168` |
| ORB 构造 | `src/ORBextractor.cc:409` |

---

## 6. 移植到纯 C / 嵌入式环境注意事项

将 NGD-SLAM 从 C++11（MSVC/GCC）移植到纯 C 或嵌入式环境，需重点处理以下问题。

### 6.1 语言特性替换

| C++ 特性 | C 替代方案 | 注意点 |
|---|---|---|
| `class`/`struct` + 方法 | `struct` + 函数（首参传 `self*`） | 手动管理 this 指针 |
| `std::vector<T>` | 动态数组 + 长度/容量 | 需手写 `push_back`/`resize`/`clear` |
| `std::map`/`std::set` | 红黑树或哈希表库 | 共视图 `mConnectedKeyFrameWeights` 是 map |
| `std::thread` | pthread（POSIX）/ FreeRTOS 任务 | 同步用 mutex/condvar |
| `std::mutex` | `pthread_mutex_t` | RAII → 手动 lock/unlock |
| `std::chrono` | `clock_gettime`/`gettimeofday` | 注意精度 |
| 模板（g2o/Eigen） | 手写具体类型函数 | g2o 优化器需重写 |
| 异常 `try/catch` | 返回错误码 | OpenCV 的 C API 不抛异常 |
| `new`/`delete` | `malloc`/`free` | 注意构造/析构逻辑 |
| 引用 `&` | 指针 `*` | |

### 6.2 第三方库依赖处理

NGD-SLAM 依赖大量 C++ 库，纯 C 移植需替换：

| 库 | 用途 | C 替代/处理 |
|---|---|---|
| **Eigen** | 矩阵/向量运算 | 手写矩阵运算或用 C 线代库（如 GSL），或保留 Eigen（它是 header-only，部分可编译为 C 思路） |
| **Sophus** | 李群李代数（SE3/SO3） | 手写 SE3（四元数+平移）运算：乘法、逆、指数/对数映射 |
| **OpenCV** | 图像处理/特征 | 用 OpenCV 的 C API（`cv::Mat` → `CvMat`/`IplImage`），或 C 接口；`calcOpticalFlowPyrLK`/`solvePnPRansac` 有 C 版本 |
| **g2o** | 图优化 | **最难**。需用 C 实现图优化，或用 ceres-solver 的 C 接口，或手写高斯-牛顿/LM |
| **DBoW2** | 词袋 | 用 C 重写词袋（k-means 聚类树 + 查询） |
| **Pangolin** | 可视化 | 移植时通常省略，或用 OpenGL C API |

### 6.3 重点重写的算法模块

#### (1) g2o 图优化（最大工作量）

PoseOptimization/LocalBA/GlobalBA/Sim3/EssentialGraph 都基于 g2o。C 移植选项：
- **手写高斯-牛顿/LM**：对每个优化问题，构建雅可比 `J`、残差 `r`，解正规方程 `JᵀJ·Δx = -Jᵀr`
- 位姿优化（单帧）：6 自由度（se3），雅可比是重投影误差对位姿的偏导
- 局部 BA：位姿(6)+地图点(3)，稀疏求解
- **建议**：优先实现 `PoseOptimization`（最常用，单帧 6-DOF），BA 可降级或省略

#### (2) Sophus 李代数

SE3 运算需手写：
```c
// SE3 = (四元数 q, 平移 t)
typedef struct { float q[4]; float t[3]; } SE3;

SE3 se3_multiply(SE3 a, SE3 b);     // 旋转复合 + 平移
SE3 se3_inverse(SE3 s);             // 四元数共轭 + -Rᵀt
void se3_to_matrix(SE3 s, float R[9], float t[3]);  // 四元数→旋转矩阵
SE3 se3_exp(float xi[6]);           // 李代数→李群（指数映射）
void se3_log(SE3 s, float xi[6]);   // 李群→李代数
```

#### (3) ORB 特征提取

OpenCV 的 `FAST` 和 `BRIEF` 有 C 实现。但四叉树分布（`DistributeOctTree`）需手写：
- 网格分 cell → FAST → 递归四等分 → 每节点保留 response 最大点

### 6.4 内存与栈管理（嵌入式关键）

这是移植到资源受限环境**最容易出问题**的地方（也是 Windows 移植的主要教训）：

| 问题 | 原因 | 对策 |
|---|---|---|
| **大对象放栈** | `Frame` 含 `mGrid[64][48]`（3072 vector）≈150KB | 改堆分配或静态缓冲池 |
| **栈大小不足** | 嵌入式默认栈可能仅 8-64KB | 增大栈或避免深递归 + 大局部变量 |
| **越界访问** | `cv::Mat::at` 无边界检查 | 加边界检查（已修复的经验） |
| **内存碎片** | 频繁 `new`/`delete` | 内存池预分配 |
| **未初始化内存** | 默认构造不保证清零 | 显式 `memset` |

**具体建议**：
- Frame/KeyFrame/MapPoint 用**对象池**（预分配数组 + 空闲链表），避免运行时分配
- 所有 `cv::Mat::at` 替换为带边界检查的访问宏
- 关键数据结构（位姿、地图点）用固定大小数组，禁用动态容器

### 6.5 多线程改造

```c
// C++ std::thread → pthread
pthread_t tid;
pthread_create(&tid, NULL, local_mapping_run, (void*)pSystem);

// C++ std::mutex → pthread_mutex_t
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_lock(&mtx);
// ...
pthread_mutex_unlock(&mtx);

// 条件变量（YOLO 生产者-消费者）
pthread_cond_t cond;
pthread_cond_wait(&cond, &mtx);
pthread_cond_signal(&cond);
```

**嵌入式（FreeRTOS）**：
- `xTaskCreate` 替代 `pthread_create`
- `xSemaphoreCreateMutex` 替代 `pthread_mutex_t`
- `xQueueSend`/`xQueueReceive` 替代双缓冲队列
- 注意任务栈大小（LocalMapping/LoopClosing 需较大栈）

### 6.6 YOLO 模块移植

YOLO 用 OpenCV DNN，移植时：
- **嵌入式**：换轻量推理框架（NCNN/TNN/MNN/TensorRT Lite），或用 NPU
- `cv::dnn::readNetFromDarknet` → 框架对应的模型加载
- `blobFromImage`/`forward` → 框架预处理 + 推理
- `postprocess`（NMS + 深度一致性分割）可保留逻辑，用 C 重写

### 6.7 OpenCV 函数的 C API 映射

| C++ | C API |
|---|---|
| `cv::Mat` | `CvMat`/`IplImage` |
| `cv::imread` | `cvLoadImage` |
| `cv::calcOpticalFlowPyrLK` | `cvCalcOpticalFlowPyrLK`（存在） |
| `cv::solvePnPRansac` | 需手写或用 cv2 C 接口 |
| `cv::findContours` | `cvFindContours` |
| `cv::connectedComponentsWithStats` | 需手写或用 `cvFindContours` 替代 |

**注意**：OpenCV 较新版本（4.x）已弱化 C API，建议用 OpenCV C++ 编译，仅业务逻辑用 C，或整体保留 C++ 但适配嵌入式编译器。

### 6.8 移植优先级建议

按"先跑通最小闭环"原则排序：

| 优先级 | 模块 | 说明 |
|---|---|---|
| P0 | 图像 IO + ORB 提取 | 基础，无特征无法跟踪 |
| P0 | StereoInitialization | 首帧初始化，能建第一个 KF |
| P0 | PoseOptimization | 单帧位姿优化，跟踪必需 |
| P0 | TrackReferenceKeyFrame | 基本跟踪 |
| P1 | TrackWithMotionModel | 恒速跟踪（可先用 TrackReferenceKeyFrame 替代） |
| P1 | LocalMapping（简化） | 关键帧处理 + 地图点剔除 |
| P2 | LocalBundleAdjustment | 提升精度，可降级或省略 |
| P2 | YOLO + 动态 mask | 动态场景才需要 |
| P3 | TrackWithOpticalFlow（NGD） | 动态场景光流跟踪 |
| P3 | LoopClosing + 回环 | 大尺度场景需要 |
| P3 | IMU 集成 | 仅视觉惯性版需要 |
| P4 | Viewer | 嵌入式通常省略 |

### 6.9 数学库自实现要点

纯 C 无 Eigen，需手写核心运算：

```c
// 3x3 矩阵
typedef struct { float m[9]; } Mat3;  // 行主序

Mat3 mat3_identity();
Mat3 mat3_multiply(Mat3 a, Mat3 b);
Mat3 mat3_transpose(Mat3 a);
Mat3 mat3_inverse(Mat3 a);

// 3D 向量
typedef struct { float x, y, z; } Vec3;

Vec3 vec3_cross(Vec3 a, Vec3 b);
float vec3_dot(Vec3 a, Vec3 b);
float vec3_norm(Vec3 a);
Vec3 vec3_normalize(Vec3 a);

// 四元数（替代 SO3）
typedef struct { float w, x, y, z; } Quat;

Quat quat_from_axis_angle(Vec3 axis, float angle);
Quat quat_multiply(Quat a, Quat b);
Quat quat_conjugate(Quat q);
Mat3 quat_to_matrix(Quat q);
Quat quat_slerp(Quat a, Quat b, float t);

// SE3（替代 Sophus::SE3f）
typedef struct { Quat q; Vec3 t; } SE3;

SE3 se3_multiply(SE3 a, SE3 b);
SE3 se3_inverse(SE3 s);
Vec3 se3_transform_point(SE3 s, Vec3 p);  // p' = R*p + t
```

---

## 7. Windows 移植经验（已完成）

以下是基于已完成的 Windows（MSVC 2022）移植总结的注意事项，对纯 C 移植同样有参考价值。

### 7.1 跨平台崩溃根因

所有问题源于 **Linux 与 Windows 内存行为差异**：Linux 栈 8MB、堆布局宽松；Windows 栈 1MB、堆紧凑。

| 修复 | 问题 | 根因 |
|---|---|---|
| Frame grid 改堆分配 | `StereoInitialization()` 卡死 | 150KB 对象放栈，Windows 1MB 栈溢出 |
| `SearchByOpticalFlow` 越界保护 | 光流点崩溃 | LK 点越界，`Mat::at` 无边界检查 |
| `PredictCurrentMask` 越界保护 | 间歇性 0xC0000005 | 同上，对称路径 |
| `CreateMaskFromClusters` 尺寸守护 | mask/depth 尺寸不一致 | 用 mask 行列索引 depth |
| `ExtractDynaPoints` 空 mask 守护 | YOLO 超时致空 mask | 空 mask 越界 |
| `isLargeRotation` 修正 | mask 流转错误 | `std::abs(rollDeg > 10)` 笔误 |

### 7.2 Windows 移植清单

**编译/链接**：
- CMakeLists.txt 是 Linux 版（`.so`/`-lboost`/`-O3`），MSVC 需忽略 GCC 选项（`-O3`/`-march=native` 会被警告忽略，无害）
- ORB_SLAM3 静态链接进 exe，运行时不依赖 ORB_SLAM3.dll
- 运行时库统一 `/MT`（MultiThreaded）或 `/MD`（MultiThreadedDLL），避免 LNK4098 混用警告

**路径处理**：
- Windows 路径分隔符 `\`，需在 `rgbd_tum.cc` WIN32 分支把 `/` 替换为 `\\`
- 图像路径拼接用 `argv[3] + "\\" + filename`

**线程/同步**：
- `usleep` → `Sleep`（毫秒）
- `std::this_thread::sleep_for` 跨平台兼容
- YOLO 线程忙等待需加 `sleep_for(10ms)`，否则 CPU 100%

**DLL/库部署**：
- exe 旁需放置 OpenCV、Pangolin、boost 等第三方 DLL
- 静态链接 ORB_SLAM3 可避免 DLL 版本不匹配问题

### 7.3 对纯 C 移植的启示

1. **所有 `cv::Mat::at` 必须加边界检查**——这是 Windows 崩溃主因，嵌入式同样（甚至更严格，内存更小）
2. **大对象必须堆分配或用对象池**——栈空间在嵌入式更宝贵
3. **检查尺寸一致性**——不同来源的图像/mask/depth 可能尺寸不匹配
4. **NaN 检查**——光流/PnP 可能产生 NaN，`x != x` 是可靠检测
5. **超时保护**——多线程等待必须有超时，避免死锁
6. **初始化所有成员**——不要依赖默认构造，显式初始化

---

## 附录：构建与运行

### 构建（Windows / MSVC）
```bash
cd NGD-SLAM
mkdir myBuild2 && cd myBuild2
cmake -G "Visual Studio 17 2022" ..
cmake --build . --config Release --target rgbd_tum
```

### 运行
```
rgbd_tum.exe ..\..\..\Vocabulary\ORBvoc.txt ..\TUM3.yaml \
  <path_to_dataset> ..\associations\<assoc>.txt
```

### 构建环境
- 编译器：MSVC 14.44.35207（VS2022 Community）
- OpenCV：4.12.0
- 生成器：Visual Studio 17 2022
- 链接：ORB_SLAM3 静态链接进 rgbd_tum.exe（约 36MB）

---

**文档完。** 本文档基于实际源码分析（截至 2026-06-29），代码位置可能随版本变化，建议以实际源码为准。
