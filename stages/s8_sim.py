"""s8_sim — ISS 周期级仿真 + CPU 参考比对。

厂商对应代码:
  - golden.py:386 run_on_nnp 的模式(本 stage 复用 s4 的 graph/lib/params,不重新编译):
      nnp_func_names(graph) → weight_second_handle →
      lib.imported_modules[asm_idx].export_library("nnp_test.o", linker,
          params=params, graph=graph, outdir=outdir, func_names=..., args_map=...)
      → 在 outdir 留下 ISS 需要的全部中间件:
        asm_work/nnp_main.hex|lst   nnp.models   model_mem_info.json   link_info
  - iss_nnp3xx/iss.py:682 run_iss():
      run_iss_pre  读 model_mem_info/link_info,生成 iss_nnp_queue.cfg
      (DDR 任务队列描述符:host 把 15×128bit 描述符写进 DDR @0x100020000,
       含 fmap/权重/代码/mid 缓冲地址 —— 这就是硬件被驱动的方式)
      → iss_scr_gen.py 生成 iss_scr 命令脚本(load ddr/cm/dm/ppm,w reg mbx...,b,c,save)
      → 执行 <target>_iss 预编译 x86 周期级模拟器(cu/lu/su/nu/pe/vu/各 DMA 单元模型)
      → save eodma_raw 抓回输出,解析回 numpy,报告周期数(log_iss cycle_cnt)
  - CPU 参考:deepeye.eval_relay(rewrite_for_cpu + LLVM relay.build + graph_runtime)

产物: iss_out/ 输出张量、log_iss(含周期数)、compare.txt(余弦相似度)
"""
import os
import re
import json
import numpy as np

from stages.common import Ctx, stage_banner, stage_dir, ensure, TARGET


def run(ctx: Ctx = None) -> Ctx:
    if ctx is None:
        ctx = Ctx()
    graph = ensure(ctx, "s4_build", "graph")
    lib = ensure(ctx, "s4_build", "lib")
    params = ensure(ctx, "s4_build", "params_out")
    infer = ensure(ctx, "s0_model", "infer_input")
    relay_int8 = ensure(ctx, "s2_quantize", "relay_int8")
    sub = TARGET
    d = stage_dir("s8_sim")
    stage_banner(f"s8_sim ISS 周期级仿真({sub})")

    from deepeye.linker.linker import linker
    from deepeye.golden import nnp_func_names, weight_second_handle
    from deepeye.iss_nnp3xx.iss import run_iss_wrapper

    # 1) 对 lib 重跑 linker(保留 ISS 所需中间件;net_bin 打包时会清理)
    func_names, out_func_names, in_func_names = nnp_func_names(graph)
    # host 侧节点(__copy 纯 DMA、fused_subtract 等 device!=NNP 的 tvm_op)无
    # NPU kernel 标签,整图 LTO generate_nodes(lto.py:644)会 KeyError。
    # net_bin 分块路径只收 device==nnp300.device_type 的节点天然排除之
    # (nnp3_net_bin.py:200+);而 nnp_func_names 对无 dedsp 图返回空列表 →
    # generate_nodes 的 len>0 过滤失效 → host 节点放行 → 崩(v010 实测两种:
    # __copy_kernel0 与 fused_subtract_5_kernel0)。对齐分块语义:按 device 过滤。
    gj = json.loads(graph)
    import tvm as _tvm
    nnp_dev = _tvm.target.create("nnp300").device_type
    di = gj["attrs"]["device_index"][1]
    di = [x[0] if isinstance(x, list) else x for x in di]
    host_nodes = [n["name"] for i, n in enumerate(gj["nodes"])
                  if n["op"] == "tvm_op" and di[i] != nnp_dev]
    if host_nodes:
        nnp_names = [n["name"] for i, n in enumerate(gj["nodes"])
                     if n["op"] == "tvm_op" and di[i] == nnp_dev]
        func_names = nnp_names if not func_names else \
            [nm for nm in func_names if nm not in set(host_nodes)]
        print(f"  [s8] host 侧节点 {host_nodes}(device!={nnp_dev})不入 LTO,"
              f"func_names 过滤为 {len(nnp_names)} 个 NNP 节点(对齐 net_bin 分块路径)")
    weight_second_handle(graph, params)
    asm_idx = next(i for i, m in enumerate(lib.imported_modules)
                   if "nnp" in m.type_key or "asm" in m.type_key)
    cwd = os.getcwd()
    os.chdir(d)
    try:
        lib.imported_modules[asm_idx].export_library(
            "nnp_test.o", linker, params=params, graph=graph, outdir=d,
            func_names=func_names, out_func_names=out_func_names,
            in_func_names=in_func_names, args_map=lib.get_source(), target=sub)

        # 2) ISS 仿真(输入按声明顺序拼接;输出形状从 relay 函数签名推断,支持多输出)
        # 输入必须按图声明 dtype 喂 ISS —— 原版 golden.py:624 astype(in_dtype) 语义。
        # osnet/v61 教训(2026-11-22):float32 直喂 uint8 图 → NPU 按
        # model_input_buf_size 只读 float 字节流前 1/4 当图像 → 垃圾输入
        # (osnet 0.812 / v61 0.051 的主因;mcv3 等 uint8 输入模型不受影响)。
        in_dtypes = {n["name"]: dt for n, dt in zip(gj["nodes"], gj["attrs"]["dltype"][1])
                     if n["op"] == "null" and isinstance(dt, str)}
        in_list = []
        for n in infer:
            a = np.asarray(infer[n])
            dt = in_dtypes.get(n)
            if dt is not None and a.dtype != np.dtype(dt):
                print(f"  [s8] 输入 {n}: {a.dtype} → {dt}(图声明 dtype,对齐 golden.py in_dtype)")
                a = a.astype(dt)
            in_list.append(a)
        ret = relay_int8.ret_type
        if hasattr(ret, "fields"):                # TupleType(多输出)
            out_shape = [tuple(int(s) for s in t.shape) for t in ret.fields]
        else:
            out_shape = [tuple(int(s) for s in ret.shape)]
        sim_out = run_iss_wrapper(in_list, out_shape, d, work_dir=d, target=sub)
    finally:
        os.chdir(cwd)

    print(f"  ISS 输出: {len(out_shape)} 个张量,形状 {out_shape}")
    print(f"  周期数: 见 {d}/log_iss 的 cycle_cnt 行")

    # 3) CPU 参考输出与比对(eval_relay 返回 {输出名: ndarray};ISS 多输出返回 list)
    from deepeye import eval_relay
    cpu_out = eval_relay(relay_int8, {}, {n: infer[n] for n in infer})
    cpu_list = list(cpu_out.values()) if hasattr(cpu_out, "values") else \
               (list(cpu_out) if isinstance(cpu_out, (list, tuple)) else [cpu_out])
    sims = sim_out if isinstance(sim_out, (list, tuple)) else [sim_out]
    sims = [np.asarray(s) for s in sims]

    def cosine(a, b):
        a, b = a.flatten().astype(np.float64), b.flatten().astype(np.float64)
        return float(np.dot(a, b) / (np.linalg.norm(a) * np.linalg.norm(b) + 1e-12))

    lines = []
    for i, (s, c) in enumerate(zip(sims, cpu_list)):
        c = np.asarray(c, dtype=s.dtype).reshape(s.shape)
        lines.append(f"output[{i}] shape={s.shape} cosine(ISS,CPU)={cosine(s, c):.6f} "
                     f"ISS[{s.min():.4f},{s.max():.4f}] CPU[{c.min():.4f},{c.max():.4f}]")
        np.save(os.path.join(d, f"iss_output_{i}.npy"), s)
        np.save(os.path.join(d, f"cpu_output_{i}.npy"), c)
    report = "\n".join(lines)
    print(report)
    with open(os.path.join(d, "compare.txt"), "w") as f:
        f.write(report + "\n")
    ctx.sim_output = sims[0] if len(sims) == 1 else sims
    # 指标上报(manifest):cycles 取 log_iss 最后一个 cycle_cnt
    m = ctx.get("metrics", {})
    try:
        cyc = re.findall(r"cycle_cnt: (\d+)", open(os.path.join(d, "log_iss")).read())
        if cyc:
            m["cycles"] = int(cyc[-1])
    except OSError:
        pass
    first_cos = re.search(r"cosine\(ISS,CPU\)=([\d.]+)", "\n".join(lines))
    if first_cos:
        m["cosine"] = float(first_cos.group(1))
    ctx.metrics = m
    return ctx


if __name__ == "__main__":
    run()
