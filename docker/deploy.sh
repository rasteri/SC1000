#!/bin/bash
# Build and deploy SC1000 to USB stick
# Usage: ./deploy.sh [OPTIONS]
#
# Options:
#   --make          Use legacy Makefile instead of CMake
#   --usb PATH      USB mount point (default: auto-detect)
#   --no-build      Skip build, just copy existing binary
#   --tar           Also create sc.tar for full system update
#
# The script will:
#   1. Build the binary in Docker (cross-compile for ARM)
#   2. Copy binary as 'sc1000' to USB stick
#   3. Copy sc_settings.json to USB stick
#   4. Optionally create sc.tar for button-hold update

set -e

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
SOFTWARE_DIR="$PROJECT_ROOT/software"
UPDATER_DIR="$PROJECT_ROOT/updater"

USE_CMAKE=true
DO_BUILD=true
CREATE_TAR=false
USB_MOUNT=""

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --make)
            USE_CMAKE=false
            shift
            ;;
        --usb)
            USB_MOUNT="$2"
            shift 2
            ;;
        --no-build)
            DO_BUILD=false
            shift
            ;;
        --tar)
            CREATE_TAR=true
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --make          Use legacy Makefile instead of CMake"
            echo "  --usb PATH      USB mount point (default: auto-detect)"
            echo "  --no-build      Skip build, just copy existing binary"
            echo "  --tar           Also create sc.tar for full system update"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Find running buildroot container
find_container() {
    # Look for a running container with buildroot
    local container=$(docker ps --format '{{.Names}}' | while read name; do
        if docker exec "$name" test -d /home/builder/buildroot-2018.08 2>/dev/null; then
            echo "$name"
            break
        fi
    done)
    echo "$container"
}

# Auto-detect USB mount point
find_usb() {
    # Common mount points to check
    local mounts=(
        "/media/$USER"/*
        "/run/media/$USER"/*
        "/mnt"/*
    )

    for mount in "${mounts[@]}"; do
        if [ -d "$mount" ] && mountpoint -q "$mount" 2>/dev/null; then
            # Check if it's a removable device
            local device=$(findmnt -n -o SOURCE "$mount" 2>/dev/null)
            if [ -n "$device" ]; then
                local removable=$(cat "/sys/block/$(basename "$device" | sed 's/[0-9]*$//')/removable" 2>/dev/null)
                if [ "$removable" = "1" ]; then
                    echo "$mount"
                    return 0
                fi
            fi
        fi
    done
    return 1
}

# Build the binary
# Returns the path to the built binary on stdout (all other output goes to stderr)
build_binary() {
    local container="$1"

    echo "=== Syncing source files to container ===" >&2
    docker cp "$SOFTWARE_DIR/src" "$container:/home/builder/sc1000/software/"
    docker cp "$SOFTWARE_DIR/deps" "$container:/home/builder/sc1000/software/"
    docker cp "$SOFTWARE_DIR/cmake" "$container:/home/builder/sc1000/software/"
    docker cp "$SOFTWARE_DIR/CMakeLists.txt" "$container:/home/builder/sc1000/software/"

    echo "=== Building SC1000 ===" >&2
    if [ "$USE_CMAKE" = true ]; then
        docker exec "$container" bash -c "cd /home/builder/sc1000/software && \
            rm -rf build-arm && mkdir -p build-arm && cd build-arm && \
            cmake -DCMAKE_BUILD_TYPE=Release \
                -DCMAKE_TOOLCHAIN_FILE=../cmake/buildroot-uclibc.cmake .. && \
            make -j\$(nproc)" >&2
        echo "/home/builder/sc1000/software/build-arm/sc1000"
    else
        docker exec "$container" bash -c "cd /home/builder/sc1000/software && \
            rm -rf src/Build/Release/obj && \
            make ARCH=SC1000 build=Release BUILDROOT_PREFIX=/home/builder/buildroot-2018.08 -j\$(nproc)" >&2
        echo "/home/builder/sc1000/software/src/Build/Release/sc1000"
    fi
}

# Main script
echo "=== SC1000 Deployment Script ==="

# Find container
CONTAINER=$(find_container)
if [ -z "$CONTAINER" ]; then
    echo "ERROR: No running buildroot container found."
    echo "Start one with: cd docker && ./run-docker.sh"
    exit 1
fi
echo "Using container: $CONTAINER"

# Find USB mount
if [ -z "$USB_MOUNT" ]; then
    USB_MOUNT=$(find_usb)
    if [ -z "$USB_MOUNT" ]; then
        echo "ERROR: No USB stick found. Insert one or specify with --usb PATH"
        exit 1
    fi
fi
echo "USB mount: $USB_MOUNT"

# Build
if [ "$DO_BUILD" = true ]; then
    BUILD_OUTPUT=$(build_binary "$CONTAINER")
else
    if [ "$USE_CMAKE" = true ]; then
        BUILD_OUTPUT="/home/builder/sc1000/software/build-arm/sc1000"
    else
        BUILD_OUTPUT="/home/builder/sc1000/software/src/Build/Release/sc1000"
    fi
fi

# Copy to USB
echo "=== Copying files to USB stick ==="
docker cp "$CONTAINER:$BUILD_OUTPUT" "$USB_MOUNT/sc1000"
cp "$SOFTWARE_DIR/sc_settings.json" "$USB_MOUNT/sc_settings.json"

# Create sc.tar if requested
if [ "$CREATE_TAR" = true ]; then
    echo "=== Creating sc.tar for full system update ==="

    # Copy binary from container to local tarball directory
    docker cp "$CONTAINER:$BUILD_OUTPUT" "$UPDATER_DIR/tarball/sc1000"

    # Use the buildupdater script to create the tarball
    "$UPDATER_DIR/buildupdater.sh" --binary "$UPDATER_DIR/tarball/sc1000"

    # Copy to USB
    cp "$UPDATER_DIR/sc.tar" "$USB_MOUNT/sc.tar"
fi

sync

echo ""
echo "=== Deployment Complete ==="
echo ""
echo "Files on USB stick:"
ls -la "$USB_MOUNT"/{sc1000,sc_settings.json} 2>/dev/null
if [ "$CREATE_TAR" = true ]; then
    ls -la "$USB_MOUNT/sc.tar" 2>/dev/null
fi
echo ""
echo "Quick update: Insert USB, power on device - it will run sc1000 from USB"
echo "Full update:  Insert USB, hold buttons at power-on to install sc.tar"
echo ""

# Offer to unmount
read -p "Unmount USB stick? [Y/n] " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Nn]$ ]]; then
    sync
    if command -v udisksctl &> /dev/null; then
        udisksctl unmount -b "$(findmnt -n -o SOURCE "$USB_MOUNT")" 2>/dev/null || true
    else
        umount "$USB_MOUNT" 2>/dev/null || true
    fi
    echo "Safe to remove USB stick."
fi
