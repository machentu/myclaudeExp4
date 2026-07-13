cmake_minimum_required(VERSION 3.15)
project(ngd_app LANGUAGES CXX C)

# ngd_app — driver layer for the refactor pure-C core (phase 1: static
# ORB-SLAM3 RGB-D equivalent). Links ngd_core + OpenCV (image IO + yaml).
# Configure with the vcpkg toolchain (same as refmain):
#   c:\Users\frank.tu\CMakePath.bat
#   set CMAKE=C:/src/vcpkg/downloads/tools/cmake-3.31.10-windows/cmake-3.31.10-windows-x86_64/bin/cmake.exe
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

# ---- refmain OpenCV shell (prebuilt ngd_shell.lib) — for phase 2 (OF / mask) ----
set(NGD_SHELL_INCLUDE "../refmain/include" CACHE PATH "refmain/include (ngd_shell headers)")
set(NGD_SHELL_LIB     "../refmain/build/Release/ngd_shell.lib" CACHE FILEPATH "prebuilt ngd_shell.lib")
get_filename_component(NGD_SHELL_LIB_ABS "${NGD_SHELL_LIB}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

if(MSVC)
  add_compile_options(/utf-8 /W3 /EHsc /O2)
  add_compile_definitions(NOMINMAX _CRT_SECURE_NO_WARNINGS)
endif()

# ---- shell lib: imio (image+depth IO) + yaml (settings) + system (driver)
#      + optflow_track (TrackWithOpticalFlow orchestration, calls ngd_shell) ----
add_library(ngd_app STATIC
    src/imio.cpp
    src/yaml.cpp
    src/system.cpp
    src/optflow_track.cpp
    src/viewer.cpp
)
target_include_directories(ngd_app PUBLIC include include/ngd_app ${NGD_CORE_INCLUDE} ${NGD_SHELL_INCLUDE})
target_link_libraries(ngd_app PUBLIC ${OpenCV_LIBS} ${NGD_CORE_LIB_ABS} ${NGD_SHELL_LIB_ABS})
if(MSVC)
  target_compile_options(ngd_app PRIVATE /wd4244)
endif()

# ---- tests ----
enable_testing()

add_executable(test_shell tests/test_shell.cpp)
target_link_libraries(test_shell PRIVATE ngd_app)
set_target_properties(test_shell PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
add_test(NAME test_shell COMMAND test_shell)

add_executable(test_optflow_track tests/test_optflow_track.cpp)
target_link_libraries(test_optflow_track PRIVATE ngd_app)
set_target_properties(test_optflow_track PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
if(MSVC)
  target_compile_options(test_optflow_track PRIVATE /wd4244)
endif()
add_test(NAME test_optflow_track COMMAND test_optflow_track)

# ---- rgbd_tum-equivalent driver (phase 1, static ORB-SLAM3 RGB-D) ----
# Usage: rgbd_tum_app <vocab> <settings> <sequence> <association>
add_executable(rgbd_tum_app src/main.cpp)
target_link_libraries(rgbd_tum_app PRIVATE ngd_app)
set_target_properties(rgbd_tum_app PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
if(MSVC)
  target_compile_options(rgbd_tum_app PRIVATE /wd4244)
endif()
