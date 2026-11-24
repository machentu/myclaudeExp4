/* ngd_opt_lmthread.c — [①] LocalMapping 工作线程 (宏 NGD_LM_THREAD, 2026-10-13)
 *
 * 现状: 全管线单线程 (板有 2 核只用 1 核), ngd_local_mapping_run 在
 * system.cpp 帧循环内联执行 → 跟踪链 (~565s) 与 lm 链 (~547s, v41 口径)
 * 完全串行, 理想重叠后 wall ~11-13min。
 *
 * 方案: lm 搬到第二线程, 地图用 rwlock 保护, 并行窗口来自 BA solve 相位
 * (~98% lm 耗时, ngd_ba_optimize 全程私有副本, 不触碰地图):
 *
 *   主线程   RD: OF 跟踪 / ngd_tracking_track / push_rel+pose_cov (只读地图)
 *            WR: create_new_key_frame + stereo init (KF/MP 结构变更,
 *                tracking.c 内 RD→WR→RD 升级); lc insert+run + retired
 *                清扫 (lm_pump, system.cpp)
 *   工作线程 WR: cull/newmp/sin/kfc + BA gather/writeback (~200ms/次)
 *            无锁: BA solve — ba.c 内 solve_begin/end 钩子换锁
 *
 * 同步细节:
 *  - 事件携带 KF 指针: 主线程 create 会覆写 lm->currentKF, 工作线程须以
 *    事件恢复 (lmt_run_one 开头, WR 下)。
 *  - 完成事件偷走 lm->retired_mps 数组所有权, 主线程 pump 清扫 owned 帧
 *    槽位后统一 free (原内联语义的分层延续)。
 *  - 工作线程取下一件工作前等完成环排空: 保证 run(N+1) 的 culling 不可能
 *    碰到主线程尚未 lc-insert 的 kf_N (事件序 == 原内联提交序)。
 *  - 深度 2 背压: post 非阻塞, 满则主线程 pump+usleep 重试 (阻塞式 post
 *    与 worker 的"取件等排空"条件成环死锁 — v42 全量 frame 153 实测,
 *    晚段 LBA ~14s 内叠 3 个 KF 即触发)。
 *  - 回退: pthread_create 失败 → g_active=0, 全部入口空操作/内联退化。
 *
 * 数值口径: 单次 lm 计算与内联版逐字节相同, 但与跟踪的交错时序不同
 * (跟踪可能读到 writeback 前的地图状态, 等价 ORB-SLAM2 本身的并行 LM
 * 语义) → 非位级复现, 由 ATE 门禁验证。
 *
 * 钩子: system.cpp (OF/track/push_rel RD, KF 块 post+pump, init/shutdown),
 *       tracking.c (create_kf / stereo init WR 升级), ba.c (solve 前后换锁)。
 * 宏关 = 本文件为空翻译单元, 与基线完全一致 (.o md5 验证)。
 */
#if defined(NGD_LM_THREAD) && NGD_LM_THREAD

#include "ngd_opt_lmthread.h"

#include "ngd/localmapping.h"
#include "ngd/ba.h"
#include "ngd/map.h"
#include "ngd/keyframe.h"
#include "ngd/mappoint.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifndef NGD_LM_THREAD_QDEPTH
#define NGD_LM_THREAD_QDEPTH 2      /* 待处理事件背压深度 */
#endif
#define NGD_LM_DONE_CAP 8           /* 完成事件环形容量 (在飞上限 QDEPTH+1) */

static double lmt_now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e3 + ts.tv_nsec / 1e6;
}

static pthread_rwlock_t g_map_lock;
static pthread_mutex_t  g_qmtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t   g_qcv  = PTHREAD_COND_INITIALIZER;
static ngd_local_mapping *g_lm;
static pthread_t g_tid;
static volatile int g_active = 0, g_stop = 0;

typedef struct {
    struct ngd_keyframe *kf;
} ngd_lm_work_evt;

static ngd_lm_work_evt g_work[NGD_LM_THREAD_QDEPTH];
static int g_nwork = 0, g_busy = 0;             /* g_busy: worker 正在跑一件 */
static ngd_lm_done_ev g_done[NGD_LM_DONE_CAP];
static int g_done_head = 0, g_done_tail = 0;

int ngd_lmthread_active(void) { return g_active; }

/* [①] LM 忙 = 队列有件或正在跑 (need_new_keyframe 据此刹车, ORB-SLAM2
 * "LocalMapping not accepting" 语义) */
int ngd_lmthread_lm_busy(void)
{
    if (!g_active) return 0;
    pthread_mutex_lock(&g_qmtx);
    int busy = (g_nwork > 0) || g_busy;
    pthread_mutex_unlock(&g_qmtx);
    return busy;
}

/* ---- 地图锁 (模式统一的 unlock: pthread_rwlock_unlock 对读/写通用) ---- */
void ngd_lmthread_map_rdlock(void)   { if (g_active) pthread_rwlock_rdlock(&g_map_lock); }
void ngd_lmthread_map_rdunlock(void) { if (g_active) pthread_rwlock_unlock(&g_map_lock); }
void ngd_lmthread_map_wrlock(void)   { if (g_active) pthread_rwlock_wrlock(&g_map_lock); }
void ngd_lmthread_map_wrunlock(void) { if (g_active) pthread_rwlock_unlock(&g_map_lock); }

/* ba.c ngd_local_ba 钩子: 进 solve 放锁 (solve 全程私有副本), 出 solve
 * 重取 WR (writeback 结构变更)。仅工作线程的 LBA 路径会走到;
 * GBA (ngd_global_ba) 无钩子, 在主线程 WR 下整段执行。 */
void ngd_lmthread_solve_begin(void)  { if (g_active) pthread_rwlock_unlock(&g_map_lock); }
void ngd_lmthread_solve_end(void)    { if (g_active) pthread_rwlock_wrlock(&g_map_lock); }

/* ---- 工作线程: 复刻 ngd_local_mapping_run 主体 (localmapping.c:452-482) ---- */
static void lmt_run_one(const ngd_lm_work_evt *ev)
{
    ngd_local_mapping *lm = g_lm;
    ngd_keyframe *kf = ev->kf;

    pthread_rwlock_wrlock(&g_map_lock);
    double t0 = lmt_now();                       /* 计时不含锁等待, 对齐内联口径 */

    lm->currentKF = kf;   /* 事件恢复: 主线程后续 create 已覆写该字段 */

    ngd_local_mapping_map_point_culling(lm);
#if NGD_BOW
    ngd_local_mapping_create_new_map_points(lm);
    ngd_local_mapping_search_in_neighbors(lm);
    ngd_local_mapping_keyframe_culling(lm);
#endif
    if (lm->map && lm->currentKF && lm->map->nKFs >= 4) {
        ngd_local_ba(lm->currentKF, lm->map, NULL);
        /* 返回时持有 WR (solve_end 钩子已重取), writeback 已完成 */
    }

    ngd_lm_done_ev dv;
    dv.kf = kf;
    dv.t_ms = lmt_now() - t0;
    dv.retired = lm->retired_mps;                /* 所有权偷走, 主线程清扫后 free */
    dv.nRetired = lm->nRetired;
    lm->retired_mps = NULL; lm->nRetired = 0; lm->capRetired = 0;

    pthread_rwlock_unlock(&g_map_lock);

    pthread_mutex_lock(&g_qmtx);
    while ((g_done_tail + 1) % NGD_LM_DONE_CAP == g_done_head && !g_stop)
        pthread_cond_wait(&g_qcv, &g_qmtx);      /* 满则等主线程排空 */
    if ((g_done_tail + 1) % NGD_LM_DONE_CAP == g_done_head) {
        /* 实际不可达 (在飞 ≤ QDEPTH+1 < CAP 且主线程逐帧排空);
         * 停机兜底: 丢弃最旧并释放其 retired, 防死锁 */
        ngd_lm_done_ev *old = &g_done[g_done_head];
        for (int r = 0; r < old->nRetired; ++r) ngd_mappoint_free(old->retired[r]);
        free(old->retired);
        g_done_head = (g_done_head + 1) % NGD_LM_DONE_CAP;
        fprintf(stderr, "[lmthr] WARN done-ring overflow, dropped oldest\n");
    }
    g_done[g_done_tail] = dv;
    g_done_tail = (g_done_tail + 1) % NGD_LM_DONE_CAP;
    g_busy = 0;                                  /* 一件完成 (LM 空闲窗起点) */
    pthread_mutex_unlock(&g_qmtx);
    pthread_cond_broadcast(&g_qcv);

    fprintf(stderr, "[lmthr] kf=%llu run=%.1fms nret=%d\n",
            (unsigned long long)kf->mnId, dv.t_ms, dv.nRetired);
}

static void *lmt_worker(void *arg)
{
    (void)arg;
    for (;;) {
        pthread_mutex_lock(&g_qmtx);
        for (;;) {
            if (g_nwork > 0 && (g_stop || g_done_head == g_done_tail))
                break;                           /* 有活且上一件已交付 (停机豁免) */
            if (g_nwork == 0 && g_stop)
                goto worker_exit;                /* 无活且停 → 退出 */
            pthread_cond_wait(&g_qcv, &g_qmtx);  /* 等活 / 等排空 */
        }
        ngd_lm_work_evt ev = g_work[0];
        for (int i = 1; i < g_nwork; ++i) g_work[i - 1] = g_work[i];  /* FIFO */
        g_nwork--;
        g_busy = 1;
        pthread_mutex_unlock(&g_qmtx);
        pthread_cond_broadcast(&g_qcv);

        lmt_run_one(&ev);
    }
worker_exit:
    pthread_mutex_unlock(&g_qmtx);
    return NULL;
}

/* ---- 主线程侧 ---- */
/* 非阻塞提交: 返回 1 = 已入队; 0 = 队列满 (调用方先 pump 排空完成环再重试
 * — worker 取件条件是完成环为空, 满时若 post 阻塞等待会成环死锁:
 * v42 全量在 frame 153 实测, 晚段 LBA ~14s 内叠 3 个 KF 触发)。 */
int ngd_lmthread_post(void *lmv)
{
    ngd_local_mapping *lm = (ngd_local_mapping *)lmv;
    if (!g_active) { ngd_local_mapping_run(lm); return 1; }   /* 回退: 内联 */

    ngd_lmthread_map_rdlock();                   /* 与工作线程恢复写互斥 */
    ngd_keyframe *kf = lm->currentKF;
    ngd_lmthread_map_rdunlock();

    pthread_mutex_lock(&g_qmtx);
    if (g_nwork == NGD_LM_THREAD_QDEPTH) {
        pthread_mutex_unlock(&g_qmtx);
        return 0;
    }
    g_work[g_nwork].kf = kf;
    g_nwork++;
    pthread_mutex_unlock(&g_qmtx);
    pthread_cond_broadcast(&g_qcv);
    return 1;
}

int ngd_lmthread_pop_done(ngd_lm_done_ev *out)
{
    if (!g_active) return 0;
    pthread_mutex_lock(&g_qmtx);
    int got = (g_done_head != g_done_tail);
    if (got) {
        *out = g_done[g_done_head];
        g_done_head = (g_done_head + 1) % NGD_LM_DONE_CAP;
    }
    pthread_mutex_unlock(&g_qmtx);
    if (got) pthread_cond_broadcast(&g_qcv);     /* 唤醒等排空的工作线程 */
    return got;
}

int ngd_lmthread_init(void *lmv)
{
    g_lm = (ngd_local_mapping *)lmv;
    if (pthread_rwlock_init(&g_map_lock, NULL) != 0) return 0;

    pthread_attr_t at;
    pthread_attr_init(&at);
    pthread_attr_setstacksize(&at, 1 << 20);     /* 板端 RAM 紧, 1MB 栈够 BA 胶水 */
    int rc = pthread_create(&g_tid, &at, lmt_worker, NULL);
    pthread_attr_destroy(&at);
    if (rc != 0) {
        pthread_rwlock_destroy(&g_map_lock);
        fprintf(stderr, "[lmthr] pthread_create failed (%d), inline fallback\n", rc);
        return 0;
    }
    g_active = 1;
    fprintf(stderr, "[lmthr] worker started (qdepth=%d, done_cap=%d)\n",
            NGD_LM_THREAD_QDEPTH, NGD_LM_DONE_CAP);
    return 1;
}

void ngd_lmthread_shutdown(void)
{
    if (!g_active) return;
    pthread_mutex_lock(&g_qmtx);
    g_stop = 1;
    int pend = g_nwork;
    pthread_mutex_unlock(&g_qmtx);
    pthread_cond_broadcast(&g_qcv);
    pthread_join(g_tid, NULL);                   /* 排空剩余工作后退出 */
    fprintf(stderr, "[lmthr] worker joined (flushed=%d, pending done events left for pump)\n", pend);
}

#endif /* NGD_LM_THREAD */
