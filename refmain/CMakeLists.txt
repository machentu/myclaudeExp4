cmake_minimum_required(VERSION 3.15)
project(ngd_shell LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# ---- OpenCV (vcpkg). Configure with the vcpkg toolchain so dnn's transitive
# Protobuf dependency resolves too:
#   cmake -B build -G "Visual Studio 17 2022" -A x64 \
#         -DCMAKE_TOOLCHAIN_FILE=C:/src/vcpkg/scripts/buildsystems/vcpkg.cmake
# (-DOpenCV_DIR alone is NOT enough — it misses Protobuf, breaking the dnn module.)
find_package(OpenCV REQUIRED)
message(STATUS "OpenCV ${OpenCV_VERSION} found")

# ---- refactor pure-C core binding (ngd_core) ----
# Only the demos that drive the core's mask chain (test_lk_shell) need to LINK
# ngd_core; the shell modules themselves only include ngd/*.h headers.
set(NGD_CORE_INCLUDE "../refactor/include" CACHE PATH "refactor/include (ngd headers)")
set(NGD_CORE_LIB     "../refactor/myBuild/Release/ngd_core.lib" CACHE FILEPATH "prebuilt ngd_core.lib")

if(MSVC)
    add_compile_options(/utf-8 /W3 /EHsc /O2)
    add_compile_definitions(NOMINMAX _CRT_SECURE_NO_WARNINGS)
endif()

# Resolve ngd_core.lib to an ABSOLUTE path: MSVC mangles a relative path passed
# to target_link_libraries (appends .lib again -> ngd_core.lib.lib).
get_filename_component(NGD_CORE_LIB_ABS "${NGD_CORE_LIB}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# ---- ngd_shell: self-contained OpenCV shell ----
# The YOLO detector + LK/DBSCAN mask-prediction chain live here as local copies
# (include/ngd_shell/{YoloDetector,MaskPredictor,MaskTracker}.h, src/*.cpp),
# originally stripped from NGD-SLAM. refmain adds the optical-flow SLAM tracking
# (SearchByOpticalFlow + PnP, src/optflow.cpp) and the ngd_lk_flow_fn callback
# (src/lk_shell.cpp). No dependency on the sibling MaskTracker project.
add_library(ngd_shell STATIC
    src/lk_shell.cpp
    src/optflow.cpp
    src/YoloDetector.cpp
    src/MaskPredictor.cpp
    src/MaskTracker.cpp
)
target_include_directories(ngd_shell PUBLIC include include/ngd_shell ${NGD_CORE_INCLUDE})
target_link_libraries(ngd_shell PUBLIC ${OpenCV_LIBS})
if(MSVC)
    # C4244 (int<->float) fires in the MaskTracker-origin sources (benign casts
    # in MaskPredictor.cpp). Suppress target-wide; the refmain-authored shell
    # sources (lk_shell/optflow) are warning-clean.
    target_compile_options(ngd_shell PRIVATE /wd4244)
endif()

# ---- demos (plain assert, no framework) ----
enable_testing()

add_executable(test_lk_shell tests/test_lk_shell.cpp)
target_link_libraries(test_lk_shell PRIVATE ngd_shell ${NGD_CORE_LIB_ABS})
if(MSVC)
    target_compile_options(test_lk_shell PRIVATE /wd4244)
endif()
set_target_properties(test_lk_shell PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
add_test(NAME test_lk_shell COMMAND test_lk_shell)

add_executable(test_optflow tests/test_optflow.cpp)
target_link_libraries(test_optflow PRIVATE ngd_shell)
set_target_properties(test_optflow PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
add_test(NAME test_optflow COMMAND test_optflow)

add_executable(test_yolo_mask tests/test_yolo_mask.cpp)
target_link_libraries(test_yolo_mask PRIVATE ngd_shell)
set_target_properties(test_yolo_mask PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
add_test(NAME test_yolo_mask COMMAND test_yolo_mask)
