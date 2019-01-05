# SC1000
## Open-source portable digital scratch instrument

This github holds source code and CAM files for the SC1000.

The SC1000 is a portable digital scratch instrument which loads samples and beats from a USB stick. At less than the size of three stacked DVD cases, it’s probably the smallest integrated portablist solution ever. Despite this, the software and hardware have been carefully tuned and optimised, and it’s responsive enough for even the most complex scratch patterns.

The device, including its enclosure, uses no custom parts apart from printed circuit boards. It will be possible for anyone with a bit of electronics know-how to build one, and I hope other makers in the portablist scene will be interested in manufacturing some.

The build tutorial video can be found here : https://www.youtube.com/watch?v=t1wy7IFSynY

If you want to support development of the SC1000, please visit https://www.patreon.com/rasteri

[![Demo Video](https://img.youtube.com/vi/ReuCnZciOf4/0.jpg)](https://www.youtube.com/watch?v=ReuCnZciOf4)

The folders are as follows : 
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

* **Main PCB and components** - Board files are in [hardware/gerbers/Main PCB](./hardware/gerbers/Main%20PCB) and can be ordered from somewhere like https://jlcpcb.com/
* **Components** - Bill of Materials is in [hardware/docs/SC1000 - Bill Of Materials.xlsx](./hardware/docs/SC1000%20-%20Bill%20Of%20Materials.xlsx) and can be ordered from Mouser
* **A13 System-on-Module** - Available from https://www.olimex.com/Products/SOM/A13/A13-SOM-256/, connects to the main PCB via 0.05" headers
* **SD Card** - To hold the operating system. It only needs 200Mb so just get the smallest card you can find
* **Enclosure parts** - The enclosure is made from PCBs and aluminium supports. Gerber files are in *./hardware/gerbers/Enclosure/*, aluminium supports are 20x10x156.8mm. The front and rear plates should be 1mm thick, the rest should be 1.6mm. I got mine from https://www.aluminiumwarehouse.co.uk/20-mm-x-10-mm-aluminium-flat-bar, they even cut it for me.
* **Jogwheel parts** - The jogwheel itself is a gold-plated PCB, available in [hardware/gerbers/Jog Wheel](./hardware/gerbers/Jog%20Wheel). Mine is made from 0.6mm thick board, you can choose the thickness you prefer. You'll also need M8 bearing/hex bolt/nuts/washers, and a diametrically polarized magnet from https://www.kjmagnetics.com/proddetail.asp?prod=D42DIA-N52. The bearing I used is available at https://uk.rs-online.com/web/p/ball-bearings/6189957/
* **Mini innoFADER** - the OEM model (for example found in the innoFADER Mini DUO pack) is fine, but a Mini innoFADER Plus has  better performance


### Method ###

* **Order** the Main and Enclosure PCBs, the components, the A13 SoM, and SD Card, and the Aluminium bar.

* **Assemble the Main PCB.** I recommend assembling/testing the 3.3v power section first, so you don't blow all the other components. Don't connect the A13 module yet.

* **Flash the input processor with its firmware** through connector J8. You will need a PIC programmer, such as the Microchip Pickit 3. The firmware hex file is [firmware/firmware.hex](./firmware/firmware.hex)

* **Transfer the operating system to the SD card.** You will need an SD card interface, either USB or built-in to your PC. You can use dd on Linux/MacOS or Etcher on Windows to transfer the image. The image is [os/sdcard.img.gz](./os/sdcard.img.gz)

* **Insert the SD card in the A13 module, and attach the SoM to the main PCB.** Make sure it's the correct way round - the SD card should be right beside the USB storage connector on the rear of the SC1000.

* **Connect a USB power source, and power up the unit to test** - the A13 module's green light should blink a few times before remaining on.

* **Assemble the jogwheel** - glue the bearing into the hole in the top plate of the enclosure. Now glue the magnet to the tip of the M8 bolt. Attach the jogwheel to the bearing using the bolt/nut/washer. Solder a wire to the outside of the bearing to act as a capacitive touch sensor.

* **Connect** the fader to J1, capacitive touch sensor to J4, and (optionally) a small internal USB power bank to J3. If you don't use an internal power bank, put two jumpers horizontally across J3 to allow the power to bypass it.

* **Test** - copy some beats and samples to a USB stick, and see if they play. Check below for how to structure the folders on the USB stick.

* **Assemble the enclosure** - drill and tap M3 holes in the aluminium, and screw the whole enclosure together. Make sure the magnet at the end of the jogwheel bolt is suspended directly above the rotary sensor IC.


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

Optionally, you can put an updated version of xwax on the root of the USB stick, and the SC1000 will run it instead of the internal version. This gives a very easy way to update the software on the device.


## Usage ##

Simply switch on SC1000 with a valid USB stick in, and after a few seconds it will start playing the first beat and sample on the USB stick. Plug in some headphones or a portable speaker, adjust the volume controls to your liking, and get skratchin!

*Pressing* the **beat/sample down** button will select the next file in the current folder, and *holding* the button will skip to the next folder.

Note that you shouldn't touch the jog wheel while you are turning the device on - this is because the SC1000 does a short calibration routine. Leave it a few seconds before touching it.


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

