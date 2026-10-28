# shh - Silent Hill: Homecoming, ReXGlue recompiled project
#
# SDK boilerplate lives in generated/rexglue.cmake (written by the codegen).

cmake_minimum_required(VERSION 3.25)
project(shh LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/generated/rexglue.cmake")
    message(FATAL_ERROR "Run the codegen first: REXGLUE=sdk/out/win-amd64/rexglue.exe tools/codegen.sh app")
endif()
include(generated/rexglue.cmake)

set(SHH_SOURCES
    src/main.cpp
)

if(WIN32)
    add_executable(shh WIN32 ${SHH_SOURCES})
else()
    add_executable(shh ${SHH_SOURCES})
endif()

# GPU_PLUGINS puts the graphics backend DLL (rexgpu-xenos) next to the .exe; src/shh_app.h
# sets gpu_plugin=xenos by default so it is loaded.
rexglue_setup_target(shh GPU_PLUGINS xenos)

# Renut's Direct3D-level renderer (docs/plano-renut.md): no PM4 ring, no EDRAM; the game's Direct3D runs natively
# through plume. Linked in with this option, chosen at boot with shh_renderer = "renut" (src/renut_engine, from
# sa2-nx). OFF builds exactly the stage 1 program.
#
# shh-nx (2026-10-28): the PC only. The Switch build cannot take this yet: plume there needs its deko3d back end, the
# static NVK driver and the shader package (docs/plano-renut.md, stage 4), and cmake/shh_renut_gpu.cmake is written for
# the PC's D3D12/Vulkan. Until stage 4, the console keeps the emulated Xenos GPU, which is what its NRO has today.
if(REXGLUE_PLATFORM_SWITCH)
    option(SHH_RENUT_RENDERER "Link Renut's Direct3D-level renderer (PC only; stage 4 adds the Switch)" OFF)
else()
    option(SHH_RENUT_RENDERER "Link Renut's Direct3D-level renderer (shh_renderer = renut)" ON)
endif()
if(SHH_RENUT_RENDERER)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/shh_renut_gpu.cmake")
endif()

# shh_register.cpp is one huge function that registers the ~54,000 game functions; GCC at -O3 spends over
# 20 minutes on it. It only runs once at startup, so -O1 costs nothing. The precompiled header is built at the
# target's optimization level, and clang refuses it at another one, so this file goes without it.
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/generated/default/shh_register.cpp"
    PROPERTIES COMPILE_OPTIONS "-O1" SKIP_PRECOMPILE_HEADERS ON)

# Nintendo Switch: shh.nro next to the ELF. On the SD card it goes in sdmc:/switch/shh-nx/ with the
# game_root/ folder beside it (src/shh_app.h looks for it there).
if(REXGLUE_PLATFORM_SWITCH)
    # Homebrew menu icon: a 256x256 JPEG, as elf2nro expects. Not distributed; without it the NRO gets
    # the default libnx icon.
    set(SHH_ICON "${CMAKE_CURRENT_SOURCE_DIR}/../tools/switch/icon/shh_icon.jpg")
    set(SHH_ICON_ARG)
    if(EXISTS "${SHH_ICON}")
        set(SHH_ICON_ARG ICON "${SHH_ICON}")
    endif()
    rexglue_switch_add_nro(shh
        NAME "Silent Hill Homecoming"
        AUTHOR "shh-nx"
        VERSION "0.1.0"
        ${SHH_ICON_ARG})
endif()
