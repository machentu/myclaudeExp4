#!/bin/bash
# build_arm_xc01_mnn.sh - xc01 全 CPU 基线: rgbd_tum_app_v8seg_full_xc01mnn
# 与 build_arm_xc01.sh (NPU+DSP 主线) 的区别:
#   DNN 后端 NGD_DNN_BACKEND=3 → 1 (MNN CPU, libMNN.a 来自 MNN/mnn_xc01_build)
#   ORB 后端 NGD_ORB_DSP=1 → 0 (CPU ORB)
#   NGD_BOW=1 保持一致 (同词袋, 与主线同口径对比 ATE)
# 用途: 全 CPU 基线跑同数据集 → ATE 对比 NNP310+VDSP 主线 (v2)
# 其余 (工具链/SDK/链接结构) 与 build_arm_xc01_cpuorb.sh 完全同构 (已验证链接组合);
# NPU 运行时源照旧编译链接 (BACKEND=1 下死代码, 保持最小 diff 降风险)
# 产物全新文件名 _xc01mnn: 规避 NFS 同名覆盖 -> exec 页陈旧 -> 假 SIGILL 陷阱
# 用法: bash build_arm_xc01_mnn.sh
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"                      # slampart/

CROSS="$ROOT/arm-fhva12c-linux-uclibcgnueabihf-b5/bin/arm-fhva12c-linux-uclibcgnueabihf-"
CC="$CROSS"gcc
CXX="$CROSS"g++

SDK="/home/frank/workspace/github/xc01sdk/MC635X_IPC_V2.0.0_20250103"
SDK_INC="$SDK/media/mpp/include"                    # + /types (vmm_api.h) + /dsp (fh_vdsp_mpi.h)
SDK_LIB="$SDK/media/mpp/linux/va12c/lib"
QB="/DEngine/projects/qwen_xc01_mul/board_qrun"     # xc01 NPU 运行时源码 (只读引用)

MNN_ROOT="/home/frank/workspace/tianxuan/myProject/minicpm/MNN"
MNN_LIB="$MNN_ROOT/mnn_xc01_build_v2/libMNN.a"      # 3.6.1 xc01 交叉构建 v2 (armv7 汇编内核, 见 mnn_xc01_build/BUILD_NOTES.md)
if [ ! -f "$MNN_LIB" ]; then echo "[err] $MNN_LIB missing (先构建 mnn_xc01_build)"; exit 1; fi

# FPU flags 对齐 qwen board_qrun/Makefile (xc01 A7 带 NEON, 与 FY01P 的 vfpv4-d16 不同)
ARM_FPU="-mcpu=cortex-a7 -mfloat-abi=hard -mfpu=neon-vfpv4 -mno-unaligned-access"

# 与 qwen board_qrun 一致的 NPU 运行时库组 (TY_NPU/dcl/mdl_parse/hcp) + libmpi (VDSP+VMM)
NPU_LIBS="$SDK_LIB/npu/libdcl_ioctl.a $SDK_LIB/npu/libdcl_memory.a \
 $SDK_LIB/npu/libdcl_runtime.a $SDK_LIB/npu/libdcl_utils.a \
 $SDK_LIB/npu/libdrv_dp2000.a $SDK_LIB/npu/libmdl_parse.a \
 $SDK_LIB/npu/libstream_sched.a $SDK_LIB/npu/libhcp_cv.a \
 $SDK_LIB/npu/libhcp_ive.a $SDK_LIB/npu/libhcp_model.a \
 $SDK_LIB/npu/libhcp_npu.a $SDK_LIB/npu/libhcp_sys.a"
LDLIBS="-Wl,--start-group $NPU_LIBS $SDK_LIB/libmpi.a -Wl,--end-group -lpthread -lm -ldl"

OUT="$ROOT/build_arm_xc01"
mkdir -p "$OUT"

# 全部用 gnu 方言 (SDK 头兼容性最稳); -Wno-*-cast: SDK vmm_api.h 32 位指针/整数转换告警
# -include stdio.h: refactor 部分 .c 的调试 printf 未包含 stdio.h (uclibc 下无传递包含)
CCFLAGS="-std=gnu11 -O2 -Wall $ARM_FPU -DNGD_BOW=1 -DNGD_DNN_BACKEND=1 -DNGD_ORB_DSP=0 \
 -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -include stdio.h \
 -DNGD_BA_F32=0 -I$ROOT/refactor/include -I$HERE/include -I$HERE/src/stb -I$HERE/src/yolov8seg \
 -I$MNN_ROOT/include \
 -I$QB -I$SDK_INC -I$SDK_INC/types -I$SDK_INC/dsp"
CXXFLAGS="-std=gnu++11 -O2 -Wall $ARM_FPU -DNGD_BOW=1 -DNGD_DNN_BACKEND=1 -DNGD_ORB_DSP=0 \
 -DNGD_BA_F32=0 -I$ROOT/refactor/include -I$HERE/include -I$HERE/src/stb -I$HERE/src/yolov8seg \
 -I$MNN_ROOT/include \
 -I$QB -I$SDK_INC -I$SDK_INC/types -I$SDK_INC/dsp"

CORE_SRC=$(ls "$ROOT"/refactor/src/*.c)
APP_C="$HERE/src/imio_pure.c $HERE/src/yaml_pure.c $HERE/src/color_convert.c \
 $HERE/src/image_proc.c $HERE/src/stb_image_impl.c $HERE/src/demo_orb.c \
 $HERE/src/orb_dsp.c $HERE/src/dsp/dspcalling.c \
 $HERE/src/yolov8seg/libkptdecode.c $HERE/src/yolov8seg/libnms.c \
 $HERE/src/yolov8seg/libobjectpool.c $HERE/src/math_shim_uclibc.c"
APP_CXX="$HERE/src/system.cpp $HERE/src/optflow_track.cpp \
 $HERE/src/viewer.cpp $HERE/src/yolov8_seg.cpp"

# NPU 运行时 (qwen_xc01_mul/board_qrun 源, 只读引用)
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

# -static-libstdc++: SDK .a 内嵌了老工具链的静态 libstdc++ weak 定义, 动态链接时被拉入 exe
#   与板上 libstdc++.so 混绑 → locale/num_get 双拷贝分裂 → 全部 istream operator>> 失效
#   (最小复现: repro 只引用 TY_NPU_CreateModel 即触发; 详见 xc01_layout_switch_manual 排错表)
$CXX $CXXFLAGS "$HERE/src/main.cpp" $OBJS "$MNN_LIB" $LDLIBS -static-libstdc++ \
    -o "$OUT/rgbd_tum_app_v8seg_full_xc01mnn"
echo "[ok] $OUT/rgbd_tum_app_v8seg_full_xc01mnn"
