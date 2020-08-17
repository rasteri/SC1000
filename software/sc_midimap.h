/*
 * Copyright (C) 2018 Andrew Tait <rasteri@gmail.com>
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

#include <stdbool.h>

#ifndef SC_MIDIMAP_H
#define SC_MIDIMAP_H

#define CONTROL_NOTE 1
#define CONTROL_CC 2

#define ACTION_CUE 0
#define ACTION_SHIFTON 1
#define ACTION_SHIFTOFF 2
#define ACTION_STARTSTOP 3
#define ACTION_START 4
#define ACTION_STOP 5
#define ACTION_PITCH 6
#define ACTION_NOTE 7
#define ACTION_GND 8
#define ACTION_VOL 9
#define ACTION_NEXTFILE 10
#define ACTION_PREVFILE 11
#define ACTION_RANDOMFILE 12
#define ACTION_NEXTFOLDER 13
#define ACTION_PREVFOLDER 14
#define ACTION_RECORD 15
#define ACTION_VOLUP 16
#define ACTION_VOLDOWN 17
#define ACTION_JOGPIT 18
#define ACTION_DELETECUE 19
#define ACTION_SC500 20
#define ACTION_VOLUHOLD 21
#define ACTION_VOLDHOLD 22
#define ACTION_JOGPSTOP 23

#define MAP_MIDI 0
#define MAP_IO 1

// Defines a mapping between a MIDI event and an action
struct mapping {

	// Event type (MIDI or IO)
	unsigned char Type;

	// MIDI event info
	unsigned char MidiBytes[3];

	// IO event info
	unsigned char Pin; // IO Pin Number
	bool Pullup; // Whether or not to pull the pin up
	char Edge; // Edge (1 for unpressed-to-pressed)

	// GPIO event info
	unsigned char port; // GPIO port number

	// Action
	unsigned char DeckNo; // Which deck to apply this action to
	unsigned char Action; // The action to take - cue, shift etc
	unsigned char Param; // for example the output note
	
	int debounce;

	struct mapping *next;
};
void add_mapping(struct mapping **maps, unsigned char Type, unsigned char deckno, unsigned char buf[3], unsigned char port, unsigned char Pin, bool Pullup, bool Edge, unsigned char action, unsigned char parameter);
struct mapping *find_MIDI_mapping(struct mapping *maps, unsigned char buf[3]);
struct mapping *find_IO_mapping(struct mapping *maps, unsigned char port, unsigned char pin, char edge);
void IOevent(struct mapping *map, unsigned char MidiBuffer[3]);
#endif
