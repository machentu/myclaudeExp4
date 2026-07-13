#!/usr/bin/env python3
# per_frame_residual.py — per-frame transl+rot residual after Umeyama alignment.
# Localizes which frames diverge and whether jitter is spiky (control-flow) or diffuse (numerics/ORB).
#
# Usage: python per_frame_residual.py <mine.tum> <reference.tum> [out_csv]
# mine/ref are TUM: timestamp tx ty tz qx qy qz qw  (Twc, camera-in-world)
import sys, numpy as np

def load(path):
    ts, p, q = [], [], []
    for line in open(path, encoding='utf-8'):
        line = line.strip()
        if not line or line.startswith('#'): continue
        f = line.split()
        if len(f) < 8: continue
        try:
            t = float(f[0]); xyz = [float(f[1]), float(f[2]), float(f[3])]
            quat = [float(f[4]), float(f[5]), float(f[6]), float(f[7])]  # qx qy qz qw
        except ValueError:
            continue
        if any(v != v or abs(v) > 1e6 for v in xyz): continue
        ts.append(t); p.append(xyz); q.append(quat)
    return np.array(ts), np.array(p), np.array(q)

def quat_to_R(q):
    # q = [qx qy qz qw]
    qx, qy, qz, qw = q
    n = qw*qw + qx*qx + qy*qy + qz*qz
    if n < 1e-12: return np.eye(3)
    s = 2.0/n
    R = np.array([
        [1-s*(qy*qy+qz*qz),   s*(qx*qy-qz*qw),   s*(qx*qz+qy*qw)],
        [s*(qx*qy+qz*qw), 1-s*(qx*qx+qz*qz),     s*(qy*qz-qx*qw)],
        [s*(qx*qz-qy*qw),   s*(qy*qz+qx*qw), 1-s*(qx*qx+qy*qy)]])
    return R

def umeyama(A, B):
    muA, muB = A.mean(0), B.mean(0)
    H = (A - muA).T @ (B - muB) / A.shape[0]
    U, S, Vt = np.linalg.svd(H)
    R = Vt.T @ U.T
    if np.linalg.det(R) < 0:
        Vt[-1] *= -1; R = Vt.T @ U.T
    var = ((A - muA)**2).sum() / A.shape[0]
    s = S.sum()/var if var > 0 else 1.0
    t = muB - s*R@muA
    return s, R, t

a_ts, a_p, a_q = load(sys.argv[1])
b_ts, b_p, b_q = load(sys.argv[2])
ta = {round(t,6): i for i,t in enumerate(a_ts)}
tb = {round(t,6): i for i,t in enumerate(b_ts)}
common = sorted(set(ta) & set(tb))
ia = [ta[t] for t in common]; ib = [tb[t] for t in common]
A, B = a_p[ia], b_p[ib]
Aq, Bq = a_q[ia], b_q[ib]
n = len(common)
print(f"mine={len(a_ts)} frames, ref={len(b_ts)} frames, matched={n}")

s, R, t = umeyama(A, B)
Ahat = (s*(R@A.T)).T + t
d = np.linalg.norm(Ahat - B, axis=1)            # per-frame transl err (m)
sse = (d**2).sum()
ate_rmse = np.sqrt(sse/n)
print(f"\n=== TRANSLATION residual (after Umeyama s={s:.4f}) ===")
print(f"ATE RMSE = {ate_rmse*100:.3f} cm   mean={d.mean()*100:.3f}  median={np.median(d)*100:.3f}  p95={np.percentile(d,95)*100:.3f}  max={d.max()*100:.3f} (all cm)")

# per-frame rotation residual under same global R alignment
rot_deg = np.zeros(n)
for i in range(n):
    Rm = quat_to_R(Aq[i]); Rr = quat_to_R(Bq[i])
    Rd = R@Rm@Rr.T                                   # aligned mine-rot relative to ref-rot
    tr = np.clip((np.trace(Rd)-1)/2, -1, 1)
    rot_deg[i] = np.degrees(np.arccos(tr))
print(f"\n=== ROTATION residual (deg) ===")
print(f"mean={rot_deg.mean():.4f}  median={np.median(rot_deg):.4f}  p95={np.percentile(rot_deg,95):.4f}  max={rot_deg.max():.4f}")

# shape classification: is the error concentrated in a few frames (spiky) or spread (diffuse)?
order = np.argsort(d)[::-1]
top3_share = d[order[:3]].sum()/sse if sse>0 else 0
med = np.median(d)
over5x = np.mean(d > 5*med) if med>0 else 0
# consecutive-frame jump (second derivative) — spiky jitter shows large frame-to-frame delta spikes
ddiff = np.abs(np.diff(d))
jump_p95 = np.percentile(ddiff,95)*100 if n>2 else 0
print(f"\n=== JITTER SHAPE ===")
print(f"top-3 frames hold {top3_share*100:.1f}% of total SSE")
print(f"frames >5x median: {over5x*100:.1f}%")
print(f"frame-to-frame |delta| p95 = {jump_p95:.3f} cm (large => discontinuous spikes)")
if top3_share > 0.25 or over5x > 0.05:
    print("=> SPIKY / discontinuous (points to control-flow: branch/KF decision divergence) -> Phase 3")
else:
    print("=> DIFFUSE / continuous (points to numerics/ORB drift) -> Phase 2/6")

print(f"\n=== TOP-20 worst frames (transl) ===")
print(f"{'idx':>5} {'timestamp':>16} {'transl_cm':>10} {'rot_deg':>9}")
for k in range(min(20,n)):
    i = order[k]
    print(f"{i:>5} {common[i]:>16.6f} {d[i]*100:>10.3f} {rot_deg[i]:>9.4f}")

if len(sys.argv) > 3:
    with open(sys.argv[3], 'w') as fo:
        fo.write("idx,timestamp,transl_cm,rot_deg\n")
        for i in range(n):
            fo.write(f"{i},{common[i]:.6f},{d[i]*100:.4f},{rot_deg[i]:.4f}\n")
    print(f"\nper-frame CSV written: {sys.argv[3]}")
