# Prebuilt Binaries

This directory contains prebuilt ARM binaries for the SC1000/SC500 devices.

## Files

- `sc1000` - The main application binary (ARM, uClibc)
- `sc.tar` - Complete update package for button-hold update

## Quick Deploy

Copy to USB stick:
```bash
cp sc1000 /media/YOUR_USB/
cp ../software/sc_settings.json /media/YOUR_USB/
```

Insert USB and power on device - it runs from USB automatically.

## Full System Update

For updating the init script and importer (migrates from legacy xwax naming):
```bash
cp sc.tar /media/YOUR_USB/
```

Hold update buttons while powering on:
- **SC500**: Either beat button
- **SC1000**: Button combo

## Rebuilding

To rebuild these binaries:
```bash
cd ../docker
./release.sh
```

Requires Docker with the buildroot container running.
