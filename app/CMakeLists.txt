# shh - Silent Hill: Homecoming, ReXGlue recompiled project
#
# SDK boilerplate lives in generated/rexglue.cmake (written by the codegen).

cmake_minimum_required(VERSION 3.25)
project(shh LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# shh-nx (2026-10-29, from sa2-nx): -mearly-ra=none for everything compiled for the console. devkitA64 GCC 16.1's
# AArch64 early register allocation pass (on at -O2 and above) dropped the table lookups of the XDK's streaming copy
# in Peggle 2's console build (garbage textures on the console only), and SA2's copy of that routine shows the same
# damage. Here, before the SDK is added, so its libraries and the LTO link get it too.
option(SHH_SWITCH_EARLY_RA "Leave GCC's AArch64 early register allocation pass on (Switch; miscompiles)" OFF)
if(REXGLUE_PLATFORM_SWITCH AND NOT SHH_SWITCH_EARLY_RA)
    add_compile_options(-mearly-ra=none)
    add_link_options(-mearly-ra=none)
endif()

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
# shh-nx (2026-10-29, stage 4): the Switch too, on plume's deko3d back end with the shader package from the PC
# (cmake/shh_renut_gpu.cmake). A Switch build with REXGLUE_SWITCH_NO_NVK (tools\build_switch.bat nonvk) has only Renut.
option(SHH_RENUT_RENDERER "Link Renut's Direct3D-level renderer (shh_renderer = renut)" ON)
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
