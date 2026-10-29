#!/bin/bash
# First full build of the Switch Vulkan driver (NVK, from mesa-switch) on an x86-64 Windows PC.
# Run it from MSYS2 at C:\msys64 (MINGW64):
#
#   C:\msys64\usr\bin\env.exe MSYSTEM=MINGW64 /usr/bin/bash -l /c/dev/shh-nx/tools/build_mesa.sh
#
# Expects:
#   - mesa-switch at commit 1a8c1a66d6f with nfsmw-nx's mesa/mesa-switch-nfsmw.patch applied (MESA, default
#     C:/dev/mesa-switch)
#   - devkitPro linked at C:\msys64\opt\devkitpro (a junction to C:\devkitPro)
#   - Rust (stable-x86_64-pc-windows-gnu + aarch64-unknown-linux-gnu) in $MESA/build/deps/{rustup,cargo}
#   - MSYS2 packages: flex bison pkgconf, mingw64 gcc python python-mako python-yaml python-packaging meson ninja,
#     clang64 clang llvm lld libclc spirv-llvm-translator spirv-tools cmake pkgconf cbindgen rust-bindgen
#
# The result is the SDK folder $MESA/mesa-unified-install/opt/devkitpro/portlibs/switch (libvulkan.a and friends),
# which the app takes as REXGLUE_SWITCH_NVK_SDK.
set -eu
MESA=${MESA:-/c/dev/mesa-switch}
cd "$MESA"

# mesa-switch was written on an ARM64 Windows machine: its native tools file takes cbindgen from clangarm64.
sed 's|C:/msys64/clangarm64/bin/cbindgen.exe|C:/msys64/clang64/bin/cbindgen.exe|' \
    switch_native_tools_msys2.txt > switch_native_tools_msys2_x64.txt

export NATIVE_FILE="$MESA/switch_native_tools_msys2_x64.txt"
export MESA_SWITCH_BINDGEN=/clang64/bin/bindgen.exe
export DEVKITPRO=/opt/devkitpro
export RUSTUP_TOOLCHAIN=stable-x86_64-pc-windows-gnu
# The patched checkout is "dirty" for build-unified.sh, which otherwise refuses to build.
export ALLOW_DIRTY=1
export PATH="/mingw64/bin:/usr/bin:$PATH:/clang64/bin"

./build-unified.sh
