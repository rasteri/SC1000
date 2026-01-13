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

#include <cstdint>
#include <functional>

//
// MidiCommand - Type-safe MIDI message wrapper
//
// Encapsulates a 3-byte MIDI message with semantic helpers.
// Used for Mapping lookups and action dispatch.
//

struct MidiCommand {
    uint8_t status = 0;   // Status byte (type | channel)
    uint8_t data1 = 0;    // Note/CC number or pitch bend LSB
    uint8_t data2 = 0;    // Velocity/value or pitch bend MSB

    // Message type (upper nibble of status)
    uint8_t type() const { return status & 0xF0; }

    // MIDI channel (lower nibble of status, 0-15)
    uint8_t channel() const { return status & 0x0F; }

    // Message type checks
    bool is_note_on() const { return type() == 0x90 && data2 > 0; }
    bool is_note_off() const { return type() == 0x80 || (type() == 0x90 && data2 == 0); }
    bool is_cc() const { return type() == 0xB0; }
    bool is_pitch_bend() const { return type() == 0xE0; }

    // 14-bit pitch bend value (0-16383, center at 8192)
    uint16_t pitch_bend_value() const {
        return (static_cast<uint16_t>(data2) << 7) | data1;
    }

    // Normalized pitch bend (-1.0 to +1.0)
    double pitch_bend_normalized() const {
        return (static_cast<double>(pitch_bend_value()) - 8192.0) / 8192.0;
    }

    // Equality for use as map key
    // Pitch bend matches on status only (ignores data bytes which are values)
    // Everything else matches on status and data1 (note/CC number)
    bool operator==(const MidiCommand& o) const {
        if (is_pitch_bend()) return status == o.status;
        return status == o.status && data1 == o.data1;
    }

    // Factory from raw bytes
    static MidiCommand from_bytes(const uint8_t buf[3]) {
        return {buf[0], buf[1], buf[2]};
    }

    // Normalize note-on with velocity 0 to note-off
    void normalize() {
        if (type() == 0x90 && data2 == 0) {
            status = 0x80 | channel();
        }
    }
};

// Hash for use in unordered_map
struct MidiCommandHash {
    size_t operator()(const MidiCommand& cmd) const {
        // Pitch bend: hash only status (channel matters, but not value)
        if (cmd.is_pitch_bend()) {
            return std::hash<uint8_t>{}(cmd.status);
        }
        // Others: hash status + data1 (note/CC number)
        return std::hash<uint16_t>{}(
            static_cast<uint16_t>((cmd.status << 8) | cmd.data1)
        );
    }
};
