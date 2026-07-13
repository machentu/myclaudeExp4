# refappthread — 多线程效率测试结果

4 线程驱动（Tracking 主线程 + LocalMapping + LoopClosing + Viewer），对标原版 NGD-SLAM
`src/System.cc:200-256`。粗粒度全局 `map_mutex` 守护 map/kfdb/KF/MP（纯 C 核心无自带线程安全）。
核心 `ngd_core.lib` / 薄壳 `ngd_shell.lib` 未改动，仅应用层加线程。

## 构建 / 运行

```
c:\Users\frank.tu\CMakePath.bat
cmake -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=C:/src/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
# 需 vcpkg OpenCV DLL（从 refapp\build\Release 拷 22 个 dll 到 refappthread\build\Release）
```
运行（从任意目录，yolo_dir 用绝对路径）：
```
set NGD_VIEWER=0 && set NGD_PHASE=3
rgbd_tum_app_thread.exe <ORBvoc.bin> <TUM3.yaml> <dataset> <assoc> <Thirdparty\YOLO>
```

## 效率对比（TUM3 fr3_walk_xyz，827 帧，phase3 OF+MLPnP 默认配置，YOLO on）

| 指标 | 单线程 refapp | 4 线程 refappthread |
|---|---|---|
| mean ttrack | 49.4 ms | 48.0 ms |
| median ttrack | 31.7 ms | 31.4 ms |
| total wall-clock (30fps 节流) | ~56 s | 58.3 s |
| LBA 位置 | 内联（建 KF 帧） | 后台 LM 线程 |
| LBA 总耗时 | 1817 ms (内联) | 2172 ms (异步) |
| 最大 LM 队列深度 | — | 1（LM 跟得上） |
| KFs | 48 | 48 |
| ATE vs 单线程 | — | **0.00 cm（轨迹逐位相同）** |
| 路径长 | 10.393 m | 10.393 m |
| extent | x[-0.59..0.38] y[-0.50..0.67] z[-0.22..0.62] | 同左（= 基准） |

## 结论：多线程收益极小（~1.4 ms/帧，~3%）

- **LBA 仅占每帧 4.5%**（2.2 ms/帧均摊）。把它挪到后台线程只省下这点。
- **主线程真正的大头是 YOLO(33%) + ORB(37%) + mask(21%) = 91%**，三者本就在主线程锁外跑，
  无法与 LM 重叠。多线程 LM/LC 动不了它们。
- 0.8 ms 的 LBA 节省被 map_mutex 争用抵消（主线程在 LM 跑 LBA 时等锁）。
- 30fps 节流进一步掩盖了每帧收益——OF 帧（93%）本就 sleep 等 33 ms 周期。

**正确性达标**：827/827 全跟踪、0 崩溃、0 数据竞争、轨迹与单线程逐位一致（粗锁串行化保证
计算顺序不变；shutdown 排空 LM/LC 队列确保所有 LBA/回环纠正落盘后再存轨迹）。

## 下一步杠杆（若要真提速）

1. **YOLO 独立线程**（原版第 5 线程，33% 工作量）——最大头，但用户要求 4 线程故未做。
2. **OF 路径内拆锁**：MP worldPos/描述子在锁内快照，LK+PnP 锁外跑——当前已基本如此
   （of 仅 2.4 ms/帧，杠杆小）。
3. **ORB 提取器 SIMD 化 / 降 nFeatures**（37%，纯 C 无 SIMD）——与多线程正交，单线程也能做。

## 文件清单（仅改应用层）

- `include/ngd_app/system.h`、`src/system.cpp`：4 线程 + map_mutex + LM/LC 队列 + viewer 快照。
- `include/ngd_app/viewer.h`、`src/viewer.cpp`：snapshot 机制（Tracking 持锁建快照，Viewer 线程锁外画）。
- `src/main.cpp`：起/停线程 + wall-clock 计时。
- `CMakeLists.txt`：`ngd_app_thread` / `rgbd_tum_app_thread.exe`。
- 原样拷贝：`imio`、`yaml`、`optflow_track`、`tests`。
