#include "ngd/orb.h"
#include "ngd/calib.h"
#include "ngd/se3.h"
#include "ngd/pinhole.h"
#include "ngd/keyframe.h"
#include "ngd/matcher.h"
#include "ngd/bow.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#define W 640
#define H 480
#define NPTS 40
static unsigned int rng=42u;
static float frand(void){rng=rng*1664525u+1013904223u;return (rng>>8)*(1.0f/16777216.0f);}
static ngd_bow_vocab* lv(void){
 const char*c[]={"../../../Vocabulary/ORBvoc.bin","../../Vocabulary/ORBvoc.bin","../Vocabulary/ORBvoc.bin","Vocabulary/ORBvoc.bin",0};
 for(int i=0;c[i];++i){FILE*f=fopen(c[i],"rb");if(f){fclose(f);return ngd_bow_vocab_load_binary(c[i]);}}
 return 0;
}
static int proj(ngd_cam_ctx*cam,ngd_se3 T,const float X[3],float*ux,float*uy,float*ur,float*z){
 ngd_vec3 Xc=ngd_se3_map(T,ngd_v3(X[0],X[1],X[2]));if(Xc.z<=0.1f)return 0;
 float u=cam->cam.fx*Xc.x/Xc.z+cam->cam.cx,v=cam->cam.fy*Xc.y/Xc.z+cam->cam.cy;
 if(u<4||u>W-4||v<4||v>H-4)return 0;*ux=u;*uy=v;*ur=u-cam->mbf/Xc.z;*z=Xc.z;return 1;
}
static ngd_keyframe* bkf(uint64_t id,ngd_se3 pose,ngd_cam_ctx*cam,const float pts[][3],int n){
 ngd_keypoint*kps=malloc(n*sizeof* kps);uint8_t*desc=calloc(n*32,1);float*ur=malloc(n*sizeof*ur);float*dep=malloc(n*sizeof*dep);
 int k=0;for(int j=0;j<n;++j){float ux,uy,r,z;if(!proj(cam,pose,pts[j],&ux,&uy,&r,&z))continue;
 kps[k].x=ux;kps[k].y=uy;kps[k].octave=0;kps[k].angle=0;kps[k].response=1;kps[k].size=31;ur[k]=r;dep[k]=z;
 memset(desc+k*32,0,32);desc[k*32]=(uint8_t)j;k++;}
 ngd_keyframe*kf=ngd_keyframe_new(id,pose,cam,k,kps,desc,ur,dep);free(kps);free(desc);free(ur);free(dep);return kf;
}
int main(void){
 ngd_bow_vocab*voc=lv();if(!voc){printf("no voc\n");return 1;}
 ngd_orb_extractor ex;ngd_orb_init(&ex,1000,1.2f,8,20,7);
 ngd_cam_ctx cam;ngd_cam_ctx_init(&cam,&ex,535.4f,539.2f,320.1f,247.6f,0.0747f,2.988f,W,H);
 static float pts[NPTS][3];for(int j=0;j<NPTS;++j){pts[j][0]=(frand()-0.5f)*1.4f;pts[j][1]=(frand()-0.5f);pts[j][2]=1.8f+frand()*1.5f;}
 ngd_se3 P0=ngd_se3_identity(),P1=ngd_se3_from_rt(ngd_quat_to_matrix(ngd_quat_identity()),ngd_v3(0.15f,0,0));
 ngd_keyframe*kf0=bkf(0,P0,&cam,pts,NPTS),*kf1=bkf(1,P1,&cam,pts,NPTS);
 ngd_keyframe_compute_bow(kf0,voc);ngd_keyframe_compute_bow(kf1,voc);
 printf("kf0.feat.n=%d kf1.feat.n=%d\n",kf0->feat->n,kf1->feat->n);
 int i1[2048],i2[2048];
 int m0=ngd_search_for_triangulation(kf1,kf0,i1,i2,2048,0,0);
 printf("bCoarse=0 nmatches=%d\n",m0);
 int m1=ngd_search_for_triangulation(kf1,kf0,i1,i2,2048,0,1);
 printf("bCoarse=1 nmatches=%d\n",m1);
 /* show first few matches */
 for(int i=0;i<m1&&i<5;++i) printf("  match kf1.idx%d <-> kf0.idx%d\n",i1[i],i2[i]);
 return 0;
}
