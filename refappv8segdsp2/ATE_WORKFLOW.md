# 端到端结果生产 + ATE 对比操作手册（本机可直接粘贴版）

适用：本机 Windows + Git Bash + MSVC，工程
`C:/Users/frank.tu/workspace/trae_study/slam-splat/NGD-SLAM/refappv8segdsp2`。
所有命令在 Git Bash 里逐块粘贴执行。**改一次配置必须重跑重比；重新配置会
覆盖 exe，先归档上一轮轨迹再重配。**

当前 Git Bash 里 `cmake` 直接可用（uv-2 venv 的 4.4.2）；若提示找不到，
先开 cmd 跑一次 `CMakePath.bat` 再回来。

## 1. 配置 A：纯C 基线（bit-exact 基准）

```bash
cd /c/Users/frank.tu/workspace/trae_study/slam-splat/NGD-SLAM/refappv8segdsp2/build
cmake -S .. -B . -DVP6_EXTRACT=OFF -DBRIEF_INT_VP6=OFF -DNGD_APP_VIEWER=OFF
cmake --build . --config Release --target rgbd_tum_app_v8segdsp2
```

## 2. 配置 C：VP6 驱动 + I1 定点 brief（DSP 目标形态）

```bash
cd /c/Users/frank.tu/workspace/trae_study/slam-splat/NGD-SLAM/refappv8segdsp2/build
cmake -S .. -B . -DVP6_EXTRACT=ON -DBRIEF_INT_VP6=ON -DNGD_APP_VIEWER=OFF
cmake --build . --config Release --target rgbd_tum_app_v8segdsp2
```

（B 隔离配置 = `-DVP6_EXTRACT=ON -DBRIEF_INT_VP6=OFF`，只在 C 异常需要
归因时才跑。）

## 3. 冒烟（每配置必做，~5 秒）

```bash
cd /c/Users/frank.tu/workspace/trae_study/slam-splat/NGD-SLAM/refappv8segdsp2/build
./Release/rgbd_tum_app_v8segdsp2.exe \
    /c/Users/frank.tu/workspace/trae_study/slam-splat/NGD-SLAM/Vocabulary/ORBvoc.txt \
    /c/Users/frank.tu/workspace/trae_study/slam-splat/NGD-SLAM/Examples/RGB-D/TUM3.yaml \
    /e/datasets/slam-data/rgbd_dataset_freiburg3_walking_xyz \
    /c/Users/frank.tu/workspace/trae_study/slam-splat/NGD-SLAM/Examples/RGB-D/associations/fr3_smoke30.txt \
    > smoke.log 2>&1
echo "exit=$?"
grep -E "images|tracked|sum " smoke.log
ls -la CameraTrajectory.txt
```

检查：`exit=0`、日志有 `30 images`、`CameraTrajectory.txt` ~3KB。
不满足就别跑全量，先查配置。

## 4. 全量跑 + 归档（A 配置版本；C 配置把 trajA_pureC 换成 trajC_I1）

```bash
cd /c/Users/frank.tu/workspace/trae_study/slam-splat/NGD-SLAM/refappv8segdsp2/build
./Release/rgbd_tum_app_v8segdsp2.exe \
    /c/Users/frank.tu/workspace/trae_study/slam-splat/NGD-SLAM/Vocabulary/ORBvoc.txt \
    /c/Users/frank.tu/workspace/trae_study/slam-splat/NGD-SLAM/Examples/RGB-D/TUM3.yaml \
    /e/datasets/slam-data/rgbd_dataset_freiburg3_walking_xyz \
    /e/datasets/slam-data/rgbd_dataset_freiburg3_walking_xyz/asso.txt \
    > walkA.log 2>&1
echo "exit=$?"
mv CameraTrajectory.txt trajA_pureC.txt
mv KeyFrameTrajectory.txt kfA.txt 2>/dev/null
mv PoseCovariance.txt covA.txt 2>/dev/null
grep -E "tracked|orb |lm |sum " walkA.log
wc -l trajA_pureC.txt
```

C 配置跑全量前，先按第 2 节重配重建，然后整块重跑，把三处 `A` 换 `C`
（`walkC.log`、`trajC_I1.txt`、`kfC_I1.txt`/`covC.txt`）。
说明：输出文件写在当前目录（build/）；轨迹必须立刻改名，否则换配置后被
覆盖；日志尾 stage 表用来归因（KF/ORB 帧数变化 = 描述子扰动改变编排）。

## 5. ATE 计算

```bash
cd /c/Users/frank.tu/workspace/trae_study/slam-splat/NGD-SLAM/refapp
python ate.py \
    /c/Users/frank.tu/workspace/trae_study/slam-splat/NGD-SLAM/refappv8segdsp2/build/trajA_pureC.txt \
    /e/datasets/slam-data/rgbd_dataset_freiburg3_walking_xyz/groundtruth.txt

python ate.py \
    /c/Users/frank.tu/workspace/trae_study/slam-splat/NGD-SLAM/refappv8segdsp2/build/trajC_I1.txt \
    /e/datasets/slam-data/rgbd_dataset_freiburg3_walking_xyz/groundtruth.txt
```

输出 `ATE RMSE/mean/median/std/max`（cm）+ `scale`（Umeyama 尺度，应接近
1）。要算 NEES 时追加第 3 参数 cov 文件：
`python ate.py trajA_pureC.txt groundtruth.txt covA.txt`。

## 6. 判读

- 主看 **RMSE**；两配置 tracked 帧数应相同（本数据集 827/4135，phase3 OF
  模式属正常），KF/ORB 帧数略差（98/224 vs 93/196）正常。
- 参考基线（2026-09-18 实测，walking_xyz 4135 帧）：

| 配置 | ATE RMSE | mean | median | tracked |
|---|---|---|---|---|
| A 纯C | 12.65 cm | 8.31 | 5.94 | 827/4135 |
| C VP6+I1 | 12.52 cm | 6.69 | 3.57 | 827/4135 |

- 差异 ±0.5cm 内视为近似影响在噪声内；C 大幅劣化先跑 B 归因
  （B 正常→I1 问题；B 也差→VP6 驱动问题）。

## 7. 附：golden 抓取（同一 app，环境变量开关）

C 配置（I1 语义 golden，抓前 8 次提取）：

```bash
cd /c/Users/frank.tu/workspace/trae_study/slam-splat/NGD-SLAM/refappv8segdsp2/build
NGD_DUMP_ORB_GOLDEN="C:/Users/frank.tu/workspace/trae_study/slam-splat/NGD-SLAM/refappv8segdsp2/dspcalling/golden" \
./Release/rgbd_tum_app_v8segdsp2.exe \
    /c/Users/frank.tu/workspace/trae_study/slam-splat/NGD-SLAM/Vocabulary/ORBvoc.txt \
    /c/Users/frank.tu/workspace/trae_study/slam-splat/NGD-SLAM/Examples/RGB-D/TUM3.yaml \
    /e/datasets/slam-data/rgbd_dataset_freiburg3_walking_xyz \
    /c/Users/frank.tu/workspace/trae_study/slam-splat/NGD-SLAM/Examples/RGB-D/associations/fr3_smoke30.txt \
    > dumpgolden_i1.log 2>&1
ls /c/Users/frank.tu/workspace/trae_study/slam-splat/NGD-SLAM/refappv8segdsp2/dspcalling/golden/f*.kps | wc -l   # 应为 8
```

纯C 参考集：先按第 1 节切 A 配置，同样命令重跑，环境变量目录换成
`.../dspcalling/golden/prev_pureC`（先 `mkdir -p`）。详见
`dspcalling/golden/README.md`。

## 8. 踩坑记录（都实际踩过）

1. **相对路径层级**：从 `build/` 跑时 `../../../` 指 slam-splat（少一级）
   ——本文全部用绝对路径，别改回相对的。
2. 后台任务的 `/tmp` 与前台映射可能不一致，产物落到任务 cwd（如 build/）；
   找不到就按文件名 find。
3. Windows 下 `cmd /c "exe > log"` 会开交互 shell 不执行——bash 直接重定向。
4. bash 命令里别放中文注释（编码问题）。
5. 前台 `sleep` 被 harness 禁——长任务用后台任务等通知。
6. `fr3_walk_half.txt` 与本数据集时间戳不匹配（0 帧）——全量只用数据集
   自带的 `asso.txt`。
