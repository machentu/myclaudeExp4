#!/usr/bin/env python
# Plot two trajectories against groundtruth on the same chart.
# Usage: python plot_dual.py <traj1.txt> <label1> <traj2.txt> <label2> <gt.txt> <out.png> [title]
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

mine1_path, lab1, mine2_path, lab2, gt_path, out_png = sys.argv[1:7]
title = sys.argv[7] if len(sys.argv) > 7 else "dual comparison"

ts1, T1 = load_tum(mine1_path)
ts2, T2 = load_tum(mine2_path)
tsG, GT = load_tum(gt_path)

# Align each to GT independently
A1, B1 = associate(ts1, T1, tsG, GT)
A2, B2 = associate(ts2, T2, tsG, GT)
s1, R1, t1 = umeyama(A1, B1)
s2, R2, t2 = umeyama(A2, B2)
H1 = (s1 * (R1 @ T1.T).T + t1)
H2 = (s2 * (R2 @ T2.T).T + t2)

# ATE (on associated pairs)
err1 = np.sqrt(((A1.shape[0] > 0 and (s1 * (R1 @ A1.T).T + t1) - B1) ** 2).sum(1))
err2 = np.sqrt(((A2.shape[0] > 0 and (s2 * (R2 @ A2.T).T + t2) - B2) ** 2).sum(1))
ate1_rmse = np.sqrt(err1.mean()) if len(err1) else 0
ate2_rmse = np.sqrt(err2.mean()) if len(err2) else 0
ate1_med  = np.median(err1) if len(err1) else 0
ate2_med  = np.median(err2) if len(err2) else 0

COLOR1 = '#1f9c4a'   # green: pure-C
COLOR2 = '#e74c3c'   # red:   OpenCV ref

fig = plt.figure(figsize=(17, 11))

fig.suptitle(f"{title}   |   GT path = {plen(GT):.2f} m   "
             f"{lab1} path = {plen(H1):.2f} m   {lab2} path = {plen(H2):.2f} m\n"
             f"{lab1}: ATE RMSE={ate1_rmse*100:.1f}cm  median={ate1_med*100:.1f}cm   |   "
             f"{lab2}: ATE RMSE={ate2_rmse*100:.1f}cm  median={ate2_med*100:.1f}cm",
             fontsize=10)

# ---- top-left: XY ----
ax1 = fig.add_subplot(2, 3, 1)
ax1.plot(GT[:, 0],  GT[:, 1],  'k-',  lw=2.5, label='Groundtruth', zorder=1)
ax1.plot(H2[:, 0],  H2[:, 1],  '-',  color=COLOR2, lw=1.6, label=lab2, zorder=2)
ax1.plot(H1[:, 0],  H1[:, 1],  '-',  color=COLOR1, lw=1.4, label=lab1, zorder=3)
ax1.plot(GT[0, 0], GT[0, 1], 'ko', ms=8, zorder=4)
ax1.set_title("Top-down (XY)"); ax1.set_xlabel("x [m]"); ax1.set_ylabel("y [m]")
ax1.set_aspect('equal', adjustable='datalim'); ax1.grid(alpha=0.3); ax1.legend(fontsize=8)

# ---- top-middle: XZ ----
ax2 = fig.add_subplot(2, 3, 2)
ax2.plot(GT[:, 0],  GT[:, 2],  'k-',  lw=2.5, label='Groundtruth')
ax2.plot(H2[:, 0],  H2[:, 2],  '-',  color=COLOR2, lw=1.6, label=lab2)
ax2.plot(H1[:, 0],  H1[:, 2],  '-',  color=COLOR1, lw=1.4, label=lab1)
ax2.set_title("Side (XZ)"); ax2.set_xlabel("x [m]"); ax2.set_ylabel("z [m]")
ax2.set_aspect('equal', adjustable='datalim'); ax2.grid(alpha=0.3)

# ---- top-right: YZ ----
ax3 = fig.add_subplot(2, 3, 3)
ax3.plot(GT[:, 1],  GT[:, 2],  'k-',  lw=2.5, label='Groundtruth')
ax3.plot(H2[:, 1],  H2[:, 2],  '-',  color=COLOR2, lw=1.6, label=lab2)
ax3.plot(H1[:, 1],  H1[:, 2],  '-',  color=COLOR1, lw=1.4, label=lab1)
ax3.set_title("Side (YZ)"); ax3.set_xlabel("y [m]"); ax3.set_ylabel("z [m]")
ax3.set_aspect('equal', adjustable='datalim'); ax3.grid(alpha=0.3)

# ---- bottom-left: 3D ----
ax4 = fig.add_subplot(2, 3, 4, projection='3d')
ax4.plot(GT[:, 0], GT[:, 1], GT[:, 2], 'k-',  lw=2.5, label='Groundtruth')
ax4.plot(H2[:, 0], H2[:, 1], H2[:, 2], '-',  color=COLOR2, lw=1.3, label=lab2)
ax4.plot(H1[:, 0], H1[:, 1], H1[:, 2], '-',  color=COLOR1, lw=1.0, label=lab1)
ax4.set_title("3D"); ax4.set_xlabel("x"); ax4.set_ylabel("y"); ax4.set_zlabel("z")
ax4.legend(fontsize=8)

# ---- bottom-middle: XY zoom (last 30% of trajectory) ----
n3 = len(GT) * 3 // 10
ax5 = fig.add_subplot(2, 3, 5)
ax5.plot(GT[-n3:, 0], GT[-n3:, 1], 'k-',  lw=2.5, label='GT')
ax5.plot(H2[-n3:, 0], H2[-n3:, 1], '-',  color=COLOR2, lw=1.6, label=lab2)
ax5.plot(H1[-n3:, 0], H1[-n3:, 1], '-',  color=COLOR1, lw=1.4, label=lab1)
ax5.set_title(f"XY zoom (last 30%)"); ax5.set_xlabel("x [m]"); ax5.set_ylabel("y [m]")
ax5.set_aspect('equal', adjustable='datalim'); ax5.grid(alpha=0.3); ax5.legend(fontsize=8)

# ---- bottom-right: error over time ----
ax6 = fig.add_subplot(2, 3, 6)
t_common = ts1[:min(len(H1), len(H2))]  # timestamps for H1
n_err = min(len(err1), len(err2), len(ts1), len(ts2))
ax6.plot(ts1[:n_err], err1[:n_err]*100, color=COLOR1, lw=0.6, alpha=0.8, label=lab1)
ax6.plot(ts2[:n_err], err2[:n_err]*100, color=COLOR2, lw=1.0, alpha=0.9, label=lab2)
ax6.axhline(y=ate1_rmse*100, color=COLOR1, ls='--', lw=1, alpha=0.6)
ax6.axhline(y=ate2_rmse*100, color=COLOR2, ls='--', lw=1, alpha=0.6)
ax6.set_title("ATE over time"); ax6.set_xlabel("timestamp"); ax6.set_ylabel("error [cm]")
ax6.grid(alpha=0.3); ax6.legend(fontsize=8)

fig.tight_layout(rect=[0, 0, 1, 0.93])
fig.savefig(out_png, dpi=150)
print(f"saved: {out_png}")
