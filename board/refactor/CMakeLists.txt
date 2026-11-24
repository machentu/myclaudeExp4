cmake_minimum_required(VERSION 3.10)
project(ngd_core C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# ---- Warnings (cross MSVC / GCC) ----
if(MSVC)
    add_compile_options(/W4 /permissive- /wd4996)
    # Use a sane floating-point model so LM converges predictably
    add_compile_options(/fp:precise)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ---- BoW (Bag-of-Words) + loop closing ----
option(NGD_BOW "Enable Bag-of-Words vocabulary + loop closing" OFF)
message(STATUS "NGD_BOW: ${NGD_BOW}")

# ---- Static core library (pure C, no external deps) ----
set(NGD_CORE_SRC
    src/math.c
    src/so3.c
    src/se3.c
    src/pinhole.c
    src/pose_opt.c
    src/orb.c
    src/calib.c
    src/matcher.c
    src/mappoint.c
    src/keyframe.c
    src/frame.c
    src/tracking.c
    src/pnp.c
    src/map.c
    src/localmapping.c
    src/ba.c
    src/ba_sparse.c
    # mixed-precision LBA solver (NGD_BA_F32, default 1 via ngd/ba_f32.h):
    # lives next to the app-side ngd_opt_* kernels; compiled here so the
    # ngd_core tests link whenever ba.c's LBA site references it
    ../refappv8seg_full/src/ngd_opt_lmf32.c
    src/mask.c
    src/mlpnp.c
    src/optflow.c
)
if(NGD_BOW)
    list(APPEND NGD_CORE_SRC
        src/bow.c
        src/kfdb.c
        src/sim3.c
        src/sim3solver.c
        src/sim3opt.c
        src/essgraph.c
        src/loopclosing.c
    )
endif()

add_library(ngd_core STATIC ${NGD_CORE_SRC})
target_include_directories(ngd_core PUBLIC include)
target_compile_definitions(ngd_core PUBLIC "NGD_BOW=$<BOOL:${NGD_BOW}>")

if(NOT MSVC)
    target_link_libraries(ngd_core PUBLIC m)
endif()

# ---- Tests (no external framework: plain asserts) ----
enable_testing()

set(NGD_TESTS
    test_math
    test_se3
    test_pose_opt
    test_orb
    test_frame
    test_stereo_init
    test_frustum
    test_projmatch
    test_motion
    #test_localmap
    test_pnp
    test_map
    test_mappointculling
    test_needkf
    test_ba
    test_ba_sparse
    test_fuse
    test_mask
    test_mlpnp
    test_framemask
    test_optflow
)
if(NGD_BOW)
    list(APPEND NGD_TESTS
        test_bow
        test_searchbow
        test_trackrefkf
        test_score
        test_kfdb
        test_reloc
        test_sim3
        test_sim3solver
        test_sim3opt
        test_searchbysim3
        test_essgraph
        test_loopclosing
        test_kfinsert
        test_createmps
        test_keyframeculling
    )
endif()

foreach(t ${NGD_TESTS})
    add_executable(${t} tests/${t}.c)
    target_link_libraries(${t} PRIVATE ngd_core)
    add_test(NAME ${t} COMMAND ${t})
endforeach()

# ---- Replay tests (dump-and-diff vs original golden): link ngd_core, not part of ctest ----
# Build: cmake --build myBuild --config Release --target replay_pose_opt
# Run:   replay_pose_opt <golden_poseopt.txt> [fid...]
add_executable(replay_pose_opt replay/replay_pose_opt.c)
target_link_libraries(replay_pose_opt PRIVATE ngd_core)

# ---- BA solver benchmark (dense vs block-sparse), manual target ----
# Build: cmake --build myBuild --config Release --target bench_ba_sparse
# Run:   ./Release/bench_ba_sparse [configs...]  (no args = all)
add_executable(bench_ba_sparse tests/bench_ba_sparse.c)
target_link_libraries(bench_ba_sparse PRIVATE ngd_core)
