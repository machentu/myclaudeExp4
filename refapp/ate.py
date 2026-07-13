#!/usr/bin/env python
# ATE computation with timestamp association, Umeyama alignment.
import sys, numpy as np

def load_tum(path):
    ts, p = [], []
    for line in open(path, encoding='utf-8'):
        line = line.strip()
        if line.startswith('#'): continue
        if not line: continue
        f = line.split()
        if len(f) < 8: continue
        try:
            t = float(f[0])
            xyz = [float(f[1]), float(f[2]), float(f[3])]
        except ValueError:
            continue
        if any(v != v or abs(v) > 1e6 for v in xyz):
            continue
        ts.append(t)
        p.append(xyz)
    return np.array(ts), np.array(p)

def associate(tsA, PA, tsB, PB, max_dt=0.02):
    matched_A, matched_B = [], []
    j = 0
    for i, t in enumerate(tsA):
        while j < len(tsB) and tsB[j] < t - max_dt:
            j += 1
        if j >= len(tsB):
            break
        if abs(tsB[j] - t) <= max_dt:
            matched_A.append(PA[i])
            matched_B.append(PB[j])
    return np.array(matched_A), np.array(matched_B)

def umeyama(A, B):
    muA, muB = A.mean(0), B.mean(0)
    H = (A - muA).T @ (B - muB) / A.shape[0]
    U, S, Vt = np.linalg.svd(H)
    R = Vt.T @ U.T
    if np.linalg.det(R) < 0:
        Vt[-1] *= -1
        R = Vt.T @ U.T
    var = ((A - muA) ** 2).sum() / A.shape[0]
    s = (S.sum()) / var if var > 0 else 1.0
    t = muB - s * R @ muA
    return s, R, t

if __name__ == '__main__':
    mine_path = sys.argv[1] if len(sys.argv) > 1 else "CameraTrajectory.txt"
    gt_path   = sys.argv[2] if len(sys.argv) > 2 else "groundtruth.txt"

    ts_m, M = load_tum(mine_path)
    ts_g, G = load_tum(gt_path)
    print(f'Mine: {M.shape[0]} poses, t=[{ts_m[0]:.3f}..{ts_m[-1]:.3f}]')
    print(f'GT:   {G.shape[0]} poses, t=[{ts_g[0]:.3f}..{ts_g[-1]:.3f}]')

    A, B = associate(ts_m, M, ts_g, G, max_dt=0.02)
    print(f'Associated: {A.shape[0]} pairs (dt <= 0.02s)')

    s, R, t = umeyama(A, B)
    Ahat = (s * (R @ A.T).T + t)
    errors = np.sqrt(((Ahat - B) ** 2).sum(1))
    ate_rmse   = np.sqrt((errors**2).mean())   # sqrt(mean(err^2)); was sqrt(mean(err)) (bug)
    ate_mean   = errors.mean()
    ate_median = np.median(errors)
    ate_std    = errors.std()
    ate_max    = errors.max()

    print(f'')
    print(f'ATE RMSE:   {ate_rmse*100:.2f} cm')
    print(f'ATE mean:   {ate_mean*100:.2f} cm')
    print(f'ATE median: {ate_median*100:.2f} cm')
    print(f'ATE std:    {ate_std*100:.2f} cm')
    print(f'ATE max:    {ate_max*100:.2f} cm')
    print(f'scale:      {s:.6f}')
