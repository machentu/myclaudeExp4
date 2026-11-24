cmake_minimum_required(VERSION 3.10)
project(ngd_app_v8seg_full LANGUAGES CXX C)

# ngd_app_v8seg_full — ZERO-OpenCV variant of refappv8seg.
# Replaces OpenCV (image IO + yaml + dnn + imgproc + highgui) with:
#   - stb_image (PNG loading)
#   - ONNX Runtime (YOLOv8-seg DNN inference)
#   - pure-C colour conversion + image processing
#   - pure-C YAML parser
#   - headless viewer stub
# Removes ngd_shell dependency (pure-C LK + EPnP already in ngd_core).
# Links only ngd_core + onnxruntime.
#
# Prerequisites:
#   vcpkg install onnxruntime:x64-windows
#   prebuilt ngd_core.lib (../refactor/myBuild/Release/)
#
# Configure:
#   cmake -B build -G "Visual Studio 17 2022" -A x64 ^
#       -DCMAKE_TOOLCHAIN_FILE=C:/src/vcpkg/scripts/buildsystems/vcpkg.cmake
#   cmake --build build --config Release

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# ---- DNN backend selector ----
set(NGD_DNN_BACKEND "1" CACHE STRING "DNN backend: 0=onnxruntime, 1=mnn (default), 2=fy01p NPU(nnp318), 3=xc01 NPU(nnp310)")
message(STATUS "DNN backend: ${NGD_DNN_BACKEND} (0=onnxruntime, 1=mnn, 2=fy01p NPU, 3=xc01 NPU)")

# ---- DNN library (onnxruntime or MNN) ----
if(NGD_DNN_BACKEND EQUAL 1)
    # ---- MNN (local copy under thirdparty/) ----
    # Pre-built with: cd thirdparty && git clone --depth 1 --branch 3.1.0 https://github.com/alibaba/MNN.git MNN310
    #   cd MNN310 && cmake -B build -G "Visual Studio 17 2022" -A x64 -DMNN_BUILD_SHARED_LIBS=ON ...
    #   cmake --build build --config Release -- /m:1
    #set(MNN_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/MNN310" CACHE PATH "MNN install root")

    set(MNN_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/build_arm/install" CACHE PATH "MNN install root")

    find_path(MNN_INCLUDE_DIR MNN/Interpreter.hpp
        PATHS "${MNN_ROOT}/include" "${MNN_ROOT}/source" NO_DEFAULT_PATH)
    # find_library(MNN_LIB MNN.lib
    #     PATHS "${MNN_ROOT}/build/Release" "${MNN_ROOT}/build" NO_DEFAULT_PATH)
    find_library(MNN_LIB libMNN.a #MNN.lib
        PATHS "${MNN_ROOT}/lib" "${MNN_ROOT}/build" NO_DEFAULT_PATH)
    if(NOT MNN_INCLUDE_DIR )
        message(FATAL_ERROR "MNN include not found at ${MNN_ROOT}. Build MNN first.")
    endif()

    if(NOT MNN_LIB)
        message(FATAL_ERROR "MNN lib not found at ${MNN_ROOT}. Build MNN first.")
    endif()

    message(STATUS "MNN found: ${MNN_ROOT}")
    add_library(mnn SHARED IMPORTED)
    set_target_properties(mnn PROPERTIES
        IMPORTED_LOCATION "${MNN_LIB}" #/../MNN.dll
        IMPORTED_IMPLIB   "${MNN_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${MNN_INCLUDE_DIR}")
    set(DNN_LIB mnn)
else()
    # ---- ONNX Runtime (default) ----
    set(ORT_ROOT "C:/src/onnxruntime/v1.20.1/onnxruntime-win-x64-1.20.1" CACHE PATH "ONNX Runtime install root")
    find_path(ORT_INCLUDE_DIR onnxruntime_c_api.h
        PATHS "${ORT_ROOT}/include" NO_DEFAULT_PATH)
    find_library(ORT_LIB onnxruntime.lib
        PATHS "${ORT_ROOT}/lib" NO_DEFAULT_PATH)
    if(NOT ORT_INCLUDE_DIR OR NOT ORT_LIB)
        message(FATAL_ERROR "ONNX Runtime not found at ${ORT_ROOT}. Download from https://github.com/microsoft/onnxruntime/releases")
    endif()
    message(STATUS "ONNX Runtime found: ${ORT_ROOT}")
    add_library(onnxruntime SHARED IMPORTED)
    set_target_properties(onnxruntime PROPERTIES
        IMPORTED_LOCATION "${ORT_ROOT}/lib/onnxruntime.dll"
        IMPORTED_IMPLIB   "${ORT_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${ORT_INCLUDE_DIR}")
    set(DNN_LIB onnxruntime)
endif()

#tzg
set(MNN_LIB_PATH "/home/frank/workspace/github/use_sdk20/slamproj/refappv8seg_full/thirdparty/build_arm/install/lib/libMNN.a")
#/home/frank/workspace/github/use_sdk20/slamproj/refappv8seg_full/thirdparty/build_arm/install/libMNN.a


# ---- refactor pure-C core (prebuilt ngd_core.lib) ----
set(NGD_CORE_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/../refactor/include" CACHE PATH "refactor/include (ngd headers)")
set(NGD_CORE_LIB     "${CMAKE_CURRENT_SOURCE_DIR}/../refactor/build_arm/libngd_core.a" CACHE FILEPATH "prebuilt ngd_core.lib")
get_filename_component(NGD_CORE_LIB_ABS "${NGD_CORE_LIB}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

if(MSVC)
  add_compile_options(/utf-8 /W3 /EHsc /O2)
  add_compile_definitions(NOMINMAX _CRT_SECURE_NO_WARNINGS)
endif()

# ---- stb_image include path ----
set(STB_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/src/stb")

#set(REFACT_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/../refactor/include")

# ---- core app library (zero OpenCV) ----
add_library(ngd_app_v8seg_full STATIC
    src/imio_pure.c
    src/yaml_pure.c
    src/color_convert.c
    src/image_proc.c
    src/stb_image_impl.c
    src/system.cpp
    src/optflow_track.cpp
    src/viewer.cpp
    src/yolov8_seg.cpp
    src/yolov8seg/libkptdecode.c
    src/yolov8seg/libnms.c
    src/yolov8seg/libobjectpool.c
    src/demo_orb.c
)
target_include_directories(ngd_app_v8seg_full PUBLIC
    include include/ngd_app
    ${NGD_CORE_INCLUDE}
    ${STB_INCLUDE}
    #${REFACT_INCLUDE}
    src/yolov8seg)
target_link_libraries(ngd_app_v8seg_full PUBLIC
    ${NGD_CORE_LIB_ABS}
    ${DNN_LIB}
    ${MNN_LIB_PATH}
    )
if(MSVC)
  target_compile_options(ngd_app_v8seg_full PRIVATE /wd4244)
endif()

# Force pure-C OF + EPnP (these defines are read by optflow_track.cpp):
target_compile_definitions(ngd_app_v8seg_full PRIVATE
    NGD_DNN_BACKEND=${NGD_DNN_BACKEND}
    NGD_OF_LK_C=1
    NGD_PNP=0
    NGD_LK_BACKEND=2)
message(STATUS "OF backend: pure-C float LK + EPnP (NGD_OF_LK_C=1, NGD_PNP=0)")

# ---- Demo ORB option ----
option(USE_DEMO_ORB "Use demo_feature_extraction.c ORB extractor" OFF)
if(USE_DEMO_ORB)
  target_compile_definitions(ngd_app_v8seg_full PRIVATE USE_DEMO_ORB)
  message(STATUS "Demo ORB extractor: ENABLED")
endif()

# ---- ORB comparison test ----
add_executable(test_orb_compare_full test/test_orb_compare.c src/demo_orb.c)
target_include_directories(test_orb_compare_full PRIVATE
    include include/ngd_app ${NGD_CORE_INCLUDE})
target_link_libraries(test_orb_compare_full PRIVATE ${NGD_CORE_LIB_ABS} m)
set_target_properties(test_orb_compare_full PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
if(MSVC)
  target_compile_options(test_orb_compare_full PRIVATE /wd4244)
endif()

# ---- rgbd_tum driver ----
add_executable(rgbd_tum_app_v8seg_full src/main.cpp)
target_link_libraries(rgbd_tum_app_v8seg_full PRIVATE 
    -Wl,--start-group
    ngd_app_v8seg_full 
    ${MNN_LIB_PATH} 
    ${DNN_LIB}
    -Wl,--end-group
    pthread
    m
    )
set_target_properties(rgbd_tum_app_v8seg_full PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
if(MSVC)
  target_compile_options(rgbd_tum_app_v8seg_full PRIVATE /wd4244)
endif()
# Copy DNN DLL next to the executable at build time.
if(WIN32)
if(NGD_DNN_BACKEND EQUAL 1)
    add_custom_command(TARGET rgbd_tum_app_v8seg_full POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MNN_ROOT}/build/Release/MNN.dll"
            "$<TARGET_FILE_DIR:rgbd_tum_app_v8seg_full>/MNN.dll"
        COMMENT "Copying MNN.dll (v3.1.0) to output directory")
else()
    add_custom_command(TARGET rgbd_tum_app_v8seg_full POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ORT_ROOT}/lib/onnxruntime.dll"
            "$<TARGET_FILE_DIR:rgbd_tum_app_v8seg_full>/onnxruntime.dll"
        COMMENT "Copying onnxruntime.dll to output directory")
endif()
endif()
