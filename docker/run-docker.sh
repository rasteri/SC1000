#!/bin/bash
# Run the SC1000 buildroot Docker container

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

echo "Starting SC1000 buildroot environment..."
echo "Project mounted at: /home/builder/sc1000"
echo ""
echo "First run: copy config and build toolchain with:"
echo "  cp /home/builder/sc1000/os/buildroot/buildroot_config .config"
echo "  make olddefconfig"
echo "  make toolchain"
echo ""

docker run -it --rm \
    -v "$PROJECT_DIR:/home/builder/sc1000:rw" \
    sc1000-buildroot \
    /bin/bash

