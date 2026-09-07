#!/bin/sh
# Build sc-oled.tar : the tarball consumed by the deploy/xwax "hold a beat
# button at power-on" path.  It contains ONLY the patched device tree plus the
# audio prompts -- it does not replace the kernel or /usr/bin/xwax, so the
# change is trivially reverted by re-flashing the stock updater/sc.tar.
#
# Output: oled-build/deploy/sc-oled.tar  (copy it to the USB stick root)
set -e
cd "$(dirname "$0")/../.."                       # -> SC1000 source root

DTB=oled-build/deploy/sun5i-a13-olinuxino.dtb
[ -f "$DTB" ] || sh oled-build/deploy/patch-dtb.sh

STAGE=$(mktemp -d)
cp "$DTB"                             "$STAGE/sun5i-a13-olinuxino.dtb"
cp updater/tarball/successful.mp3     "$STAGE/"
cp updater/tarball/failed.mp3         "$STAGE/"

tar -C "$STAGE" -cf oled-build/deploy/sc-oled.tar \
    sun5i-a13-olinuxino.dtb successful.mp3 failed.mp3
rm -rf "$STAGE"

echo "wrote oled-build/deploy/sc-oled.tar"
tar -tvf oled-build/deploy/sc-oled.tar
