#!/usr/bin/env python
# Plot trajectory comparison with timestamp association.
# Usage: python plot_assoc.py <mine.txt> <ref.txt> <out.png> [title]
import sys, numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D  # noqa

def load_tum(path):
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

def associate(tsA, PA, tsB, PB, max_dt=0.02):
    matched_A, matched_B = [], []
    j = 0
    for i, t in enumerate(tsA):
        while j < len(tsB) and tsB[j] < t - max_dt: j += 1
        if j >= len(tsB): break
        if abs(tsB[j] - t) <= max_dt:
            matched_A.append(PA[i]); matched_B.append(PB[j])
    return np.array(matched_A), np.array(matched_B)

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

tsM, M = load_tum(mine_path)
tsR, R_raw = load_tum(ref_path)

# Timestamp association
A, B = associate(tsM, M, tsR, R_raw, max_dt=0.02)
print(f'Mine: {M.shape[0]} -> associated {A.shape[0]} pairs')

s, R, t = umeyama(A, B)
Ahat = (s * (R @ A.T).T + t)
errors = np.sqrt(((Ahat - B) ** 2).sum(1))
ate_rmse = np.sqrt(errors.mean())
ate_med  = np.median(errors)

# Also transform the full mine trajectory for plotting
Mhat = (s * (R @ M.T).T + t)

fig = plt.figure(figsize=(16, 5.5))
fig.suptitle(f"{title}   |   ATE RMSE = {ate_rmse*100:.2f} cm   median = {ate_med*100:.2f} cm"
             f"   |   path mine = {plen(M):.2f} m   ref = {plen(R_raw):.2f} m",
             fontsize=11)

# ---- left: top-down XY ----
ax1 = fig.add_subplot(1, 3, 1)
ax1.plot(B[:, 0], B[:, 1], 'k-',  lw=2,   label='Groundtruth')
ax1.plot(Mhat[:, 0], Mhat[:, 1], '-', color='#1f9c4a', lw=1.6, label='refapp')
ax1.plot(B[0, 0], B[0, 1], 'ko', ms=7); ax1.plot(Mhat[0, 0], Mhat[0, 1], 'o', color='#1f9c4a', ms=6)
ax1.set_title("Top-down (XY)"); ax1.set_xlabel("x [m]"); ax1.set_ylabel("y [m]")
ax1.set_aspect('equal', adjustable='datalim'); ax1.grid(alpha=0.3); ax1.legend(fontsize=9)

# ---- middle: error over time ----
ax2 = fig.add_subplot(1, 3, 2)
ax2.plot(tsM[:len(Ahat)], errors*100, color='#e74c3c', lw=0.8)
ax2.axhline(y=ate_rmse*100, color='k', ls='--', lw=1, label=f'RMSE {ate_rmse*100:.1f}cm')
ax2.set_title("ATE over time"); ax2.set_xlabel("timestamp"); ax2.set_ylabel("error [cm]")
ax2.grid(alpha=0.3); ax2.legend(fontsize=9)

# ---- right: 3D ----
ax3 = fig.add_subplot(1, 3, 3, projection='3d')
ax3.plot(B[:, 0], B[:, 1], B[:, 2], 'k-',  lw=2,   label='Groundtruth')
ax3.plot(Mhat[:, 0], Mhat[:, 1], Mhat[:, 2], '-', color='#1f9c4a', lw=1.6, label='refapp')
ax3.set_title("3D trajectory"); ax3.set_xlabel("x"); ax3.set_ylabel("y"); ax3.set_zlabel("z")
ax3.legend(fontsize=9)

fig.tight_layout(rect=[0, 0, 1, 0.94])
fig.savefig(out_png, dpi=140)
print(f"ATE RMSE = {ate_rmse*100:.2f} cm   median = {ate_med*100:.2f} cm")
print(f"saved: {out_png}")
