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

#include "../player/deck_input.h"  // For BeepType

//
// DeckProcessingState - Audio engine output state per deck
//
// This struct contains ALL fields written by the audio engine.
// External code can query these values via AudioEngine::get_*() methods.
//
// OWNERSHIP: Audio engine writes, external code reads (via query API).
// Thread safety: Single writer (audio engine), multiple readers (safe for POD reads).
//

namespace sc {
namespace audio {

struct DeckProcessingState {
    // === Playback Position ===
    double position = 0.0;              // Current playback position (seconds)
    double position_offset = 0.0;       // Track start offset (copied from input on seek)

    // === Pitch Processing ===
    double pitch = 0.0;                 // Current smoothed pitch (playback speed)
    double motor_speed = 1.0;           // Virtual motor speed (brakes when stopped)
    double last_external_speed = 1.0;   // For instant MIDI response detection

    // === Volume Processing ===
    double fader_current = 0.0;         // Smoothed crossfader position, default muted until input sets it
    double volume = 0.0;                // Current output volume (after all processing)

    // === Platter State ===
    bool touched_prev = false;          // Previous frame touch state (for edge detection)

    // === Recording State ===
    bool is_recording = false;          // Currently recording audio
    bool has_loop = false;              // Loop buffer contains audio
    double loop_length = 0.0;           // Length of recorded loop in seconds

    // === Playback Source ===
    PlaybackSource source = PlaybackSource::File;  // Current playback source

    // === Feedback State ===
    BeepType current_beep = BeepType::None;  // Currently playing beep
    unsigned long beep_position = 0;         // Sample position in beep

    // === Derived Values ===

    // Get elapsed time (position relative to offset)
    double elapsed() const {
        return position - position_offset;
    }

    // Is the deck actively playing (non-zero pitch)?
    bool is_active() const {
        return pitch > 0.01 || pitch < -0.01;
    }

    // Reset all state to defaults
    void reset() {
        position = 0.0;
        position_offset = 0.0;
        pitch = 0.0;
        motor_speed = 1.0;
        last_external_speed = 1.0;
        fader_current = 0.0;  // Match default member initializer (muted until input sets it)
        volume = 0.0;
        touched_prev = false;
        // Note: recording state is NOT reset here (managed separately)
    }

    // Reset recording state
    void reset_recording() {
        is_recording = false;
        has_loop = false;
        loop_length = 0.0;
    }

    // Reset feedback state
    void reset_feedback() {
        current_beep = BeepType::None;
        beep_position = 0;
    }
};

} // namespace audio
} // namespace sc
