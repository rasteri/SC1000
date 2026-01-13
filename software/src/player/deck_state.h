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

struct Track;

//
// Deck state substates - grouped for clarity and maintainability
//
// These structs organize deck-level state into logical groups.
//

// Playlist navigation state
struct NavigationState {
    size_t folder_idx = 0;      // Current folder index in playlist
    int file_idx = 0;           // Current file index (-1 = loop position)
    bool files_present = false; // Whether playlist has any files

    bool is_at_loop() const { return file_idx == -1; }

    void reset() {
        folder_idx = 0;
        file_idx = 0;
        // files_present is set by load_folder, not reset
    }
};

// Rotary encoder state (platter position tracking)
struct EncoderState {
    static constexpr int UNINITIALIZED = 0xffff;

    int angle = UNINITIALIZED;      // Current filtered angle (0-4095)
    int angle_raw = UNINITIALIZED;  // Latest raw reading before filtering
    int32_t offset = 0;             // Offset to convert angle → track position

    bool is_initialized() const { return angle != UNINITIALIZED; }

    void reset() {
        angle = UNINITIALIZED;
        angle_raw = UNINITIALIZED;
        offset = 0;
    }
};

// Loop recording state (deck-level, persists across track changes)
struct LoopState {
    Track* track = nullptr;  // Recorded loop track (ref-counted)

    bool has_loop() const;  // Defined in deck.cpp where track is complete

    void reset() {
        // Note: Does NOT release the track - that's handled by Deck::clear()
        // This just resets the pointer for state tracking
        track = nullptr;
    }
};
