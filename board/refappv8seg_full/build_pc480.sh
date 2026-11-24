#!/bin/bash
# build_pc480.sh - PC x86 构建 rgbd_tum_app_v8seg_full (MNN 后端, 256x480 clip 模型口径)
# 鉴别实验: app 解码/分辨率适配 vs NPU netbin —— 与 build_pc.sh 唯一区别是三个 yolo 宏。
# 用法: bash build_pc480.sh
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"                      # slampart/
MNN_INC="/DEngine/projects/MNN/include"
MNN_LIB="/DEngine/projects/mnn_full_only_alone/build_mnn/libMNN.a"

OUT="$ROOT/build_pc480"
mkdir -p "$OUT"

YDEF='-DNGD_YOLO_NET_W=480 -DNGD_YOLO_NET_H=256 -DNGD_YOLO_TAG="256x480"'

CXXFLAGS="-std=c++11 -O2 -Wall -DNGD_BOW=1 $YDEF -I$ROOT/refactor/include \
 -I$HERE/include -I$HERE/src/stb -I$HERE/src/yolov8seg -I$MNN_INC"

CORE_SRC=$(ls "$ROOT"/refactor/src/*.c)
APP_C="$HERE/src/imio_pure.c $HERE/src/yaml_pure.c $HERE/src/color_convert.c \
 $HERE/src/image_proc.c $HERE/src/stb_image_impl.c $HERE/src/demo_orb.c \
 $HERE/src/yolov8seg/libkptdecode.c $HERE/src/yolov8seg/libnms.c \
 $HERE/src/yolov8seg/libobjectpool.c"
APP_CXX="$HERE/src/system.cpp $HERE/src/optflow_track.cpp \
 $HERE/src/viewer.cpp $HERE/src/yolov8_seg.cpp"

OBJS=""
for f in $CORE_SRC $APP_C; do
    o="$OUT/$(basename "${f%.*}").o"
    gcc -std=c11 -O2 -Wall -DNGD_BOW=1 $YDEF -I$ROOT/refactor/include -I$HERE/include \
        -I$HERE/src/stb -I$HERE/src/yolov8seg -I$MNN_INC -c "$f" -o "$o"
    OBJS="$OBJS $o"
done
for f in $APP_CXX; do
    o="$OUT/$(basename "${f%.*}").o"
    g++ $CXXFLAGS -c "$f" -o "$o"
    OBJS="$OBJS $o"
done

g++ $CXXFLAGS "$HERE/src/main.cpp" $OBJS "$MNN_LIB" -lpthread -ldl \
    -o "$OUT/rgbd_tum_app_v8seg_full_pc480"
echo "[ok] $OUT/rgbd_tum_app_v8seg_full_pc480"
