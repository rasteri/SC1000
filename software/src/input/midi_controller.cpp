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
 * Generic MIDI Controller handler
 *
 * Originally based on Novation Dicer support from xwax.
 * Now handles any MIDI input device, parsing raw MIDI bytes
 * and pushing events to the lock-free queue for processing.
 */

#include <cstdio>
#include <cstring>

#include "../util/debug.h"
#include "../util/log.h"
#include "../player/deck.h"
#include "../core/global.h"
#include "../core/sc1000.h"
#include "../thread/realtime.h"

#include "../platform/midi.h"
#include "midi_event.h"
#include "midi_controller.h"

MidiController::MidiController() = default;

MidiController::~MidiController()
{
    if (initialized_) {
        clear();
    }
}

int MidiController::init(struct Rt* rt, const char* hw)
{
    LOG_INFO("MIDI controller init %p from %s", static_cast<void*>(this), hw);

    rt_ = rt;  // Store for access to engine->input_state

    strncpy(port_name_, hw, sizeof(port_name_) - 1);
    port_name_[sizeof(port_name_) - 1] = '\0';

    if (midi_open(&midi_, hw) == -1) {
        return -1;
    }

    ofill_ = 0;

    for (int i = 0; i < NUMDECKS; i++) {
        deck_[i] = nullptr;
    }

    if (rt->add_controller(this) == -1) {
        midi_close(&midi_);
        return -1;
    }

    parsing_ = false;
    parsed_bytes_ = 0;
    initialized_ = true;

    return 0;
}

int MidiController::add_deck(struct Deck* k)
{
    debug("%p add deck %p", this, k);

    for (int i = 0; i < NUMDECKS; i++) {
        if (deck_[i] == nullptr) {
            deck_[i] = k;
            break;
        }
    }

    return 0;
}

void MidiController::process_midi_message()
{
    unsigned char status = midi_buffer_[0] & 0xF0;
    unsigned char channel = midi_buffer_[0] & 0x0F;
    const char* type = "???";
    if (status == 0x90) type = "NoteOn";
    else if (status == 0x80) type = "NoteOff";
    else if (status == 0xB0) type = "CC";
    else if (status == 0xE0) type = "PitchBend";

    LOG_INFO("MIDI: %s ch=%d data=[%d, %d]", type, channel, midi_buffer_[1], midi_buffer_[2]);

    // Push MIDI event to lock-free queue for processing by input thread
    // Capture current shifted state from engine's input state
    bool shifted = rt_ && rt_->engine ? rt_->engine->input_state.is_shifted() : false;
    if (!midi_event_queue_push(midi_buffer_, shifted)) {
        LOG_WARN("MIDI event queue full, dropping event");
    }
}

ssize_t MidiController::pollfds(struct pollfd* pe, size_t z)
{
    return midi_pollfds(&midi_, pe, z);
}

int MidiController::realtime()
{
    for (;;) {
        unsigned char buf;
        unsigned char command;
        ssize_t z;

        z = midi_read(&midi_, &buf, 1);
        if (z == -1) {
            return -1;
        }
        if (z == 0) {
            return 0;
        }

        // Bit 7 set = status byte
        if (buf & 0x80) {
            command = buf & 0xF0;
            // Note Off, Note On, Control Change, Pitch Bend
            if (command == 0x80 || command == 0x90 || command == 0xB0 || command == 0xE0) {
                parsing_ = true;
                parsed_bytes_ = 0;  // Reset counter for new message
                midi_buffer_[0] = buf;
            } else {
                parsing_ = false;
            }
        }
        // Bit 7 unset = data byte
        else {
            if (parsing_) {
                midi_buffer_[++parsed_bytes_] = buf;

                // Complete message (status + 2 data bytes)
                if (parsed_bytes_ == 2) {
                    parsing_ = false;
                    parsed_bytes_ = 0;
                    process_midi_message();
                }
            }
        }

        debug("got event");
    }

    return 0;
}

void MidiController::clear()
{
    debug("%p", this);

    if (initialized_) {
        midi_close(&midi_);
        initialized_ = false;
    }
}

std::unique_ptr<MidiController> create_midi_controller(struct Rt* rt, const char* hw)
{
    auto controller = std::make_unique<MidiController>();
    if (controller->init(rt, hw) == -1) {
        return nullptr;
    }
    return controller;
}
