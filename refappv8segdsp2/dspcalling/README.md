# dspcalling — 真实 VDSP RPC 调用封装

把 `vdsp_test_ylm`（`C:\Users\frank.tu\workspace\vdsp_test_ylm`，FH 样例树）里
**真实调 DSP** 的那套代码整理成本工程可复用的形态。规范调用方式以
`vdsp_persp_trans_trapezoid_test_ext` 中的 `FH_MPI_SVP_DSP_RPC_Ext`（通用命令
结构体路线）为准，typed API（`FH_MPI_SVP_DSP_PerspTrans` 等）不在本封装内。
**DSP 那一侧怎么实现不在关心范围内。**

## 文件

| 文件 | 内容 |
|---|---|
| `dspcalling.h/.c` | 封装：init/exit、MMZ buffer、RPC_Ext、计时 |
| `dspcalling_example.c` | 可运行模板 = 样例 case 6（trapezoid ext）的重写 |
| `dspcalling_orb.h/.c` | ORB 特征提取的 RPC 封装（命令结构体 + 持久 buffer + 每帧一次 RPC） |
| `dspcalling_orb_example.c` | ORB 调用示例：灰度图进 → 关键点/描述子出，落盘可对拍 |
| `golden/` | 真机对拍黄金数据（8 组，端到端实跑抓取；格式与用法见 golden/README.md） |

## 调用模式（四步）

1. **装载固件** `dspc_init(sram_bin_path)` —— 顺序照抄 `vdsp_test_init`：
   `DisableCore(0)` → `PowerOff(0)` → `LoadBin(bin, SVP_DSP_MEM_TYPE_SYS_DDR_DSP_0)`
   → `PowerOn(0)` → `EnableCore(0)`。
2. **MMZ 内存** `dspc_buf_alloc(&buf, name, size)` —— 包 `FH_SYS_VmmAlloc`，
   一个分配同时拿 `phy`（命令结构体里引用给 DSP）和 `vir`（CPU 读写用），
   两者指向同一块字节。
3. **发命令** —— CPU 通过 `cmd.vir` 填命令结构体（`TRANS_TRAPEZOID_S` 形态：
   图像描述符 + 操作参数，**内部引用全部用物理地址**），然后
   `dspc_rpc_ext(cmd_id, &cmd, -1)` 阻塞等完成（内部即
   `SVP_DSP_REQ_PARAMS_S{u32CmdId, u32ParamPhyAddr, u32ParamLen, u32TimeoutMs}`
   + `FH_MPI_SVP_DSP_RPC_Ext(&h, &req, 0)`）。
4. **收结果** —— DSP 把输出写进命令结构体引用的 MMZ buffer，CPU 从对应
   `vir` 读；`dspc_buf_free` 释放，`dspc_exit` 关核。

`dspc_pts_us()` 包 `FH_SYS_GetCurPts`，用于测单次 RPC 往返耗时（样例同款用法）。

## 编译（仅 Linux；按需，暂无 Makefile）

依赖 FH SDK 的 `fh_vdsp_mpi.h`、`fh_system_mpi.h` 头和 `libmpi`（Windows 侧
无此环境，**本目录不进 CMakeLists，不在 Windows 编译**）。SDK 就位后手工编：

```
$(CC) dspcalling.c dspcalling_example.c -o dspcalling_example \
    -I<SDK头文件路径> -L<SDK库路径> -lmpi -lpthread -lm
```

运行（数据文件在 vdsp_test_ylm 下）：

```
./dspcalling_example -s trapezoid/sram-83b0-1.6.2.5-tile16-h.bin \
                     -r 864 480 -i trapezoid/in_864x480_nv12.yuv
```

## ORB 调用（dspcalling_orb.h/.c）

已按同一模式实现，接口对齐 `dsp_orb_vp6_extract` 的语义：

```c
dspc_orb_t orb;
dspc_orb_alloc(&orb, max_w, max_h, max_kps);   /* 一次分配 4 块 MMZ：
                                                  灰度图/kps/desc/命令块 */
dspc_orb_extract(&orb, gray, w, h,             /* 每帧一次（阻塞）：
                                                  拷图→填命令→RPC→拷出 */
                 1000, 1.2f, 8, 20, 7,         /* ngd_orb_init 同款参数 */
                 kps, desc, &n);
dspc_orb_free(&orb);
```

命令结构体 `DSPC_ORB_CMD_S`：输入图几何（宽高/stride/灰度 PHY 地址）、
5 个标量参数、输出容量 + kps/desc PHY 地址 + 返回的 `u32NKps`。
关键点线格式 `DSPC_ORB_KEYPOINT_S` 与 refactor 的 `ngd_keypoint` 逐字段
一致（6×4B，两侧均 32 位小端）。`DSPC_ORB_CMD_ID` 默认 0（样例固件
u32CmdId 全 0，bin 决定算子）；与 DSP 侧派发表一起改。

示例 `dspcalling_orb_example.c` 编译/运行：

```
$(CC) dspcalling.c dspcalling_orb.c dspcalling_orb_example.c \
      -o dspcalling_orb_example -I<SDK头> -L<SDK库> -lmpi -lpthread -lm
./dspcalling_orb_example -s orb_sram.bin -r 640 480 -i frame.bin -o orb_out
```

输出 `orb_out.kps`（4B 数量 + n×keypoint）和 `orb_out.desc`（n×32B），
可与 Windows 侧纯C / cstub 路径的输出离线对拍。
