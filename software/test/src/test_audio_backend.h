/*
 * Copyright (C) 2024-2026 Niklas Klügel <lodsb@lodsb.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, as published by the Free Software Foundation.
 */

#pragma once

#include "core/sc1000.h"
#include "engine/audio_engine.h"
#include <vector>
#include <cstdint>

namespace sc {
namespace test {

//
// TestAudioBackend - Mock audio hardware for testing
//
// Implements AudioHardware interface but renders to an in-memory buffer
// instead of real audio hardware. Supports:
// - Synchronous rendering (no threading, deterministic)
// - Output buffer capture for analysis
// - Optional input injection for recording tests
//
class TestAudioBackend : public AudioHardware {
public:
    static constexpr unsigned int DEFAULT_SAMPLE_RATE = 48000;
    static constexpr unsigned int DEFAULT_PERIOD_SIZE = 256;

    TestAudioBackend(Sc1000* engine, unsigned int sample_rate = DEFAULT_SAMPLE_RATE);
    ~TestAudioBackend() override;

    // AudioHardware interface (mostly no-ops for testing)
    ssize_t pollfds(struct pollfd* pe, size_t z) override { return 0; }
    int handle() override { return 0; }
    unsigned int sample_rate() const override { return sample_rate_; }
    void start() override { running_ = true; }
    void stop() override { running_ = false; }

    // Recording control
    bool start_recording(int deck, double playback_position) override;
    void stop_recording(int deck) override;
    bool is_recording(int deck) const override;
    bool has_loop(int deck) const override;
    bool has_capture() const override { return capture_enabled_; }
    void reset_loop(int deck) override;
    Track* get_loop_track(int deck) override;
    Track* peek_loop_track(int deck) override;

    // Query API
    sc::audio::DeckProcessingState get_deck_state(int deck) const override;
    double get_position(int deck) const override;
    double get_pitch(int deck) const override;
    double get_volume(int deck) const override;

    // === Test-specific API ===

    // Render a specific number of samples synchronously
    // This drives the audio engine directly without threading
    void render(unsigned long frames);

    // Render for a specific duration in seconds
    void render_seconds(double seconds);

    // Get rendered output buffer (interleaved stereo, float normalized -1 to 1)
    const std::vector<float>& output_buffer() const { return output_buffer_; }

    // Clear output buffer (for fresh test run)
    void clear_output() { output_buffer_.clear(); }

    // Set capture input for recording tests (optional)
    void set_capture_input(const std::vector<float>& input);
    void enable_capture(bool enabled) { capture_enabled_ = enabled; }

    // Get total rendered sample count
    size_t total_samples_rendered() const { return total_samples_; }

    // Get render time in seconds
    double render_time() const { return static_cast<double>(total_samples_) / sample_rate_; }

private:
    Sc1000* engine_;
    std::unique_ptr<sc::audio::AudioEngineBase> audio_engine_;

    unsigned int sample_rate_;
    unsigned int period_size_;
    bool running_ = false;

    // Output buffer (interleaved stereo float)
    std::vector<float> output_buffer_;
    size_t total_samples_ = 0;

    // Capture input for recording tests
    std::vector<float> capture_input_;
    size_t capture_offset_ = 0;
    bool capture_enabled_ = false;

    // Temporary buffer for period rendering
    std::vector<float> period_buffer_;
};

} // namespace test
} // namespace sc
