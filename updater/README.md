# Update SC1000

Besides just updating the `xwax` version, there's also a full firmware upgrade process.

> SC1000 v1.3 firmware update! (2020-06-02)
> https://www.youtube.com/watch?v=4zC_MFa_150


1. Download the firmware: http://rasteri.com/SC1000_V1.3.zip (might be outdated, when you read this).

   Or build yourself, see [../software/DEVELOPMENT.md](./../software/DEVELOPMENT.md).

2. Unpack the zip file in the root of the USB stick.

   Zip file contains:
   * `sc.tar`
     - some `*.mp3` files: for audio feedback during the update process
     - `sun5i-a13-olinuxino.dtb`: DeviceTreeBlob to change hardware config
     - `xwax`: the actual updated `xwax` software
     - `zImage`: compressed version of the linux kernel image
   * `scsettings.txt`
   * `xwax` replacement script (which triggers update, then calls actual `xwax`)

3. Install on SC

   Boot your SC1000 while holding down any (or both) of the beat selection buttons. Make sure you use headphones, because the result of the upgrade will be output via audio.

5. Reboot

   You might need to remove the `xwax` replacement script from the USB stick

   To _hear_ the firmware version: hold down sample select up during boot


## Build new firmware version

#### Create new `os-version.mp3`

e.g. use https://ttsmp3.com/
