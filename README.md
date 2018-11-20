# SC1000
## Open-source portable digital scratch instrument

This github holds source code and CAM files for the SC1000.

Demo video : 

[![Demo Video](https://img.youtube.com/vi/JTFGoQHsh3w/0.jpg)](https://www.youtube.com/watch?v=JTFGoQHsh3w)

The folders are as follows : 
* **Firmware** - Source code for the input processor. This handles the pots, switches and capacitive touch sensor, and passes the information on to the main processor.
* **SCOS** - Buildroot configuration and toolchain for the custom linux distribution that runs on the main SoC.
* **Software** - Source code for the modified version of xwax running on the main SoC.
* **Hardware** - Gerbers, Schematics and Altium source files for the main PCB and enclosure (which is made of PCBs)
