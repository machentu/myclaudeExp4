#!/bin/bash
# build_arm_npu.sh - FY01P (NNP318 A7, 无 NEON) 交叉编译 rgbd_tum_app_v8seg_full_fy01p_npu
# 与 build_arm.sh 的区别: 板端 DNN 后端走 NPU (NGD_DNN_BACKEND=2), 输入 AIPP uint8 原图;
#   不链接 MNN, 改链接 qwen_board 的 NPU 运行时 (npu_runtime.c/mmz.c/logger.c/subgraph_pool.cpp)
#   + FY SDK libmcai.a/libmpi.a。
# 工具链 (已迁址): /home/frank/workspace/toochain/arm-molchip-linux (gcc 4.9.4)
# 方言: 混 SDK 头 (fy_defines.h 用裸 asm()) 的 TU 必须 gnu11/gnu++11 — 此处全部用 gnu 方言。
# 产物全新文件名 _fy01p_npu: 规避 NFS 同名覆盖 -> exec 页陈旧 -> 假 SIGILL 陷阱
# 用法: bash build_arm_npu.sh
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"                      # slampart/

CROSS="/home/frank/workspace/toochain/arm-molchip-linux/bin/arm-mol-linux-uclibcgnueabihf-"
CC="$CROSS"gcc
CXX="$CROSS"g++

SDK="/home/frank/workspace/github/fy01p/FY01P_SDK_V0.9.5_RC1_202608031855"
SDK_INC="$SDK/mpp/core/include/v2"
SDK_LIB="$SDK/output/fy01p_chip/lib"
QB="/DEngine/projects/qwen_board"                   # NPU 运行时基线源码

# FPU flags 对齐 qwen_board/Makefile: 芯片 A7 无 NEON, 任何 NEON 编码 -> SIGILL
ARM_FPU="-mcpu=cortex-a7 -mfloat-abi=hard -mfpu=vfpv4-d16 -mno-unaligned-access"
ARM_LDFLAGS="-static-libstdc++ -static-libgcc"
LDLIBS="$SDK_LIB/libmcai.a $SDK_LIB/libmpi.a -lpthread -lm -ldl"

OUT="$ROOT/build_arm_npu"
mkdir -p "$OUT"

# 全部用 gnu 方言: SDK fy_defines.h 用裸 asm() (严格 c11/c++11 不认)
CCFLAGS="-std=gnu11 -O2 -Wall $ARM_FPU -DNGD_BOW=1 -DNGD_DNN_BACKEND=2 \
 -DNGD_BA_F32=0 -I$ROOT/refactor/include -I$HERE/include -I$HERE/src/stb -I$HERE/src/yolov8seg -I$QB -I$SDK_INC"
CXXFLAGS="-std=gnu++11 -O2 -Wall $ARM_FPU -DNGD_BOW=1 -DNGD_DNN_BACKEND=2 \
 -DNGD_BA_F32=0 -I$ROOT/refactor/include -I$HERE/include -I$HERE/src/stb -I$HERE/src/yolov8seg -I$QB -I$SDK_INC"

CORE_SRC=$(ls "$ROOT"/refactor/src/*.c)
APP_C="$HERE/src/imio_pure.c $HERE/src/yaml_pure.c $HERE/src/color_convert.c \
 $HERE/src/image_proc.c $HERE/src/stb_image_impl.c $HERE/src/demo_orb.c \
 $HERE/src/yolov8seg/libkptdecode.c $HERE/src/yolov8seg/libnms.c \
 $HERE/src/yolov8seg/libobjectpool.c $HERE/src/math_shim_uclibc.c"
APP_CXX="$HERE/src/system.cpp $HERE/src/optflow_track.cpp \
 $HERE/src/viewer.cpp $HERE/src/yolov8_seg.cpp"

# NPU 运行时 (qwen_board 基线源, 只读引用)
NPU_C="$QB/common/mmz.c $QB/common/logger.c $QB/backend/npu_runtime.c"
NPU_CXX="$QB/backend/subgraph_pool.cpp"

OBJS=""
for f in $CORE_SRC $APP_C $NPU_C; do
    o="$OUT/$(basename "${f%.*}").o"
    $CC $CCFLAGS -c "$f" -o "$o"
    OBJS="$OBJS $o"
done
for f in $APP_CXX $NPU_CXX; do
    o="$OUT/$(basename "${f%.*}").o"
    $CXX $CXXFLAGS -c "$f" -o "$o"
    OBJS="$OBJS $o"
done

$CXX $CXXFLAGS "$HERE/src/main.cpp" $OBJS $LDLIBS $ARM_LDFLAGS \
    -o "$OUT/rgbd_tum_app_v8seg_full_fy01p_npu"
echo "[ok] $OUT/rgbd_tum_app_v8seg_full_fy01p_npu"
