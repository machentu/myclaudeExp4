#!/bin/bash
# build_pc.sh - PC x86 构建 rgbd_tum_app_v8seg_full (MNN 后端)
# 链 mnn_full_only_alone/build_mnn/libMNN.a (与板端同源码树 MNN, 数值对拍前提)
# 用法: bash build_pc.sh
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"                      # slampart/
MNN_INC="/DEngine/projects/MNN/include"
MNN_LIB="/DEngine/projects/mnn_full_only_alone/build_mnn/libMNN.a"

OUT="$ROOT/build_pc"
mkdir -p "$OUT"

CXXFLAGS="-std=c++11 -O2 -Wall -DNGD_BOW=1 -I$ROOT/refactor/include \
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
    gcc -std=c11 -O2 -Wall -DNGD_BOW=1 -I$ROOT/refactor/include -I$HERE/include \
        -I$HERE/src/stb -I$HERE/src/yolov8seg -I$MNN_INC -c "$f" -o "$o"
    OBJS="$OBJS $o"
done
for f in $APP_CXX; do
    o="$OUT/$(basename "${f%.*}").o"
    g++ $CXXFLAGS -c "$f" -o "$o"
    OBJS="$OBJS $o"
done

g++ $CXXFLAGS "$HERE/src/main.cpp" $OBJS "$MNN_LIB" -lpthread -ldl \
    -o "$OUT/rgbd_tum_app_v8seg_full"
echo "[ok] $OUT/rgbd_tum_app_v8seg_full"
