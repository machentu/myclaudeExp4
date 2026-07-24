cmake_minimum_required(VERSION 3.15)
project(ngd_app_v8seg LANGUAGES CXX C)

# ngd_app_v8seg -- refapp variant whose dynamic mask is produced END-TO-END by
# YOLOv8-seg (yolov8n-seg-320x448.onnx) every frame, replacing refapp's
# yolo-fastest-xl + LK PredictCurrentMask two-stage chain. Links ngd_core +
# ngd_shell + OpenCV (image IO + yaml + dnn). Decoder C files under
# src/yolov8seg are copied verbatim from testblurDnn.
#
# Configure with the vcpkg toolchain (same as refmain/refapp):
#   c:\Users\frank.tu\CMakePath.bat
#   set CMAKE=C:/src/vcpkg/downloads/tools/cmake-3.31.10-windows-x86_64/bin/cmake.exe
#   %CMAKE% -B build -G "Visual Studio 17 2022" -A x64 ^
#       -DCMAKE_TOOLCHAIN_FILE=C:/src/vcpkg/scripts/buildsystems/vcpkg.cmake
#   %CMAKE% --build build --config Release

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# ---- OpenCV (vcpkg). ----
find_package(OpenCV REQUIRED)
message(STATUS "OpenCV ${OpenCV_VERSION} found")

# ---- refactor pure-C core (prebuilt ngd_core.lib) ----
set(NGD_CORE_INCLUDE "../refactor/include" CACHE PATH "refactor/include (ngd headers)")
set(NGD_CORE_LIB     "../refactor/myBuild/Release/ngd_core.lib" CACHE FILEPATH "prebuilt ngd_core.lib")
# Resolve to ABSOLUTE: MSVC mangles a relative path in target_link_libraries
# (appends .lib again -> ngd_core.lib.lib).
get_filename_component(NGD_CORE_LIB_ABS "${NGD_CORE_LIB}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# ---- refmain OpenCV shell (prebuilt ngd_shell.lib) -- for OF-pose (SearchByOpticalFlow + solvePnPRansac) ----
set(NGD_SHELL_INCLUDE "../refmain/include" CACHE PATH "refmain/include (ngd_shell headers)")
set(NGD_SHELL_LIB     "../refmain/build/Release/ngd_shell.lib" CACHE FILEPATH "prebuilt ngd_shell.lib")
get_filename_component(NGD_SHELL_LIB_ABS "${NGD_SHELL_LIB}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

if(MSVC)
  add_compile_options(/utf-8 /W3 /EHsc /O2)
  add_compile_definitions(NOMINMAX _CRT_SECURE_NO_WARNINGS)
endif()

# ---- shell lib: copied refapp app layer + Yolov8SegDetector + testblurDnn C decoders ----
add_library(ngd_app_v8seg STATIC
    src/imio.cpp
    src/yaml.cpp
    src/system.cpp
    src/optflow_track.cpp
    src/viewer.cpp
    src/yolov8_seg.cpp
    src/yolov8seg/libkptdecode.c
    src/yolov8seg/libnms.c
    src/yolov8seg/libobjectpool.c
    src/demo_orb.c
)
target_include_directories(ngd_app_v8seg PUBLIC
    include include/ngd_app
    ${NGD_CORE_INCLUDE} ${NGD_SHELL_INCLUDE}
    src/yolov8seg)                 # for libkptdecode.h / outsize.h / sigmoidTable_q15.h
target_link_libraries(ngd_app_v8seg PUBLIC ${OpenCV_LIBS} ${NGD_CORE_LIB_ABS} ${NGD_SHELL_LIB_ABS})
if(MSVC)
  target_compile_options(ngd_app_v8seg PRIVATE /wd4244)
endif()

# ---- Uncomment to use demo_feature_extraction.c ORB instead of ngd_orb_extract ----
#   Or configure with: cmake -DUSE_DEMO_ORB=ON ...
option(USE_DEMO_ORB "Use demo_feature_extraction.c ORB extractor" OFF)
if(USE_DEMO_ORB)
  target_compile_definitions(ngd_app_v8seg PRIVATE USE_DEMO_ORB)
  message(STATUS "Demo ORB extractor: ENABLED (demo_feature_extraction.c)")
endif()

# ---- ORB comparison test: runs both extractors on one image ----
# Usage: test_orb_compare <image.pgm>
# Links ngd_core + demo_orb.c directly (no USE_DEMO_ORB needed).
add_executable(test_orb_compare test/test_orb_compare.c src/demo_orb.c)
target_include_directories(test_orb_compare PRIVATE
    include include/ngd_app ${NGD_CORE_INCLUDE})
target_link_libraries(test_orb_compare PRIVATE ${NGD_CORE_LIB_ABS})
set_target_properties(test_orb_compare PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
if(MSVC)
  target_compile_options(test_orb_compare PRIVATE /wd4244)
endif()

# ---- rgbd_tum-equivalent driver (YOLOv8-seg end-to-end mask variant) ----
# Usage: rgbd_tum_app_v8seg <vocab> <settings> <sequence> <association> [yolo_dir]
add_executable(rgbd_tum_app_v8seg src/main.cpp)
target_link_libraries(rgbd_tum_app_v8seg PRIVATE ngd_app_v8seg)
set_target_properties(rgbd_tum_app_v8seg PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
if(MSVC)
  target_compile_options(rgbd_tum_app_v8seg PRIVATE /wd4244)
endif()
