#!/bin/bash
# Build SC1000 and update prebuilt binaries for release
# Usage: ./release.sh [--no-build]
#
# This script:
#   1. Builds the sc1000 binary using Docker
#   2. Copies binary to prebuilt/ directory
#   3. Creates updater tarball (sc.tar)
#   4. Shows what to commit

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
SOFTWARE_DIR="$PROJECT_ROOT/software"
UPDATER_DIR="$PROJECT_ROOT/updater"
PREBUILT_DIR="$PROJECT_ROOT/prebuilt"

DO_BUILD=true

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --no-build)
            DO_BUILD=false
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [--no-build]"
            echo ""
            echo "Options:"
            echo "  --no-build    Skip build, use existing binary"
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
    docker ps --format '{{.Names}}' | while read name; do
        if docker exec "$name" test -d /home/builder/buildroot-2018.08 2>/dev/null; then
            echo "$name"
            break
        fi
    done
}

echo "=== SC1000 Release Build ==="

# Find container
CONTAINER=$(find_container)
if [ -z "$CONTAINER" ]; then
    echo "ERROR: No running buildroot container found."
    echo "Start one with: ./run-docker.sh"
    exit 1
fi
echo "Using container: $CONTAINER"

# Build if requested
if [ "$DO_BUILD" = true ]; then
    echo ""
    echo "=== Building SC1000 binary ==="

    # Sync source
    docker cp "$SOFTWARE_DIR/src" "$CONTAINER:/home/builder/sc1000/software/"
    docker cp "$SOFTWARE_DIR/deps" "$CONTAINER:/home/builder/sc1000/software/"
    docker cp "$SOFTWARE_DIR/cmake" "$CONTAINER:/home/builder/sc1000/software/"
    docker cp "$SOFTWARE_DIR/CMakeLists.txt" "$CONTAINER:/home/builder/sc1000/software/"

    # Build
    docker exec "$CONTAINER" bash -c "cd /home/builder/sc1000/software && \
        mkdir -p build-arm && cd build-arm && \
        cmake -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_TOOLCHAIN_FILE=../cmake/buildroot-uclibc.cmake .. && \
        make -j\$(nproc)"

    BUILD_PATH="/home/builder/sc1000/software/build-arm/sc1000"
else
    BUILD_PATH="/home/builder/sc1000/software/build-arm/sc1000"
    echo "Skipping build, using existing binary"
fi

# Verify binary exists
if ! docker exec "$CONTAINER" test -f "$BUILD_PATH"; then
    echo "ERROR: Binary not found at $BUILD_PATH"
    exit 1
fi

# Check it's ARM
echo ""
echo "=== Verifying binary ==="
docker exec "$CONTAINER" file "$BUILD_PATH"

# Create prebuilt directory
mkdir -p "$PREBUILT_DIR"

# Copy binary to prebuilt
echo ""
echo "=== Copying to prebuilt/ ==="
docker cp "$CONTAINER:$BUILD_PATH" "$PREBUILT_DIR/sc1000"
chmod +x "$PREBUILT_DIR/sc1000"

# Show binary info
ls -lh "$PREBUILT_DIR/sc1000"
file "$PREBUILT_DIR/sc1000"

# Create updater tarball
echo ""
echo "=== Creating updater tarball ==="
"$UPDATER_DIR/buildupdater.sh" --binary "$PREBUILT_DIR/sc1000"

# Copy sc.tar to prebuilt as well
cp "$UPDATER_DIR/sc.tar" "$PREBUILT_DIR/sc.tar"

echo ""
echo "=== Release Build Complete ==="
echo ""
echo "Files updated:"
ls -lh "$PREBUILT_DIR/"
echo ""
echo "To commit these changes:"
echo "  git add prebuilt/"
echo "  git commit -m 'Update prebuilt binaries'"
echo ""
echo "To deploy to USB stick:"
echo "  cp prebuilt/sc1000 /media/YOUR_USB/"
echo "  cp software/sc_settings.json /media/YOUR_USB/"
echo "  # For full update:"
echo "  cp prebuilt/sc.tar /media/YOUR_USB/"
