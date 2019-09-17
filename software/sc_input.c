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
#include <time.h>
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
#include "sc_queue.h"


void i2c_read_address(int file_i2c, unsigned char address, unsigned char *result)
{

	*result = address;
	if (write(file_i2c, result, 1) != 1)
		exit(1);

	if (read(file_i2c, result, 1) != 1)
		exit(1);
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

void load_and_sync_encoder(struct player *pl, struct track *track)
{
	player_set_track(pl, track);
	pl->target_position = 0;
	pl->position = 0;

	// If touch sensor is enabled, set the "zero point" to the current encoder angle
	if (scsettings.platterenabled)
		angleOffset = (pl->position * scsettings.platterspeed) - encoderAngle;

	else // If touch sensor is disabled, set the "zero point" to encoder zero point so sticker is exactly on each time sample is loaded
		angleOffset = (pl->position * scsettings.platterspeed) - encoderAngle;
}

unsigned int numblocks = 0;

void *SC_InputThread(void *ptr)
{

	int file_i2c_rot, file_i2c_pic;

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
	unsigned char i = 0;
	unsigned int NumBeats, NumSamples;
	struct Folder *FirstBeatFolder, *CurrentBeatFolder, *FirstSampleFolder, *CurrentSampleFolder;
	struct File *CurrentBeatFile, *CurrentSampleFile;
	unsigned char faderOpen = 0;
	unsigned int faderCutPoint;
	unsigned char picpresent = 1;
	unsigned char rotarypresent = 1;

	int8_t crossedZero; // 0 when we haven't crossed zero, -1 when we've crossed in anti-clockwise direction, 1 when crossed in clockwise

	// Initialise PIC input processor on I2C2

	if ((file_i2c_pic = setupi2c("/dev/i2c-2", 0x69)) < 0)
	{
		printf("Couldn't init input processor\n");
		picpresent = 0;
	}

	// Initialise rotary sensor on I2C1

	if ((file_i2c_rot = setupi2c("/dev/i2c-0", 0x36)) < 0)
	{
		printf("Couldn't init rotary sensor\n");
		rotarypresent = 0;
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

	srand(time(NULL)); // TODO - need better entropy source, SoC is starting up annoyingly deterministically

	struct timeval tv;
	unsigned long lastTime = 0;
	double inputtime =0, lastinputtime = 0;
	unsigned int frameCount = 0;
	long lastsamplecount = 0;

	struct timespec ts;

	deck[1].player.target_position = 0;
	sleep(2);
	inputstate sq;
	sq.target_position = 0;

	while (1)
	{

		frameCount++;
		gettimeofday(&tv, NULL);
		if (tv.tv_sec != lastTime)
		{
			lastTime = tv.tv_sec;
			//printf("%d\n", lastusec);
			printf("\033[H\033[J"); // Clear Screen
			printf("\nFPS: %06u - ADCS: %04u, %04u, %04u, %04u, %04u\nButtons: %01u,%01u,%01u,%01u,%01u\n%f %f  -- %f - %f = %f\n%d\n",
				   frameCount, ADCs[0], ADCs[1], ADCs[2], ADCs[3], encoderAngle,
				   buttons[0], buttons[1], buttons[2], buttons[3], capIsTouched,
				   deck[1].player.position, sq.target_position, inputtime, deck[1].player.timestamp, inputtime - deck[1].player.timestamp,
				   deck[1].player.samplesSoFar

			);
			deck[1].player.samplesSoFar = 0;
			lastsamplecount = deck[1].player.samplesSoFar;
			numblocks = 0;
			frameCount = 0;
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
				printf("blip! %d %d %d\n", newEncoderAngle, encoderAngle, wrappedAngle);
				numBlips++;
			}
			else
			{
				numBlips = 0;
				encoderAngle = newEncoderAngle;

				if (scsettings.platterenabled)
				{
					// Handle touch sensor
					if (capIsTouched)
					{
						// Positive touching edge
						if (!deck[1].player.capTouch)
						{
							angleOffset = (deck[1].player.position * scsettings.platterspeed) - encoderAngle;
							printf("touch! %d %d\n", encoderAngle, angleOffset);
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
					
					clock_gettime(CLOCK_MONOTONIC, &ts);
					inputtime = (double)ts.tv_sec + ((double)ts.tv_nsec / 1000000000.0);

					sq.timestamp = inputtime;
					if (lastinputtime != 0){
						sq.target_position = (double)(encoderAngle + angleOffset) / scsettings.platterspeed;
						spin_lock(&deck[1].player.lock); /* Synchronise with the playback thread */
						char res = fifoWrite(deck[1].player.scqueue, &sq);
						spin_unlock(&deck[1].player.lock);
						
					}
					lastinputtime = inputtime;

					//deck[1].player.target_position = (double)(encoderAngle + angleOffset) / scsettings.platterspeed;
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
				if (totalbuttons[0] && !totalbuttons[1] && !totalbuttons[2] && !totalbuttons[3])
				{
					printf("Samples - Up pushed\n");
					if (CurrentSampleFile->prev != NULL)
					{
						CurrentSampleFile = CurrentSampleFile->prev;
						load_and_sync_encoder(&deck[1].player, track_acquire_by_import(deck[0].importer, CurrentSampleFile->FullPath));
					}
				}
				else if (!totalbuttons[0] && totalbuttons[1] && !totalbuttons[2] && !totalbuttons[3])
				{
					printf("Samples - Down pushed\n");
					if (CurrentSampleFile->next != NULL)
					{
						CurrentSampleFile = CurrentSampleFile->next;
						load_and_sync_encoder(&deck[1].player, track_acquire_by_import(deck[0].importer, CurrentSampleFile->FullPath));
					}
				}
				else if (totalbuttons[0] && totalbuttons[1] && !totalbuttons[2] && !totalbuttons[3])
				{
					printf("Samples - both buttons pushed\n");
					r = rand() % NumSamples;
					printf("Playing file %d/%d\n", r, NumSamples);
					load_and_sync_encoder(&deck[1].player, track_acquire_by_import(deck[0].importer, GetFileAtIndex(r, FirstSampleFolder)->FullPath));
				}

				else if (!totalbuttons[0] && !totalbuttons[1] && totalbuttons[2] && !totalbuttons[3])
				{
					printf("Beats - Up pushed\n");
					if (CurrentBeatFile->prev != NULL)
					{
						CurrentBeatFile = CurrentBeatFile->prev;
						player_set_track(&deck[0].player, track_acquire_by_import(deck[0].importer, CurrentBeatFile->FullPath));
					}
				}
				else if (!totalbuttons[0] && !totalbuttons[1] && !totalbuttons[2] && totalbuttons[3])
				{
					printf("Beats - Down pushed\n");
					if (CurrentBeatFile->next != NULL)
					{
						CurrentBeatFile = CurrentBeatFile->next;
						player_set_track(&deck[0].player, track_acquire_by_import(deck[0].importer, CurrentBeatFile->FullPath));
					}
				}
				else if (!totalbuttons[0] && !totalbuttons[1] && totalbuttons[2] && totalbuttons[3])
				{
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
				if (buttons[0] && !buttons[1] && !buttons[2] && !buttons[3])
				{
					printf("Samples - Up held\n");
					if (CurrentSampleFolder->prev != NULL)
					{
						CurrentSampleFolder = CurrentSampleFolder->prev;
						CurrentSampleFile = CurrentSampleFolder->FirstFile;
						load_and_sync_encoder(&deck[1].player, track_acquire_by_import(deck[0].importer, CurrentSampleFile->FullPath));
					}
				}
				else if (!buttons[0] && buttons[1] && !buttons[2] && !buttons[3])
				{
					printf("Samples - Down held\n");
					if (CurrentSampleFolder->next != NULL)
					{
						CurrentSampleFolder = CurrentSampleFolder->next;
						CurrentSampleFile = CurrentSampleFolder->FirstFile;
						load_and_sync_encoder(&deck[1].player, track_acquire_by_import(deck[0].importer, CurrentSampleFile->FullPath));
					}
				}
				else if (buttons[0] && buttons[1] && !buttons[2] && !buttons[3])
					printf("Samples - both buttons held\n");

				else if (!buttons[0] && !buttons[1] && buttons[2] && !buttons[3])
				{
					printf("Beats - Up held\n");
					if (CurrentBeatFolder->prev != NULL)
					{
						CurrentBeatFolder = CurrentBeatFolder->prev;
						CurrentBeatFile = CurrentBeatFolder->FirstFile;
						player_set_track(&deck[0].player, track_acquire_by_import(deck[0].importer, CurrentBeatFile->FullPath));
					}
				}
				else if (!buttons[0] && !buttons[1] && !buttons[2] && buttons[3])
				{
					printf("Beats - Down held\n");
					if (CurrentBeatFolder->next != NULL)
					{
						CurrentBeatFolder = CurrentBeatFolder->next;
						CurrentBeatFile = CurrentBeatFolder->FirstFile;
						player_set_track(&deck[0].player, track_acquire_by_import(deck[0].importer, CurrentBeatFile->FullPath));
					}
				}
				else if (!buttons[0] && !buttons[1] && buttons[2] && buttons[3])
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
			sq.target_fader = 0.5;
			
			clock_gettime(CLOCK_MONOTONIC, &ts);
			inputtime = (double)ts.tv_sec + ((double)ts.tv_nsec / 1000000000.0);

			sq.timestamp = inputtime;
			if (lastinputtime != 0){
				sq.target_position += ((double)(inputtime - lastinputtime));
				spin_lock(&deck[1].player.lock); /* Synchronise with the playback thread */
				char res = fifoWrite(deck[1].player.scqueue, &sq);
				spin_unlock(&deck[1].player.lock);
				
				//printf("%f\n", sq.target_position);
				//deck[1].player.target_position += ((double)(usec - lastusec))/1000000;
			}
			//printf("-------------------%u\n",usec - lastusec );
			
			lastinputtime = inputtime;
			
		}

		//usleep(400);
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
