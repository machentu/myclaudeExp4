/* test_map.c — ngd_map container unit test. No vocabulary / no images. */
#include "ngd/map.h"
#include "ngd/keyframe.h"
#include "ngd/mappoint.h"
#include "ngd/calib.h"
#include "ngd/orb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { if(!(cond)){ printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); fails++; } } while(0)

/* minimal KF with just mnId set (ngd_map only reads mnId). */
static ngd_keyframe *make_min_kf(uint64_t id)
{
    ngd_keyframe *kf = (ngd_keyframe*)calloc(1, sizeof(ngd_keyframe));
    kf->mnId = id;
    return kf;
}
static ngd_mappoint *make_min_mp(void)
{
    ngd_mappoint *mp = (ngd_mappoint*)calloc(1, sizeof(ngd_mappoint));
    return mp;
}

int main(void)
{
    ngd_map m; ngd_map_init(&m);

    /* empty */
    CHECK(m.nKFs == 0 && m.nMPs == 0, "empty map");

    /* add 3 KFs */
    ngd_keyframe *kf0 = make_min_kf(0), *kf1 = make_min_kf(5), *kf2 = make_min_kf(9);
    CHECK(ngd_map_add_keyframe(&m, kf0) == 1, "add kf0");
    CHECK(m.nKFs == 1, "nKFs=1");
    CHECK(m.mnInitKFid == 0, "mnInitKFid = first KF id");
    CHECK(m.mnMaxKFid == 0, "mnMaxKFid = 0");
    CHECK(ngd_map_add_keyframe(&m, kf1) == 1, "add kf1");
    CHECK(m.mnMaxKFid == 5, "mnMaxKFid = 5");
    CHECK(ngd_map_add_keyframe(&m, kf2) == 1, "add kf2");
    CHECK(m.nKFs == 3, "nKFs=3");
    CHECK(m.mnMaxKFid == 9, "mnMaxKFid = 9");
    CHECK(m.mnInitKFid == 0, "mnInitKFid unchanged");

    /* dedup */
    CHECK(ngd_map_add_keyframe(&m, kf1) == 0, "re-add kf1 dedup");
    CHECK(m.nKFs == 3, "nKFs still 3");

    /* add MPs */
    ngd_mappoint *mp0 = make_min_mp(), *mp1 = make_min_mp();
    CHECK(ngd_map_add_mappoint(&m, mp0) == 1, "add mp0");
    CHECK(ngd_map_add_mappoint(&m, mp1) == 1, "add mp1");
    CHECK(m.nMPs == 2, "nMPs=2");
    CHECK(ngd_map_add_mappoint(&m, mp0) == 0, "re-add mp0 dedup");

    /* erase */
    ngd_map_erase_mappoint(&m, mp0);
    CHECK(m.nMPs == 1, "erase mp0 -> nMPs=1");
    CHECK(m.mps[0] == mp1, "remaining is mp1");
    ngd_map_erase_keyframe(&m, kf1);
    CHECK(m.nKFs == 2, "erase kf1 -> nKFs=2");
    /* erase non-existent is a no-op */
    ngd_map_erase_keyframe(&m, kf1);
    CHECK(m.nKFs == 2, "erase absent kf no-op");

    ngd_map_free(&m);
    free(kf0); free(kf1); free(kf2); free(mp0); free(mp1);

    if (fails == 0) { printf("test_map: PASS\n"); return 0; }
    printf("test_map: %d FAILURES\n", fails);
    return 1;
}
