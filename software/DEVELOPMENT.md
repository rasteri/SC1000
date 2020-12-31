# Development setup

_DISCLAIMER: This developer help was written with very limited beginner knowledge in all these topics. Take it with caution!_
_I figured out all this by try and error and i'm sure there are better ways of doing it. Still hope this helps someone who wants to tinker around in the future. Corrections appreciated. – @psturm_


## install buildroot env

As preparation, create ARM buildroot to be able to compile for the SC..

```sh
# download and unpack
wget https://buildroot.org/downloads/buildroot-2018.08.4.tar.gz
mv ./buildroot-2018.08.4.tar.gz /opt
cd /opt
tar xfv buildroot-2018.08.4.tar.gz

# prepare config & overlay
cd <SC1000_PROJECT_PATH>
cp ./os/buildroot/buildroot_config /opt/buildroot-2018.08.4/.config
cp -R ./os/buildroot/sc1000overlay /opt/buildroot-2018.08.4/

# build
cd /opt/buildroot-2018.08.4/
make  # takes some time ..
```


## build `xwax`

Requires a buildroot environment (see setion above).

To build the `xwax` binary from source:

```sh
cd ./software

# Adjust path in Makefile to your buildroot:
# CC=/opt/buildroot-2018.08.4/output/host/usr/bin/arm-linux-gcc

make clean && make xwax
```

In my case, this still created wrong paths in the binary. As a quick fix, i created a symlink `/root/buildroot-2018.08.4 -> /opt/buildroot-2018.08.4` and used this in Makefile: `CC=/root/buildroot-2018.08.4/output/host/usr/bin/arm-linux-gcc` (there are better ways to do this for sure).


## debugging SC1000

Needs a USB-TTL-serial-converter to connect to UART of main processor on the SC1000.

Check "6.1.1 UART1 interface" in https://www.olimex.com/Products/SOM/A13/A13-SOM-512/resources/A13-SOM-um.pdf (page 24)

Hint: don't forget to cross RX & TX!

To connect:
`screen /dev/ttyUSB0 115200` (Adjust the USB port to your actual one)

or maybe use `GtkTerm` 


## DeviceTreeBlob (dtb)

To change hardware mappings (e.g. PIN layouts)

```sh
cd ./updater/tarball/

# binary -> source
dtc -I dtb -O dts sun5i-a13-olinuxino.dtb > sun5i-a13-olinuxino.dts

# Change the source file to your needs

# source -> binary
dtc -O dtb -o sun5i-a13-olinuxino.dtb -b 0 sun5i-a13-olinuxino.dts
```



Customized DTS for the LED Mod:
* Remove power LED on PG9
* Enable uart3 (RX:PG9, TX:PG10), results in `/dev/ttyS1` on SC1000 (see `led_mod.c`)
