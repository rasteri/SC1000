# SSD1306 OLED waveform display for the SC500 / SC1000

Adds a 128×64 I²C OLED to the SC1000 firmware. It shows a **zoomed, scrolling
waveform** of the sample track played with the jog platter (`deck[1]`): the
waveform scrolls left/right under a fixed centre cursor as you move the jog
wheel, with **cue points as markers** and a **whole-track progress bar** along
the bottom. The waveform body is shaded with a 4×4 ordered dither so it reads
as a filled shape on a 1‑bit panel, with a crisp top edge.

Tested on an **SC500** (Allwinner A13, Olimex A13‑SOM‑512, kernel 4.17,
Buildroot 2018.08 / uClibc‑ng). The SC1000 uses the same SoC and firmware.

## What changed in the firmware

| File | Change |
|------|--------|
| `software/sc_display.c` | **new** – the whole driver (SSD1306 init, framebuffer, waveform renderer, display thread) |
| `software/sc_display.h` | **new** – `SC_Display_Start()` prototype |
| `software/sc_input.c` | adds `i2c0_lock` mutex; wraps the AS5601 jog read so the display and the jog poll never collide on `/dev/i2c-0` |
| `software/xwax.c` | calls `SC_Display_Start()` after `SC_Input_Start()` (no‑op if no panel is fitted) |
| `software/Makefile` | adds `sc_display.o` to `OBJS` |

If no display is connected the driver logs one failure and the thread exits;
the rest of xwax is unaffected.

## Wiring

The OLED shares **`/dev/i2c-0`** with the AS5601 jog sensor (address `0x36`);
the panel answers at `0x3C` (try `0x3D` if `i2cdetect -y 0` shows it there).

| OLED pin | SC500 connection |
|----------|------------------|
| `SCL` | **R13 pad (ROT_SCL)**, next to the rotary sensor |
| `SDA` | **R12 pad (ROT_SDA)** |
| `VCC` | 3.3 V (e.g. **J7 pin 2**) |
| `GND` | GND (e.g. **J7 pin 1**) |

> **Why not the J7 expansion I²C (`i2c-1`)?** On this A13 kernel an I²C bus
> with no other users goes "cold" and every transfer times out once xwax's
> realtime audio thread is running. `i2c-0` stays alive only because the jog
> is polled hundreds of times a second, so the display has to ride on it.
> To keep the jog read (and therefore the scratch audio) clean, every
> `i2c-0` access from the display is taken under `i2c0_lock` and split into
> ≤24‑byte chunks, so `process_rot()` never waits behind more than one short
> transfer.

## Required: bump `i2c-0` to 400 kHz (device‑tree patch)

At the kernel default of **100 kHz** the jog poll alone uses ~70 % of the bus;
adding the display makes the audio warble on slow platter moves. The fix is a
one‑property change to the device tree — add `clock-frequency = <400000>` to
the `i2c@1c2ac00` node.

`deploy/patch-dtb.sh` regenerates the patched blob from the stock device tree
already in this repo (`updater/tarball/sun5i-a13-olinuxino.dtb`); the result is
committed at `deploy/sun5i-a13-olinuxino.dtb`. It needs `dtc`
(`brew install dtc` / `apt install device-tree-compiler`).

This is a **DTB‑only** change: it does not touch the kernel or `/usr/bin/xwax`,
so re‑flashing the stock `updater/sc.tar` reverts it completely.

## Building the ARM binary

`oled-build/xwax.bin` in this repo is a pre‑built binary
(`armv7-a`, EABI hardfloat, uClibc‑ng, `NEEDED = libasound.so.2, libc.so.0`
— same ABI as the stock firmware). To rebuild it:

```sh
sh oled-build/build.sh
```

Needs Docker. On macOS:

```sh
brew install colima docker
colima start --vm-type=vz --vz-rosetta --cpu 4 --memory 6
```

`build.sh` builds a Debian image containing the Bootlin
`armv7-eabihf--uclibc--stable-2018.11-1` toolchain (= Buildroot 2018.08.1 /
gcc 7.3 / uClibc‑ng), cross‑compiles `alsa-lib 1.1.6` + `libiconv 1.15` into
its sysroot, builds xwax, and copies the result to `oled-build/xwax`.

## Deploying (USB stick)

Everything runs from the SC1000 USB stick — no reflash of the rootfs.

1. **One time – flash the 400 kHz device tree.** Build the tarball and copy it
   to the stick root:

   ```sh
   sh oled-build/deploy/make-tarball.sh     # -> oled-build/deploy/sc-oled.tar
   cp oled-build/deploy/sc-oled.tar   /Volumes/<STICK>/
   cp oled-build/deploy/xwax          /Volumes/<STICK>/xwax
   cp oled-build/xwax.bin             /Volumes/<STICK>/xwax.bin
   ```

   Then **hold a BEAT button while powering the SC500 on**. `deploy/xwax`
   detects the held button, un‑tars `sun5i-a13-olinuxino.dtb` onto the boot
   partition, and plays the "successful" prompt. Power‑cycle.

2. **Every boot after that.** With `xwax` + `xwax.bin` on the stick, the stock
   `etc/init.d/S50xwax` runs `/media/sda/xwax`, which (no button held) execs
   `/media/sda/xwax.bin`, logging stderr to `/media/sda/xwax.log`.

To go back to stock: delete `xwax` / `xwax.bin` from the stick (returns to
`/usr/bin/xwax`), and flash the original `updater/sc.tar` the same button‑held
way to restore the 100 kHz device tree.

## Files in `oled-build/`

```
Dockerfile              cross-build recipe (toolchain + alsa + xwax)
build.sh                one-shot: docker build + extract oled-build/xwax
xwax.bin                pre-built ARM binary (the OLED build)
deploy/xwax             USB-stick launcher (button-held = flash DTB; else run xwax.bin)
deploy/patch-dtb.sh     regenerate the 400 kHz device tree from updater/tarball/
deploy/make-tarball.sh  build deploy/sc-oled.tar (DTB + audio prompts)
deploy/sun5i-a13-olinuxino.dtb   patched device tree (i2c-0 @ 400 kHz)
deploy/sc-oled.tar      ready-to-flash tarball for the button-held path
```

## Tuning

Compile‑time knobs at the top of `software/sc_display.c`:

| define | meaning |
|--------|---------|
| `WF_WINDOW_SEC` | seconds of audio across the full screen width (zoom) |
| `WF_STYLE` | `0` filled · `1` silhouette · `2` shaded dither (default) |
| `WF_SHADE_TOP` / `WF_SHADE_EDGE` | dither density at the centre line vs. the peak edge |
| `WF_AUTOGAIN` | lift quiet samples so they still fill the height |
| `OLED_ROTATE180` | flip if the panel is mounted upside down |
| `OLED_ADDR` | `0x3C` / `0x3D` |
