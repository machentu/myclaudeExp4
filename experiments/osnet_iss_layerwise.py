#!/usr/bin/env python3.8
"""osnet 精度(0.812)逐层归因:整模型 ISS + 逐层 save ddr128 注入(零 SDK)。

原理(全部复刻厂商机制,不改厂商源码):
  P. 级联重跑 s0→s4 拿 fused_func/params(params 进 s8 前深拷贝快照——
     golden.py:318 weight_second_handle 会就地改写 conv2d_transpose 权重),
     再跑 s8(linker+整模型 ISS,刷新 iss_scr/asm_work/lg_info 保证自洽,
     同时复现 0.812 基线)。--reuse-artifacts 跳过 s8,用 Nov19 现成产物。
  I. get_lg_bp_from_lst('./iss_scr', target) 扫 gp lst 的 mov mbox3.s3(0e184068)
     imm16 变化沿 → 断点表;复刻 up_iss_scr.py:27-53 注入循环(厂商契约:
     跳过全局第 1 个断点,第 k+2 个断点处 save lg k,"晚一层"),在每条
     `load ppm ...lib_bb_gpN_cpp.lst` 锚点行后插 b/c/dis b/save ddr128 四连
     → iss_scr_layerwise;跑 <target>_iss。断言:bp 总数==lg 数、save 数==lg-1。
  C. SubfunctionRunner(run_sim=False) 对 fused_func.json 逐 Call 走纯 LLVM
     eval_relay;返回序按 dump_func_output:397-442 契约映射到 graph tvm_op
     的 debug_name(前 len(func.params) 项是输入回显,其后按 graph 节点序,
     多输出为 list)。带 shape 交叉校验兜底。npz 缓存 + 输入指纹。
  A. lg↔cpu 对齐按 func_name 出现序队列(graph attrs.func_name);逐层
     cosine(float64);低相似度时自动做 ±1/±2 全局错位诊断;输出形态判定
     (线性缓降=复合舍入 / 断崖=特定算子) → layerwise.md + layerwise.json。

用法: source ../env.sh && python3.8 experiments/osnet_iss_layerwise.py
      [--reuse-artifacts] [--force-iss] [--force-cpu]
产物: experiments/osnet_layerwise/{ref/*.ref, layerwise.json, layerwise.md,
      cpu_dump_cache.npz, input_fingerprint.json};ISS 日志在 s8_sim/log_iss_layerwise
"""
import os
import sys
import json
import hashlib
import shutil
import time

MODEL_TAG = os.environ.get("NPUP_LAYERWISE_TAG", "my_osnet_ain_ms_d_c")
os.environ.setdefault("NPUP_RUN_TAG", MODEL_TAG)

PIPE_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, PIPE_ROOT)

import numpy as np                                                        # noqa: E402

TARGET = os.environ.get("NPUP_TARGET", "nnp318p")
S8_DIR = os.path.join(PIPE_ROOT, "work", TARGET, MODEL_TAG, "s8_sim")
S4_DIR = os.path.join(PIPE_ROOT, "work", TARGET, MODEL_TAG, "s4_build")
EXP_DIR = os.path.join(PIPE_ROOT, "experiments", f"layerwise_{MODEL_TAG}")

DTYPE_SIZE = {"uint8": 1, "int8": 1, "bool": 1, "uint1": 1,
              "uint16": 2, "int16": 2, "float16": 2,
              "uint32": 4, "int32": 4, "float32": 4,
              "uint64": 8, "int64": 8, "float64": 8}


def log(msg):
    print(f"[layerwise {time.strftime('%H:%M:%S')}] {msg}", flush=True)


# ---------------------------------------------------------------- phase P
def phase_pipeline(reuse_artifacts):
    from stages import s4_build, s8_sim
    from stages.common import Ctx
    log("P: 级联 s0→s4(拿 fused_func/params/infer_input)")
    ctx = s4_build.run(Ctx())
    # params 快照必须在 s8 之前(weight_second_handle 就地改写)
    params_snap = {k: np.array(v.asnumpy() if hasattr(v, "asnumpy") else v, copy=True)
                   for k, v in ctx["params_out"].items()}
    infer = {k: np.array(v, copy=True) for k, v in ctx["infer_input"].items()}
    if reuse_artifacts:
        log("P: --reuse-artifacts,跳过 s8,使用现成 s8_sim 产物")
    else:
        log("P: 跑 s8(linker+整模型 ISS,刷新 iss_scr/lg_info 并复现基线)")
        ctx = s8_sim.run(ctx)
    return ctx, params_snap, infer


# ---------------------------------------------------------------- phase I
def build_lg_addr_info():
    """lg_info.json + queue_cfg.json → 每个 out_tensor 补 byte_size/ddr_addr。
    规则照抄 get_lg_addr.py:99-126;mid 基址取 queue_cfg(与 iss_glob 一致)。"""
    lg_info = json.load(open(os.path.join(S8_DIR, "lg_info.json")))["lg_info"]
    q0 = json.load(open(os.path.join(S8_DIR, "queue_cfg.json")))["queue0"]
    mid_base = int(q0["data_mid_addr"], 16) + (int(q0["data_mid_addr_hi"], 16) << 32)
    from deepeye.iss_nnp3xx import iss_glob
    assert mid_base == iss_glob.mid_buf_base, f"mid 基址不一致 {mid_base:#x}"
    out_base = iss_glob.model_dout_base
    n_out_tensors = 0
    for lg in lg_info:
        for ot in lg["out_tensor"]:
            n_out_tensors += 1
            if "offset" not in ot:      # 末层 out_buf 用嵌套字段(osnet lg172 实测)
                ob = ot.get("offset_for_one_buf") or ot.get("offset_for_multi_buf") or {}
                ot["offset"] = ob.get("offset", "0x0")
            shape, dt = ot["shape"], ot["dtype"]
            if dt in ("int4", "uint4"):
                flat = int(np.prod(shape[:-1])) if len(shape) > 1 else 1
                ot["byte_size"] = ((shape[-1] + 1) // 2) * flat
            else:
                assert dt in DTYPE_SIZE, f"未知 dtype {dt}"
                ot["byte_size"] = int(np.prod(shape)) * DTYPE_SIZE[dt]
            if ot["buf_type"] == "mid_buf":
                ot["ddr_addr"] = hex(mid_base + int(ot["offset"], 16))
            elif ot["buf_type"] == "out_buf":
                ot["ddr_addr"] = hex(out_base + int(ot["offset"], 16))
            else:
                raise AssertionError(f"未知 buf_type {ot['buf_type']}")
    log(f"I: lg={len(lg_info)} out_tensor={n_out_tensors} "
        f"mid_base={mid_base:#x} out_base={out_base:#x}")
    return lg_info


def phase_inject_iss(lg_info, force_iss):
    from deepeye.linker.get_lg_bp import get_lg_bp_from_lst
    cwd = os.getcwd()
    os.chdir(S8_DIR)
    try:
        # 1) 断点表(写 lg_bp_info.json 到 cwd=S8_DIR)
        get_lg_bp_from_lst("./iss_scr", TARGET)
        bp_info = json.load(open("lg_bp_info.json"))
        total_bp = sum(len(g["lib_bb_gp_bp_list"])
                       for g in bp_info["lib_bb_gp_lg_bp_info"])
        assert total_bp == len(lg_info), f"断点数 {total_bp} != lg 数 {len(lg_info)}"

        # 2) 注入(复刻 up_iss_scr.py:27-53;我们的 iss_scr 本身已是完整脚本,
        #    尾部 save eodma_raw 覆盖最后一个 lg 的 out_buf,无需厂商 run_all 拼装)
        out_lines, lg_tot, skip_first = [], 0, True
        with open("iss_scr") as fp:
            for line in fp.readlines():
                out_lines.append(line)
                if "lib_bb_gp" in line or "lib_cpp.lst" in line:
                    lst_name = line.split(" ")[-1].strip()
                    for grp in bp_info["lib_bb_gp_lg_bp_info"]:
                        if grp["lib_bb_gp_lst_file_name"] != lst_name:
                            continue
                        for bp in grp["lib_bb_gp_bp_list"]:
                            if skip_first:            # 厂商契约:跳过全局第 1 个断点
                                skip_first = False
                                continue
                            out_lines.append("\n")
                            out_lines.append(f"    b 0x{bp}\n")
                            out_lines.append("    c\n")
                            out_lines.append(f"    dis b 0x{bp}\n")
                            for t, ot in enumerate(lg_info[lg_tot]["out_tensor"]):
                                end = hex(int(ot["ddr_addr"], 16) + ot["byte_size"] - 1)
                                out_lines.append(
                                    f"    save ddr128 lg{lg_tot}_out{t}.ref "
                                    f"{ot['ddr_addr']}:{end} 0\n")
                            lg_tot += 1
                        out_lines.append("\n")
                        break
        assert lg_tot == len(lg_info) - 1, f"注入 save 组 {lg_tot} != {len(lg_info)-1}"
        with open("iss_scr_layerwise", "w") as fh:
            fh.writelines(out_lines)
        log(f"I: iss_scr_layerwise 生成({len(out_lines)} 行,注入 {lg_tot} 组断点+save)")

        # 3) 跑 ISS(ref 文件落 S8_DIR;--force-iss 强制重跑)
        expect = [(k, t) for k in range(len(lg_info) - 1)
                  for t in range(len(lg_info[k]["out_tensor"]))]
        have_all = all(os.path.isfile(f"lg{k}_out{t}.ref") for k, t in expect)
        if have_all and not force_iss:
            log("I: 全部 ref 已存在,跳过 ISS(--force-iss 强制重跑)")
        else:
            import deepeye.iss_nnp3xx.iss as iss_mod
            iss_bin = os.path.join(os.path.dirname(os.path.realpath(iss_mod.__file__)),
                                   f"{TARGET}_iss")
            log(f"I: 运行 {iss_bin} iss_scr_layerwise({len(lg_info)-1} 次断点暂停,耗时未知)")
            t0 = time.time()
            ret = os.system(f"{iss_bin} iss_scr_layerwise > log_iss_layerwise 2>&1")
            assert ret == 0, f"ISS 失败 ret={ret},看 {S8_DIR}/log_iss_layerwise"
            log(f"I: ISS 完成,耗时 {time.time()-t0:.0f}s")
            missing = [f"lg{k}_out{t}.ref" for k, t in expect
                       if not os.path.isfile(f"lg{k}_out{t}.ref")]
            assert not missing, f"缺 ref 文件 {len(missing)} 个,如 {missing[:5]}"
    finally:
        os.chdir(cwd)
    return expect


# ---------------------------------------------------------------- phase C
def fingerprint(params_snap, infer):
    h = hashlib.md5()
    for d in (infer, params_snap):
        for k in sorted(d):
            h.update(k.encode())
            h.update(np.ascontiguousarray(d[k]).tobytes())
    return h.hexdigest()


def phase_cpu_reference(fused_func_path, params_snap, infer, force_cpu):
    """SubfunctionRunner(run_sim=False) 逐层 CPU 参考,按 debug_name 建 dict。"""
    os.makedirs(EXP_DIR, exist_ok=True)
    fp_path = os.path.join(EXP_DIR, "input_fingerprint.json")
    cache = os.path.join(EXP_DIR, "cpu_dump_cache.npz")
    fp_now = fingerprint(params_snap, infer)
    if (os.path.isfile(cache) and os.path.isfile(fp_path) and not force_cpu
            and json.load(open(fp_path)).get("fingerprint") == fp_now):
        log("C: cpu_dump 缓存命中(输入指纹一致)")
        meta = json.load(open(fp_path))
        z = np.load(cache, allow_pickle=True)   # 自家生成的缓存,多输出为 object 数组
        return {k: z[k.replace("/", "_")] for k in meta["cache_keys"]}, \
            meta["graph_order"]

    import tvm
    from deepeye.relay_pass.run_function_fused import SubfunctionRunner
    func = tvm.load_json(open(fused_func_path).read())
    graph = json.load(open(os.path.join(S4_DIR, "graph.json")))
    runner = SubfunctionRunner(dump_data_only=True, run_sim=False,
                               work_dir=os.path.join(EXP_DIR, "SubfunctionRunner"))
    inputs = dict(infer)
    inputs.update({k: v for k, v in params_snap.items() if k not in inputs})
    log(f"C: SubfunctionRunner 逐层 eval_relay(inputs {len(inputs)} 项,10-30min)")
    t0 = time.time()
    cpu_outs, _ = runner.run(func, inputs, TARGET)
    log(f"C: 完成,耗时 {time.time()-t0:.0f}s,输出 {len(cpu_outs)} 项")

    # dump_func_output:397-442 契约:前 len(func.params) 项为输入回显,
    # 其后按 graph tvm_op 节点序;多输出节点为 list
    def to_np(v):
        if isinstance(v, tvm.ndarray.NDArray):
            return v.asnumpy()
        if isinstance(v, list):
            return [to_np(x) for x in v]
        return np.asarray(v)

    idx = len(func.params)
    cpu_dict, graph_order, shapes = {}, [], {}
    for ni, node in enumerate(graph["nodes"]):
        if node["op"] != "tvm_op":
            continue
        v = to_np(cpu_outs[idx])
        cpu_dict[node["debug_name"]] = v
        graph_order.append(node["debug_name"])
        shapes[node["debug_name"]] = graph["attrs"]["shape"][1][ni]
        idx += 1
    if idx != len(cpu_outs):
        # 多输出模型:外层 body 是 Tuple,visit_tuple 也进 tensor_map_cpu(尾项)
        extra = cpu_outs[idx:]
        assert all(isinstance(e, list) for e in extra), \
            f"契约漂移:cpu_outs {len(cpu_outs)} 用到 {idx},多出项非 Tuple"
        print(f"  [C] 忽略尾部 {len(extra)} 个 Tuple 项(多输出模型正常)")
    np.savez(cache, **{k.replace("/", "_"): (v if not isinstance(v, list) else
                                             np.array(v, dtype=object))
                       for k, v in cpu_dict.items()})
    json.dump({"fingerprint": fp_now, "graph_order": graph_order,
               "cache_keys": list(cpu_dict.keys()), "shapes": shapes},
              open(fp_path, "w"))
    return cpu_dict, graph_order


# ---------------------------------------------------------------- phase A
def phase_align_compare(lg_info, expect, cpu_dict, graph_order):
    graph = json.load(open(os.path.join(S4_DIR, "graph.json")))
    # func_name → debug_name 出现序队列
    from collections import defaultdict, deque
    fn2dn = defaultdict(deque)
    for ni, node in enumerate(graph["nodes"]):
        if node["op"] == "tvm_op":
            fn2dn[node["attrs"]["func_name"]].append(node["debug_name"])

    from deepeye.relay_pass.run_function_fused import from_iss_dump
    rows, refs = [], {}
    for k, t in expect:
        lg = lg_info[k]
        ot = lg["out_tensor"][t]
        ref_path = os.path.join(S8_DIR, f"lg{k}_out{t}.ref")
        ref = from_iss_dump(ref_path, tuple(ot["shape"]), ot["dtype"])
        dn_q = fn2dn.get(lg["func_name"])
        if t == 0:
            assert dn_q, f"lg{k} func_name {lg['func_name']} 在 graph 中无对应"
            dn = dn_q[0] if len(dn_q) == 1 else dn_q.popleft()
            last_dn = dn
        else:
            dn = last_dn
        cpu = cpu_dict[dn]
        if isinstance(cpu, list):
            cpu = np.asarray(cpu[t])
        # shape 交叉校验:relay 与 NPU 元数据允许差冗余 1 维(lg168 实测
        # (1,1,512,1) vs (1,512,1)),squeeze 后必须一致且元素数相等
        if tuple(ref.shape) != tuple(cpu.shape):
            if np.squeeze(ref).shape != np.squeeze(cpu).shape or ref.size != cpu.size:
                raise AssertionError(
                    f"lg{k} shape 不齐 ref{ref.shape} vs cpu{cpu.shape}({dn})")
            print(f"  [A] lg{k} rank 差异(squeeze 对齐):ref{ref.shape} cpu{cpu.shape}")
        a = ref.astype(np.float64).flatten()
        b = cpu.astype(np.float64).flatten()
        cos = float(np.dot(a, b) / (np.linalg.norm(a) * np.linalg.norm(b) + 1e-12))
        refs[(k, t)] = (ref, cpu)
        rows.append({"lg": k, "out": t, "func_name": lg["func_name"],
                     "debug_name": dn, "dtype": ot["dtype"],
                     "shape": list(ot["shape"]), "cosine": cos})

    cosines = np.array([r["cosine"] for r in rows])
    diag = {"median": float(np.median(cosines))}
    if diag["median"] < 0.5:      # 错位/布局诊断:整体平移比对
        shifts = {}
        for sh in (-2, -1, 1, 2):
            cs = []
            for k, t in expect:
                k2 = k + sh
                if not (0 <= k2 < len(lg_info) - 1):
                    continue
                try:
                    dn = next(r["debug_name"] for r in rows
                              if r["lg"] == k2 and r["out"] == t)
                except StopIteration:
                    continue
                a = refs[(k, t)][0].astype(np.float64).flatten()
                b = cpu_dict[dn]
                b = np.asarray(b[t] if isinstance(b, list) else b).astype(np.float64).flatten()
                if a.size == b.size:
                    cs.append(float(np.dot(a, b) /
                                    (np.linalg.norm(a) * np.linalg.norm(b) + 1e-12)))
            shifts[sh] = float(np.median(cs)) if cs else None
        diag["shift_median"] = shifts
        log(f"A: 警告 median cosine={diag['median']:.3f},错位诊断 {shifts}")

    below99 = next((r["lg"] for r in rows if r["cosine"] < 0.99), None)
    below90 = next((r["lg"] for r in rows if r["cosine"] < 0.90), None)
    if diag["median"] < 0.5:
        verdict = "整体低相似——先解决布局/对齐(见 shift_median 诊断),暂不能判形态"
    elif below90 is None:
        verdict = "全程 ≥0.90:发散非单层骤降,倾向复合舍入的缓慢累积"
    elif below99 is not None and below90 is not None and below90 - below99 > 20:
        verdict = (f"缓降后于 lg{below90} 跌破 0.90:前段复合舍入 + 后段放大,"
                   f"重点查 lg{below90}({rows[below90]['func_name']})附近算子")
    else:
        verdict = f"断崖:lg{below90}({rows[below90]['func_name']})骤降,特定算子缺陷嫌疑"
    diag.update({"first_below_0.99": below99, "first_below_0.90": below90,
                 "verdict": verdict})
    log(f"A: {verdict}")

    os.makedirs(os.path.join(EXP_DIR, "ref"), exist_ok=True)
    for k, t in expect:
        src = os.path.join(S8_DIR, f"lg{k}_out{t}.ref")
        shutil.move(src, os.path.join(EXP_DIR, "ref", f"lg{k}_out{t}.ref"))
    json.dump({"rows": rows, "diag": diag},
              open(os.path.join(EXP_DIR, "layerwise.json"), "w"), indent=1)
    with open(os.path.join(EXP_DIR, "layerwise.md"), "w") as f:
        f.write(f"# osnet 逐层归因(ISS save ddr128 vs CPU eval_relay,{TARGET})\n\n")
        f.write(f"- 判定:{verdict}\n- median cosine={diag['median']:.4f},"
                f"首破 0.99:lg{below99},首破 0.90:lg{below90}\n")
        if "shift_median" in diag:
            f.write(f"- 错位诊断: {diag['shift_median']}\n")
        f.write(f"- 整模型基线见 s8_sim/compare.txt;逐层数据 layerwise.json\n\n")
        f.write("| lg | out | debug_name | dtype | shape | cosine |\n")
        f.write("|---|---|---|---|---|---|\n")
        for r in rows:
            f.write(f"| {r['lg']} | {r['out']} | {r['debug_name']} | {r['dtype']} "
                    f"| {r['shape']} | {r['cosine']:.6f} |\n")
    log(f"A: 产物在 {EXP_DIR}/(layerwise.md/json,ref/ 已归档)")
    return rows, diag


def main():
    args = sys.argv[1:]
    reuse = "--reuse-artifacts" in args
    force_iss = "--force-iss" in args
    force_cpu = "--force-cpu" in args
    ctx, params_snap, infer = phase_pipeline(reuse)
    lg_info = build_lg_addr_info()
    expect = phase_inject_iss(lg_info, force_iss)
    cpu_dict, _ = phase_cpu_reference(os.path.join(S4_DIR, "fused_func.json"),
                                      params_snap, infer, force_cpu)
    phase_align_compare(lg_info, expect, cpu_dict, _)


if __name__ == "__main__":
    main()
