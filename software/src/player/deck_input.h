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

//
// DeckInput - All input state for a single deck
//
// This struct contains ALL fields written by the input thread.
// The audio engine reads these values at buffer boundaries.
//
// OWNERSHIP: Input thread writes, audio engine reads.
// Thread safety: Single writer (input thread), single reader (audio engine).
// No locks needed - worst case is audio engine sees slightly stale value.
//

struct Track;  // Forward declaration

namespace sc {

// Playback source selection
enum class PlaybackSource {
    File,   // Playing from loaded file
    Loop    // Playing from recorded loop
};

// Feedback beep types (requests from input to audio engine)
enum class BeepType {
    None = -1,
    RecordingStart = 0,
    RecordingStop = 1,
    RecordingError = 2
};

struct DeckInput {
    // === Encoder/Platter ===
    int32_t encoder_angle = 0;      // Raw encoder angle in ticks
    int32_t encoder_offset = 0;     // Offset for position calculation
    double target_position = 0.0;   // Encoder-derived target position (for scratching)
    bool touched = false;           // Capacitive touch state

    // === Transport ===
    bool stopped = false;           // Motor stopped (braking)
    double seek_to = -1.0;          // Seek request (-1 = no seek pending)
    double position_offset = 0.0;   // Track start offset (for cue points)

    // === Pitch (all multiplicative) ===
    double pitch_fader = 1.0;       // Hardware/MIDI pitch fader
    double pitch_note = 1.0;        // MIDI note (equal temperament)
    double pitch_bend = 1.0;        // MIDI pitch bend

    // === Volume ===
    double volume_knob = 0.0;       // Volume pot or MIDI CC (0-1), default muted for safety
    double crossfader = 0.0;        // Crossfader position after cut logic (0-1), default muted until input sets it

    // === Source Selection ===
    PlaybackSource source = PlaybackSource::File;

    // === Track Loading ===
    // Set load_track to request a track change. Audio engine applies and clears.
    Track* load_track = nullptr;
    double load_start_position = 0.0;

    // === Recording Requests ===
    bool record_start = false;      // Request to start recording
    bool record_stop = false;       // Request to stop recording

    // === Feedback Requests ===
    BeepType beep_request = BeepType::None;  // Request a beep sound

    // === Mode flags ===
    bool just_play = false;         // Beat deck mode (no platter interaction)

    // Combined external pitch (fader * note * bend)
    double external_pitch() const {
        return pitch_fader * pitch_note * pitch_bend;
    }

    // Reset pitch modifiers to neutral
    void reset_pitch() {
        pitch_fader = 1.0;
        pitch_note = 1.0;
        pitch_bend = 1.0;
    }

    // Clear one-shot requests (called by audio engine after processing)
    void clear_requests() {
        seek_to = -1.0;
        load_track = nullptr;
        record_start = false;
        record_stop = false;
        beep_request = BeepType::None;
    }
};

} // namespace sc
