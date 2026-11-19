#!/usr/bin/env python3.8
"""exp_pm_depth — PM(程序存储器)容量 vs 运行周期数实验。

nnp318p 的 PM 只有 1024 行,模型代码超过就触发 hex_l2ld 运行时换页
(PDMA 搬代码块进 PM,每块 bl 执行,bb_ctrl 驱动循环有开销)。
本实验临时改 linker/linker_glob_nnp318p.py 的 pm_depth,对比不同 PM 容量下
同一模型的 ISS 实测周期数 → 量化换页开销,回答"PM 该做多大"。

用法: python3.8 experiments/exp_pm_depth.py <model_tag> [1024 2048 4096]
"""
import json
import os
import re
import shutil
import subprocess
import sys
import time

PIPE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GLOB = "/DEngine/projects/TyTVM_NNP3xx_src_v1.0.41/tytvm_src_pkg/deepeye/python/deepeye/linker/linker_glob_nnp318p.py"
ENV = dict(os.environ,
           PYTHONPATH=f"{PIPE}:{os.environ.get('PYTHONPATH', '')}",
           NPUP_TARGET="nnp318p", NPUP_DUMP_TIR="0", NPUP_RUN_TAG="")


def run_level(tag, pm):
    env = dict(ENV, NPUP_RUN_TAG=tag)
    t0 = time.time()
    r = subprocess.run([os.environ.get("PY", "/usr/bin/python3.8"),
                        os.path.join(PIPE, "stages", "s8_sim.py")],
                       env=env, cwd=PIPE, capture_output=True, text=True, timeout=3000)
    w = os.path.join(PIPE, "work", "nnp318p", tag, "s8_sim")
    m = {"tag": tag, "pm_depth": pm, "ok": r.returncode == 0,
         "wall_s": round(time.time() - t0, 1)}
    li = os.path.join(w, "link_info")
    if os.path.isfile(li):
        d = dict(l.split(": ") for l in open(li).read().splitlines() if ": " in l)
        m["model_hex_len"] = int(d["model_hex_len"])
        m["l2ld_json"] = os.path.isfile(os.path.join(w, "model_hex_l2ld_en.json"))
    log = os.path.join(w, "log_iss")
    if os.path.isfile(log):
        cyc = re.findall(r"cycle_cnt: (\d+)", open(log).read())
        if cyc:
            m["cycles"] = int(cyc[-1])
    cmp_f = os.path.join(w, "compare.txt")
    if os.path.isfile(cmp_f):
        c = re.search(r"cosine\(ISS,CPU\)=([\d.]+)", open(cmp_f).read())
        if c:
            m["cosine"] = float(c.group(1))
    if not m["ok"]:
        m["err_tail"] = (r.stdout + r.stderr).splitlines()[-8:]
    return m


def main():
    tag = sys.argv[1]
    levels = [int(x) for x in sys.argv[2:]] or [1024, 2048, 4096]
    backup = GLOB + ".bak_pm"
    shutil.copy(GLOB, backup)
    src = open(backup).read()
    results = []
    try:
        for pm in levels:
            open(GLOB, "w").write(re.sub(r"pm_depth = \d+", f"pm_depth = {pm}", src))
            print(f"\n===== pm_depth={pm} =====", flush=True)
            m = run_level(tag, pm)
            results.append(m)
            print(json.dumps(m, ensure_ascii=False, default=str)[:500], flush=True)
    finally:
        shutil.move(backup, GLOB)
        print("[linker_glob_nnp318p.py 已恢复]")
    print("\n==== 汇总 ====")
    print(f"{'pm_depth':>8} {'l2ld':>6} {'cycles':>12} {'model_hex':>10} {'cosine':>8}")
    base = next((r.get("cycles") for r in results if r.get("cycles")), None)
    for m in results:
        c = m.get("cycles", "-")
        delta = f"(-{base - c})" if base and isinstance(c, int) and c != base else ""
        print(f"{m['pm_depth']:>8} {str(m.get('l2ld_json','-')):>6} {str(c):>12}{delta:<10} "
              f"{str(m.get('model_hex_len','-')):>10} {str(m.get('cosine','-')):>8}")
    out = os.path.join(PIPE, "experiments", f"pm_depth_{tag}.json")
    json.dump(results, open(out, "w"), ensure_ascii=False, indent=2, default=str)
    print(f"→ {out}")


if __name__ == "__main__":
    main()
