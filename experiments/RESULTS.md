# 硬件设计支撑实验结果(第一轮)

> 2026-11-18。基线 tiny CNN(32×32 五层):nnp318p cycle=3654,cosine=1.0,代码 718 行(<PM 1024,不换页)

## 实验 0:真实模型端到端(nnp318p,全 9 阶段)

| 模型 | conv 数 | **ISS 实测 cycles** | 编译期估算 | 估算/实测 | 权重 | DDR mid_buf | 模型代码(hex 行) | PM 换页 | cosine(ISS,CPU) |
|---|---|---|---|---|---|---|---|---|---|
| tiny CNN | 5 | 3,654 | — | — | 3.0 KB | 6.5 KB | 718 | 否 | 1.000000 |
| yolov5-personcar 640×384 | 60 | **4,460,569** | 14,242,665 | **3.19×** | 4.00 MB | 2.95 MB | **1490** | **是** | 0.85/0.96/0.90(3 输出) |
| mcv3 NV12 768×448(双输入) | 73 | **6,996,832** | 19,130,340 | **2.73×** | 1.48 MB | 3.64 MB | **1886** | **是** | 1.000000/0.999987 |
| osnet ReID(attention 模块) | 187 | **3,167,689** | 7,309,440 | **2.31×** | 2.50 MB | 0.92 MB | **1772** | **是** | **0.812**(发散最大) |
| yolov8n-pose 480×256 | 73 | **2,306,604** | 9,451,217 | **4.10×** | 3.62 MB | 1.88 MB | **1202** | **是** | 0.998 |
| yolov8n-seg 480×256(ir9/opset10) | 77 | **3,757,423** | 13,050,292 | **3.47×** | 3.56 MB | 1.99 MB | **1202** | **是** | 0.995/0.975(det+mask 双输出) |
| v61 双目 800×1280(950 节点,220 conv) | 220 | **汇编器拒绝** | 276,214,940 | — | — | — | — | — | — |

补充:估算/实测比 2.3~4.1×(6 模型);所有 >1024 行模型都走 PM 换页;ir9+opset10 的
yolov8n-seg 在 ir6 规范化 + gs 禁用后全链通过;osnet 的 channel-attention(sigmoid 密集)
数值发散最大(0.812),进一步佐证"发散集中在 LUT 化的非线性层"。

## 实验 4:v61 eodma 描述符超限修复(✅ 成功,2026-11-19)

**问题**:v61(双目 800×1280,950 节点)编译通过但汇编器拒绝:
`eodma_ext_dist loop_num=9600 > 8191`(loop_num 字段 13 位,全家族约束)。
病灶在 `fused_transpose_5`:strided-scatter 输出(每 2B 元素按 640B 步长散写,
q_width=2 + data_extract=1,源于 v61 的 47 个 strided_slice)。

**修复**(改厂商汇编器,备份 .orig_npu_pipeline):汇编前文本后处理,把
`index=0 单级干活、index=1..3 空转(loop=1)` 的超限描述符拆两级嵌套
(9600→96×100,type1 dist=内块尺寸,与 nnp300.py 既有模板语义一致),ext/dm 侧联动。

**三轮迭代教训**(每轮都是工具链结构的实锤):
1. 每芯片汇编器是 6000+ 行**完整拷贝**而非薄封装 → 9 个变体(nnp3020..318p + 两个基类)全部打补丁
2. 模型代码经 cpp `#include` 展开,报错行号在展开产物 → 补丁扫描命令行文件所在目录全部 .asm
3. 同文件多组描述符 → 顺序配对算法(逐行扫,index=0 记住、紧邻 index=1 评估)

**验证**:离线(现成 asm_work 重汇编 ASSEMBLE_OK,8 处拆分)+ 链内
(ass-patch 六处生效,net.bin 3.6MB 代码/3.1MB 权重/**56.9MB DDR 中间缓冲**/双输入,
s7 结构解析通过)。s8 ISS 仿真待内存空闲时补跑(与逐层诊断并跑时疑似 OOM)。

**硬件设计含义**:DMA 4 级地址生成器的每级 13 位 loop_num 是输出图尺寸的硬约束
(8191×stride 覆盖范围);描述符规划器必须支持嵌套分解——当前工具链缺失这一层,
本补丁即该缺失层的最小实现。

## 实验 5:osnet 精度(0.812)归因(进行中)

已排除/确认:
- **LUT 洗清嫌疑**:NU_LUT_EN=0 后 cosine 逐位不变(0.811564),编译图确有变化
  (fused_func 哈希不同)但数值路径不变
- 假设:逐层复合舍入(osnet 数百层,fp16 scale/24bit 累加误差线性累积)
- 厂商逐层工具 `dump_func_output`/SubfunctionRunner 的 sim 侧走 **SDK 模拟器
  (内网下载)**,本容器不可达 → 逐层数值比对被阻(第一个计算层即抛
  "Please config SDK related environment variable")
- **下一步(已明确)**:用 linker/get_lg_addr.py + up_iss_scr.py 在整模型 ISS 脚本里
  注入逐层 `save ddr`(golden.py 的逐层 golden 路径,不依赖 SDK),CPU 侧逐层参考
  用 eval_relay 跑 fused_func —— 形态判定:线性下降=复合舍入(位宽设计依据),
  骤降=特定算子缺陷

## 实验 0b:PM 深度不是独立旋钮(l2ld 展开耦合,exp_pm_depth.py)

| pm_depth | l2ld 触发 | 模型代码(hex 行) | ISS |
|---|---|---|---|
| 1024(出厂) | 是 | 1490 | ✅ 4,460,569 cycles |
| 2048 | **仍是** | **3538**(膨胀 2.4×) | ❌ 块布局非法,29 cycles 即死 |
| 4096 | **仍是** | **7536**(再膨胀) | ❌ 同上 |

**结论**:hex_l2ld 的"循环展开填满窗口"策略使代码体积随 PM 窗口**比例膨胀**
(1024→1490、2048→3538、4096→7536 行),膨胀后单块(len0=3866)仍超窗口 →
非标 pm_depth 的构建无法通过 ISS。**在当前工具链里"加大 PM 省换页"的实验路径
被封死;换页开销的量化要改走 profile 固件(NNP_CYCLE_CNT_EN)或 ISS trace 统计
PDMA 传输次数**。对硬件设计的含义:PM 容量与代码生成策略(展开/不展开)必须
联合设计,孤立放大 PM 无意义。

**v61 失败详情(第 5 条硬件结论)**:s4 编译成功(956s,fused.model 正常),lto 完成,
汇编器报 `eodma_ext_dist loop_num=9600 > max 8191`。**跨 nnp310/316/317/318/318p 五张
指令表核实:loop_num 字段统一 13 位(上限 8191)= 家族级 DMA 描述符约束**。大输出图
(800×1280)的 eodma 循环需分解到多级描述符(type0×type1 嵌套)或拆条发射,该模型的
codegen 路径没有做此分解——属工具链对大图 eodma 的缺陷;对硬件设计的含义:
**DMA 地址生成器的级数×每级位宽是硬约束,超大特征图必须在描述符规划层提前分解**。

**五条硬件设计结论**:
1. **编译期代价模型系统性高估 ~2.7-3.2×**(build_module.cc:792 的 Total Cycles vs ISS 实测)——
   ILP 融合用它做分组决策,意味着实际分组可能偏保守;做新硬件代价模型时必须校准。
2. **PM 1024 行在 640×384 检测网络就已打爆**(1490/1886 行)——hex_l2ld 运行时换页是常态而非
   边缘情况,换页开销已含在实测 cycles 里;PM 容量是 nnp318p 级别芯片的第一约束。
3. **数值发散与模型相关**:tiny/mcv3 cosine=1.0,yolov5 的 sigmoid 输出头 0.85-0.96——
   NPU 侧 fp16 LUT + 24bit 再量化 vs CPU float 参考的差异在 sigmoid 密集的检测头上放大
   (研究逐层误差用 deepeye/relay_pass/run_function_fused.py + linker/up_iss_scr.py)。
4. NV12 双输入(y_plane+uv_plane uint8)流水线无障碍——量化/前端对多输入原生支持。
5. **DMA 描述符 loop_num 13 位上限 8191 为全家族约束**(nnp310/316/317/318/318p 指令表
   一致);v61(800×1280)的 eodma 单级循环 9600 次无法编码 → 汇编器拒绝。超大特征图
   必须在描述符规划层分解到多级(type0×type1)或拆条发射。

## 实验 1:PE_NUM 敏感性(nnp318p config json 临时改写)

| PE_NUM | 编译 | cycles | model_hex_len | 结论 |
|---|---|---|---|---|
| 4(基线) | ✅ | **3654** | 718 | cosine=1.0 |
| 2 | ✅ | — | 718 | ISS 拒载(iss error) |
| 8 | ✅ | — | **656** | ISS 拒载;代码反而变小 |

**关键发现:PE_NUM 是"编译侧旋钮",不是"硬件侧旋钮"**:
- 编译全部成功,且 8-PE 时模型代码缩短 62 行(656 vs 718,更宽的阵列→更少 tile 循环);
- 但 `nnp318p_iss` 二进制**固定 4-PE 微架构**,2/8-PE 的 pe_mask 编码无法在错配模拟器上验证;
- ⇒ 设计空间探索的正确姿势:**跨芯片对比**(各自配套 ISS),而非单芯片改 PE_NUM。
  若要研究"同一芯片不同 PE 数",需要对应版本的 ISS(厂商内部应有)。

原始数据:`hw_sensitivity_result.json`;脚本:`exp_hw_sensitivity.py`(自动备份/恢复 json)。

## 实验 2:nnp316 vs nnp318p 编译期对比(tiny CNN)

| 指标 | nnp316(PE=12) | nnp318p(PE=4) |
|---|---|---|
| PE 分解(同一算子) | **gp_size=2×6组 / 6×2组** | gp_size=4×1 / 2×2 |
| pe_mask 形态 | 12 位(0x555/0xaaa/0x3cf...) | 4 位(0x5/0xa/0xf) |
| 固件 hex 行 | 208 | 122 |
| 模型 hex 行 | 782 | 718 |
| 权重(model.bin) | 4,736 B | **3,040 B**(WEIGHT_TRANSPOSE 压缩) |
| ci_num / num_obuf | 3,8 / 3,4 | 8 / 2 |

- 同一网络在两种阵列上得到**完全不同的空间分解**——PE 数直接改变 com_w×q_gp_size 划分、
  通道分组(ci)与输出缓冲组织(obuf);
- nnp316 权重不压缩(318p 的 WEIGHT_TRANSPOSE 是芯片特性)。

## 实验 3:nnp316 ISS 挂起(跨芯片移植性问题)

`nnp316_iss` 执行固件任务循环时永久空转(99.9% CPU,断点不达):
- 疑似原因:nnp316 的 CM 基址是 40bit(`CM_BASE_ADDR_HI=0x40`,见 lib_asm_nnp316 头文件),
  而 iss.py 生成的队列描述符/邮箱写入按 33bit 地址布局 → 固件轮询不到任务;
- **教训:工具链的"四方契约"(队列描述符+CM 堆+固件+ISS)对地址空间假设极其敏感,
  改地址位宽必须四方同步**——这正是给硬件设计者的核心约束示例(docs/03 §5)。
- 待办:对照 iss_nnp3xx/iss.py 的 nnp316 分支与 lib_asm_nnp316/nnp3xx_main.h 的宏差。

## 附:工具链兼容性坑(真实模型,mywork/)

| 坑 | 现象 | 规避 |
|---|---|---|
| **ir_version>6 非确定性折叠** | gs 预处理里 onnxruntime 1.8.1 拒载 ir7 → 部分折叠:轻则 from_onnx 产出 0 算子退化图(静默错误!),重则 `Input names don't match` 断言 | s0 已自动降 ir_version→6 存副本(normalized_ir6.onnx) |
| ir_version=9(yolov8n-seg) | opset 10 + ir9,降 6 后仍可能有限制 | 未测,遇错再看 |
| standalone 阶段重跑丢 --model | ensure 重建时 s0 不知道模型路径 | s0 持久化 model_path.txt(自动恢复) |

## 下一步建议

1. PM 换页量化:yolov5(718 行模型代码已超 nnp318p PM?目前 718<1024 未触发;v61 双目 950 节点大概率触发 hex_l2ld)——对比 s8 的 link_info `model_hex_len` 与 PM 深度 1024;
2. 拿到厂商各芯片 ISS 后重跑跨 PE 对比;
3. 用 `dump_or_profile_on_asic.py` 的 MAC 利用率公式,把各模型 cycles 换算成利用率排行,定位 DMA/配置开销占比(小模型 3654 cycles 中理想计算只占 ~1/24)。

---

# 第二轮实验(2026-11-20/21)

## 实验 6:v61 s8 补跑完成 —— cycles 到手,数值全错

| 项 | 值 |
|---|---|
| ISS 实测 cycles | **84,228,510** |
| 编译期估算 | 276,214,940 → 估算/实测 **3.28×**(落在实验 0 的 2.3-4.1× 带内) |
| cosine(ISS,CPU) | **0.051** —— 范围接近(ISS[-539,269.5] vs CPU[-505,235.75])但空间结构全乱 |
| 耗时 | s4 级联 28min + s8(linker+ISS) ~2h15m |

输出取证(iss_output_0.npy vs cpu_output_0.npy):
- 非行置换:逐行最佳匹配 cos 仅 0.42(mean 0.16),行映射恒等率 0.5%
- 非 96×100 块转置(0.050→0.016 更差);列方向 cos 0.65-0.82(列结构部分保留)
- eodma 拆分补丁数学复核**等价**:ext 96×640B+100×61440B(=96×640✓)、dm 96×2B+100×192B,
  与单级 9600 地址集/序一致;dm 窗口 19200B=9600×2B✓;配对指令 r5=2 是元素大小(vu 家族
  segment 语义,对照 vu_abs.py:322)非 loop 派生,文本补丁未留陈旧常数
- **补丁本身排除嫌疑 → 逐层注入定位首个发散层(进行中)**。注意:9600 非法描述符出自厂商
  codegen 本身,该散写路径厂商从未执行过,不排除上游还有潜伏缺陷

## 实验 7:逐层归因工具打通(零 SDK,实验 5 "下一步"落地)

`experiments/osnet_iss_layerwise.py`(通用化:环境变量 NPUP_LAYERWISE_TAG 指定模型):
- **注入**:get_lg_bp_from_lst 扫 gp lst 的 `mov mbox3.s3 r2`(0e184068) imm16 变化沿→断点表;
  复刻 up_iss_scr.py:27-53 厂商契约(跳过全局第 1 断点、第 k+2 断点处 save lg k 即"晚一层"),
  在每条 `load ppm ...lib_bb_gpN_cpp.lst` 锚点后插 b/c/dis b/save ddr128 四连 → iss_scr_layerwise;
  与共存的原生 `b 0x1f8`(bb_ctrl L2LD 循环)断点系统无冲突(gp 内断点必先于下一次 0x1f8 命中)
- **地址**:lg_info.json offset + queue_cfg.json mid 基址(0x1d0000000,与 iss_glob 一致);
  末层 out_buf 的 offset 在 offset_for_one_buf 嵌套字段;byte_size 规则照抄 get_lg_addr.py:99-126
- **CPU 参考**:SubfunctionRunner(run_sim=False) 纯 LLVM eval_relay 逐层(零 SDK;
  osnet 819 项输入 99s、mcv3 52s——远比预估 10-30min 快);返回序契约=dump_func_output:397-442
  (前 len(params) 项输入回显,其后按 graph tvm_op 节点序,多输出为 list)
- **对齐**:func_name 出现序队列 ↔ graph debug_name;rank 差异 squeeze 容忍(relay 与 NPU 元数据
  允许差冗余 1 维,如 (1,1,512,1) vs (1,512,1))
- **开销**:osnet 172 断点+save 的 ISS 仅 240s(基线 ~200s)——断点代价可忽略
- **契约坑(实测)**:多输出模型外层 Tuple 会混进 cpu_outs(mcv3 110=1+107+1,需过滤);
  厂商自己的 run_function_fused.py:498 调 get_lg_bp_from_lst 少传 nnp_name 参数(别走该入口);
  params 必须在 s8 前快照(weight_second_handle 就地改写 conv2d_transpose 权重,golden.py:318)

**对照验证(方法可信度)**:mcv3 86 层全部 ≥0.999987(median 1.000000,最低层=其整模型基线)。
该对照用 11-18 旧 s8 产物 × 11-21 重建 CPU 参考 → **s2/s4 跨 3 天确定性实证**。
mid_buf 无交织:from_iss_dump 裸 row-major+16B 行内字节反转即正确(co_split 反交织只属于
eodma_raw 输出抓取格式,iss.py:957-1056)。

## 实验 8:osnet 0.812 逐层结果 —— "复合舍入"出局,两个真实发散点

173 lg:median 0.9386,**首 conv(lg0)即 0.746**,此后 0.45~0.99 振荡无单调下降;
3 个 sigmoid 注意力门 [1,64,1,1] 精确 1.000000(饱和吸收上游差异);
末层 lg171 逐层值 0.8116 = 整模型基线 0.811564(方法自洽交叉验证)。

- **复合舍入假设出局**(应 ~1.0 起步缓慢单调下降,与"首层即发散+振荡"矛盾)
- **硬件缺陷①:lg143 `fused_nn_instance_norm_8` NPU 侧产生 64 个 float16 NaN(CPU 侧 0 个)**
  —— fp16 除法/溢出路径缺陷,NaN 下游传播(被 clip/relu 吸收但信息已毁)
- **发散②:lg0 int8 conv 输出 NPU[-110,105]/211 级 vs CPU[-128,127]/247 级,拟合斜率 0.61,
  精确相等仅 2%,分段 cos 均匀低(0.29-0.96)** —— 而 mcv3 首个 int8 conv 逐位 1.0,
  非 conv 路径通病;特征指向 requantize scale/输入量化路径差异(osnet 输入是 float RGB,
  mcv3 是 uint8 NV12)
- 已排除:save 时序(无半写特征)、空间排布置换(置换/通道分块/NHWC/16B 交织全试过无提升)、
  量化参数漂移(确定性实证)
- 下一步:lg0 微实验(抽 Conv_0 fused func,对比 NPU/CPU 两侧输入量化与 requant 参数)

## 实验 9:v010 host 节点 —— 整图 ISS 路径的结构性边界(工具链结论第 6 条)

v010(context-gate board NV12 768×448,195 tvm_op)含 **6 个 device=1 host 侧节点**:
4×`__copy` + 2×`fused_subtract`(NV12 board 预处理),图中段形成
NPU cast→copy→host subtract→copy→NPU cast 的**设备往返链**。
原版语义 = 3 个 NPU block + ARM wrapper.c 交替执行(s6 三次 LTO=三块,net.bin/s7 正常);
编译期估算 37,652,313 cycles(三次重建逐位一致)。

s8 整图路径两连崩与修复(本地研究版):
1. `KeyError '__copy_kernel0'`→`'fused_subtract_5_kernel0'`(lto.py:644):host 节点无 NPU kernel
   标签。根因:nnp_func_names 对无 dedsp 图返回**空列表** → generate_nodes 的 `len(func_names)>0`
   过滤失效 → host 节点直通 func_lib。**已修**:s8 按 device==nnp300.device_type 显式构造
   func_names(对齐 net_bin 分块语义,stages/s8_sim.py)
2. `ddr_manage.py:1343 assert len(in_bb)>0`:输入经 host 链才达 NPU bb,整图 ddr_manage 假设
   "输入直接进 NPU" —— **结构性不适用,非单点 bug**;且即使打通,ISS 不执行 host subtract,
   数值必错(原版靠 ARM+QEMU 交替,SDK 路径)
3. 决策待定:A. 多块 ISS 桥接(块间 numpy 执行 host op,最忠实,工作量大)/ B. 图手术挪
   subtract 进 NPU(改研究对象语义)/ C. 归档为 s6-only 模型

## 实验 10:whisper encoder —— 工具链能力边界是 erf/GELU(结论第 7 条)

whisper-small encoder(纯 transformer:MatMul×96、LayerNorm 分解式 ReduceMean/Pow/Div、
**零卷积**,1423 节点,固定形状 [1,80,3000]→[1,1500,768],ir7/opset14):
- s0/s1/s2/s3 **全过**(前端+量化对 transformer 无障碍;ir7→6 自动降级正常)
- s4 FusionS:**MatMul→ATOMIC_CONV 映射成功**(NPU 以 1×1 conv 吃矩阵乘),804 原子 op 被消化
  (RESHAPE/CONV/BIASADD/VU_ELTWISE/CAST/ANONYMOUS),tile 搜索正常启动
- **10 个 ATOMIC_ANONYMOUS 全部落在 GELU 子图**(`%18=erf→reshape→multiply→round` 链,
  encoder 每层一个,Scope 19/62/88/94...)
- 死点:`deepeye_fusion_stitching.cc:2362 Check failed: node_2_subgraph... OP not recognized`
- **结论:融合引擎的算子边界卡在 erf**;默认开启的 LUT(NU_LUT_EN=1)也未收录 erf
- 出路(未验证):①改用 tanh 近似 GELU 的导出版 ②s3 前图手术 erf→LUT 化算子
  ③研究厂商 LUT 注册机制补 erf 条目。decoder(KV cache+动态形状+Shape/Gather)未试,
  预期边界更靠前

## 环境/工程修复(第二轮)

| 项 | 内容 |
|---|---|
| **ARM 工具链固化** | net_bin_helper.py:202 硬依赖 ARM_C_COMPILER/ARM_CXX_FLAGS/ARM_LD(s6/s8 缺了必崩);厂商 cross_toolchain_setup.sh 的 nnp318p 段指向容器内不存在的 external_toolchain/arm-fhv512c-…-b2 → 实测选定 /opt/molchipv500-armgcc-uclibc(前缀 arm-mol-linux-uclibcgnueabihf- 与厂商 nnp318 段一致,真实 lib.o+wrapper.o 链接验证通过),已写入 env.sh;**TVM_HOME 不要设**(设了触发 arm_multi_thread/*.cc 编译分支,预编译树无 C++ 源码) |
| TIR dump 散落归档 | 根目录 10766 个 *_ir.cc(18MB,11-19 实验期 dump 漏到 cwd;索引 13063-23679 与 osnet s4_build 的 0-13062 连续)→ work/_stray_tir_dump_20261119/;现行 s4 已 chdir 阶段目录,不再复发;.gitignore 已加 *_ir.cc |
| s8 device 过滤 | 见实验 9,stages/s8_sim.py |
| 逐层脚本 | 见实验 7,任意模型可用(NPUP_LAYERWISE_TAG) |

## 第二轮后的下一步

1. v61 逐层归因(进行中,~3h):定位 cosine 0.05 的首个发散层,验证 eodma 拆分 vs 上游 codegen 潜伏缺陷
2. osnet lg0 微实验:Conv_0 fused func 的输入量化/requant 参数 NPU vs CPU 对比
3. v010 A/B/C 决策(等用户拍板)
4. whisper:确认 ANONYMOUS=erf 后,试 tanh-GELU 导出版过 s4
5. RESULTS 第一轮"下一步"仍有效(PM 换页 profile 固件路径、跨芯片 ISS、MAC 利用率换算)

---

## 实验 11:osnet/v61 数值谜团破案 —— s8 输入 dtype 喂送 bug(重大本地 bug,2026-11-22)

**根因**:s8_sim.py 把 s0 的 float32 推理数据直喂 run_iss_wrapper,而量化图的输入
placeholder 是 uint8。原版 golden.py:624 有 `[data.astype(in_dtype) for data in ...]`
(签名默认 in_dtype='uint8'),**我们的 s8 抄 run_on_nnp 模式时漏了这一步**。
NPU 按 model_mem_info 的 `model_input_buf_size`(osnet=0x6000=24576B)只读
float32 字节流(98304B)的前 1/4 —— IEEE754 指数/尾数字节被当 uint8 像素,
NPU 全程计算"确定性伪图像"。首 kernel eidma `data_format=3`(8bit)证实硬件按 uint8 读。

**修复**(stages/s8_sim.py):按 graph.json null 节点的 dltype 对 infer 输入 astype
(float→uint8 = C 截断,与 CPU 侧 TVM 绑定语义一致),带 cast 日志。

**修复后 osnet 逐层终审(全新一致性跑)**:
| 区段 | 修复前 | 修复后 |
|---|---|---|
| lg0(首 conv) | 0.7461 | **1.000000** |
| lg0~142 | median 0.94,最低 0.45 | **158/171 层 ≥0.99,median 0.9992** |
| lg143 InstanceNorm_335 | 64 个 fp16 NaN | **仍 64 个 NaN(真实输入下复现)** |
| lg144~166(NaN 下游) | — | 0.71~0.98(relu 链扩散,部分恢复) |
| lg167 GAP~末层 | — | 0.84→0.75(GAP 混入全通道) |
| 整模型 cosine | 0.8116 | **0.8140(仍被 NaN 层锁死)** |

**osnet 0.812 最终归因(两个独立 bug 叠加)**:
1. 从犯(本地,已修):输入喂送 bug 制造 lg0-142 全线假发散(0.5~0.9),掩盖真信号
2. **主犯(硬件缺陷,实锤)**:NPU InstanceNorm 对零方差输入无保护 → NaN。
   成因链:注意力门控使 2 个通道(184/323)fp16 下溢全零 → var=0 →
   CPU 模拟 `(0-0)/sqrt(0+eps)=0`,NPU 硬件 rsqrt(0)→inf→NaN(64 个)→
   污染下游 29 层 → GAP 混合后 embedding 损毁 → 0.814。
   **硬件设计建议:IN 的 rsqrt 前对 var 做 max(var,eps) 钳位,或输出 NaN flush。**
   注:此为数据依赖缺陷,真实图像输入同样可触发(relu 死亡通道→IN 零方差)。

**连带影响**:
- **v61 cosine 0.051 同因**(图输入 left/right=uint8,s0 产 float32,NPU 读 1MB/4MB
  float 前缀当图像)——**eodma 96×100 拆分补丁被洗清嫌疑**;v61 s8 修复版重跑中,
  预期 cosine 大幅回升,84.2M cycles 与 3.28× 估算比仍有效(与输入数据无关)
- 历史结果有效性:mcv3/yolov5/yolov8n-pose/seg/tiny(原生 uint8 输入)不受影响,全部有效;
  osnet 0.812 与 v61 0.051 两个"精度结论"作废,以修复版为准
- 输入舍入(trunc vs round)探针结论修正:0.81→0.85 的变化是垃圾字节模式改变的副产物,
  非独立因素;修复后输入两侧逐位一致,舍入问题消失

## 实验 10 补充:GELU 单算子探针(2026-11-22)

三个 8 节点以下微型网直测 s4(方法:最小复现):
| 探针 | FusionS | s4 结果 |
|---|---|---|
| X→Erf→Y | ATOMIC_ANONYMOUS | ❌ stitching CHECK 失败(与 whisper 同死点) |
| X→Tanh→Y | ATOMIC_ANONYMOUS | ✅ **通过**,Total Cycles 40512 |
| MatMul+Erf GELU | — | ❌ s2 量化即崩:`batch_matmul.py realize → KeyError 'scale'` |

- **erf 钉死为 whisper 的边界算子**;ANONYMOUS ≠ 必然失败——tanh 同为 ANONYMOUS
  但有 LUT 兜底路径,erf 没有 → **出路①(tanh-GELU 重导出/图手术)可行性实证**
- 顺带发现量化框架 bug:常量权重作 batch_matmul 第二操作数时 realize 阶段
  `output_config` 缺 'scale'(realize.py:94)——whisper 真实图权重走 nn.dense 未触发
