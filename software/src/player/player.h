/*
 * Copyright (C) 2018 Mark Hills <mark@xwax.org>
 * Copyright (C) 2019 Andrew Tait <rasteri@gmail.com>
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

#include <cstdio>
#include <cstdlib>

#include "../thread/spin.h"
#include "deck_input.h"

#define PLAYER_CHANNELS 2

// Playback mode state machine
enum class PlaybackMode {
    STOPPED,      // Motor braking toward 0, position frozen
    PLAYING,      // Motor at speed, slipmat simulation when platter released
    SCRATCHING,   // Position-locked to platter input
};

// How many samples each beep stage lasts
#define BEEPSPEED 4800

struct Track;
struct ScSettings;

struct Player
{
    // Timing
    double sample_dt;

    // Thread synchronization
    spin lock;

    // Current track
    Track* track;

    // Unified input state - all input fields in one place
    // Audio engine reads this at buffer boundaries
    sc::DeckInput input;

    // Playback mode
    PlaybackMode mode = PlaybackMode::STOPPED;
    bool just_play = false;     // Beat deck mode (no scratch control)
    bool stopped = false;       // Motor stopped (braking)

    // C++ member functions
    void init(unsigned int sample_rate, Track* track, struct ScSettings* settings);
    void clear();
    void set_track(Track* track);

    // Centralized state reset for track loading
    void reset_for_track_load();
};

