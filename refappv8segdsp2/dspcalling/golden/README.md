# golden — 真机 DSP ORB 对拍的黄金数据（8 组，I1 定点 brief 语义）

来源：**端到端实跑抓取**（2026-09-18）。`rgbd_tum_app_v8segdsp2` 跑
freiburg3_walking_xyz（smoke30 关联文件，前 30 帧里 ORB 分支的前 8 次提取
调用），`VP6_EXTRACT + BRIEF_INT_VP6` 路径 —— **brief 就是 I1 定点计算
方式**（Q7 sin/cos LUT + 1° 分桶），即 DSP 固件的目标行为。输入为 app
送给提取器的原始灰度图（未经 mask）。

- 重新生成：设 `NGD_DUMP_ORB_GOLDEN=<目录>`、按上面配置构建并跑 app
  （钩子在 system.cpp 提取调用后、mask 过滤前，取前 8 次）。
- 提取参数（TUM3.yaml，与 `dspcalling_orb_example` 一致）：
  `nfeatures=1000, scaleFactor=1.2, nLevels=8, iniThFAST=20, minThFAST=7`。
- 图像：640×480 紧凑灰度（stride=width）。

## 每组三个文件（f000…f007）

| 文件 | 格式 |
|---|---|
| `fNNN_gray.bin` | 307200 B，输入灰度图，直接喂 `-i` |
| `fNNN.kps` | 4B 小端 int32 数量 n + n×24B 关键点；关键点 = 6×4B：`x, y, angle, response, octave(int), size`（float，`DSPC_ORB_KEYPOINT_S` / `ngd_keypoint` 布局） |
| `fNNN.desc` | n×32B 描述子（**I1 语义**），第 i 行 = 第 i 个关键点 |

各组 n：1011 / 1008 / 1007 / 1007 / 1008 / 1006 / 1006 / 1010。

## prev_pureC/ — 纯C bit-exact 参考集（同名三件套）

同 8 帧的纯C `dsp_orb_extract` 输出。与 I1 版的实测关系：**kps 逐字节
一致**；desc 平均差 3.3 bit/kp（256 bit 的 1.3%），设计内近似。

## 真机对拍用法

```
./dspcalling_orb_example -s orb_sram.bin -r 640 480 -i golden/f000_gray.bin -o /tmp/t0
cmp /tmp/t0.kps  golden/f000.kps    # 关键点：必须逐字节一致
cmp /tmp/t0.desc golden/f000.desc   # 描述子（I1 固件）：必须逐字节一致
```

判定：
- **DSP 固件 = I1 定点 brief（目标形态）** → 两组 cmp 都必须干净。
- 若 desc 不一致：先和 `prev_pureC/fNNN.desc` 对照——如果差异模式与
  「I1 vs 纯C ≈ 3.3 bit/kp」同量级，是语义错位（固件编了 R6 路径）；
  如果远大于此（整行翻转/错位），是移植 bug（查 offset/gather/乱序表）。
- kps 任何不一致都是移植 bug（brief 之前的管线必须 bit-exact）。
