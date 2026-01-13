# CMake toolchain file for buildroot uClibc cross-compilation
# Use this inside the Docker container

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Buildroot toolchain paths (set via environment or here)
set(BUILDROOT_DIR "$ENV{BUILDROOT_DIR}" CACHE PATH "Buildroot directory")
if(NOT BUILDROOT_DIR)
    set(BUILDROOT_DIR "/home/builder/buildroot-2018.08")
endif()

set(TOOLCHAIN_PREFIX "${BUILDROOT_DIR}/output/host/bin/arm-linux")
set(SYSROOT "${BUILDROOT_DIR}/output/staging")

# Cross-compiler
set(CMAKE_C_COMPILER "${TOOLCHAIN_PREFIX}-gcc")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_PREFIX}-g++")

# Sysroot for uClibc and libraries
set(CMAKE_SYSROOT "${SYSROOT}")

# Target architecture flags for Cortex-A8 (Allwinner A13)
set(CMAKE_C_FLAGS_INIT "-mcpu=cortex-a8 -mfpu=neon -mfloat-abi=hard")
set(CMAKE_CXX_FLAGS_INIT "-mcpu=cortex-a8 -mfpu=neon -mfloat-abi=hard")

# Search paths
set(CMAKE_FIND_ROOT_PATH "${SYSROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
