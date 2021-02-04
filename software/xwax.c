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

#include <unistd.h>		   //Needed for I2C port
#include <fcntl.h>		   //Needed for I2C port
#include <sys/ioctl.h>	   //Needed for I2C port
#include <linux/i2c-dev.h> //Needed for I2C port
#include <time.h>
#include <dirent.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>

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

#define DEFAULT_IMPORTER EXECDIR "/xwax-import"

struct deck deck[2];

struct rt rt;

static const char *importer;

SC_SETTINGS scsettings;

struct mapping *maps = NULL;

unsigned int countChars(char *string, char c)
{
	unsigned int count = 0;
	
	//printf("Checking for commas in %s\n", string);

	do
	{
		if ((*string) == c)
		{
			count++;
		}
	} while ((*(string++)));
	return count;
}

void loadSettings()
{
	FILE *fp;
	char *line = NULL;
	size_t len = 0;
	ssize_t read;
	char *param, *actions;
	char *value;
	unsigned char channel = 0, notenum = 0, controlType = 0, pin = 0, pullup = 0, port = 0;
	char edge;
	char delim[] = "=";
	char delimc[] = ",";
	unsigned char midicommand[3];
	char *linetok, *valuetok;
	// set defaults
	scsettings.buffersize = 256;
	scsettings.faderclosepoint = 2;
	scsettings.faderopenpoint = 10;
	scsettings.platterenabled = 1;
	scsettings.platterspeed = 2275;
	scsettings.samplerate = 48000;
	scsettings.updaterate = 2000;
	scsettings.debouncetime = 5;
	scsettings.holdtime = 100;
	scsettings.slippiness = 200;
	scsettings.brakespeed = 3000;
	scsettings.pitchrange = 50;
	scsettings.mididelay = 5;
	scsettings.volAmount = 0.03;
	scsettings.volAmountHeld = 0.001;
	scsettings.initialVolume = 0.125;
	scsettings.midiRemapped = 0;
	scsettings.ioRemapped = 0;
	scsettings.jogReverse = 0;

	// later we'll check for sc500 pin and use it to set following settings
	scsettings.disablevolumeadc = 0;
	scsettings.disablepicbuttons = 0;

	// Load any settings from config file
	fp = fopen("/media/sda/scsettings.txt", "r");
	if (fp == NULL)
	{
		// couldn't open settings
	}
	else
	{
		while ((read = getline(&line, &len, fp)) != -1)
		{
			if (strlen(line) < 2 || line[0] == '#')
			{ // Comment or blank line
			}
			else
			{
				param = strtok_r(line, delim, &linetok);
				value = strtok_r(NULL, delim, &linetok);

				if (strcmp(param, "buffersize") == 0)
					scsettings.buffersize = atoi(value);
				else if (strcmp(param, "faderclosepoint") == 0)
					scsettings.faderclosepoint = atoi(value);
				else if (strcmp(param, "faderopenpoint") == 0)
					scsettings.faderopenpoint = atoi(value);
				else if (strcmp(param, "platterenabled") == 0)
					scsettings.platterenabled = atoi(value);
				else if (strcmp(param, "disablevolumeadc") == 0)
					scsettings.disablevolumeadc = atoi(value);
				else if (strcmp(param, "platterspeed") == 0)
					scsettings.platterspeed = atoi(value);
				else if (strcmp(param, "samplerate") == 0)
					scsettings.samplerate = atoi(value);
				else if (strcmp(param, "updaterate") == 0)
					scsettings.updaterate = atoi(value);
				else if (strcmp(param, "debouncetime") == 0)
					scsettings.debouncetime = atoi(value);
				else if (strcmp(param, "holdtime") == 0)
					scsettings.holdtime = atoi(value);
				else if (strcmp(param, "slippiness") == 0)
					scsettings.slippiness = atoi(value);
				else if (strcmp(param, "brakespeed") == 0)
					scsettings.brakespeed = atoi(value);
				else if (strcmp(param, "pitchrange") == 0)
					scsettings.pitchrange = atoi(value);
				else if (strcmp(param, "jogreverse") == 0)
					scsettings.jogReverse = atoi(value);
				else if (strstr(param, "midii") != NULL)
				{
					scsettings.midiRemapped = 1;
					controlType = atoi(strtok_r(value, delimc, &valuetok));
					channel = atoi(strtok_r(NULL, delimc, &valuetok));
					notenum = atoi(strtok_r(NULL, delimc, &valuetok));
					edge = atoi(strtok_r(NULL, delimc, &valuetok));
					actions = strtok_r(NULL, delimc, &valuetok);

					// Build MIDI command
					midicommand[0] = (controlType << 4) | channel;
					midicommand[1] = notenum;
					midicommand[2] = 0;
					add_config_mapping(
						&maps,
						MAP_MIDI,
						midicommand,
						0,
						0,
						0,
						edge,
						actions);
				}
				else if (strstr(param, "io") != NULL)
				{
					scsettings.ioRemapped = 1;
					unsigned int commaCount = countChars(value, ',');
					//printf("Found io %s - comacount %d\n", value, commaCount);
					port = 0;
					if (commaCount == 4){
						port = atoi(strtok_r(value, delimc, &valuetok));
						pin = atoi(strtok_r(NULL, delimc, &valuetok));
					}
					else {0
						pin = atoi(strtok_r(value, delimc, &valuetok));
					}
					pullup = atoi(strtok_r(NULL, delimc, &valuetok));
					edge = atoi(strtok_r(NULL, delimc, &valuetok));
					actions = strtok_r(NULL, delimc, &valuetok);
					add_config_mapping(
						&maps,
						MAP_IO,
						NULL,
						port,
						pin,
						pullup,
						edge,
						actions);
				}
				else if (strcmp(param, "mididelay") == 0) // Literally just a sleep to allow USB devices longer to initialize
					scsettings.mididelay = atoi(value);
				else
				{
					printf("Unrecognised configuration line - Param : %s , value : %s\n", param, value);
				}
			}
		}
	}0

	

	printf("bs %d, fcp %d, fop %d, pe %d, ps %d, sr %d, ur %d\n",
		   scsettings.buffersize,
		   scsettings.faderclosepoint,
		   scsettings.faderopenpoint,
		   scsettings.platterenabled,
		   scsettings.platterspeed,
		   scsettings.samplerate,
		   scsettings.updaterate);

	if (fp)
		fclose(fp);
	if (line)
		free(line);
}

void sig_handler(int signo)
{
	if (signo == SIGINT)
	{
		printf("received SIGINT\n");
		exit(0);
	}
}

int main(int argc, char *argv[])
{

	int rc = -1, priority;
	bool use_mlock;

	int rate;

	if (signal(SIGINT, sig_handler) == SIG_ERR)
	{
		printf("\ncan't catch SIGINT\n");
		exit(1);
	}

	if (setlocale(LC_ALL, "") == NULL)
	{
		fprintf(stderr, "Could not honour the local encoding\n");
		return -1;
	}
	if (thread_global_init() == -1)
		return -1;
	if (rig_init() == -1)
		return -1;
	rt_init(&rt);

	importer = DEFAULT_IMPORTER;
	use_mlock = false;

	loadSettings();

	// Create two decks, both pointed at the same audio device

	rate = 48000;

	alsa_init(&deck[0].device, "hw:0,0", rate, scsettings.buffersize, 0);
	alsa_init(&deck[1].device, "hw:0,0", rate, scsettings.buffersize, 1);

	deck_init(&deck[0], &rt, importer, 1.0, false, false, 0);
	deck_init(&deck[1], &rt, importer, 1.0, false, false, 1);

	// point deck1's output at deck0, it will be summed in

	deck[0].device.player2 = deck[1].device.player;

	// Tell deck0 to just play without considering inputs

	deck[0].player.justPlay = 1;

	alsa_clear_config_cache();

	rc = EXIT_FAILURE; /* until clean exit */

	// Check for samples folder
	if (access("/media/sda/samples", F_OK) == -1)
	{
		// Not there, so presumably the boot script didn't manage to mount the drive
		// Maybe it hasn't initialized yet, or at least wasn't at boot time
		// We have to do it ourselves

		// Timeout after 12 sec, in which case emergency samples will be loaded
		for (int uscnt = 0; uscnt < 12; uscnt++)
		{
			printf("Waiting for USB stick...\n");
			// Wait for /dev/sda1 to show up and then mount it
			if (access("/dev/sda1", F_OK) != -1)
			{
				printf("Found USB stick, mounting!\n");
				system("/bin/mount /dev/sda1 /media/sda");
				break;
			}
			else
			{
				// If not here yet, wait a second then check again
				sleep(1);
			}
		}
	}

	deck_load_folder(&deck[0], "/media/sda/beats/");
	deck_load_folder(&deck[1], "/media/sda/samples/");
	if (!deck[1].filesPresent)
	{
		// Load the default sentence if no sample files found on usb stick
		player_set_track(&deck[1].player, track_acquire_by_import(deck[1].importer, "/var/scratchsentence.mp3"));
		cues_load_from_file(&deck[1].cues, deck[1].player.track->path);
		// Set the time back a bit so the sample doesn't start too soon
		deck[1].player.target_position = -4.0;
		deck[1].player.position = -4.0;
	}

	// Start input processing thread

	SC_Input_Start();

	// Start realtime stuff

	priority = 0;

	if (rt_start(&rt, priority) == -1)
		return -1;

	if (use_mlock && mlockall(MCL_CURRENT) == -1)
	{
		perror("mlockall");
		goto out_rt;
	}

	// Main loop

	if (rig_main() == -1)
		goto out_interface;

	// Exit

	rc = EXIT_SUCCESS;
	fprintf(stderr, "Exiting cleanly...\n");

out_interface:
out_rt:
	rt_stop(&rt);

	deck_clear(&deck[0]);
	deck_clear(&deck[1]);

	rig_clear();
	thread_global_clear();

	if (rc == EXIT_SUCCESS)
		fprintf(stderr, "Done.\n");

	return rc;
}
