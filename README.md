# SC1000 (v2)
## Open-source portable digital scratch instrument

This github holds source code and CAM files for the SC1000.

The SC1000 is a portable digital scratch instrument which loads samples and beats from a USB stick. At less than the size of three stacked DVD cases, it's probably the smallest integrated portablist solution ever. Despite this, the software and hardware have been carefully tuned and optimised, and it's responsive enough for even the most complex scratch patterns.

The device, including its enclosure, uses no custom parts apart from printed circuit boards. It will be possible for anyone with a bit of electronics know-how to build one, and I hope other makers in the portablist scene will be interested in manufacturing some.

The build tutorial video can be found here : https://www.youtube.com/watch?v=t1wy7IFSynY

---

## SC1000 Software v2 (C++ Rewrite)

The software in the `software/` folder has been completely rewritten and modernized. This is a major update from the original C-based xwax fork.

### What's New

- **Loop recording** with punch-in overdub and per-deck control (via external audio interface)
- Simple **Auto-cue mode** for beat slicing (divide track into 4/8/16/32 equal parts)
- **High-quality sinc interpolation** (16-tap, anti-aliased)
- **Float-based audio engine** with native device bit-depth support for I/O
- **CV outputs** for modular synthesis integration (via external audio interface)
- **Multi-device audio routing** with per-device configuration
- **Extensive logging system** with file output, DSP load to terminal etc
- **JSON configuration** for easy customization
- **CMake build system** with Docker cross-compilation support
- **C++17 codebase** with modern patterns (RAII, namespaces, smart pointers, hardware abstraction)

For more info see below (section [Software Features Version 2](#software-features-version-2))

### Compatibility Note

**The new software is a drop-in binary replacement.** It runs on the existing SD card Linux image - no OS update required. Simply copy the `sc1000` binary to the USB stick root - the device will run it automatically. See [Deploying to Device](#deploying-to-device) for details.

---

## Usage ##

Simply switch on SC1000 with a valid USB stick in, and after a few seconds it will start playing the first beat and sample on the USB stick. Plug in some headphones or a portable speaker, adjust the volume controls to your liking, and get skratchin!

*Pressing* the **beat/sample down** button will select the next file in the current folder, and *holding* the button will skip to the next folder.

Note that you shouldn't touch the jog wheel while you are turning the device on - this is because the SC1000 does a short calibration routine. Leave it a few seconds before touching it.


## USB Folder layout ##

The SC1000 expects the USB stick to have two folders on it - **beats** and **samples**. Note that the names of these folders *must* be in all-lowercase letters.

The beats and samples folders should in turn contain a number of subfolders, to organise your files into albums. Each of these subfolders should contain a number of audio files, in **mp3** or **wav** format. For example, you might have a folder layout like :

* beats/Deluxe Shampoo Breaks/beat1.mp3
* beats/Deluxe Shampoo Breaks/beat2.mp3
* beats/Deluxe Shampoo Breaks/beat3.mp3
* beats/Gag Seal Breaks/beat1.wav
* beats/Gag Seal Breaks/beat2.wav
* beats/Gag Seal Breaks/beat3.wav
* samples/Super Seal Breaks/01 - Aaaah.wav
* samples/Super Seal Breaks/02 - Fresh.wav
* samples/Enter the Scratch Game vol 1/01 - Aaaah Fresh.wav
* samples/Enter the Scratch Game vol 1/02 - Funkyfresh Aaaah.wav
* samples/Enter the Scratch Game vol 1/03 - Funkydope Aaaah.wav

Optionally, you can put an updated version of the software (`sc1000` binary) on the root of the USB stick, and the SC1000 will run it instead of the internal version. This gives a very easy way to update the software on the device. See [Deploying to Device](#deploying-to-device) for details.

![SC implementation chart](http://rasteri.com/SC1000_MIDI_chart.png)


[![Demo Video](https://img.youtube.com/vi/ReuCnZciOf4/0.jpg)](https://www.youtube.com/watch?v=ReuCnZciOf4)

## Repository Structure ##

* **Firmware** - Source code for the input processor. This handles the pots, switches and capacitive touch sensor, and passes the information on to the main processor.
* **OS** - SD card images and buildroot configs for the operating system that runs on the main processor.
* **Software** - Source code for the modified version of xwax running on the main SoC.
* **Hardware** - Schematics and gerbers for the main PCB and enclosure (which is made of PCBs)


## Tech Info ##

The device is based around the Olimex A13-SOM-256 system-on-module, which in turn uses an Allwinner A13 ARM Cortex A8 SoC. The sensing of the scratch wheel is handled by an Austria Microsystems AS5601 magnetic rotary sensor, and the other inputs are processed via a Microchip PIC18LF14K22 MCU. The whole unit is powered via USB, and optionally includes the ability to fit a power bank inside the enclosure.


## Build guide :

### Assembly video ###

A video covering most of this information can be found at https://www.youtube.com/watch?v=t1wy7IFSynY


### Ingredients

* **Main PCB and components** - Board files are in [hardware](./hardware) and can be ordered from somewhere like https://jlcpcb.com/
* **Components** - Bill of Materials is in [hardware/](./hardware) and can be ordered from Mouser
* **A13 System-on-Module** - Available from https://www.olimex.com/Products/SOM/A13/A13-SOM-256/, connects to the main PCB via 0.05" headers
* **SD Card** - To hold the operating system. It only needs 200Mb so just get the smallest card you can find
* **Enclosure parts** - The enclosure is made from PCBs and aluminium supports. Gerber files are in *./hardware/gerbers/Enclosure/*, aluminium supports are 20x10x156.8mm. The front and rear plates should be 1mm thick, the rest should be 1.6mm. I got mine from https://www.aluminiumwarehouse.co.uk/20-mm-x-10-mm-aluminium-flat-bar, they even cut it for me.
* **Jogwheel parts** - The jogwheel itself is a gold-plated PCB, available in [hardware/gerbers/Jog Wheel](./hardware/gerbers/Jog%20Wheel). Mine is made from 0.6mm thick board, you can choose the thickness you prefer. You'll also need M8 bearing/hex bolt/nuts/washers, and a diametrically polarized magnet from https://www.kjmagnetics.com/proddetail.asp?prod=D42DIA-N52. The bearing I used is available at https://uk.rs-online.com/web/p/ball-bearings/6189957/
* **Mini innoFADER** - the OEM model (for example found in the innoFADER Mini DUO pack) is fine, but a Mini innoFADER Plus has  better performance


### Method ###

* **Order** the Main and Enclosure PCBs, the components, the A13 SoM, and SD Card, and the Aluminium bar. I recommend using ENIG coating for the Jogwheel as you don't really want to be touching solder all day.

* **Assemble the Main PCB.** I recommend assembling/testing the 3.3v power section first, so you don't blow all the other components. Don't connect the A13 module yet.

* **Flash the input processor with its firmware** through connector J8. You will need a PIC programmer, such as the Microchip Pickit 3. The firmware hex file is [firmware/firmware.hex](./firmware/firmware.hex)

* **Transfer the operating system to the SD card.** You will need an SD card interface, either USB or built-in to your PC. You can use dd on Linux/MacOS or Etcher on Windows to transfer the image. The image is [os/sdcard.img.gz](./os/sdcard.img.gz)

* **Insert the SD card in the A13 module, and attach the SoM to the main PCB.** Make sure it's the correct way round - the SD card should be right beside the USB storage connector on the rear of the SC1000.

* **Connect a USB power source, and power up the unit to test** - the A13 module's green light should blink a few times before remaining on.

* **Assemble the jogwheel** - glue the bearing into the hole in the top plate of the enclosure. Now glue the magnet to the tip of the M8 bolt. Attach the jogwheel to the bearing using the bolt/nut/washer. Solder a wire to the outside of the bearing to act as a capacitive touch sensor.

* **Connect** the fader to J1, capacitive touch sensor to J4, and (optionally) a small internal USB power bank to J3. If you don't use an internal power bank, put two jumpers horizontally across J3 to allow the power to bypass it.

* **Test** - copy some beats and samples to a USB stick, and see if they play. Check below for how to structure the folders on the USB stick.

* **Assemble the enclosure** - drill and tap M3 holes in the aluminium, and screw the whole enclosure together. Make sure the magnet at the end of the jogwheel bolt is suspended directly above the rotary sensor IC.


---

## Software Features Version 2 ##

### Loop Recording (Memory Sampler)

Record audio input into an in-memory buffer that can be immediately scratched, looped and played via MIDI. This feature needs an external audio interface connected to the USB port and a valid device configuration for the input channels, see [Multi-Device Audio Configuration](#multi-device-audio-configuration).

**Controls (per deck):**
| Action | How |
|--------|-----|
| Start/Stop Recording | SHIFT + PLAY (release to trigger) |
| Erase Loop | SHIFT + PLAY (hold ~1 sec) |
| Recall Loop | After loading another track, recall last recorded loop |

**Recording workflow:**
1. **First SHIFT+PLAY release**: Starts capturing audio from input
2. **Second SHIFT+PLAY release**: Stops recording, defines loop length, loads onto deck
3. **Subsequent SHIFT+PLAY releases**: Punch-in mode - overwrites from current position while preserving loop length
4. **Hold SHIFT+PLAY (~1 sec)**: Erases the loop, next recording starts fresh

**Behavior:**
- Recording appears as the first track in the playlist (position 0)
- While recording, input audio is monitored through the deck's volume control
- Scratching the deck is possible during both looping and recording
- Maximum loop duration is configurable in settings (`loop_max_seconds`, default 60 sec), this is pre-allocated per deck at initialization

---

### CV Outputs

Control voltage outputs for modular synthesis integration. This feature needs an external audio interface connected to the USB port and a valid device configuration for the output channels, see [Multi-Device Audio Configuration](#multi-device-audio-configuration).

**Available CV signals:**
| Signal | Description |
|--------|-------------|
| Platter Angle | Current rotational position (0-1) |
| Platter Speed | Playback speed (-1 to +1, with filtering) |
| Sample Position | Position in current track (0-1) |
| Crossfader | Crossfader position (0-1) |
| Gate A | High when scratch deck side is open |
| Gate B | High when beat deck side is open |
| Direction Pulse | Trigger pulse on platter direction change |

**Configuration:**
```json
{
  "cv_mapping": {
    "platter_speed": 2,
    "platter_angle": 3,
    "sample_position": 4,
    "crossfader": 5,
    "gate_a": 6,
    "gate_b": 7,
    "direction_pulse": 8
  }
}
```

---

### Audio Engine

The audio engine has been rewritten for quality and performance:

- **Float-based mixing**: All internal processing uses 32-bit float
- **Native device resolution**: Supports 16-bit, 24-bit, and 32-bit I/O
- **Automatic dithering**: Applied when outputting to 16-bit (internal audio interface)
- **Vectorized processing**: Optimized for ARM NEON (auto-)vectorization
- **CPU usage**: ~7% with cubic interpolation, ~9% with sinc interpolation

---

### Sinc Interpolation

High-quality 16-tap windowed sinc resampling with anti-aliasing for pitch-shifted playback.

**Anti-aliasing behavior:**
- Bandwidth automatically adjusts based on playback speed
- At pitch ≤ 1.0: full bandwidth (no filtering needed)
- At pitch 1.0-2.0: 0.5 cutoff
- At pitch > 2.0: aggressive 0.25 cutoff for fast scratching

---

### Auto-Cue Mode (Beat Slicer)

Automatic cue point generation that divides the track into equal parts. Useful for beat slicing and sample chopping via MIDI.

**Controls:**
| Combo | Action |
|-------|--------|
| Cue buttons 1+2 (release both) | Cycle scratch deck auto-cue mode |
| Cue buttons 3+4 (release both) | Cycle beat deck auto-cue mode |

**Mode cycle:** Off → 4 divisions → 8 divisions → 16 divisions → 32 divisions → Off

**Behavior:**
- Pressing any cue button jumps to `position = (track_length / divisions) × (cue_number % divisions)`
- MIDI notes on channel 2 (scratch) or channel 3 (beat) trigger cue positions
- Mode resets to Off when loading a new track
- Jumps are artifact-free even while scratching (encoder syncs automatically)

**Example with Div8 mode:**
- Cue 0 → 0% of track
- Cue 1 → 12.5% of track
- Cue 2 → 25% of track
- ...
- Cue 7 → 87.5% of track
- Cue 8 → wraps to 0% (same as cue 0)

---

### JSON Configuration

Settings are stored in JSON format (`sc_settings.json`) for easy editing and extensibility.

Key configuration areas:
- Audio device routing
- MIDI controller mappings
- GPIO button mappings
- CV output assignments
- Crossfader calibration and behavior (VCA mode, cut mode, hamster switch)
- Platter speed/reverse/brake settings
- Loop recording parameters

---

### MIDI Control

Works as before. Full MIDI controller support with configurable mappings.

**Supported message types:**
- Note on/off (with velocity)
- Control Change (CC)
- Pitch bend (14-bit resolution for fine pitch control)

**NOTE action:** MIDI notes can trigger pitch changes using equal temperament tuning (middle C = 1.0x pitch). Useful for melodic scratching.

---

### Multi-Device Audio Configuration

Per-device settings with flexible routing for inputs, outputs, and CV mappings.

**Device matching:**
- Devices matched by USB device name
- Use `"*"` to match any USB audio device
- Multiple device configurations supported in settings

**Input/Output routing:**
```json
{
  "audio_devices": [{
    "name": "USB Audio Device",
    "input_channels": 2,
    "input_left": 0,
    "input_right": 1,
    "output_channels": 2,
    "output_left": 0,
    "output_right": 1
  }]
}
```

---
### Command-Line Options

```bash
./sc1000 [OPTIONS]

Options:
  --root PATH            Root directory for samples/settings (default: /media/sda)
  --log-console          Log to console (default)
  --log-file             Log to {root}/sc1000.log
  --log-file-path PATH   Log to specified file path
  --log-level LEVEL      Set log level (debug, info, warn, error)
  --show-stats           Enable DSP load meter output
  --cubic                Use cubic interpolation (faster, no anti-aliasing)
  --sinc                 Use sinc interpolation (default, anti-aliased)
  --help                 Show help message
```

**Examples:**
```bash
# Desktop development with local samples
./sc1000 --root ~/Music/samples --log-console --log-level debug

# Production with file logging
./sc1000 --root /media/sda --log-file --log-level info

# Performance testing with cubic interpolation
./sc1000 --cubic --show-stats
```

---

### Logging System

Configurable logging with multiple output targets and severity levels. You can also monitor the DSP/CPU use by enabling the flag `--show-stats`

**Log levels:**
| Level | Description |
|-------|-------------|
| `debug` | Verbose debugging information |
| `info` | General operational messages |
| `warn` | Warning conditions |
| `error` | Error conditions |

**Output targets:**
- **Console**: Default, logs to stdout
- **File**: Logs to `{root}/sc1000.log` or custom path

---

### Building the Software

**Native build (for development):**
```bash
cd software
cmake -B cmake-build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build cmake-build-debug
```

**Cross-compilation for SC1000 hardware:**

The device uses uClibc (not glibc), requiring the Buildroot toolchain. A Docker environment is provided for reproducible builds:

```bash
cd docker
./build-docker.sh          # One-time: build Docker image with toolchain
./run-docker.sh            # Enter build environment
./sc1000/docker/build-sc1000.sh  # Build inside container
```

Output: `software/build-buildroot/sc1000`

**Updating prebuilt binaries (for maintainers):**
```bash
cd docker
./release.sh              # Build and update prebuilt/ directory
git add prebuilt/
git commit -m 'Update prebuilt binaries'
```

See `docker/README.md` for full build details.

---

### Deploying to Device

There are two ways to update the SC1000 software:

#### Using Prebuilt Binaries (Easiest)

Ready-to-use binaries are available in the `prebuilt/` directory:

```bash
# Copy to USB stick
cp prebuilt/sc1000 /media/USB_STICK/
cp software/sc_settings.json /media/USB_STICK/

# For full system update (includes init script, importer)
cp prebuilt/sc.tar /media/USB_STICK/
# Hold buttons during power-on to apply
```

#### Method 1: USB Stick Override (Recommended for Development)

The simplest method - the device automatically runs binaries from the USB stick instead of the internal version.

1. **Build** the binary using Docker (see above)
2. **Copy** files to USB stick root:
   - `sc1000` (the binary)
   - `sc_settings.json` (configuration)
3. **Insert** USB stick and power on device

The device checks for `/media/sda/sc1000` at boot and runs it if present.

**Using the deploy script:**
```bash
cd docker
./run-docker.sh          # Start build container (if not running)
./deploy.sh              # Build, copy to USB, unmount

# Options:
./deploy.sh --no-build   # Skip build, just copy existing binary
./deploy.sh --usb /path  # Specify USB mount point
./deploy.sh --tar        # Also create sc.tar for full system update
```

#### Method 2: Full System Update (via sc.tar)

For permanent installation or when updating kernel/device tree:

1. **Create** update package:
   ```bash
   cd updater
   ./buildupdater.sh
   ```

2. **Copy** `sc.tar` to USB stick root

3. **Hold update buttons** while powering on:
   - **SC500**: Either beat button
   - **SC1000**: Specific button combination (check PIC register 0x69:0x05)

4. **Wait** for success audio feedback

The update extracts:
- `sc1000` → `/usr/bin/`
- `S50sc1000` → `/etc/init.d/` (init script)
- `sc1000-import` → `/root/` (audio importer)
- `sc_settings.json` → `/media/sda/`
- `zImage`, `*.dtb` → boot partition (if present in tarball)

**Legacy migration:** The updater automatically removes old xwax files:
- `/usr/bin/xwax` → removed
- `/etc/init.d/S50xwax` → removed (replaced by S50sc1000)
- `/root/xwax-import` → removed (replaced by sc1000-import)

#### Binary Priority Order

The init script checks for executables in this order:
1. `/media/sda/sc1000` (USB stick)
2. `/usr/bin/sc1000` (system)

---

### Running the Software

The SC1000 doesn't automatically run the binary just by placing it on the USB stick - it needs to be started by the init system or manually via serial console.

#### Automatic Startup (via Init Scripts)

The init script `/etc/init.d/S50sc1000` runs at boot and:
1. Waits for USB stick to mount at `/media/sda`
2. Checks for `sc1000` binary (USB stick first, then system)
3. Launches it with appropriate arguments

**To get the init scripts on your device:**

- **New devices**: Flash the SD card image from `os/sdcard.img.gz`
- **Existing devices**: Use the updater (`sc.tar`) which installs `S50sc1000`

Without the init script, the binary won't start automatically - you'll need to run it manually.

#### Manual Startup (via Serial Console)

For development, debugging, or devices without updated init scripts, connect via serial console:

**Hardware connection:**
- Serial pins on the A13-SOM: TX, RX, GND (active 3.3V UART)
- Use a USB-to-serial adapter (FTDI, CP2102, etc.)
- Settings: **115200 baud, 8N1**

**Connect and run:**
```bash
# On your development machine
screen /dev/ttyUSB0 115200
# or
minicom -D /dev/ttyUSB0 -b 115200

# Once connected to the SC1000, log in as root (no password)
# Then run manually:
/media/sda/sc1000 --root /media/sda --log-console
```

**Stopping the auto-started instance:**
```bash
# If init script is running sc1000, kill it first
killall sc1000

# Then run your version
/media/sda/sc1000 --log-console --log-level debug
```

**Useful for:**
- Testing new builds without rebooting
- Debugging with verbose logging
- Running with different command-line options
- Devices that don't have updated init scripts yet

#### Quick Reference: What's Needed for Auto-Start

| Component | Location | Purpose |
|-----------|----------|---------|
| `sc1000` binary | `/media/sda/` or `/usr/bin/` | The main application |
| `S50sc1000` | `/etc/init.d/` | Init script that starts the binary |
| `sc_settings.json` | `/media/sda/` | Configuration file |
| `sc1000-import` | `/root/` | Audio file importer (called by sc1000) |

**If auto-start doesn't work:**
1. Check that `S50sc1000` exists in `/etc/init.d/`
2. If missing, either use the updater (`sc.tar`) or run manually via serial

---

### Building Complete OS Image

To build a fresh SD card image with updated kernel, filesystem, and init scripts:

```bash
cd docker
./build-os.sh
```

This uses Docker to build the complete buildroot system:
- **U-Boot** 2017.05 bootloader
- **Linux Kernel** 4.17.19 (sunxi)
- **Root Filesystem** with SC1000 overlay
- **Output**: `os/sdcard.img.gz`

**Flash to SD card:**
```bash
gunzip -c os/sdcard.img.gz | sudo dd of=/dev/sdX bs=4M status=progress
```

See [docker/README.md](docker/README.md) for detailed build options and configuration.

---

### Codebase Modernization

The software has been refactored from C to C++17:

- **Modular architecture**: Code organized into `src/core/`, `src/engine/`, `src/platform/`, `src/player/`, `src/control/`
- **RAII patterns**: Smart pointers for resource management
- **Namespaces**: `sc::audio`, `sc::config`, `sc::platform`, `sc::control`
- **CMake build system**: Replaced legacy Makefile, supports native and cross-compilation
- **Type-safe mappings**: O(1) lookup for MIDI and GPIO events


## License ##

Copyright (C) 2018 Andrew Tait <rasteri@gmail.com>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 2, as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License version 2 for more details.

You should have received a copy of the GNU General Public License
version 2 along with this program; if not, write to the Free
Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
MA 02110-1301, USA.
