/*
 * Copyright (C) 2024-2026 Niklas Klügel <lodsb@lodsb.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, as published by the Free Software Foundation.
 */

#pragma once

#include "test_audio_backend.h"
#include "input_sequence.h"
#include "test_samples.h"
#include "core/sc1000.h"
#include <functional>
#include <string>

namespace sc {
namespace test {

//
// TestHarness - Complete test environment for audio engine
//
// Sets up engine, tracks, input sequences, and captures output
// for verification.
//
class TestHarness {
public:
    TestHarness();
    ~TestHarness();

    // Access to components
    Sc1000& engine() { return engine_; }
    TestAudioBackend& audio() { return *audio_; }
    InputSequence& sequence() { return sequence_; }
    TestInputProvider& input() { return *input_; }

    // Load a test track onto a deck
    void load_track(int deck, Track* t);

    // Set up player input state at a point in time
    // (Simulates what sc_input normally does)
    void apply_input_at(double time);

    // Run simulation for a duration, applying input events
    void run(double duration_seconds);

    // Get output buffer
    const std::vector<float>& output() const { return audio_->output_buffer(); }

    // Clear output and reset for fresh test
    void reset();

    // Convenience: get left or right channel
    std::vector<float> output_left() const { return extract_channel(output(), 0); }
    std::vector<float> output_right() const { return extract_channel(output(), 1); }

    // Run assertions (returns true if all pass)
    using AssertFunc = std::function<bool(const TestHarness&)>;
    bool run_assertions(const std::vector<AssertFunc>& assertions) const;

private:
    Sc1000 engine_;
    TestAudioBackend* audio_ = nullptr;  // Owned by engine_.audio unique_ptr
    std::unique_ptr<ScSettings> settings_;

    InputSequence sequence_;
    std::unique_ptr<TestInputProvider> input_;

    double current_time_ = 0.0;
    unsigned int sample_rate_ = 48000;

    void setup_engine();
};

//
// Built-in test scenarios
//

struct TestResult {
    bool passed;
    std::string name;
    std::string details;
};

// Test: stationary playback at 1x speed
TestResult test_stationary_playback();

// Test: scratch forward at 2x speed
TestResult test_scratch_forward_2x();

// Test: scratch backward at 1x speed
TestResult test_scratch_backward_1x();

// Test: pitch via MIDI note
TestResult test_pitch_midi_note();

// Test: frequency verification (output should be input freq * pitch)
TestResult test_frequency_scaling();

// Run all built-in tests
std::vector<TestResult> run_all_tests();

} // namespace test
} // namespace sc
