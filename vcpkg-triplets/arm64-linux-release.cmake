# Upstream triplet, plus PKG_CONFIG_PATH so Qt finds the target's
# system X11/XCB libraries (the preset sets it per architecture).
set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

set(VCPKG_CMAKE_SYSTEM_NAME Linux)
set(VCPKG_BUILD_TYPE release)
set(VCPKG_ENV_PASSTHROUGH_UNTRACKED PKG_CONFIG_PATH)
