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

#define MAP_MIDI 0
#define MAP_IO 1

// Defines a mapping between a MIDI event and an action
struct mapping {

	// Event type (MIDI or IO)
	unsigned char Type;

	// MIDI event info for matching
	unsigned char MidiBytes[3];

	// IO event info
	unsigned char Pin; // IO Pin Number
	bool Edge; // Edge (1 for unpressed-to-pressed)

	// Action
	unsigned char DeckNo; // Which deck to apply this action to
	unsigned char Action; // The action to take - cue, shift etc
	unsigned char Param; // for example the output note
	
	struct mapping *next;
};

void add_MIDI_mapping(struct mapping **maps, unsigned char buf[3], unsigned char DeckNo, unsigned char Action, unsigned char Param);
void add_IO_mapping(struct mapping **maps, unsigned char Pin, bool Edge, unsigned char DeckNo, unsigned char Action, unsigned char Param);
struct mapping *find_MIDI_mapping(struct mapping *maps, unsigned char buf[3]);
struct mapping *find_IO_mapping(struct mapping *maps, unsigned char pin, bool edge);
#endif
