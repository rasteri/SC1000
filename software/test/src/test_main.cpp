/*
 * Copyright (C) 2024-2026 Niklas Klügel <lodsb@lodsb.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, as published by the Free Software Foundation.
 */

#include "test_harness.h"
#include <cstdio>
#include <cstring>
#include <string>

// Global flag for WAV export
static bool g_dump_wav = false;
static std::string g_output_dir = ".";

// Helper to sanitize test name for filename
static std::string sanitize_filename(const std::string& name)
{
    std::string result;
    for (char c : name) {
        if (c == ' ' || c == '/') result += '_';
        else if (c >= 'a' && c <= 'z') result += c;
        else if (c >= 'A' && c <= 'Z') result += static_cast<char>(c + 32);  // lowercase
        else if (c >= '0' && c <= '9') result += c;
        // skip other characters
    }
    return result;
}

// Run a single test and optionally dump WAV
static sc::test::TestResult run_test_with_dump(
    const char* name,
    sc::test::TestResult (*test_func)(),
    sc::test::TestHarness* harness = nullptr)
{
    auto result = test_func();

    if (g_dump_wav && harness) {
        std::string filename = g_output_dir + "/" + sanitize_filename(result.name) + ".wav";
        const auto& output = harness->output();
        if (!output.empty()) {
            if (sc::test::write_wav(filename.c_str(), output, 48000, 2)) {
                printf("       Saved: %s\n", filename.c_str());
            }
        }
    }

    return result;
}

// Extended test functions that return harness for WAV export
static sc::test::TestResult test_stationary_playback_ext(sc::test::TestHarness& harness)
{
    sc::test::TestResult result;
    result.name = "Stationary playback at 1x speed";

    auto* sine = sc::test::generate_sine(440.0, 48000, 48000);
    harness.load_track(1, sine);

    harness.sequence().add(0.0, sc::test::TouchEvent{false});
    harness.sequence().add(0.0, sc::test::AdcEvent{1, 1023});

    harness.run(0.5);

    auto left = harness.output_left();
    if (left.empty()) {
        result.passed = false;
        result.details = "No output generated";
        track_release(sine);
        return result;
    }

    double rms = sc::test::calculate_rms(left);
    if (rms < 0.01) {
        result.passed = false;
        result.details = "Output RMS too low: " + std::to_string(rms);
        track_release(sine);
        return result;
    }

    double peak = sc::test::find_peak_frequency(left, 48000, 100, 1000);
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

static sc::test::TestResult test_scratch_forward_2x_ext(sc::test::TestHarness& harness)
{
    sc::test::TestResult result;
    result.name = "Scratch forward at 2x speed";

    auto* sine = sc::test::generate_sine(440.0, 48000, 48000);
    harness.load_track(1, sine);

    harness.sequence().add(0.0, sc::test::TouchEvent{true});
    harness.sequence().add(0.0, sc::test::AdcEvent{1, 1023});
    harness.sequence().add_encoder_ramp(0.0, 0.5, 0, 3072, 50);

    harness.run(0.5);

    auto left = harness.output_left();
    if (left.empty()) {
        result.passed = false;
        result.details = "No output generated";
        track_release(sine);
        return result;
    }

    double rms = sc::test::calculate_rms(left);
    if (rms < 0.01) {
        result.passed = false;
        result.details = "Output RMS too low: " + std::to_string(rms);
        track_release(sine);
        return result;
    }

    double peak = sc::test::find_peak_frequency(left, 48000, 400, 1200);
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

static sc::test::TestResult test_scratch_backward_1x_ext(sc::test::TestHarness& harness)
{
    sc::test::TestResult result;
    result.name = "Scratch backward at 1x speed";

    auto* sine = sc::test::generate_sine(440.0, 48000, 96000);
    harness.load_track(1, sine);

    harness.engine().scratch_deck.player.input.seek_to = 1.0;
    harness.engine().scratch_deck.player.input.position_offset = 0.0;

    harness.sequence().add(0.0, sc::test::TouchEvent{true});
    harness.sequence().add(0.0, sc::test::AdcEvent{1, 1023});
    harness.sequence().add(0.0, sc::test::EncoderEvent{3072});
    harness.sequence().add_encoder_ramp(0.0, 0.5, 3072, 3072 - 1536, 50);

    harness.run(0.5);

    auto left = harness.output_left();
    double rms = sc::test::calculate_rms(left);

    if (rms < 0.01) {
        result.passed = false;
        result.details = "Output RMS too low";
        track_release(sine);
        return result;
    }

    double peak = sc::test::find_peak_frequency(left, 48000, 200, 800);

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

static sc::test::TestResult test_pitch_midi_note_ext(sc::test::TestHarness& harness)
{
    sc::test::TestResult result;
    result.name = "Pitch via MIDI note (octave up)";

    auto* sine = sc::test::generate_sine(440.0, 48000, 48000);
    harness.load_track(1, sine);

    harness.engine().scratch_deck.player.input.pitch_note = 2.0;
    harness.engine().scratch_deck.player.input.touched = false;

    harness.sequence().add(0.0, sc::test::AdcEvent{1, 1023});

    harness.run(0.5);

    auto left = harness.output_left();
    double peak = sc::test::find_peak_frequency(left, 48000, 400, 1200);

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

static sc::test::TestResult test_frequency_scaling_ext(sc::test::TestHarness& harness)
{
    sc::test::TestResult result;
    result.name = "Frequency scaling verification";

    auto* sine = sc::test::generate_sine(1000.0, 48000, 48000);
    harness.load_track(1, sine);

    harness.engine().scratch_deck.player.input.pitch_fader = 1.5;
    harness.engine().scratch_deck.player.input.touched = false;

    harness.sequence().add(0.0, sc::test::AdcEvent{1, 1023});

    harness.run(0.5);

    auto left = harness.output_left();
    double peak = sc::test::find_peak_frequency(left, 48000, 500, 2500);

    double expected = 1500.0;
    double tolerance = 75.0;

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

static void print_usage(const char* prog)
{
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  --dump [dir]    Export test output as WAV files (default: current dir)\n");
    printf("  --help          Show this help\n");
}

int main(int argc, char* argv[])
{
    // Parse arguments
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--dump") == 0) {
            g_dump_wav = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                g_output_dir = argv[++i];
            }
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }

    printf("SC1000 Audio Engine Test Suite\n");
    printf("================================\n");
    if (g_dump_wav) {
        printf("WAV output: %s/\n", g_output_dir.c_str());
    }
    printf("\n");

    std::vector<sc::test::TestResult> results;

    // Run tests with WAV export support
    {
        sc::test::TestHarness harness;
        auto result = test_stationary_playback_ext(harness);
        if (g_dump_wav) {
            std::string filename = g_output_dir + "/" + sanitize_filename(result.name) + ".wav";
            sc::test::write_wav(filename.c_str(), harness.output(), 48000, 2);
            if (result.passed) result.details += " [" + filename + "]";
        }
        results.push_back(result);
    }

    {
        sc::test::TestHarness harness;
        auto result = test_scratch_forward_2x_ext(harness);
        if (g_dump_wav) {
            std::string filename = g_output_dir + "/" + sanitize_filename(result.name) + ".wav";
            sc::test::write_wav(filename.c_str(), harness.output(), 48000, 2);
            if (result.passed) result.details += " [" + filename + "]";
        }
        results.push_back(result);
    }

    {
        sc::test::TestHarness harness;
        auto result = test_scratch_backward_1x_ext(harness);
        if (g_dump_wav) {
            std::string filename = g_output_dir + "/" + sanitize_filename(result.name) + ".wav";
            sc::test::write_wav(filename.c_str(), harness.output(), 48000, 2);
            if (result.passed) result.details += " [" + filename + "]";
        }
        results.push_back(result);
    }

    {
        sc::test::TestHarness harness;
        auto result = test_pitch_midi_note_ext(harness);
        if (g_dump_wav) {
            std::string filename = g_output_dir + "/" + sanitize_filename(result.name) + ".wav";
            sc::test::write_wav(filename.c_str(), harness.output(), 48000, 2);
            if (result.passed) result.details += " [" + filename + "]";
        }
        results.push_back(result);
    }

    {
        sc::test::TestHarness harness;
        auto result = test_frequency_scaling_ext(harness);
        if (g_dump_wav) {
            std::string filename = g_output_dir + "/" + sanitize_filename(result.name) + ".wav";
            sc::test::write_wav(filename.c_str(), harness.output(), 48000, 2);
            if (result.passed) result.details += " [" + filename + "]";
        }
        results.push_back(result);
    }

    int passed = 0;
    int failed = 0;

    for (const auto& result : results) {
        if (result.passed) {
            printf("[PASS] %s\n", result.name.c_str());
            printf("       %s\n", result.details.c_str());
            passed++;
        } else {
            printf("[FAIL] %s\n", result.name.c_str());
            printf("       %s\n", result.details.c_str());
            failed++;
        }
        printf("\n");
    }

    printf("================================\n");
    printf("Results: %d passed, %d failed\n", passed, failed);

    return failed > 0 ? 1 : 0;
}
