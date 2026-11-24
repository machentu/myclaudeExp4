#!/usr/bin/env python
# ATE + NEES computation with timestamp association, Umeyama alignment.
#
# ATE  : absolute trajectory error (position RMSE after sR+t alignment).
# NEES : Normalized Estimation Error Squared - consistency of pose uncertainty.
#        NEES = e^T Sigma^-1 e,  e = aligned position error vs GT,
#        Sigma = pose covariance (PnP approximation: Sigma = (J^T Omega J)^-1
#        from the cv::projectPoints reprojection jacobian over PnP inliers,
#        camera-frame [rvec;tvec] perturbation of T_cw).
#
#        Camera-frame -> world-frame position covariance:
#          G = d(t_wc)/d([drvec; dtvec]) = [-R_wc * sk(t_cw), -R_wc]   (3x6)
#          Sigma_p_world = G * Sigma * G^T
#        After Umeyama (s R_ali + t_ali, aligning estimate -> GT):
#          Sigma_aligned = s^2 * R_ali * Sigma_p_world * R_ali^T
#          e = s*R_ali*p_est + t_ali - p_gt
#          NEES = e^T * Sigma_aligned^-1 * e
#
#        Consistent estimator: mean NEES ~= dim (3 for position, 3 DoF);
#        per-frame NEES ~ chi2(3), 95% interval [0.352, 7.815].
#
# Usage:
#   python ate.py <mine.txt> <gt.txt>                  # ATE only
#   python ate.py <mine.txt> <gt.txt> <cov.txt>        # ATE + NEES
import sys, numpy as np

def load_tum(path):
    """TUM trajectory -> ts (N,), pos (N,3), quat (N,4) [qx,qy,qz,qw]."""
    ts, p, q = [], [], []
    for line in open(path, encoding='utf-8'):
        line = line.strip()
        if line.startswith('#'): continue
        if not line: continue
        f = line.split()
        if len(f) < 8: continue
        try:
            t = float(f[0])
            xyz = [float(f[1]), float(f[2]), float(f[3])]
            qtn = [float(f[4]), float(f[5]), float(f[6]), float(f[7])]
        except ValueError:
            continue
        if any(v != v or abs(v) > 1e6 for v in xyz):
            continue
        ts.append(t); p.append(xyz); q.append(qtn)
    return np.array(ts), np.array(p), np.array(q)

def load_cov(path):
    """PoseCovariance.txt -> ts (N,), cov (N,6,6), valid (N,).
    Format: 'ts valid c00 c01 ... c55' (row-major 6x6, camera-frame [rvec;tvec])."""
    ts, cov, valid = [], [], []
    for line in open(path, encoding='utf-8'):
        line = line.strip()
        if not line or line.startswith('#'): continue
        f = line.split()
        if len(f) < 2: continue
        try:
            t = float(f[0]); v = int(f[1])
            if len(f) != 38: continue   # ts + valid + 36
            c = [float(x) for x in f[2:38]]
        except ValueError:
            continue
        ts.append(t); valid.append(v); cov.append(c)
    return np.array(ts), np.array(cov).reshape(-1, 6, 6), np.array(valid)

def associate(tsA, tsB, max_dt=0.02):
    """Match frames by timestamp. Returns index arrays (ia, ib), |dt| <= max_dt."""
    ia, ib = [], []
    j = 0
    for i in range(len(tsA)):
        while j < len(tsB) and tsB[j] < tsA[i] - max_dt:
            j += 1
        if j >= len(tsB):
            break
        if abs(tsB[j] - tsA[i]) <= max_dt:
            ia.append(i); ib.append(j)
    return np.array(ia, dtype=int), np.array(ib, dtype=int)

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

def quat_to_R(q):
    """q = [qx,qy,qz,qw] -> 3x3 (rotation of Twc, camera->world)."""
    qx, qy, qz, qw = q
    return np.array([
        [1 - 2*(qy*qy + qz*qz), 2*(qx*qy - qz*qw),     2*(qx*qz + qy*qw)],
        [2*(qx*qy + qz*qw),     1 - 2*(qx*qx + qz*qz), 2*(qy*qz - qx*qw)],
        [2*(qx*qz - qy*qw),     2*(qy*qz + qx*qw),     1 - 2*(qx*qx + qy*qy)],
    ])

def skew(v):
    return np.array([[0, -v[2], v[1]],
                     [v[2], 0, -v[0]],
                     [-v[1], v[0], 0]])

if __name__ == '__main__':
    mine_path = sys.argv[1] if len(sys.argv) > 1 else "CameraTrajectory.txt"
    gt_path   = sys.argv[2] if len(sys.argv) > 2 else "groundtruth.txt"
    cov_path  = sys.argv[3] if len(sys.argv) > 3 else None

    ts_m, M, Qm = load_tum(mine_path)
    ts_g, G, _  = load_tum(gt_path)
    print(f'Mine: {M.shape[0]} poses, t=[{ts_m[0]:.3f}..{ts_m[-1]:.3f}]')
    print(f'GT:   {G.shape[0]} poses, t=[{ts_g[0]:.3f}..{ts_g[-1]:.3f}]')

    ia, ib = associate(ts_m, ts_g, max_dt=0.02)
    A, B = M[ia], G[ib]
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

    # ---- NEES (Normalized Estimation Error Squared) ----
    if cov_path:
        ts_c, Cov, Valid = load_cov(cov_path)
        print(f'Cov:  {Cov.shape[0]} rows, {int(Valid.sum())} valid, '
              f't=[{ts_c[0]:.3f}..{ts_c[-1]:.3f}]')
        # cov shares ts with mine trajectory (same relTimes source) -> match by ts.
        tc = {round(float(x), 6): i for i, x in enumerate(ts_c)}
        nees = []
        n_cov_match = 0
        for k in range(len(ia)):
            mi = ia[k]
            ci = tc.get(round(float(ts_m[mi]), 6))
            if ci is None: continue
            if not Valid[ci]: continue
            n_cov_match += 1
            # mine Twc -> T_cw (camera->world inverted to world->camera)
            R_wc = quat_to_R(Qm[mi])        # Twc rotation
            pos_wc = M[mi]                  # Twc.t = camera position in world
            R_cw = R_wc.T
            t_cw = -R_cw @ pos_wc           # T_cw translation
            # G = d(t_wc)/d([drvec; dtvec]) = [-R_wc * sk(t_cw), -R_wc]   (3x6)
            Gp = np.hstack([-R_wc @ skew(t_cw), -R_wc])
            Sig_p = Gp @ Cov[ci] @ Gp.T     # world-frame position covariance (3x3)
            # Umeyama alignment of estimate -> GT: p_aligned = s*R_ali*p + t_ali
            Sig_a = (s * s) * (R @ Sig_p @ R.T)
            e = s * (R @ pos_wc) + t - B[k]
            try:
                nees_i = float(e @ np.linalg.inv(Sig_a) @ e)
            except np.linalg.LinAlgError:
                continue
            if nees_i == nees_i and np.isfinite(nees_i):
                nees.append(nees_i)
        nees = np.array(nees)
        if nees.size > 0:
            chi2_lo, chi2_hi = 0.352, 7.815   # chi2(3) 95% two-sided
            in95 = np.mean((nees >= chi2_lo) & (nees <= chi2_hi)) * 100
            print('')
            print(f'NEES frames:  {nees.size}/{len(ia)} (cov matched {n_cov_match})')
            print(f'NEES mean:    {nees.mean():.3f}   (consistent ~= 3.0)')
            print(f'NEES median:  {np.median(nees):.3f}')
            print(f'NEES std:     {nees.std():.3f}')
            print(f'NEES min/max: {nees.min():.3f} / {nees.max():.3f}')
            print(f'NEES in 95%:  {in95:.1f}%  (chi2(3) [{chi2_lo}, {chi2_hi}])')
            if nees.mean() > 5.0:
                print('  -> overconfident (covariance underestimated; mean >> 3)')
            elif nees.mean() < 1.0:
                print('  -> underconfident (covariance overestimated; mean << 3)')
            else:
                print('  -> near-consistent (mean near 3)')
        else:
            print('NEES: no valid covariance frames matched.')
