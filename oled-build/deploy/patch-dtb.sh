#!/bin/sh
# Regenerate oled-build/deploy/sun5i-a13-olinuxino.dtb from the stock device
# tree shipped in updater/tarball/, adding "clock-frequency = <400000>" to the
# i2c-0 controller node (i2c@1c2ac00).
#
# Why: i2c-0 carries both the jog-wheel encoder (polled ~730 Hz) and the OLED.
# At the kernel default of 100 kHz the jog poll eats most of the bus and the
# audio warbles whenever the display refreshes.  400 kHz gives enough headroom.
#
# Needs: dtc (device-tree-compiler).  On macOS: brew install dtc
set -e
cd "$(dirname "$0")/../.."                       # -> SC1000 source root

STOCK=updater/tarball/sun5i-a13-olinuxino.dtb
OUT=oled-build/deploy/sun5i-a13-olinuxino.dtb
TMP=$(mktemp -d)

dtc -I dtb -O dts -o "$TMP/stock.dts" "$STOCK" 2>/dev/null

awk '
  /i2c@1c2ac00 \{/                                     { inblk = 1 }
  inblk && /compatible = "allwinner,sun4i-a10-i2c";/   { print; print "\t\t\tclock-frequency = <0x61a80>;"; next }
  /^\t\t\};/ && inblk                                   { inblk = 0 }
  { print }
' "$TMP/stock.dts" > "$TMP/patched.dts"

dtc -I dts -O dtb -o "$OUT" "$TMP/patched.dts" 2>/dev/null
rm -rf "$TMP"

echo "wrote $OUT"
dtc -I dtb -O dts "$OUT" 2>/dev/null | sed -n '/i2c@1c2ac00/,/pinctrl-0/p'
