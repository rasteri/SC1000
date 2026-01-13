#!/bin/bash
# Build SC1000 complete OS image using Docker
# Usage: ./build-os.sh [OPTIONS]
#
# This builds the full buildroot system including:
#   - Linux kernel 4.17.19 (sunxi)
#   - U-Boot bootloader
#   - Root filesystem with SC1000 overlay
#   - Final sdcard.img for flashing
#
# Options:
#   --clean         Clean build (remove previous build artifacts)
#   --toolchain     Only build toolchain (faster first-time setup)
#   --menuconfig    Run buildroot menuconfig for configuration
#   --resume        Resume existing container (don't start fresh)
#
# Output: os/sdcard.img.gz (compressed SD card image)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
OS_DIR="$PROJECT_ROOT/os"
BUILDROOT_DIR="$OS_DIR/buildroot"
OUTPUT_DIR="$OS_DIR"

DO_CLEAN=false
TOOLCHAIN_ONLY=false
MENUCONFIG=false
RESUME_CONTAINER=false
CONTAINER_NAME=""

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --clean)
            DO_CLEAN=true
            shift
            ;;
        --toolchain)
            TOOLCHAIN_ONLY=true
            shift
            ;;
        --menuconfig)
            MENUCONFIG=true
            shift
            ;;
        --resume)
            RESUME_CONTAINER=true
            shift
            ;;
        -h|--help)
            head -19 "$0" | tail -18
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Find or create container
find_buildroot_container() {
    docker ps --format '{{.Names}}' | while read name; do
        if docker exec "$name" test -d /home/builder/buildroot-2018.08 2>/dev/null; then
            echo "$name"
            break
        fi
    done
}

echo "=== SC1000 OS Image Builder ==="

# Check if Docker image exists
if ! docker image inspect sc1000-buildroot >/dev/null 2>&1; then
    echo "Docker image not found. Building..."
    "$SCRIPT_DIR/build-docker.sh"
fi

# Find existing container or start new one
if [ "$RESUME_CONTAINER" = true ]; then
    CONTAINER_NAME=$(find_buildroot_container)
    if [ -z "$CONTAINER_NAME" ]; then
        echo "ERROR: No running buildroot container found."
        echo "Start one with: ./run-docker.sh"
        exit 1
    fi
    echo "Resuming container: $CONTAINER_NAME"
else
    # Start a new container for the build
    CONTAINER_NAME="sc1000-os-build-$$"
    echo "Starting build container: $CONTAINER_NAME"
    docker run -d --name "$CONTAINER_NAME" \
        -v "$PROJECT_ROOT:/home/builder/sc1000:rw" \
        sc1000-buildroot \
        sleep infinity

    # Ensure cleanup on exit
    trap "echo 'Cleaning up container...'; docker rm -f $CONTAINER_NAME 2>/dev/null" EXIT
fi

echo "Container: $CONTAINER_NAME"

# Sync buildroot config
echo ""
echo "=== Syncing buildroot configuration ==="
docker exec "$CONTAINER_NAME" cp /home/builder/sc1000/os/buildroot/buildroot_config /home/builder/buildroot-2018.08/.config

# Sync overlay
echo "=== Syncing SC1000 overlay ==="
docker exec "$CONTAINER_NAME" rm -rf /home/builder/buildroot-2018.08/sc1000overlay
docker exec "$CONTAINER_NAME" cp -r /home/builder/sc1000/os/buildroot/sc1000overlay /home/builder/buildroot-2018.08/

# Verify overlay was copied
docker exec "$CONTAINER_NAME" ls -la /home/builder/buildroot-2018.08/sc1000overlay/etc/init.d/

# Clean if requested
if [ "$DO_CLEAN" = true ]; then
    echo ""
    echo "=== Cleaning previous build ==="
    docker exec "$CONTAINER_NAME" bash -c "cd /home/builder/buildroot-2018.08 && make clean"
fi

# Run menuconfig if requested
if [ "$MENUCONFIG" = true ]; then
    echo ""
    echo "=== Running menuconfig ==="
    docker exec -it "$CONTAINER_NAME" bash -c "cd /home/builder/buildroot-2018.08 && make menuconfig"

    # Save updated config back
    echo "Saving config..."
    docker cp "$CONTAINER_NAME:/home/builder/buildroot-2018.08/.config" "$BUILDROOT_DIR/buildroot_config"
    echo "Config saved to: $BUILDROOT_DIR/buildroot_config"
    exit 0
fi

# Update config with olddefconfig
echo ""
echo "=== Updating config ==="
docker exec "$CONTAINER_NAME" bash -c "cd /home/builder/buildroot-2018.08 && make olddefconfig"

# Toolchain only?
if [ "$TOOLCHAIN_ONLY" = true ]; then
    echo ""
    echo "=== Building toolchain only ==="
    docker exec "$CONTAINER_NAME" bash -c "cd /home/builder/buildroot-2018.08 && make toolchain"
    echo ""
    echo "Toolchain built successfully!"
    echo "Run without --toolchain to build full OS image."
    exit 0
fi

# Full build
echo ""
echo "=== Building OS image (this may take a while) ==="
docker exec "$CONTAINER_NAME" bash -c "cd /home/builder/buildroot-2018.08 && make -j\$(nproc)"

# Copy output images
echo ""
echo "=== Extracting build outputs ==="
mkdir -p "$OUTPUT_DIR"

# List what was built
echo "Build outputs:"
docker exec "$CONTAINER_NAME" ls -la /home/builder/buildroot-2018.08/output/images/

# Copy and compress sdcard.img
if docker exec "$CONTAINER_NAME" test -f /home/builder/buildroot-2018.08/output/images/sdcard.img; then
    echo ""
    echo "Compressing sdcard.img..."
    docker exec "$CONTAINER_NAME" bash -c "gzip -c /home/builder/buildroot-2018.08/output/images/sdcard.img > /home/builder/sc1000/os/sdcard.img.gz"
    echo "Output: $OUTPUT_DIR/sdcard.img.gz"
    ls -lh "$OUTPUT_DIR/sdcard.img.gz"
else
    echo "WARNING: sdcard.img not found in output"
    echo "Check if genimage is configured correctly"
fi

# Copy individual images for reference
echo ""
echo "Copying individual images..."
docker cp "$CONTAINER_NAME:/home/builder/buildroot-2018.08/output/images/zImage" "$OUTPUT_DIR/" 2>/dev/null && echo "  zImage"
docker cp "$CONTAINER_NAME:/home/builder/buildroot-2018.08/output/images/sun5i-a13-olinuxino.dtb" "$OUTPUT_DIR/" 2>/dev/null && echo "  sun5i-a13-olinuxino.dtb"
docker cp "$CONTAINER_NAME:/home/builder/buildroot-2018.08/output/images/u-boot-sunxi-with-spl.bin" "$OUTPUT_DIR/" 2>/dev/null && echo "  u-boot-sunxi-with-spl.bin"
docker cp "$CONTAINER_NAME:/home/builder/buildroot-2018.08/output/images/rootfs.tar" "$OUTPUT_DIR/" 2>/dev/null && echo "  rootfs.tar"

echo ""
echo "=== Build Complete ==="
echo ""
echo "To flash to SD card:"
echo "  gunzip -c $OUTPUT_DIR/sdcard.img.gz | sudo dd of=/dev/sdX bs=4M status=progress"
echo ""
echo "Or use Raspberry Pi Imager / Balena Etcher with sdcard.img.gz"
