# refappv8segdsp — refappv8seg variant with VP6/DSP ORB kernels (Win32 build).
#
# The DSP ORB uses refoptvp6v2 _full() functions for pixel operations
# (bilinear resize, Gaussian blur, FAST-9, IC_Angle, rBRIEF) that are
# bit-exact with refactor's C reference. Tile-related VP6 driver code
# (_vp6.c + cstub + TileManager) is also compiled so the same sources
# can support tile-based DSP emulation.
#
# Macro switch: USE_DSP_ORB=ON  → dsp_orb_extract()  (VP6 kernels)
#               USE_DSP_ORB=OFF → ngd_orb_extract()  (refactor, original)
#               USE_DEMO_ORB=ON → demo_orb_extract() (demo single-scale)
#
# Configure:
#   cmake -S refappv8segdsp -B refappv8segdsp/build -G "Visual Studio 17 2022" -A Win32 ^
#       -DOpenCV_DIR=C:/Users/frank.tu/Downloads/opencv-4.8.0/myBuild32/install
#   cmake --build refappv8segdsp/build --config Release
cmake_minimum_required(VERSION 3.15)
project(ngd_app_v8segdsp LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# ---- OpenCV (32-bit) ----
# Point to the 32-bit OpenCV install (no vcpkg toolchain needed).
if(NOT OpenCV_DIR)
  set(OpenCV_DIR "C:/Users/frank.tu/Downloads/opencv-4.8.0/myBuild32/install"
      CACHE PATH "32-bit OpenCV install root")
endif()
find_package(OpenCV REQUIRED)
message(STATUS "OpenCV ${OpenCV_VERSION} found at ${OpenCV_DIR}")

# ---- refactor pure-C core (Win32 prebuilt) ----
set(NGD_CORE_INCLUDE "../refactor/include" CACHE PATH "refactor/include (ngd headers)")
set(NGD_CORE_LIB     "../refactor/myBuild32/Release/ngd_core.lib" CACHE FILEPATH "Win32 prebuilt ngd_core.lib")
get_filename_component(NGD_CORE_LIB_ABS "${NGD_CORE_LIB}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# ---- refmain OpenCV shell (Win32 prebuilt) -- for OF-pose (SearchByOpticalFlow + solvePnPRansac) ----
set(NGD_SHELL_INCLUDE "../refmain/include" CACHE PATH "refmain/include (ngd_shell headers)")
set(NGD_SHELL_LIB     "../refmain/build32/Release/ngd_shell.lib" CACHE FILEPATH "Win32 prebuilt ngd_shell.lib")
get_filename_component(NGD_SHELL_LIB_ABS "${NGD_SHELL_LIB}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# ---- cstub (TIE intrinsic stubs, 32-bit only) ----
set(CSTUB_PATH "C:/usr/XtDevTools/install/builds/RI-2022.10-win32/fy01_202210/src/cstub"
    CACHE PATH "cstub source dir (32-bit TIE intrinsic stubs)")
if(NOT EXISTS "${CSTUB_PATH}")
  message(FATAL_ERROR "CSTUB_PATH not found: ${CSTUB_PATH}")
endif()

# ---- P6 TileManager (tile DMA API used by VP6 drivers) ----
set(TM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../try-slam31-RI/TileManager"
    CACHE PATH "TileManager root (inc + src)")
set(TM_INC "${TM_ROOT}/inc")
set(TM_SRC "${TM_ROOT}/src")
if(NOT EXISTS "${TM_INC}/tileManager.h")
  message(FATAL_ERROR "TileManager headers not found: ${TM_INC}")
endif()

if(MSVC)
  add_compile_options(/utf-8 /W3 /EHsc /O2)
  add_compile_definitions(NOMINMAX _CRT_SECURE_NO_WARNINGS)
endif()

# ---- DSP VP6 modules (compiled as C++ to match cstub IVP vector types) ----
set(DSP_SRC  "${CMAKE_CURRENT_SOURCE_DIR}/src/dsp")
set(DSP_INC  "${CMAKE_CURRENT_SOURCE_DIR}/include/dsp")

# cstub shared lib
file(GLOB CSTUB_SRC ${CSTUB_PATH}/*.cpp ${CSTUB_PATH}/*.h ${CSTUB_PATH}/xtensa/tie/*.h)
add_library(cstubs SHARED ${CSTUB_SRC})
target_include_directories(cstubs PUBLIC ${CSTUB_PATH})
set_target_properties(cstubs PROPERTIES DEFINE_SYMBOL CSTUB_DLLEXPORT)

# P6 TileManager
add_library(p6_tilemanager STATIC
    ${TM_SRC}/tileManager.c
    ${TM_SRC}/tmUtils.c
    ${TM_SRC}/idmaEmulate.c)
target_include_directories(p6_tilemanager PUBLIC ${TM_INC})
target_link_libraries(p6_tilemanager PUBLIC cstubs)
set_source_files_properties(
    ${TM_SRC}/tileManager.c ${TM_SRC}/tmUtils.c ${TM_SRC}/idmaEmulate.c
    PROPERTIES LANGUAGE CXX)

# ---- VP6 _core + _vp6 libraries (one per module) ----

# orb_blur: resize (bilinear downscale for pyramid)
add_library(orb_resize_core STATIC
    ${DSP_SRC}/orb_resize_core.c
    ${DSP_SRC}/orb_resize_vp6.c)
target_include_directories(orb_resize_core PUBLIC ${DSP_INC} ${TM_INC})
target_link_libraries(orb_resize_core PUBLIC p6_tilemanager)
set_source_files_properties(
    ${DSP_SRC}/orb_resize_core.c ${DSP_SRC}/orb_resize_vp6.c
    PROPERTIES LANGUAGE CXX)

# orb_blur: Gaussian blur (7x7 separable, sigma=2)
add_library(orb_blur_core STATIC
    ${DSP_SRC}/orb_blur_core.c
    ${DSP_SRC}/orb_blur_vp6.c)
target_include_directories(orb_blur_core PUBLIC ${DSP_INC} ${TM_INC})
target_link_libraries(orb_blur_core PUBLIC p6_tilemanager)
set_source_files_properties(
    ${DSP_SRC}/orb_blur_core.c ${DSP_SRC}/orb_blur_vp6.c
    PROPERTIES LANGUAGE CXX)

# fast9: FAST-9_16 detect + NMS
add_library(fast9_core STATIC
    ${DSP_SRC}/fast9_core.c
    ${DSP_SRC}/fast9_vp6.c)
target_include_directories(fast9_core PUBLIC ${DSP_INC} ${TM_INC})
target_link_libraries(fast9_core PUBLIC p6_tilemanager)
set_source_files_properties(
    ${DSP_SRC}/fast9_core.c ${DSP_SRC}/fast9_vp6.c
    PROPERTIES LANGUAGE CXX)

# ic_angle: IC_Angle gray-centroid orientation
add_library(ic_angle_core STATIC
    ${DSP_SRC}/ic_angle_core.c
    ${DSP_SRC}/ic_angle_vp6.c)
target_include_directories(ic_angle_core PUBLIC ${DSP_INC} ${TM_INC})
target_link_libraries(ic_angle_core PUBLIC p6_tilemanager)
set_source_files_properties(
    ${DSP_SRC}/ic_angle_core.c ${DSP_SRC}/ic_angle_vp6.c
    PROPERTIES LANGUAGE CXX)

# brief: rBRIEF 256-bit descriptor
add_library(brief_core STATIC
    ${DSP_SRC}/brief_core.c
    ${DSP_SRC}/brief_vp6.c)
target_include_directories(brief_core PUBLIC ${DSP_INC} ${TM_INC})
target_link_libraries(brief_core PUBLIC p6_tilemanager)
set_source_files_properties(
    ${DSP_SRC}/brief_core.c ${DSP_SRC}/brief_vp6.c
    PROPERTIES LANGUAGE CXX)

# match: Hamming distance + best-match (for future use; not yet wired)
add_library(match_core STATIC
    ${DSP_SRC}/match_core.c
    ${DSP_SRC}/match_vp6.c)
target_include_directories(match_core PUBLIC ${DSP_INC} ${TM_INC})
target_link_libraries(match_core PUBLIC p6_tilemanager)
set_source_files_properties(
    ${DSP_SRC}/match_core.c ${DSP_SRC}/match_vp6.c
    PROPERTIES LANGUAGE CXX)

# ---- DSP ORB wrapper (compiled as C; links VP6 _core modules) ----
add_library(dsp_orb STATIC src/dsp_orb.c)
target_include_directories(dsp_orb PUBLIC
    include include/ngd_app
    ${DSP_INC}
    ${NGD_CORE_INCLUDE})
target_link_libraries(dsp_orb PUBLIC
    orb_resize_core orb_blur_core fast9_core ic_angle_core brief_core)

# ---- app library (shell + YOLOv8-seg + ORB-wrappers) ----
add_library(ngd_app_v8segdsp STATIC
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
target_include_directories(ngd_app_v8segdsp PUBLIC
    include include/ngd_app
    ${DSP_INC}
    ${NGD_CORE_INCLUDE} ${NGD_SHELL_INCLUDE}
    src/yolov8seg)
target_link_libraries(ngd_app_v8segdsp PUBLIC
    ${OpenCV_LIBS}
    ${NGD_CORE_LIB_ABS}
    ${NGD_SHELL_LIB_ABS}
    dsp_orb)
if(MSVC)
  target_compile_options(ngd_app_v8segdsp PRIVATE /wd4244)
endif()

# ---- USE_DSP_ORB (default ON for this project) ----
option(USE_DSP_ORB "Use VP6/DSP ORB extractor instead of ngd_orb_extract" ON)
if(USE_DSP_ORB)
  target_compile_definitions(ngd_app_v8segdsp PRIVATE USE_DSP_ORB)
  message(STATUS "DSP ORB extractor: ENABLED (dsp_orb_extract → VP6 _full kernels)")
else()
  message(STATUS "DSP ORB extractor: DISABLED (using ngd_orb_extract from ngd_core)")
endif()

# ---- USE_DEMO_ORB (off by default; third comparison point) ----
option(USE_DEMO_ORB "Use demo_feature_extraction.c ORB extractor" OFF)
if(USE_DEMO_ORB)
  target_compile_definitions(ngd_app_v8segdsp PRIVATE USE_DEMO_ORB)
  message(STATUS "Demo ORB extractor: ENABLED (demo_feature_extraction.c)")
endif()

# ---- ORB comparison test ----
add_executable(test_orb_compare test/test_orb_compare.c src/demo_orb.c)
target_include_directories(test_orb_compare PRIVATE
    include include/ngd_app ${NGD_CORE_INCLUDE})
target_link_libraries(test_orb_compare PRIVATE ${NGD_CORE_LIB_ABS})
set_target_properties(test_orb_compare PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
if(MSVC)
  target_compile_options(test_orb_compare PRIVATE /wd4244)
endif()

# ---- DSP ORB comparison test: dsp_orb_extract vs ngd_orb_extract on synthetic image ----
add_executable(test_dsp_orb_compare test/test_dsp_orb.c)
target_include_directories(test_dsp_orb_compare PRIVATE
    include include/ngd_app ${DSP_INC} ${NGD_CORE_INCLUDE})
target_link_libraries(test_dsp_orb_compare PRIVATE dsp_orb ${NGD_CORE_LIB_ABS})
set_target_properties(test_dsp_orb_compare PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
if(MSVC)
  target_compile_options(test_dsp_orb_compare PRIVATE /wd4244)
endif()

# ---- rgbd_tum driver executable ----
add_executable(rgbd_tum_app_v8segdsp src/main.cpp)
target_link_libraries(rgbd_tum_app_v8segdsp PRIVATE ngd_app_v8segdsp)
set_target_properties(rgbd_tum_app_v8segdsp PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
if(MSVC)
  target_compile_options(rgbd_tum_app_v8segdsp PRIVATE /wd4244)
endif()
