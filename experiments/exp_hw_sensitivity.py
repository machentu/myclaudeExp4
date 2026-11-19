#!/usr/bin/env python3.8
"""exp_hw_sensitivity —— 硬件参数敏感性实验(PE_NUM / DM_SIZE)。

方法:临时改写 deepeye_config_nnp318p.json(源码包路径,运行时由 .py 加载),
对每个参数档位完整重跑 s0→s6(+s8 可选),收集:
  - cycle_cnt(ISS 周期)
  - model_hex_len(代码体积,PM 占用)
  - tile 数(asm 中 ## tile= 标记)、pe_attr mask 形态
  - ddr/innerbuf(net.bin 头)
结束后恢复原 json。用法:
  python3.8 experiments/exp_hw_sensitivity.py            # PE_NUM 档位 [4, 2, 8]
  python3.8 experiments/exp_hw_sensitivity.py 4 2 8 12   # 自定义档位
"""
import json
import os
import re
import shutil
import subprocess
import sys
import time

PIPE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CFG = "/DEngine/projects/TyTVM_NNP3xx_src_v1.0.41/tytvm_src_pkg/deepeye/config/deepeye_config_nnp318p.json"
ENV = dict(os.environ,
           PYTHONPATH=f"{PIPE}:{os.environ.get('PYTHONPATH', '')}",
           NPUP_TARGET="nnp318p", NPUP_DUMP_TIR="0")


def run_variant(tag, key, value):
    """改 json → 跑流水线 → 采指标 → 恢复 json 由调用方统一做。"""
    env = dict(ENV, NPUP_RUN_TAG=tag)
    t0 = time.time()
    r = subprocess.run([os.environ.get("PY", "/usr/bin/python3.8"),
                        os.path.join(PIPE, "run_all.py")],
                       env=env, cwd=PIPE, capture_output=True, text=True, timeout=1200)
    ok = r.returncode == 0
    metrics = {"tag": tag, key: value, "ok": ok, "wall_s": round(time.time() - t0, 1)}
    w = os.path.join(PIPE, "work", "nnp318p", tag)

    li = os.path.join(w, "s8_sim", "link_info")
    if os.path.isfile(li):
        d = dict(l.split(": ") for l in open(li).read().splitlines() if ": " in l)
        metrics["model_hex_len"] = int(d["model_hex_len"])
        metrics["firmware_hex_len"] = int(d["nnp_main_hex_len"])
        metrics["mid_buf"] = int(d["middle_buf_size"])
    log = os.path.join(w, "s8_sim", "log_iss")
    if os.path.isfile(log):
        cyc = re.findall(r"cycle_cnt: (\d+)", open(log).read())
        if cyc:
            metrics["cycles"] = int(cyc[-1])
    cmp_f = os.path.join(w, "s8_sim", "compare.txt")
    if os.path.isfile(cmp_f):
        m = re.search(r"cosine\(ISS,CPU\)=([\d.]+)", open(cmp_f).read())
        if m:
            metrics["cosine"] = float(m.group(1))
    # tile/asm 统计
    asm_dir = os.path.join(w, "s8_sim", "model_asm")
    if os.path.isdir(asm_dir):
        tiles, masks = 0, set()
        for f in os.listdir(asm_dir):
            if f.endswith("_lg0.asm"):
                txt = open(os.path.join(asm_dir, f), errors="ignore").read()
                tiles += len(re.findall(r"^## tile=", txt, re.M))
                masks |= set(re.findall(r"pe_mask=0x([0-9a-f]+)", txt))
        metrics["tile_markers"] = tiles
        metrics["pe_masks"] = sorted(masks)
    if not ok:
        tail = (r.stdout + r.stderr).splitlines()[-12:]
        metrics["err_tail"] = tail
    return metrics


def main():
    levels = [int(x) for x in sys.argv[1:]] or [4, 2, 8]
    key = "PE_NUM"
    backup = CFG + ".bak_exp"
    shutil.copy(CFG, backup)
    original = json.load(open(CFG))
    results = []
    try:
        for v in levels:
            tag = f"{key.lower()}_{v}"
            cfg = dict(original)
            cfg[key] = v
            json.dump(cfg, open(CFG, "w"), indent=4)
            print(f"\n===== {key}={v} (baseline={original[key]}) =====", flush=True)
            m = run_variant(tag, key, v)
            results.append(m)
            print(json.dumps(m, ensure_ascii=False, default=str)[:600], flush=True)
    finally:
        shutil.move(backup, CFG)
        print(f"\n[config json 已恢复原状]")
    print("\n==== 汇总 ====")
    hdr = ["tag", key, "cycles", "model_hex_len", "tile_markers", "mid_buf", "cosine", "ok"]
    print(" | ".join(f"{h:>14}" for h in hdr))
    for m in results:
        print(" | ".join(f"{str(m.get(h, '-')):>14}" for h in hdr))
    out = os.path.join(PIPE, "experiments", "hw_sensitivity_result.json")
    json.dump(results, open(out, "w"), ensure_ascii=False, indent=2, default=str)
    print(f"→ {out}")


if __name__ == "__main__":
    main()
