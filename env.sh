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
