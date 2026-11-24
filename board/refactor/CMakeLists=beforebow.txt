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

# ---- Static core library (pure C, no external deps) ----
add_library(ngd_core STATIC
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
    src/bow.c
    src/kfdb.c
    src/pnp.c
    src/map.c
    src/localmapping.c
    src/ba.c
    src/mask.c
    src/mlpnp.c
    src/sim3.c
    src/sim3solver.c
    src/sim3opt.c
    src/essgraph.c
    src/loopclosing.c
    src/optflow.c
)
target_include_directories(ngd_core PUBLIC include)

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
    test_localmap
    test_bow
    test_searchbow
    test_trackrefkf
    test_score
    test_kfdb
    test_pnp
    test_reloc
    test_map
    test_mappointculling
    test_needkf
    test_kfinsert
    test_ba
    test_createmps
    test_fuse
    test_keyframeculling
    test_mask
    test_mlpnp
    test_sim3
    test_sim3solver
    test_sim3opt
    test_searchbysim3
    test_essgraph
    test_loopclosing
    test_framemask
    test_optflow
)

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
