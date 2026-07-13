#!/usr/bin/env python
# compare_traj.py — Umeyama-align two TUM trajectories and report ATE RMSE.
# Matches frames by timestamp (both come from the same association file).
import sys, numpy as np

def load(path):
    ts, p = [], []
    for line in open(path, encoding='utf-8'):
        line = line.strip()
        if not line or line.startswith('#'): continue
        f = line.split()
        if len(f) < 8: continue
        try:
            t = float(f[0]); xyz = [float(f[1]), float(f[2]), float(f[3])]
        except ValueError:
            continue  # skip NaN/inf lines
        if any(v != v or abs(v) > 1e6 for v in xyz): continue  # NaN or absurd
        ts.append(t); p.append(xyz)
    return np.array(ts), np.array(p)

def umeyama(A, B):
    # align A -> B: minimize || s*R*A_i + t - B_i ||^2 ; returns s,R,t
    muA, muB = A.mean(0), B.mean(0)
    H = (A - muA).T @ (B - muB) / A.shape[0]
    U, S, Vt = np.linalg.svd(H)
    R = Vt.T @ U.T
    if np.linalg.det(R) < 0:
        Vt[-1] *= -1; R = Vt.T @ U.T
    var = ((A - muA) ** 2).sum() / A.shape[0]
    s = (S.sum()) / var if var > 0 else 1.0
    t = muB - s * R @ muA
    return s, R, t

a_ts, a = load(sys.argv[1])   # mine
b_ts, b = load(sys.argv[2])   # reference
# match by timestamp
ta = {round(t,6): i for i,t in enumerate(a_ts)}
tb = {round(t,6): i for i,t in enumerate(b_ts)}
common = sorted(set(ta) & set(tb))
ia = np.array([ta[t] for t in common]); ib = np.array([tb[t] for t in common])
A, B = a[ia], b[ib]
print(f"mine={len(a)} frames, ref={len(b)} frames, matched={len(common)}")
print(f"mine extent  x[{A[:,0].min():.2f}..{A[:,0].max():.2f}] y[{A[:,1].min():.2f}..{A[:,1].max():.2f}] z[{A[:,2].min():.2f}..{A[:,2].max():.2f}]")
print(f"ref  extent  x[{B[:,0].min():.2f}..{B[:,0].max():.2f}] y[{B[:,1].min():.2f}..{B[:,1].max():.2f}] z[{B[:,2].min():.2f}..{B[:,2].max():.2f}]")
# path length
def plen(P): return np.sum(np.linalg.norm(np.diff(P,axis=0),axis=1))
print(f"path length  mine={plen(A):.3f} m   ref={plen(B):.3f} m")
s,R,t = umeyama(A, B)
Ahat = (s * (R @ A.T).T + t)
ate = np.sqrt(((Ahat - B)**2).sum(1).mean())
print(f"ATE RMSE (after Umeyama sR+t): {ate*100:.2f} cm   scale={s:.4f}")
# also raw (un-aligned) start-point error
print(f"start  mine={A[0]}  ref={B[0]}")
