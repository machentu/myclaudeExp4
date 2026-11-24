#!/bin/bash
# build_arm_xc01_npu23.sh - [1c] 混合精度 BA: npu16 + src/ngd_opt_lmf32.c + -DNGD_BA_F32=1
#   [2026-11-24] 补 -DNGD_IMG_PREFETCH=0: NGD_IMG_PREFETCH 源码树默认已翻 1,
#   本脚本是 _xc01npuW (无预取 W 参考) 的 bit 级重建器, 钉 0 保产物恒等
#   (线性代数 f32+NEON, f64 收口; ba.c 分发 F32>SPARSE>FAST>dense); 产物 _xc01npuW
# PC 对拍: ba_abtest/test_ba_f32.c 质量带 PASS (chi_rel<=2.5e-4, outlier 0 分歧,
# 盆距离=ulp 基线带)。板端门禁: 质量带 (ATE/KF 带), 非 bit 等价。
# 与 build_arm_npu.sh (FY01P/NNP318) 的区别:
#   工具链   molchip → slampart/arm-fhva12c (gcc 10.5, -mfpu=neon-vfpv4: xc01 有 NEON)
#   DNN 后端 NGD_DNN_BACKEND=2 → 3 (TY_NPU 运行时, 源码 qwen_xc01_mul/board_qrun)
#   ORB 后端 -DNGD_ORB_DSP=1 -DNGD_OF_NEON=1 -DNGD_POST_NEON=1 -DNGD_BA_FAST=1 -DNGD_BA_F32=1 -DNGD_BA_COVIS_CAP=24 -DNGD_YOLO_NET_W=480 -DNGD_YOLO_NET_H=256 -DNGD_YOLO_TAG=\"256x480\" (VDSP 固件 sram_all.bin, src/orb_dsp.c + src/dsp/dspcalling.c)
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

OUT="$ROOT/build_arm_xc01_npu23"
mkdir -p "$OUT"

# 全部用 gnu 方言 (SDK 头兼容性最稳); -Wno-*-cast: SDK vmm_api.h 32 位指针/整数转换告警
# -include stdio.h: refactor 部分 .c 的调试 printf 未包含 stdio.h (uclibc 下无传递包含)
CCFLAGS="-std=gnu11 -O2 -Wall $ARM_FPU -DNGD_BOW=1 -DNGD_DNN_BACKEND=3 -DNGD_ORB_DSP=1 -DNGD_OF_NEON=1 -DNGD_POST_NEON=1 -DNGD_BA_FAST=1 -DNGD_BA_F32=1 -DNGD_IMG_PREFETCH=0 -DNGD_BA_COVIS_CAP=24 -DNGD_YOLO_NET_W=480 -DNGD_YOLO_NET_H=256 -DNGD_YOLO_TAG=\"256x480\" \
 -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -include stdio.h \
 -I$ROOT/refactor/include -I$HERE/include -I$HERE/src/stb -I$HERE/src/yolov8seg \
 -I$QB -I$SDK_INC -I$SDK_INC/types -I$SDK_INC/dsp"
CXXFLAGS="-std=gnu++11 -O2 -Wall $ARM_FPU -DNGD_BOW=1 -DNGD_DNN_BACKEND=3 -DNGD_ORB_DSP=1 -DNGD_OF_NEON=1 -DNGD_POST_NEON=1 -DNGD_BA_FAST=1 -DNGD_BA_F32=1 -DNGD_IMG_PREFETCH=0 -DNGD_BA_COVIS_CAP=24 -DNGD_YOLO_NET_W=480 -DNGD_YOLO_NET_H=256 -DNGD_YOLO_TAG=\"256x480\" \
 -I$ROOT/refactor/include -I$HERE/include -I$HERE/src/stb -I$HERE/src/yolov8seg \
 -I$QB -I$SDK_INC -I$SDK_INC/types -I$SDK_INC/dsp"

CORE_SRC=$(ls "$ROOT"/refactor/src/*.c)
APP_C="$HERE/src/imio_pure.c $HERE/src/yaml_pure.c $HERE/src/color_convert.c \
 $HERE/src/image_proc.c $HERE/src/stb_image_impl.c $HERE/src/demo_orb.c \
 $HERE/src/ngd_opt_ofneon.c $HERE/src/ngd_opt_postneon.c $HERE/src/ngd_opt_kfsparse.c $HERE/src/ngd_opt_lmneon.c $HERE/src/ngd_opt_lmf32.c $HERE/src/orb_dsp.c $HERE/src/dsp/dspcalling.c \
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
    -o "$OUT/rgbd_tum_app_v8seg_full_xc01npuW"
echo "[ok] $OUT/rgbd_tum_app_v8seg_full_xc01npuW"
