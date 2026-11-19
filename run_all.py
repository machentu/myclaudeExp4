"""端到端驱动:python3.8 run_all.py [--from N] [--to N] [--model xxx.onnx]

单进程串联所有阶段(对象内存直传,文件产物落 work/<chip>/sN_*/ 供检查)。
"""
import os
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__))))

from stages.common import stage_banner, Ctx, work_root
from stages.manifest import Manifest
from stages import (s0_model, s1_front_end, s2_quantize, s3_rewrite, s4_build,
                    s5_tir, s6_netbin, s7_binview, s8_sim)

STAGES = [
    # (阶段, 模块, 描述, 产物路径模板(相对 work 根,用于 hash 溯源))
    ("s0_model", s0_model, "生成/加载 ONNX + 校准数据", "s0_model/tiny_cnn.onnx"),
    ("s1_front_end", s1_front_end, "ONNX → Relay(float32)", "s1_front_end/relay_fp.txt"),
    ("s2_quantize", s2_quantize, "校准 + int8 量化", "s2_quantize/relay_int8.txt"),
    ("s3_rewrite", s3_rewrite, "rewrite_for_deepeye 预览", "s3_rewrite/relay_rewritten.txt"),
    ("s4_build", s4_build, "build_relay:融合/tile/TIR/codegen", "s4_build/graph.json"),
    ("s5_tir", s5_tir, "TIR dump 索引 + intrinsic 演化", "s5_tir/index.md"),
    ("s6_netbin", s6_netbin, "linker → 汇编 → net_bin", "s6_netbin/net.bin"),
    ("s7_binview", s7_binview, "net.bin 结构解析", "s7_binview/net_bin_layout.txt"),
    ("s8_sim", s8_sim, "ISS 周期级仿真 + 比对", "s8_sim/compare.txt"),
]


def main():
    args = sys.argv[1:]
    model_path = None
    lo, hi = 0, len(STAGES) - 1
    if "--model" in args:
        model_path = args[args.index("--model") + 1]
        tag = os.path.splitext(os.path.basename(model_path))[0]
        os.environ["NPUP_RUN_TAG"] = tag          # 按模型隔离 work 目录
        print(f"[run_all] 模型 {model_path} → work/<chip>/{tag}/")
    if "--from" in args:
        lo = int(args[args.index("--from") + 1])
    if "--to" in args:
        hi = int(args[args.index("--to") + 1])

    ctx = Ctx()
    mf = Manifest(os.path.join(work_root(), "manifest.json"))
    t0 = time.time()
    for i, (name, mod, desc, artifact) in enumerate(STAGES):
        if not (lo <= i <= hi):
            continue
        ts = time.time()
        if name == "s0_model" and model_path:
            ctx = mod.run(ctx, model_path=model_path)
        else:
            ctx = mod.run(ctx)
        mf.record(name, desc, time.time() - ts,
                  [os.path.join(work_root(), artifact)], ctx.get("metrics", {}))
    stage_banner(f"流水线完成,总耗时 {time.time() - t0:.1f}s")
    print(f"  产物目录: {os.path.join(os.path.dirname(os.path.abspath(__file__)), 'work')}")
    print(f"  清单: {mf.path}(含各阶段产物 SHA256,对比两次运行可定位不确定性)")


if __name__ == "__main__":
    main()
