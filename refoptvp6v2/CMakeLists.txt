# Build & run the VP6 tile-based ORB operators end-to-end tests via cstub on PC.
# Mirrors vp6ref/vp6/CMakeLists.txt: compiles all C sources as C++ (the cstub
# TIE stubs are C++ and the IVP vector types only match in C++ mode), 32-bit
# (cstub is 32-bit only).
#
#   cmake -S refoptvp6v2 -B refoptvp6v2/build -G "Visual Studio 17 2022" -A Win32
#   cmake --build refoptvp6v2/build --config Release
#   refoptvp6v2\build\Release\test_orb_blur_vp6.exe
cmake_minimum_required(VERSION 3.15)
project(refoptvp6v2_cstub CXX)

set(CSTUB_PATH "C:/usr/XtDevTools/install/builds/RI-2022.10-win32/fy01_202210/src/cstub"
    CACHE PATH "cstub source dir")
if(NOT EXISTS "${CSTUB_PATH}")
    message(FATAL_ERROR "CSTUB_PATH not found: ${CSTUB_PATH}")
endif()

set(REFOPT "${CMAKE_CURRENT_SOURCE_DIR}")
set(TM_INC "${REFOPT}/../sgbmwarpFace/WarpRGBA/TileManager_P6/TileManager/inc")
set(TM_SRC "${REFOPT}/../sgbmwarpFace/WarpRGBA/TileManager_P6/TileManager/src")
if(NOT EXISTS "${TM_INC}/tileManager.h")
    message(FATAL_ERROR "TileManager headers not found: ${TM_INC}")
endif()

# cstub shared lib (TIE intrinsic stubs). Glob .cpp like the repo CMakeLists.
file(GLOB CSTUB_SRC ${CSTUB_PATH}/*.cpp ${CSTUB_PATH}/*.h ${CSTUB_PATH}/xtensa/tie/*.h)
add_library(cstubs SHARED ${CSTUB_SRC})
target_include_directories(cstubs PUBLIC ${CSTUB_PATH})
set_target_properties(cstubs PROPERTIES DEFINE_SYMBOL CSTUB_DLLEXPORT)

# P6 TileManager (compiled as C++ to match the cstub IVP types).
add_library(p6_tilemanager STATIC
    ${TM_SRC}/tileManager.c
    ${TM_SRC}/tmUtils.c
    ${TM_SRC}/idmaEmulate.c)
target_include_directories(p6_tilemanager PUBLIC ${TM_INC})
target_link_libraries(p6_tilemanager PUBLIC cstubs)
set_source_files_properties(
    ${TM_SRC}/tileManager.c ${TM_SRC}/tmUtils.c ${TM_SRC}/idmaEmulate.c
    PROPERTIES LANGUAGE CXX)

# ORB blur portable core + VP6 tile driver (as C++).
add_library(orb_blur_core STATIC
    ${REFOPT}/src/orb_blur_core.c
    ${REFOPT}/src/orb_blur_vp6.c)
target_include_directories(orb_blur_core PUBLIC ${REFOPT}/include ${TM_INC})
target_link_libraries(orb_blur_core PUBLIC p6_tilemanager)
set_source_files_properties(
    ${REFOPT}/src/orb_blur_core.c ${REFOPT}/src/orb_blur_vp6.c
    PROPERTIES LANGUAGE CXX)

# End-to-end test.
add_executable(test_orb_blur_vp6 ${REFOPT}/test/test_orb_blur_vp6.c)
set_source_files_properties(${REFOPT}/test/test_orb_blur_vp6.c PROPERTIES LANGUAGE CXX)
target_link_libraries(test_orb_blur_vp6 PRIVATE orb_blur_core)
if(MSVC)
    target_compile_options(test_orb_blur_vp6 PRIVATE /wd4244 /wd4267 /wd4311 /wd4312)
endif()

# FAST-9 detect + NMS portable core + VP6 tile driver (as C++).
add_library(fast9_core STATIC
    ${REFOPT}/src/fast9_core.c
    ${REFOPT}/src/fast9_vp6.c)
target_include_directories(fast9_core PUBLIC ${REFOPT}/include ${TM_INC})
target_link_libraries(fast9_core PUBLIC p6_tilemanager)
set_source_files_properties(
    ${REFOPT}/src/fast9_core.c ${REFOPT}/src/fast9_vp6.c
    PROPERTIES LANGUAGE CXX)

# FAST-9 end-to-end test (3-way: verbatim refactor ref vs fast9_full vs fast9_vp6).
add_executable(test_fast9_vp6 ${REFOPT}/test/test_fast9_vp6.c)
set_source_files_properties(${REFOPT}/test/test_fast9_vp6.c PROPERTIES LANGUAGE CXX)
target_link_libraries(test_fast9_vp6 PRIVATE fast9_core)
if(MSVC)
    target_compile_options(test_fast9_vp6 PRIVATE /wd4244 /wd4267 /wd4311 /wd4312)
endif()

# IC angle portable core + VP6 per-keypoint driver (as C++).
add_library(ic_angle_core STATIC
    ${REFOPT}/src/ic_angle_core.c
    ${REFOPT}/src/ic_angle_vp6.c)
target_include_directories(ic_angle_core PUBLIC ${REFOPT}/include ${TM_INC})
target_link_libraries(ic_angle_core PUBLIC p6_tilemanager)
set_source_files_properties(
    ${REFOPT}/src/ic_angle_core.c ${REFOPT}/src/ic_angle_vp6.c
    PROPERTIES LANGUAGE CXX)

# IC angle end-to-end test (3-way: verbatim refactor ref vs ic_angle_full vs ic_angle_vp6).
add_executable(test_ic_angle_vp6 ${REFOPT}/test/test_ic_angle_vp6.c)
set_source_files_properties(${REFOPT}/test/test_ic_angle_vp6.c PROPERTIES LANGUAGE CXX)
target_link_libraries(test_ic_angle_vp6 PRIVATE ic_angle_core)
if(MSVC)
    target_compile_options(test_ic_angle_vp6 PRIVATE /wd4244 /wd4267 /wd4311 /wd4312)
endif()

# rBRIEF descriptor portable core + VP6 per-keypoint driver (as C++).
add_library(brief_core STATIC
    ${REFOPT}/src/brief_core.c
    ${REFOPT}/src/brief_vp6.c)
target_include_directories(brief_core PUBLIC ${REFOPT}/include ${TM_INC})
target_link_libraries(brief_core PUBLIC p6_tilemanager)
set_source_files_properties(
    ${REFOPT}/src/brief_core.c ${REFOPT}/src/brief_vp6.c
    PROPERTIES LANGUAGE CXX)

# rBRIEF end-to-end test (3-way: verbatim refactor ref vs brief_full vs brief_vp6).
add_executable(test_brief_vp6 ${REFOPT}/test/test_brief_vp6.c)
set_source_files_properties(${REFOPT}/test/test_brief_vp6.c PROPERTIES LANGUAGE CXX)
target_link_libraries(test_brief_vp6 PRIVATE brief_core)
if(MSVC)
    target_compile_options(test_brief_vp6 PRIVATE /wd4244 /wd4267 /wd4311 /wd4312)
endif()

# Descriptor matching portable core + VP6 batch driver (as C++).
add_library(match_core STATIC
    ${REFOPT}/src/match_core.c
    ${REFOPT}/src/match_vp6.c)
target_include_directories(match_core PUBLIC ${REFOPT}/include ${TM_INC})
target_link_libraries(match_core PUBLIC p6_tilemanager)
set_source_files_properties(
    ${REFOPT}/src/match_core.c ${REFOPT}/src/match_vp6.c
    PROPERTIES LANGUAGE CXX)

# Descriptor matching end-to-end test (Hamming + 3-way best-match).
add_executable(test_match_vp6 ${REFOPT}/test/test_match_vp6.c)
set_source_files_properties(${REFOPT}/test/test_match_vp6.c PROPERTIES LANGUAGE CXX)
target_link_libraries(test_match_vp6 PRIVATE match_core)
if(MSVC)
    target_compile_options(test_match_vp6 PRIVATE /wd4244 /wd4267 /wd4311 /wd4312)
endif()

# Bilinear resize portable core + VP6 tile driver (as C++).
add_library(orb_resize_core STATIC
    ${REFOPT}/src/orb_resize_core.c
    ${REFOPT}/src/orb_resize_vp6.c)
target_include_directories(orb_resize_core PUBLIC ${REFOPT}/include ${TM_INC})
target_link_libraries(orb_resize_core PUBLIC p6_tilemanager)
set_source_files_properties(
    ${REFOPT}/src/orb_resize_core.c ${REFOPT}/src/orb_resize_vp6.c
    PROPERTIES LANGUAGE CXX)

# Bilinear resize end-to-end test (VP6 tile resize vs full reference).
add_executable(test_orb_resize_vp6 ${REFOPT}/test/test_orb_resize_vp6.c)
set_source_files_properties(${REFOPT}/test/test_orb_resize_vp6.c PROPERTIES LANGUAGE CXX)
target_link_libraries(test_orb_resize_vp6 PRIVATE orb_resize_core)
if(MSVC)
    target_compile_options(test_orb_resize_vp6 PRIVATE /wd4244 /wd4267 /wd4311 /wd4312)
endif()
