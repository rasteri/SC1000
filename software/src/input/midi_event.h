/*
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


/*
 * MIDI Event structure for inter-thread communication
 */

#pragma once

#include "../util/spsc_queue.h"

// API for the realtime thread to push events
// Returns 1 on success, 0 if queue full
int midi_event_queue_push(const unsigned char* midi_bytes, int shifted);

// API for the input thread to pop events
// Returns 1 if event was available, 0 if queue empty
int midi_event_queue_pop(unsigned char* midi_bytes, int* shifted);

namespace sc {

struct MidiEvent {
    unsigned char bytes[3];
    bool shifted;  // Shift state at time of event

    MidiEvent() : bytes{0, 0, 0}, shifted(false) {}

    MidiEvent(const unsigned char* buf, bool shift_state)
        : bytes{buf[0], buf[1], buf[2]}, shifted(shift_state) {}
};

// Queue size: 64 events should be more than enough
// (at 1000Hz polling, this is 64ms of buffer)
// Using moodycamel::ReaderWriterQueue under the hood
using MidiEventQueue = moodycamel::ReaderWriterQueue<MidiEvent>;

} // namespace sc
