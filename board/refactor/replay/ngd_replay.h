/*
 * ngd_replay.h - golden-text reader for dump-and-diff replay tests.
 *
 * Parses the text records written by include/ngd_dump.h (original C++) into
 * C structs, so refactor replay tests can feed the recorded inputs to the
 * pure-C modules and compare outputs.
 *
 * Format (one record per line, written by ngd_dump_line("poseopt", ...)):
 *   BEGIN v1 ts=<double> fid=<uint> qw=.. qx=.. qy=.. qz=.. tx=.. ty=.. tz=..
 *   EDGE  v1 ts=<double> i=<int>  Xw=..,..,.. u=.. v=.. uR=.. st=<0|1> oct=<int>
 *   END   v1 ts=<double> fid=<uint> qw=.. ... ninl=<int> nbad=<int>
 * A PoseOpt call = one BEGIN + N EDGE + one END (same ts/fid), in order.
 */
#ifndef NGD_REPLAY_H
#define NGD_REPLAY_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    double Xw[3], u, v, uR;
    int    st;   /* stereo flag */
    int    oct;  /* octave */
} ngd_golden_edge;

typedef struct {
    double ts;
    unsigned long long fid;
    double sq[4];   /* seed quat w,x,y,z */
    double st_[3];  /* seed translation */
    int    n_edges;
    ngd_golden_edge *edges;
    double oq[4];   /* output quat w,x,y,z */
    double ot_[3];  /* output translation */
    int    ninl, nbad;
} ngd_golden_call;

/* Load all PoseOpt calls from a golden file. Returns count; *out_calls malloc'd. */
static int ngd_replay_load_poseopt(const char* path, ngd_golden_call** out_calls) {
    FILE* f = fopen(path, "r");
    if (!f) { fprintf(stderr, "replay: cannot open %s\n", path); return -1; }

    int cap = 64, n = 0;
    ngd_golden_call* calls = (ngd_golden_call*)malloc(cap * sizeof(ngd_golden_call));
    ngd_golden_call* cur = NULL;
    int ecap = 0;

    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "BEGIN", 5) == 0) {
            if (n >= cap) { cap *= 2; calls = (ngd_golden_call*)realloc(calls, cap*sizeof(ngd_golden_call)); }
            cur = &calls[n];
            memset(cur, 0, sizeof(*cur));
            cur->n_edges = 0; ecap = 32;
            cur->edges = (ngd_golden_edge*)malloc(ecap * sizeof(ngd_golden_edge));
            sscanf(line, "BEGIN v1 ts=%lf fid=%llu qw=%lf qx=%lf qy=%lf qz=%lf tx=%lf ty=%lf tz=%lf",
                &cur->ts, &cur->fid, &cur->sq[0],&cur->sq[1],&cur->sq[2],&cur->sq[3],
                &cur->st_[0],&cur->st_[1],&cur->st_[2]);
        } else if (strncmp(line, "EDGE", 4) == 0 && cur) {
            if (cur->n_edges >= ecap) { ecap *= 2; cur->edges = (ngd_golden_edge*)realloc(cur->edges, ecap*sizeof(ngd_golden_edge)); }
            ngd_golden_edge* e = &cur->edges[cur->n_edges++];
            /* Xw=..,..,..  (comma-separated) */
            double ts; int idx;
            sscanf(line, "EDGE ts=%lf i=%d Xw=%lf,%lf,%lf u=%lf v=%lf uR=%lf st=%d oct=%d",
                &ts, &idx, &e->Xw[0],&e->Xw[1],&e->Xw[2], &e->u,&e->v,&e->uR, &e->st, &e->oct);
        } else if (strncmp(line, "END", 3) == 0 && cur) {
            sscanf(line, "END v1 ts=%lf fid=%llu qw=%lf qx=%lf qy=%lf qz=%lf tx=%lf ty=%lf tz=%lf ninl=%d nbad=%d",
                &cur->ts, &cur->fid, &cur->oq[0],&cur->oq[1],&cur->oq[2],&cur->oq[3],
                &cur->ot_[0],&cur->ot_[1],&cur->ot_[2], &cur->ninl, &cur->nbad);
            n++;
            cur = NULL;
        }
    }
    fclose(f);
    *out_calls = calls;
    return n;
}

#endif /* NGD_REPLAY_H */
