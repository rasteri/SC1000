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

static struct controller midiController;

struct mapping *maps = NULL;

void loadSettings()
{
	FILE *fp;
	char *line = NULL;
	size_t len = 0;
	ssize_t read;
	char *param, *actions;
	char *value;
	unsigned char channel, notenum, action=69, deckno, parameter, controlType, pin, pullup;
	char *edge;
	char delim[] = "=";
	char delimc[] = ",";
	unsigned char midicommand[3];
	char *linetok, *valuetok;
	bool midiRemapped = 0;
	// set defaults
	scsettings.buffersize = 256;
	scsettings.faderclosepoint = 2;
	scsettings.faderopenpoint = 5;
	scsettings.platterenabled = 1;
	scsettings.platterspeed = 2275;
	scsettings.samplerate = 48000;
	scsettings.updaterate = 2000;
	scsettings.debouncetime = 5;
	scsettings.holdtime = 100;
	scsettings.slippiness = 300;
	scsettings.brakespeed = 3000;
	scsettings.pitchrange = 50;
	scsettings.mididelay = 5; 	

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
				else if (strstr(param, "midii") != NULL)
				{
					midiRemapped = 1;
					controlType = atoi(strtok_r(value, delimc, &valuetok));
					channel = atoi(strtok_r(NULL, delimc, &valuetok));
					notenum = atoi(strtok_r(NULL, delimc, &valuetok));
					actions = strtok_r(NULL, delimc, &valuetok);
					parameter = 0;
					
					// Extract deck no from action (CHx)
					if (actions[2] == '0') deckno = 0;
					if (actions[2] == '1') deckno = 1;
					
					// figure out which action it is
					if (strstr(actions+4, "CUE") != NULL) action = ACTION_CUE;
					else if (strstr(actions+4, "SHIFTON") != NULL) action = ACTION_SHIFTON;
					else if (strstr(actions+4, "SHIFTOFF") != NULL) action = ACTION_SHIFTOFF;
					else if (strstr(actions+4, "STARTSTOP") != NULL) action = ACTION_STARTSTOP;
					else if (strstr(actions+4, "PITCH") != NULL) action = ACTION_PITCH;
					else if (strstr(actions+4, "NOTE") != NULL) {
						action = ACTION_NOTE;
						parameter = atoi(actions+8);
						printf("MIDINOTE --%s-- %u\n", actions+8, parameter);
					}
					
					// Build MIDI command
					midicommand[0] = (controlType << 4) | channel;
					midicommand[1] = notenum;
					midicommand[2] = 0;
					
					add_MIDI_mapping(
						&maps, 
						midicommand,
						deckno,
						action, 
						parameter
					);
					
				}
				else if (strstr(param, "io") != NULL){
					pin = atoi(strtok_r(value, delimc, &valuetok));
					pullup = atoi(strtok_r(NULL, delimc, &valuetok));
					edge = atoi(strtok_r(NULL, delimc, &valuetok));
					actions = strtok_r(NULL, delimc, &valuetok);
					parameter = 0;

					// Extract deck no from action (CHx)
					if (actions[2] == '0') deckno = 0;
					if (actions[2] == '1') deckno = 1;

					// figure out which action it is
					if (strstr(actions+4, "CUE") != NULL) action = ACTION_CUE;
					else if (strstr(actions+4, "SHIFTON") != NULL) action = ACTION_SHIFTON;
					else if (strstr(actions+4, "SHIFTOFF") != NULL) action = ACTION_SHIFTOFF;
					else if (strstr(actions+4, "STARTSTOP") != NULL) action = ACTION_STARTSTOP;
					else if (strstr(actions+4, "GND") != NULL) action = ACTION_GND;
					else if (strstr(actions+4, "NOTE") != NULL) {
						action = ACTION_NOTE;
						parameter = atoi(actions+9);
					}

					add_IO_mapping(
						&maps, 
						pin,
						pullup,
						edge,
						deckno,
						action, 
						parameter
					);
				}
				else if (strcmp(param, "mididelay") == 0) // Literally just a sleep to allow USB devices longer to initialize
					scsettings.mididelay = atoi(value);
				else {
					printf("Unrecognised configuration line - Param : %s , value : %s\n", param, value);
				}
			}
		}
	}
	
	// If we got no MIDI remaps, set up a default map
	if (!midiRemapped){
		
		// Set up per-deck cue/startstop/pitchbend mappings
		for (deckno = 0; deckno < 2; deckno++){
			// Notes on channels 0 and 1 are cue points
			for (notenum = 0; notenum < 128; notenum++){
				midicommand[0] = 0x90 + deckno;
				midicommand[1] = notenum;
				add_MIDI_mapping(&maps, midicommand, deckno, ACTION_CUE, 0);
			}
			
			// Notes on channels 2 and 3 are C1-style notes
			for (notenum = 0; notenum < 128; notenum++){
				midicommand[0] = 0x92 + deckno;
				midicommand[1] = notenum;
				add_MIDI_mapping(&maps, midicommand, deckno, ACTION_NOTE, notenum);
			}
			
			// Pitch bend on channels 0 and 1 is, well, pitchbend
			midicommand[0] = 0xE0 + deckno; midicommand[1] = 0; midicommand[2] = 0;
			add_MIDI_mapping(&maps, midicommand, deckno, ACTION_PITCH, 0);
			
			// Notes 0-1 of channel 4 are startstop
			midicommand[0] = 0x94; midicommand[1] = deckno;
			add_MIDI_mapping(&maps, midicommand, deckno, ACTION_STARTSTOP, 0);
		}
		
		// note 7F of channel 4 is shift
		midicommand[0] = 0x94; midicommand[1] = 0x7F;
		add_MIDI_mapping(&maps, midicommand, deckno, ACTION_SHIFTON, 0);
		midicommand[0] = 0x84; midicommand[1] = 0x7F;
		add_MIDI_mapping(&maps, midicommand, deckno, ACTION_SHIFTOFF, 0);
		
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

void sig_handler(int signo)
{
  if (signo == SIGINT){
    printf("received SIGINT\n");
exit(0);
}
}

int main(int argc, char *argv[])
{

	int rc = -1, priority;
	bool use_mlock;

	int alsa_buffer;
	int rate;

	if (signal(SIGINT, sig_handler) == SIG_ERR){
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

	/*int num;
	char midinames[32][32];

	int devs = device_list(midinames);
	for (int cunt = 0; cunt < devs; cunt++)
		printf("%d - %s\n", cunt, midinames[cunt]);*/


	loadSettings();

	// Create two decks, both pointed at the same audio device

	alsa_buffer = 2;
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
