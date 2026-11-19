# TyTVM v1.0.41 后段链路分析(.asm → net_bin → ISS/硬件)

> 探查代理报告,2026-11-18。`$D = $SRC/deepeye/python/deepeye`;`$SRC = /DEngine/projects/TyTVM_NNP3xx_src_v1.0.41/tytvm_src_pkg`

## 0. 产物链总览

```
relay 模型
  └─ make.py:44 make_netbin() → build_relay() (codegen, 产出 .asm 文本)
       └─ net_bin.py:183 net_bin() ──→ nnp3_net_bin.py:39 net_bin()
             │  对每个 NNP block:
             │   net_bin_helper.py:276 nnp_hex_bin()
             │     └─ lib.export_library(..., linker) → linker/linker.py:16 linker()
             │           ├─ lto.py:4249 lto()            # asm 整理/权重 hex/DDR 规划
             │           ├─ ass(...)                     # .asm → cpp → .lst + .hex
             │           ├─ model_hex_l2ld.py:1503       # PM 容量不足时二级动态装载切分
             │           └─ linker.py:173 link_info_gen() # 代码长度/中缓大小/权重长度
             └─ nnp3_vu/nnp3_vu_block.py 打包 block/layer/tensor + code_bin + weight_bin
       → 产物: net.bin / model.bin / libarm.so / net_combine.bin / nnp_hex_info_0xb0000000.bin / model_profile.json
运行:
  A) 工具链内 ISS:  golden.py:386 run_on_nnp() → iss_nnp3xx/iss.py:682 run_iss()
  B) SDK 模拟器:    run_net_bin/run_net_bin.py:356 run_net_bin()(内网下载 SDK,qemu-arm + libnnp3XX_iss.so)
  C) 真硬件:        run_net_bin/run_on_hardware.py(tvm.rpc + dcl.desdk DeSDKModule)
  D) RTL/UVM 验证:  iss_scr_gen.py:641 case_sv_gen() 生成 UVM testcase(同一套 hex/nnp.models 装 RTL TB)
```

## 1. 汇编器:.asm → (cpp) → .lst/.hex

- `$D/ass_nnp3xx/ass.py`(5627 行, PLY lex/yacc);每芯片薄封装 `nnp316/317/318/318p/310/3020/315m_ass.py`
- 调用:`ass(['nnp_main.asm', 'lib.asm', '-cpp', '-nnp318', '-hex_size_en'])`;`-cpp` 走 GNU 预处理展开 `#include "nnp3xx_main.h"`
- **输出格式**:`.lst` = 汇编清单(每行 64hex 编码 + PC + 源行号注释,ISS/RTL 的 ppm 反汇编来源);`.hex` 每行 `%.64x :%.4x` = **256bit VLIW 指令包(8×32bit 槽) + PC**;nnp317 为 64bit 行,nnp305/310/318/318p 权重 128bit 行
- 指令编码表 `nnp3xx_inst_table.py`:`['add', 4, 8, ['issue_type',31,31,1], ...]` = [助记符, opcode, 字数, 字段位域...]
- `.hex → 二进制`:hex2bin.py:8(struct.pack 小端)

## 2. linker 实际做什么

`linker/linker.py:16 linker()`(由 lib.export_library(..., linker=...) 触发):

### 2.1 lto.py:4249 lto()
1. 解析 codegen 的 .asm,按图节点重组、重命名标签
2. `generate_pe_nnp300()`(lto.py:2397)把权重/量化因子转 256bit 行 hex 写 **`nnp.models`**;支持 weight_compress
3. **DDR 内存规划器 `ddr_manage.py:59 ddr_manage()`**:每个 bb(算子)的输出 tensor 在 DDR 中分配偏移(lifetime 感知贪心复用)→ `model_mem_info.json` → link_info 的 middle_buf_size → net.bin block 的 innerbuf
4. **地址回填**:codegen 阶段 DMA 地址是占位符,linker 用 mem_info 回填(update_param_addr :338 / update_eidma_addr :879 / update_eodma_addr :955)

### 2.2 装配与汇编
- `asm_work/` = `lib_asm_<target>/`(芯片固件库)+ `model_asm/*` 合并工作区
- `ass(...)` → `nnp_main.hex`;NNP_CYCLE_CNT_EN>0 时换周期计数版固件;NNP_NUM==4 时 4 核各自汇编再拼接

### 2.3 model_hex_l2ld.py:1503 —— PM 二级动态装载
NPU 程序存储器 PM 小(nnp317/318/318p 只有 **1024 行**):模型超 PM 时,`bb_ctrl_asm_gen` 生成驱动循环,固件运行时用 **PDMA** 把后续代码块从 DDR 搬进 PM,每搬一块 `bl` 跳进去执行。单 bb 超限时按 `## asm cutpoint` 切 tile、展开循环再切;多 bb 贪心合并进 bb_gp 段。

### 2.4 link_info(文本)
`nnp_main_hex_len`(固件段行数)、`model_hex_len`(模型段)、`middle_buf_size`、`model_param_size`、`nnp_cyc_cnt_buf_size`。net_bin 打包用它算 code 配置长度和 innerbuf。

## 3. net.bin 二进制结构

```
NET HEADER (384B, nnp3_vu_block.py:27):
  [0:128] model_name | [128:152] 时间串 | [152:156] net_header_len=384(I)
  [156:158] block_header_len=48(H) | [158:160] layer_header_len=64(H)
  [160:224] magic: "intellif-<TARGET>-ISA"
  [224:312] '@18ILL'(88B,含对齐): block_num, net_bin_size, ddr_size(33bit),
            net_weight_size, tvm_ver×2, arm_so_size, model_inputs_num,
            rsv×4, model_info_offset, internal_ver[3], lst_file_size, weight_reorder_file_size
BLOCKS (384 起): 每块头 48B('4H4IH'+保留): blockId, layerNum,
  type(0=NNP,1=CRGH+NNP,2=ARM,3=CRGH,4=MODEL_OUT), blockSize, innerbuf_size, innerbuf DDR 地址(33bit)
  每层头 64B('H4B3H9I'): tensor 数, 权重地址(model.bin 内偏移), 代码地址, 融合算子数...
  tensor_in/out 每个 48B: type(0=图像/1=数据), shape, dtype, len, daddr(DDR 偏移 33bit)
  (NNP block) code_len/cfg_len 对 + code_bin(模型指令流二进制, 16B 对齐)
  (ARM block) func_name + modelWeightbin(内嵌权重)
尾部: model_info JSON(compile_info/输入输出张量描述)
combine 模式: [net.bin][model.bin][libarm.so][lst_info][weight_reorder_info] → net_combine.bin
```

要点:权重不在 net.bin 里(layer 头只存 model.bin 内偏移);图级 tensor 缓冲规划 `tensor_resource_alloc`($D/nnp3_vu/net_bin_helper.py)按 128B 对齐复用中间 tensor;nnp318p 的 WEIGHT_TRANSPOSE 会附 weight_reorder_file_bin。

## 4. 运行链路(ISS 架构)

### 4.1 工具链内 ISS(主要验证路径)
`golden.py:386 run_on_nnp()`:
1. build_relay → `nnp_func_names(graph)` → `weight_second_handle` → `lib.imported_modules[0].export_library("nnp_test.o", linker, params=params, graph=graph, outdir=outdir, ...)` — ★npu_pipeline s8 复用此模式
2. `iss.py:682 run_iss()` = run_iss_pre(:1263) + run_iss_simu(:690):
   - run_iss_pre:读 model_mem_info.json/link_info → `gen_all_queue()` 生成 **iss_nnp_queue.cfg**(DDR 任务队列描述符:每模型 15×4 项,fmap/权重/代码/mid 地址)+ din/dout/mid_addr.cfg;输入数据转 hex(nnp.models.ci);生成 **iss_scr 命令脚本**
   - run_iss_simu:执行 `$D/iss_nnp3xx/nnp318p_iss iss_scr > log_iss`
3. iss_scr 内容(= 硬件怎么被驱动):
```
load ddr    asm_work/nnp_main.hex 0x1a8000000    # 代码→DDR
load cm/dm  vu_base.cm.init.dat / dm.init.dat    # 片上存储初值
load ddr128 iss_nnp_queue.cfg 0x100020000        # 任务描述符→DDR
load ddr128 nnp.models.ci   0x190000000          # 输入 fmap
load ddr128 nnp.models      0x1f0000000          # 权重
load ppm asm_work/nnp_main_cpp.lst               # 用 .lst 建 PM 反汇编表
w reg mbx_ram:0/1 <queue_base>                   # 队列地址写 mailbox
w reg mbx_ram:13 <nnp_main_hex_len>              # 模型代码 PM 起始行
b 0x<WAIT_ON_MBOX0_TASK_LABEL>                   # 断点在固件任务循环
c
save eodma_raw <file> 0x1b0000000 co_num co_h co_w fmt  # 从 DDR 抓输出
p cycle                                          # ★周期数
```

### 4.2 ISS 本体
**预编译 x86-64 ELF,每芯片一个**(`nnp305/3020/310/315m/316/317/318/318p/320_iss`,12-16MB,带 debug_info)。**周期级解释式模拟器**(nm 可见 cu/ifetch/lu/su/nu/pe/vu_valu/vu_vlsu/vu_opmx_vreg/mbx/ccm/idma/odma/wdma/bdma/eidma/eodma/ewdma/ebdma/pma.cpp 等硬件单元模型;`step`/`cont <n>`/`print_cycle` 命令)。源码不在包内。

### 4.3 SDK 模拟器(部署级)
`run_net_bin.py:356`:内网 wget SDK 包 → iss_config.txt → `run_single_model`(x86 ELF,源码 `$SRC/deepeye/src/run_net_bin/src/main.cc`,链接 DP2000 板端 SDK:libdcl_runtime/libdrv_dp2000/libstream_sched/libmdl_parse)→ ARM 侧在 qemu-arm 下模拟,NPU 侧连 ISS(libnnp3XX_iss.so)。

### 4.4 RTL/UVM 桥(硬件 DV 支撑)
`iss_scr_gen.py:641 case_sv_gen()`:生成 UVM SystemVerilog testcase(build_phase 里 `$system` 重汇编保证编码一致;`tb.load_hex/load_file_ddr` 装 hex/nnp.models;`tb.apb_write` 配 mailbox 寄存器 = ISS 的 `w reg mbx_ram` 完全等价)。**RTL TB 与 C++ ISS 消费同一套产物** —— ISS↔硬件一致性机制。

## 5. DMA/数据搬运的运行时视角

- **两级内存规划**:① linker ddr_manage(NNP 中间缓冲 → innerbuf);② netbin 级 tensor_resource_alloc(模型 IO/中间 tensor DDR 偏移表)
- **片上 CM 布局**(`$D/lib_asm_nnp3xx/nnp3xx_main.h`):`CM_BASE_ADDR 0x10_0000_0000`;`QUEUE0_CM_BASE=4*16`,每队列 21×16B 描述符;**`HEAP_CM_BASE=200*16`** 固件 scratch:EIDMA/EODMA/EWDMA/EBDMA_ADDR_INST_LO32/HI32_OFFT(**保存"指令地址",固件按槽位回改已编码 DMA 指令的 DDR 地址字段** = 运行时输入/输出地址重定位,配合 update_data_addr.asm 的 MODE0 直接增量/MODE1 经 DDR 指针表)、MODEL_HEX_L2LD_CNT/SRC_ADDR_OFFT、TILE_OFFT0..6
- **固件任务循环**(lib_asm_nnp3xx):nnp_main.asm(bl INIT_TASK → bl RUN_TASK → halt)→ nnp3xx_init_task.asm → nnp3xx_run_task.asm:WAIT_ON_MBOX0_TASK_LABEL(轮询 6 队列 → PDMA 搬描述符到 CM → 取代码/权重地址 → UPDATE_DATA_ADDR 回改 DMA 指令 → PDMA 把代码搬 PM → 执行 → IRQ_TASK_STATUS)
- **四方共享契约**:队列描述符(DDR)+ CM 堆布局 + 固件,同时被 C++ ISS / SDK 模拟器 / UVM RTL TB / 真硬件(DeSDK)消费

## 6. profile / dump / perf

- 编译期:lto.py:3533 performance_data_process → `perf_data.fornetbin`/model_profile.json(逐算子周期估算 + DDR_Usage)
- 运行期:NNP_CYCLE_CNT_EN → profile 版固件;ISS `p cycle`/log_iss cycle_cnt;板端 `MODEL_DRV.block.slice.0` 的 hwcycles/opCycles
- **MAC 利用率公式**(dump_or_profile_on_asic.py:20-47):`macNum/1.5/(2.5×cycles×1e3)` —— 硬件设计支撑的直接数据
- 逐层 ISS golden:linker/get_lg_addr.py + up_iss_scr.py

## 7. lib_asm_* 目录 = 每芯片一份的 NNP 固件(运行时)汇编微库

13 个文件:nnp_main.asm(入口)、**nnp3xx_main.h(228 行,寄存器/CM/heap 契约头)**、nnp3xx_init_task.asm、nnp3xx_run_task.asm(565 行任务调度核心)、_profile.asm(周期统计)、get_queue_id/get_model_entry/get_descriptor_cm_addr/update_task_id_mailbox/update_mailbox_config_reg/irq_task_status/update_data_addr.asm。
芯片差异:CM_BASE 高位、队列数(nnp317/318/318p 只有 3 队列)、timestamp 缓冲等。**"算子汇编库"的说法不成立** —— 算子 asm 由 codegen+lto 每模型生成(model_asm/);lib_asm 只提供板上调度固件。

## 8. 芯片变体选择机制(五层 if/elif 映射表)

| 层面 | 位置 | nnp318p 的取值 |
|---|---|---|
| 参数配置 | deepeye/config/deepeye_config_<t>.json | DM 128K, WEIGHT_TRANSPOSE=1 |
| lib_asm | linker.py:64-79 | lib_asm_nnp318 |
| 汇编器 | linker.py:97-116 | nnp318p_ass(+ -hex_size_en) |
| PM/CM 容量 | linker_glob_*.py | PM 1024 行 / CM 512 |
| ISS 二进制 | iss.py:852-870 | nnp318p_iss |
| DDR 行宽 | iss_scr_gen.py:191-230 | 权重 128bit 行 |

## 9. 对硬件设计支撑最有价值的结论

1. **单一产物契约贯穿四级仿真**:nnp_main.hex + nnp.models + 队列描述符 + nnp3xx_main.h 同时被 ISS/SDK/UVM TB/真硬件消费。改硬件接口只需改 nnp3xx_main.h + 固件 asm + ISS,四处自动一致。
2. **PM 容量约束与 hex_l2ld 代价**:nnp318p PM 只有 1024 行(32B×1024=32KB 代码窗),大模型必然运行时 PDMA 换页,bb_ctrl 驱动循环的周期开销可在 ISS `p cycle` 量化 —— PM/Cache 设计的直接依据。
3. **周期级 ISS + 板级 hwcycles + MAC 利用率公式**构成从 RTL 前到硅后的性能闭环。
4. **DMA 地址运行时重定位**(CM heap 槽位 + update_data_addr.asm):NPU 不做虚拟地址,部署灵活性全靠固件改指令 —— DM/DDR 地址空间设计的关键约束。
