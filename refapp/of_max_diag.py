#!/usr/bin/env python
# of_max_diag.py — per-frame residual: pure-C full path vs OpenCV, locate max-outlier frames.
import numpy as np
from ate import load_tum, umeyama

GT  = r'C:\Users\frank.tu\Downloads\rgbd_dataset_freiburg3_walking_xyz\groundtruth.txt'
DIR = r'C:\Users\frank.tu\workspace\trae_study\slam-splat\NGD-SLAM\refapp\build\Release'

def assoc(tsA, PA, tsB, PB, max_dt=0.02):
    j = 0; d = {}
    for i, t in enumerate(tsA):
        while j < len(tsB) and tsB[j] < t - max_dt: j += 1
        if j < len(tsB) and abs(tsB[j] - t) <= max_dt:
            d[round(t, 3)] = (PA[i], PB[j])
    return d

gt_t, G = load_tum(GT)

def errs(tag):
    ts, m = load_tum(DIR + '\\' + tag + '.txt')
    d = assoc(ts, m, gt_t, G)
    t = sorted(d); A = np.array([d[k][0] for k in t]); B = np.array([d[k][1] for k in t])
    s, R, tr = umeyama(A, B); Ahat = s * (R @ A.T).T + tr
    e = np.sqrt(((Ahat - B) ** 2).sum(1))
    return {t[i]: e[i] for i in range(len(t))}

m = errs('refine_on')   # pure-C full (1.82cm)
r = errs('C0')          # OpenCV (1.61cm)
common = sorted(set(m) & set(r))
rows = [(t, m[t], r[t]) for t in common]
rows.sort(key=lambda x: -x[1])
print(f"{'time':>12} {'pureC_cm':>9} {'opencv_cm':>10} {'diff_cm':>8}")
for t, me, re in rows[:15]:
    print(f"{t:>12.3f} {me*100:>9.2f} {re*100:>10.2f} {(me-re)*100:>8.2f}")
print(f"\npureC: max={max(m.values())*100:.2f}cm  p95={np.percentile(list(m.values()),95)*100:.2f}cm")
print(f"opencv: max={max(r.values())*100:.2f}cm  p95={np.percentile(list(r.values()),95)*100:.2f}cm")
# how many frames pureC > 3cm
big = [t for t in common if m[t] > 0.03]
print(f"pureC >3cm frames: {len(big)}  (opencv same frames avg: {np.mean([r[t] for t in big])*100:.2f}cm)")
