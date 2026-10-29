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
set(RENUT_NO_SHADER_CACHE OFF)
set(RENUT_SHADER_DUMP_DIR "${CMAKE_SOURCE_DIR}/shaders")
set(RENUT_ASSETS_DIR "${CMAKE_SOURCE_DIR}/../assets/game_root" CACHE PATH
    "Game assets (default.xex and the .PAK files)")

if(NOT Python3_EXECUTABLE)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
endif()
if(NOT PYTHON_EXECUTABLE)
    set(PYTHON_EXECUTABLE "${Python3_EXECUTABLE}")
endif()

# shh-nx (2026-10-29, stage 4): the Switch, as sa2-nx builds it. A cross build on a Windows host: Renut's XenosRecomp
# (a host tool) cannot be built with the console's compiler, so the shader cache and the deko3d GLSL package come
# from the PC (docs/plano-renut.md, "Etapa 4": the Vulkan SDK's DXC for the SPIR-V, tools/deko3d_shaders.py,
# deko3d_glsl_opt.py, deko3d_shader_package.py and renut_switch_shader_cache.py into app/out/renut_switch).
if(REXGLUE_PLATFORM_SWITCH)
    set(PLUME_PLATFORM_SWITCH ON CACHE BOOL "" FORCE)
    set(PLUME_SWITCH_DEVKITPRO "${DEVKITPRO}" CACHE PATH "" FORCE)
    set(PLUME_SWITCH_NVK_ROOT "${REXGLUE_SWITCH_NVK_SDK}" CACHE PATH "" FORCE)
    set(PLUME_SWITCH_NVK_LIBRARY "${REXGLUE_SWITCH_NVK_SDK}/lib/libvulkan.a" CACHE FILEPATH "" FORCE)
    set(PLUME_SWITCH_NO_NVK "${REXGLUE_SWITCH_NO_NVK}" CACHE BOOL "" FORCE)
    # plume's deko3d back end (renut_gpu_backend = "deko3d") and the uam it compiles shader variants with on the console.
    set(PLUME_SWITCH_UAM_DIR "${CMAKE_SOURCE_DIR}/thirdparty/uam" CACHE PATH "" FORCE)
    set(PLUME_DEKO3D ON CACHE BOOL "deko3d back end (Switch)" FORCE)
    # The host shaders' SPIR-V needs a DXC with SPIR-V code generation, which dxc-bin's is not: the Vulkan SDK's.
    set(_shh_vulkan_sdk "$ENV{VULKAN_SDK}")
    if(NOT _shh_vulkan_sdk)
        set(_shh_vulkan_sdk "C:/VulkanSDK/1.4.363.0")
    endif()
    set(RENUT_DXC_OVERRIDE "${_shh_vulkan_sdk}/Bin/dxc.exe" CACHE FILEPATH "DXC with SPIR-V code generation")
    if(NOT EXISTS "${RENUT_DXC_OVERRIDE}")
        message(FATAL_ERROR "No ${RENUT_DXC_OVERRIDE}: the Switch build needs the Vulkan SDK's DXC (SPIR-V)")
    endif()
    # The host shaders' deko3d blobs (gpu.cmake renut_host_shader): SPIRV-Cross (the Vulkan SDK's copy) and uam built
    # for the PC (tools/deko3d_build_tools.sh, or by hand with MSYS2's clang64), both in out/deko3d_tools.
    set(BANJO_DEKO3D_HOST_SHADERS ON)
    set(BANJO_DEKO3D_SPIRV_CROSS "${CMAKE_SOURCE_DIR}/../out/deko3d_tools/spirv-cross-build/spirv-cross.exe" CACHE FILEPATH
        "SPIRV-Cross for the deko3d host shaders")
    set(BANJO_DEKO3D_UAM "${CMAKE_SOURCE_DIR}/../out/deko3d_tools/uam-build/uam.exe" CACHE FILEPATH
        "uam (thirdparty/uam built for the PC) for the deko3d host shaders")
    foreach(_banjo_tool BANJO_DEKO3D_SPIRV_CROSS BANJO_DEKO3D_UAM)
        if(NOT EXISTS "${${_banjo_tool}}")
            message(FATAL_ERROR "No ${${_banjo_tool}}: build the deko3d PC tools first (docs/plano-renut.md, Etapa 4)")
        endif()
    endforeach()
    # Every translated shader built in: game data, for this user's own builds only, never distributed.
    set(_shh_renut_shaders "${CMAKE_SOURCE_DIR}/out/renut_switch")
    set(BANJO_DEKO3D_GLSL_CPP "${_shh_renut_shaders}/deko3d_glsl_switch.cpp" CACHE FILEPATH
        "The deko3d GLSL package's source (tools/deko3d_shader_package.py)" FORCE)
    set(BANJO_RENUT_SHADER_CACHE_CPP "${_shh_renut_shaders}/shader_cache_switch.cpp" CACHE FILEPATH
        "Renut's shader cache for the Switch build (tools/renut_switch_shader_cache.py)" FORCE)
    function(banjo_renut_shader_cache_override TARGET)
        if(NOT EXISTS "${BANJO_RENUT_SHADER_CACHE_CPP}")
            message(FATAL_ERROR "No ${BANJO_RENUT_SHADER_CACHE_CPP}: run tools/renut_switch_shader_cache.py first")
        endif()
        target_sources(${TARGET} PRIVATE "${BANJO_RENUT_SHADER_CACHE_CPP}")
        set_source_files_properties("${BANJO_RENUT_SHADER_CACHE_CPP}" PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
    endfunction()
endif()

# Renut's SDK exports o1heap as rex::o1heap; banjo-nx's SDK fork names it o1heap (gpu.cmake links the latter).
if(TARGET o1heap AND NOT TARGET rex::o1heap)
    add_library(rex::o1heap ALIAS o1heap)
endif()

# shh-nx (2026-10-29): the early-alpha pass is off: it rewrites the pixel shaders' HLSL, and DXC 1.9 refuses the
# result ("partial explicit stage input location assignment via vk::location(X) unsupported"). The exact shaders are
# what stage 3 needs first; the pass can come back with a DXC that accepts them.
set(BANJO_RENUT_EARLY_ALPHA OFF CACHE BOOL "" FORCE)

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
    "${CMAKE_SOURCE_DIR}/src/renut_engine/shh_compat/shh_compat.cpp"
    "${CMAKE_SOURCE_DIR}/src/renut_engine/shh_compat/fast_boot.cpp")
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
if(REXGLUE_PLATFORM_SWITCH)
    # The deko3d back end's glue and the game shaders' GLSL package.
    target_sources(shh_renut_gpu PRIVATE "${CMAKE_SOURCE_DIR}/src/renut_engine/gpu/banjo_renut_deko3d.cpp")
    if(EXISTS "${BANJO_DEKO3D_GLSL_CPP}")
        target_sources(shh_renut_gpu PRIVATE "${BANJO_DEKO3D_GLSL_CPP}")
        set_source_files_properties("${BANJO_DEKO3D_GLSL_CPP}" PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
    else()
        message(WARNING "No ${BANJO_DEKO3D_GLSL_CPP}: deko3d will have no game shaders (tools/deko3d_shader_package.py)")
    endif()
endif()
if(COMMAND rexglue_apply_target_settings)
    rexglue_apply_target_settings(shh_renut_gpu)
endif()
renut_gpu_setup(shh_renut_gpu)
target_compile_definitions(shh_renut_gpu PRIVATE SHH_RENUT_RENDERER=1)
if(REXGLUE_PLATFORM_SWITCH AND REXGLUE_SWITCH_NO_NVK)
    # No Vulkan driver in this build: Renut on deko3d whatever shh_renderer / renut_gpu_backend say (per source, so
    # nothing else recompiles). The macro keeps sa2-nx's name, which backend.cpp tests.
    set_property(SOURCE "${CMAKE_SOURCE_DIR}/src/renut_engine/gpu/banjo_renut_select.cpp"
                        "${RENUT_GPU_DIR}/backend/backend.cpp"
                 APPEND PROPERTY COMPILE_DEFINITIONS SA2_SWITCH_NO_NVK=1)
endif()

# The game's hooks into it (the Direct3D entry points, src/renut_engine/gpu/d3d/hooks) and the app's wiring
# (src/shh_app.h asks banjo_renut_select.h which renderer the boot key chose).
target_link_libraries(shh PRIVATE shh_renut_gpu)
target_include_directories(shh PRIVATE "${CMAKE_SOURCE_DIR}")
target_compile_definitions(shh PRIVATE SHH_RENUT_RENDERER=1)
