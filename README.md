# SC1000
## Open-source portable digital scratch instrument

This github holds source code and CAM files for the SC1000.

Demo video : 

[![Demo Video](https://img.youtube.com/vi/JTFGoQHsh3w/0.jpg)](https://www.youtube.com/watch?v=JTFGoQHsh3w)

The folders are as follows : 
* **Firmware** - Source code for the input processor. This handles the pots, switches and capacitive touch sensor, and passes the information on to the main processor.
* **SCOS** - Buildroot configuration and toolchain for the custom linux distribution that runs on the main SoC.
* **Software** - Source code for the modified version of xwax running on the main SoC.
* **Hardware** - Schematics and gerbers for the main PCB and enclosure (which is made of PCBs)



## (very) Brief build guide : 


### Ingredients

* **Main PCB and components** - Board files are in *./hardware/gerbers/Main PCB/* and can be ordered from somewhere like https://jlcpcb.com/
* **Components** - Bill of Materials is in *./hardware/docs/SC1000 - Bill Of Materials.pdf* and can be ordered from Mouser
* **A13 System-on-Module** - Available from https://www.olimex.com/Products/SOM/A13/A13-SOM-256/, connects to the main PCB via 0.05" headers
* **SD Card** - To hold the operating system. It only needs 200Mb so just get the smallest card you can find
* **Enclosure parts** - The enclosure is made from PCBs and aluminium supports. Gerber files are in *./hardware/gerbers/Enclosure/*, aluminium supports are 20x10x156.8mm. I got mine from https://www.aluminiumwarehouse.co.uk/20-mm-x-10-mm-aluminium-flat-bar, they even cut it for me.
* **Jogwheel parts** - M8 bearing/hex bolt/nuts/washers, diametrically polarized magnet from https://www.kjmagnetics.com/proddetail.asp?prod=D42DIA-N52
* **Mini Innofader** - the OEM model (for example found in the innoFADER Mini DUO pack) is fine.


### Method ###

* Order the Main and Enclosure PCBs, the components, the A13 SoM, and SD Card, and the Aluminium bar.

* Assemble the Main PCB. I recommend assembling/testing the 3.3v power section first, so you don't blow all the other components. Don't connect the A13 module yet.

* Flash the input processor with its firmware. You will need a PIC programmer, such as the Microchip Pickit 3. The firmware hex file is available under */firmware/*

* Transfer the operating system to the SD card. You will need an SD card interface, either USB or built-in to your PC. You can use dd on Linux/MacOS or Etcher on Windows to transfer the image. 

* Insert the SD card in the A13 module, and attach the SoM to the main PCB. Make sure it's the correct way round - the SD card should be right beside the USB storage connector on the rear of the SC1000.

* Connect a USB power source, and power up the unit to test - the A13 module's green light should blink a few times before remaining on.

* Assemble the jogwheel - glue the bearing into the hole in the top plate of the enclosure. Now glue the magnet to the tip of the M8 bolt. Attach the jogwheel to the bearing using the bolt/nut/washer. Solder a wire to the outside of the bearing to act as a capacitive touch sensor.

* Connect the fader to J1, capacitive touch sensor to J4, and (optionally) a small USB power bank to J3. If you don't use a power bank, put two jumpers horizontally across J3 to allow the power to bypass it.

* Assemble the enclosure - drill and tap M3 holes in the aluminium, and screw the whole enclosure together. Make sure the magnet at the end of the jogwheel bolt is suspended directly above the rotary sensor IC.
