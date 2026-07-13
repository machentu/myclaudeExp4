#!/usr/bin/env python
# plot_traj.py — Umeyama-align two TUM trajectories and save a comparison PNG.
# Usage: python plot_traj.py <mine.txt> <ref.txt> <out.png> [title]
#
# Produces a 2-panel figure: (left) top-down XY overlay, (right) 3D trajectory
# overlay, with ATE RMSE / scale / path length in the title.
import sys, numpy as np
import matplotlib
matplotlib.use("Agg")           # headless: save PNG, no display
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D  # noqa: F401 (registers 3d projection)

def load(path):
    ts, p = [], []
    for line in open(path, encoding='utf-8'):
        line = line.strip()
        if not line or line.startswith('#'): continue
        f = line.split()
        if len(f) < 8: continue
        try: t = float(f[0]); xyz = [float(f[1]), float(f[2]), float(f[3])]
        except ValueError: continue
        if any(v != v or abs(v) > 1e6 for v in xyz): continue
        ts.append(t); p.append(xyz)
    return np.array(ts), np.array(p)

def umeyama(A, B):
    muA, muB = A.mean(0), B.mean(0)
    H = (A - muA).T @ (B - muB) / A.shape[0]
    U, S, Vt = np.linalg.svd(H)
    R = Vt.T @ U.T
    if np.linalg.det(R) < 0: Vt[-1] *= -1; R = Vt.T @ U.T
    var = ((A - muA) ** 2).sum() / A.shape[0]
    s = (S.sum()) / var if var > 0 else 1.0
    t = muB - s * R @ muA
    return s, R, t

def plen(P): return np.sum(np.linalg.norm(np.diff(P, axis=0), axis=1))

mine_path, ref_path, out_png = sys.argv[1], sys.argv[2], sys.argv[3]
title = sys.argv[4] if len(sys.argv) > 4 else "trajectory comparison"

_, A = load(mine_path)      # mine (refapp)
_, B = load(ref_path)       # reference (NGD-SLAM)
s, R, t = umeyama(A, B)
Ahat = (s * (R @ A.T).T + t)
ate = np.sqrt(((Ahat - B) ** 2).sum(1).mean())

fig = plt.figure(figsize=(13, 5.5))
fig.suptitle(f"{title}   |   ATE RMSE = {ate*100:.2f} cm   scale = {s:.4f}"
             f"   |   path mine = {plen(A):.2f} m   ref = {plen(B):.2f} m",
             fontsize=11)

# ---- left: top-down XY ----
ax1 = fig.add_subplot(1, 2, 1)
ax1.plot(B[:, 0], B[:, 1], 'k-',  lw=2,   label='NGD-SLAM (ref)')
ax1.plot(Ahat[:, 0], Ahat[:, 1], '-', color='#1f9c4a', lw=1.6, label='refapp (mine)')
ax1.plot(B[0, 0], B[0, 1], 'ko', ms=7); ax1.plot(Ahat[0, 0], Ahat[0, 1], 'o', color='#1f9c4a', ms=6)
ax1.set_title("Top-down (XY)"); ax1.set_xlabel("x [m]"); ax1.set_ylabel("y [m]")
ax1.set_aspect('equal', adjustable='datalim'); ax1.grid(alpha=0.3); ax1.legend(fontsize=9)

# ---- right: 3D ----
ax2 = fig.add_subplot(1, 2, 2, projection='3d')
ax2.plot(B[:, 0], B[:, 1], B[:, 2], 'k-',  lw=2,   label='NGD-SLAM (ref)')
ax2.plot(Ahat[:, 0], Ahat[:, 1], Ahat[:, 2], '-', color='#1f9c4a', lw=1.6, label='refapp (mine)')
ax2.set_title("3D trajectory"); ax2.set_xlabel("x"); ax2.set_ylabel("y"); ax2.set_zlabel("z")
ax2.legend(fontsize=9)

fig.tight_layout(rect=[0, 0, 1, 0.94])
fig.savefig(out_png, dpi=130)
print(f"ATE RMSE = {ate*100:.2f} cm   scale = {s:.4f}")
print(f"saved: {out_png}")
