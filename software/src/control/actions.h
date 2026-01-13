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


// Action dispatch for SC1000 control events
// Maps input events (GPIO, MIDI) to deck operations
#pragma once

#include "../core/sc_input.h"

struct Deck;
struct Sc1000;
struct ScSettings;

namespace sc {
namespace control {

class InputState;  // Forward declaration

// Execute an action on a specific deck
void perform_action_for_deck(Deck* d, const Mapping* map,
                             const unsigned char midi_buffer[3],
                             Sc1000* engine, ScSettings* settings,
                             InputState& input_state);

// Dispatch an input event to the appropriate deck
void dispatch_event(const Mapping* map, const unsigned char midi_buffer[3],
                    Sc1000* engine, ScSettings* settings,
                    InputState& input_state);

} // namespace control
} // namespace sc
