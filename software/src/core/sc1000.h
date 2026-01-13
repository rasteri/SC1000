/*
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

#include "../player/deck.h"
#include "../platform/crossfader.h"
#include "../control/mapping_registry.h"
#include "../control/input_state.h"
#include "../engine/deck_processing_state.h"
#include "sc_input.h"
#include <memory>

struct ScSettings;

//
// Abstract audio hardware interface
// Platform implementations (ALSA, etc.) inherit from this
//
class AudioHardware {
public:
    virtual ~AudioHardware() = default;

    virtual ssize_t pollfds(struct pollfd* pe, size_t z) = 0;
    virtual int handle() = 0;
    virtual unsigned int sample_rate() const = 0;
    virtual void start() = 0;
    virtual void stop() = 0;

    // Recording control (delegated to audio engine internally)
    virtual bool start_recording(int deck, double playback_position) = 0;
    virtual void stop_recording(int deck) = 0;
    virtual bool is_recording(int deck) const = 0;
    virtual bool has_loop(int deck) const = 0;
    virtual bool has_capture() const = 0;
    virtual void reset_loop(int deck) = 0;
    virtual Track* get_loop_track(int deck) = 0;
    virtual Track* peek_loop_track(int deck) = 0;

    // Query API (reads audio engine output state)
    virtual sc::audio::DeckProcessingState get_deck_state(int deck) const = 0;
    virtual double get_position(int deck) const = 0;
    virtual double get_pitch(int deck) const = 0;
    virtual double get_volume(int deck) const = 0;
};

struct Sc1000
{
    struct Deck scratch_deck;
    struct Deck beat_deck;

    std::unique_ptr<ScSettings> settings;

    // Input mappings (GPIO and MIDI) with indexed lookup
    sc::control::MappingRegistry mappings;

    // Global input state (shift key, pitch mode)
    sc::control::InputState input_state;

    // Crossfader input (handles ADC conversion and calibration)
    Crossfader crossfader;

    // Audio hardware (ALSA implementation)
    std::unique_ptr<AudioHardware> audio;
    bool fault = false;

    // Setup and lifecycle
    void setup(struct Rt* rt, const char* root_path);
    void load_sample_folders();
    void clear();

    // Audio hardware control (delegates to AudioHardware interface)
    void audio_start();
    void audio_stop();
    void handle_deck_recording();
    ssize_t audio_pollfds(struct pollfd* pe, size_t z);
    void audio_handle();
};