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

#ifndef XWAX_H
#define XWAX_H

#include "deck.h"

extern size_t ndeck;
extern struct deck deck[];


typedef struct SC_SETTINGS {

    // output buffer size, probably 256
    int buffersize;

    // sample rate, probably 48000
    int samplerate;
	
	// fader options
	char singleVCA;
	char doublecut;
	char hamster;

    // fader thresholds for hysteresis
    int faderopenpoint; // value required to open the fader (when fader is closed)
    int faderclosepoint; // value required to close the fader (when fader is open)



    // delay between iterations of the input loop
    int updaterate;

    // 1 when enabled, 0 when not
    int platterenabled;

    // specifies the ratio of platter movement to sample movement
    // 4096 = 1 second for every platter rotation
    // Default 3072 = 1.33 seconds for every platter rotation
    int platterspeed;


    // How long to debounce external GPIO switches
    int debouncetime;
	
	// How long a button press counts as a hold
    int holdtime;
	
	// Virtual slipmat slippiness - how quickly the sample returns to normal speed after you let go of the jog wheel
	// Higher values are slippier
    int slippiness;

	// How long the the platter takes to stop after you hit a stop button, higher values are longer
	int brakespeed;
	
	// Pitch range of MIDI commands
	int pitchrange;
	
	// How many seconds to wait before initializing MIDI
	int mididelay;


} SC_SETTINGS;

extern SC_SETTINGS scsettings;
#endif
