# TyTVM v1.0.41 前端链路分析(模型文件 → Relay → 量化 → build 交接)

> 探查代理报告,2026-11-18。`$SRC = /DEngine/projects/TyTVM_NNP3xx_src_v1.0.41/tytvm_src_pkg`

## 0. 结论速览

- 对外总入口不是 `intellif_tvm.py`(那是早期 nnp200/MXNet 门面),而是两条:
  - **测试/量产批处理线**:`deepeye.testing.onchip_netbin.genbins_*_model_quantize()`(ONNX/Caffe/TF/TFLite/MXNet/PyTorch 全走这里)
  - **裸 Python API 线**(README 官方示例):`relay.frontend.from_onnx/from_caffe` → `tvm.relay.quantization.quantize()` → `deepeye.make_netbin()`
- **量化是独立 pass,发生在 `build_relay` 之前**:量化吃 float32 Relay 图 + 校准图片目录,吐"已实化"(realized)的 int8 Relay 图(`multiply→round→clip→cast(int8)` + 整数权重常量 + `right_shift` 链),然后才交给 `deepeye.build_module.build_relay()`(中段)。
- 量化框架本体 `$SRC/python/tvm/relay/quantization/` 是**独立 git 子模块**,同时兼容老/新两代 TVM API。
- decaffe 是完整 BVLC Caffe fork(子模块),**不参与编译**;编译侧 Caffe 解析用 `tvm/relay/frontend/caffe_pb2.py`。decaffe 的 pycaffe 只被 `deepeye/testing/run_caffe.py` import,用来跑原始模型产出 golden 参考输出。

## 1. 端到端入口(最小示例)

### 1a. 官方 README 示例
`$SRC/deepeye/README.md:35`:`python3 example/deepeye/squeezenet_genbins.py onnx`

**文件:`$SRC/example/deepeye/squeezenet_genbins.py`**:
- :139-158 ONNX 分支:`onnx.load(onnx_file)` → `relay.frontend.from_onnx(model, shape_dict, dtype_dict)`
- :208-248 Step1 量化:`relay.quantization.get_quantize_config(target, input_dtypes)`(:230)→ `from tvm.relay.quantization import quantize; quantize(sym, params, dataset=图片目录, prof_img_num, rgb_en, norm={mean,std,axis}, quantize_config, debug_level, save_dir)`(:241)
- :251 `deepeye.save(relay_func, "relay.model")`
- :271 `deepeye.eval_relay(...)` CPU 参考输出(`deepeye/run.py:27`,内部 `rewrite_for_cpu` + LLVM `relay.build` + graph_runtime)
- :291 **Step2 Build:`deepeye.make_netbin(chip_relay_func, target, chip_model_path, input_info, debug_level=1)`**(`deepeye/make.py:44`;内部 :146 `build_relay(...)` + :155 `net_bin(...)`)
- :317 `deepeye.run_net_bin(netbin_file, in_datas, ...)` 上 ISS 仿真对比 cosine 相似度

产物(chip_model_path 下):`net_combine.bin`、`model_info.json`、`quan.json`、`relay_quant.model`、`data_*.bin`、`similarity.txt`、`fused.model`(Netron 可开)等。

### 1b. 批处理线(量产/回归)
- 驱动:`$SRC/model_evaluate_script/run.py:31-68` — 多进程跑 `test_tvm_quantize_<model>.py <target>` 脚本
- 单模型脚本模板:`$SRC/deepeye/tests/python/onchip/genbins/onnx_quantize/test_tvm_quantize_onnx_*.py`
  - :51-57 一行调用 `genbins_onnx_model_quantize(model_file, save_path, folder_name, img_path, mean, scale, shape, dtype, ..., nnp_dev=target, ...)`
  - 该目录共 **264 个**模型脚本
- `deepeye/apps/tsim_example/` 是 VTA 风格硬件仿真,**与 NN 前端无关**

## 2. 用户视角调用链(以 genbins_onnx_model_quantize 为例)

`$SRC/deepeye/python/deepeye/testing/onchip_netbin.py`:

1. **加载模型** :1279-1280 `onnx.load(model_file)`
2. **→ Relay** :1308 `relay.frontend.from_onnx(model, shape_dict, dtype_dict, out_names, add_nms, nms_info_config, target=nnp_dev)`
3. **布局/前端后处理** :1320 `relay.relay_pass.onnx_frontend_convert(sym, params, shape_dict, convert_input_as_nchw)`
4. **量化配置** :1357 `quantize_config = relay.quantization.get_quantize_config(nnp_dev, net_in_dtype)`(`quantization/tools.py:89`);:1388-1393 calib_method 覆盖(nnp200 强制 min_max,nnp3xx 可 kld)
5. **量化** `relay_model_test()` :456:nnp3xx 走 :514-522 `from tvm.relay.quantization import quantize; quantize(...)`
6. **CPU 参考输出**(可选): :657 `rewrite_for_cpu` → :663 LLVM `relay.build` → graph_runtime
7. **★交接点(前端→中段)** :707-711
   `from deepeye.build_module import build_relay; graph, lib, params = build_relay(chip_relay_func, target=nnp_dev, target_host='deck', debug=True, work_dir=chip_model_path)`
8. **出 bin** :720/:743 `net_bin(chip_model_path, graph, lib, params, ...)`

其余 `genbins_*_model_quantize`(caffe/tf/tflite/pytorch/mxnet)结构完全同构。

## 3. 量化链路详解

### 3a. deepeye_quantize/ 仅 2 文件 = 遗留外壳
`deepeye_quantize.py:129` 只是 `os.system` 调 `deepeye/demxnet_quantization/` 下的脚本(该目录不在包内,是早期 MXNet 外部量化工具残迹)。当前流程不 import 它。

### 3b. 真正的量化框架:`$SRC/python/tvm/relay/quantization/`(git 子模块,~30 文件)

入口 `quantize.py:184 quantize(sym, params, model_name, dataset, ...)`:
- :233-249 绑定 params 为常量;dataset 可为图片目录 str / `lambda: iter([{name: array}])` / DataLoader
- :384-409 建 `QuantizeSearch` 并 `optimize()`
- :441-447 `gen_name_and_config()`(get_name → **opt_ir.pdf** 图 dump)+ `get_default_config()` + `quantize(config)`

`Quantize` 类(quantize.py:44)五步流水线:
1. **analyze_graph**(analyze.py:741)→ 每个 Call 一个 `VertexConfig`
2. **collect_stats**(collect.py:342):把待观测中间张量包成临时 Function 输出,`:313 _get_node_runtime` 用 **LLVM(target='llvm')graph_runtime 在 host CPU 上跑 float 模型**,逐 batch 喂校准图,对每个 node 记录 min/max(或直方图)
3. **calibrate_params**(calibrate.py:334):逐节点定 scale/zero_point;`recursive_set`(:38)对分叉/相邻节点 scale 递归同步
4. **realize_graph**(realize.py:1347):`_quantize_shift`(:371)把 Q 算子变成 `cast(fp16)→multiply(1/scale)→round→clip→cast(int8)`(激活)或直接烘成 int 常量(权重,per-channel scale,zero_point 通常 0,即**对称量化**);`_requantize_shift`(:670)生成 `multiply + round_right_shift/right_shift` 链匹配 NPU 语义(activation 24bit、bias 24bit)
5. **post_process**(post_process.py:29):remove_and_fuse、sync_outputs_dtype、FoldConstantQuantize 等

**int8 算法要点**(config 默认,tools.py:99-139):nbit_input/weight=8(int8),bias/activation=24(int32 存储),`perchannel: True`(权重 per-channel,激活 per-tensor),`pow2scale: False`,calib_method 默认 "l2norm"(可 kld/percentile_0.99/mse,threshold.py:1623 Threshold 类注册 MinMax/MovingAverageMinMax/Percentile/L2Norm/RelativeEntropy/KLDAbs/MSE 等)。

### 3c. 老 nnp200 量化(对照)
`$SRC/python/tvm/relay/quantize/quantize.py`:TVM 0.6 annotate/realize 框架大改版,`quantize()` :1445、`optimize` :765、`qconfig` :257、`save/load_deepeye_quan_model` :1948/1964。

### 3d. intellif_quantize_rewrite.py = 非校准量化
`relay_pass/intellif_quantize_rewrite.py:460`:`convert_in_dtype(uint8)` → `schedule_op_on_dsp` → `RightshiftMerge`(:33)→ `FuseMultiplyAddFloorToRoundRightShift`(:176)→ `ConvertFloatToInt`(:275)→ 后续优化。**定位**:假设模型已被外部工具预量化(pow2 scale),只做"浮点伪量化图 → 整数 right_shift IR"重写。调用方:`intellif_tvm.py:48`、`onchip_netbin.py:1663`。

## 4. intellif_tvm.py 定位

**早期(nnp200+MXNet)facade,非当前主入口**。`build_mxnet_model` :34:from_mxnet → convert_in_dtype → intellif_quantize_rewrite → build_relay → net_bin。当前无代码 import 它;真正 facade 是 `deepeye/__init__.py`。

## 5. ONNX 路径 & decaffe 角色

- **onnx.py 被深度改写**:8220 行(上游 0.6 约 2-3 千行);`from_onnx(model, shape, dtype, opset, freeze_params, out_names, is_qir, add_nms, nms_info_config, target="nnp300")` :8086 —— target/add_nms/nms_info_config 全是 deepeye 私货;convert map ~200 op(上游约 70);:7868-7973 `RegisterNms`(把 NMS 后处理直接编进图)
- caffe 前端 **deepeye 全新**:`frontend/caffe.py:2310 from_caffe`,convert_map 含 Intellifusion 私货层:proposal/PSROIPooling/PriorBox/Python 自定义层/RRC/SSD 系列
- 其它前端:`frontend/qir/qir.py from_qir`(QIR-ONNX 变体)、`build_qnn.py:96 from_qnn()`(输入已是 QDQ 量化模型的免校准通道)

## 6. 中间表示与 dump 手段

| 产物 | 位置 |
|---|---|
| `opt_ir.pdf`(graphviz IR 结构图) | quantization/name.py:78-122 |
| `origin_mod.json` / `post_processed_mod.json` | search.py:204-216 / :354-359 |
| `quan.json`(量化模型可回读) | save_deepeye_quan_model, quantize/quantize.py:1948 |
| `relay_quant.model` / `relay.model` | `deepeye.save`(util/util.py:245) |
| build 后 `fused.model`(Netron 可开) | make_netbin/debug_level=1 |
| 逐层输出 pickle、`dump_func_output` | relay_pass/run_function_fused.py:337;deepeye/debug.py:33-182 |

## 7. 文件归属:deepeye 新增 vs TVM 原有

**deepeye 新增**:`$SRC/deepeye/` 整树、`model_evaluate_script/`、`example/deepeye/`、`python/tvm/relay/quantization/`(子模块)、`frontend/{caffe.py,caffe_pb2.py,qir/,dsp/}`、`relay/{custom_op/,qnn/,qir/,relay_pass/}`(qnn 非上游 QNN,是自家 QDQ 处理)
**TVM 原有、被大幅修改**:`frontend/onnx.py`(重写级)、`frontend/common.py`、`tvm/relay/quantize/`(从 ~700 行涨到 ~2000 行)
**基本未动**:mxnet/tensorflow/tflite/keras/coreml/caffe2/pytorch 前端
