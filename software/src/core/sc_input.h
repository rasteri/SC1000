/*
 * Copyright (C) 2019 Andrew Tait <rasteri@gmail.com>
 * Copyright (C) 2024-2026 Niklas Klügel <lodsb@lodsb.org>
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


#pragma once

#include <array>

#define CONTROL_NOTE 1
#define CONTROL_CC 2


// Defines a Mapping between a MIDI event and an action

enum IOType
{
   MIDI,
   IO
};

enum EventType
{
   BUTTON_RELEASED = 0,
   BUTTON_PRESSED = 1,
   BUTTON_HOLDING = 2,
   BUTTON_PRESSED_SHIFTED = 3,
   BUTTON_HOLDING_SHIFTED = 4,
   BUTTON_RELEASED_SHIFTED = 5,
};

enum MIDIStatusType
{
   MIDI_NOTE_OFF = 8,
   MIDI_NOTE_ON = 9,
   MIDI_CC = 11,
   MIDI_PB = 14,
};

enum ActionType
{
   CUE,
   SHIFTON,
   SHIFTOFF,
   STARTSTOP,
   START,
   STOP,
   PITCH,
   NOTE,
   GND,
   VOLUME,
   NEXTFILE,
   PREVFILE,
   RANDOMFILE,
   NEXTFOLDER,
   PREVFOLDER,
   RECORD,
   LOOPERASE,    // Long-hold to erase loop recording
   LOOPRECALL,   // Recall last loop recording
   VOLUP,
   VOLDOWN,
   JOGPIT,
   DELETECUE,
   SC500,
   VOLUHOLD,
   VOLDHOLD,
   JOGPSTOP,
   JOGREVERSE,
   BEND,
   NOTHING,
};

struct Mapping {

   // Event type (MIDI or IO)
   IOType type = IOType::MIDI;

   // IO event info
   unsigned char pin = 0;            // IO Pin Number
   bool pullup = false;              // Whether or not to pull the pin up
   EventType edge_type = EventType::BUTTON_PRESSED;  // Edge (1 for unpressed-to-pressed)

   // GPIO event info
   unsigned char gpio_port = 0;      // GPIO port number

   // MIDI event info
   std::array<unsigned char, 3> midi_command_bytes = {0, 0, 0};

   // Action
   unsigned char   deck_no = 0;      // Which deck to apply this action to
   ActionType action_type = ActionType::NOTHING;  // The action to take - cue, shift etc
   unsigned char   parameter = 0;    // for example the output note

   // Runtime state (debounce, shifted_at_press) is stored separately in ButtonState
   // See sc::control::ButtonState in control/mapping_registry.h
};

#ifdef __cplusplus
extern "C" {
#endif

void start_sc_input_thread();
void stop_sc_input_thread();

#ifdef __cplusplus
}
#endif

// Action dispatch and Mapping lookup are now in control/actions.h