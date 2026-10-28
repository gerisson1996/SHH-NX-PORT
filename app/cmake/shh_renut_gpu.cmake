# shh-nx (2026-10-28): Renut's Direct3D-level renderer (docs/plano-renut.md, stage 2). Adapted from sa2-nx's
# app/cmake/sa2_renut_gpu.cmake, which is banjo-nx's cmake/banjo_renut_gpu.cmake: Renut's gpu.cmake as banjo-nx uses
# it, with its dependencies where it looks for them: thirdparty/plume (with the deko3d back end for later), the
# reblue-XenosRecomp translator, thirdparty/imgui and cmake/collect_captured_shaders.cmake.
#
# An OBJECT library, not a static one: its hooks replace the generated code's weak sub_X definitions, and a static
# archive's members are only pulled in for undefined symbols, so the hooks would silently not be linked.
#
# Stage 2 is the PC and the device only: creation through SHH's type-2 (no hardware) path, the ring sink, the fences
# and Swap. The game's shaders are not translated yet (stage 3), so the shader cache is empty (RENUT_NO_SHADER_CACHE).

# Stage 3 turns this off: XenosRecomp then translates the containers tools/dump_shaders.py dumps from the game files.
set(RENUT_NO_SHADER_CACHE ON)
set(RENUT_SHADER_DUMP_DIR "${CMAKE_SOURCE_DIR}/shaders")
set(RENUT_ASSETS_DIR "${CMAKE_SOURCE_DIR}/../assets/game_root" CACHE PATH
    "Game assets (default.xex and the .PAK files)")

if(NOT Python3_EXECUTABLE)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
endif()
if(NOT PYTHON_EXECUTABLE)
    set(PYTHON_EXECUTABLE "${Python3_EXECUTABLE}")
endif()

# Renut's SDK exports o1heap as rex::o1heap; banjo-nx's SDK fork names it o1heap (gpu.cmake links the latter).
if(TARGET o1heap AND NOT TARGET rex::o1heap)
    add_library(rex::o1heap ALIAS o1heap)
endif()

include("${CMAKE_SOURCE_DIR}/src/renut_engine/gpu/gpu.cmake")

add_library(shh_renut_gpu OBJECT
    "${CMAKE_SOURCE_DIR}/src/renut_engine/gpu/banjo_renut_select.cpp"
    "${CMAKE_SOURCE_DIR}/src/renut_engine/gpu/banjo_renut_tuning.cpp"
    "${CMAKE_SOURCE_DIR}/src/renut_engine/gpu/banjo_renut_checks.cpp"
    "${CMAKE_SOURCE_DIR}/src/renut_engine/gpu/banjo_renut_record.cpp"
    # banjo-nx (2026-10-19, latency batch 3): the display's refresh for the frame limiter (it reads the presents' fences)
    "${CMAKE_SOURCE_DIR}/src/renut_engine/display_sync.cpp"
    "${CMAKE_SOURCE_DIR}/src/renut_engine/perf/frame_stats.cpp"
    # The pieces banjo-nx's SDK and generated code would give the renderer, app-side here (shh_compat).
    "${CMAKE_SOURCE_DIR}/src/renut_engine/shh_compat/frame_latency.cpp"
    "${CMAKE_SOURCE_DIR}/src/renut_engine/shh_compat/shh_compat.cpp")
if(NOT REXGLUE_PLATFORM_SWITCH)
    # The Switch profiler counters the renderer feeds (sdk/src/ui/switch_perf.cpp on the console): no-ops on the PC.
    target_sources(shh_renut_gpu PRIVATE "${CMAKE_SOURCE_DIR}/src/me/me_pc_stubs.cpp")
endif()
target_include_directories(shh_renut_gpu PRIVATE
    "${CMAKE_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/src"
    # banjo-nx's headers the renderer's tuning includes, answered by SHH's here (cpu/banjo_enhancements.h, ...).
    "${CMAKE_SOURCE_DIR}/src/renut_engine/shh_compat"
    "${REXGLUE_ENTRYPOINT_INCLUDE_DIR}"
    # banjo_renut_tuning.cpp: the SDK's thread helpers (threading_switch.h, through src/me/me_platform_compat.h).
    "${REXSDK_DIR}/src/core")
target_link_libraries(shh_renut_gpu PRIVATE rex::runtime)
if(COMMAND rexglue_apply_target_settings)
    rexglue_apply_target_settings(shh_renut_gpu)
endif()
renut_gpu_setup(shh_renut_gpu)
target_compile_definitions(shh_renut_gpu PRIVATE SHH_RENUT_RENDERER=1)

# The game's hooks into it (the Direct3D entry points, src/renut_engine/gpu/d3d/hooks) and the app's wiring
# (src/shh_app.h asks banjo_renut_select.h which renderer the boot key chose).
target_link_libraries(shh PRIVATE shh_renut_gpu)
target_include_directories(shh PRIVATE "${CMAKE_SOURCE_DIR}")
target_compile_definitions(shh PRIVATE SHH_RENUT_RENDERER=1)
