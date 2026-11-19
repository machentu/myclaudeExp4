# npu_pipeline — 云天(Intellifusion)TyTVM 工具链模块化研究流水线

> 目标:把 TyTVM v1.0.41(天泫/DEngine,基于 TVM 0.6.dev)的整条 NPU 编译工具链
> 按**流水线阶段解耦**成一个个可独立运行、可观察中间产物、可修改实验的模块,
> 为 NPU 工具链研究和硬件设计提供支撑。
>
> 首个端到端样例:nnp318p + 小 CNN(conv/relu/eltwise/pool/1x1/LUT-sigmoid),
> ISS 周期级仿真与 CPU 参考输出 cosine = 1.000000。

## 快速开始

```bash
source env.sh
python3.8 run_all.py                    # 全流程(小 CNN)
python3.8 run_all.py --model /DEngine/projects/mywork/yolov5-personcar-640x384-addsigmoid.onnx
python3.8 stages/s4_build.py            # 单独重跑某阶段(自动向前补算)
NPUP_TARGET=nnp316 python3.8 run_all.py # 换芯片
NPUP_DUMP_TIR=0 ...                     # 关闭 TIR 逐 pass dump(更快)
```

环境原理:**预编译 `/DEngine/tytvm/tytvm_build` 提供 libtvm.so(C++/LLVM 不重编),
PYTHONPATH 前置源码包使 deepeye 的 Python 逻辑从 .py 加载**(可读可改,非 Cython .so)。

## 流水线全景

```
s0 ONNX ─► s1 Relay(fp32) ─► s2 量化(int8) ─► s3 改写预览 ─► s4 build_relay ─► ...
   s4 内部(黑盒,靠 dump 观察): TVM优化 → 切分落地 → ILP融合 → tile搜索
                                   → TIR 50+pass(4阶段) → NPU codegen(.asm)
... ─► s5 TIR索引 ─► s6 linker→汇编器→net_bin ─► s7 net.bin解析 ─► s8 ISS仿真+比对
```

## 阶段模块索引

| 阶段 | 模块 | 做什么 | 厂商核心代码 | 关键产物 |
|---|---|---|---|---|
| s0 | `stages/s0_model.py` | 生成小 CNN / 加载外部 ONNX + 伪随机校准帧 | — | tiny_cnn.onnx, input_data.npz |
| s1 | `stages/s1_front_end.py` | ONNX → Relay | `frontend/onnx.py:8086 from_onnx`(8220 行深度改写版) | relay_fp.txt |
| s2 | `stages/s2_quantize.py` | 校准 + int8 量化(对称, per-channel 权重) | `quantization/quantize.py:184`(五步:analyze/collect_stats/calibrate/realize/post_process) | relay_int8.txt |
| s3 | `stages/s3_rewrite.py` | deepeye relay 前处理**预览**(~60 pass)⚠️只读 | `relay_pass/deepeye_rewrite.py:5146` | relay_rewritten.txt |
| s4 | `stages/s4_build.py` | 编译(rewrite+融合ILP+tile搜索+TIR+codegen) | `build_module/build_module.py:34 build_relay` | graph.json, fused.model, 783×NN_*_ir.cc |
| s5 | `stages/s5_tir.py` | TIR dump 索引 + intrinsic 演化 + DMA 视角 | 解析 s4 产物(customK→pass 名) | index.md, intrinsic_evolution.md, dma_view.md |
| s6 | `stages/s6_netbin.py` | linker(lto/DDR规划/汇编器/hex_l2ld)→ 打包 | `net_bin.py:183` → `linker/linker.py:16` → `lto.py:4249` | net.bin, model.bin, net_combine.bin |
| s7 | `stages/s7_binview.py` | net.bin 二进制结构解析 | 布局依据 `nnp3_vu/nnp3_vu_block.py` | net_bin_layout.txt, model_info.json |
| s8 | `stages/s8_sim.py` | ISS 周期级仿真 + CPU 参考比对 | `golden.py:386` 模式;`iss_nnp3xx/iss.py:682 run_iss` | iss_output.npy, log_iss(cycle_cnt), compare.txt |

深度分析文档:
- [docs/01_frontend.md](docs/01_frontend.md) — 前端/量化(模型→Relay→int8)
- [docs/02_middle.md](docs/02_middle.md) — 中段(relay pass 表/host-NPU 切分/ILP 融合/TIR 50+ pass 全表)
- [docs/03_backend.md](docs/03_backend.md) — 后段(汇编器/linker/DDR 规划/net.bin 布局/ISS/UVM/固件契约)
- 历史:`/DEngine/projects/TyTVM_codegen_analysis.md`(codegen 深挖)+ `tytvm_traces/`

## 硬件设计支撑要点(为什么这些模块值得研究)

1. **改一个数字改变整个策略**:PE_NUM/DM_SIZE 在 `deepeye/config/deepeye_config_*.json`,
   影响 tile 切分(`top/helper.py:195 small_enough`)、融合 ILP 代价、PM 换页。
   用 NPUP_TARGET 切芯片对比 s4 的 TIR dump 与 s8 的 cycle_cnt 即做设计空间探索。
2. **周期级闭环**:ISS `p cycle`(本流水线 log_iss)↔ 板端 hwcycles ↔ MAC 利用率公式
   (`profile_and_dump/dump_or_profile_on_asic.py:20`:macNum/1.5/(2.5×cycles×1e3))。
3. **四方共享契约**:nnp_main.hex + nnp.models + DDR 队列描述符 + `lib_asm_nnp3xx/nnp3xx_main.h`
   同时被 ISS / SDK 模拟器 / UVM RTL TB(`iss_scr_gen.py:641 case_sv_gen`)/ 真硬件消费。
4. **PM 约束**:nnp318p PM 仅 1024 行,大模型触发 hex_l2ld 运行时换页(`model_hex_l2ld.py:1503`),
   开销可量化 —— PM/Cache 设计依据。
5. **DMA 地址运行时重定位**:CM heap 槽位 + `update_data_addr.asm` 回改已编码指令
   —— 地址空间设计的关键约束(docs/03 §5)。

## 已发现的工具链脆弱点(实测)

| 现象 | 位置 | 影响 |
|---|---|---|
| **DMA 描述符 loop_num 13 位上限 8191(全家族:nnp310/316/317/318/318p 一致)**:v61 双目 800×1280 的 eodma 单级循环 9600 次 → 编译通过但汇编器拒绝 | ass_nnp3xx/*_inst_table.py 的 eodma_ext_dist 字段 | 大输出图必须在 codegen/描述符规划层分解到多级;硬件设计上这是 DMA 地址生成器级数×位宽的硬约束 |
| ir_version>6 非确定性折叠:gs 预处理内 onnxruntime 1.8.1 拒载 ir7 模型 → 部分折叠。轻则产出静默退化图、重则 from_onnx 断言崩溃,同一命令两次结果不同 | from_onnx 内 graphsurgeon 块(onnx.py:8170 附近) | s0 已自动降 ir→6 存副本(normalized_ir6.onnx);s1 默认禁用 gs(NPUP_USE_GS=1 恢复) |
| `rewrite_for_deepeye` 与 `relay.build` 隐性耦合:拆开调用 → graph 参数节点留原名而 get_params 返回 pXX → lto KeyError;同进程二次 rewrite 同样崩 | build_module.py:50 vs s4 拆分实验(2026-11-18) | s3 只能做只读预览,s4 必须整体调 build_relay |
| **PE_NUM 是编译侧旋钮**:改 2/8 编译成功(8-PE 代码更短),但 ISS 二进制固定 4-PE 拒载 | nnp318p_iss 微架构 | 设计空间探索须跨芯片对比,不能单芯片改 PE |
| nnp316 ISS 挂起:固件任务循环空转,疑 40bit CM 基址 vs 33bit 队列描述符假设不匹配 | iss.py 生成脚本 vs lib_asm_nnp316 头 | 地址位宽改动必须四方(固件/ISS/UVM/SDK)同步 |
| 编译期周期估算系统性高估 ~2.7-3.2×(yolov5/mcv3 实测) | build_module.cc:792 | ILP 分组决策用该估算,做硬件模型需校准 |
| NPU(fp16 LUT+24bit)与 CPU float 参考存在数值发散:sigmoid 密集检测头 cosine 0.85-0.96,非 sigmoid 模型 1.0 | 量化 realize vs odma LUT | 用 run_function_fused.py/up_iss_scr.py 逐层定位 |
| VU 子函数名带纳秒时间戳,两次编译产物逐字节不同 | nnp300/vu_abs.py:118 | 无法做内容寻址缓存/二进制差分 |
| conv intrinsic 天生 tile 级,56×56 全图在 nnp318p 无解(PE_NUM=4) | nnp300/helper.py:18 断言 | DeepEyeTilePartition 是合法性前提,非优化 |
| 手搓 TIR 直接进 C++ codegen 会 segfault(形态依赖极紧) | codegen_nnp300.cc:546 as<Load> | 设备 codegen 与 pass 链硬耦合 |

## 目录结构

```
npu_pipeline/
├── env.sh              # 环境(PYTHONPATH 源码优先 + LD_LIBRARY_PATH 预编译)
├── run_all.py          # 端到端驱动(--from/--to/--model)
├── stages/             # 9 个阶段模块(每个含厂商代码索引 docstring)
├── docs/               # 三份深度分析文档
└── work/<chip>/sN_*/   # 每阶段中间产物(可单独检查/重跑)
```
