# refappv8segdsp2 — refappv8segdsp with accelerated ORB DSP kernels (Win32 build).
#
# Same pipeline as refappv8segdsp (VP6 _full kernels + quadtree), with the
# per-pixel kernels optimized using the algorithmic techniques from
# try-slam31-RI/xsTileLib/slam/src (fast9NMS.c shared-core min/max tree,
# score-map NMS, sliding-window blur, in-frame fast paths for IC_Angle /
# rBRIEF) while staying BIT-EXACT with refappv8segdsp (test_bitexact_ab
# compares old vs new dsp_orb_extract byte-for-byte; the trajectory must be
# byte-identical too).
#
# Macro switch: USE_DSP_ORB=ON  → dsp_orb_extract()  (VP6 kernels, optimized)
#               USE_DSP_ORB=OFF → ngd_orb_extract()  (refactor, original)
#               USE_DEMO_ORB=ON → demo_orb_extract() (demo single-scale)
#
# Configure:
#   cmake -S refappv8segdsp2 -B refappv8segdsp2/build -G "Visual Studio 17 2022" -A Win32 ^
#       -DOpenCV_DIR=C:/Users/frank.tu/Downloads/opencv-4.8.0/myBuild32/install
#   cmake --build refappv8segdsp2/build --config Release
cmake_minimum_required(VERSION 3.15)
project(ngd_app_v8segdsp2 LANGUAGES CXX C)

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

# ORB app-side TileManager init: 2x128KB static bank pools (InitTileMgr
# pattern, .dram0/.dram1 sections on Xtensa) + one iDMA object; used by the
# four *_vp6 tests and mirrored DSP-side by the HelloWorld integration.
add_library(orb_tm STATIC ${DSP_SRC}/orb_tile_manager.c)
target_include_directories(orb_tm PUBLIC ${DSP_INC} ${TM_INC})
target_link_libraries(orb_tm PUBLIC p6_tilemanager)
set_source_files_properties(${DSP_SRC}/orb_tile_manager.c PROPERTIES LANGUAGE CXX)

# ---- VP6 _core + _vp6 libraries (one per module) ----

# orb_blur: resize (bilinear downscale for pyramid) - orb_resize_core.c is
# the shared golden/bit-exact table builder; orb_resize_vp6.c is the legacy
# tile driver; orb_resize_strip_vp6.c is the IVP strip driver (gather+MAC
# horizontal, byte-plane vertical, bank-1 strip sweep) that replaces
# orb_resize_full inside dsp_orb_vp6_extract
add_library(orb_resize_core STATIC
    ${DSP_SRC}/orb_resize_core.c
    ${DSP_SRC}/orb_resize_vp6.c
    ${DSP_SRC}/orb_resize_strip_vp6.c)
target_include_directories(orb_resize_core PUBLIC ${DSP_INC} ${TM_INC} ${CSTUB_PATH})
target_link_libraries(orb_resize_core PUBLIC p6_tilemanager)
set_source_files_properties(
    ${DSP_SRC}/orb_resize_core.c ${DSP_SRC}/orb_resize_vp6.c
    ${DSP_SRC}/orb_resize_strip_vp6.c
    PROPERTIES LANGUAGE CXX)

# orb_blur: Gaussian blur (7x7 separable, sigma=2) - the vp6 driver carries
# the IVP pixel kernel (LA/LAV streams + 24-lane MAC + byte-plane vertical)
add_library(orb_blur_core STATIC
    ${DSP_SRC}/orb_blur_core.c
    ${DSP_SRC}/orb_blur_vp6.c)
target_include_directories(orb_blur_core PUBLIC ${DSP_INC} ${TM_INC} ${CSTUB_PATH})
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

# fast9 score map: single-scan threshold-independent score map + cached NMS
# (optimized restructure of the per-cell fast9_full calls; bit-exact with
# fast9_detect_kernel — see fast9_scoremap.c header for the equivalence proof)
add_library(fast9_scoremap STATIC
    ${DSP_SRC}/fast9_scoremap.c)
target_include_directories(fast9_scoremap PUBLIC ${DSP_INC})
set_source_files_properties(
    ${DSP_SRC}/fast9_scoremap.c
    PROPERTIES LANGUAGE CXX)

# fast9 score map VP6+IVP: strip ping-pong DMA driver + 64-lane IVP pixel
# kernel (separate lib so dsp_orb/the app keep the pure-C link graph; the
# vp6 path is exercised by test_fast9_scoremap_vp6)
add_library(fast9_scoremap_vp6 STATIC
    ${DSP_SRC}/fast9_scoremap_vp6.c)
target_include_directories(fast9_scoremap_vp6 PUBLIC ${DSP_INC} ${TM_INC} ${CSTUB_PATH})
target_link_libraries(fast9_scoremap_vp6 PUBLIC p6_tilemanager)
set_source_files_properties(
    ${DSP_SRC}/fast9_scoremap_vp6.c
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
    ${DSP_SRC}/brief_vp6.c
    ${DSP_SRC}/brief_int_vp6.c)
target_include_directories(brief_core PUBLIC ${DSP_INC} ${TM_INC})
target_link_libraries(brief_core PUBLIC p6_tilemanager)
set_source_files_properties(
    ${DSP_SRC}/brief_core.c ${DSP_SRC}/brief_vp6.c ${DSP_SRC}/brief_int_vp6.c
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

# ---- quadtree spatial distribution (extracted from dsp_orb.c verbatim;
# shared by the pure-C and VP6+IVP orchestrators so both distribute
# identical keypoint sets identically) ----
add_library(dsp_orb_octtree STATIC ${DSP_SRC}/dsp_orb_octtree.c)
target_include_directories(dsp_orb_octtree PUBLIC
    include include/ngd_app ${NGD_CORE_INCLUDE})

# ---- DSP ORB wrapper (compiled as C; links VP6 _core modules) ----
add_library(dsp_orb STATIC src/dsp_orb.c)
target_include_directories(dsp_orb PUBLIC
    include include/ngd_app
    ${DSP_INC}
    ${NGD_CORE_INCLUDE})
target_link_libraries(dsp_orb PUBLIC
    orb_resize_core orb_blur_core fast9_core fast9_scoremap ic_angle_core brief_core
    dsp_orb_octtree)

# ---- DSP ORB VP6+IVP orchestrator: dsp_orb_extract's pipeline over the five
# persistent bank-pool/raw-iDMA drivers (orb_resize_strip_vp6 with pure-C
# fallback, fast9_scoremap_vp6, orb_blur_vp6, ic_angle_vp6, brief_vp6);
# bit-exact with dsp_orb_extract (test_dsp_orb_vp6 memcmps both) ----
add_library(dsp_orb_vp6 STATIC ${DSP_SRC}/dsp_orb_vp6.c)
target_include_directories(dsp_orb_vp6 PUBLIC
    include include/ngd_app
    ${DSP_INC} ${NGD_CORE_INCLUDE} ${TM_INC})
target_link_libraries(dsp_orb_vp6 PUBLIC
    dsp_orb_octtree orb_resize_core fast9_scoremap fast9_scoremap_vp6
    orb_blur_core ic_angle_core brief_core orb_tm)
# Per-stage profiling: CCOUNT on Xtensa (cycles), QPC on host (us, stage mix
# only). The Linux DSP integration compiles the same file WITHOUT this define
# unless profiling is wanted (zero cost when off).
if(MSVC)
  target_compile_definitions(dsp_orb_vp6 PRIVATE DSP_ORB_VP6_TIMING)
endif()
# Fixed-point LUT rBRIEF (brief_int_vp6.c): opt-in, NOT bit-exact with the
# pure-C path (descriptors differ by a few bits/kp; see test_brief_int_vp6).
# The Linux DSP build passes the same define for a hardware cycle round.
option(BRIEF_INT_VP6 "Use the fixed-point LUT rBRIEF kernel in dsp_orb_vp6" OFF)
if(BRIEF_INT_VP6)
  target_compile_definitions(dsp_orb_vp6 PRIVATE BRIEF_INT_VP6)
endif()

# ---- app library (shell + YOLOv8-seg + ORB-wrappers) ----
add_library(ngd_app_v8segdsp2 STATIC
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
# ngd_core.lib is built with NGD_BOW=1 (refactor option, default ON); the gate
# in ngd/bow.h etc. must match, and the lib is linked by path so the PUBLIC
# definition does not propagate — define it here.
target_compile_definitions(ngd_app_v8segdsp2 PUBLIC NGD_BOW=1)
target_include_directories(ngd_app_v8segdsp2 PUBLIC
    include include/ngd_app
    ${DSP_INC}
    ${NGD_CORE_INCLUDE} ${NGD_SHELL_INCLUDE}
    src/yolov8seg)
target_link_libraries(ngd_app_v8segdsp2 PUBLIC
    ${OpenCV_LIBS}
    ${NGD_CORE_LIB_ABS}
    ${NGD_SHELL_LIB_ABS}
    dsp_orb)
if(MSVC)
  target_compile_options(ngd_app_v8segdsp2 PRIVATE /wd4244)
endif()

# ---- USE_DSP_ORB (default ON for this project) ----
option(USE_DSP_ORB "Use VP6/DSP ORB extractor instead of ngd_orb_extract" ON)
if(USE_DSP_ORB)
  target_compile_definitions(ngd_app_v8segdsp2 PRIVATE USE_DSP_ORB)
  message(STATUS "DSP ORB extractor: ENABLED (dsp_orb_extract → VP6 _full kernels)")
else()
  message(STATUS "DSP ORB extractor: DISABLED (using ngd_orb_extract from ngd_core)")
endif()

# ---- VP6_EXTRACT: route the app through dsp_orb_vp6_extract (tile-manager
#      VP6+IVP drivers; cstub emulation on host). Combine with BRIEF_INT_VP6
#      for the approximate fixed-point rBRIEF end-to-end ATE check. ----
option(VP6_EXTRACT "App uses dsp_orb_vp6_extract (VP6+IVP drivers)" OFF)
if(VP6_EXTRACT)
  target_compile_definitions(ngd_app_v8segdsp2 PRIVATE VP6_EXTRACT)
  target_include_directories(ngd_app_v8segdsp2 PUBLIC ${TM_INC})
  target_link_libraries(ngd_app_v8segdsp2 PUBLIC dsp_orb_vp6 orb_tm)
  message(STATUS "App ORB path: dsp_orb_vp6_extract (VP6+IVP drivers)")
endif()

# ---- headless benchmark mode (no OpenCV viewer windows) ----
option(NGD_APP_VIEWER "Enable the trajectory/mask viewer windows" ON)
if(NOT NGD_APP_VIEWER)
  target_compile_definitions(ngd_app_v8segdsp2 PRIVATE NGD_VIEWER=0)
  message(STATUS "App viewer: OFF (headless benchmark)")
endif()

# ---- USE_DEMO_ORB (off by default; third comparison point) ----
option(USE_DEMO_ORB "Use demo_feature_extraction.c ORB extractor" OFF)
if(USE_DEMO_ORB)
  target_compile_definitions(ngd_app_v8segdsp2 PRIVATE USE_DEMO_ORB)
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

# ---- bit-exact A/B test: optimized dsp_orb_extract vs refappv8segdsp legacy ----
# The legacy copy (test/ref_legacy/dsp_orb_legacy.c) is the refappv8segdsp
# orchestration calling the unmodified *_core kernels; both run on the same
# synthetic images and the keypoint/descriptor outputs are compared
# byte-for-byte. Any optimization that changes an output byte fails here.
add_executable(test_bitexact_ab
    test/test_bitexact_ab.c
    test/ref_legacy/dsp_orb_legacy.c)
target_include_directories(test_bitexact_ab PRIVATE
    include include/ngd_app ${DSP_INC} ${NGD_CORE_INCLUDE})
target_link_libraries(test_bitexact_ab PRIVATE
    dsp_orb orb_resize_core orb_blur_core fast9_core ic_angle_core brief_core
    ${NGD_CORE_LIB_ABS})
set_target_properties(test_bitexact_ab PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
if(MSVC)
  target_compile_options(test_bitexact_ab PRIVATE /wd4244)
endif()

# ---- bit-exact test: dsp_orb_vp6_extract (VP6+IVP drivers) vs dsp_orb_extract
# (pure-C kernels) on synthetic images — keypoints AND descriptors memcmp-identical
add_executable(test_dsp_orb_vp6 test/test_dsp_orb_vp6.c)
target_include_directories(test_dsp_orb_vp6 PRIVATE
    include include/ngd_app ${DSP_INC} ${NGD_CORE_INCLUDE})
target_link_libraries(test_dsp_orb_vp6 PRIVATE
    dsp_orb dsp_orb_vp6 orb_tm ${NGD_CORE_LIB_ABS})
set_target_properties(test_dsp_orb_vp6 PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
if(MSVC)
  target_compile_options(test_dsp_orb_vp6 PRIVATE /wd4244)
endif()

# ---- IVP intrinsic semantics probe (cstub): documents every lane/rounding
# assumption the VP6+IVP kernels rest on; run after any cstub/TIE change ----
add_executable(test_ivp_probe test/test_ivp_probe.cpp)
target_include_directories(test_ivp_probe PRIVATE ${CSTUB_PATH})
target_link_libraries(test_ivp_probe PRIVATE cstubs)
set_target_properties(test_ivp_probe PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# ---- VP6+IVP FAST-9 score map: 3-way golden vs strip/IVP driver ----
add_executable(test_fast9_scoremap_vp6 test/test_fast9_scoremap_vp6.c)
set_source_files_properties(test/test_fast9_scoremap_vp6.c PROPERTIES LANGUAGE CXX)
target_include_directories(test_fast9_scoremap_vp6 PRIVATE ${DSP_INC})
target_link_libraries(test_fast9_scoremap_vp6 PRIVATE fast9_scoremap fast9_scoremap_vp6 orb_tm)
set_target_properties(test_fast9_scoremap_vp6 PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
if(MSVC)
    target_compile_options(test_fast9_scoremap_vp6 PRIVATE /wd4244 /wd4267)
endif()

# ---- VP6+IVP ORB blur: tile driver with IVP kernel vs orb_blur_full ----
add_executable(test_orb_blur_vp6 test/test_orb_blur_vp6.c)
set_source_files_properties(test/test_orb_blur_vp6.c PROPERTIES LANGUAGE CXX)
target_include_directories(test_orb_blur_vp6 PRIVATE ${DSP_INC})
target_link_libraries(test_orb_blur_vp6 PRIVATE orb_blur_core orb_tm)
set_target_properties(test_orb_blur_vp6 PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
if(MSVC)
    target_compile_options(test_orb_blur_vp6 PRIVATE /wd4244 /wd4267)
endif()

# ---- VP6+IVP resize strip driver vs orb_resize_full (bit-exact) ----
add_executable(test_orb_resize_vp6 test/test_orb_resize_vp6.c)
set_source_files_properties(test/test_orb_resize_vp6.c PROPERTIES LANGUAGE CXX)
target_include_directories(test_orb_resize_vp6 PRIVATE ${DSP_INC})
target_link_libraries(test_orb_resize_vp6 PRIVATE orb_resize_core orb_tm)
set_target_properties(test_orb_resize_vp6 PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
if(MSVC)
    target_compile_options(test_orb_resize_vp6 PRIVATE /wd4244 /wd4267)
endif()

# ---- VP6+IVP rBRIEF: per-keypoint patch driver with IVP gather kernel ----
add_executable(test_brief_vp6 test/test_brief_vp6.c)
set_source_files_properties(test/test_brief_vp6.c PROPERTIES LANGUAGE CXX)
target_include_directories(test_brief_vp6 PRIVATE ${DSP_INC})
target_link_libraries(test_brief_vp6 PRIVATE brief_core orb_tm)
set_target_properties(test_brief_vp6 PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# fixed-point LUT rBRIEF variant (vector==scalar bit-exact + hamming gates)
add_executable(test_brief_int_vp6 test/test_brief_int_vp6.c)
set_source_files_properties(test/test_brief_int_vp6.c PROPERTIES LANGUAGE CXX)
target_include_directories(test_brief_int_vp6 PRIVATE ${DSP_INC})
target_link_libraries(test_brief_int_vp6 PRIVATE brief_core orb_tm)
set_target_properties(test_brief_int_vp6 PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
if(MSVC)
    target_compile_options(test_brief_vp6 PRIVATE /wd4244 /wd4267)
    target_compile_options(test_brief_int_vp6 PRIVATE /wd4244 /wd4267)
endif()

# ---- VP6+IVP IC angle: per-keypoint patch driver with IVP gather kernel ----
add_executable(test_ic_angle_vp6 test/test_ic_angle_vp6.c)
set_source_files_properties(test/test_ic_angle_vp6.c PROPERTIES LANGUAGE CXX)
target_include_directories(test_ic_angle_vp6 PRIVATE ${DSP_INC})
target_link_libraries(test_ic_angle_vp6 PRIVATE ic_angle_core orb_tm)
set_target_properties(test_ic_angle_vp6 PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
if(MSVC)
    target_compile_options(test_ic_angle_vp6 PRIVATE /wd4244 /wd4267)
endif()

# ---- rgbd_tum driver executable ----
add_executable(rgbd_tum_app_v8segdsp2 src/main.cpp)
target_link_libraries(rgbd_tum_app_v8segdsp2 PRIVATE ngd_app_v8segdsp2)
set_target_properties(rgbd_tum_app_v8segdsp2 PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
if(MSVC)
  target_compile_options(rgbd_tum_app_v8segdsp2 PRIVATE /wd4244)
endif()
