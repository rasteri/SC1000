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

#pragma once

#include <memory>
#include "controller.h"
#include "../platform/midi.h"

#define NUMDECKS 2

struct Rt;

/*
 * MidiController - handles MIDI input devices
 *
 * Parses raw MIDI bytes into messages and pushes them
 * to a lock-free queue for processing by the input thread.
 */
class MidiController : public Controller {
public:
    MidiController();
    ~MidiController() override;

    // Non-copyable, non-movable (owns MIDI resources)
    MidiController(const MidiController&) = delete;
    MidiController& operator=(const MidiController&) = delete;
    MidiController(MidiController&&) = delete;
    MidiController& operator=(MidiController&&) = delete;

    /*
     * Initialize the MIDI controller
     * Return: 0 on success, -1 on failure
     */
    int init(struct Rt* rt, const char* hw);

    // Controller interface implementation
    int add_deck(struct Deck* d) override;
    ssize_t pollfds(struct pollfd* pe, size_t z) override;
    int realtime() override;
    void clear() override;

    const char* port_name() const { return port_name_; }

private:
    void process_midi_message();

    struct Rt* rt_ = nullptr;  // For accessing engine->input_state
    struct Midi midi_;
    struct Deck* deck_[NUMDECKS] = {nullptr, nullptr};

    char obuf_[180] = {};
    size_t ofill_ = 0;

    bool parsing_ = false;
    unsigned char parsed_bytes_ = 0;
    unsigned char midi_buffer_[3] = {};

    char port_name_[32] = {};
    bool initialized_ = false;
};

/*
 * Factory function to create a MidiController
 * Returns nullptr on failure
 */
std::unique_ptr<MidiController> create_midi_controller(struct Rt* rt, const char* hw);
