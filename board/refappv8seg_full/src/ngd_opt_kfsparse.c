/* ngd_opt_kfsparse.c — 优化①: 降 KF 率 (2026-10-12)
 *
 * 背景 (perf_analysis_v34.md): v34 全量 827 帧 lm 局部建图 9195ms/KF × 108 KF
 * = 47% 总耗时, 是最大单点。KF 率 108/827 ≈ 每 7.6 帧一个 KF。
 *
 * 策略: 硬性最小 KF 间隔 NGD_KF_MIN_INTERVAL 帧 (默认 12 → 上限 ~69 KF,
 * 直省 lm 总量 ~1/3 ≈ 总耗时 -15%)。应急阀: mnMatchesInliers < 20 时放行 —
 * 这是 c5 首款 (tracking.c:425, 跟踪将死的紧急建图), 不拦, 保跟踪不丢。
 *
 * 语义: 等价于把 ORB-SLAM3 的 mMinFrames 调大 (原 c1b 语义, 非新发明);
 * 对词袋/回环的影响 = KF 密度下降, **必须 ATE 回归验证** (head100 + 全量)。
 *
 * 激活: build 脚本 -DNGD_KF_SPARSE=1 (默认编译不激活 = 与 v34 完全一致)。
 * 钩子位置: refactor/src/tracking.c::ngd_tracking_need_new_keyframe 顶部
 *           (reloc cooldown 之后), 3 行宏守卫调用本文件。
 */

#include "ngd/tracking.h"

#ifndef NGD_KF_MIN_INTERVAL
#define NGD_KF_MIN_INTERVAL 12      /* 最小 KF 间隔 (帧); 827/12 → ≤69 KF */
#endif
#ifndef NGD_KF_EMERGENCY_INLIERS
#define NGD_KF_EMERGENCY_INLIERS 20 /* c5 首款阈值: 低于此放行 (跟踪急救) */
#endif

/* 返回 1 = 拦下本次建帧 (间隔未到且非紧急); 0 = 放行。 */
int ngd_opt_kf_sparse_gate(ngd_tracking *tk)
{
    if (!tk || !tk->current) return 0;

    /* 急救阀: c5 首款同阈 (mnMatchesInliers < 20), 不拦 */
    if (tk->mnMatchesInliers < NGD_KF_EMERGENCY_INLIERS) return 0;

    /* 首帧/无参考: mnLastKeyFrameId 尚未设置过 (巨大值) → 放行
     * (与原版 c1b 对 mnLastKeyFrameId 初值语义一致) */
    if (tk->mnLastKeyFrameId > tk->current->mnId) return 0;

    return (tk->current->mnId < tk->mnLastKeyFrameId + (uint64_t)NGD_KF_MIN_INTERVAL) ? 1 : 0;
}
