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
#include <unistd.h>                     //Needed for I2C port
#include <fcntl.h>                      //Needed for I2C port
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
#include "sc_input.h"

#define DEFAULT_OSS_BUFFERS 8
#define DEFAULT_OSS_FRAGMENT 7

#define DEFAULT_ALSA_BUFFER 5 /* milliseconds, change this to reduce latency*/

#define DEFAULT_RATE 48000
#define DEFAULT_PRIORITY 0 

#define DEFAULT_IMPORTER EXECDIR "/xwax-import"
#define DEFAULT_SCANNER EXECDIR "/xwax-scan"
#define DEFAULT_TIMECODE "serato_2a"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*x))

size_t ndeck;
struct deck deck[3];

static size_t nctl;
static struct controller ctl[2];

static struct rt rt;

static double speed;
static bool protect, phono;
static const char *importer;

void i2c_read_address(int file_i2c, unsigned char address, unsigned char *result) {

	*result = address;
	if (write(file_i2c, result, 1) != 1)
		exit(1);

	if (read(file_i2c, result, 1) != 1)
		exit(1);
}


int main(int argc, char *argv[]) {
	int rc = -1, n, priority;
	bool use_mlock;

	int rate;

	int alsa_buffer;

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

	
	// Create two decks, both pointed at the same audio device

	alsa_init(&deck[0].device, "plughw:0,0", rate, alsa_buffer, 0);
	alsa_init(&deck[1].device, "plughw:0,0", rate, alsa_buffer, 1);

	deck_init(&deck[0], &rt, importer, speed, phono, protect, 0);
	deck_init(&deck[1], &rt, importer, speed, phono, protect, 1);
	
	
	// point deck1's output at deck0

	deck[0].device.player2 = deck[1].device.player;
	
	
	// Tell deck0 to just play without considering inputs
	
	deck[0].player.justPlay = 1;

	alsa_clear_config_cache();

	rc = EXIT_FAILURE; /* until clean exit */
	
	
	// Start input processing thread
	
	SC_Input_Start();


	// Start realtime stuff 
	
	if (rt_start(&rt, priority) == -1)
		return -1;

	if (use_mlock && mlockall(MCL_CURRENT) == -1) {
		perror("mlockall");
		goto out_rt;
	}


	if (rig_main() == -1)
		goto out_interface;

	rc = EXIT_SUCCESS;
	fprintf(stderr, "Exiting cleanly...\n");

	out_interface: out_rt: rt_stop(&rt);

	for (n = 0; n < 2; n++)
		deck_clear(&deck[n]);

	for (n = 0; n < 2; n++)
		controller_clear(&ctl[n]);

	rig_clear();
	thread_global_clear();

	if (rc == EXIT_SUCCESS)
		fprintf(stderr, "Done.\n");

	return rc;
}
