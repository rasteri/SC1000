/*
 * Copyright (C) 2014 Mark Hills <mark@xwax.org>
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

#include <cmath>
#include <map>
#include <optional>
#include <string>

// Sentinel value for unset cue points in .cue file format (file I/O only)
constexpr double CUE_FILE_UNSET = HUGE_VAL;

// TODO: Auto-slice cue marker generation
// Add automatic cue marker placement for slicing the current deck's sample.
// UI concept: Press cue markers 1 & 2 simultaneously to trigger auto-slice:
//   - First press: create 8 evenly-spaced slices across the sample
//   - Second press: 16 slices
//   - Third press: 32 slices
//   - Fourth press: clear auto-slices / return to manual cues
// Implementation notes:
//   - Need to detect simultaneous button press in actions.cpp
//   - Calculate slice positions based on sample length (track->length)
//   - Consider beat-grid alignment if BPM detection is added later
//   - Store slice count state per deck to cycle through options

// Cue points manager
// Stores labeled cue positions in a sample, with file persistence
class Cues {
public:
    Cues() = default;

    // Set a cue point at the given position
    void set(unsigned int label, double position);

    // Get a cue point position, returns nullopt if not set
    std::optional<double> get(unsigned int label) const;

    // Remove a cue point
    void unset(unsigned int label);

    // Clear all cue points
    void reset();

    // Check if a cue point is set
    bool is_set(unsigned int label) const;

    // File I/O
    void load_from_file(const char* pathname);
    void save_to_file(const char* pathname) const;

private:
    std::map<unsigned int, double> positions_;
};

