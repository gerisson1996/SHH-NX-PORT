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
    src/shh_pgo.cpp  # shh-nx (2026-11-11): PGO profile collection (SHH_PGO=generate; empty otherwise)
)

if(WIN32)
    add_executable(shh WIN32 ${SHH_SOURCES})
elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    # the app bundle's Info.plist is ios/Info.plist; the .ipa is packed unsigned
    add_executable(shh MACOSX_BUNDLE ${SHH_SOURCES})
    set_target_properties(shh PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/../ios/Info.plist"
        MACOSX_BUNDLE_BUNDLE_NAME "Silent Hill"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.gerisson.shh"
        XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"
        XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO")
    target_link_libraries(shh PRIVATE "-framework UIKit" "-framework Foundation" "-framework Metal"
        "-framework QuartzCore" "-framework CoreGraphics" "-framework AudioToolbox" "-framework CoreAudio")
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

# shh-nx (2026-11-11, from mgsv-nx, sa2-nx's method): profile-guided optimisation of the Switch build, for the game
# code (shh_recomp), the app (shh), Renut's renderer (shh_renut_gpu) and plume. Three steps:
#   SHH_PGO=generate  an instrumented NRO (toolsuild_switch.bat <nvk_sdk> pgo-generate) that streams its branch counts
#                     to pgo/shh_pgo_<N>.stream beside it on the SD card (src/shh_pgo.cpp). Value profiling is off: its
#                     shared lists are not thread-safe without atomic updates.
#   tools/pgo_merge.ps1 on the PC turns the streams into pgo/*.gcda.
#   SHH_PGO=use       (pgo-use) the build optimised with those counts. Code the sessions did not run is compiled as
#                     without a profile (-fprofile-partial-training); functions changed since keep none.
# No LTO: the game code is 53 MB of text, and the whole-program link would not fit this PC's 16 GB (sa2-nx's 39 MB
# already needed LTO limited to 2 jobs). PGO works one object at a time.
if(REXGLUE_PLATFORM_SWITCH)
    set(SHH_PGO "" CACHE STRING "Profile-guided optimisation of the Switch build: empty, generate or use")
    set_property(CACHE SHH_PGO PROPERTY STRINGS "" generate use)
    set(SHH_PGO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../pgo" CACHE PATH "Folder of the PGO profile (.gcda files)")
    set(SHH_PGO_INLINE_AUTO "64" CACHE STRING "max-inline-insns-auto of the game code with a profile (empty: GCC's)")
    set(SHH_PGO_INLINE_UNIT_GROWTH "12" CACHE STRING "inline-unit-growth of the game code with a profile (empty: GCC's)")
    get_filename_component(_shh_pgo_dir "${SHH_PGO_DIR}" ABSOLUTE)
    file(TO_NATIVE_PATH "${_shh_pgo_dir}" _shh_pgo_dir)
    file(TO_NATIVE_PATH "${CMAKE_BINARY_DIR}" _shh_pgo_prefix)
    set(_shh_pgo "")
    set(_shh_pgo_inline "")
    if(SHH_PGO STREQUAL "generate")
        set(_shh_pgo "-fprofile-generate=${_shh_pgo_dir}" "-fprofile-prefix-path=${_shh_pgo_prefix}"
                     -fno-profile-values -fprofile-update=single -fprofile-info-section=gcov_info)
        target_compile_definitions(shh PRIVATE SHH_PGO_GENERATE=1)
    elseif(SHH_PGO STREQUAL "use")
        if(NOT EXISTS "${SHH_PGO_DIR}")
            message(FATAL_ERROR "SHH_PGO=use: no profile in ${SHH_PGO_DIR} (tools/pgo_merge.ps1 makes it)")
        endif()
        # -fprofile-correction: counted without atomic updates while several game threads ran the same code.
        set(_shh_pgo "-fprofile-use=${_shh_pgo_dir}" "-fprofile-prefix-path=${_shh_pgo_prefix}" -fprofile-correction
                     -fno-profile-values -fprofile-partial-training -Wno-missing-profile -Wno-coverage-mismatch)
        if(SHH_PGO_INLINE_AUTO)
            list(APPEND _shh_pgo_inline --param=max-inline-insns-auto=${SHH_PGO_INLINE_AUTO})
        endif()
        if(SHH_PGO_INLINE_UNIT_GROWTH)
            list(APPEND _shh_pgo_inline --param=inline-unit-growth=${SHH_PGO_INLINE_UNIT_GROWTH})
        endif()
    elseif(SHH_PGO)
        message(FATAL_ERROR "SHH_PGO must be empty, generate or use (got ${SHH_PGO})")
    endif()
    if(SHH_PGO)
        foreach(target shh_recomp shh shh_renut_gpu plume)
            if(TARGET ${target})
                target_compile_options(${target} PRIVATE ${_shh_pgo})
            endif()
        endforeach()
        if(_shh_pgo_inline AND TARGET shh_recomp)
            target_compile_options(shh_recomp PRIVATE ${_shh_pgo_inline})
        endif()
        target_link_options(shh PRIVATE ${_shh_pgo})
        message(STATUS "shh: PGO ${SHH_PGO} (profile ${_shh_pgo_dir}; game code inliner '${_shh_pgo_inline}')")
    endif()
endif()

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
        AUTHOR "RECOMP-GUTO"
        VERSION "1.0"
        ${SHH_ICON_ARG})
endif()
