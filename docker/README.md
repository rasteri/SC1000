# SC1000 Buildroot Docker Environment

This Docker setup provides an isolated buildroot 2018.08 environment for building
SC1000/SC500 binaries that are compatible with the device's uClibc-based OS.

## Why Docker?

The SC1000/SC500 uses an older Linux with **uClibc** (not glibc). Standard ARM
cross-compilers produce glibc binaries that won't run on the device. The buildroot
2018.08 toolchain produces compatible uClibc binaries.

## Prerequisites

- Docker installed (`sudo apt install docker.io`)
- Your user in the docker group (`sudo usermod -aG docker $USER`, then re-login)

## Quick Start

```bash
# 1. Build the Docker image (one-time setup, ~2 minutes)
./build-docker.sh

# 2. Enter the build environment
./run-docker.sh

# 3. Inside the container, build SC1000:
./sc1000/docker/build-sc1000.sh
```

The build script automatically:
1. Configures buildroot with C++ support
2. Builds the ARM cross-compiler toolchain (first run only, ~30-60 min)
3. Builds ALSA library headers
4. Compiles SC1000 using CMake

## What's Inside

- Ubuntu 18.04 (matches buildroot 2018.08 era)
- Buildroot 2018.08 with SC1000 config
- CMake for the build system
- ARM cross-compiler targeting Cortex-A8 with NEON
- uClibc C library (matches device)

## Build Output

The compiled binary will be at:
```
software/build-buildroot/sc1000
```

To deploy to the device, copy to USB stick:
```bash
# From host machine with USB stick mounted:
cp software/build-buildroot/sc1000 /media/username/USB_STICK/sc1000
```

## Verifying the Binary

A correctly built binary should show:
```
$ file build-buildroot/sc1000
ELF 32-bit LSB executable, ARM, EABI5 version 1 (SYSV),
dynamically linked, interpreter /lib/ld-uClibc.so.0, ...
```

Key indicators:
- `interpreter /lib/ld-uClibc.so.0` - uses uClibc (correct)
- NOT `interpreter /lib/ld-linux-armhf.so.3` - that's glibc (wrong)

## Build Times

| Step | First Build | Subsequent |
|------|-------------|------------|
| Docker image | ~2 min | Cached |
| Toolchain | 30-60 min | Cached |
| ALSA lib | ~2 min | Cached |
| SC1000 | ~30 sec | ~30 sec |

Buildroot caches everything in Docker volumes, so only SC1000 itself
is recompiled on subsequent builds.

## Manual Build Steps

If you need more control, here are the manual steps inside the container:

```bash
# 1. Setup buildroot config (one-time)
cd /home/builder/buildroot-2018.08
cp /home/builder/sc1000/os/buildroot/buildroot_config .config

# 2. Enable C++ (one-time)
sed -i 's/# BR2_TOOLCHAIN_BUILDROOT_CXX is not set/BR2_TOOLCHAIN_BUILDROOT_CXX=y/' .config
make olddefconfig

# 3. Build toolchain (one-time, ~30-60 min)
make toolchain

# 4. Build ALSA headers (one-time)
make alsa-lib

# 5. Build SC1000 with CMake
cd /home/builder/sc1000/software
mkdir -p build-buildroot && cd build-buildroot
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/buildroot-uclibc.cmake \
      -DBUILDROOT_DIR=/home/builder/buildroot-2018.08 \
      -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

## Troubleshooting

### Build fails with download errors
Buildroot 2018.08 has some outdated package URLs. The toolchain and alsa-lib
should still work, but if you encounter issues:
1. Check buildroot-2018.08/dl/ for partially downloaded files
2. Manually download the package and place in dl/

### Binary won't run on device
Check interpreter with: `readelf -l sc1000 | grep interpreter`
- Should show `/lib/ld-uClibc.so.0`
- If it shows `/lib/ld-linux-armhf.so.3`, you used the wrong toolchain

### CMake can't find compiler
Make sure you're inside the Docker container and the toolchain was built:
```bash
ls /home/builder/buildroot-2018.08/output/host/bin/arm-linux-g++
```

## Deployment

### Quick Deploy to USB Stick

The `deploy.sh` script handles the entire build and deploy process:

```bash
# Auto-detect USB stick, build and copy
./deploy.sh

# Skip build, just copy existing binary
./deploy.sh --no-build

# Also create sc.tar for full system update
./deploy.sh --tar

# Specify USB mount point manually
./deploy.sh --usb /media/username/MYUSB
```

### Update Methods

**Method 1: USB Override (Simplest)**
1. Copy binary as `sc1000` to USB stick root
2. Copy `sc_settings.json` to USB stick root
3. Insert USB, power on device
4. Device automatically runs from USB

**Method 2: Full System Update (via sc.tar)**
1. Create update package: `../updater/buildupdater.sh`
2. Copy `sc.tar` to USB stick root
3. Insert USB, hold buttons while powering on
4. Wait for success audio feedback

### Manual Deployment

```bash
# Copy binary to USB stick
docker cp CONTAINER:/home/builder/sc1000/software/build-arm/sc1000 /media/usb/sc1000
cp software/sc_settings.json /media/usb/
```

## Device Notes

- **SC1000**: Full device with beat deck volume ADC
- **SC500**: Simplified version, auto-detected via GPIO 6:11

The same binary works on both devices - the SC500 detection is automatic.

## Building Complete OS Image

To build a fresh SD card image with updated kernel, rootfs, and overlay:

```bash
# Build complete OS image (takes 30-60 minutes first time)
./build-os.sh

# Resume build using existing container
./build-os.sh --resume

# Clean build (remove cached artifacts)
./build-os.sh --clean

# Only build toolchain (faster first-time setup)
./build-os.sh --toolchain

# Run menuconfig for configuration changes
./build-os.sh --menuconfig
```

### What Gets Built

| Component | Version | Source |
|-----------|---------|--------|
| U-Boot | 2017.05 | A13-OLinuXino defconfig |
| Linux Kernel | 4.17.19 | sunxi defconfig |
| Root Filesystem | ext2 60MB | buildroot packages |
| Overlay | - | os/buildroot/sc1000overlay |

### Output Files

After a successful build:
```
os/
├── sdcard.img.gz         # Complete SD card image (flash this)
├── zImage                # Linux kernel
├── sun5i-a13-olinuxino.dtb   # Device tree
├── u-boot-sunxi-with-spl.bin # Bootloader
└── rootfs.tar            # Root filesystem tarball
```

### Flashing to SD Card

```bash
# Using dd (Linux)
gunzip -c os/sdcard.img.gz | sudo dd of=/dev/sdX bs=4M status=progress

# Or use Raspberry Pi Imager / Balena Etcher with sdcard.img.gz
```

**WARNING**: Replace `/dev/sdX` with your actual SD card device. Double-check with `lsblk` to avoid overwriting your system drive.

### Overlay Contents

The `os/buildroot/sc1000overlay` directory is merged into the root filesystem:

```
sc1000overlay/
├── etc/
│   ├── init.d/S50sc1000  # Init script (waits for USB/ALSA)
│   └── asound.state      # ALSA mixer defaults
├── root/
│   ├── sc1000-import     # Track import script
│   └── libexec/sc1000-import
└── usr/bin/sc1000        # Default binary (replaced by update)
```

### Modifying the OS

1. Edit files in `os/buildroot/sc1000overlay/`
2. Run `./build-os.sh` to rebuild
3. Flash new `sdcard.img.gz` to SD card

For buildroot package changes:
```bash
./build-os.sh --menuconfig
# Make changes, save, exit
./build-os.sh
```

## Release Workflow

To build and commit prebuilt binaries for distribution:

```bash
# Build binary and update prebuilt/ directory
./release.sh

# Skip build, just repackage existing binary
./release.sh --no-build
```

### What release.sh Does

1. Finds running buildroot Docker container
2. Syncs source code to container
3. Builds ARM binary with CMake (Release mode)
4. Copies binary to `prebuilt/sc1000`
5. Creates update tarball via `buildupdater.sh`
6. Copies tarball to `prebuilt/sc.tar`

### Prebuilt Directory

After running `release.sh`:

```
prebuilt/
├── sc1000       # ARM binary (uClibc, ready for device)
├── sc.tar       # Complete update package
└── README.md    # Usage instructions
```

### Committing a Release

```bash
./release.sh
git add prebuilt/
git commit -m 'Update prebuilt binaries'
git push
```

### Quick Deploy from Prebuilt

For users who just want to run the latest version:

```bash
# Copy to USB stick
cp prebuilt/sc1000 /media/USB_STICK/
cp software/sc_settings.json /media/USB_STICK/

# For full system update (init script, importer)
cp prebuilt/sc.tar /media/USB_STICK/
# Hold buttons during power-on to apply
```
