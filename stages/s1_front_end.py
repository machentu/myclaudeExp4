"""s1_front_end — ONNX → Relay IR。

厂商对应代码:
  - python/tvm/relay/frontend/onnx.py:8086  from_onnx()(被 deepeye 深度改写,8220 行,
    convert map ~200 算子,含 QLinearConv/NMS/GridSample 等私货)
  - onchip_netbin.py:1308 同样的调用方式(target=nnp_dev)
Relay 文本落盘: relay_fp.txt(float32 图,量化前)
"""
import os
import sys

from stages.common import Ctx, stage_banner, stage_dir, ensure, INPUT_NAME, INPUT_SHAPE, TARGET


def run(ctx: Ctx = None) -> Ctx:
    if ctx is None:
        ctx = Ctx()
    onnx_path = ensure(ctx, "s0_model", "onnx_path")
    stage_banner("s1_front_end ONNX → Relay(float32)")
    import onnx
    from tvm import relay

    model = onnx.load(onnx_path)
    specs = ctx.get("input_specs")
    if specs:
        shape_dict = {n: dims for n, dims, _, _ in specs}
        dtype_dict = {n: dt for n, _, dt, _ in specs}
    else:
        shape_dict = {INPUT_NAME: list(INPUT_SHAPE)}
        dtype_dict = {INPUT_NAME: "float32"}

    # ⚠️ 禁用 from_onnx 内的 graphsurgeon 预处理:容器内 onnxruntime 1.8.1 对较新模型
    # 推理必失败 → gs 部分折叠非确定性(同一命令两次结果不同:轻则静默退化图,重则
    # "Input names don't match" 断言;ir6 的 v61 也中招)。厂商代码在 gs 不可导入时
    # 会干净回退到原始模型路径,故通过临时隐藏该模块实现确定性转换。
    # 需要 gs 时设 NPUP_USE_GS=1。
    use_gs = os.environ.get("NPUP_USE_GS", "0") == "1"
    hidden = None
    if not use_gs and "onnx_graphsurgeon" in sys.modules:
        hidden = sys.modules.pop("onnx_graphsurgeon")
    import builtins
    real_import = builtins.__import__

    def guarded_import(name, *a, **k):
        if name == "onnx_graphsurgeon" and not use_gs:
            raise ModuleNotFoundError("gs disabled by npu_pipeline (determinism)")
        return real_import(name, *a, **k)

    builtins.__import__ = guarded_import
    try:
        sym, params = relay.frontend.from_onnx(model, shape_dict, dtype_dict,
                                               target=TARGET)
    finally:
        builtins.__import__ = real_import
        if hidden is not None:
            sys.modules["onnx_graphsurgeon"] = hidden
    sym = relay.ir_pass.infer_type(sym)

    d = stage_dir("s1_front_end")
    text = sym.astext(show_meta_data=False)
    with open(os.path.join(d, "relay_fp.txt"), "w") as f:
        f.write(text)
    from collections import Counter
    import re
    calls = Counter(m.group(1) for m in re.finditer(r"= ([\w.]+)\(", text))
    print(f"  Relay 节点类型统计:")
    for k, v in sorted(calls.items()):
        print(f"    {k:30s} × {v}")
    print(f"  params: {len(params)} 个,产物 {d}/relay_fp.txt")

    ctx.relay_fp = sym
    ctx.params_fp = params
    return ctx


if __name__ == "__main__":
    run()
