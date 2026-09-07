#!/bin/sh
# Build the SC1000/SC500 xwax binary (with OLED waveform display).
# Run from anywhere:  sh oled-build/build.sh
set -e

cd "$(dirname "$0")/.."          # -> SC1000 source root (has software/)

if ! command -v docker >/dev/null 2>&1; then
    echo "docker not found. One-time setup on macOS:" >&2
    echo "    brew install colima docker" >&2
    echo "    colima start --vm-type=vz --vz-rosetta --cpu 4 --memory 6" >&2
    exit 1
fi

echo ">>> building cross toolchain + xwax (first run downloads ~60 MB, takes a few min)"
docker build --platform=linux/amd64 -t sc1000-oled -f oled-build/Dockerfile .

echo ">>> extracting binary"
docker run --rm --platform=linux/amd64 -v "$PWD/oled-build:/out" sc1000-oled

echo
ls -l oled-build/xwax
file oled-build/xwax
echo
echo "NEXT: copy  oled-build/xwax  to the ROOT of the SC1000 USB stick"
echo "      (next to the beats/ and samples/ folders), then reboot the SC500."
echo "      The SC1000 runs /media/sda/xwax automatically if it is present."
