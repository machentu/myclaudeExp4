#!/bin/bash
# build_arm_xc01.sh - xc01 (NNP310 + VDSP, A7 NEON) 交叉编译 rgbd_tum_app_v8seg_full_xc01vD
# 与 build_arm_npu.sh (FY01P/NNP318) 的区别:
#   工具链   molchip → slampart/arm-fhva12c (gcc 10.5, -mfpu=neon-vfpv4: xc01 有 NEON)
#   DNN 后端 NGD_DNN_BACKEND=2 → 3 (TY_NPU 运行时, 源码 qwen_xc01_mul/board_qrun)
#   ORB 后端 -DNGD_ORB_DSP=0 (VDSP 固件 sram_all.bin, src/orb_dsp.c + src/dsp/dspcalling.c)
#   SDK      FY01P_SDK → xc01sdk MC635X (头 media/mpp/include, 库 va12c/lib)
#   链接     12×npu .a + libmpi.a 包 --start-group (参照 qwen board_qrun/Makefile, 板端已验证)
# 产物全新文件名 _xc01: 规避 NFS 同名覆盖 -> exec 页陈旧 -> 假 SIGILL 陷阱
# 用法: bash build_arm_xc01.sh
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
CCFLAGS="-std=gnu11 -O2 -Wall $ARM_FPU -DNGD_BOW=0 -DNGD_DNN_BACKEND=3 -DNGD_ORB_DSP=0 \
 -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -include stdio.h \
 -DNGD_BA_F32=0 -I$ROOT/refactor/include -I$HERE/include -I$HERE/src/stb -I$HERE/src/yolov8seg \
 -I$QB -I$SDK_INC -I$SDK_INC/types -I$SDK_INC/dsp"
CXXFLAGS="-std=gnu++11 -O2 -Wall $ARM_FPU -DNGD_BOW=0 -DNGD_DNN_BACKEND=3 -DNGD_ORB_DSP=0 \
 -DNGD_BA_F32=0 -I$ROOT/refactor/include -I$HERE/include -I$HERE/src/stb -I$HERE/src/yolov8seg \
 -I$QB -I$SDK_INC -I$SDK_INC/types -I$SDK_INC/dsp"

# NGD_BOW=0: 词袋三件 (bow/kfdb/loopclosing) 符号被守卫裁掉, 不参与编译
CORE_SRC=$(ls "$ROOT"/refactor/src/*.c | grep -v -E "/(bow|kfdb|loopclosing)\.c$")
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
$CXX $CXXFLAGS "$HERE/src/main.cpp" $OBJS $LDLIBS -static-libstdc++ \
    -o "$OUT/rgbd_tum_app_v8seg_full_xc01vD"
echo "[ok] $OUT/rgbd_tum_app_v8seg_full_xc01vD"
