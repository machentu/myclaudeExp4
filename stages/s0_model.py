"""s0_model — 生成测试用小 CNN(ONNX)。

网络结构(特意覆盖多种 NPU 路径):
    data [1,3,32,32] fp32
      ├─ Conv 3→8  k3 s1 p1 ─ Relu          (PE conv,relu epilogue)
      ├─ Conv 8→16 k3 s2 p1 ─ Relu          (PE conv,下采样)
      ├─┬ Conv 16→8 k1                      (1x1 conv 分支 A)
      │ └ Conv 16→8 k1                      (1x1 conv 分支 B)
      ├─ Add                                 (PE eltwise)
      ├─ AvgPool 2x2 s2                     (PE pool)
      ├─ Conv 8→4 k1                        (1x1 conv)
      └─ Sigmoid                             (VU 向量单元路径!)
    输出 [1,4,8,8]

32×32 输入在 nnp318p(PE_NUM=4)上必然触发 DeepEyeTilePartition —— 专门用来看 tile 切分。

产物: s0_model/tiny_cnn.onnx, input_data.npz(定长伪随机校准/推理输入, seed 固定可复现)
"""
import os

import numpy as np
import onnx
from onnx import helper, TensorProto

from stages.common import Ctx, stage_banner, stage_dir, INPUT_NAME, INPUT_SHAPE, TARGET


def _conv(name, in_ch, out_ch, k, stride, pad):
    w = (np.random.randn(out_ch, in_ch, k, k) * 0.05).astype(np.float32)
    b = np.zeros(out_ch, dtype=np.float32)
    init = [
        helper.make_tensor(name + "_w", TensorProto.FLOAT, w.shape, w.flatten()),
        helper.make_tensor(name + "_b", TensorProto.FLOAT, b.shape, b.flatten()),
    ]
    node = helper.make_node(
        "Conv", [name + "_in", name + "_w", name + "_b"], [name + "_out"], name=name,
        kernel_shape=[k, k], strides=[stride, stride], pads=[pad, pad, pad, pad])
    return node, init


def build_model():
    np.random.seed(42)
    nodes, inits = [], []
    n, i = _conv("conv1", 3, 8, 3, 1, 1); n.input[0] = INPUT_NAME; nodes.append(n); inits += i
    nodes.append(helper.make_node("Relu", ["conv1_out"], ["relu1_out"], name="relu1"))
    n, i = _conv("conv2", 8, 16, 3, 2, 1); n.input[0] = "relu1_out"; nodes.append(n); inits += i
    nodes.append(helper.make_node("Relu", ["conv2_out"], ["relu2_out"], name="relu2"))
    n, i = _conv("conv3a", 16, 8, 1, 1, 0); n.input[0] = "relu2_out"; nodes.append(n); inits += i
    n, i = _conv("conv3b", 16, 8, 1, 1, 0); n.input[0] = "relu2_out"; nodes.append(n); inits += i
    nodes.append(helper.make_node("Add", ["conv3a_out", "conv3b_out"], ["add_out"], name="add"))
    nodes.append(helper.make_node("AveragePool", ["add_out"], ["pool_out"], name="pool",
                                  kernel_shape=[2, 2], strides=[2, 2]))
    n, i = _conv("conv4", 8, 4, 1, 1, 0); n.input[0] = "pool_out"; nodes.append(n); inits += i
    nodes.append(helper.make_node("Sigmoid", ["conv4_out"], ["output"], name="sigmoid"))

    graph = helper.make_graph(nodes, "tiny_cnn",
                              [helper.make_tensor_value_info(INPUT_NAME, TensorProto.FLOAT,
                                                             list(INPUT_SHAPE))],
                              [helper.make_tensor_value_info("output", TensorProto.FLOAT,
                                                             [1, 4, 8, 8])],
                              inits)
    return helper.make_model(graph, opset_imports=[helper.make_opsetid("", 11)],
                             producer_name="npu_pipeline_s0")


def run(ctx: Ctx = None, model_path: str = None) -> Ctx:
    if ctx is None:
        ctx = Ctx()
    if not model_path:
        # standalone 重建时恢复上次 --model 指定的外部模型(tag 目录持久化)
        from stages.common import stage_dir
        memo = os.path.join(stage_dir("s0_model"), "model_path.txt")
        if os.path.isfile(memo):
            model_path = open(memo).read().strip()
            if not os.path.isfile(model_path):
                print(f"  [s0] memo 中的模型不存在,退回内置小 CNN: {model_path}")
                model_path = None
    if model_path:
        return _run_external(ctx, model_path)
    stage_banner("s0_model 生成小 CNN ONNX + 伪随机输入")
    d = stage_dir("s0_model")
    model = build_model()
    onnx_path = os.path.join(d, "tiny_cnn.onnx")
    onnx.save(model, onnx_path)

    # 定长输入:校准(多帧)与推理(单帧)共用分布,uint8 模拟摄像头输入
    rng = np.random.RandomState(7)
    calib = [rng.randint(0, 256, INPUT_SHAPE).astype(np.uint8) for _ in range(8)]
    infer = rng.randint(0, 256, INPUT_SHAPE).astype(np.uint8)
    np.savez(os.path.join(d, "input_data.npz"), calib=np.stack(calib), infer=infer)

    ctx.onnx_path = onnx_path
    ctx.calib_frames = {INPUT_NAME: calib}
    ctx.infer_input = {INPUT_NAME: infer}
    print(f"  ONNX  节点数 {len(model.graph.node)}, 产物 {onnx_path}")
    print(f"  校准帧 {len(calib)} × {INPUT_SHAPE} uint8, 推理输入 1 帧")
    return ctx


def _run_external(ctx: Ctx, model_path: str) -> Ctx:
    """使用外部 ONNX 模型(如 /DEngine/projects/mywork/ 下的真实模型)。
    自动读取输入签名,按声明 dtype 生成伪随机校准帧。"""
    stage_banner(f"s0_model 使用外部模型 {os.path.basename(model_path)}")
    import numpy as np
    import onnx
    m = onnx.load(model_path)
    if m.ir_version > 6:
        # ⚠️ 工具链内 graphsurgeon 用 onnxruntime 1.8.1 做形状推理,只认 ir_version≤6;
        # ir7 模型会触发非确定性部分折叠(轻则产出 0 算子退化图,重则 from_onnx 断言崩溃)
        m.ir_version = 6
        norm = os.path.join(stage_dir("s0_model"), "normalized_ir6.onnx")
        onnx.save(m, norm)
        model_path = norm
        print(f"  ir_version {'>6 已规范化为 6(副本)'} → {norm}")
    inits = {i.name for i in m.graph.initializer}
    dtype_map = {1: ("float32", np.float32), 2: ("uint8", np.uint8)}
    specs = []
    for i in m.graph.input:
        if i.name in inits:
            continue
        dims = [d.dim_value for d in i.type.tensor_type.shape.dim]
        dt = dtype_map[i.type.tensor_type.elem_type]
        specs.append((i.name, dims, dt[0], dt[1]))
    assert specs, "模型没有可变输入"

    d = stage_dir("s0_model")
    rng = np.random.RandomState(7)
    calib = {n: [rng.randint(0, 256, shape).astype(np8)
                 if np8 == np.uint8 else (rng.rand(*shape) * 255).astype(np8)
                 for _ in range(8)]
             for n, shape, _, np8 in specs}
    infer = {n: v[0].copy() for n, v in calib.items()}
    np.savez(os.path.join(d, "input_data.npz"),
             **{f"calib_{n}": np.stack(v) for n, v in calib.items()},
             **{f"infer_{n}": v for n, v in infer.items()})

    ctx.onnx_path = os.path.abspath(model_path)
    with open(os.path.join(d, "model_path.txt"), "w") as f:
        f.write(ctx.onnx_path)
    ctx.input_specs = specs                      # [(name, dims, dtype_str, np_dtype)]
    ctx.calib_frames = calib                     # {name: [frame,...]}
    ctx.infer_input = infer                      # {name: frame}
    for n, s, dt, _ in specs:
        print(f"  input {n:12s} {s} {dt}")
    print(f"  校准 8 帧/输入,产物 {d}/input_data.npz")
    return ctx


if __name__ == "__main__":
    import sys
    run(model_path=sys.argv[1] if len(sys.argv) > 1 else None)
