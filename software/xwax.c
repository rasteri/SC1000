/*
 * Copyright (C) 2018 Mark Hills <mark@xwax.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 *
 * You should have received a copy of the GNU General Public License
 * version 2 along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */

#include <assert.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h> /* mlockall() */

#include <SDL.h> /* may override main() */
#include <unistd.h>                             //Needed for I2C port
#include <fcntl.h>                              //Needed for I2C port
#include <sys/ioctl.h>                  //Needed for I2C port
#include <linux/i2c-dev.h>              //Needed for I2C port
#include <time.h>
#include <dirent.h>

#include "alsa.h"
#include "controller.h"
#include "device.h"
#include "dummy.h"
#include "realtime.h"
#include "thread.h"
#include "rig.h"
#include "track.h"
#include "xwax.h"

#define DEFAULT_OSS_BUFFERS 8
#define DEFAULT_OSS_FRAGMENT 7

#define DEFAULT_ALSA_BUFFER 5 /* milliseconds, change this to reduce latency*/

#define DEFAULT_RATE 48000
#define DEFAULT_PRIORITY 0 

#define DEFAULT_IMPORTER EXECDIR "/xwax-import"
#define DEFAULT_SCANNER EXECDIR "/xwax-scan"
#define DEFAULT_TIMECODE "serato_2a"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*x))

char *banner = "xwax " VERSION " Butchered by rasteri for the sc1000";

size_t ndeck;
struct deck deck[3];

static size_t nctl;
static struct controller ctl[2];

static struct rt rt;

static double speed;
static bool protect, phono;
static const char *importer;

int file_i2c;
unsigned char buff[60] = { 0 };

void i2c_read_address(unsigned char address, unsigned char *result) {

	*result = address;
	if (write(file_i2c, result, 1) != 1)
		exit(1);

	if (read(file_i2c, result, 1) != 1)
		exit(1);
}

// Folders contain files
struct Folder {
	char FullPath[256];
	struct File* FirstFile;
	struct Folder* next;
	struct Folder* prev;
};

// Struct to hold file (beat or sample)
struct File {
	char FullPath[256];
	unsigned int Index;
	struct File* next;
	struct File* prev;
};

struct File * GetFileAtIndex(unsigned int index, struct Folder *FirstFolder) {

	struct Folder *CurrFolder = FirstFolder;
	struct File *CurrFile = FirstFolder->FirstFile;

	bool FoundIt = false;

	while (!FoundIt) {
		if (CurrFile->Index == index) {
			return CurrFile;
		}

		else {
			CurrFile = CurrFile->next;
			if (CurrFile == NULL) {
				CurrFolder = CurrFolder->next;
				if (CurrFolder == NULL)
					return NULL;

				CurrFile = CurrFolder->FirstFile;
			}
		}
	}
	return NULL;
}

struct Folder * LoadFileStructure(char *BaseFolderPath,
		unsigned int *TotalNum) {

	struct Folder *prevFold = NULL;
	struct File *prevFile = NULL;

	struct dirent **dirList, **fileList;
	int n, m, o, p;

	struct Folder *FirstFolder = NULL;
	struct File *FirstFile = NULL;

	struct File* new_file;
	struct Folder* new_folder;

	char tempName[256];
	unsigned int FilesCount = 0;

	n = scandir(BaseFolderPath, &dirList, 0, alphasort);
	if (n < 0)
		exit(1);
	for (o = 0; o < n; o++) {
		if (dirList[o]->d_name[0] != '.') {

			sprintf(tempName, "%s%s", BaseFolderPath, dirList[o]->d_name);

			m = scandir(tempName, &fileList, 0, alphasort);

			FirstFile = NULL;
			prevFile = NULL;

			for (p = 0; p < m; p++) {
				if (fileList[p]->d_name[0] != '.') {

					new_file = (struct File*) malloc(sizeof(struct File));
					sprintf(new_file->FullPath, "%s/%s", tempName,
							fileList[p]->d_name);

					new_file->Index = FilesCount++;

					// set up prev connection
					new_file->prev = prevFile;

					prevFile = new_file;

					// next connection (NULL FOR NOW)
					new_file->next = NULL;

					// and prev's next connection (points to this object)
					if (new_file->prev != NULL)
						new_file->prev->next = new_file;

					if (FirstFile == NULL)
						FirstFile = new_file;

					printf("Added file %d - %s\n", new_file->Index,
							new_file->FullPath);

				}
				free(fileList[p]);
			}

			if (FirstFile != NULL) { // i.e. we found at least one file

				new_folder = (struct Folder*) malloc(sizeof(struct Folder));
				sprintf(new_folder->FullPath, "%s%s", BaseFolderPath,
						dirList[o]->d_name);

				// set up prev connection
				new_folder->prev = prevFold;

				prevFold = new_folder;

				// next connection (NULL FOR NOW)
				new_folder->next = NULL;

				// and prev's next connection (points to this object)
				if (new_folder->prev != NULL)
					new_folder->prev->next = new_folder;

				if (FirstFolder == NULL) {
					FirstFolder = new_folder;
				}

				new_folder->FirstFile = FirstFile;

				printf("Added Subfolder %s\n", new_folder->FullPath);

			}

			free(fileList);

		}

		free(dirList[o]);

	}

	free(dirList);

	*TotalNum = FilesCount;

	return FirstFolder;

}

void DumpFileStructure(struct Folder *FirstFolder) {

	struct Folder *currFold = FirstFolder;
	struct File *currFile;

	do {
		currFile = currFold->FirstFile;

		if (currFile != NULL) {

			do {
				printf("%s - %s\n", currFold->FullPath, currFile->FullPath);

			} while ((currFile = currFile->next) != NULL);
		}

	} while ((currFold = currFold->next) != NULL);

}

void *encoderThread(void *ptr) {
	char *message;
	message = (char *) ptr;
	printf("%s \n", message);
	unsigned char result;
	int prevAngle = 0x0000;
	int encoderAngle = 0x0000;
	int wrappedAngle = 0x0000;
	unsigned int totalTurns = 0x0001;

	//----- OPEN THE I2C BUS -----
	char *filename = (char*) "/dev/i2c-1";
	if ((file_i2c = open(filename, O_RDWR)) < 0) {
		//ERROR HANDLING: you can check errno to see what went wrong
		printf("Failed to open the i2c bus");
		return NULL;
	}

	int addr = 0x36;          //<<<<<The I2C address of the slave
	if (ioctl(file_i2c, I2C_SLAVE, addr) < 0) {
		printf("Failed to acquire bus access and/or talk to slave.\n");
		//ERROR HANDLING; you can check errno to see what went wrong
		return NULL;
	}

	unsigned char touchedNum = 0;
	uint32_t accumulatedPos = 0;

	unsigned char buttonState = 0;

	bool buttons[4] = {0,0,0,0}, totalbuttons[4] = {0,0,0,0};

	unsigned int butCounter = 0;

	unsigned char i = 0;

	struct Folder *FirstBeatFolder;

	unsigned int NumBeats, NumSamples;

	FirstBeatFolder = LoadFileStructure("/media/sda/beats/", &NumBeats);
	//DumpFileStructure(FirstBeatFolder);

	struct Folder *CurrentBeatFolder = FirstBeatFolder;
	struct File *CurrentBeatFile = CurrentBeatFolder->FirstFile;

	struct Folder *FirstSampleFolder;

	FirstSampleFolder = LoadFileStructure("/media/sda/samples/", &NumSamples);
	//DumpFileStructure(FirstSampleFolder);

	struct Folder *CurrentSampleFolder = FirstSampleFolder;
	struct File *CurrentSampleFile = CurrentSampleFolder->FirstFile;

	player_set_track(&deck[0].player,
			track_acquire_by_import(deck[0].importer,
					CurrentBeatFile->FullPath));
	//player_set_track(&deck[1].player, track_acquire_by_import(deck[1].importer, CurrentSampleFile->FullPath));

	srand (time(NULL));

unsigned	int delcount = 0;

	while (1) {

		delcount++;
		if (delcount > 100) {
			//printf("pos : %d %d\n", accumulatedPos, encoderAngle  );
			delcount = 0;
		}

		//ret = mcp3008read(&attr, data);

		/* TODO - get ADC stuff from input processor
		 if (data[CH2] < 1021 && data[CH2] > 5)
		 deck[1].player.faderTarget = ((double) data[CH0]) / 1024;
		 else
		 deck[1].player.faderTarget = 0.0;
		 deck[0].player.faderTarget = ((double) data[CH1]) / 1024;
		 */

		if (true) { // TODO : REPLACE THIS WITH I2C code to input processor
			touchedNum = 0;
			if (!deck[1].player.capTouch) { // Positive touching edge
				//printf("Touched from %d %f\n", accumulatedPos, deck[1].player.position);
				accumulatedPos = (uint32_t)(deck[1].player.position * 3072);
				deck[1].player.target_position = deck[1].player.position;
				deck[1].player.capTouch = 1;
				//printf("Touched to %d %f\n", accumulatedPos, deck[1].player.target_position);
			}
		} else {

			if (deck[1].player.capTouch) { // negative touching edge
				touchedNum++;
				if (touchedNum > 0) {
					deck[1].player.capTouch = 0;
					printf("untouched %d\n", touchedNum);
					touchedNum = 0;
				}
			}
		}

		if (deck[1].player.capTouch) {

			// Handle rotary sensor

			i2c_read_address(0x0e, &result);
			encoderAngle = result << 8;
			i2c_read_address(0x0f, &result);
			encoderAngle = (encoderAngle & 0x0f00) | result;

			if (encoderAngle < 1024 && prevAngle >= 3072) { // We crossed zero in the positive direction
				totalTurns++;
				wrappedAngle = prevAngle - 4096;
			} else if (encoderAngle >= 3072 && prevAngle < 1024) { // We crossed zero in the negative direction
				totalTurns--;
				wrappedAngle = prevAngle + 4096;
			} else {
				wrappedAngle = prevAngle;
			}

			if (abs(encoderAngle - wrappedAngle) > 100) {
				//printf("blip! %d %d %d\n", encoderAngle, wrappedAngle, accumulatedPos);
				prevAngle = encoderAngle;
			} else {
				prevAngle = encoderAngle;
				// Add the difference from the last angle to the position
				accumulatedPos += encoderAngle - wrappedAngle;
			}
			deck[1].player.target_position = ((double) accumulatedPos) / 3072;

			//deck[1].player.target_position = (((double)((totalTurns << 12) | encoderAngle)) / 3072);
			//printf("%d\n", accumulatedPos );

			// Handle ADCs (crossfader, volume)

		}

		/*buttons[0] = !GET_GPIO(16);
		 buttons[1] = !GET_GPIO(6);
		 buttons[2] = !GET_GPIO(5);
		 buttons[3] = !GET_GPIO(12);*/

		
		
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

		case BUTTONSTATE_NONE:
			if (buttons[0] || buttons[1] || buttons[2] || buttons[3]) {
				buttonState = BUTTONSTATE_PRESSING;

			}
			break;

		case BUTTONSTATE_PRESSING:
			for (i = 0; i < 4; i++)
				totalbuttons[i] |= buttons[i];

			if (!(buttons[0] || buttons[1] || buttons[2] || buttons[3]))
				buttonState = BUTTONSTATE_ACTING_INSTANT;

			butCounter++;
			if (butCounter > 500) {
				butCounter = 0;
				buttonState = BUTTONSTATE_ACTING_HELD;
			}

			break;

		case BUTTONSTATE_ACTING_INSTANT:
			if (totalbuttons[0] && !totalbuttons[1] && !totalbuttons[2]
					&& !totalbuttons[3]) {
				printf("Samples - Up pushed\n");
				if (CurrentSampleFile->prev != NULL) {
					CurrentSampleFile = CurrentSampleFile->prev;
					player_set_track(&deck[1].player,
							track_acquire_by_import(deck[0].importer,
									CurrentSampleFile->FullPath));
				}
			} else if (!totalbuttons[0] && totalbuttons[1] && !totalbuttons[2]
					&& !totalbuttons[3]) {
				printf("Samples - Down pushed\n");
				if (CurrentSampleFile->next != NULL) {
					CurrentSampleFile = CurrentSampleFile->next;
					player_set_track(&deck[1].player,
							track_acquire_by_import(deck[0].importer,
									CurrentSampleFile->FullPath));
				}
			} else if (totalbuttons[0] && totalbuttons[1] && !totalbuttons[2]
					&& !totalbuttons[3]) {
				printf("Samples - both buttons pushed\n");
				r = rand() % NumSamples;
				printf("Playing file %d/%d\n", r, NumSamples);
				player_set_track(&deck[1].player,
						track_acquire_by_import(deck[0].importer,
								GetFileAtIndex(r, FirstSampleFolder)->FullPath));
			}

			else if (!totalbuttons[0] && !totalbuttons[1] && totalbuttons[2]
					&& !totalbuttons[3]) {
				printf("Beats - Up pushed\n");
				if (CurrentBeatFile->prev != NULL) {
					CurrentBeatFile = CurrentBeatFile->prev;
					player_set_track(&deck[0].player,
							track_acquire_by_import(deck[0].importer,
									CurrentBeatFile->FullPath));
				}
			} else if (!totalbuttons[0] && !totalbuttons[1] && !totalbuttons[2]
					&& totalbuttons[3]) {
				printf("Beats - Down pushed\n");
				if (CurrentBeatFile->next != NULL) {
					CurrentBeatFile = CurrentBeatFile->next;
					player_set_track(&deck[0].player,
							track_acquire_by_import(deck[0].importer,
									CurrentBeatFile->FullPath));
				}
			} else if (!totalbuttons[0] && !totalbuttons[1] && totalbuttons[2]
					&& totalbuttons[3]) {
				printf("Beats - both buttons pushed\n");
				r = rand() % NumBeats;
				printf("Playing file %d/%d\n", r, NumBeats);
				player_set_track(&deck[0].player,
						track_acquire_by_import(deck[0].importer,
								GetFileAtIndex(r, FirstBeatFolder)->FullPath));
			}

			else if (totalbuttons[0] && totalbuttons[1] && totalbuttons[2]
					&& totalbuttons[3])
				printf("All buttons pushed!\n");

			else
				printf("Sod knows what you were trying to do there\n");

			buttonState = BUTTONSTATE_WAITING;

			break;

		case BUTTONSTATE_ACTING_HELD: // Act on whatever buttons are being held down when the timeout happens
			if (buttons[0] && !buttons[1] && !buttons[2] && !buttons[3]) {
				printf("Samples - Up held\n");
				if (CurrentSampleFolder->prev != NULL) {
					CurrentSampleFolder = CurrentSampleFolder->prev;
					CurrentSampleFile = CurrentSampleFolder->FirstFile;
					player_set_track(&deck[1].player,
							track_acquire_by_import(deck[0].importer,
									CurrentSampleFile->FullPath));
				}
			} else if (!buttons[0] && buttons[1] && !buttons[2]
					&& !buttons[3]) {
				printf("Samples - Down held\n");
				if (CurrentSampleFolder->next != NULL) {
					CurrentSampleFolder = CurrentSampleFolder->next;
					CurrentSampleFile = CurrentSampleFolder->FirstFile;
					player_set_track(&deck[1].player,
							track_acquire_by_import(deck[0].importer,
									CurrentSampleFile->FullPath));
				}
			} else if (buttons[0] && buttons[1] && !buttons[2] && !buttons[3])
				printf("Samples - both buttons held\n");

			else if (!buttons[0] && !buttons[1] && buttons[2] && !buttons[3]) {
				printf("Beats - Up held\n");
				if (CurrentBeatFolder->prev != NULL) {
					CurrentBeatFolder = CurrentBeatFolder->prev;
					CurrentBeatFile = CurrentBeatFolder->FirstFile;
					player_set_track(&deck[0].player,
							track_acquire_by_import(deck[0].importer,
									CurrentBeatFile->FullPath));
				}
			} else if (!buttons[0] && !buttons[1] && !buttons[2]
					&& buttons[3]) {
				printf("Beats - Down held\n");
				if (CurrentBeatFolder->next != NULL) {
					CurrentBeatFolder = CurrentBeatFolder->next;
					CurrentBeatFile = CurrentBeatFolder->FirstFile;
					player_set_track(&deck[0].player,
							track_acquire_by_import(deck[0].importer,
									CurrentBeatFile->FullPath));
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

		usleep(1500);
	}
}

int main(int argc, char *argv[]) {
	int rc = -1, n, priority;
	bool use_mlock;

	int rate;

	int alsa_buffer;

	fprintf(stderr, "%s\n\n" NOTICE "\n\n", banner);

	if (setlocale(LC_ALL, "") == NULL) {
		fprintf(stderr, "Could not honour the local encoding\n");
		return -1;
	}

	if (rig_init() == -1)
		return -1;
	rt_init(&rt);

	ndeck = 0;
	nctl = 0;
	priority = DEFAULT_PRIORITY;
	importer = DEFAULT_IMPORTER;

	speed = 1.0;
	protect = false;
	phono = false;
	use_mlock = false;

	alsa_buffer = DEFAULT_ALSA_BUFFER;
	rate = DEFAULT_RATE;

	/* Create a deck */

	alsa_init(&deck[0].device, "plughw:0,0", rate, alsa_buffer, 0);
	
	alsa_init(&deck[1].device, "plughw:0,0", rate, alsa_buffer, 1);

	deck_init(&deck[0], &rt, importer, speed, phono, protect, 0);
	deck_init(&deck[1], &rt, importer, speed, phono, protect, 1);

	deck[0].device.player2 = deck[1].device.player;

	ndeck = 2;

	alsa_clear_config_cache();

	rc = EXIT_FAILURE; /* until clean exit */

	/* Order is important: launch realtime thread first, then mlock.
	 * Don't mlock the interface, use sparingly for audio threads */

	if (rt_start(&rt, priority) == -1)
		return -1;

	if (use_mlock && mlockall(MCL_CURRENT) == -1) {
		perror("mlockall");
		goto out_rt;
	}

	/*if (interface_start(&library, geo, decor) == -1)
	 goto out_rt;*/

	player_set_track(&deck[1].player,
			track_acquire_by_import(deck[1].importer,
					"/root/beepahhfresh.raw"));

	deck[0].player.justPlay = 1;

	pthread_t thread1;
	const char *message1 = "Thread 1";
	int iret1;

	/* Create independent threads each of which will execute function */

	iret1 = pthread_create(&thread1, NULL, encoderThread, (void*) message1);
	if (iret1) {
		fprintf(stderr, "Error - pthread_create() return code: %d\n", iret1);
		exit (EXIT_FAILURE);
	}

	printf("pthread_create() for thread 1 returns: %d\n", iret1);

	if (rig_main() == -1)
		goto out_interface;

	rc = EXIT_SUCCESS;
	fprintf(stderr, "Exiting cleanly...\n");

	out_interface: out_rt: rt_stop(&rt);

	for (n = 0; n < ndeck; n++)
		deck_clear(&deck[n]);

	for (n = 0; n < nctl; n++)
		controller_clear(&ctl[n]);

	rig_clear();
	thread_global_clear();

	if (rc == EXIT_SUCCESS)
		fprintf(stderr, "Done.\n");

	return rc;
}
