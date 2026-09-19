1. copy python_local_share_uv to .local/share/uv/python...

2. use uv_python312_nw

2.1 extract
2.2 backup python python3 python3.2
2.3 in .venv/bin use cmd: find ./ -type f -exec sed -i 's@srcPath@dstPath@g' {} +
2.4 copy back python
2.5 source dstPath/.venv/bin/activate



32bit 测试程序
sudo apt install gcc-multilib g++-multilib
mkdir build && cd build
cmake -DCMAKE_C_FLAGS=-m32 -DCMAKE_CXX_FLAGS=-m32 ..
make


set XWS_GEN=1
cmake --preset cstub32-dbg -DABSTRACT_NN_USEE-COEFF_BLOB=1 -DHAS_HIFI=1 
cmake --build build/cstub32-dbg

Use -DABSTRACT_NN_USE_COEFF_BLOB=1 when running make/cmake to use built-in weights

确认32位
file your_executable_name

cmake --toolchain ../cmake/toolchain-cstub.cmake -DCMAKE_C_FLAGS=-m32 -DCMAKE_CXX_FLAGS=-m32 -DCSTUB64=0 -DABSTRACT_NN_USEE-COEFF_BLOB=1 -DHAS_HIFI=1 ..
