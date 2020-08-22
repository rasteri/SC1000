// SC1000 input handler
// Thread that grabs data from the rotary sensor and PIC input processor and processes it

#include <assert.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>		   //Needed for I2C port
#include <sys/ioctl.h>	   //Needed for I2C port
#include <linux/i2c-dev.h> //Needed for I2C port
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/time.h>
#include "sc_playlist.h"
#include "alsa.h"
#include "controller.h"
#include "device.h"
#include "dummy.h"
#include "realtime.h"
#include "thread.h"
#include "rig.h"
#include "track.h"
#include "xwax.h"
#include "sc_input.h"
#include "sc_midimap.h"
#include "dicer.h"
#include "midi.h"

bool shifted = 0;
bool shiftLatched = 0;

extern struct rt rt;

struct controller midiControllers[32];
int numControllers = 0;

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)       \
	(byte & 0x80 ? '1' : '0'),     \
		(byte & 0x40 ? '1' : '0'), \
		(byte & 0x20 ? '1' : '0'), \
		(byte & 0x10 ? '1' : '0'), \
		(byte & 0x08 ? '1' : '0'), \
		(byte & 0x04 ? '1' : '0'), \
		(byte & 0x02 ? '1' : '0'), \
		(byte & 0x01 ? '1' : '0')

extern struct mapping *maps;

void i2c_read_address(int file_i2c, unsigned char address, unsigned char *result)
{

	*result = address;
	if (write(file_i2c, result, 1) != 1)
	{
		printf("I2C read error\n");
		exit(1);
	}

	if (read(file_i2c, result, 1) != 1)
	{
		printf("I2C read error\n");
		exit(1);
	}
}

int i2c_write_address(int file_i2c, unsigned char address, unsigned char value)
{
	char buf[2];
	buf[0] = address;
	buf[1] = value;
	if (write(file_i2c, buf, 2) != 2)
	{
		printf("I2C Write Error\n");
		return 0;
	}
	else
		return 1;
}

void dump_maps(){
	struct mapping *new_map = maps;
	while (new_map != NULL)
	{
		printf("Dump Mapping - ty:%d po:%d pn%x pl:%x ed%x mid:%x:%x:%x- dn:%d, a:%d, p:%d\n", new_map->Type, new_map->port, new_map->Pin, new_map->Pullup, new_map->Edge,new_map->MidiBytes[0],new_map->MidiBytes[1],new_map->MidiBytes[2], new_map->DeckNo, new_map->Action, new_map->Param);
		new_map = new_map->next;
	}

}

int setupi2c(char *path, unsigned char address)
{

	int file = 0;

	if ((file = open(path, O_RDWR)) < 0)
	{
		printf("%s - Failed to open\n", path);
		return -1;
	}
	else if (ioctl(file, I2C_SLAVE, address) < 0)
	{
		printf("%s - Failed to acquire bus access and/or talk to slave.\n", path);
		return -1;
	}
	else
		return file;
}

void AddNewMidiDevices(char mididevices[64][64], int mididevicenum)
{
	bool alreadyAdded;
	// Search to see which devices we've already added
	for (int devc = 0; devc < mididevicenum; devc++)
	{

		alreadyAdded = 0;

		for (int controlc = 0; controlc < numControllers; controlc++)
		{
			char *controlName = ((struct dicer *)(midiControllers[controlc].local))->PortName;
			if (strcmp(mididevices[devc], controlName) == 0)
				alreadyAdded = 1;
		}

		if (!alreadyAdded)
		{
			if (dicer_init(&midiControllers[numControllers], &rt, mididevices[devc]) != -1)
			{
				printf("Adding MIDI device %d - %s\n", numControllers, mididevices[devc]);
				controller_add_deck(&midiControllers[numControllers], &deck[0]);
				controller_add_deck(&midiControllers[numControllers], &deck[1]);
				numControllers++;
			}
		}
	}
}
unsigned char gpiopresent = 1;
int file_i2c_gpio;
volatile void *gpio_addr;

void addDefaultIOMap(bool ExternalGPIO)
{

	unsigned char midicommand[3];
	unsigned char deckno;
	unsigned char notenum;
	
	if (!scsettings.midiRemapped)
	{
		// Set up per-deck cue/startstop/pitchbend mappings
		for (deckno = 0; deckno < 2; deckno++)
		{
			//CC 0 of channels 0/1 is volume
			midicommand[0] = 0xB0 + deckno;
			midicommand[1] = 0x00;
			add_mapping(&maps, MAP_MIDI, deckno, midicommand, 0, 0, 0, 1, ACTION_VOLUME, 0);

			// Notes on channels 0 and 1 are cue points
			for (notenum = 0; notenum < 128; notenum++)
			{
				midicommand[0] = 0x90 + deckno;
				midicommand[1] = notenum;
				add_mapping(&maps, MAP_MIDI, deckno, midicommand, 0, 0, 0, 1, ACTION_CUE, 0);

				// Also add the delete cue command for shift modifier
				midicommand[0] = 0x90 + deckno;
				midicommand[1] = notenum;
				add_mapping(&maps, MAP_MIDI, deckno, midicommand, 0, 0, 0, 3, ACTION_DELETECUE, 0);
			}

			// Notes on channels 2 and 3 are C1-style notes
			for (notenum = 0; notenum < 128; notenum++)
			{
				midicommand[0] = 0x92 + deckno;
				midicommand[1] = notenum;
				//add_MIDI_mapping(&maps, midicommand, deckno, ACTION_NOTE, notenum);
				add_mapping(&maps, MAP_MIDI, deckno, midicommand, 0, 0, 0, 1, ACTION_NOTE, notenum);
			}

			// Pitch bend on channels 0 and 1 is, well, pitchbend
			midicommand[0] = 0xE0 + deckno;
			midicommand[1] = 0;
			midicommand[2] = 0;
			//add_MIDI_mapping(&maps, midicommand, deckno, ACTION_PITCH, 0);
			add_mapping(&maps, MAP_MIDI, deckno, midicommand, 0, 0, 0, 1, ACTION_PITCH, 0);

			// Notes 0-1 of channel 4 are startstop
			midicommand[0] = 0x94;
			midicommand[1] = deckno;
			//add_MIDI_mapping(&maps, midicommand, deckno, ACTION_STARTSTOP, 0);
			add_mapping(&maps, MAP_MIDI, deckno, midicommand, 0, 0, 0, 1, ACTION_STARTSTOP, 0);

			// Notes 2-3 of channel 4 are Next File
			midicommand[0] = 0x94;
			midicommand[1] = deckno + 2;
			//add_MIDI_mapping(&maps, midicommand, deckno, ACTION_STARTSTOP, 0);
			add_mapping(&maps, MAP_MIDI, deckno, midicommand, 0, 0, 0, 1, ACTION_NEXTFILE, 0);

			// Notes 4-5 of channel 4 are Next Folder
			midicommand[0] = 0x94;
			midicommand[1] = deckno + 4;
			//add_MIDI_mapping(&maps, midicommand, deckno, ACTION_STARTSTOP, 0);
			add_mapping(&maps, MAP_MIDI, deckno, midicommand, 0, 0, 0, 1, ACTION_NEXTFOLDER, 0);

			// Notes 6-7 of channel 4 are Prev File
			midicommand[0] = 0x94;
			midicommand[1] = deckno + 6;
			//add_MIDI_mapping(&maps, midicommand, deckno, ACTION_STARTSTOP, 0);
			add_mapping(&maps, MAP_MIDI, deckno, midicommand, 0, 0, 0, 1, ACTION_PREVFILE, 0);

			// Notes 8-9 of channel 4 are Prev Folder
			midicommand[0] = 0x94;
			midicommand[1] = deckno + 8;
			//add_MIDI_mapping(&maps, midicommand, deckno, ACTION_STARTSTOP, 0);
			add_mapping(&maps, MAP_MIDI, deckno, midicommand, 0, 0, 0, 1, ACTION_PREVFOLDER, 0);

			// Notes 10-11 of channel 4 are Random File
			midicommand[0] = 0x94;
			midicommand[1] = deckno + 10;
			//add_MIDI_mapping(&maps, midicommand, deckno, ACTION_STARTSTOP, 0);
			add_mapping(&maps, MAP_MIDI, deckno, midicommand, 0, 0, 0, 1, ACTION_RANDOMFILE, 0);

			// Notes 10-11 of channel 4 are Random File
			midicommand[0] = 0x94;
			midicommand[1] = deckno + 10;
			//add_MIDI_mapping(&maps, midicommand, deckno, ACTION_STARTSTOP, 0);
			add_mapping(&maps, MAP_MIDI, deckno, midicommand, 0, 0, 0, 1, ACTION_RANDOMFILE, 0);
		}

		// Note 7E of channel 4 is RECORD
		midicommand[0] = 0x94;
		midicommand[1] = 0x7E;
		add_mapping(&maps, MAP_MIDI, 0, midicommand, 0, 0, 0, 1, ACTION_RECORD, 0);

		// note 7F of channel 4 is shift
		midicommand[0] = 0x94;
		midicommand[1] = 0x7F;
		add_mapping(&maps, MAP_MIDI, deckno, midicommand, 0, 0, 0, 1, ACTION_SHIFTON, 0);
		midicommand[0] = 0x84;
		midicommand[1] = 0x7F;
		// Edge is 3 in this next statement because obviously we're shifted if we're disengaging shift
		add_mapping(&maps, MAP_MIDI, deckno, midicommand, 0, 0, 0, 3, ACTION_SHIFTOFF, 0);
	}

	// Now onto GPIO

	// SC500 detection pin, always mapped no matter what
	add_mapping(&maps, MAP_IO, 0, midicommand, 6, 11, 1, 1, ACTION_SC500, 0);

	if (!scsettings.ioRemapped)
	{
		// To start with we always map SC500 buttons, no harm in it even on the SC1000

		// CH0 Transport
		add_mapping(&maps, MAP_IO, 0, midicommand, 2, 14, 1, 1, ACTION_PREVFILE, 0);
		add_mapping(&maps, MAP_IO, 0, midicommand, 2, 14, 1, 2, ACTION_PREVFOLDER, 0);
		add_mapping(&maps, MAP_IO, 0, midicommand, 2, 14, 1, 3, ACTION_JOGPIT, 0);
		add_mapping(&maps, MAP_IO, 0, midicommand, 2, 14, 1, 0, ACTION_JOGPSTOP, 0);

		add_mapping(&maps, MAP_IO, 0, midicommand, 2, 13, 1, 1, ACTION_STARTSTOP, 0);

		add_mapping(&maps, MAP_IO, 0, midicommand, 2, 10, 1, 1, ACTION_NEXTFILE, 0);
		add_mapping(&maps, MAP_IO, 0, midicommand, 2, 10, 1, 2, ACTION_NEXTFOLDER, 0);
		add_mapping(&maps, MAP_IO, 0, midicommand, 2, 10, 1, 3, ACTION_RANDOMFILE, 0);

		// CH0 Volume
		add_mapping(&maps, MAP_IO, 0, midicommand, 4, 5, 1, 1, ACTION_VOLUP, 0);
		add_mapping(&maps, MAP_IO, 0, midicommand, 4, 5, 1, 2, ACTION_VOLUHOLD, 0);
		add_mapping(&maps, MAP_IO, 0, midicommand, 4, 4, 1, 1, ACTION_VOLDOWN, 0);
		add_mapping(&maps, MAP_IO, 0, midicommand, 4, 4, 1, 2, ACTION_VOLDHOLD, 0);

		// Shift
		add_mapping(&maps, MAP_IO, 1, midicommand, 2, 9, 1, 1, ACTION_SHIFTON, 0);
		add_mapping(&maps, MAP_IO, 1, midicommand, 2, 9, 1, 0, ACTION_SHIFTOFF, 0);

		// CH1 cues
		add_mapping(&maps, MAP_IO, 1, midicommand, 2, 4, 1, 1, ACTION_CUE, 0);
		add_mapping(&maps, MAP_IO, 1, midicommand, 2, 4, 1, 3, ACTION_DELETECUE, 0);
		add_mapping(&maps, MAP_IO, 1, midicommand, 2, 7, 1, 1, ACTION_CUE, 0);
		add_mapping(&maps, MAP_IO, 1, midicommand, 2, 7, 1, 3, ACTION_DELETECUE, 0);
		add_mapping(&maps, MAP_IO, 1, midicommand, 2, 11, 1, 1, ACTION_CUE, 0);
		add_mapping(&maps, MAP_IO, 1, midicommand, 2, 11, 1, 3, ACTION_DELETECUE, 0);
		add_mapping(&maps, MAP_IO, 1, midicommand, 2, 12, 1, 1, ACTION_CUE, 0);
		add_mapping(&maps, MAP_IO, 1, midicommand, 2, 12, 1, 3, ACTION_DELETECUE, 0);

		// CH1 transport
		add_mapping(&maps, MAP_IO, 1, midicommand, 2, 5, 1, 1, ACTION_PREVFILE, 0);
		add_mapping(&maps, MAP_IO, 1, midicommand, 2, 5, 1, 2, ACTION_PREVFOLDER, 0);
		add_mapping(&maps, MAP_IO, 1, midicommand, 2, 5, 1, 3, ACTION_JOGPIT, 0);
		add_mapping(&maps, MAP_IO, 1, midicommand, 2, 5, 1, 0, ACTION_JOGPSTOP, 0);

		add_mapping(&maps, MAP_IO, 1, midicommand, 2, 6, 1, 1, ACTION_STARTSTOP, 0);
		add_mapping(&maps, MAP_IO, 1, midicommand, 2, 6, 1, 3, ACTION_RECORD, 0);

		add_mapping(&maps, MAP_IO, 1, midicommand, 2, 8, 1, 1, ACTION_NEXTFILE, 0);
		add_mapping(&maps, MAP_IO, 1, midicommand, 2, 8, 1, 2, ACTION_NEXTFOLDER, 0);
		add_mapping(&maps, MAP_IO, 1, midicommand, 2, 8, 1, 3, ACTION_RANDOMFILE, 0);

		// CH1 Volume
		add_mapping(&maps, MAP_IO, 1, midicommand, 1, 10, 1, 1, ACTION_VOLUP, 0);
		add_mapping(&maps, MAP_IO, 1, midicommand, 1, 10, 1, 2, ACTION_VOLUHOLD, 0);
		add_mapping(&maps, MAP_IO, 1, midicommand, 1, 4, 1, 1, ACTION_VOLDOWN, 0);
		add_mapping(&maps, MAP_IO, 1, midicommand, 1, 4, 1, 2, ACTION_VOLDHOLD, 0);

		// If there's an external GPIO on the expansion port (J7),
		// map it like in the default settings file of v1.4
		if (ExternalGPIO)
		{
			add_mapping(&maps, MAP_IO, 0, midicommand, 0, 0, 0, 1, ACTION_GND, 0);
			add_mapping(&maps, MAP_IO, 1, midicommand, 0, 1, 1, 1, ACTION_CUE, 0);
			add_mapping(&maps, MAP_IO, 1, midicommand, 0, 2, 1, 1, ACTION_CUE, 0);
			add_mapping(&maps, MAP_IO, 1, midicommand, 0, 3, 1, 1, ACTION_CUE, 0);
			add_mapping(&maps, MAP_IO, 1, midicommand, 0, 4, 1, 1, ACTION_CUE, 0);
			add_mapping(&maps, MAP_IO, 1, midicommand, 0, 5, 1, 1, ACTION_CUE, 0);
			add_mapping(&maps, MAP_IO, 1, midicommand, 0, 6, 1, 1, ACTION_CUE, 0);
			add_mapping(&maps, MAP_IO, 1, midicommand, 0, 7, 1, 1, ACTION_CUE, 0);

			add_mapping(&maps, MAP_IO, 1, midicommand, 0, 8, 1, 1, ACTION_STARTSTOP, 0);
			add_mapping(&maps, MAP_IO, 0, midicommand, 0, 9, 1, 1, ACTION_SHIFTON, 0);
			add_mapping(&maps, MAP_IO, 0, midicommand, 0, 9, 1, 0, ACTION_SHIFTOFF, 0);
			add_mapping(&maps, MAP_IO, 1, midicommand, 0, 10, 1, 1, ACTION_CUE, 0);
			add_mapping(&maps, MAP_IO, 0, midicommand, 0, 11, 1, 1, ACTION_STARTSTOP, 0);
			add_mapping(&maps, MAP_IO, 1, midicommand, 0, 12, 0, 1, ACTION_GND, 0);
			add_mapping(&maps, MAP_IO, 1, midicommand, 0, 13, 1, 1, ACTION_CUE, 0);
			add_mapping(&maps, MAP_IO, 1, midicommand, 0, 14, 1, 1, ACTION_CUE, 0);
			add_mapping(&maps, MAP_IO, 1, midicommand, 0, 15, 1, 1, ACTION_CUE, 0);
		}

		// If not, map the expansion port pins as regular inputs
		else
		{
			// Shift, startstop for first three pins
			add_mapping(&maps, MAP_IO, 0, midicommand, 6, 10, 1, 1, ACTION_SHIFTON, 0);	  //J7 pin 3
			add_mapping(&maps, MAP_IO, 0, midicommand, 6, 10, 1, 0, ACTION_SHIFTOFF, 0);  //J7 pin 3
			add_mapping(&maps, MAP_IO, 0, midicommand, 1, 15, 1, 1, ACTION_STARTSTOP, 0); //J7 pin 4
			//J7 pin5 is pulled down through an LED so probably won't work as a switch input
			add_mapping(&maps, MAP_IO, 1, midicommand, 1, 16, 1, 1, ACTION_STARTSTOP, 0); //J7 pin 6

			// Everything else is sample cues
			add_mapping(&maps, MAP_IO, 1, midicommand, 2, 3, 1, 1, ACTION_CUE, 0); // J7 pin 7
			add_mapping(&maps, MAP_IO, 1, midicommand, 2, 0, 1, 1, ACTION_CUE, 0); // J7 pin 8
			add_mapping(&maps, MAP_IO, 1, midicommand, 2, 2, 1, 1, ACTION_CUE, 0); // J7 pin 7
			add_mapping(&maps, MAP_IO, 1, midicommand, 2, 1, 1, 1, ACTION_CUE, 0); // J7 pin 7
		}
	}
}
bool firstTimeRound = 1;
void init_io()
{
	int i, j, k;
	struct mapping *map;

	// Initialise external MCP23017 GPIO on I2C1
	if ((file_i2c_gpio = setupi2c("/dev/i2c-1", 0x20)) < 0)
	{
		printf("Couldn't init external GPIO\n");
		gpiopresent = 0;
	}
	else
	{
		// Do a test write to make sure we got in
		if (!i2c_write_address(file_i2c_gpio, 0x0C, 0xFF))
		{
			gpiopresent = 0;
			printf("Couldn't init external GPIO\n");
		}
	}

	// Configure external IO
	if (gpiopresent)
	{

		// default to pulled up and input
		unsigned int pullups = 0xFFFF;
		unsigned int iodirs = 0xFFFF;

		// For each pin
		for (i = 0; i < 16; i++)
		{
			map = find_IO_mapping(maps, 0, i, 1);
			// If pin is marked as ground
			if (map != NULL && map->Action == ACTION_GND)
			{
				printf("Grounding pin %d\n", i);
				iodirs &= ~(0x0001 << i);
			}

			// If pin's pullup is disabled
			if (map != NULL && !map->Pullup)
			{
				printf("Disabling pin %d pullup\n", i);
				pullups &= ~(0x0001 << i);
			}
		}

		unsigned char tmpchar;

		// Bank A pullups
		tmpchar = (unsigned char)(pullups & 0xFF);
		i2c_write_address(file_i2c_gpio, 0x0C, tmpchar);

		// Bank B pullups
		tmpchar = (unsigned char)((pullups >> 8) & 0xFF);
		i2c_write_address(file_i2c_gpio, 0x0D, tmpchar);

		printf("PULLUPS - B");
		printf(BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY((pullups >> 8) & 0xFF));
		printf("A");
		printf(BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY((pullups & 0xFF)));
		printf("\n");

		// Bank A direction
		tmpchar = (unsigned char)(iodirs & 0xFF);
		i2c_write_address(file_i2c_gpio, 0x00, tmpchar);

		// Bank B direction
		tmpchar = (unsigned char)((iodirs >> 8) & 0xFF);
		i2c_write_address(file_i2c_gpio, 0x01, tmpchar);

		printf("IODIRS  - B");
		printf(BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY((iodirs >> 8) & 0xFF));
		printf("A");
		printf(BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(iodirs & 0xFF));
		printf("\n");
		addDefaultIOMap(true);
	}
	else
	{
		addDefaultIOMap(false);
	}



	// Configure A13 GPIO

	int fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (fd < 0)
	{
		fprintf(stderr, "Unable to open port\n\r");
		exit(fd);
	}
	gpio_addr = mmap(NULL, 65536, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0x01C20800 & 0xffff0000);
	if (gpio_addr == MAP_FAILED)
	{
		fprintf(stderr, "Unable to open mmap\n\r");
		exit(fd);
	}
	gpio_addr += 0x0800;

	// For each port
	for (j = 1; j <= 6; j++)
	{
		// For each pin (max number of pins on each port is 28)
		for (i = 0; i < 28; i++)
		{

			map = find_IO_mapping(maps, j, i, 1);

			if (map != NULL)
			{
				printf("Pulling %d %d %d\n", j, i, map->Pullup);
				// which config register to use, 0-3
				uint32_t configregister = i >> 3;

				// which pull register to use, 0-1
				uint32_t pullregister = i >> 4;

				// how many bits to shift the config register
				uint32_t configShift = (i % 8) * 4;

				// how many bits to shift the pull register
				uint32_t pullShift = (i % 16) * 2;

				volatile uint32_t *PortConfigRegister = gpio_addr + (j * 0x24) + (configregister * 0x04);
				volatile uint32_t *PortPullRegister = gpio_addr + (j * 0x24) + 0x1C + (pullregister * 0x04);
				uint32_t portConfig = *PortConfigRegister;
				uint32_t portPull = *PortPullRegister;

				// mask to unset the relevant pins in the registers
				uint32_t configMask = ~(0b1111 << configShift);
				uint32_t pullMask = ~(0b11 << pullShift);

				// Set port as input
				// portConfig = (portConfig & configMask) | (0b0000 << configShift); (not needed because input is 0 anyway)
				portConfig = (portConfig & configMask);

				portPull = (portPull & pullMask) | (map->Pullup << pullShift);
				*PortConfigRegister = portConfig;
				*PortPullRegister = portPull;
			}
		}
	}

	unsigned char tmpchar;
}

void process_io()
{ // Iterate through all digital input mappings and check the appropriate pin
	unsigned int gpios = 0x00000000;
	unsigned char result;
	if (gpiopresent)
	{
		i2c_read_address(file_i2c_gpio, 0x13, &result); // Read bank B
		gpios = ((unsigned int)result) << 8;
		//printf(BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(result));
		i2c_read_address(file_i2c_gpio, 0x12, &result); // Read bank A
		gpios |= result;
		//printf(" - ");
		//printf(BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(result));
		//printf("\n");

		// invert logic
		gpios ^= 0xFFFF;
	}
	struct mapping *last_map = maps;
	while (last_map != NULL)
	{
		//printf("arses : %d %d\n", last_map->port, last_map->Pin);

		// Only digital pins
		if (last_map->Type = MAP_IO && (!(last_map->port == 0 && !gpiopresent)))
		{

			bool pinVal = 0;
			if (last_map->port == 0) // port 0, I2C GPIO expander
			{
				pinVal = (bool)((gpios >> last_map->Pin) & 0x01);
			}
			else // Ports 1-6, olimex GPIO
			{
				volatile uint32_t *PortDataReg = gpio_addr + (last_map->port * 0x24) + 0x10;
				uint32_t PortData = *PortDataReg;
				PortData ^= 0xffffffff;
				pinVal = (bool)((PortData >> last_map->Pin) & 0x01);
			}

			// iodebounce = 0 when button not pressed,
			// > 0 and < scsettings.debouncetime when debouncing positive edge
			// > scsettings.debouncetime and < scsettings.holdtime when holding
			// = scsettings.holdtime when continuing to hold
			// > scsettings.holdtime when waiting for release
			// > -scsettings.debouncetime and < 0 when debouncing negative edge

			// Button not pressed, check for button
			if (last_map->debounce == 0)
			{
				if (pinVal)
				{
					printf("Button %d pressed\n", last_map->Pin);
					if (firstTimeRound && last_map->DeckNo == 1 && (last_map->Action == ACTION_VOLUP || last_map->Action == ACTION_VOLDOWN))
					{
						printf("doing\n", last_map->Pin);
						player_set_track(&deck[0].player, track_acquire_by_import(deck[0].importer, "/var/os-version.mp3"));
						cues_load_from_file(&deck[0].cues, deck[0].player.track->path);
						deck[1].player.setVolume = 0.0;

					}
					else
					{
						if ((!shifted && last_map->Edge == 1) || (shifted && last_map->Edge == 3))
							IOevent(last_map, NULL);

						// start the counter
						last_map->debounce++;
					}
				}
			}

			// Debouncing positive edge, increment value
			else if (last_map->debounce > 0 && last_map->debounce < scsettings.debouncetime)
			{
				last_map->debounce++;
			}

			// debounce finished, keep incrementing until hold reached
			else if (last_map->debounce >= scsettings.debouncetime && last_map->debounce < scsettings.holdtime)
			{
				// check to see if unpressed
				if (!pinVal)
				{
					printf("Button %d released\n", last_map->Pin);
					if (last_map->Edge == 0)
						IOevent(last_map, NULL);
					// start the counter
					last_map->debounce = -scsettings.debouncetime;
				}

				else
					last_map->debounce++;
			}
			// Button has been held for a while
			else if (last_map->debounce == scsettings.holdtime)
			{
				printf("Button %d-%d held\n", last_map->port, last_map->Pin);
				if ((!shifted && last_map->Edge == 2) || (shifted && last_map->Edge == 4))
					IOevent(last_map, NULL);
				last_map->debounce++;
			}

			// Button still holding, check for release
			else if (last_map->debounce > scsettings.holdtime)
			{
				if (pinVal)
				{
					if (last_map->Action == ACTION_VOLUHOLD || last_map->Action == ACTION_VOLDHOLD)
					{
						// keep running the vol up/down actions if they're held
						if ((!shifted && last_map->Edge == 2) || (shifted && last_map->Edge == 4))
							IOevent(last_map, NULL);
					}
				}
				// check to see if unpressed
				else
				{
					printf("Button %d released\n", last_map->Pin);
					if (last_map->Edge == 0)
						IOevent(last_map, NULL);
					// start the counter
					last_map->debounce = -scsettings.debouncetime;
				}
			}

			// Debouncing negative edge, increment value - will reset when zero is reached
			else if (last_map->debounce < 0)
			{
				last_map->debounce++;
			}
		}

		last_map = last_map->next;
	}

	// Dumb hack to process MIDI commands in this thread rather than the realtime one
	if (QueuedMidiCommand != NULL)
	{
		IOevent(QueuedMidiCommand, QueuedMidiBuffer);
		QueuedMidiCommand = NULL;
	}
}

int file_i2c_rot, file_i2c_pic;

int pitchMode = 0; // If we're in pitch-change mode
int oldPitchMode = 0;
bool capIsTouched = 0;
unsigned char buttons[4] = {0, 0, 0, 0}, totalbuttons[4] = {0, 0, 0, 0};
unsigned int ADCs[4] = {0, 0, 0, 0};
unsigned char buttonState = 0;
unsigned int butCounter = 0;
void process_pic()
{
	unsigned int i;

	unsigned char result;

	unsigned char faderOpen = 0;
	unsigned int faderCutPoint;

	i2c_read_address(file_i2c_pic, 0x00, &result);
	ADCs[0] = result;
	i2c_read_address(file_i2c_pic, 0x01, &result);
	ADCs[1] = result;
	i2c_read_address(file_i2c_pic, 0x02, &result);
	ADCs[2] = result;
	i2c_read_address(file_i2c_pic, 0x03, &result);
	ADCs[3] = result;
	i2c_read_address(file_i2c_pic, 0x04, &result);
	ADCs[0] |= ((unsigned int)(result & 0x03) << 8);
	ADCs[1] |= ((unsigned int)(result & 0x0C) << 6);
	ADCs[2] |= ((unsigned int)(result & 0x30) << 4);
	ADCs[3] |= ((unsigned int)(result & 0xC0) << 2);
	// Now buttons and capsense

	i2c_read_address(file_i2c_pic, 0x05, &result);
	buttons[0] = !(result & 0x01);
	buttons[1] = !(result >> 1 & 0x01);
	buttons[2] = !(result >> 2 & 0x01);
	buttons[3] = !(result >> 3 & 0x01);
	capIsTouched = (result >> 4 & 0x01);

	process_io();

	// Apply volume and fader

	if (!scsettings.disablevolumeadc)
	{
		deck[0].player.setVolume = ((double)ADCs[2]) / 1024;
		deck[1].player.setVolume = ((double)ADCs[3]) / 1024;
	}

	faderCutPoint = faderOpen ? scsettings.faderclosepoint : scsettings.faderopenpoint; // Fader Hysteresis

	if (ADCs[0] > faderCutPoint && ADCs[1] > faderCutPoint)
	{ // cut on both sides of crossfader
		deck[1].player.faderTarget = deck[1].player.setVolume;
		faderOpen = 1;
	}
	else
	{
		deck[1].player.faderTarget = 0.0;
		faderOpen = 0;
	}

	deck[0].player.faderTarget = deck[0].player.setVolume;

	if (!scsettings.disablepicbuttons)
	{
		/*
		 Button scanning logic goes like -

		 1. Wait for ANY button to be pressed
		 2. Note which buttons are pressed
		 3. If we're still holding down buttons after an amount of time, act on held buttons, goto 5
		 4. If ALL buttons are unpressed act on them instantaneously, goto 5
		 5. wait half a second or so, then goto 1;

		 */

#define BUTTONSTATE_NONE 0
#define BUTTONSTATE_PRESSING 1
#define BUTTONSTATE_ACTING_INSTANT 2
#define BUTTONSTATE_ACTING_HELD 3
#define BUTTONSTATE_WAITING 4
		int r;

		switch (buttonState)
		{

		// No buttons pressed
		case BUTTONSTATE_NONE:
			if (buttons[0] || buttons[1] || buttons[2] || buttons[3])
			{
				buttonState = BUTTONSTATE_PRESSING;

				if (firstTimeRound)
				{
					player_set_track(&deck[0].player, track_acquire_by_import(deck[0].importer, "/var/os-version.mp3"));
					cues_load_from_file(&deck[0].cues, deck[0].player.track->path);
					buttonState = BUTTONSTATE_WAITING;
				}
			}

			break;

		// At least one button pressed
		case BUTTONSTATE_PRESSING:
			for (i = 0; i < 4; i++)
				totalbuttons[i] |= buttons[i];

			if (!(buttons[0] || buttons[1] || buttons[2] || buttons[3]))
				buttonState = BUTTONSTATE_ACTING_INSTANT;

			butCounter++;
			if (butCounter > scsettings.holdtime)
			{
				butCounter = 0;
				buttonState = BUTTONSTATE_ACTING_HELD;
			}

			break;

		// Act on instantaneous (i.e. not held) button press
		case BUTTONSTATE_ACTING_INSTANT:

			// Any button to stop pitch mode
			if (pitchMode)
			{
				pitchMode = 0;
				oldPitchMode = 0;
				printf("Pitch mode Disabled\n");
			}
			else if (totalbuttons[0] && !totalbuttons[1] && !totalbuttons[2] && !totalbuttons[3] && deck[1].filesPresent)
				deck_prev_file(&deck[1]);
			else if (!totalbuttons[0] && totalbuttons[1] && !totalbuttons[2] && !totalbuttons[3] && deck[1].filesPresent)
				deck_next_file(&deck[1]);
			else if (totalbuttons[0] && totalbuttons[1] && !totalbuttons[2] && !totalbuttons[3] && deck[1].filesPresent)
				pitchMode = 2;
			else if (!totalbuttons[0] && !totalbuttons[1] && totalbuttons[2] && !totalbuttons[3] && deck[0].filesPresent)
				deck_prev_file(&deck[0]);
			else if (!totalbuttons[0] && !totalbuttons[1] && !totalbuttons[2] && totalbuttons[3] && deck[0].filesPresent)
				deck_next_file(&deck[0]);
			else if (!totalbuttons[0] && !totalbuttons[1] && totalbuttons[2] && totalbuttons[3] && deck[0].filesPresent)
				pitchMode = 1;
			else if (totalbuttons[0] && totalbuttons[1] && totalbuttons[2] && totalbuttons[3])
				shiftLatched = 1;
			else
				printf("Sod knows what you were trying to do there\n");

			buttonState = BUTTONSTATE_WAITING;

			break;

		// Act on whatever buttons are being held down when the timeout happens
		case BUTTONSTATE_ACTING_HELD:
			if (buttons[0] && !buttons[1] && !buttons[2] && !buttons[3] && deck[1].filesPresent)
				deck_prev_folder(&deck[1]);
			else if (!buttons[0] && buttons[1] && !buttons[2] && !buttons[3] && deck[1].filesPresent)
				deck_next_folder(&deck[1]);
			else if (buttons[0] && buttons[1] && !buttons[2] && !buttons[3] && deck[1].filesPresent)
				deck_random_file(&deck[1]);
			else if (!buttons[0] && !buttons[1] && buttons[2] && !buttons[3] && deck[0].filesPresent)
				deck_prev_folder(&deck[0]);
			else if (!buttons[0] && !buttons[1] && !buttons[2] && buttons[3] && deck[0].filesPresent)
				deck_next_folder(&deck[0]);
			else if (!buttons[0] && !buttons[1] && buttons[2] && buttons[3] && deck[0].filesPresent)
				deck_random_file(&deck[0]);
			else if (buttons[0] && buttons[1] && buttons[2] && buttons[3])
			{
				printf("All buttons held!\n");
				if (deck[1].filesPresent)
					deck_record(&deck[0]);
			}
			else
				printf("Sod knows what you were trying to do there\n");

			buttonState = BUTTONSTATE_WAITING;

			break;

		case BUTTONSTATE_WAITING:

			butCounter++;

			// wait till buttons are released before allowing the countdown
			if (buttons[0] || buttons[1] || buttons[2] || buttons[3])
				butCounter = 0;

			if (butCounter > 20)
			{
				butCounter = 0;
				buttonState = BUTTONSTATE_NONE;

				for (i = 0; i < 4; i++)
					totalbuttons[i] = 0;
			}
			break;
		}
	}
}

void process_rot()
{
	unsigned char result;
	int8_t crossedZero; // 0 when we haven't crossed zero, -1 when we've crossed in anti-clockwise direction, 1 when crossed in clockwise
	int wrappedAngle = 0x0000;
	unsigned int numBlips = 0;
	// Handle rotary sensor

	i2c_read_address(file_i2c_rot, 0x0c, &result);
	deck[1].newEncoderAngle = result << 8;
	i2c_read_address(file_i2c_rot, 0x0d, &result);
	deck[1].newEncoderAngle = (deck[1].newEncoderAngle & 0x0f00) | result;

	// First time, make sure there's no difference
	if (deck[1].encoderAngle == 0xffff)
		deck[1].encoderAngle = deck[1].newEncoderAngle;

	// Handle wrapping at zero

	if (deck[1].newEncoderAngle < 1024 && deck[1].encoderAngle >= 3072)
	{ // We crossed zero in the positive direction

		crossedZero = 1;
		wrappedAngle = deck[1].encoderAngle - 4096;
	}
	else if (deck[1].newEncoderAngle >= 3072 && deck[1].encoderAngle < 1024)
	{ // We crossed zero in the negative direction
		crossedZero = -1;
		wrappedAngle = deck[1].encoderAngle + 4096;
	}
	else
	{
		crossedZero = 0;
		wrappedAngle = deck[1].encoderAngle;
	}

	// rotary sensor sometimes returns incorrect values, if we skip more than 100 ignore that value
	// If we see 3 blips in a row, then I guess we better accept the new value
	if (abs(deck[1].newEncoderAngle - wrappedAngle) > 100 && numBlips < 2)
	{
		//printf("blip! %d %d %d\n", newEncoderAngle, encoderAngle, wrappedAngle);
		numBlips++;
	}
	else
	{
		numBlips = 0;
		deck[1].encoderAngle = deck[1].newEncoderAngle;

		if (pitchMode)
		{

			if (!oldPitchMode)
			{ // We just entered pitchmode, set offset etc

				deck[(pitchMode - 1)].player.nominal_pitch = 1.0;
				deck[1].angleOffset = -deck[1].encoderAngle;
				oldPitchMode = 1;
				capIsTouched = 0;
			}

			// Handle wrapping at zero

			if (crossedZero > 0)
			{
				deck[1].angleOffset += 4096;
			}
			else if (crossedZero < 0)
			{
				deck[1].angleOffset -= 4096;
			}

			// Use the angle of the platter to control sample pitch
			deck[(pitchMode - 1)].player.nominal_pitch = (((double)(deck[1].encoderAngle + deck[1].angleOffset)) / 16384) + 1.0;
		}
		else
		{

			if (scsettings.platterenabled)
			{
				// Handle touch sensor
				if (capIsTouched)
				{
					// Positive touching edge
					if (!deck[1].player.capTouch)
					{
						deck[1].angleOffset = (deck[1].player.position * scsettings.platterspeed) - deck[1].encoderAngle;
						printf("touch!\n");
						deck[1].player.target_position = deck[1].player.position;
						deck[1].player.capTouch = 1;
					}
				}
				else
				{
					deck[1].player.capTouch = 0;
				}
			}

			else
				deck[1].player.capTouch = 1;

			if (deck[1].player.capTouch)
			{

				// Handle wrapping at zero

				if (crossedZero > 0)
				{
					deck[1].angleOffset += 4096;
				}
				else if (crossedZero < 0)
				{
					deck[1].angleOffset -= 4096;
				}

				// Convert the raw value to track position and set player to that pos

				deck[1].player.target_position = (double)(deck[1].encoderAngle + deck[1].angleOffset) / scsettings.platterspeed;

				// Loop when track gets to end

				/*if (deck[1].player.target_position > ((double)deck[1].player.track->length / (double)deck[1].player.track->rate))
						{
							deck[1].player.target_position = 0;
							angleOffset = encoderAngle;
						}*/
			}
		}
		oldPitchMode = pitchMode;
	}
}

void *SC_InputThread(void *ptr)
{
	unsigned char picskip = 0;
	unsigned char picpresent = 1;
	unsigned char rotarypresent = 1;

	char mididevices[64][64];
	int mididevicenum = 0, oldmididevicenum = 0;

	// Initialise rotary sensor on I2C0

	if ((file_i2c_rot = setupi2c("/dev/i2c-0", 0x36)) < 0)
	{
		printf("Couldn't init rotary sensor\n");
		rotarypresent = 0;
	}

	// Initialise PIC input processor on I2C2

	if ((file_i2c_pic = setupi2c("/dev/i2c-2", 0x69)) < 0)
	{
		printf("Couldn't init input processor\n");
		picpresent = 0;
	}

	init_io();

	//detect SC500 by seeing if G11 is pulled high

	volatile uint32_t *PortDataReg = gpio_addr + (6 * 0x24) + 0x10;
	uint32_t PortData = *PortDataReg;
	PortData ^= 0xffffffff;
	if ((PortData >> 11) & 0x01)
	{
		printf("SC500 detected\n");
		scsettings.disablevolumeadc = 1;
		scsettings.disablepicbuttons = 1;
	}

	srand(time(NULL)); // TODO - need better entropy source, SoC is starting up annoyingly deterministically

	struct timeval tv;
	unsigned long lastTime = 0;
	unsigned int frameCount = 0;
	struct timespec ts;
	double inputtime = 0, lastinputtime = 0;

	sleep(2);

	int secondCount = 0;

	while (1) // Main input loop
	{

		frameCount++;

		// Update display every second
		gettimeofday(&tv, NULL);
		if (tv.tv_sec != lastTime)
		{
			lastTime = tv.tv_sec;
			printf("\033[H\033[J"); // Clear Screen
			printf("\nFPS: %06u - ADCS: %04u, %04u, %04u, %04u, %04u\nButtons: %01u,%01u,%01u,%01u,%01u\nTP: %f, P : %f\n%f -- %f\n",
				   frameCount, ADCs[0], ADCs[1], ADCs[2], ADCs[3], deck[1].encoderAngle,
				   buttons[0], buttons[1], buttons[2], buttons[3], capIsTouched,
				   deck[1].player.target_position, deck[1].player.position,
				   deck[0].player.setVolume, deck[1].player.setVolume);
				   	//dump_maps();

			//printf("\nFPS: %06u\n", frameCount);
			frameCount = 0;

			// list midi devices
			for (int cunt = 0; cunt < numControllers; cunt++)
			{
				printf("MIDI : %s\n", ((struct dicer *)(midiControllers[cunt].local))->PortName);
			}

			// Wait 10 seconds to enumerate MIDI devices
			// Give them a little time to come up properly
			if (secondCount < scsettings.mididelay)
				secondCount++;
			else if (secondCount == scsettings.mididelay)
			{
				// Check for new midi devices
				mididevicenum = listdev("rawmidi", mididevices);

				// If there are more MIDI devices than last time, add them
				if (mididevicenum > oldmididevicenum)
				{
					AddNewMidiDevices(mididevices, mididevicenum);
					oldmididevicenum = mididevicenum;
				}
				secondCount = 999;
			}
		}

		// Get info from input processor registers
		// First the ADC values
		// 5 = XFADER1, 6 = XFADER2, 7 = POT1, 8 = POT2

		//picpresent = 0;

		if (picpresent)
		{
			picskip++;
			if (picskip > 4)
			{
				picskip = 0;
				process_pic();
				firstTimeRound = 0;
			}

			process_rot();
		}
		else // couldn't find input processor, just play the tracks
		{
			deck[1].player.capTouch = 1;
			deck[0].player.faderTarget = 0.0;
			deck[1].player.faderTarget = 0.5;
			deck[0].player.justPlay = 1;
			deck[0].player.pitch = 1;

			clock_gettime(CLOCK_MONOTONIC, &ts);
			inputtime = (double)ts.tv_sec + ((double)ts.tv_nsec / 1000000000.0);

			if (lastinputtime != 0)
			{
				deck[1].player.target_position += (inputtime - lastinputtime);
			}

			lastinputtime = inputtime;
		}

		//usleep(scsettings.updaterate);

	}
}

// Start the input thread
void SC_Input_Start()
{

	pthread_t thread1;
	const char *message1 = "Thread 1";
	int iret1;

	iret1 = pthread_create(&thread1, NULL, SC_InputThread, (void *)message1);

	if (iret1)
	{
		fprintf(stderr, "Error - pthread_create() return code: %d\n", iret1);
		exit(EXIT_FAILURE);
	}
}
