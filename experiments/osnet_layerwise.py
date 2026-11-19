"""osnet 逐层发散诊断:每个融合函数 ISS vs CPU 的 cosine 序列。
SubfunctionRunner 要求 inputs 覆盖 fused 函数全部自由参数(含 pXX 权重),
故进程内重建 s0-s4 拿 params_out 后再喂入。"""
import sys, os
import numpy as np
sys.path.insert(0, '/DEngine/projects/npu_pipeline')
os.chdir('/DEngine/projects/npu_pipeline')
os.environ.setdefault('NPUP_DUMP_TIR', '0')
from stages import s0_model, s1_front_end, s2_quantize, s4_build
ctx = s4_build.run(s2_quantize.run(s1_front_end.run(s0_model.run())))
infer = ctx['infer_input']
wd = ctx['build_dir']
inputs = {n: infer[n] for n in infer}
for k, v in (ctx.get('params_out') or {}).items():      # pXX 权重参数
    inputs[k] = v.asnumpy() if hasattr(v, 'asnumpy') else np.asarray(v)
os.chdir(wd)
from deepeye.relay_pass import dump_func_output
sim, cpu, weight = dump_func_output(wd, inputs, 'nnp318p', dump_cpu=True)

def cosine(a, b):
    a = np.asarray(a, dtype=np.float64).flatten()
    b = np.asarray(b, dtype=np.float64).flatten()
    if a.size != b.size:
        return float('nan')
    return float(np.dot(a, b) / (np.linalg.norm(a) * np.linalg.norm(b) + 1e-12))

lines = ["# osnet 逐层 ISS vs CPU cosine", "", "| # | fused func | cosine | max_abs |", "|---|---|---|---|"]
i = 0
worst = []
for k, sv in sim.items():
    cv = cpu.get(k)
    if cv is None:
        continue
    for s_, c_ in zip((sv if isinstance(sv, list) else [sv]),
                      (cv if isinstance(cv, list) else [cv])):
        c = cosine(s_, c_)
        d = float(np.max(np.abs(np.asarray(s_, dtype=np.float64) -
                                np.asarray(c_, dtype=np.float64)))) if c == c else float('nan')
        lines.append(f"| {i} | {k[:58]} | {c:.6f} | {d:.4f} |")
        worst.append((c, k))
        i += 1
open('/DEngine/projects/npu_pipeline/experiments/osnet_layerwise.md', 'w').write("\n".join(lines) + "\n")
worst.sort()
print(f"共 {i} 层;cosine 最低 8 层:")
for c, k in worst[:8]:
    print(f"  {c:.6f}  {k[:70]}")
