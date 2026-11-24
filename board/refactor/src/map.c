/* ngd/map.c — Map container (pure C, minimal).
 * See ngd/map.h for scope. Reference: src/Map.cc. */
#include "ngd/map.h"
#include "ngd/keyframe.h"
#include "ngd/mappoint.h"

#include <stdlib.h>
#include <string.h>

void ngd_map_init(ngd_map *m)
{
    memset(m, 0, sizeof(*m));
}

void ngd_map_free(ngd_map *m)
{
    /* retired KF 对象由本层释放 (它们已不在 m->kfs, 调用方只遍历后者). */
    for (int i = 0; i < m->nRetiredKFs; ++i) ngd_keyframe_free(m->retired_kfs[i]);
    free(m->retired_kfs);
    m->retired_kfs = NULL;
    m->nRetiredKFs = m->capRetiredKFs = 0;

    free(m->kfs); free(m->mps);
    m->kfs = NULL; m->mps = NULL;
    m->nKFs = m->capKFs = 0;
    m->nMPs = m->capMPs = 0;
}

int ngd_map_add_keyframe(ngd_map *m, ngd_keyframe *kf)
{
    for (int i = 0; i < m->nKFs; ++i) if (m->kfs[i] == kf) return 0;   /* dedup */
    if (m->nKFs == m->capKFs) {
        int nc = m->capKFs ? m->capKFs * 2 : 16;
        m->kfs = (ngd_keyframe**)realloc(m->kfs, (size_t)nc * sizeof(ngd_keyframe*));
        m->capKFs = nc;
    }
    m->kfs[m->nKFs++] = kf;
    if (m->nKFs == 1) m->mnInitKFid = kf->mnId;     /* Map.cc: first KF */
    if (kf->mnId > m->mnMaxKFid) m->mnMaxKFid = kf->mnId;
    return 1;
}

int ngd_map_add_mappoint(ngd_map *m, ngd_mappoint *mp)
{
    for (int i = 0; i < m->nMPs; ++i) if (m->mps[i] == mp) return 0;   /* dedup */
    if (m->nMPs == m->capMPs) {
        int nc = m->capMPs ? m->capMPs * 2 : 64;
        m->mps = (ngd_mappoint**)realloc(m->mps, (size_t)nc * sizeof(ngd_mappoint*));
        m->capMPs = nc;
    }
    m->mps[m->nMPs++] = mp;
    return 1;
}

void ngd_map_erase_mappoint(ngd_map *m, ngd_mappoint *mp)
{
    for (int i = 0; i < m->nMPs; ++i) {
        if (m->mps[i] == mp) {
            m->mps[i] = m->mps[m->nMPs - 1];   /* swap-pop (set, order irrelevant) */
            m->nMPs--;
            return;
        }
    }
}

void ngd_map_erase_keyframe(ngd_map *m, ngd_keyframe *kf)
{
    for (int i = 0; i < m->nKFs; ++i) {
        if (m->kfs[i] == kf) {
            m->kfs[i] = m->kfs[m->nKFs - 1];
            m->nKFs--;
            /* 摘除即转 retired: 剔除时已解缠, 但跨帧持有者仍在读,
             * 对象活到 ngd_map_free (上游 shared_ptr 续命语义). */
            if (m->nRetiredKFs == m->capRetiredKFs) {
                int nc = m->capRetiredKFs ? m->capRetiredKFs * 2 : 16;
                m->retired_kfs = (ngd_keyframe**)realloc(m->retired_kfs,
                    (size_t)nc * sizeof(ngd_keyframe*));
                m->capRetiredKFs = nc;
            }
            m->retired_kfs[m->nRetiredKFs++] = kf;
            return;
        }
    }
}
