"""s4_build — relay.build:融合/切分落地/tile 搜索/TIR 降级/codegen。

调用厂商原版 build_relay(保持 rewrite+build 的隐性耦合不被拆散,见 s3 头注释),
通过 monkeypatch tvm.build_config 给 build_config_nnp300 的配置注入
dump_pass_ir=True(NPUP_DUMP_TIR=0 关闭),让每个 TIR pass 前后的 IR
落盘到本阶段目录(<N>_<pass>_ir.cc,由 s5 索引)。

厂商内部流水线(黑盒,靠 dump 观察):
  relay.build → C++ RelayBuildModule::BuildRelay(src/relay/backend/build_module.cc:725)
    1. Optimize()             TVM 原生优化(FoldConstant×2, LegalizeOps)
    2. RunDeviceAnnotationPass ARMAnnotate 注解落地,边界插 device_copy
    3. DeepEyeFusionStitching ★替换原生 FuseOps:AtomicGraph(34 类)→
       FusionPlanGenerator → ILP 整数线性规划分组(fusion_stitching.cc:2489)
    4. CompileEngine 逐融合组:schedule_grouped_ops(tile/hw_cut 搜索空间枚举 +
       周期级代价模型选优,schedule_relay_nnp300.py:3070)
    5. lower() = TIR 50+ pass 四阶段(phase0:TilePartition/vu_tensorize;
       phase2:DMA intrinsics 注入;phase3:barrier/obuf/DM 内存分配)
    6. tvm.build → codegen.build_nnp300(C++)→ Python genasm 回调 → .asm

dump 产物(debug=True):
  fused_func.json / graph.json / fused.model(Netron 可开)+ NN_*_ir.cc(TIR 逐 pass)
"""
import os

from stages.common import Ctx, stage_banner, stage_dir, ensure, TARGET


def run(ctx: Ctx = None) -> Ctx:
    if ctx is None:
        ctx = Ctx()
    func = ensure(ctx, "s2_quantize", "relay_int8")
    sub = TARGET
    d = stage_dir("s4_build")
    stage_banner(f"s4_build build_relay(rewrite+融合ILP+tile搜索+TIR+codegen) → {d}")

    import tvm
    from deepeye.build_module import build_relay

    dump_tir = os.environ.get("NPUP_DUMP_TIR", "1") == "1"
    real_build_config = tvm.build_config

    def patched_build_config(**kwargs):
        if "add_lower_pass" in kwargs:          # build_config_nnp300 的调用
            kwargs.setdefault("dump_pass_ir", dump_tir)
        return real_build_config(**kwargs)

    cwd = os.getcwd()
    os.chdir(d)   # dump_pass_ir 与 debug 产物落在本阶段目录
    for old in os.listdir("."):           # 清上次运行的 TIR dump,避免跨次累积
        if old.endswith("_ir.cc"):
            os.remove(old)
    try:
        tvm.build_config = patched_build_config
        import deepeye.build_module.build_module as _bm
        _bm.tvm.build_config = patched_build_config
        graph, lib, params = build_relay(func, target=sub, debug=True, work_dir=d)
    finally:
        _bm.tvm.build_config = real_build_config
        tvm.build_config = real_build_config
        os.chdir(cwd)

    with open(os.path.join(d, "graph.json"), "w") as f:
        f.write(graph if isinstance(graph, str) else graph.graph_json)
    n_ir = len([f for f in os.listdir(d) if f.endswith("_ir.cc")])
    dumps = [f for f in os.listdir(d) if f in ("fused_func.json", "fused.model")]
    print(f"  graph.json 已存;TIR dump 文件 {n_ir} 个;debug 产物: {dumps}")

    ctx.graph = graph
    ctx.lib = lib
    ctx.params_out = params
    ctx.build_dir = d
    return ctx


if __name__ == "__main__":
    run()
