# TyTVM v1.0.41 编译中段 pass 流水线分析(Relay → TIR)

> 探查代理报告,2026-11-18。`$SRC = /DEngine/projects/TyTVM_NNP3xx_src_v1.0.41/tytvm_src_pkg`

## 0. 总控制流

```
build_relay()                                            deepeye/build_module/build_module.py:34
 ├─ rewrite_for_deepeye(func, params, target)           :50   [relay 前处理 ~60 个 pass,deepeye_rewrite.py:5146]
 ├─ DeleteDedspOnDevice().run(func)                     :59
 ├─ relay.build_config(opt_level=2, disable_pass=['SimplifyInference'],
 │                      fallback_device=nnp300, debug_build=debug)
 ├─ build_config_nnp300(opt_level=1)                    :184  [注册 TIR 段 add_lower_pass phase0-3,行 218-304]
 └─ relay.build(func, {"nnp300":…, "llvm":arm_opts}, target_host, params)  :65
      ↓ C++ RelayBuildModule::BuildRelay                 src/relay/backend/build_module.cc:725
        ├─ Optimize()                                     :735 → cc:420   (TVM 原生 relay 优化)
        ├─ RunDeviceAnnotationPass()                      :737 → cc:533   (host/NPU 切分落地)
        ├─ DeepEyeFusionStitching (替换 FuseOps)          cc:746-752
        ├─ GraphRuntimeCodegen → CompileEngine            cc:761-763
        │    每 fused func: FTVMCompute → schedule_grouped_ops → relay.backend.lower → tvm.lower(TIR phase0-3)
        └─ tvm.build(lowered_funcs, target_host)          cc:815
```

## 1. Relay 段 pass 流水线表

### 阶段 A:deepeye 前处理(rewrite_for_deepeye,deepeye_rewrite.py:5146-5289)

| # | pass | 行 | 职责 |
|---|------|------|------|
| 1 | DeleteDedspOnDevice | :5150;delete_on_device.py:50 | 删 dedsp 的 on_device 注解 |
| 2 | ExtractParamsPass(无 params 时) | :5155;extract_params.py:29 | 内联 Constant 提为参数 ★产出 nn.conv2d_N_arg_1 命名 |
| 3 | RemoveUselessParamPass | :5156 | 删未引用参数 |
| 4 | MoveRightShiftBeforeRelu | :5158 | 量化位移动重排 |
| 5 | RecoverRoundRightShift | :2165 | add+floor 恢复 round_right_shift |
| 6 | FocusOpt | :5166 | 焦点网络定制 |
| 7 | OpTransform | :5172 | 算子变形大杂烩(~600 行 visit_call) |
| 8 | SplitGroupConv | :5175 | 拆 group conv |
| 9 | LUTConvert / sigmoid / erf / yolov5 后处理 | :5176-5178 | sigmoid/tanh→LUT |
| 10 | SoftmaxConvert 等(仅 nnp316/318/318p) | :5189-5191 | softmax→exp/sum;erf→LUT |
| 11-16 | LeagalizeSpaceOps / ConvertReorgToConv / SplitBigConv / RemoveLayoutAttr / ConvertUpsamplingToConv / FuseStridedSlice | :5192-5214 | 空间算子合法化/reorg→conv/拆大conv/上采样→deconv |
| 17-23 | ElementWiseBroadcast / RelayOptimize / RemoveUselessClip / RemoveBatchflatten / DelayBatchflatten / Eltwise 系列 | :5220-5242 | 通用优化+eltwise 合法化到 NPU 原语 |
| 24 | **ARMAnnotate(nnp300)** | :5243;类 :3401 | **host/NPU 切分标注**(见 §2) |
| 25-26 | ConvertdenseToBatch_matmul / ConvertFCToConv | :5246-5248 | dense/FC → conv1x1 |
| 27 | Convert2D3DTo4D / ConvertHDTo4D / RemoveReshapeAndBatchflatten | :5251-5258 | 统一 4D/5D NPU 布局 |
| 28-31 | 零散修补 / convert_mul_to_conv2dweight / OptConcatShape(nnp318p) / 收尾清理 | :5259-5283 | |

旁路文件:`schedule_on_device.py`(nnp200 专用 DSP 切分)、`cpu_rewrite.py`(CPU golden 用)、`run_function_fused.py`(逐层 dump 调试)、`intellif_quantize_rewrite.py`(外部预量化图用)、`gen_subfunction.py`(子图切取)。

### 阶段 B:TVM 原生 relay 优化(C++ Optimize,cc:420-490)

opt_level=2 下:BindParamsByName ✅、LegalizeOps ✅、SimplifyInference ❌(显式禁用)、EliminateCommonSubexpr ❌(≥3)、FoldConstant ✅×2、FoldScaleAxis ❌、CanonicalizeOps ❌、AlterOpLayout ❌。

### 阶段 C:host/NPU 切分(cc:533)

`relay._ir_pass.RewriteDeviceAnnotation`(device_annotation.cc):fallback_device=nnp300,未标注算子归 NPU,边界插 device_copy。

### 阶段 D:算子融合(自定义 fuser,cc:741-758)

`relay._ir_pass.DeepEyeFusionStitching`(src/relay/pass/deepeye_fusion_stitching.cc:2489):
1. DeepEyeDependencyGraph 2. AtomicGraph(:91,34 类原子算子,带设备属性)3. FusionPlanGenerator 4. **ILPFusionPlanning().Run(:2530)** — 整数线性规划做性能搜索分组 5. SubFunctionFuser 包成带 `Grouped` attr 的 fused Function。

### 阶段 E:graph codegen + 逐函数 TIR 化(cc:761-763)

- `ScheduleGetter::Create`(compile_engine.cc:150-239):逐算子 `FTVMCompute` → topi GenericFunc 按 target('nnp300')分派;`Grouped` attr==1 → `relay.schedule_grouped_ops`(**tile/hw_cut 决策在这里**)
- `LowerInternal`(cc:728-819):20 次 retry(处理 DM Alloc fail)→ Python `relay.backend.lower` → `tvm.build_module.lower`(:337)= TIR pass 链(§4)

## 2. host/NPU 切分机制

- **决策**:ARMAnnotate(deepeye_rewrite.py:3401)逐算子规则判定:float32/int32 输入且不在 NPU 白名单 → ARM;`contrib.custom.*` → ARM;sum/variance/mean 超过 DM_SIZE pingpong 容量(:3469-3497)→ ARM。**纯规则,无代价模型**。
- **落地**:TVM 原生 device annotation;FusionStitching 保证融合组不跨设备;device_copy 子函数由 graph runtime 做跨设备拷贝。

## 3. 算子注册(top/nnp300/)

- 注册机制:`@autotvm.register_topi_compute(topi.nn.xxx, 'nnp300', 'direct')` + `tvm.register_func("relay.schedule_grouped_ops")`(schedule_relay_common.py:66-88)
- **CLT/主引擎算子**(compute_relay_nnp300.py,44 个注册):conv2d/conv3d/depthwise×2/group_conv、winograd1d×4、bias_add、dense、pool_new/pool3d/adaptive×2、pad/prelu/leaky_relu/relu/right_shift/cast/batch_flatten/spatial_trans/unmaxpool_upsample/sigmoid、softmax 系列、mean/variance/argmax/topk/strided_slice、resize 系列、vision 全家(nms/proposal/psroi/roi_align/reorg/multibox)
- **VU 算子**(compute_relay_nnp_vu.py,27 个):vision 全家、softmax/lrn/l2_normalize、argmax/mean/topk、resize、sigmoid、adaptive pool 等(由 `vu_tensorize` 打成 nnp_vu intrinsic)
- **schedule/搜索空间**(schedule_relay_nnp300.py):`schedule_grouped_ops`(:3070)转 NNPGraph → autotvm template → `get_new_hw_cut_and_tile` 枚举 hw_cut/tile → estimate_cost_nnp300.py 代价评估选优 → `schedule_ops`(:1144)生成带 pragma 的 schedule → **tile 切分参数的来源**

## 4. TIR pass 链(lower() 内完整顺序)

pass 表注册在 `build_config_nnp300`(build_module.py:218-304);phase 执行点:phase0 @ build_module.py:393、phase1 @ :400、phase2 @ :419、phase3 @ :427。

| 序 | pass | 实现位置 | 职责 |
|---|---|---|---|
| P0.0 | form_body | python/tvm/build_module.py:315-334 | schedule→raw Stmt |
| P0.1 | DeepEyeEvaluateLoop1 | src/pass/nnp300 | 循环结构规整 |
| P0.2 | Simplify | TVM | 算术化简 |
| **P0.3** | **DeepEyeTilePartition_nnp300** | deepeye_tile_partition_nnp300.cc | **按 pragma 做循环 tile 切分** |
| P0.4-0.5 | Simplify/CanonicalSimplify/RemoveNoOp | TVM | 化简 |
| P0.6 | vu_tensorize | ir_pass/nnp300/vu_tensorize.py:54 | VU 算子 tensorize 成 nnp_vu intrinsic |
| — | StorageFlatten + DeepEyeInjectArgInfo + CanonicalSimplify | deepeye C++ | buffer 平坦化;注入参数信息 |
| P1.1-1.4 | LoopPartition(const) / Simplify / RemoveNoOp / DeepEyeSimplify / DeepEyeTensorTrimmer | TVM+deepeye | 循环切分/裁剪无效计算 |
| — | VectorizeLoop / InjectVirtualThread / InjectDoubleBuffer / UnrollLoop(StorageRewrite 被 disable_rewriting 跳过) | TVM | 多为 no-op 形态 |
| P2.1 | Simplify | TVM | — |
| **P2.2** | **inject_dma_intrin_nnp300** | ir_pass/nnp300/inject_dma_intrin_nnp300.py:31 | **iobuf↔dm 拷贝循环 → nnp_idma/eidma/odma/eodma DMA intrinsic** |
| P2.3 | inject_pad_intrin_nnp300 | 同文件 :397 | pad 模式→DMA pad 指令 |
| P2.4-2.5 | DeepEyeRemoveNoStore / RemoveNoOp | deepeye/TVM | 删无 store 死循环 |
| **P2.6** | **inject_ops_intrin_nnp300** | inject_ops_intrin_nnp300.py:29 | **计算循环 → nnp_conv2d/eltwise 等 CLT intrinsic(几何反推 :317)** |
| P2.7-2.8 | eidma_idma_shape_update / eidma_buffer_handler | eidma_buffer_handler.py:148/:24 | eidma 形状修正/pingpong 缓冲 |
| P2.9-2.11 | DeepEyeLiftAuxOp / merge_intrin_nnp300 / DeepEyeLiftEbdma | deepeye + merge_intrin_nnp300.py:27 | 辅助 op 提升/相邻 intrinsic 合并 |
| P2.12-2.16 | intrinsic_legalize / update_data_type / label_concat_k11 / update_concat / update_dma_icp_mode / conv_kernel1x1_reshape | ir_pass/nnp300 同名文件 | 合法化/int4-int12 打包/concat 优化/1x1 reshape |
| P2.17-2.22 | DeepEyeDropEidmaIdma / DeepEyeLiftEodma / eidma_idma_handshak_handler / dep_sync_nnp300 / update_winograd_nnp300 / update_layout_nnp300 | deepeye + ir_pass/nnp300 | 冗余 DMA 删除/握手依赖/同步标记/winograd 变体/layout 改写 |
| P2.23-2.27 | update_dma_info / update_odma_info / reorder_idma / update_wbdma_cb / merge_ewdma | ir_pass/nnp300 同名 | DMA 描述符字段填充(addr/stride/cb) |
| P2.28-2.29 | merge_nnp_vu_func_nodes / check_params_nnp300 | merge_nnp_vu_func_nodes.py:25 | VU intrinsic 合并成 VU 函数/参数校验 |
| — | Simplify / LowerStorageAccessInfo / RemoveNoOp / RewriteUnsafeSelect | TVM | 收尾 |
| P3.1 | fold_uop_loop | ir_pass/fold_uop_loop.py | 循环折叠成 uop 编程模型 AST |
| P3.2-3.5 | DecorateDeviceScope / DeepEyeBarrierSetup_nnp300 / DeepEyeObufReuseRewrite / DeepEyeRoundRewrite | TVM + src/pass/nnp300 | device scope/硬件 barrier/obuf 复用/多轮 tile |
| P3.6-3.8 | replicate_idma_nnp300 / ewdma_ebdma_buffer_handler / update_eidma_inround | ir_pass/nnp300 | 多输入复制/缓冲/round 内 eidma |
| **P3.9** | **DeepEyeStorageRewrite_nnp300** | deepeye_storage_rewrite_nnp300.cc | **DM/CM 内存管理分配** |
| P3.10-3.12 | update_eidma_idma_transfer / DeepEyeLiftOdma / update_odma_bdma_info_nnp300 | ir_pass + deepeye | 传输修正/odma 提升 |
| P3.13-3.16 | DeepEyeInjectAsmCutPoint / DeepEyeCleanLoop1 / Simplify / intrinsic_legalize_phase2 / verify_ir_nnp300 | deepeye + ir_pass | 汇编切点注入/最终校验 |

注:perf_mode=True 时 phase3 整段不执行;ARM/llvm target 的函数跳过全部自定义 phase。

## 5. 中间 IR dump 点

| 阶段 | 机制 | 位置 | 输出 |
|---|---|---|---|
| relay 优化前后 | logger 'deepeye' INFO | build_module.py:49 | 日志 |
| FusionS 前后 | relay_info_printer / relay_printer | cc:743/:754;debug.py:42/:164 | stdout/日志 |
| 每个 fused 函数 | relay_info_printer(nnp300 必调) | compile_engine.cc:766-768 | 日志 |
| fused 图+graph json | `build_relay(debug=True)` → relay_debug_dump | debug.py:47 | work_dir 下 fused_func.json、graph.json、fused.model |
| **TIR 每 pass** | `dump_pass_ir=True`(DumpIR 装饰所有 ir_pass) | python/tvm/build_module.py:41-115、:374 | **CWD 下 `NN_passname_ir.cc` 逐 pass 文件** ★npu_pipeline s5 的数据源 |
| schedule/tuning 记录 | Environment(work_dir) | environment.py:240-252 | autotvm.log/perf_data.txt |
| 逐层输出比对 | run_function_fused / dump_func_output | run_function_fused.py:318/:335 | 逐层 bin |

## 6. 模块化重构关键交接点

1. `build_relay` :50 → `rewrite_for_deepeye` :5146(输入 relay.Function+params;输出同型)★但注意:与 relay.build 有隐性参数命名耦合,拆开单独调用会崩(见 npu_pipeline/stages/s3_rewrite.py 头注释)
2. `relay.build` :65 → C++ BuildRelay cc:725
3. relay→schedule→TIR 边界:GraphRuntimeCodegen cc:763 → CompileEngine LowerInternal cc:728 → ScheduleGetter cc:150(FTVMCompute 分派 cc:326,Grouped 分支 cc:210-224)→ Python `relay.backend.lower`(_backend.py:26)→ `tvm.build_module.lower`(:337)
4. **TIR pass 注册表**:`build_config_nnp300` build_module.py:218-304 —— 重构 TIR 段只需重排/替换这张表
5. tile 切分双源头:schedule 参数(NNPGraph 搜索)以 pragma 附在 compute 上;真正循环切分在 P0.3 DeepEyeTilePartition_nnp300。两者必须成对迁移。
