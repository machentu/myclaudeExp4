#!/bin/bash
# compare_ate_xc01.sh — xc01 板端 base(MNN CPU+CPU ORB) vs npu(nnp310+VDSP) ATE 对比
# 结果目录在 NFS 直读 (板上写 /mnt/xc01/..., 宿主同名):
#   /home/frank/nfs/xc01/use_sdkv2/slamres/run_base/  (run_slam_xc01_base.sh 产物)
#   /home/frank/nfs/xc01/use_sdkv2/slamres/run_npu/   (run_slam_xc01_npu.sh  产物)
# 用法: bash compare_ate_xc01.sh [asso 默认 asso_head100.txt]
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"          # slampart/ (ate.py 所在)
R=/home/frank/nfs/xc01/use_sdkv2/slamres
GT=$R/rgbd_dataset_freiburg3_walking_xyz/groundtruth.txt
ASSO=${1:-asso_head100.txt}
echo "对比口径: $ASSO (GT: groundtruth.txt, Umeyama 对齐)"

for v in base npu; do
    T=$R/run_$v/CameraTrajectory.txt
    echo
    if [ -f "$T" ]; then
        echo "== run_$v ($(wc -l <"$T") 帧) =="
        python3 "$HERE/ate.py" "$T" "$GT" 2>&1 | tail -6
        # 耗时统计: main.cpp print_timing 输出在 board_$v.log, 抓 yolo/orb 每帧均值
        L=$R/run_$v/board_$v.log
        if [ -f "$L" ]; then
            # stage timing 表: yolo/orb/track/sum 行 + median tracking
            grep -E "^yolo|^orb|^track|^sum|median tracking" "$L"
        fi
    else
        echo "== run_$v: 缺 $T =="
    fi
done
