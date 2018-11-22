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

#include <unistd.h>                     //Needed for I2C port
#include <fcntl.h>                      //Needed for I2C port
#include <sys/ioctl.h>                  //Needed for I2C port
#include <linux/i2c-dev.h>              //Needed for I2C port
#include <time.h>
#include <dirent.h>
#include <stdint.h>

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

#define DEFAULT_IMPORTER EXECDIR "/xwax-import"


struct deck deck[2];

static struct rt rt;

static const char *importer;

int main(int argc, char *argv[]) {
	
	int rc = -1, priority;
	bool use_mlock;
	
	int alsa_buffer;
	int rate;

	if (setlocale(LC_ALL, "") == NULL) {
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


	// Create two decks, both pointed at the same audio device
	
	alsa_buffer = 1;
	rate = 48000;

	alsa_init(&deck[0].device, "plughw:0,0", rate, alsa_buffer, 0);
	alsa_init(&deck[1].device, "plughw:0,0", rate, alsa_buffer, 1);

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

	if (use_mlock && mlockall(MCL_CURRENT) == -1) {
		perror("mlockall");
		goto out_rt;
	}

	
	// Main loop
	
	if (rig_main() == -1)
		goto out_interface;
	
	
	// Exit

	rc = EXIT_SUCCESS;
	fprintf(stderr, "Exiting cleanly...\n");

	out_interface: out_rt: rt_stop(&rt);

	deck_clear(&deck[0]);
	deck_clear(&deck[1]);

	rig_clear();
	thread_global_clear();

	if (rc == EXIT_SUCCESS)
		fprintf(stderr, "Done.\n");

	return rc;
}
