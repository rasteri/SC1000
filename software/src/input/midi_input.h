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

// MIDI Input Layer
// Generic MIDI device enumeration and event processing
// This layer is not SC1000-specific and could be reused

#pragma once

#include "midi_controller.h"
#include "midi_command.h"
#include <vector>
#include <memory>

struct Sc1000;
struct ScSettings;

namespace sc {
namespace input {

// Runtime state for MIDI input processing
struct MidiContext {
    // MIDI controllers (owned via unique_ptr)
    std::vector<std::unique_ptr<MidiController>> controllers;

    // Device enumeration state
    char device_names[64][64] = {};
    int device_count = 0;
    int old_device_count = 0;
};

// Initialize MIDI context
void init_midi(MidiContext* ctx);

// Poll for new MIDI devices (call periodically, e.g., once per second)
// Adds newly detected devices to the controller list
void poll_midi_devices(MidiContext* ctx, Sc1000* engine);

// Process MIDI events from the lock-free queue
// Dispatches events to the appropriate action handlers
void process_midi_events(Sc1000* engine);

} // namespace input
} // namespace sc
