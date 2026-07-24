# refoptvp6 - VP6 (Cadence DSP) tile+idma 优化 of refactored SLAM kernels

参照 `vp6ref/vp6/`（IPM 的 tile 切分 + iDMA 搬运范例），把 `refactor/` 已重构的纯 C
SLAM 核逐步优化到 Cadence VP6（P6 TileManager / `xvTile` API），先在本机 cstub 仿真，
要求与 refactor C 逐位一致，再逐步优化耗时部分。YOLO 与光流未重构，仍走原版 OpenCV，
不在本目录范围内。

## 一致性策略（每阶段通用）

- **可移植核逐位一致**：每阶段抽出一个 `_core.c`，float 数学 1:1 复制自 `refactor/`，
  作为 PC 参考与 VP6 共用的核。VP6 tile 驱动调同一个核 -> 与 refactor 逐位一致。
- **IVP 快速路可选**：每阶段预留 `*_USE_IVP_INTRIN` 宏 hook（`extern *_kernel_ivp`），
  默认走可移植核（xcc 可自动向量化）；手写 IVP 快速路单独验证（能逐位则逐位，数值
  不同处用容差并标注），对标 `vp6ref` 的 `IPM_USE_IVP_INTRIN`。

## 当前状态

| 阶段 | 核 | VP6 驱动 | cstub 测试 | 逐位一致 |
|------|----|----------|------------|----------|
| ORB 高斯模糊 | `src/orb_blur_core.c` ✓ | `src/orb_blur_vp6.c` ✓ | `test/test_orb_blur_vp6.c` ✓ | **✓ max\|diff\|=0** |
| ORB FAST-9 检测+NMS | `src/fast9_core.c` ✓ | `src/fast9_vp6.c` ✓ | `test/test_fast9_vp6.c` ✓ | **✓ 3-way 逐位一致** |
| ORB IC 角度 | `src/ic_angle_core.c` ✓ | `src/ic_angle_vp6.c` ✓ | `test/test_ic_angle_vp6.c` ✓ | **✓ 3-way float 严格相等** |
| ORB 旋转 BRIEF | `src/brief_core.c` ✓ | `src/brief_vp6.c` ✓ | `test/test_brief_vp6.c` ✓ | **✓ 3-way 32 字节逐位一致** |
| 描述子匹配 | `src/match_core.c` ✓ | `src/match_vp6.c` ✓ | `test/test_match_vp6.c` ✓ | **✓ Hamming + 3-way best1/idx1/best2** |

- **orb_blur**：8 case（内部/单 tile/非整倍边界/小帧 × 梯度/随机/常数）全 `max|diff|=0`，与
  `refactor/src/orb.c::ngd_gaussian_blur` 逐位一致。
- **fast9**：13 case（全图/内缩 ROI、阈值 7/20/40、strip 高 16/32/64、小帧、随机角点丰富图、
  cap=50/5 命中）三方比对（verbatim refactor ref vs `fast9_full` vs `fast9_vp6`）全逐位一致——
  关键点数、**顺序**(全局 raster)、每个字段(x,y,response,octave,angle,size)、cap 截断全等。
- **ic_angle**：8 case（梯度/随机/checker × 多尺寸、关键点 0/1/20~200 含内部+近边、小帧 30×30）
  三方比对（verbatim `ngd_ic_angle` ref vs `ic_angle_full` vs `ic_angle_vp6`）全 **float 严格相等**
  --umax 表 `15 15 15 15 14 14 14 13 13 12 11 10 9 8 6 3` 与 ORB 标准一致。
- **brief**：8 case（梯度/随机/checker × 多尺寸、关键点 0/1/20~200 含内部+近边、随机角度
  [0,360) + 固定 0/45/90/180）三方比对（verbatim `ngd_compute_descriptor` ref vs `brief_full`
  vs `brief_vp6`）全 **32 字节/关键点逐位一致**。`brief_bit_pattern_31` 已 diff 验证与
  `refactor/src/orb.c::ngd_bit_pattern_31` 完全相同（词表兼容）。
- **match**：Hamming 1000 随机对 `mism=0`（`match_hamming` == `ngd_descriptor_distance`）；
  best-match 10 组 Q/N（含 0/1/edge）三方比对（verbatim `search_by_bow_core` 内层循环 ref vs
  `match_best_kernel` vs `match_vp6`）全 `kernel_mism=0 vp6_mism=0`——best1/idx1/best2 每 query 全等。

### FAST9 用 strip 而非方块子 tile（关键设计）
FAST9 输出是**稀疏关键点列表**（非稠密图），发射顺序对 cap 截断有影响。方块子 tile 按块发射
→ 顺序 ≠ refactor 的全局 raster → cap 命中时结果不一致。故改用**水平 strip**（全 ROI 宽 ×
`strip_h` 高）：每 strip 跨全宽，kernel 逐行发射 = 该 strip 的全局 raster；strip 自上而下拼接
= 全局 raster，与 `ngd_fast_detect` 逐位一致（含 cap）。±4 垂直 margin 覆盖 ring(3)+NMS(1) 跨
strip 边界。无 DMA-out（稀疏输出直接写 DRAM `out`）。`max_src_w` 需 ≥ ROI 宽+8（调用方按 ROI 设）。

### IC 角度用 per-keypoint patch DMA（非 strip）
IC 角度是**每关键点 gather**（半径 15 圆盘，via umax），与 blur(稠密)/FAST(稠密扫描) 不同。VP6
单位 = 每关键点一个 31×31 patch tile：DMA 该关键点 ±15 patch(裁到帧) 进 SRAM，`ic_angle_kernel`
算 `atan2f(m_01,m_10)*180/π`，按索引写 `angles[i]`（保关键点顺序）。ping/pong 双 patch tile 重叠。
`ic_pix_clamp` 按帧坐标裁到帧边(同 `ngd_pix_clamp`，非 reflect-101)，裁后坐标必在裁后 patch 内 ->
EDGE=0、逐位一致。无 DMA-out（每关键点一个 float）。xtensa `ORBAngleBRIEF.c` 用 IVP gather 读圆盘
像素，是 DMA-patch 方式的 IVP 快速路参考。

### 旋转 BRIEF 同为 per-keypoint patch DMA
rBRIEF 与 IC 角度同属每关键点 gather，但读 256 个旋转后的 patch 采样点（`bit_pattern_31` 坐标
max 13 -> 旋转后偏移 ≤ 13√2≈18.4 -> lroundf 18），故 patch = ±19（`BRIEF_MARGIN`）。驱动结构与
IC 角度一致：每关键点 DMA ±19 patch(裁帧) 进 SRAM(ping/pong)，`brief_kernel` 用 `cosf/sinf`+
`lroundf` 旋转 256 对、`brief_pix_clamp` 采样、`(t0<t1)` 打包 32 字节，按索引写 `desc[i*32]`。
xtensa `ORBAngleBRIEF.c` 的 sincos LUT + IVP gather 是 IVP 快速路参考（便携核用 cosf/sinf，可自动
向量化）。xtensa `ORBAngleBRIEF.c` 的 sincos LUT + IVP gather 是 IVP 快速路参考（便携核用 cosf/sinf，可自动
向量化）。

### 描述子匹配用批量（query 驻 SRAM，train 切片）
匹配是所有 matcher（SearchByBoW/Projection/Triangulation/Fuse/Sim3）共用的内层循环：对每 query 扫
train 描述子算 Hamming、找 best1/best2/idx1（1:1 自 `search_by_bow_core` cc:269-302）。VP6 批量：Q 个
query 描述子 DMA 进 SRAM（qtile，32×Q，一次），N 个 train 描述子竖切（32×B ping/pong，每个 train 只
DMA 一次），每 train tile 对全 Q query 跑 `match_tile_kernel`（Hamming + 更新 best1/best2/idx1，写 DRAM）。
`match_hamming` 1:1 复制 `ngd_descriptor_distance`（8×uint32 bit-parallel popcount）。输出 3 int/query
（无 DMA-out）。matcher 集成时把 BoW/几何过滤后的候选打包成 dense train 数组再调用。xtensa
`ORBforwardKeypointMatching_U16` 用 `IVP_POPC2NX8` + min-reduction，是 IVP 快速路参考（便携核 popcount
可自动向量化）。

### 踩坑
- **变量遮蔽**：驱动循环变量曾用 `t`，遮蔽了阈值参数 `t`，导致传 tile 索引当阈值（tile 0 阈值
  变 0，角点暴增）。已改循环变量为 `ti`/`j`。blur 无此问题（kernel 无阈值参数）。

## 构建（PC / cstub，32 位 C++）

cstub 仅 32 位、TIE stub 是 C++，故全量 `.c` 当 CXX 编译、`-A Win32`（与 `vp6ref/vp6`
一致）。本机 cmake 不在默认 PATH，先 sourcing `C:\Users\frank.tu\CMakePath.bat`（指向
cmake 3.31.10）或直接用全路径 cmake：

```
CMAKE=C:/src/vcpkg/downloads/tools/cmake-3.31.10-windows/cmake-3.31.10-windows-x86_64/bin/cmake.exe
"$CMAKE" -S refoptvp6 -B refoptvp6/build -G "Visual Studio 17 2022" -A Win32
"$CMAKE" --build refoptvp6/build --config Release
refoptvp6\build\Release\test_orb_blur_vp6.exe
```

`CSTUB_PATH` 默认 `C:/usr/XtDevTools/install/builds/RI-2022.10-win32/fy01_202210/src/cstub`；
TileManager 取自 `../sgbmwarpFace/WarpRGBA/TileManager_P6/TileManager`。cstub 首次编译
约 10+ 分钟（40+ 个 TIE stub .cpp）；后续增量很快。

## 阶段模板（新增阶段照此推进）

1. `src/<name>_core.c/h`：可移植核，float 数学 1:1 复制自 refactor 对应函数；提供
   `<name>_kernel`（tile 感知，origin + frame_w/h 做边界）、`<name>_tile_source_bbox`、
   `<name>_full`（PC 全图参考）。
2. `src/<name>_vp6.c/h`：tile 驱动，ping/pong DMA 重叠，镜像 `orb_blur_vp6.c` /
   `vp6ref/vp6/ipm_vp6.c`；预留 `*_USE_IVP_INTRIN` hook。
3. `test/test_<name>_vp6.c`：cstub 端到端比对 VP6 tile 路径 vs `<name>_full`，期望
   `max|diff|=0`，覆盖内部/边界/小帧/多种输入。
4. 在 `CMakeLists.txt` 把新 `.c` 加进 `orb_blur_core` 库（或新建库）、加新 test exe。

## 关键正确性点（复用自 vp6ref DEVELOPMENT.md 踩坑）

- **tile 索引 ≠ 帧像素坐标**：`tu0=(t%n_tiles_x)*TW; tv0=(t/n_tiles_x)*TH`。
- **边界 tile 写回按 stride**：`XV_TILE_UPDATE_DIMENSIONS(out,tu,tv,tw,th,TW)` 后 DMA-out，
  不要 `memset(buf,0,tw*th)`。
- cstub 仅 32 位（`-A Win32`）；所有 `.c` 当 C++ 编译（`LANGUAGE CXX`）。
- MSVC C++：复合字面量改临时变量；`XV_TILE_GET_DATA_PTR` 返回 `void*` 须显式转
  `(const uint8_t*)`/`(uint8_t*)`。
- `idma_desc_done` 是假完成标志但 `add_and_schedule2d` 同步拷贝，数据就位。

## 参考代码

- `vp6ref/vp6/`：tile+DMA 范例（`ipm_vp6.c`/`test_ipm_vp6.c`/`CMakeLists.txt`）。
- `C:/usr/xtensa/Xplorer-9.0.20-workspaces/slam2p5/xsTileLib/slam/src`：Cadence 官方 VP6
  IVP 向量化 SLAM 核，**可参考算法/内联函数/LUT 构造**，但注意：
  - 用 `xsTileLib`（`xs_core`/`xs_slam`/`xs_pTile`）抽象，非本目录的 `xvTile`/`tileManager.h`；
  - 数值约定不同（Q15 角度 + LUT，refactor 用 float + lroundf），直接抄不逐位一致 ->
    作 IVP 快速路参考，可移植核仍以 refactor float 为准。
  - `ORBAngleBRIEF.c` = 角度+BRIEF；`ORBforward/reverseKeypointMatching_U16.c` = 汉明匹配；
    `fast9NMS.c`/`nonmaximasuppression3.c`/`adjustExtremaInterpolate.c` = FAST9；
    另有 BA/pose/triangulation/searchByProjection 等供后续非 ORB 阶段参考。

## 后续阶段（未做）

非 ORB 模块：几何 matcher 的集成（把 BoW/投影过滤后的候选喂给 `match_vp6`）、BA/pose/triangulation
（参考 xtensa `SolveForPoseWorldPoints_BA_*`/`SparseBundleAdjustment_*`/`TriangulateNewMapPoints`/
`estimatePose`/`poseoptimization` 等）。真实 DSP（xcc/`__XTENSA__`）编译留待 DSP 环境。
