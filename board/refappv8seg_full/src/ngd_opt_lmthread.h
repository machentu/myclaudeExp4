/* ngd_opt_lmthread.h — [①] LocalMapping 工作线程对驱动层的最小 API
 * (宏 NGD_LM_THREAD, 实现见 ngd_opt_lmthread.c)。
 * 宏关时本头无内容, 驱动层钩子亦不编译 → 与基线完全一致。 */
#ifndef NGD_OPT_LMTHREAD_H
#define NGD_OPT_LMTHREAD_H

#ifdef __cplusplus
extern "C" {
#endif

struct ngd_keyframe;
struct ngd_mappoint;

/* 工作线程 → 主线程 完成事件 (retired 数组所有权转移, 主线程清扫后 free) */
typedef struct ngd_lm_done_ev {
    struct ngd_keyframe *kf;            /* 该次 run 的 currentKF (lc insert 用) */
    double t_ms;                        /* run 体耗时 (不含锁等待, 对齐内联口径) */
    struct ngd_mappoint **retired;      /* 所有权偷自 lm->retired_mps */
    int nRetired;
} ngd_lm_done_ev;

#if defined(NGD_LM_THREAD) && NGD_LM_THREAD

/* 返回 1 = 线程模式 (驱动层走 post/pump 路径); 0 = pthread 失败, 走原内联 */
int  ngd_lmthread_active(void);
int  ngd_lmthread_init(void *lm);       /* system init: local_mapping_init 之后调 */
void ngd_lmthread_shutdown(void);       /* system free: 任何地图释放之前调; 之后 pump 排空 */

int  ngd_lmthread_post(void *lm);       /* KF 块提交 (非阻塞: 满返回 0, 调用方 pump 后重试) */
int  ngd_lmthread_pop_done(ngd_lm_done_ev *out);  /* 非阻塞取回一条完成事件 */

/* 地图锁: 主线程跟踪读 RD / KF 创建升级 WR / lc insert+run WR */
void ngd_lmthread_map_rdlock(void);
void ngd_lmthread_map_rdunlock(void);
void ngd_lmthread_map_wrlock(void);
void ngd_lmthread_map_wrunlock(void);

/* ba.c ngd_local_ba 内部钩子: 进 BA solve 放锁, 出 solve 重取 WR */
void ngd_lmthread_solve_begin(void);
void ngd_lmthread_solve_end(void);

#endif /* NGD_LM_THREAD */

#ifdef __cplusplus
}
#endif
#endif /* NGD_OPT_LMTHREAD_H */
