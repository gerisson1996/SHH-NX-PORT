# iOS toolchain for shh-nx
#
#   cmake -S app -B build/ios -G Xcode \
#         -DCMAKE_TOOLCHAIN_FILE=ios/toolchain.cmake -DCMAKE_BUILD_TYPE=Release
#
# The runtime picks APPLE (ReXGlue's mac path) and Vulkan through MoltenVK (sdk/thirdparty/moltenvk), the same way
# the macOS build does. Codesigning is off: the .ipa is unsigned.
set(CMAKE_SYSTEM_NAME iOS)

set(CMAKE_OSX_ARCHITECTURES arm64 CACHE STRING "" FORCE)
set(CMAKE_OSX_DEPLOYMENT_TARGET 15.0 CACHE STRING "" FORCE)

# the app bundle's identity lives in ios/Info.plist, and the game files come from the app's Documents folder
set(REXGLUE_IOS ON CACHE BOOL "" FORCE)
