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
 * MIDI Event queue implementation
 *
 * Provides the single queue instance and C API wrappers
 * for communication between RT thread and input thread.
 *
 * Uses moodycamel::ReaderWriterQueue - a battle-tested lock-free SPSC queue.
 */

#include "midi_event.h"

namespace sc {

// Single global queue instance for MIDI events
// Pre-allocate space for 64 events to avoid runtime allocation
static MidiEventQueue g_midi_event_queue(64);

} // namespace sc

// C API implementations

int midi_event_queue_push(const unsigned char* midi_bytes, int shifted) {
    sc::MidiEvent event(midi_bytes, shifted != 0);
    // try_enqueue won't allocate - returns false if queue is full
    return sc::g_midi_event_queue.try_enqueue(event) ? 1 : 0;
}

int midi_event_queue_pop(unsigned char* midi_bytes, int* shifted) {
    sc::MidiEvent event;
    if (sc::g_midi_event_queue.try_dequeue(event)) {
        midi_bytes[0] = event.bytes[0];
        midi_bytes[1] = event.bytes[1];
        midi_bytes[2] = event.bytes[2];
        *shifted = event.shifted ? 1 : 0;
        return 1;
    }
    return 0;
}
