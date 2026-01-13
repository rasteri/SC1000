# CMake toolchain file for ARM cross-compilation (SC1000 device)
# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/arm-linux-gnueabihf.cmake ..

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Cross-compiler (installed via apt install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf)
set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)

# Target architecture flags for Cortex-A8 (Allwinner A13)
set(CMAKE_C_FLAGS_INIT "-march=armv7-a -mfpu=neon -mfloat-abi=hard")
set(CMAKE_CXX_FLAGS_INIT "-march=armv7-a -mfpu=neon -mfloat-abi=hard")

# Sysroot for ARM libraries (if using multiarch)
# The ARM libraries are installed in /usr/lib/arm-linux-gnueabihf/
set(CMAKE_FIND_ROOT_PATH /usr/arm-linux-gnueabihf)

# Search for programs in the host environment
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# Search for libraries and headers in the target environment
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Help find ALSA in multiarch location
set(CMAKE_LIBRARY_PATH /usr/lib/arm-linux-gnueabihf)
set(CMAKE_INCLUDE_PATH /usr/include)
