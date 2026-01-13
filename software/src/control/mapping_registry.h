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


#pragma once

#include "../core/sc_input.h"
#include "../input/midi_command.h"
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace sc {
namespace control {

//
// ButtonState - Runtime state for a mapped button
//
// Separated from Mapping (which is configuration only).
// Stored in a map keyed by Mapping index.
//
struct ButtonState {
    int debounce = 0;
    bool shifted_at_press = false;
};

//
// GpioKey - Hash key for GPIO Mapping lookup
//
struct GpioKey {
    uint8_t port;
    uint8_t pin;
    EventType edge;

    bool operator==(const GpioKey& o) const {
        return port == o.port && pin == o.pin && edge == o.edge;
    }
};

struct GpioKeyHash {
    size_t operator()(const GpioKey& k) const {
        return (static_cast<size_t>(k.port) << 16) |
               (static_cast<size_t>(k.pin) << 8) |
               static_cast<size_t>(k.edge);
    }
};

//
// MidiKey - Hash key for MIDI Mapping lookup
//
struct MidiKey {
    MidiCommand cmd;
    EventType edge;

    bool operator==(const MidiKey& o) const {
        return cmd == o.cmd && edge == o.edge;
    }
};

struct MidiKeyHash {
    size_t operator()(const MidiKey& k) const {
        return MidiCommandHash{}(k.cmd) ^ (static_cast<size_t>(k.edge) << 24);
    }
};

//
// MappingRegistry - Indexed storage for input mappings
//
// Provides O(1) lookup by GPIO (port, pin, edge) or MIDI (command, edge).
// Stores mappings in a vector with hash indices for fast lookup.
//
class MappingRegistry {
public:
    // Add a Mapping (updates indices automatically)
    void add(Mapping m);

    // Clear all mappings
    void clear();

    // O(1) lookup - returns nullptr if not found
    Mapping* find_gpio(uint8_t port, uint8_t pin, EventType edge);
    Mapping* find_midi(const MidiCommand& cmd, EventType edge);

    // Get Mapping by index (for ButtonState association)
    Mapping* at(size_t index);
    const Mapping* at(size_t index) const;

    // Iteration (for init, debug, serialization)
    std::vector<Mapping>& all() { return mappings_; }
    const std::vector<Mapping>& all() const { return mappings_; }

    // Count
    size_t size() const { return mappings_.size(); }
    bool empty() const { return mappings_.empty(); }

private:
    std::vector<Mapping> mappings_;
    std::unordered_map<GpioKey, size_t, GpioKeyHash> gpio_index_;
    std::unordered_map<MidiKey, size_t, MidiKeyHash> midi_index_;

    void index_mapping(size_t idx);
};

} // namespace control
} // namespace sc
