#!/bin/bash
# Build sc.tar update package for SC1000/SC500
# Usage: ./buildupdater.sh [--binary PATH]
#
# This creates a tarball that can be placed on USB stick for button-hold update.
# The tarball contains:
#   - sc1000 (the binary)
#   - S50sc1000 (init script - migrates from legacy S50xwax)
#   - sc1000-import (audio importer script)
#   - sc_settings.json (settings)
#   - zImage, sun5i-a13-olinuxino.dtb (kernel/dtb if present)
#   - Audio feedback files

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
TARBALL_DIR="$SCRIPT_DIR/tarball"
OUTPUT_TAR="$SCRIPT_DIR/sc.tar"
OVERLAY_DIR="$PROJECT_ROOT/os/buildroot/sc1000overlay"

# Default binary location (Docker build output)
BINARY_PATH=""

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --binary)
            BINARY_PATH="$2"
            shift 2
            ;;
        -h|--help)
            echo "Usage: $0 [--binary PATH]"
            echo ""
            echo "Options:"
            echo "  --binary PATH   Path to compiled sc1000 binary"
            echo ""
            echo "If --binary not specified, looks for:"
            echo "  1. software/build-buildroot/sc1000 (Docker CMake build)"
            echo "  2. software/build-arm/sc1000"
            echo "  3. software/src/Build/Release/sc1000 (Makefile build)"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Find binary if not specified
if [ -z "$BINARY_PATH" ]; then
    CANDIDATES=(
        "$PROJECT_ROOT/software/build-buildroot/sc1000"
        "$PROJECT_ROOT/software/build-arm/sc1000"
        "$PROJECT_ROOT/software/src/Build/Release/sc1000"
    )

    for candidate in "${CANDIDATES[@]}"; do
        if [ -f "$candidate" ]; then
            BINARY_PATH="$candidate"
            break
        fi
    done

    if [ -z "$BINARY_PATH" ]; then
        echo "ERROR: No binary found. Build first or specify with --binary"
        echo "Searched:"
        for candidate in "${CANDIDATES[@]}"; do
            echo "  $candidate"
        done
        exit 1
    fi
fi

echo "=== Building SC1000 Update Package ==="
echo "Binary: $BINARY_PATH"

# Verify binary exists and is ARM
if [ ! -f "$BINARY_PATH" ]; then
    echo "ERROR: Binary not found: $BINARY_PATH"
    exit 1
fi

# Check if it's an ARM binary
if file "$BINARY_PATH" | grep -q "ARM"; then
    echo "Binary type: ARM (correct for device)"
elif file "$BINARY_PATH" | grep -q "x86-64\|x86_64"; then
    echo "WARNING: Binary is x86-64, not ARM! This won't run on the device."
    read -p "Continue anyway? [y/N] " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

# Ensure tarball directory exists and is clean
mkdir -p "$TARBALL_DIR"
rm -f "$TARBALL_DIR"/{sc1000,xwax,S50sc1000,S50xwax,sc1000-import,xwax-import,sc_settings.json,scsettings.txt} 2>/dev/null || true

# Copy binary
echo "Copying binary..."
cp "$BINARY_PATH" "$TARBALL_DIR/sc1000"
chmod +x "$TARBALL_DIR/sc1000"

# Copy init script (for migration from S50xwax to S50sc1000)
echo "Copying init script..."
if [ -f "$OVERLAY_DIR/etc/init.d/S50sc1000" ]; then
    cp "$OVERLAY_DIR/etc/init.d/S50sc1000" "$TARBALL_DIR/S50sc1000"
    chmod +x "$TARBALL_DIR/S50sc1000"
fi

# Copy importer script
echo "Copying importer script..."
if [ -f "$OVERLAY_DIR/root/sc1000-import" ]; then
    cp "$OVERLAY_DIR/root/sc1000-import" "$TARBALL_DIR/sc1000-import"
    chmod +x "$TARBALL_DIR/sc1000-import"
fi

# Copy settings
echo "Copying settings..."
if [ -f "$PROJECT_ROOT/software/sc_settings.json" ]; then
    cp "$PROJECT_ROOT/software/sc_settings.json" "$TARBALL_DIR/sc_settings.json"
fi

# Create tarball
echo "Creating $OUTPUT_TAR..."
rm -f "$OUTPUT_TAR"
cd "$TARBALL_DIR"
tar -cf "$OUTPUT_TAR" *

echo ""
echo "=== Update Package Created ==="
echo "Output: $OUTPUT_TAR"
echo ""
echo "Contents:"
tar -tvf "$OUTPUT_TAR"
echo ""
echo "Size: $(du -h "$OUTPUT_TAR" | cut -f1)"
echo ""
echo "To update device:"
echo "  1. Copy sc.tar to USB stick root"
echo "  2. Insert USB into device"
echo "  3. Hold buttons while powering on"
echo "  4. Wait for success audio"
echo ""
echo "Note: This update migrates from legacy xwax naming to sc1000"
