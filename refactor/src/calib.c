/* ngd/calib.c — camera context (pure C). */
#include "ngd/calib.h"
#include <string.h>
#include <math.h>

void ngd_cam_ctx_init(ngd_cam_ctx *c, const ngd_orb_extractor *ex,
                      float fx, float fy, float cx, float cy,
                      float baseline, float thDepth, int imgW, int imgH)
{
    c->cam = ngd_pinhole_make(fx, fy, cx, cy);
    c->mbf = fx * baseline;
    c->mb  = baseline;
    c->thDepth = thDepth;
    c->imgW = imgW;
    c->imgH = imgH;
    c->nlevels = ex ? ex->nlevels : 0;
    for (int i = 0; i < c->nlevels; ++i) {
        c->mvScaleFactor[i]     = ex->mvScaleFactor[i];
        c->mvInvScaleFactor[i]  = ex->mvInvScaleFactor[i];
        c->mvLevelSigma2[i]     = ex->mvLevelSigma2[i];
        c->mvInvLevelSigma2[i]  = ex->mvInvLevelSigma2[i];
    }
    /* Frame::mfLogScaleFactor = log(mfScaleFactor). mvScaleFactor[0]=1 -> log(1)=0, so the
     * original uses level-1's factor (= log(scaleFactor)); MapPoint::PredictScale divides by it.
     * Guard nlevels<2 (degenerate extractor) by falling back to 0 (predict_scale returns 0). */
    c->mfLogScaleFactor = (c->nlevels >= 2) ? logf(c->mvScaleFactor[1]) : 0.0f;
}
