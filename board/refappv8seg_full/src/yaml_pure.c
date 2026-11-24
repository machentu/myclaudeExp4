/* ngd_app/yaml_pure.c — minimal YAML parser for ORB-SLAM3 settings files.
 * Replaces cv::FileStorage. Only handles flat "key: value" pairs.
 * Supports # and % comment lines, skips blank lines.
 * Key names match Settings.cc exactly (Camera1.fx, ORBextractor.nFeatures, etc.). */
#include "ngd_app/yaml.h"
#include "ngd/calib.h"
#include "ngd/orb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

// static void trim(char *s)
// {
//     /* leading */
//     while (isspace((unsigned char)*s)) ++s;
//     if (!*s) return;
//     /* trailing */
//     char *e = s + strlen(s) - 1;
//     while (e > s && isspace((unsigned char)*e)) --e;
//     e[1] = '\0';
//     /* move trimmed string to beginning */
//     //if (s != s) { /* no-op, just keeping the trimmed pointer */
//     //}
//     if(isnan(s))
//     {
//         printf("nan occured\n");
//     }
// }

int ngd_app_load_yaml(const char *path, ngd_app_config *cfg)
{
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    memset(cfg, 0, sizeof(*cfg));

    char line[512];
    while (fgets(line, (int)sizeof(line), f)) {
        /* skip blanks and comments */
        char *p = line;
        while (isspace((unsigned char)*p)) ++p;
        if (!*p || *p == '#' || *p == '%') continue;

        /* find ':' separator */
        char *colon = strchr(p, ':');
        if (!colon) continue;

        *colon = '\0';
        char *key = p;
        char *val = colon + 1;
        /* trim key */
        while (isspace((unsigned char)*key)) ++key;
        { char *e = key + strlen(key) - 1; while (e > key && isspace((unsigned char)*e)) --e; e[1] = '\0'; }
        /* trim value */
        while (isspace((unsigned char)*val)) ++val;
        { char *e = val + strlen(val) - 1; while (e > val && isspace((unsigned char)*e)) --e; e[1] = '\0'; }

        if (!*key || !*val) continue;

        /* match keys */
        #define F(k, fld) if (!strcmp(key, k)) { cfg->fld = (float)atof(val); continue; }
        #define I(k, fld) if (!strcmp(key, k)) { cfg->fld = atoi(val); continue; }

        F("Camera1.fx", fx)
        F("Camera1.fy", fy)
        F("Camera1.cx", cx)
        F("Camera1.cy", cy)
        I("Camera.width", imgW)
        I("Camera.height", imgH)
        F("Camera.fps", fps)
        F("Stereo.b", baseline)
        F("Stereo.ThDepth", thDepthRaw)
        F("RGBD.DepthMapFactor", depthMapFactor)
        I("ORBextractor.nFeatures", nFeatures)
        F("ORBextractor.scaleFactor", scaleFactor)
        I("ORBextractor.nLevels", nLevels)
        I("ORBextractor.iniThFAST", iniThFAST)
        I("ORBextractor.minThFAST", minThFAST)
        F("Viewer.ViewpointX", viewpointX)
        F("Viewer.ViewpointY", viewpointY)
        F("Viewer.ViewpointZ", viewpointZ)
        F("Viewer.ViewpointF", viewpointF)

        #undef F
        #undef I
    }
    fclose(f);
    return (cfg->fx != 0.0f) ? 1 : 0;
}

void ngd_app_build(const ngd_app_config *cfg, ngd_orb_extractor *ex, ngd_cam_ctx *cam)
{
    ngd_orb_init(ex, cfg->nFeatures, cfg->scaleFactor, cfg->nLevels, cfg->iniThFAST, cfg->minThFAST);
    const float thDepth_m = cfg->thDepthRaw * cfg->baseline;
    ngd_cam_ctx_init(cam, ex, cfg->fx, cfg->fy, cfg->cx, cfg->cy,
                     cfg->baseline, thDepth_m, cfg->imgW, cfg->imgH);
}
