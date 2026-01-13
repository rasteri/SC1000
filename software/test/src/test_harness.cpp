/*
 * Copyright (C) 2024-2026 Niklas Klügel <lodsb@lodsb.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, as published by the Free Software Foundation.
 */

#include "test_harness.h"
#include "core/sc_settings.h"
#include <cmath>
#include <cstdio>

namespace sc {
namespace test {

TestHarness::TestHarness()
{
    setup_engine();
}

TestHarness::~TestHarness()
{
    // Tracks are reference counted, engine cleanup handles them
}

void TestHarness::setup_engine()
{
    // Create minimal settings
    settings_ = std::make_unique<ScSettings>();
    settings_->sample_rate = sample_rate_;
    settings_->period_size = 256;
    settings_->buffer_period_factor = 4;
    settings_->platter_enabled = 1;
    settings_->platter_speed = 3072;
    settings_->slippiness = 100;
    settings_->brake_speed = 50;
    settings_->initial_volume = 1.0;
    settings_->max_volume = 1.0;
    settings_->pitch_range = 8;
    settings_->importer = "/bin/true";  // Dummy - not used in test mode

    engine_.settings = std::move(settings_);

    // Initialize decks
    engine_.beat_deck.init(engine_.settings.get());
    engine_.scratch_deck.init(engine_.settings.get());

    engine_.beat_deck.deck_no = 0;
    engine_.scratch_deck.deck_no = 1;

    // Set beat deck to just_play mode
    engine_.beat_deck.player.input.just_play = true;

    // Create test audio backend
    auto audio_ptr = std::make_unique<TestAudioBackend>(&engine_, sample_rate_);
    audio_ = audio_ptr.get();
    engine_.audio = std::move(audio_ptr);

    // Create input provider
    input_ = std::make_unique<TestInputProvider>(&sequence_);

    // Start audio
    audio_->start();
}

void TestHarness::load_track(int deck, Track* t)
{
    if (!t) return;

    if (deck == 0) {
        engine_.beat_deck.player.set_track(t);
        engine_.beat_deck.player.input.seek_to = 0.0;
        engine_.beat_deck.player.input.position_offset = 0.0;
    } else {
        engine_.scratch_deck.player.set_track(t);
        engine_.scratch_deck.player.input.seek_to = 0.0;
        engine_.scratch_deck.player.input.position_offset = 0.0;
    }
}

void TestHarness::apply_input_at(double time)
{
    input_->set_time(time);

    // Apply current input state to player
    auto& scratch_input = engine_.scratch_deck.player.input;
    auto& beat_input = engine_.beat_deck.player.input;

    // Encoder -> target position
    // platter_speed = 3072 means 3072 ticks per second of audio
    // Encoder is 4096 ticks per rotation
    double position = static_cast<double>(input_->encoder_angle()) /
                      engine_.settings->platter_speed;
    scratch_input.target_position = position;

    // Touch state
    scratch_input.touched = input_->cap_touched();

    // Volume from ADC (normalize 0-1023 to 0-1)
    beat_input.crossfader = static_cast<double>(input_->adc_value(0)) / 1023.0;
    scratch_input.crossfader = static_cast<double>(input_->adc_value(1)) / 1023.0;
}

void TestHarness::run(double duration_seconds)
{
    sequence_.finalize();

    unsigned long total_frames = static_cast<unsigned long>(duration_seconds * sample_rate_);
    unsigned long frames_per_chunk = 256;
    double dt = static_cast<double>(frames_per_chunk) / sample_rate_;

    unsigned long rendered = 0;
    while (rendered < total_frames) {
        unsigned long chunk = std::min(frames_per_chunk, total_frames - rendered);

        // Apply input state for current time
        apply_input_at(current_time_);

        // Render audio
        audio_->render(chunk);

        current_time_ += dt;
        rendered += chunk;
    }
}

void TestHarness::reset()
{
    audio_->clear_output();
    sequence_.clear();
    input_->set_time(0.0);
    current_time_ = 0.0;
}

bool TestHarness::run_assertions(const std::vector<AssertFunc>& assertions) const
{
    for (const auto& fn : assertions) {
        if (!fn(*this)) return false;
    }
    return true;
}

//
// Built-in test scenarios
//

TestResult test_stationary_playback()
{
    TestResult result;
    result.name = "Stationary playback at 1x speed";

    TestHarness harness;

    // Generate 1 second 440Hz sine at 48kHz
    auto* sine = generate_sine(440.0, 48000, 48000);
    harness.load_track(1, sine);  // Scratch deck

    // Set up: platter not touched (slipmat simulation at 1x)
    harness.sequence().add(0.0, TouchEvent{false});
    harness.sequence().add(0.0, AdcEvent{1, 1023});  // Full volume

    // Run for 0.5 seconds
    harness.run(0.5);

    // Verify output
    auto left = harness.output_left();
    if (left.empty()) {
        result.passed = false;
        result.details = "No output generated";
        track_release(sine);
        return result;
    }

    // Check RMS is non-zero (audio is playing)
    double rms = calculate_rms(left);
    if (rms < 0.01) {
        result.passed = false;
        result.details = "Output RMS too low: " + std::to_string(rms);
        track_release(sine);
        return result;
    }

    // Check peak frequency is near 440Hz
    double peak = find_peak_frequency(left, 48000, 100, 1000);
    if (std::abs(peak - 440.0) > 20.0) {
        result.passed = false;
        result.details = "Peak frequency " + std::to_string(peak) + " Hz, expected ~440 Hz";
        track_release(sine);
        return result;
    }

    result.passed = true;
    result.details = "RMS: " + std::to_string(rms) + ", Peak: " + std::to_string(peak) + " Hz";
    track_release(sine);
    return result;
}

TestResult test_scratch_forward_2x()
{
    TestResult result;
    result.name = "Scratch forward at 2x speed";

    TestHarness harness;

    // Generate 1 second 440Hz sine
    auto* sine = generate_sine(440.0, 48000, 48000);
    harness.load_track(1, sine);

    // Set up: touch and move encoder forward at 2x rate
    // platter_speed = 3072 ticks/sec of audio
    // For 2x speed, we need 6144 ticks/sec
    // In 0.5 seconds, that's 3072 ticks
    harness.sequence().add(0.0, TouchEvent{true});
    harness.sequence().add(0.0, AdcEvent{1, 1023});
    harness.sequence().add_encoder_ramp(0.0, 0.5, 0, 3072, 50);

    harness.run(0.5);

    auto left = harness.output_left();
    if (left.empty()) {
        result.passed = false;
        result.details = "No output generated";
        track_release(sine);
        return result;
    }

    double rms = calculate_rms(left);
    if (rms < 0.01) {
        result.passed = false;
        result.details = "Output RMS too low: " + std::to_string(rms);
        track_release(sine);
        return result;
    }

    // At 2x speed, 440Hz should become ~880Hz
    double peak = find_peak_frequency(left, 48000, 400, 1200);
    double expected = 880.0;

    if (std::abs(peak - expected) > 50.0) {
        result.passed = false;
        result.details = "Peak frequency " + std::to_string(peak) +
                         " Hz, expected ~" + std::to_string(expected) + " Hz";
        track_release(sine);
        return result;
    }

    result.passed = true;
    result.details = "Peak: " + std::to_string(peak) + " Hz (expected ~880 Hz)";
    track_release(sine);
    return result;
}

TestResult test_scratch_backward_1x()
{
    TestResult result;
    result.name = "Scratch backward at 1x speed";

    TestHarness harness;

    // Use 2 second track to avoid wrap-around issues at boundary
    auto* sine = generate_sine(440.0, 48000, 96000);
    harness.load_track(1, sine);

    // Start at encoder position 3072, move to 1536 in 0.5 seconds
    // That's 1536 ticks in 0.5 seconds = 3072 ticks/sec = 1x backward
    // platter_speed = 3072 ticks per second of audio
    //
    // Important: We must seek the playback position to match the encoder!
    // Encoder 3072 / platter_speed 3072 = 1.0 second position
    harness.engine().scratch_deck.player.input.seek_to = 1.0;
    harness.engine().scratch_deck.player.input.position_offset = 0.0;

    harness.sequence().add(0.0, TouchEvent{true});
    harness.sequence().add(0.0, AdcEvent{1, 1023});
    harness.sequence().add(0.0, EncoderEvent{3072});  // Start position
    harness.sequence().add_encoder_ramp(0.0, 0.5, 3072, 3072 - 1536, 50);

    harness.run(0.5);

    auto left = harness.output_left();
    double rms = calculate_rms(left);

    if (rms < 0.01) {
        result.passed = false;
        result.details = "Output RMS too low";
        track_release(sine);
        return result;
    }

    // Backward playback should still produce 440Hz (just reversed phase)
    double peak = find_peak_frequency(left, 48000, 200, 800);

    if (std::abs(peak - 440.0) > 30.0) {
        result.passed = false;
        result.details = "Peak frequency " + std::to_string(peak) + " Hz, expected ~440 Hz";
        track_release(sine);
        return result;
    }

    result.passed = true;
    result.details = "Peak: " + std::to_string(peak) + " Hz (backward scratch)";
    track_release(sine);
    return result;
}

TestResult test_pitch_midi_note()
{
    TestResult result;
    result.name = "Pitch via MIDI note (octave up)";

    TestHarness harness;

    auto* sine = generate_sine(440.0, 48000, 48000);
    harness.load_track(1, sine);

    // Set pitch_note to 2.0 (one octave up)
    harness.engine().scratch_deck.player.input.pitch_note = 2.0;
    harness.engine().scratch_deck.player.input.touched = false;  // Slipmat mode

    harness.sequence().add(0.0, AdcEvent{1, 1023});

    harness.run(0.5);

    auto left = harness.output_left();
    double peak = find_peak_frequency(left, 48000, 400, 1200);

    // Should be 880Hz (440 * 2)
    if (std::abs(peak - 880.0) > 50.0) {
        result.passed = false;
        result.details = "Peak frequency " + std::to_string(peak) + " Hz, expected ~880 Hz";
        track_release(sine);
        return result;
    }

    result.passed = true;
    result.details = "Peak: " + std::to_string(peak) + " Hz (pitch_note = 2.0)";
    track_release(sine);
    return result;
}

TestResult test_frequency_scaling()
{
    TestResult result;
    result.name = "Frequency scaling verification";

    TestHarness harness;

    // Test with 1000Hz sine for easier measurement
    auto* sine = generate_sine(1000.0, 48000, 48000);
    harness.load_track(1, sine);

    // Set pitch_fader to 1.5x
    harness.engine().scratch_deck.player.input.pitch_fader = 1.5;
    harness.engine().scratch_deck.player.input.touched = false;

    harness.sequence().add(0.0, AdcEvent{1, 1023});

    harness.run(0.5);

    auto left = harness.output_left();
    double peak = find_peak_frequency(left, 48000, 500, 2500);

    // Should be 1500Hz (1000 * 1.5)
    double expected = 1500.0;
    double tolerance = 75.0;  // 5% tolerance

    if (std::abs(peak - expected) > tolerance) {
        result.passed = false;
        result.details = "Peak frequency " + std::to_string(peak) +
                         " Hz, expected " + std::to_string(expected) + " Hz";
        track_release(sine);
        return result;
    }

    result.passed = true;
    result.details = "Peak: " + std::to_string(peak) + " Hz (expected " +
                     std::to_string(expected) + " Hz)";
    track_release(sine);
    return result;
}

std::vector<TestResult> run_all_tests()
{
    std::vector<TestResult> results;

    results.push_back(test_stationary_playback());
    results.push_back(test_scratch_forward_2x());
    results.push_back(test_scratch_backward_1x());
    results.push_back(test_pitch_midi_note());
    results.push_back(test_frequency_scaling());

    return results;
}

} // namespace test
} // namespace sc
