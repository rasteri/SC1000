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
#include <sys/ioctl.h>	 //Needed for I2C port
#include <linux/i2c-dev.h> //Needed for I2C port
#include <time.h>
#include <dirent.h>
#include <stdint.h>
#include <string.h>

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
#include "dicer.h"
#include "sc_queue.h"

#define DEFAULT_IMPORTER EXECDIR "/xwax-import"

struct deck deck[2];
statequeue queues[2];
statequeue filterqueues[2];

static struct rt rt;

static const char *importer;

SC_SETTINGS scsettings;

static struct controller midiController;

void loadSettings()
{
	FILE *fp;
	char *line = NULL;
	size_t len = 0;
	ssize_t read;
	char *param;
	char *value;
	char delim[] = "=";

	// set defaults
	scsettings.buffersize = 256;
	scsettings.faderclosepoint = 2;
	scsettings.faderopenpoint = 5;
	scsettings.platterenabled = 1;
	scsettings.platterspeed = 3072;
	scsettings.samplerate = 48000;
	scsettings.updaterate = 1000;

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
				param = strtok(line, delim);
				value = strtok(NULL, delim);

				if (strcmp(param, "buffersize") == 0)
					scsettings.buffersize = atoi(value);
				else if (strcmp(param, "faderclosepoint") == 0)
					scsettings.faderclosepoint = atoi(value);
				else if (strcmp(param, "faderopenpoint") == 0)
					scsettings.faderopenpoint = atoi(value);
				else if (strcmp(param, "platterenabled") == 0)
					scsettings.platterenabled = atoi(value);
				else if (strcmp(param, "platterspeed") == 0)
					scsettings.platterspeed = atoi(value);
				else if (strcmp(param, "samplerate") == 0)
					scsettings.samplerate = atoi(value);
				else if (strcmp(param, "updaterate") == 0)
					scsettings.updaterate = atoi(value);
			}
		}
	}

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

int main(int argc, char *argv[])
{

	int rc = -1, priority;
	bool use_mlock;

	int alsa_buffer;
	int rate;

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

	alsa_buffer = 2;
	rate = 48000;

	// Tell deck0 to just play without considering inputs

	deck[0].player.justPlay = 1;

	deck[0].player.scqueue = &queues[0];
	deck[1].player.scqueue = &queues[1];

	deck[0].player.filterqueue = &filterqueues[0];
	deck[1].player.filterqueue = &filterqueues[1];

	fifoInit(&queues[0], 9);
	fifoInit(&queues[1], 9);
	fifoInit(&filterqueues[0], 1024);
	fifoInit(&filterqueues[1], 1024);

	alsa_init(&deck[0].device, "hw:0,0", rate, scsettings.buffersize, 0);
	alsa_init(&deck[1].device, "hw:0,0", rate, scsettings.buffersize, 1);

	deck_init(&deck[0], &rt, importer, 1.0, false, false, 0);
	deck_init(&deck[1], &rt, importer, 1.0, false, false, 1);


	// point deck1's output at deck0, it will be summed in

	deck[0].device.player2 = deck[1].device.player;



	// Stop deck1 from looping
	deck[0].player.looping = 1;
	deck[1].player.looping = 0;

	alsa_clear_config_cache();

	rc = EXIT_FAILURE; /* until clean exit */

	// Start input processing thread

	SC_Input_Start();

	if (dicer_init(&midiController, &rt, "hw:1,0,0") != -1){
		controller_add_deck(&midiController, &deck[1]);
	}

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
