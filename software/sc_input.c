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
#include <fcntl.h>                      //Needed for I2C port
#include <sys/ioctl.h>                  //Needed for I2C port
#include <linux/i2c-dev.h>              //Needed for I2C port
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

void i2c_read_address(int file_i2c, unsigned char address, unsigned char *result) {

	*result = address;
	if (write(file_i2c, result, 1) != 1)
		exit(1);

	if (read(file_i2c, result, 1) != 1)
		exit(1);

}

void *SC_InputThread(void *ptr) {

	int file_i2c_rot, file_i2c_pic;

	unsigned char result;
	int prevAngle = 0x0000;
	int encoderAngle = 0x0000;
	int wrappedAngle = 0x0000;
	unsigned int totalTurns = 0x0001;
	unsigned int ADCs[4] = { 0, 0, 0, 0 };
	bool capIsTouched = 0;
	uint32_t accumulatedPos = 0;
	unsigned char buttonState = 0;
	unsigned char buttons[4] = { 0, 0, 0, 0 }, totalbuttons[4] = { 0, 0, 0, 0 };
	unsigned int butCounter = 0;
	unsigned char i = 0;
	unsigned int NumBeats, NumSamples;
	struct Folder *FirstBeatFolder, *CurrentBeatFolder, *FirstSampleFolder, *CurrentSampleFolder;
	struct File *CurrentBeatFile, *CurrentSampleFile;

	// Initialise PIC input processor on I2C2

	if ((file_i2c_pic = open("/dev/i2c-2", O_RDWR)) < 0) {
		printf("I2C #2 (input processor) - Failed to open\n");
		return NULL;
	}
	if (ioctl(file_i2c_pic, I2C_SLAVE, 0x69) < 0) {
		printf("I2C #2 (input processor) - Failed to acquire bus access and/or talk to slave.\n");
		return NULL;
	}

	// Initialise rotary sensor on I2C1

	if ((file_i2c_rot = open("/dev/i2c-0", O_RDWR)) < 0) {
		printf("I2C #0 (rotary sensor) - Failed to open\n");
		return NULL;
	}
	if (ioctl(file_i2c_rot, I2C_SLAVE, 0x36) < 0) {
		printf("I2C #0 (rotary sensor) - Failed to acquire bus access and/or talk to slave.\n");
		return NULL;
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
	player_set_track(&deck[1].player, track_acquire_by_import(deck[1].importer, CurrentSampleFile->FullPath));

	srand (time(NULL)); // TODO - need better entropy source, SoC is starting up annoyingly deterministically
	
	/*
	struct timeval	tv;
	unsigned long lastTime = 0;
	unsigned int frameCount = 0;
	*/
	
	while (1) {
		
		/*
		frameCount++;
		 gettimeofday(&tv, NULL);
		 if (tv.tv_sec != lastTime) {
		 lastTime = tv.tv_sec;
		 printf("\nFPS : %u - %u %u %u %u\n", frameCount, ADCs[0], ADCs[1], ADCs[2], ADCs[3]);
		 frameCount = 0;
		 }
		 */

		// Get info from input processor registers
		// First the ADC values
		// 5 = XFADER1, 6 = XFADER2, 7 = POT1, 8 = POT2
		i2c_read_address(file_i2c_pic, 0x00, &result);
		ADCs[0] = result;
		i2c_read_address(file_i2c_pic, 0x01, &result);
		ADCs[1] = result;
		i2c_read_address(file_i2c_pic, 0x02, &result);
		ADCs[2] = result;
		i2c_read_address(file_i2c_pic, 0x03, &result);
		ADCs[3] = result;
		i2c_read_address(file_i2c_pic, 0x04, &result);
		ADCs[0] |= ((unsigned int) (result & 0x03) << 8);
		ADCs[1] |= ((unsigned int) (result & 0x0C) << 6);
		ADCs[2] |= ((unsigned int) (result & 0x30) << 4);
		ADCs[3] |= ((unsigned int) (result & 0xC0) << 2);

		// Now buttons and capsense

		i2c_read_address(file_i2c_pic, 0x05, &result);
		buttons[0] = !(result & 0x01);
		buttons[1] = !(result >> 1 & 0x01);
		buttons[2] = !(result >> 2 & 0x01);
		buttons[3] = !(result >> 3 & 0x01);
		capIsTouched = (result >> 4 & 0x01);

		// Apply volume and fader

		if (ADCs[0] > 5 && ADCs[1] > 5){		// cut on both sides of crossfader
			deck[1].player.faderTarget = ((double) ADCs[3]) / 1024;
		}
		else
			deck[1].player.faderTarget = 0.0;


		deck[0].player.faderTarget = ((double) ADCs[2]) / 1024;

		// Handle touch sensor
		if (capIsTouched) {
			if (!deck[1].player.capTouch) { // Positive touching edge
				accumulatedPos = (uint32_t)(deck[1].player.position * 3072);
				deck[1].player.target_position = deck[1].player.position;
				deck[1].player.capTouch = 1;
			}
		} else {
			deck[1].player.capTouch = 0;
		}
		
	
		if (deck[1].player.capTouch) {

			// Handle rotary sensor

			i2c_read_address(file_i2c_rot, 0x0e, &result);
			encoderAngle = result << 8;
			i2c_read_address(file_i2c_rot, 0x0f, &result);
			encoderAngle = (encoderAngle & 0x0f00) | result;

			// Handle wrapping at zero

			if (encoderAngle < 1024 && prevAngle >= 3072) { // We crossed zero in the positive direction
				totalTurns++;
				wrappedAngle = prevAngle - 4096;
			} else if (encoderAngle >= 3072 && prevAngle < 1024) { // We crossed zero in the negative direction
				totalTurns--;
				wrappedAngle = prevAngle + 4096;
			} else {
				wrappedAngle = prevAngle;
			}

			// rotary sensor sometimes returns incorrect values, if we skip more than 100 ignore that value

			if (abs(encoderAngle - wrappedAngle) > 100) {
				//printf("blip! %d %d %d\n", encoderAngle, wrappedAngle, accumulatedPos);
				prevAngle = encoderAngle;
			} else {
				prevAngle = encoderAngle;
				// Add the difference from the last angle to the position
				accumulatedPos += encoderAngle - wrappedAngle;
			}

			// Convert the raw value to track position and set player to that pos

			deck[1].player.target_position = ((double) accumulatedPos) / 3072;

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

		switch (buttonState) {

			// No buttons pressed
			case BUTTONSTATE_NONE:
				if (buttons[0] || buttons[1] || buttons[2] || buttons[3]) {
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
				if (butCounter > 250) {
					butCounter = 0;
					buttonState = BUTTONSTATE_ACTING_HELD;
				}

				break;
				
			// Act on instantaneous (i.e. not held) button press
			case BUTTONSTATE_ACTING_INSTANT:
				if (totalbuttons[0] && !totalbuttons[1] && !totalbuttons[2] && !totalbuttons[3]) {
					printf("Samples - Up pushed\n");
					if (CurrentSampleFile->prev != NULL) {
						CurrentSampleFile = CurrentSampleFile->prev;
						player_set_track(&deck[1].player, track_acquire_by_import(deck[0].importer, CurrentSampleFile->FullPath));
					}
				} else if (!totalbuttons[0] && totalbuttons[1] && !totalbuttons[2] && !totalbuttons[3]) {
					printf("Samples - Down pushed\n");
					if (CurrentSampleFile->next != NULL) {
						CurrentSampleFile = CurrentSampleFile->next;
						player_set_track(&deck[1].player, track_acquire_by_import(deck[0].importer, CurrentSampleFile->FullPath));
					}
				} else if (totalbuttons[0] && totalbuttons[1] && !totalbuttons[2] && !totalbuttons[3]) {
					printf("Samples - both buttons pushed\n");
					r = rand() % NumSamples;
					printf("Playing file %d/%d\n", r, NumSamples);
					player_set_track(&deck[1].player, track_acquire_by_import(deck[0].importer, GetFileAtIndex(r, FirstSampleFolder)->FullPath));
				}

				else if (!totalbuttons[0] && !totalbuttons[1] && totalbuttons[2] && !totalbuttons[3]) {
					printf("Beats - Up pushed\n");
					if (CurrentBeatFile->prev != NULL) {
						CurrentBeatFile = CurrentBeatFile->prev;
						player_set_track(&deck[0].player, track_acquire_by_import(deck[0].importer, CurrentBeatFile->FullPath));
					}
				} else if (!totalbuttons[0] && !totalbuttons[1] && !totalbuttons[2] && totalbuttons[3]) {
					printf("Beats - Down pushed\n");
					if (CurrentBeatFile->next != NULL) {
						CurrentBeatFile = CurrentBeatFile->next;
						player_set_track(&deck[0].player, track_acquire_by_import(deck[0].importer, CurrentBeatFile->FullPath));
					}
				} else if (!totalbuttons[0] && !totalbuttons[1] && totalbuttons[2] && totalbuttons[3]) {
					printf("Beats - both buttons pushed\n");
					r = rand() % NumBeats;
					printf("Playing file %d/%d\n", r, NumBeats);
					player_set_track(&deck[0].player, track_acquire_by_import(deck[0].importer, GetFileAtIndex(r, FirstBeatFolder)->FullPath));
				}

				else if (totalbuttons[0] && totalbuttons[1] && totalbuttons[2] && totalbuttons[3])
					printf("All buttons pushed!\n");

				else
					printf("Sod knows what you were trying to do there\n");

				buttonState = BUTTONSTATE_WAITING;

				break;

				
			// Act on whatever buttons are being held down when the timeout happens
			case BUTTONSTATE_ACTING_HELD: 
				if (buttons[0] && !buttons[1] && !buttons[2] && !buttons[3]) {
					printf("Samples - Up held\n");
					if (CurrentSampleFolder->prev != NULL) {
						CurrentSampleFolder = CurrentSampleFolder->prev;
						CurrentSampleFile = CurrentSampleFolder->FirstFile;
						player_set_track(&deck[1].player, track_acquire_by_import(deck[0].importer, CurrentSampleFile->FullPath));
					}
				} else if (!buttons[0] && buttons[1] && !buttons[2] && !buttons[3]) {
					printf("Samples - Down held\n");
					if (CurrentSampleFolder->next != NULL) {
						CurrentSampleFolder = CurrentSampleFolder->next;
						CurrentSampleFile = CurrentSampleFolder->FirstFile;
						player_set_track(&deck[1].player, track_acquire_by_import(deck[0].importer, CurrentSampleFile->FullPath));
					}
				} else if (buttons[0] && buttons[1] && !buttons[2] && !buttons[3])
					printf("Samples - both buttons held\n");

				else if (!buttons[0] && !buttons[1] && buttons[2] && !buttons[3]) {
					printf("Beats - Up held\n");
					if (CurrentBeatFolder->prev != NULL) {
						CurrentBeatFolder = CurrentBeatFolder->prev;
						CurrentBeatFile = CurrentBeatFolder->FirstFile;
						player_set_track(&deck[0].player, track_acquire_by_import(deck[0].importer, CurrentBeatFile->FullPath));
					}
				} else if (!buttons[0] && !buttons[1] && !buttons[2] && buttons[3]) {
					printf("Beats - Down held\n");
					if (CurrentBeatFolder->next != NULL) {
						CurrentBeatFolder = CurrentBeatFolder->next;
						CurrentBeatFile = CurrentBeatFolder->FirstFile;
						player_set_track(&deck[0].player, track_acquire_by_import(deck[0].importer, CurrentBeatFile->FullPath));
					}
				} else if (!buttons[0] && !buttons[1] && buttons[2] && buttons[3])
					printf("Beats - both buttons held\n");

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

				if (butCounter > 20) {
					butCounter = 0;
					buttonState = BUTTONSTATE_NONE;

					for (i = 0; i < 4; i++)
						totalbuttons[i] = 0;
				}
				break;

		}

		usleep(1000);
	}
}

// Start the input thread
void SC_Input_Start() {

	pthread_t thread1;
	const char *message1 = "Thread 1";
	int iret1;

	iret1 = pthread_create(&thread1, NULL, SC_InputThread, (void*) message1);

	if (iret1) {
		fprintf(stderr, "Error - pthread_create() return code: %d\n", iret1);
		exit (EXIT_FAILURE);
	}

}
