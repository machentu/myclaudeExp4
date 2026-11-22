#!/usr/bin/env python3.8
"""osnet lg0 分歧定位:输入量化语义 vs conv 运算本身(2026-11-22)。

方法:喂 ISS 整数值 float 输入(trunc),使 NPU 硬件输入量化与 CPU eval_relay
的 float32→uint8 截断绑定严格同值;重跑注入版 ISS,对比 lg0(及前几层):
  - lg0 cos → ~1.0  ⇒ 分歧全在 float 输入量化语义(benchmark 伪影+硬件量化舍入)
  - lg0 cos 仍 ~0.75 ⇒ conv 运算路径真实分歧(24bit 累加/饱和/requant)

跑法: source env.sh && python3.8 experiments/osnet_trunc_input_probe.py
产物: experiments/layerwise_my_osnet_ain_ms_d_c/{ref_trunc/, trunc_probe.json}
"""
import os
import sys
import json
import math
import shutil
import time

os.environ.setdefault("NPUP_RUN_TAG", "my_osnet_ain_ms_d_c")
PIPE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, PIPE)

import numpy as np  # noqa: E402

TARGET = "nnp318p"
TAG = "my_osnet_ain_ms_d_c"
S0 = os.path.join(PIPE, "work", TARGET, TAG, "s0_model")
S4 = os.path.join(PIPE, "work", TARGET, TAG, "s4_build")
S8 = os.path.join(PIPE, "work", TARGET, TAG, "s8_sim")
EXP = os.path.join(PIPE, "experiments", f"layerwise_{TAG}")


def log(m):
    print(f"[trunc-probe {time.strftime('%H:%M:%S')}] {m}", flush=True)


def write_models_ci(x):
    """复刻 iss.py:789-810 的 float32 输入 → nnp.models.ci 十六进制格式。"""
    d = x.flatten()
    byte_num = 4
    pad_len = (math.ceil(len(d) * byte_num / 32) * 32 - len(d) * byte_num) // byte_num
    d = np.append(d, [0] * pad_len).astype(np.float32)
    d = d.reshape(-1, 16 // byte_num)
    di = d.view(np.int32) if hasattr(d, "view") else d.astype(np.int32)
    di = (di.astype(np.int64) + (1 << 32)) % (1 << 32)
    fmt = "08x"
    lines = ["".join(format(int(w), fmt) for w in np.flip(di[i], 0))
             for i in range(len(di))]
    with open(os.path.join(S8, "nnp.models.ci"), "w") as fp:
        fp.write("\n".join(lines) + "\n")


def main():
    inf = np.load(os.path.join(S0, "input_data.npz"))["infer_input.1"]
    x_trunc = np.trunc(inf).astype(np.float32)
    log(f"输入 {inf.shape},trunc 后与原始差 {np.abs(inf - x_trunc).mean():.4f} LSB(均值)")

    # 1) s4 级联拿 params(快照在 weight_second_handle 之前,与已验证路径一致)
    from stages import s4_build
    from stages.common import Ctx
    ctx = s4_build.run(Ctx())
    params = {k: np.array(v.asnumpy() if hasattr(v, "asnumpy") else v, copy=True)
              for k, v in ctx["params_out"].items()}

    # 2) 备份 nnp.models.ci,写 trunc 输入镜像
    bkp = os.path.join(S8, "nnp.models.ci.floatbak")
    if not os.path.isfile(bkp):
        shutil.copy2(os.path.join(S8, "nnp.models.ci"), bkp)
    write_models_ci(x_trunc)
    log("nnp.models.ci 已写为 trunc 输入(原 float 版备份 .floatbak)")

    # 3) 重跑注入版 ISS(ref 落 S8 目录)
    import deepeye.iss_nnp3xx.iss as iss_mod
    iss_bin = os.path.join(os.path.dirname(os.path.realpath(iss_mod.__file__)),
                           f"{TARGET}_iss")
    cwd = os.getcwd()
    os.chdir(S8)
    try:
        t0 = time.time()
        ret = os.system(f"{iss_bin} iss_scr_layerwise > log_iss_trunc 2>&1")
        assert ret == 0, f"ISS 失败 ret={ret}"
        log(f"注入 ISS(trunc 输入)完成 {time.time()-t0:.0f}s")
    finally:
        os.chdir(cwd)
        shutil.copy2(bkp, os.path.join(S8, "nnp.models.ci"))   # 恢复现场
        log("nnp.models.ci 已恢复 float 原版")

    # 4) CPU 参考(trunc 输入,单独缓存不污染主缓存)
    import tvm
    from deepeye.relay_pass.run_function_fused import SubfunctionRunner, from_iss_dump
    func = tvm.load_json(open(os.path.join(S4, "fused_func.json")).read())
    graph = json.load(open(os.path.join(S4, "graph.json")))
    runner = SubfunctionRunner(dump_data_only=True, run_sim=False,
                               work_dir=os.path.join(EXP, "SubfunctionRunner_trunc"))
    npz = np.load(os.path.join(S0, "input_data.npz"))
    infer_key = [k for k in npz.files if k.startswith("infer_input")][0]
    raw = npz[infer_key]
    param_names = [p.name_hint for p in func.params]
    inputs = {}
    for pn in param_names:
        if pn in params:
            inputs[pn] = params[pn]
        else:                       # 模型输入名 = param 名去掉可能前缀
            inputs[pn] = np.trunc(raw).astype(np.float32)
    log(f"CPU 参考(trunc):inputs {len(inputs)} 项")
    cpu_outs, _ = runner.run(func, inputs, TARGET)

    def to_np(v):
        if isinstance(v, tvm.ndarray.NDArray):
            return v.asnumpy()
        if isinstance(v, list):
            return [to_np(x) for x in v]
        return np.asarray(v)

    idx = len(func.params)
    cpu_map = {}
    for ni, node in enumerate(graph["nodes"]):
        if node["op"] != "tvm_op":
            continue
        cpu_map[node["debug_name"]] = to_np(cpu_outs[idx])
        idx += 1

    # 5) 对比 lg0..lg8 与末层
    lg = json.load(open(os.path.join(S8, "lg_info.json")))["lg_info"]
    ref_dir = os.path.join(EXP, "ref_trunc")
    os.makedirs(ref_dir, exist_ok=True)

    def cos(a, b):
        a, b = np.asarray(a).astype(np.float64).flatten(), np.asarray(b).astype(np.float64).flatten()
        return float(np.dot(a, b) / (np.linalg.norm(a) * np.linalg.norm(b) + 1e-12))

    from collections import deque
    fn2dn = {}
    for node in graph["nodes"]:
        if node["op"] == "tvm_op":
            fn2dn.setdefault(node["attrs"]["func_name"], deque()).append(node["debug_name"])
    old = json.load(open(os.path.join(EXP, "layerwise.json")))["rows"]
    out = []
    for k in list(range(9)) + [len(lg) - 2]:
        ot = lg[k]["out_tensor"][0]
        src = os.path.join(S8, f"lg{k}_out0.ref")
        shutil.move(src, os.path.join(ref_dir, f"lg{k}_out0.ref"))
        ref = from_iss_dump(os.path.join(ref_dir, f"lg{k}_out0.ref"),
                            tuple(ot["shape"]), ot["dtype"])
        dn = fn2dn[lg[k]["func_name"]]
        dn = dn.popleft() if len(dn) > 1 else dn[0]
        c = cpu_map[dn]
        c = np.asarray(c[0] if isinstance(c, list) else c)
        if ref.shape != c.shape and np.squeeze(ref).shape == np.squeeze(c).shape:
            c = c.reshape(ref.shape)
        ct = cos(ref, c)
        prev = next((r["cosine"] for r in old if r["lg"] == k and r["out"] == 0), None)
        out.append({"lg": k, "debug_name": dn, "cos_trunc_feed": ct,
                    "cos_float_feed_prev": prev})
        log(f"lg{k:3d} {dn[:40]:40s} trunc={ct:.6f}  (float 版={prev})")
    json.dump(out, open(os.path.join(EXP, "trunc_probe.json"), "w"), indent=1)
    d0 = out[0]["cos_trunc_feed"]
    log("判定:" + ("输入量化语义主导(lg0 大幅回升)" if d0 > 0.95 else
                  "conv 运算路径真实分歧(lg0 未回升)" if d0 < 0.85 else
                  "混合因素(lg0 部分回升)"))


if __name__ == "__main__":
    main()
