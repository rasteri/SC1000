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
#include <sys/ioctl.h>	 //Needed for I2C port
#include <linux/i2c-dev.h> //Needed for I2C port
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
		exit(1);

	if (read(file_i2c, result, 1) != 1)
		exit(1);
}

void i2c_write_address(int file_i2c, unsigned char address, unsigned char value)
{
	char buf[2];
	buf[0] = address;
	buf[1] = value;
	if (write(file_i2c, buf, 2) != 2)
	{
		printf("I2C Write Error\n");
		//exit(1);
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

int32_t angleOffset = 0; // Offset between encoder angle and track position, reset every time the platter is touched
int encoderAngle = 0xffff, newEncoderAngle = 0xffff;

void load_track(struct deck *d, struct track *track)
{
	struct player *pl = &d->player;
	cues_save_to_file(&d->cues, pl->track->path);
	player_set_track(pl, track);
	pl->target_position = 0;
	pl->position = 0;
	pl->offset = 0;
	cues_load_from_file(&d->cues, pl->track->path);
	pl->nominal_pitch = 1.0;
}

void load_and_sync_encoder(struct deck *d, struct track *track)
{
	struct player *pl = &d->player;
	cues_save_to_file(&d->cues, pl->track->path);
	player_set_track(pl, track);
	pl->target_position = 0;
	pl->position = 0;
	pl->offset = 0;
	cues_load_from_file(&d->cues, pl->track->path);
	pl->nominal_pitch = 1.0;
	// If touch sensor is enabled, set the "zero point" to the current encoder angle
	if (scsettings.platterenabled)
		angleOffset = 0 - encoderAngle;

	else // If touch sensor is disabled, set the "zero point" to encoder zero point so sticker is exactly on each time sample is loaded
		angleOffset = (pl->position * scsettings.platterspeed) - encoderAngle;
}

/*
 * Process an IO event
 */

static void IOevent(unsigned char pin, bool edge)
{
	//	printf("%x %x %x\n",d->MidiBuffer[0], d->MidiBuffer[1], d->MidiBuffer[2]);
	struct mapping *map = find_IO_mapping(maps, pin, edge);
	unsigned int pval;

	if (map != NULL)
	{
		printf("Map notnull %d %d %d\n", map->DeckNo, map->Action, map->Param);

		if (map->Action == ACTION_CUE)
		{
			if (shifted || shiftLatched)
			{
				deck_unset_cue(&deck[map->DeckNo], pin);
				shiftLatched = 0;
			}
			else
				deck_cue(&deck[map->DeckNo], pin);
		}
		else if (map->Action == ACTION_NOTE)
		{
			deck[map->DeckNo].player.nominal_pitch = pow(pow(2, (double)1 / 12), map->Param - 0x3C); // equal temperament
		}
		else if (map->Action == ACTION_STARTSTOP)
		{
			printf("Startstop %d %d\n", map->DeckNo, deck[map->DeckNo].player.stopped);
			deck[map->DeckNo].player.stopped = !deck[map->DeckNo].player.stopped;
		}
		else if (map->Action == ACTION_SHIFTON)
		{
			shifted = 1;
		}
		else if (map->Action == ACTION_SHIFTOFF)
		{
			shiftLatched = 0;
			shifted = 0;
		}
	}
}

void *SC_InputThread(void *ptr)
{

	int file_i2c_rot, file_i2c_pic, file_i2c_gpio;

	unsigned char result;
	unsigned char picskip = 0;

	int wrappedAngle = 0x0000;
	unsigned int totalTurns = 0x0001;
	unsigned int ADCs[4] = {0, 0, 0, 0};
	unsigned int numBlips = 0;
	bool capIsTouched = 0;
	uint32_t accumulatedPos = 0;
	unsigned char buttonState = 0;
	unsigned char buttons[4] = {0, 0, 0, 0}, totalbuttons[4] = {0, 0, 0, 0};
	unsigned int butCounter = 0;
	unsigned int i = 0;
	unsigned int NumBeats, NumSamples;
	struct Folder *FirstBeatFolder, *CurrentBeatFolder, *FirstSampleFolder, *CurrentSampleFolder;
	struct File *CurrentBeatFile, *CurrentSampleFile;
	unsigned char faderOpen = 0;
	unsigned int faderCutPoint;
	unsigned char picpresent = 1;
	unsigned char rotarypresent = 1;
	unsigned char gpiopresent = 1;
	unsigned int gpios;
	int pitchMode = 0; // If we're in pitch-change mode
	int oldPitchMode = 0;
	unsigned int pullups = 0, iodirs = 0;
	struct mapping *map;

	bool alreadyAdded = 0;

	char mididevices[32][32];
	int mididevicenum = 0, oldmididevicenum = 0;

	int gpiodebounce[16];

	int8_t crossedZero; // 0 when we haven't crossed zero, -1 when we've crossed in anti-clockwise direction, 1 when crossed in clockwise

	// Initialise rotary sensor on I2C0

	if ((file_i2c_rot = setupi2c("/dev/i2c-0", 0x36)) < 0)
	{
		printf("Couldn't init rotary sensor\n");
		rotarypresent = 0;
	}

	// Initialise external MCP23017 GPIO on I2C1

	if ((file_i2c_gpio = setupi2c("/dev/i2c-1", 0x20)) < 0)
	{
		printf("Couldn't init external GPIO\n");
		gpiopresent = 0;
	}

	// Initialise PIC input processor on I2C2

	if ((file_i2c_pic = setupi2c("/dev/i2c-2", 0x69)) < 0)
	{
		printf("Couldn't init input processor\n");
		picpresent = 0;
	}

	// Configure GPIO
	if (gpiopresent)
	{

		// default to pulled up and input
		pullups = 0xFF;
		iodirs = 0xFF;

		// For each pin
		for (i = 0; i < 16; i++)
		{
			map = find_IO_mapping(maps, i, 1);
			// If pin is marked as ground
			if (map != NULL && map->Action == ACTION_GND)
			{
				printf("Grounding pin %d\n", i);
				iodirs &= ~(0x01 << i);
			}

			// If pin's pullup is disabled
			if (map != NULL && !map->Pullup)
			{
				printf("Disabling pin %d pullup\n", i);
				pullups &= ~(0x01 << i);
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
	}

	// Build index of all audio files on the USB stick

	FirstBeatFolder = LoadFileStructure("/media/sda/beats/", &NumBeats);
	//DumpFileStructure(FirstBeatFolder);
	CurrentBeatFolder = FirstBeatFolder;
	CurrentBeatFile = CurrentBeatFolder->FirstFile;

	FirstSampleFolder = LoadFileStructure("/media/sda/samples/", &NumSamples);
	//DumpFileStructure(FirstSampleFolder);
	CurrentSampleFolder = FirstSampleFolder;
	CurrentSampleFile = CurrentSampleFolder->FirstFile;

	// Load the first track

	player_set_track(&deck[0].player, track_acquire_by_import(deck[0].importer, CurrentBeatFile->FullPath));
	cues_load_from_file(&deck[0].cues, deck[0].player.track->path);
	player_set_track(&deck[1].player, track_acquire_by_import(deck[1].importer, CurrentSampleFile->FullPath));
	cues_load_from_file(&deck[1].cues, deck[1].player.track->path);

	srand(time(NULL)); // TODO - need better entropy source, SoC is starting up annoyingly deterministically

	struct timeval tv;
	unsigned long lastTime = 0;
	unsigned int frameCount = 0;
	struct timespec ts;
	double inputtime = 0, lastinputtime = 0;
	int decknum = 0, cuepointnum = 0;

	/*if (dicer_init(&midiControllers[0], &rt, "hw:0,0") != -1){
		printf("Added cntrl\n");
		controller_add_deck(&midiControllers[0], &deck[0]);
		controller_add_deck(&midiControllers[0], &deck[1]);
		
	*/
	sleep(2);
	for (i = 0; i < 16; i++)
		gpiodebounce[i] = 0;

	while (1)
	{

		frameCount++;

		// Update display every second
		gettimeofday(&tv, NULL);
		if (tv.tv_sec != lastTime)
		{
			lastTime = tv.tv_sec;
			printf("\033[H\033[J"); // Clear Screen
			printf("\nFPS: %06u - ADCS: %04u, %04u, %04u, %04u, %04u\nButtons: %01u,%01u,%01u,%01u,%01u\nTP: %f, P : %f\n",
				   frameCount, ADCs[0], ADCs[1], ADCs[2], ADCs[3], encoderAngle,
				   buttons[0], buttons[1], buttons[2], buttons[3], capIsTouched,
				   deck[1].player.target_position, deck[1].player.position);

			frameCount = 0;

			for (int cunt = 0; cunt < numControllers; cunt++)
			{
				printf("MIDI : %s\n", ((struct dicer *)(midiControllers[cunt].local))->PortName);
			}

			// Also check midi devices every second
			mididevicenum = getmididevices(mididevices);

			// If there are more MIDI devices than last time, add them
			if (mididevicenum > oldmididevicenum)
			{

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
							numControllers ++;
						}
					}
				}

				oldmididevicenum = mididevicenum;
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

					for (i = 0; i < 16; i++)
					{
						// gpiodebounce = 0 when button not pressed,
						// > 0 and < scsettings.debouncetime when debouncing positive edge
						// > scsettings.debouncetime and < scsettings.holdtime when holding
						// = scsettings.holdtime when continuing to hold
						// > scsettings.holdtime when waiting for release
						// > -scsettings.debouncetime and < 0 when debouncing negative edge

						// Button not pressed, check for button
						if (gpiodebounce[i] == 0)
						{
							if (gpios & (0x01 << i))
							{
								printf("Button %d pressed\n", i);

								IOevent(i, 1);

								// start the counter
								gpiodebounce[i]++;
							}
						}

						// Debouncing positive edge, increment value
						else if (gpiodebounce[i] > 0 && gpiodebounce[i] < scsettings.debouncetime)
						{
							gpiodebounce[i]++;
						}

						// debounce finished, keep incrementing until hold reached
						else if (gpiodebounce[i] >= scsettings.debouncetime && gpiodebounce[i] < scsettings.holdtime)
						{
							// check to see if unpressed
							if (!(gpios & (0x01 << i)))
							{
								printf("Button %d released\n", i);
								IOevent(i, 0);
								// start the counter
								gpiodebounce[i] = -scsettings.debouncetime;
							}

							else
								gpiodebounce[i]++;
						}
						// Button has been held for a while
						else if (gpiodebounce[i] == scsettings.holdtime)
						{
							printf("Button %d held\n", i);

							gpiodebounce[i]++;
						}

						// Button still holding, check for release
						else if (gpiodebounce[i] > scsettings.holdtime)
						{
							// check to see if unpressed
							if (!(gpios & (0x01 << i)))
							{
								printf("Button %d released\n", i);
								IOevent(i, 0);
								// start the counter
								gpiodebounce[i] = -scsettings.debouncetime;
							}
						}

						// Debouncing negative edge, increment value - will reset when zero is reached
						else if (gpiodebounce[i] < 0)
						{
							gpiodebounce[i]++;
						}
					}
				}
			}

			// Apply volume and fader

			faderCutPoint = faderOpen ? scsettings.faderclosepoint : scsettings.faderopenpoint; // Fader Hysteresis

			if (ADCs[0] > faderCutPoint && ADCs[1] > faderCutPoint)
			{ // cut on both sides of crossfader
				deck[1].player.faderTarget = ((double)ADCs[3]) / 1024;
				faderOpen = 1;
			}
			else
			{
				deck[1].player.faderTarget = 0.0;
				faderOpen = 0;
			}

			deck[0].player.faderTarget = ((double)ADCs[2]) / 1024;

			// Handle rotary sensor

			i2c_read_address(file_i2c_rot, 0x0c, &result);
			newEncoderAngle = result << 8;
			i2c_read_address(file_i2c_rot, 0x0d, &result);
			newEncoderAngle = (newEncoderAngle & 0x0f00) | result;

			// First time, make sure there's no difference
			if (encoderAngle == 0xffff)
				encoderAngle = newEncoderAngle;

			// Handle wrapping at zero

			if (newEncoderAngle < 1024 && encoderAngle >= 3072)
			{ // We crossed zero in the positive direction

				crossedZero = 1;
				wrappedAngle = encoderAngle - 4096;
			}
			else if (newEncoderAngle >= 3072 && encoderAngle < 1024)
			{ // We crossed zero in the negative direction
				crossedZero = -1;
				wrappedAngle = encoderAngle + 4096;
			}
			else
			{
				crossedZero = 0;
				wrappedAngle = encoderAngle;
			}

			// rotary sensor sometimes returns incorrect values, if we skip more than 100 ignore that value
			// If we see 3 blips in a row, then I guess we better accept the new value
			if (abs(newEncoderAngle - wrappedAngle) > 100 && numBlips < 2)
			{
				//printf("blip! %d %d %d\n", newEncoderAngle, encoderAngle, wrappedAngle);
				numBlips++;
			}
			else
			{
				numBlips = 0;
				encoderAngle = newEncoderAngle;

				if (pitchMode)
				{

					if (!oldPitchMode)
					{ // We just entered pitchmode, set offset etc

						deck[(pitchMode - 1)].player.nominal_pitch = 1.0;
						angleOffset = -encoderAngle;
						oldPitchMode = 1;
						capIsTouched = 0;
					}

					// Handle wrapping at zero

					if (crossedZero > 0)
					{
						angleOffset += 4096;
					}
					else if (crossedZero < 0)
					{
						angleOffset -= 4096;
					}

					// Use the angle of the platter to control sample pitch
					deck[(pitchMode - 1)].player.nominal_pitch = (((double)(encoderAngle + angleOffset)) / 16384) + 1.0;
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
								angleOffset = (deck[1].player.position * scsettings.platterspeed) - encoderAngle;
								//printf("touch! %d %d\n", encoderAngle, angleOffset);
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
							angleOffset += 4096;
							printf("CZ+\n");
						}
						else if (crossedZero < 0)
						{
							angleOffset -= 4096;
							printf("CZ-\n");
						}

						// Convert the raw value to track position and set player to that pos

						deck[1].player.target_position = (double)(encoderAngle + angleOffset) / scsettings.platterspeed;
					}
				}
			}

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
				}
				break;

			// At least one button pressed
			case BUTTONSTATE_PRESSING:
				for (i = 0; i < 4; i++)
					totalbuttons[i] |= buttons[i];

				if (!(buttons[0] || buttons[1] || buttons[2] || buttons[3]))
					buttonState = BUTTONSTATE_ACTING_INSTANT;

				butCounter++;
				if (butCounter > 250)
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
				else if (totalbuttons[0] && !totalbuttons[1] && !totalbuttons[2] && !totalbuttons[3])
				{
					printf("Samples - Up pushed\n");
					if (CurrentSampleFile->prev != NULL)
					{
						CurrentSampleFile = CurrentSampleFile->prev;
					}
					load_and_sync_encoder(&deck[1], track_acquire_by_import(deck[0].importer, CurrentSampleFile->FullPath));
				}
				else if (!totalbuttons[0] && totalbuttons[1] && !totalbuttons[2] && !totalbuttons[3])
				{
					printf("Samples - Down pushed\n");
					if (CurrentSampleFile->next != NULL)
					{
						CurrentSampleFile = CurrentSampleFile->next;
					}
					load_and_sync_encoder(&deck[1], track_acquire_by_import(deck[0].importer, CurrentSampleFile->FullPath));
				}
				else if (totalbuttons[0] && totalbuttons[1] && !totalbuttons[2] && !totalbuttons[3])
				{
					printf("Samples - both buttons pushed\n");
					pitchMode = 2;
				}

				else if (!totalbuttons[0] && !totalbuttons[1] && totalbuttons[2] && !totalbuttons[3])
				{
					printf("Beats - Up pushed\n");
					if (CurrentBeatFile->prev != NULL)
					{
						CurrentBeatFile = CurrentBeatFile->prev;
					}
					load_track(&deck[0], track_acquire_by_import(deck[0].importer, CurrentBeatFile->FullPath));
				}
				else if (!totalbuttons[0] && !totalbuttons[1] && !totalbuttons[2] && totalbuttons[3])
				{
					printf("Beats - Down pushed\n");
					if (CurrentBeatFile->next != NULL)
					{
						CurrentBeatFile = CurrentBeatFile->next;
					}
					load_track(&deck[0], track_acquire_by_import(deck[0].importer, CurrentBeatFile->FullPath));
				}
				else if (!totalbuttons[0] && !totalbuttons[1] && totalbuttons[2] && totalbuttons[3])
				{
					printf("Beats - both buttons pushed\n");
					pitchMode = 1;
				}

				else if (totalbuttons[0] && totalbuttons[1] && totalbuttons[2] && totalbuttons[3])
				{
					printf("All buttons pushed!\n");
					shiftLatched = 1;
				}

				else
					printf("Sod knows what you were trying to do there\n");

				buttonState = BUTTONSTATE_WAITING;

				break;

			// Act on whatever buttons are being held down when the timeout happens
			case BUTTONSTATE_ACTING_HELD:
				if (buttons[0] && !buttons[1] && !buttons[2] && !buttons[3])
				{
					printf("Samples - Up held\n");
					if (CurrentSampleFolder->prev != NULL)
					{
						CurrentSampleFolder = CurrentSampleFolder->prev;
						CurrentSampleFile = CurrentSampleFolder->FirstFile;
						load_and_sync_encoder(&deck[1], track_acquire_by_import(deck[1].importer, CurrentSampleFile->FullPath));
					}
				}
				else if (!buttons[0] && buttons[1] && !buttons[2] && !buttons[3])
				{
					printf("Samples - Down held\n");
					if (CurrentSampleFolder->next != NULL)
					{
						CurrentSampleFolder = CurrentSampleFolder->next;
						CurrentSampleFile = CurrentSampleFolder->FirstFile;
						load_and_sync_encoder(&deck[1], track_acquire_by_import(deck[1].importer, CurrentSampleFile->FullPath));
					}
				}
				else if (buttons[0] && buttons[1] && !buttons[2] && !buttons[3])
				{
					printf("Samples - both buttons held\n");
					r = rand() % NumSamples;
					printf("Playing file %d/%d\n", r, NumSamples);
					load_and_sync_encoder(&deck[1], track_acquire_by_import(deck[1].importer, GetFileAtIndex(r, FirstSampleFolder)->FullPath));
					deck[1].player.nominal_pitch = 1.0;
				}

				else if (!buttons[0] && !buttons[1] && buttons[2] && !buttons[3])
				{
					printf("Beats - Up held\n");
					if (CurrentBeatFolder->prev != NULL)
					{
						CurrentBeatFolder = CurrentBeatFolder->prev;
						CurrentBeatFile = CurrentBeatFolder->FirstFile;
						load_track(&deck[0], track_acquire_by_import(deck[0].importer, CurrentBeatFile->FullPath));
					}
				}
				else if (!buttons[0] && !buttons[1] && !buttons[2] && buttons[3])
				{
					printf("Beats - Down held\n");

					if (CurrentBeatFolder->next != NULL)
					{
						CurrentBeatFolder = CurrentBeatFolder->next;
						CurrentBeatFile = CurrentBeatFolder->FirstFile;
						load_track(&deck[0], track_acquire_by_import(deck[0].importer, CurrentBeatFile->FullPath));
					}
				}
				else if (!buttons[0] && !buttons[1] && buttons[2] && buttons[3])
				{
					printf("Beats - both buttons held\n");
					r = rand() % NumBeats;
					printf("Playing file %d/%d\n", r, NumBeats);
					load_track(&deck[0], track_acquire_by_import(deck[0].importer, GetFileAtIndex(r, FirstBeatFolder)->FullPath));
				}

				else if (buttons[0] && buttons[1] && buttons[2] && buttons[3])
					printf("All buttons held!\n");

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
