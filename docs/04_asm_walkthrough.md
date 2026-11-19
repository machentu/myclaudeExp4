# 逐算子汇编精读 —— tiny CNN 在 nnp318p 上的完整落地

> 素材:`work/nnp318p/s8_sim/`(model_asm/、asm_work/、link_info、model_mem_info.json、iss_scr、log_iss)
> 全部行号/数值为 2026-11-18 实际产物,非示意。
> 网络:data[1,3,32,32]u8 → conv1(3→8,k3,s1) → relu → conv2(8→16,k3,s2) → relu
>       → conv3a(16→8,k1)+conv3b(16→8,k1) → add → avgpool → conv4(8→4,k1) → sigmoid

## 0. 函数表:融合组 ↔ 网络算子(model_asm/ 下 4 个文件)

| model_asm 文件(截短) | 内容 | 依据(ld_info) | 行数 |
|---|---|---|---|
| `..._nn_relu_clip_nn_conv_...__lg0.asm` | **conv1+relu+量化+conv2 前半**(两层 PE 链) | 见文件内两段 pe_start | ~300 |
| `..._clip_lg0.asm` | **conv2 收尾**(s_vertical=2) | s_vertical=2, q_height=16 | 340 |
| `..._add_multiply_round_r_...__lg0.asm` | **conv3a+conv3b+add+pool+conv4+LUT**(632 行,最复杂) | filter arg=['p12','p20'](两套权重), depthwise=[0,1](pool 编成 depthwise 模式), factor arg=['p24'](LUT 表), odma lut_mode=7 | 632 |
| `fused_cast_3_lg0.asm` | **fp16→fp32 cast**(host 侧 device_copy) | 纯搬运 | ~20 |

**ILP 融合的直观体现**:5 conv+eltwise+pool+sigmoid 被缝进 3 个 NPU 函数,`_lg0` 后缀 = tile 循环层(generate_asm 的两层结构 _main/_lg0)。

## 1. 单个 conv tile 的解剖(以 `..._clip_lg0.asm` 为例)

一个 tile 内的固定点程序,段序 = 数据通路顺序:

```
函数 prologue          mov r8 HEAP_CM_BASE;st TILE_OFFT0..6 ← 清 7 个 tile 偏移槽
## c_tile / ## tile=1  ← DeepEyeInjectAsmCutPoint 的切点标记(lto/ISS 用)
## ld_info ...         ← 参数元信息注释(get_param_info 解析;arg=['p8'] 即权重在
                          params 里的键名 —— 这就是 pXX 命名链的终点)
------ ewdma ------    权重 DDR→DM(external weight dma):
                        4 级 ewdma_ext_dist / ewdma_dm_dist(type=0..3 = DMA 地址
                        生成器的 4 级嵌套循环:每级 loop_num×dist);
                        DM ping-pong:dm_start_addr=0x1ff80, end=0x1ffff
                        ★0x1ffff = DM(128KB)顶部 —— buffer 在 DM 顶端向下长
------ wdma ------     权重 DM→PE 因子缓冲:gp_size=2 gp_num=2(2PE×2组),first_gp_mask=0x3
------ idma ------     输入 DM→PE(含硬件 padding dcon_* / 常量填充)
------ pe ------       计算主体(见 §2)
------ odma ------     PE→DM 输出:odma_layer_cfg0 lut_mode=7(此层 LUT 在输出级套用!)
------ eodma ------    DM→DDR(ebdma/bdma 变体负责 LUT 表/因子等旁路数据)
```

## 2. PE 阵列映射(nnp318p,PE_NUM=4)

`..._clip_lg0.asm:253` 起的真实配置:

```
pe_layer_cfg0 ... gp_size=2 gp_num=2 s_horizontal=1 s_vertical=2 bias_mode=1 ...
pe_layer_cfg1 ... q_height=16 pad_left=0 ...
pe_attr low_pe_en=1  ... pe_mask=0x00000005   ← 组0:PE{0,2}(0b0101)
pe_attr high_pe_en=1 ... pe_mask=0x0000000a   ← 组1:PE{1,3}(0b1010)
pe_attr effective_pe_en=1 pe_mask=0x0000000f  ← 全阵列 0b1111
pe_attr effective_high_pe_en=1 pe_mask=0x0000000a
pe_ci_num ci_num=8        ← 本轮吃 8 个输入通道
pe_num_obuf num_obuf=2 pe_icp=2   ← 2 组输出缓冲,icp=2(双通道交织)
pe_start pe_num_input=1 pe_num_round=1 odma_rd_obuf=1 obuf_reuse=0 layer_end=0
```

**q_gp_size × com_w ≤ PE_NUM 的实例**:gp_size=2(每 PE 组管 2 个输出通道组),com_w=2 → 2×2=4=PE_NUM 恰好用满。`s_vertical=2` 就是 conv2 的 stride 2。`layer_end=0/1` 标记融合组内两层 conv 的 PE 流水衔接。

## 3. 第三融合函数的三个特殊结构(632 行)

1. **双权重单函数**:`ld_info filter arg=['p12','p20']` + `ci=[16,8] co=[8,4]` —— conv3a/conv3b 的两套权重一次 ewdma 装载,`max_obuf=[4,2]` 分组输出。
2. **avgpool 编成 depthwise**:`depthwise=[0,1]` —— 池化与 depthwise conv 共用同一 PE 通路(k=2×2 等效)。
3. **LUT(sigmoid)**:`factor arg=['p24']`(p24=256 项 fp16 LUT)→ `bdma_burst bdma_lut16=1`(BDMA 旁路装载到 LUT RAM)→ `odma_layer_cfg0 lut_mode=7`(输出级查表)。**sigmoid 没有 PE/VU 指令,是 odma 硬件查表完成的**。
4. **跨函数 DMA 链**:`ld_info eidma arg='fused_..._conv_...:0'` —— 本函数输入直接引用上一融合函数的输出(DDR 上的衔接),fusion stitching 的组间契约。

## 4. .lst ↔ .hex ↔ PM(程序存储器)

`asm_work/nnp_main_cpp.lst`(cpp 展开后)每行 = 256bit VLIW 指令包 + PC:

```
0000...00010022 //11 :0000: bl NNP3xx_INIT_TASK_LABEL
0000...00fffc064 //12 :0001: mov r14.lo 0x1ffc
```

- 前 64 hex = 8×32bit 槽(VLIW 并行发射);`//行号 :PC : 助记符`
- `load ppm asm_work/nnp_main_cpp.lst`(iss_scr)—— ISS/RTL 用它建 PM 反汇编表
- **link_info**:`nnp_main_hex_len: 122`(固件)+ `model_hex_len: 718`(模型)< PM 1024 行 → 不触发 hex_l2ld 换页。`iss_scr` 的 `w reg mbx_ram:7 122` 把"模型代码从 PM 第 122 行开始"告诉固件。

## 5. DDR 地址空间图(从 iss_scr + model_mem_info 拼出)

```
0x100010000  din_addr.cfg       多输入地址指针表
0x100018000  dout_addr.cfg      多输出地址指针表
0x100020000  iss_nnp_queue.cfg  任务队列描述符(15×128bit/模型: fmap/权重/代码/mid 地址)
0x190000000  nnp.models.ci      输入 fmap(128bit 行 hex)
0x1a8000000  nnp_main.hex       固件+模型代码(→PDMA 搬进 PM)
0x1b0000000  (输出区,eodma_raw 从这抓回)
0x1f0000000  nnp.models         权重+量化因子(3040B)
             mid_buf/middle_buf = 6656B(innerbuf,ddr_manage 规划,life 复用)
CM(片上 0x10_0000_0000 基):QUEUE 描述符区(4*16 起,每队列 21×16B)
             HEAP_CM_BASE=200*16(固件 scratch:EIDMA/EODMA_ADDR_INST 槽=运行时改
             DMA 指令地址字段、TILE_OFFT0..6、HEX_L2LD 计数)
DM(片上 128KB):0x1ff80..0x1ffff = ping-pong buffer 顶部区
```

## 6. 固件任务循环(lib_asm_nnp3xx ↔ iss_scr 对应)

```
nnp_main.asm:      bl INIT_TASK → bl RUN_TASK → halt
nnp3xx_run_task.asm:
WAIT_ON_MBOX0_TASK_LABEL:                 ← iss_scr 的 b 0x<此标签 PC>(断点)
  GET_QUEUE_ID        轮询 6 队列任务数   ← iss_scr: w reg mbx_ram:0 0x20000(队列基址)
  GET_DESCRIPTOR_CM_ADDR  PDMA 从 DDR 搬描述符到 CM(0x100020000 那 15×128bit)
  GET_MODEL_ENTRY     代码/权重 DDR 地址入 CM_QUEUE_EXE_*
  UPDATE_DATA_ADDR    按 descriptor 回改已编码 DMA 指令的 DDR 地址(CM heap 槽位法)
  (PDMA 把代码搬 PM) → 执行模型代码
  IRQ_TASK_STATUS     完成计数写回 CM
```

## 7. 周期与利用率

- `log_iss` 的 `cycle_cnt`(本模型 3654;首段 29 = 固件空转)
- MAC 利用率公式(板端同样适用):`macNum/1.5/(2.5×cycles×1e3)`
  本模型 conv1 MAC 数 = 8×3×9×32×32 = 221184,conv2 = 16×8×9×16×16=294912,...
  总 ≈ 0.58M MAC;nnp318p 峰值 4PE×12MAC = 48 MAC/cycle → 理想 ≈ 12.1K cycles 的 1/24
  —— 小模型 DMA/配置开销主导,**这正是 tile/融合优化的研究对象**(用 s5 的 dump 和
  本 asm 对照每段的指令条数即可量化配置开销占比)。

## 8. 怎么继续玩

- 改 `NPUP_TARGET=nnp316`(PE_NUM=12):同一段 pe_attr 会变成 12 位 mask,gp_size/com_w 重排 —— 对比两个目录的同名 asm 文件即可看到 PE 数对代码形态的影响
- 改 `deepeye_config_nnp318p.json` 的 DM_SIZE/PE_NUM(经 Environment 属性)重跑 s4/s8,看 tile 结构与 cycle 变化(见 experiments/)
- 用 `iss_bin_to_txt.py` 把 net_combine.bin 反转回 hex 文本,验证"部署产物 ↔ 本目录汇编"逐字节一致
