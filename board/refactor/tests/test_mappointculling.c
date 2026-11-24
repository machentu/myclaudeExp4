/* test_mappointculling.c — ngd_local_mapping_map_point_culling unit test.
 *
 * Hand-builds an ngd_local_mapping with a currentKF (mnId=5) and a recent-MP
 * list covering every culling branch (LocalMapping.cc:354-393), then checks the
 * decisions. cnThObs=3 for RGBD.
 *
 *   mp_bad      : already bad             -> drop (not deleted again)
 *   mp_lowfound : found ratio < 0.25      -> set_bad + drop
 *   mp_young_lowobs: age>=2 & obs<=3      -> set_bad + drop
 *   mp_young_ok : age<2 (age=1)           -> keep
 *   mp_mature   : age>=3                  -> drop, MP stays alive (not bad)
 *   mp_healthy  : age>=2 & obs>3          -> keep
 *   mp_age2_obs4: age>=2 & obs=4(>3)      -> keep
 */
#include "ngd/localmapping.h"
#include "ngd/map.h"
#include "ngd/keyframe.h"
#include "ngd/mappoint.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { if(!(cond)){ printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); fails++; } } while(0)

static ngd_keyframe *make_kf(uint64_t id)
{
    ngd_keyframe *kf = (ngd_keyframe*)calloc(1, sizeof(ngd_keyframe));
    kf->mnId = id;
    return kf;
}

/* build a MP with given firstKFid, visible/found (->ratio), and nObs.
 * ngd_mappoint_new needs a refKF for mnFirstKFid, but we override it after. */
static ngd_mappoint *make_mp(uint64_t firstKFid, int visible, int found, int nObs)
{
    float pos[3] = {0,0,0};
    ngd_mappoint *mp = ngd_mappoint_new(pos, NULL);
    mp->mnFirstKFid = firstKFid;
    mp->mnVisible = visible;
    mp->mnFound = found;
    mp->nObs = nObs;       /* ngd_mappoint_observations reads nObs */
    return mp;
}

int main(void)
{
    ngd_map map; ngd_map_init(&map);
    ngd_local_mapping lm; ngd_local_mapping_init(&lm, &map, /*mbMonocular=*/0);

    ngd_keyframe *cur = make_kf(5);
    lm.currentKF = cur;

    ngd_mappoint *mp_bad          = make_mp(4, 10, 5, 2);  mp_bad->mbBad = 1;
    ngd_mappoint *mp_lowfound     = make_mp(4, 10, 1, 4);  /* ratio 0.1 < 0.25 */
    ngd_mappoint *mp_young_lowobs = make_mp(3, 10, 5, 2);  /* age 2, obs 2 <= 3 */
    ngd_mappoint *mp_young_ok     = make_mp(4, 10, 5, 2);  /* age 1, keep */
    ngd_mappoint *mp_mature       = make_mp(1, 10, 8, 5);  /* age 4 >= 3, drop alive */
    ngd_mappoint *mp_healthy      = make_mp(3, 10, 8, 5);  /* age 2, obs 5 > 3, keep */
    ngd_mappoint *mp_age2_obs4    = make_mp(3, 10, 8, 4);  /* age 2, obs 4 > 3, keep */

    ngd_local_mapping_push_recent(&lm, mp_bad);
    ngd_local_mapping_push_recent(&lm, mp_lowfound);
    ngd_local_mapping_push_recent(&lm, mp_young_lowobs);
    ngd_local_mapping_push_recent(&lm, mp_young_ok);
    ngd_local_mapping_push_recent(&lm, mp_mature);
    ngd_local_mapping_push_recent(&lm, mp_healthy);
    ngd_local_mapping_push_recent(&lm, mp_age2_obs4);
    CHECK(lm.nRecent == 7, "7 recent MPs pushed");

    ngd_local_mapping_map_point_culling(&lm);
    printf("  recent after culling: %d (expect 3)\n", lm.nRecent);
    CHECK(lm.nRecent == 3, "3 retained (young_ok, healthy, age2_obs4)");

    /* culled MPs are bad */
    CHECK(mp_lowfound->mbBad, "lowfound set bad");
    CHECK(mp_young_lowobs->mbBad, "young_lowobs set bad");
    /* mature dropped but alive */
    CHECK(!mp_mature->mbBad, "mature NOT bad (just left list)");
    /* kept MPs alive */
    CHECK(!mp_young_ok->mbBad, "young_ok kept alive");
    CHECK(!mp_healthy->mbBad, "healthy kept alive");
    CHECK(!mp_age2_obs4->mbBad, "age2_obs4 kept alive");

    /* the 3 survivors are exactly the keepers */
    int has_young_ok=0, has_healthy=0, has_age2=0;
    for (int i = 0; i < lm.nRecent; ++i) {
        if (lm.recent_mps[i] == mp_young_ok) has_young_ok = 1;
        if (lm.recent_mps[i] == mp_healthy)  has_healthy = 1;
        if (lm.recent_mps[i] == mp_age2_obs4) has_age2 = 1;
    }
    CHECK(has_young_ok && has_healthy && has_age2, "survivors are the 3 keepers");

    /* cleanup: free all MPs (set_bad freed obs already for culled ones) */
    ngd_mappoint_free(mp_bad);
    ngd_mappoint_free(mp_lowfound);
    ngd_mappoint_free(mp_young_lowobs);
    ngd_mappoint_free(mp_young_ok);
    ngd_mappoint_free(mp_mature);
    ngd_mappoint_free(mp_healthy);
    ngd_mappoint_free(mp_age2_obs4);
    free(cur);
    ngd_local_mapping_free(&lm);
    ngd_map_free(&map);

    if (fails == 0) { printf("test_mappointculling: PASS\n"); return 0; }
    printf("test_mappointculling: %d FAILURES\n", fails);
    return 1;
}
