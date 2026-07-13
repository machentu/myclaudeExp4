// ngd_app/viewer.cpp — threaded live OpenCV 3D map viewer (no Pangolin/VTK).
//
// Multi-threaded variant: Tracking builds ngd_app_viewer_snapshot (under
// map_mutex); the Viewer thread calls ngd_app_viewer_draw with that snapshot +
// a live map read (under map_mutex). See viewer.h for the rationale.
//
// 3D->2D projection via cv::projectPoints (calib3d). The camera-pose drawing
// faithfully mirrors Pangolin's MapDrawer (src/MapDrawer.cc):
//   - DrawMapPoints        -> point cloud (coloured by height here)
//   - DrawKeyFrames(graph) -> every keyframe drawn as a wireframe camera
//                             frustum (blue; init KF red) + covisibility graph
//                             (green lines, weight>=100)
//   - DrawCurrentCamera    -> current camera as a green wireframe frustum
// Frustum geometry is identical to MapDrawer: w=size, h=w*0.75, z=w*0.6,
// 8 lines (apex->4 corners + image rectangle), built in camera frame and
// transformed by Twc. Sizes from TUM3.yaml: KeyFrameSize=0.05, CameraSize=0.08.
#include "ngd_app/viewer.h"

#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/calib3d.hpp>

#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <vector>

/* ---------------- ngd_vec3 helpers (math.h has no operators) ---------------- */
static ngd_vec3 v3add(ngd_vec3 a, ngd_vec3 b){ ngd_vec3 r={a.x+b.x,a.y+b.y,a.z+b.z}; return r; }
static ngd_vec3 v3sub(ngd_vec3 a, ngd_vec3 b){ ngd_vec3 r={a.x-b.x,a.y-b.y,a.z-b.z}; return r; }
static ngd_vec3 v3scale(ngd_vec3 a, float s){ ngd_vec3 r={a.x*s,a.y*s,a.z*s}; return r; }
static float   v3dot(ngd_vec3 a, ngd_vec3 b){ return a.x*b.x+a.y*b.y+a.z*b.z; }
static ngd_vec3 v3cross(ngd_vec3 a, ngd_vec3 b){
    ngd_vec3 r={a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x}; return r;
}
static ngd_vec3 v3normed(ngd_vec3 a){
    float n=std::sqrt(v3dot(a,a));
    return (n>1e-12f)?v3scale(a,1.0f/n):a;
}

/* HSV (h in [0,360], s/v in [0,1]) -> OpenCV BGR Scalar. */
static cv::Scalar hsv2bgr(float h, float s, float v){
    float c=v*s, hp=h/60.0f;
    float x=c*(1.0f-std::fabs(std::fmod(hp,2.0f)-1.0f));
    float r=0,g=0,b=0,m=v-c;
    if(hp<1){r=c;g=x;} else if(hp<2){r=x;g=c;} else if(hp<3){g=c;b=x;}
    else if(hp<4){g=x;b=c;} else if(hp<5){r=x;b=c;} else {r=c;b=x;}
    return cv::Scalar((b+m)*255.0f,(g+m)*255.0f,(r+m)*255.0f);
}
static cv::Scalar height_color(float t){
    if(t<0)t=0; if(t>1)t=1;
    return hsv2bgr((1.0f-t)*240.0f, 1.0f, 1.0f);
}
static cv::Point to_pt(cv::Point2f p){ return cv::Point((int)(p.x+0.5f),(int)(p.y+0.5f)); }

/* ---------------- viewer state ---------------- */
void ngd_app_viewer_init(ngd_app_viewer *v, int enabled,
                         float vpX, float vpY, float vpZ, float vpF)
{
    memset(v, 0, sizeof(*v));
    v->enabled = enabled;
    v->W3d = 900; v->H3d = 700;
    v->eye[0]=vpX; v->eye[1]=vpY; v->eye[2]=vpZ;
    v->eye_home[0]=vpX; v->eye_home[1]=vpY; v->eye_home[2]=vpZ;
    v->target[0]=0.0f; v->target[1]=0.0f; v->target[2]=0.0f;
    v->up[0]=0.0f;    v->up[1]=-1.0f;   v->up[2]=0.0f;
    v->focal = (vpF>1.0f)?vpF:500.0f;
    v->kf_size   = 0.05f;
    v->cam_size  = 0.08f;
    v->cap_traj  = 4096;
    v->traj = (ngd_vec3 *)calloc((size_t)v->cap_traj, sizeof(ngd_vec3));
    v->n_traj = 0;
}

void ngd_app_viewer_free(ngd_app_viewer *v)
{
    free(v->traj);
    v->traj=NULL; v->n_traj=v->cap_traj=0;
}

static void traj_push(ngd_app_viewer *v, ngd_vec3 p)
{
    if (v->n_traj == v->cap_traj) {
        int nc = v->cap_traj*2;
        v->traj = (ngd_vec3 *)realloc(v->traj, (size_t)nc*sizeof(ngd_vec3));
        v->cap_traj = nc;
    }
    v->traj[v->n_traj++] = p;
}

static void view_setup(const ngd_app_viewer *v, cv::Mat &rvec, cv::Mat &tvec, cv::Mat &K)
{
    ngd_vec3 eye   = ngd_vec3{v->eye[0],   v->eye[1],   v->eye[2]};
    ngd_vec3 target= ngd_vec3{v->target[0],v->target[1],v->target[2]};
    ngd_vec3 up    = ngd_vec3{v->up[0],    v->up[1],    v->up[2]};
    ngd_vec3 f = v3normed(v3sub(target, eye));
    ngd_vec3 r = v3normed(v3cross(f, up));
    ngd_vec3 u = v3cross(r, f);
    cv::Mat R = (cv::Mat_<float>(3,3) <<
        r.x, r.y, r.z,
       -u.x,-u.y,-u.z,
        f.x, f.y, f.z);
    tvec = (cv::Mat_<float>(3,1) <<
        -(r.x*eye.x+r.y*eye.y+r.z*eye.z),
         (u.x*eye.x+u.y*eye.y+u.z*eye.z),
        -(f.x*eye.x+f.y*eye.y+f.z*eye.z));
    cv::Rodrigues(R, rvec);
    float F=v->focal;
    K = (cv::Mat_<float>(3,3) <<
        F,0,v->W3d*0.5f,
        0,F,v->H3d*0.5f,
        0,0,1);
}

static std::vector<cv::Point2f> project_with(const cv::Mat &rvec, const cv::Mat &tvec, const cv::Mat &K,
                                             const ngd_vec3 *pts, int n)
{
    std::vector<cv::Point2f> img;
    if (n<=0 || !pts) return img;
    cv::Mat obj(n, 3, CV_32F);
    for (int i=0;i<n;++i){ obj.at<float>(i,0)=pts[i].x; obj.at<float>(i,1)=pts[i].y; obj.at<float>(i,2)=pts[i].z; }
    cv::projectPoints(obj, rvec, tvec, K, cv::Mat(), img);
    return img;
}

static void draw_frustum(cv::Mat &canvas, const cv::Mat &rvec, const cv::Mat &tvec, const cv::Mat &K,
                         ngd_se3 Twc, float w, const cv::Scalar &color, int thick)
{
    const float h=w*0.75f, z=w*0.6f;
    ngd_vec3 cam[5]={ {0,0,0},{ w, h,z},{ w,-h,z},{-w,-h,z},{-w, h,z} };
    ngd_vec3 wld[5];
    for (int i=0;i<5;++i) wld[i]=ngd_se3_map(Twc, cam[i]);
    std::vector<cv::Point2f> p=project_with(rvec, tvec, K, wld, 5);
    if (p.size()!=5) return;
    static const int segs[8][2]={{0,1},{0,2},{0,3},{0,4},{1,2},{3,4},{4,1},{3,2}};
    for (int i=0;i<8;++i)
        cv::line(canvas, to_pt(p[segs[i][0]]), to_pt(p[segs[i][1]]), color, thick, cv::LINE_AA);
}

static ngd_vec3 v3rot(ngd_vec3 v, ngd_vec3 k, float th)
{
    float c=std::cos(th), s=std::sin(th);
    ngd_vec3 kxv=v3cross(k,v);
    ngd_vec3 a={v.x*c + kxv.x*s + k.x*v3dot(k,v)*(1.0f-c),
                v.y*c + kxv.y*s + k.y*v3dot(k,v)*(1.0f-c),
                v.z*c + kxv.z*s + k.z*v3dot(k,v)*(1.0f-c)};
    return a;
}

static void on_mouse(int event, int x, int y, int flags, void *userdata)
{
    ngd_app_viewer *v = (ngd_app_viewer *)userdata;
    if (!v || !v->enabled) return;
    if (event==cv::EVENT_LBUTTONDOWN){ v->dragging=1; v->drag_last_x=x; v->drag_last_y=y; }
    else if (event==cv::EVENT_LBUTTONUP){ v->dragging=0; }
    else if (event==cv::EVENT_MOUSEMOVE && v->dragging){
        ngd_vec3 eye{v->eye[0],v->eye[1],v->eye[2]};
        ngd_vec3 tgt{v->target[0],v->target[1],v->target[2]};
        ngd_vec3 up {v->up[0],   v->up[1],   v->up[2]};
        ngd_vec3 fwd=v3sub(tgt,eye);
        float fl=std::sqrt(v3dot(fwd,fwd)); if(fl<1e-6f) return;
        float dist=fl;
        fwd=v3scale(fwd,1.0f/fl);
        ngd_vec3 r=v3normed(v3cross(fwd,up));
        float da=(float)(x - v->drag_last_x)*0.01f;
        float de=(float)(v->drag_last_y - y)*0.01f;
        ngd_vec3 u=v3cross(r,fwd);
        ngd_vec3 nf=v3rot(fwd,u,-da);
        nf=v3rot(nf,r,-de);
        ngd_vec3 cr=v3cross(nf,up);
        if (std::sqrt(v3dot(cr,cr)) > 0.15f){
            ngd_vec3 ne=v3sub(tgt, v3scale(nf,dist));
            v->eye[0]=ne.x; v->eye[1]=ne.y; v->eye[2]=ne.z;
        }
        v->drag_last_x=x; v->drag_last_y=y;
    } else if (event==cv::EVENT_MOUSEWHEEL){
        int d = cv::getMouseWheelDelta(flags);
        ngd_vec3 eye{v->eye[0],v->eye[1],v->eye[2]};
        ngd_vec3 tgt{v->target[0],v->target[1],v->target[2]};
        ngd_vec3 fwd=v3sub(eye,tgt);
        float dist=std::sqrt(v3dot(fwd,fwd));
        dist *= (1.0f - d*0.0015f);
        if (dist<0.05f) dist=0.05f;
        if (v3dot(fwd,fwd)>1e-12f){
            ngd_vec3 dir=v3scale(fwd, 1.0f/std::sqrt(v3dot(fwd,fwd)));
            ngd_vec3 ne=v3add(tgt, v3scale(dir,dist));
            v->eye[0]=ne.x; v->eye[1]=ne.y; v->eye[2]=ne.z;
        }
    }
}

/* ---------------- snapshot build (Tracking, under map_mutex) ---------------- */
void ngd_app_viewer_snapshot_build(ngd_app_viewer *v, ngd_app_viewer_snapshot *snap,
                                   const ngd_frame *cur,
                                   const cv::Mat &imRGB,
                                   const uint8_t *mask, int maskW, int maskH,
                                   int state, int nInliers, uint64_t frame_num)
{
    if (!v->enabled) { snap->valid = 0; return; }
    snap->state = state;
    snap->nInliers = nInliers;
    snap->frame_num = frame_num;

    snap->imRGB = imRGB.empty() ? cv::Mat() : imRGB.clone();
    snap->maskW = maskW; snap->maskH = maskH;
    snap->mask.clear();
    if (mask && maskW>0 && maskH>0) snap->mask.assign(mask, mask + (size_t)maskW*maskH);

    if (cur) {
        snap->cur_pose = cur->pose;
        snap->cur_center = ngd_frame_get_camera_center(cur);
        snap->cur_N = cur->N;
        snap->cur_keys.assign(cur->keys, cur->keys + cur->N);
        snap->cur_outlier.assign(cur->N, 0);
        snap->cur_mp_present.assign(cur->N, 0);
        for (int i=0;i<cur->N;++i) {
            if (cur->mvbOutlier)   snap->cur_outlier[i]    = cur->mvbOutlier[i] ? 1 : 0;
            if (cur->mvpMapPoints) snap->cur_mp_present[i] = cur->mvpMapPoints[i] ? 1 : 0;
        }
    } else {
        snap->cur_N = 0;
        snap->cur_keys.clear();
        snap->cur_outlier.clear();
        snap->cur_mp_present.clear();
    }
    snap->valid = 1;
}

/* ---------------- draw (Viewer thread, under map_mutex) ---------------- */
void ngd_app_viewer_draw(ngd_app_viewer *v,
                         const ngd_app_viewer_snapshot *snap,
                         const ngd_map *map)
{
    if (!v->enabled || !snap || !snap->valid) return;

    /* current camera centre -> trajectory */
    traj_push(v, snap->cur_center);

    if (!v->win_created){
        cv::namedWindow("NGD refapp-thread: 3D Map", cv::WINDOW_NORMAL);
        cv::resizeWindow("NGD refapp-thread: 3D Map", v->W3d, v->H3d);
        cv::setMouseCallback("NGD refapp-thread: 3D Map", on_mouse, v);
        v->win_created=1;
    }

    const int W3=v->W3d, H3=v->H3d;
    cv::Mat canvas(H3, W3, CV_8UC3, cv::Scalar(8,8,12));
    cv::Mat rvec, tvec, K; view_setup(v, rvec, tvec, K);

    /* ---- point cloud (coloured by height along the up axis) ---- */
    std::vector<ngd_vec3> cloud;
    if (map) {
        cloud.reserve((size_t)map->nMPs);
        for (int i=0;i<map->nMPs;++i){
            ngd_mappoint *mp=map->mps[i];
            if(!mp||mp->mbBad) continue;
            ngd_vec3 pp={mp->worldPos[0],mp->worldPos[1],mp->worldPos[2]};
            cloud.push_back(pp);
        }
    }
    if (!cloud.empty()){
        ngd_vec3 upv{v->up[0],v->up[1],v->up[2]};
        float ylo=1e30f,yhi=-1e30f;
        for (size_t i=0;i<cloud.size();++i){
            float hc=v3dot(cloud[i],upv);
            if(hc<ylo)ylo=hc; if(hc>yhi)yhi=hc;
        }
        float yr=(yhi-ylo>1e-6f)?1.0f/(yhi-ylo):0.0f;
        std::vector<cv::Point2f> img=project_with(rvec,tvec,K, cloud.data(), (int)cloud.size());
        for (size_t i=0;i<img.size();++i){
            cv::Point2f p=img[i];
            if(p.x<0||p.y<0||p.x>=W3||p.y>=H3) continue;
            float hc=v3dot(cloud[i],upv);
            cv::circle(canvas, p, 2, height_color((hc-ylo)*yr), -1);
        }
    }

    /* ---- good keyframes: centres + Twc (for graph + frustums) ---- */
    std::vector<const ngd_keyframe *> kfs;
    std::vector<ngd_vec3> kfc;
    int nKF = map?map->nKFs:0;
    if (map && nKF>0){
        kfs.reserve((size_t)nKF); kfc.reserve((size_t)nKF);
        for (int i=0;i<nKF;++i){
            ngd_keyframe *kf=map->kfs[i];
            if(!kf||ngd_keyframe_is_bad(kf)) continue;
            kfs.push_back(kf);
            kfc.push_back( ngd_keyframe_get_camera_center(kf) );
        }
    }

    /* ---- covisibility graph: green lines, weight>=100, mnId<j dedup ---- */
    if (kfc.size()>=2){
        std::vector<cv::Point2f> img=project_with(rvec,tvec,K, kfc.data(), (int)kfc.size());
        const cv::Scalar graph_col(0,200,0);
        for (size_t i=0;i<kfs.size();++i){
            const ngd_keyframe *a=kfs[i];
            for (int j=0;j<a->nConn;++j){
                if (a->connWeights[j] < 100) continue;
                const ngd_keyframe *b=a->connKFs[j];
                if (!b || b->mnId <= a->mnId) continue;
                int bi=-1;
                for (size_t k=0;k<kfs.size();++k) if (kfs[k]==b){ bi=(int)k; break; }
                if (bi<0) continue;
                cv::line(canvas, to_pt(img[i]), to_pt(img[bi]), graph_col, 1, cv::LINE_AA);
            }
        }
    }

    /* ---- keyframe frustums: blue, init KF red ---- */
    {
        const cv::Scalar kf_col(255,0,0);
        const cv::Scalar init_col(0,0,255);
        uint64_t init_id = map?map->mnInitKFid:(uint64_t)-1;
        for (size_t i=0;i<kfs.size();++i){
            const ngd_keyframe *kf=kfs[i];
            int is_init = (kf->mnId==init_id);
            draw_frustum(canvas, rvec,tvec,K, ngd_se3_inverse(kf->pose),
                         v->kf_size, is_init?init_col:kf_col, is_init?2:1);
        }
    }

    /* ---- continuous camera trajectory ---- */
    if (v->n_traj>=2){
        std::vector<cv::Point2f> img=project_with(rvec,tvec,K, v->traj, v->n_traj);
        std::vector<cv::Point> poly; poly.reserve(img.size());
        for (size_t i=0;i<img.size();++i) poly.push_back(to_pt(img[i]));
        cv::polylines(canvas, poly, false, cv::Scalar(0,120,70), 1, cv::LINE_AA);
        cv::circle(canvas, poly.back(), 4, cv::Scalar(0,255,200), -1, cv::LINE_AA);
    }

    /* ---- current camera frustum (green) ---- */
    draw_frustum(canvas, rvec,tvec,K, ngd_se3_inverse(snap->cur_pose), v->cam_size, cv::Scalar(0,255,0), 2);

    /* ---- HUD ---- */
    char buf[192];
    std::snprintf(buf,sizeof(buf),"frame %llu  state=%d  inliers=%d  KFs=%d  MPs=%d  traj=%d",
        (unsigned long long)snap->frame_num, snap->state, snap->nInliers, (int)kfs.size(),
        map?map->nMPs:0, v->n_traj);
    cv::putText(canvas, buf, cv::Point(10,25), cv::FONT_HERSHEY_SIMPLEX, 0.5,
                cv::Scalar(225,225,225), 1, cv::LINE_AA);
    cv::putText(canvas, "drag=orbit  wheel=zoom  r=reset view",
                cv::Point(10,H3-12), cv::FONT_HERSHEY_SIMPLEX, 0.45,
                cv::Scalar(150,150,150), 1, cv::LINE_AA);
    cv::imshow("NGD refapp-thread: 3D Map", canvas);

    /* ---- 2D frame overlay (from snapshot) ---- */
    if (!snap->imRGB.empty() && snap->cur_N>0 && !snap->cur_keys.empty()){
        cv::Mat ov;
        if (snap->imRGB.channels()==3) ov=snap->imRGB.clone();
        else cv::cvtColor(snap->imRGB, ov, cv::COLOR_GRAY2BGR);

        if (!snap->mask.empty() && snap->maskW>0 && snap->maskH>0 &&
            snap->maskW<=ov.cols && snap->maskH<=ov.rows){
            cv::Mat maskMat(snap->maskH, snap->maskW, CV_8U,
                            (void *)snap->mask.data(), (size_t)snap->maskW);
            cv::Mat masknz; cv::threshold(maskMat, masknz, 0, 255, cv::THRESH_BINARY);
            cv::Mat red(ov.size(), ov.type(), cv::Scalar(0,0,180));
            cv::Mat tint; cv::addWeighted(ov, 0.65, red, 0.35, 0, tint);
            tint.copyTo(ov, masknz);
        }
        for (int i=0;i<snap->cur_N && i<(int)snap->cur_keys.size();++i){
            const ngd_keypoint &k=snap->cur_keys[i];
            cv::Point pt((int)(k.x+0.5f),(int)(k.y+0.5f));
            int mp_present = (i<(int)snap->cur_mp_present.size()) ? snap->cur_mp_present[i] : 0;
            int outlier    = (i<(int)snap->cur_outlier.size())    ? snap->cur_outlier[i]    : 0;
            cv::Scalar col; int rad;
            if (mp_present && !outlier){ col=cv::Scalar(0,255,0);  rad=3; }
            else if (mp_present && outlier){ col=cv::Scalar(0,0,255); rad=2; }
            else { col=cv::Scalar(170,170,170); rad=1; }
            cv::circle(ov, pt, rad, col, -1, cv::LINE_AA);
        }
        cv::imshow("NGD refapp-thread: Frame", ov);
    }

    int key = cv::waitKey(1) & 0xff;
    if (key=='r'||key=='R'){
        v->eye[0]=v->eye_home[0]; v->eye[1]=v->eye_home[1]; v->eye[2]=v->eye_home[2];
    }
}
