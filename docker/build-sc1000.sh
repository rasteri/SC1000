#!/bin/bash
# Build SC1000 software using CMake + buildroot toolchain
# Run from host: docker exec <container> /home/builder/sc1000/docker/build-sc1000.sh
# Or from inside container: /home/builder/sc1000/docker/build-sc1000.sh

set -e

BUILDROOT_DIR="/home/builder/buildroot-2018.08"
SC1000_DIR="/home/builder/sc1000/software"
BUILD_DIR="$SC1000_DIR/build-buildroot"

# Check if buildroot config exists, copy if needed
if [ ! -f "$BUILDROOT_DIR/.config" ]; then
    echo "=============================================="
    echo "Setting up buildroot configuration..."
    echo "=============================================="
    cd "$BUILDROOT_DIR"
    cp /home/builder/sc1000/os/buildroot/buildroot_config .config

    # Enable C++ support (required for SC1000)
    sed -i 's/# BR2_TOOLCHAIN_BUILDROOT_CXX is not set/BR2_TOOLCHAIN_BUILDROOT_CXX=y/' .config

    make olddefconfig
fi

# Build toolchain if not already built
if [ ! -f "$BUILDROOT_DIR/output/host/bin/arm-linux-g++" ]; then
    echo "=============================================="
    echo "Building buildroot toolchain (this takes ~30-60 min)..."
    echo "=============================================="
    cd "$BUILDROOT_DIR"
    make toolchain
fi

# Build alsa-lib for headers if not present
if [ ! -f "$BUILDROOT_DIR/output/staging/usr/include/alsa/asoundlib.h" ]; then
    echo "=============================================="
    echo "Building alsa-lib..."
    echo "=============================================="
    cd "$BUILDROOT_DIR"
    make alsa-lib
fi

echo "=============================================="
echo "Building SC1000 with CMake + buildroot toolchain"
echo "=============================================="

cd "$SC1000_DIR"

# Configure with CMake using toolchain file
cmake -B "$BUILD_DIR" \
    -DCMAKE_TOOLCHAIN_FILE="$SC1000_DIR/cmake/buildroot-uclibc.cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILDROOT_DIR="$BUILDROOT_DIR"

# Build
cmake --build "$BUILD_DIR" -j$(nproc)

echo "=============================================="
echo "Build complete!"
echo "Binary: $BUILD_DIR/sc1000"
echo "=============================================="

# Show binary info
file "$BUILD_DIR/sc1000"
echo ""
echo "Library dependencies:"
"$BUILDROOT_DIR/output/host/bin/arm-linux-readelf" -d "$BUILD_DIR/sc1000" | grep NEEDED || true

echo ""
echo "To deploy: copy $BUILD_DIR/sc1000 to device as /usr/bin/sc1000"
