# Not in upstream vcpkg; used by the x86 Linux build.
set(VCPKG_TARGET_ARCHITECTURE x86)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

set(VCPKG_CMAKE_SYSTEM_NAME Linux)
set(VCPKG_BUILD_TYPE release)
# GCC 12's i686 compiler defaults to no SSE2, which breaks the SIMD headers
# libspng and Qt include. Every 32-bit x86 CPU in use today has SSE2.
set(VCPKG_C_FLAGS "-msse2")
set(VCPKG_CXX_FLAGS "-msse2")
set(VCPKG_ENV_PASSTHROUGH_UNTRACKED PKG_CONFIG_PATH)
set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/x86-linux-toolchain.cmake")
