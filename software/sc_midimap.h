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

#ifndef SC_MIDIMAP_H
#define SC_MIDIMAP_H

#define CONTROL_NOTE 1
#define CONTROL_CC 2

#define ACTION_CUE 0
#define ACTION_SHIFTON 1
#define ACTION_SHIFTOFF 2
#define ACTION_STARTSTOP 3
#define ACTION_PITCH 4
#define ACTION_NOTE 5

// Defines a mapping between a MIDI event and an action
struct mapping {
	// MIDI event
	unsigned char MidiBytes[3];

	// Action
	unsigned char DeckNo; // Which deck to apply this action to
	unsigned char Action; // The action to take - cue, shift etc
	unsigned char Param; // for example the output note
	
	struct mapping *next;
};

void add_mapping(struct mapping **maps, unsigned char buf[3], unsigned char DeckNo, unsigned char Action, unsigned char Param);

#endif
