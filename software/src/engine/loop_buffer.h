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


//
// Loop Buffer - records audio input into memory for immediate scratching
//

#pragma once

#include <stdbool.h>
#include <stdint.h>

struct Track;

//
// Loop Buffer State
//
struct LoopBuffer {
    Track* track;          // Underlying track with block storage
    unsigned int write_pos;       // Current write position (samples)
    unsigned int max_samples;     // Maximum recording length (samples)
    unsigned int loop_length;     // Defined loop length (set after first recording)
    int sample_rate;              // Sample rate (48000)
    bool recording;               // Currently recording?
    bool length_locked;           // Loop length defined (first recording complete)?
    bool max_reached;             // Hit max length during recording?
};

// Initialize loop buffer with sample rate and max recording time
void loop_buffer_init(struct LoopBuffer* lb, int sample_rate, int max_seconds);

// Clear loop buffer (release track, reset state)
void loop_buffer_clear(struct LoopBuffer* lb);

// Start recording - creates new track, resets position
// Returns true on success, false if already recording or allocation failed
bool loop_buffer_start(struct LoopBuffer* lb);

// Stop recording - finalizes track length
void loop_buffer_stop(struct LoopBuffer* lb);

// Write audio frames to buffer (call from capture callback)
// Input is float [-1, 1], will be converted to S16 for storage
// Returns number of frames written (may be less than requested if max reached)
unsigned int loop_buffer_write_float(struct LoopBuffer* lb,
                                     float left,
                                     float right);

// Get the recorded track (acquires reference - caller must release)
// Returns NULL if no recording exists
Track* loop_buffer_get_track(struct LoopBuffer* lb);

// Check if recording is active
bool loop_buffer_is_recording(struct LoopBuffer* lb);

// Check if loop length is defined (first recording complete)
bool loop_buffer_has_loop(struct LoopBuffer* lb);

// Get current recording length in samples
unsigned int loop_buffer_get_length(struct LoopBuffer* lb);

// Reset/erase the loop (clears track and unlocks length for fresh recording)
void loop_buffer_reset(struct LoopBuffer* lb);

// Set write position for punch-in (syncs to playback position)
// position_samples should be within [0, loop_length)
void loop_buffer_set_position(struct LoopBuffer* lb, unsigned int position_samples);
