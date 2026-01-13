#!/bin/bash
# Build the SC1000 buildroot Docker image

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "Building SC1000 buildroot Docker image..."
docker build -t sc1000-buildroot "$SCRIPT_DIR"

echo ""
echo "Done! Run ./run-docker.sh to enter the build environment."
