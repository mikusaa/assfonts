# The Linux build image has i686-linux-gnu cross compilers but no 32-bit
# multilib for the host gcc, which upstream vcpkg would use with -m32.
# Pick the cross compilers, then use upstream's Linux toolchain as usual.
if(NOT DEFINED CMAKE_C_COMPILER)
    set(CMAKE_C_COMPILER "i686-linux-gnu-gcc")
endif()
if(NOT DEFINED CMAKE_CXX_COMPILER)
    set(CMAKE_CXX_COMPILER "i686-linux-gnu-g++")
endif()
if(NOT DEFINED CMAKE_ASM_COMPILER)
    set(CMAKE_ASM_COMPILER "i686-linux-gnu-gcc")
endif()

if(NOT DEFINED Z_VCPKG_ROOT_DIR)
    set(Z_VCPKG_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/../vcpkg")
endif()
include("${Z_VCPKG_ROOT_DIR}/scripts/toolchains/linux.cmake")
