#!/bin/bash
# build_arm.sh - FY01P (NNP318 A7, 无 NEON) 交叉编译 rgbd_tum_app_v8seg_full_fy01p
# 工具链: molchipv500-armgcc-uclibc_1 (gcc 4.9.4)
# MNN:    qwen_board/mnn_glue/mnn_build_arm/libMNN.a (vfpv4-d16, 无 NEON, 已验证可跑板)
# 链接:   ARM 静态 libstdc++/libgcc (照抄 mnn_glue/Makefile ARM_LDFLAGS)
# 产物全新文件名 _fy01p: 规避 NFS 同名覆盖 -> exec 页陈旧 -> 假 SIGILL 陷阱
# 用法: bash build_arm.sh
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"                      # slampart/

CROSS="/home/frank/workspace/toolchain/molchipv500-armgcc-uclibc_1/bin/arm-mol-linux-uclibcgnueabihf-"
CC="$CROSS"gcc
CXX="$CROSS"g++
MNN_INC="/DEngine/projects/MNN/include"
MNN_LIB="${MNN_LIB:-/DEngine/projects/qwen_board/mnn_glue/mnn_build_arm/libMNN.a}"
# 单线程 MNN (无 ThreadPool worker, 排查板端堆损坏): MNN_LIB=.../mnn_build_arm_st/libMNN.a

# FPU flags 对齐 mnn_glue/Makefile: A7 无 NEON, 任何 NEON 编码 -> SIGILL
# (2026-08-24 NEON 探针复测 11/11 SIGILL, VFP 7/7 OK)
ARM_FPU="-mcpu=cortex-a7 -mfloat-abi=hard -mfpu=vfpv4-d16 -mno-unaligned-access"
ARM_LDFLAGS="-static-libstdc++ -static-libgcc"

OUT="$ROOT/build_arm"
mkdir -p "$OUT"

CXXFLAGS="-std=c++11 -O2 -Wall $ARM_FPU -DNGD_BOW=1 -DNGD_BA_F32=0 -I$ROOT/refactor/include \
 -I$HERE/include -I$HERE/src/stb -I$HERE/src/yolov8seg -I$MNN_INC"

CORE_SRC=$(ls "$ROOT"/refactor/src/*.c)
APP_C="$HERE/src/imio_pure.c $HERE/src/yaml_pure.c $HERE/src/color_convert.c \
 $HERE/src/image_proc.c $HERE/src/stb_image_impl.c $HERE/src/demo_orb.c \
 $HERE/src/yolov8seg/libkptdecode.c $HERE/src/yolov8seg/libnms.c \
 $HERE/src/yolov8seg/libobjectpool.c $HERE/src/math_shim_uclibc.c"
APP_CXX="$HERE/src/system.cpp $HERE/src/optflow_track.cpp \
 $HERE/src/viewer.cpp $HERE/src/yolov8_seg.cpp"

OBJS=""
for f in $CORE_SRC $APP_C; do
    o="$OUT/$(basename "${f%.*}").o"
    $CC -std=c11 -O2 -Wall $ARM_FPU -DNGD_BOW=1 -DNGD_BA_F32=0 -I$ROOT/refactor/include -I$HERE/include \
        -I$HERE/src/stb -I$HERE/src/yolov8seg -I$MNN_INC -c "$f" -o "$o"
    OBJS="$OBJS $o"
done
for f in $APP_CXX; do
    o="$OUT/$(basename "${f%.*}").o"
    $CXX $CXXFLAGS -c "$f" -o "$o"
    OBJS="$OBJS $o"
done

$CXX $CXXFLAGS "$HERE/src/main.cpp" $OBJS "$MNN_LIB" $ARM_LDFLAGS \
    -lpthread -lm -ldl -o "$OUT/rgbd_tum_app_v8seg_full_fy01p"
echo "[ok] $OUT/rgbd_tum_app_v8seg_full_fy01p"
