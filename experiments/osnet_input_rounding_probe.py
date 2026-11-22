#!/usr/bin/env python3.8
"""osnet 输入舍入不一致判决实验(2026-11-21)。

背景:s0 的 infer_input 是 float32 [0,255],而 relay_int8 图输入是 uint8。
  - ISS 侧: run_iss 把 float32 原始字节写进 nnp.models.ci(实测 6144 行=98304B),
    NPU 硬件内部做 float→int 量化(舍入模式=硬件定)
  - CPU 侧: eval_relay 绑定时 TVM 做 float32→uint8 C-cast = 截断
  → 两侧输入系统性差 ±0.5~1 LSB,被 7×7×3 conv 放大 → lg0 cos 0.746,全程发散

判决:喂 ISS 整数值 float(np.trunc / np.round 两个变体),只重跑 ISS
(linker 产物不动),与 Nov20 的 cpu_output_0.npy(截断语义)比 cosine:
  - 若 trunc 变体 cos → ~1.0:输入舍入不一致是主因(实锤)
  - round 变体作对照(应保持 ~0.81 或更差)

跑法: source env.sh && python3.8 experiments/osnet_input_rounding_probe.py
产物: experiments/layerwise_my_osnet_ain_ms_d_c/input_probe/{iss_out_trunc.npy,
      iss_out_round.npy, probe_result.json};原 s8 现场先备份到 s8_sim/probe_backup/
"""
import os
import sys
import json
import shutil
import time

os.environ.setdefault("NPUP_RUN_TAG", "my_osnet_ain_ms_d_c")
PIPE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, PIPE)

import numpy as np  # noqa: E402

TARGET = "nnp318p"
S0 = os.path.join(PIPE, "work", TARGET, "my_osnet_ain_ms_d_c", "s0_model")
S8 = os.path.join(PIPE, "work", TARGET, "my_osnet_ain_ms_d_c", "s8_sim")
EXP = os.path.join(PIPE, "experiments", "layerwise_my_osnet_ain_ms_d_c", "input_probe")


def main():
    os.makedirs(EXP, exist_ok=True)
    inf = np.load(os.path.join(S0, "input_data.npz"))["infer_input.1"]
    assert inf.dtype == np.float32
    print(f"输入: {inf.shape} {inf.dtype} [{inf.min():.3f},{inf.max():.3f}]")

    # 备份将被覆盖的现场文件
    bkp = os.path.join(S8, "probe_backup")
    os.makedirs(bkp, exist_ok=True)
    for f in ("nnp.models.ci", "lib.task0.batch0.rawdata.dat", "log_iss",
              "iss_output_0.npy", "cpu_output_0.npy", "compare.txt", "iss_scr"):
        p = os.path.join(S8, f)
        if os.path.isfile(p) and not os.path.isfile(os.path.join(bkp, f)):
            shutil.copy2(p, bkp)
    print(f"现场已备份 → {bkp}")

    cpu_out = np.load(os.path.join(S8, "cpu_output_0.npy")).astype(np.float64)
    iss_base = np.load(os.path.join(S8, "iss_output_0.npy")).astype(np.float64)

    def cos(a, b):
        a, b = np.asarray(a).flatten(), np.asarray(b).flatten()
        return float(np.dot(a, b) / (np.linalg.norm(a) * np.linalg.norm(b) + 1e-12))

    from deepeye.iss_nnp3xx.iss import run_iss_wrapper
    out_shape = [tuple(cpu_out.shape)]
    results = {"baseline_cos_iss_vs_cpu": cos(iss_base, cpu_out)}
    print(f"基线 cos(ISS_float原始输入, CPU_截断) = {results['baseline_cos_iss_vs_cpu']:.6f}")

    cwd = os.getcwd()
    os.chdir(S8)
    try:
        for name, conv in (("trunc", np.trunc), ("round", np.round)):
            x = conv(inf).astype(np.float32)
            t0 = time.time()
            print(f"\n=== 变体 {name}: 喂 ISS {name}(float) 整数值输入 ===")
            out = run_iss_wrapper([x], out_shape, ".", work_dir=".", target=TARGET)
            arr = np.asarray(out[0] if isinstance(out, (list, tuple)) else out,
                             dtype=np.float64)
            np.save(os.path.join(EXP, f"iss_out_{name}.npy"), arr)
            c = cos(arr, cpu_out)
            results[f"cos_iss_{name}_vs_cpu_trunc"] = c
            print(f">>> cos(ISS_{name}, CPU_截断) = {c:.6f}  ({time.time()-t0:.0f}s)")
    finally:
        os.chdir(cwd)

    # 恢复现场(保持 s8 基线产物为 Nov20 原版)
    for f in os.listdir(bkp):
        shutil.copy2(os.path.join(bkp, f), os.path.join(S8, f))
    print("\n现场已恢复")

    verdict = ("输入舍入不一致实锤:trunc 变体与 CPU 截断参考吻合"
               if results.get("cos_iss_trunc_vs_cpu_trunc", 0) > 0.99 else
               "trunc 变体未达 0.99:输入舍入不是(唯一)主因,需继续深挖")
    results["verdict"] = verdict
    json.dump(results, open(os.path.join(EXP, "probe_result.json"), "w"), indent=1)
    print("\n===== 结果 =====")
    for k, v in results.items():
        print(f"  {k}: {v}")


if __name__ == "__main__":
    main()
