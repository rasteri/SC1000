/*
 * Copyright (C) 2024-2026 Niklas Klügel <lodsb@lodsb.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, as published by the Free Software Foundation.
 */

#pragma once

#include "player/track.h"
#include <cmath>
#include <vector>

namespace sc {
namespace test {

//
// Test sample generators
//
// Create track structures filled with deterministic test signals
// for audio engine verification.
//

// Generate a pure sine wave
// Returns a track that must be released with track_release()
Track* generate_sine(
    double frequency,           // Hz
    int sample_rate,           // Samples per second
    unsigned int length_samples,  // Total samples
    double amplitude = 0.9     // 0.0 - 1.0
);

// Generate a frequency sweep (chirp)
Track* generate_sweep(
    double start_freq,         // Start frequency in Hz
    double end_freq,           // End frequency in Hz
    int sample_rate,
    unsigned int length_samples,
    double amplitude = 0.9
);

// Generate white noise
Track* generate_noise(
    int sample_rate,
    unsigned int length_samples,
    double amplitude = 0.9,
    unsigned int seed = 12345
);

// Generate impulse train (clicks at regular intervals)
Track* generate_impulses(
    int sample_rate,
    unsigned int length_samples,
    double interval_seconds,   // Time between impulses
    double amplitude = 0.9
);

// Generate silence
Track* generate_silence(
    int sample_rate,
    unsigned int length_samples
);

// Generate a track from raw float samples (normalized -1 to 1, stereo interleaved)
Track* generate_from_buffer(
    const std::vector<float>& buffer,
    int sample_rate
);

//
// Analysis utilities
//

// Simple RMS level calculation
double calculate_rms(const std::vector<float>& buffer);

// Find peak frequency using DFT (returns Hz)
// Note: Simple implementation, not FFT-optimized
double find_peak_frequency(
    const std::vector<float>& buffer,
    int sample_rate,
    double min_freq = 20.0,
    double max_freq = 20000.0
);

// Calculate correlation between two buffers (for similarity comparison)
// Returns 0.0 for uncorrelated, 1.0 for identical
double calculate_correlation(
    const std::vector<float>& a,
    const std::vector<float>& b
);

// Check if two buffers are similar within tolerance
bool buffers_similar(
    const std::vector<float>& a,
    const std::vector<float>& b,
    double tolerance = 0.01
);

// Extract mono channel from stereo buffer
std::vector<float> extract_channel(
    const std::vector<float>& stereo_buffer,
    int channel  // 0 = left, 1 = right
);

// Write buffer to WAV file (for external analysis)
// Buffer is interleaved stereo float samples (-1.0 to 1.0)
bool write_wav(
    const char* path,
    const std::vector<float>& buffer,
    int sample_rate,
    int channels = 2
);

// Write mono buffer to WAV file
bool write_wav_mono(
    const char* path,
    const std::vector<float>& buffer,
    int sample_rate
);

} // namespace test
} // namespace sc
