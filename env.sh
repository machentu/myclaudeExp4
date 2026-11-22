# 云天(Intellifusion)TyTVM 工具链研究环境 — source env.sh
# 原理:预编译 /DEngine/tytvm/tytvm_build 提供 libtvm.so(C++/LLVM 静态链接),
#       PYTHONPATH 前置源码包,使 deepeye 的 Python 逻辑从 .py(可读可改)加载而非 Cython .so
export TVM_PREBUILT=/DEngine/tytvm/tytvm_build
export SRC=/DEngine/projects/TyTVM_NNP3xx_src_v1.0.41/tytvm_src_pkg
export LD_LIBRARY_PATH=$TVM_PREBUILT/build
export PYTHONPATH=$SRC/python:$SRC/topi/python:$SRC/nnvm/python:$SRC/deepeye/python
export MPLCONFIGDIR=/tmp/mpl
export PY=/usr/bin/python3.8          # 必须 3.8,匹配 cpython-38 的 .so
export PIPE_ROOT=/DEngine/projects/npu_pipeline

# s6/s8 的 linker→net_bin 需要 ARM 交叉工具链(deepeye/net_bin_helper.py:202
# 直接 os.environ["ARM_C_COMPILER"],缺了必 KeyError)。
# 厂商 cross_toolchain_setup.sh 的 nnp318p 段指向 $TVM_HOME/external_toolchain/
# arm-fhv512c-linux-uclibcgnueabihf-b2 —— 容器内不存在。改用 /opt 下现成的
# molchipv500(二进制前缀 arm-mol-linux-uclibcgnueabihf- 与厂商 nnp318 段完全一致,
# nnp318 是 nnp318p 姊妹芯片;2026-11-20 实测编译 v010 wrapper.c 通过)。
# flags 照抄厂商 nnp318/318p 段。
export ARM_TOOL=/opt/molchipv500-armgcc-uclibc
export ARM_C_COMPILER=$ARM_TOOL/bin/arm-mol-linux-uclibcgnueabihf-gcc
export ARM_CXX_COMPILER=$ARM_TOOL/bin/arm-mol-linux-uclibcgnueabihf-g++
export ARM_LD=$ARM_TOOL/bin/arm-mol-linux-uclibcgnueabihf-ld
export ARM_CXX_FLAGS="-mfloat-abi=hard -mfpu=vfpv4-d16 -mcpu=cortex-a7"
