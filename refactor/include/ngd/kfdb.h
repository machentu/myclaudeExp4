#ifndef NGD_KFDB_H
#define NGD_KFDB_H

/*
 * ngd/kfdb.h — KeyFrameDatabase inverted index (pure C).
 *
 * Faithful port of ORB_SLAM3::KeyFrameDatabase (include/KeyFrameDatabase.h,
 * src/KeyFrameDatabase.cc) for the relocalization path:
 *   - mvInvertedFile: per-WordId list of KeyFrames that observe that word
 *   - add / erase (KeyFrameDatabase.cc:39-66)
 *   - DetectRelocalizationCandidates (KeyFrameDatabase.cc:733-845)
 *
 * Simplifications (single-map, non-serialized):
 *   - GetMap() filter skipped (single map)
 *   - backup/serialize (mvBackupInvertedFileId / PreSave / PostLoad) not modelled
 *   - loop-candidate detection (DetectLoopCandidates etc.) not modelled
 *
 * A KeyFrame must have its BoW computed (ngd_keyframe_compute_bow) before add().
 */

#include "ngd/bow.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ngd_keyframe;
struct ngd_frame;

typedef struct ngd_kfdb {
    const ngd_bow_vocab *vocab;     /* mpVoc (caller-owned) */
    int nWords;                     /* vocab->n_words */

    /* mvInvertedFile: per-word growable array of KeyFrame pointers. */
    struct ngd_keyframe ***invFile;
    int *invN;
    int *invCap;
} ngd_kfdb;

/* Init to an empty database for the given vocabulary. */
void ngd_kfdb_init(ngd_kfdb *db, const ngd_bow_vocab *vocab);
void ngd_kfdb_free(ngd_kfdb *db);

/* add (KeyFrameDatabase.cc:39-45): for each word in kf->bow, append kf to that
 * word's inverted list. Requires kf->bow to be computed. */
void ngd_kfdb_add(ngd_kfdb *db, struct ngd_keyframe *kf);

/* erase (KeyFrameDatabase.cc:47-66): remove kf from every word list it appears
 * in. Safe to call on a kf that was never added. */
void ngd_kfdb_erase(ngd_kfdb *db, struct ngd_keyframe *kf);

/* DetectRelocalizationCandidates (KeyFrameDatabase.cc:733-845). Fills *out with
 * a heap array of candidate KeyFrames (caller must free(*out)) and *nOut with
 * its length. Returns 0 on success (including the empty case), non-zero on
 * allocation failure. The caller owns *out. F must have its BoW computed.
 *
 * Algorithm: per-word inverted index lookup -> shared-word KFs (deduped via
 * mnRelocQuery) -> maxCommonWords, minCommonWords = 0.8*max -> L1 score vs
 * F->bow -> covisibility-accumulated score (best 10 neighbours) ->
 * minScoreToRetain = 0.75*bestAccScore -> retain. */
int ngd_kfdb_detect_reloc_candidates(ngd_kfdb *db, struct ngd_frame *F,
                                     struct ngd_keyframe ***out, int *nOut);

#ifdef __cplusplus
}
#endif
#endif /* NGD_KFDB_H */
