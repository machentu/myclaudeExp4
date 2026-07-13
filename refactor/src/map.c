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
            return;
        }
    }
}
