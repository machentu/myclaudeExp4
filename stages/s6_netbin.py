"""s6_netbin — lib → linker → 汇编 → 打包 net_bin。

厂商对应代码(后段总链):
  deepeye/python/deepeye/net_bin.py:183  net_bin() 入口
    └ nnp3_net_bin.py:39  nnp3_net_bin()
        └ lib.export_library(..., linker=deepeye.linker.linker.linker)  触发:
          1. lto.py:4249 lto()     解析 codegen .asm → 图节点重组/标签重命名;
                                    权重→nnp.models(256bit 行 hex);
                                    ★ddr_manage.py:59 ddr_manage() DDR 内存规划器
                                    (中间缓冲 lifetime 感知复用 → model_mem_info.json);
                                    地址回填(DMA 地址在 codegen 是占位符,linker 定位)
          2. ass_nnp3xx/ass.py     汇编器(PLY 手写): .asm → cpp 预处理 → .lst + .hex
                                    (hex 每行 256bit VLIW 指令包 + PC)
          3. model_hex_l2ld.py     PM(程序存储器)容量不足时的二级装载切分
                                    (nnp318p PM 只有 1024 行,大模型必然触发)
          4. link_info_gen         代码长度/中缓大小/权重大小 → link_info
        → net.bin + model.bin + libarm.so + net_combine.bin

产物(本阶段目录 linker_results/ 下):
  asm_work/nnp_main.asm|hex|lst   汇编产物   model_asm/*.asm  每算子汇编
  nnp.models                      权重 hex   link_info        链接信息
  net.bin / model.bin / net_combine.bin      部署二进制
  model_mem_info.json             DDR 规划   model_profile.json 性能/内存报表
"""
import os

from stages.common import Ctx, stage_banner, stage_dir, ensure, TARGET


def run(ctx: Ctx = None) -> Ctx:
    if ctx is None:
        ctx = Ctx()
    graph = ensure(ctx, "s4_build", "graph")
    lib = ctx.get("lib") or ensure(ctx, "s4_build", "lib")
    params = ctx.get("params_out") or ensure(ctx, "s4_build", "params_out")
    infer = ensure(ctx, "s0_model", "infer_input")
    sub = TARGET
    d = stage_dir("s6_netbin")
    stage_banner(f"s6_netbin linker→汇编→打包 → {d}/linker_results")

    from deepeye.net_bin import net_bin

    cwd = os.getcwd()
    os.chdir(d)   # linker 大量子产物落在 CWD
    try:
        net_bin(d, graph, lib, params,
                model_input_names=list(infer),
                bin_file_mode="both",
                model_name="pipeline_tiny",
                nnp_dev=sub)
    finally:
        os.chdir(cwd)

    lr = d   # 产物直接落在 net_path(=阶段目录)下
    print(f"  产物目录: {lr}")
    for f in sorted(os.listdir(lr)):
        p = os.path.join(lr, f)
        if os.path.isfile(p):
            print(f"    {os.path.getsize(p):>10,}B  {f}")
        else:
            print(f"    {'<dir>':>12}  {f}/ ({len(os.listdir(p))} 项)")
    ctx.netbin_dir = lr
    return ctx


if __name__ == "__main__":
    run()
