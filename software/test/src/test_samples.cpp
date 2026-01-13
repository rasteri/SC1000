/*
 * Copyright (C) 2024-2026 Niklas Klügel <lodsb@lodsb.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, as published by the Free Software Foundation.
 */

#include "test_samples.h"
#include <cstring>
#include <algorithm>
#include <random>
#include <numeric>

namespace sc {
namespace test {

static constexpr double PI = 3.14159265358979323846;
static constexpr double TWO_PI = 2.0 * PI;

// Helper to write samples to a track
static void write_sample(Track* t, unsigned int sample_idx,
                         signed short left, signed short right)
{
    if (sample_idx >= t->length) return;

    unsigned int block_idx = sample_idx / TRACK_BLOCK_SAMPLES;
    unsigned int offset = (sample_idx % TRACK_BLOCK_SAMPLES) * TRACK_CHANNELS;

    if (block_idx < t->blocks && t->block[block_idx]) {
        t->block[block_idx]->pcm[offset] = left;
        t->block[block_idx]->pcm[offset + 1] = right;
    }
}

Track* generate_sine(double frequency, int sample_rate,
                            unsigned int length_samples, double amplitude)
{
    Track* t = track_acquire_for_recording(sample_rate);
    if (!t) return nullptr;

    t->ensure_space(length_samples);
    t->set_length(length_samples);

    double phase_inc = TWO_PI * frequency / sample_rate;
    double phase = 0.0;

    for (unsigned int i = 0; i < length_samples; ++i) {
        double sample = amplitude * std::sin(phase);
        signed short s = static_cast<signed short>(sample * 32767.0);
        write_sample(t, i, s, s);  // Mono -> both channels

        phase += phase_inc;
        if (phase >= TWO_PI) phase -= TWO_PI;
    }

    return t;
}

Track* generate_sweep(double start_freq, double end_freq,
                             int sample_rate, unsigned int length_samples,
                             double amplitude)
{
    Track* t = track_acquire_for_recording(sample_rate);
    if (!t) return nullptr;

    t->ensure_space(length_samples);
    t->set_length(length_samples);

    double phase = 0.0;
    double log_start = std::log(start_freq);
    double log_end = std::log(end_freq);

    for (unsigned int i = 0; i < length_samples; ++i) {
        // Logarithmic frequency sweep
        double frac = static_cast<double>(i) / length_samples;
        double freq = std::exp(log_start + frac * (log_end - log_start));

        double phase_inc = TWO_PI * freq / sample_rate;
        phase += phase_inc;
        if (phase >= TWO_PI) phase -= TWO_PI;

        double sample = amplitude * std::sin(phase);
        signed short s = static_cast<signed short>(sample * 32767.0);
        write_sample(t, i, s, s);
    }

    return t;
}

Track* generate_noise(int sample_rate, unsigned int length_samples,
                             double amplitude, unsigned int seed)
{
    Track* t = track_acquire_for_recording(sample_rate);
    if (!t) return nullptr;

    t->ensure_space(length_samples);
    t->set_length(length_samples);

    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> dist(-1.0, 1.0);

    for (unsigned int i = 0; i < length_samples; ++i) {
        double sample = amplitude * dist(rng);
        signed short s = static_cast<signed short>(sample * 32767.0);
        write_sample(t, i, s, s);
    }

    return t;
}

Track* generate_impulses(int sample_rate, unsigned int length_samples,
                                double interval_seconds, double amplitude)
{
    Track* t = track_acquire_for_recording(sample_rate);
    if (!t) return nullptr;

    t->ensure_space(length_samples);
    t->set_length(length_samples);

    unsigned int interval_samples = static_cast<unsigned int>(interval_seconds * sample_rate);
    signed short impulse = static_cast<signed short>(amplitude * 32767.0);

    // Initialize to silence
    for (unsigned int b = 0; b < t->blocks; ++b) {
        if (t->block[b]) {
            std::memset(t->block[b]->pcm, 0,
                        TRACK_BLOCK_SAMPLES * TRACK_CHANNELS * sizeof(signed short));
        }
    }

    // Write impulses
    for (unsigned int i = 0; i < length_samples; i += interval_samples) {
        write_sample(t, i, impulse, impulse);
    }

    return t;
}

Track* generate_silence(int sample_rate, unsigned int length_samples)
{
    Track* t = track_acquire_for_recording(sample_rate);
    if (!t) return nullptr;

    t->ensure_space(length_samples);
    t->set_length(length_samples);

    // Zero all blocks
    for (unsigned int b = 0; b < t->blocks; ++b) {
        if (t->block[b]) {
            std::memset(t->block[b]->pcm, 0,
                        TRACK_BLOCK_SAMPLES * TRACK_CHANNELS * sizeof(signed short));
        }
    }

    return t;
}

Track* generate_from_buffer(const std::vector<float>& buffer, int sample_rate)
{
    Track* t = track_acquire_for_recording(sample_rate);
    if (!t) return nullptr;

    unsigned int length_samples = static_cast<unsigned int>(buffer.size() / 2);  // Stereo
    t->ensure_space(length_samples);
    t->set_length(length_samples);

    for (unsigned int i = 0; i < length_samples; ++i) {
        float left = buffer[i * 2];
        float right = buffer[i * 2 + 1];

        // Clamp to valid range
        left = std::max(-1.0f, std::min(1.0f, left));
        right = std::max(-1.0f, std::min(1.0f, right));

        signed short sl = static_cast<signed short>(left * 32767.0f);
        signed short sr = static_cast<signed short>(right * 32767.0f);
        write_sample(t, i, sl, sr);
    }

    return t;
}

// Analysis utilities

double calculate_rms(const std::vector<float>& buffer)
{
    if (buffer.empty()) return 0.0;

    double sum_sq = 0.0;
    for (float sample : buffer) {
        sum_sq += sample * sample;
    }
    return std::sqrt(sum_sq / buffer.size());
}

double find_peak_frequency(const std::vector<float>& buffer, int sample_rate,
                           double min_freq, double max_freq)
{
    // Simple DFT-based peak finder (not optimized, for testing only)
    // For production use, replace with proper FFT

    size_t n = buffer.size();
    if (n < 2) return 0.0;

    double max_magnitude = 0.0;
    double peak_freq = 0.0;

    // Frequency resolution
    double freq_step = static_cast<double>(sample_rate) / n;
    int min_bin = static_cast<int>(min_freq / freq_step);
    int max_bin = static_cast<int>(max_freq / freq_step);
    max_bin = std::min(max_bin, static_cast<int>(n / 2));

    for (int k = min_bin; k <= max_bin; ++k) {
        double real = 0.0, imag = 0.0;
        double freq = k * freq_step;

        for (size_t i = 0; i < n; ++i) {
            double angle = TWO_PI * k * i / n;
            real += buffer[i] * std::cos(angle);
            imag -= buffer[i] * std::sin(angle);
        }

        double magnitude = std::sqrt(real * real + imag * imag);
        if (magnitude > max_magnitude) {
            max_magnitude = magnitude;
            peak_freq = freq;
        }
    }

    return peak_freq;
}

double calculate_correlation(const std::vector<float>& a, const std::vector<float>& b)
{
    if (a.empty() || b.empty() || a.size() != b.size()) return 0.0;

    size_t n = a.size();

    // Calculate means
    double mean_a = std::accumulate(a.begin(), a.end(), 0.0) / n;
    double mean_b = std::accumulate(b.begin(), b.end(), 0.0) / n;

    // Calculate correlation
    double sum_ab = 0.0, sum_a2 = 0.0, sum_b2 = 0.0;
    for (size_t i = 0; i < n; ++i) {
        double da = a[i] - mean_a;
        double db = b[i] - mean_b;
        sum_ab += da * db;
        sum_a2 += da * da;
        sum_b2 += db * db;
    }

    double denom = std::sqrt(sum_a2 * sum_b2);
    if (denom < 1e-10) return 0.0;

    return sum_ab / denom;
}

bool buffers_similar(const std::vector<float>& a, const std::vector<float>& b,
                     double tolerance)
{
    if (a.size() != b.size()) return false;

    for (size_t i = 0; i < a.size(); ++i) {
        if (std::abs(a[i] - b[i]) > tolerance) {
            return false;
        }
    }
    return true;
}

std::vector<float> extract_channel(const std::vector<float>& stereo_buffer, int channel)
{
    std::vector<float> mono;
    mono.reserve(stereo_buffer.size() / 2);

    for (size_t i = channel; i < stereo_buffer.size(); i += 2) {
        mono.push_back(stereo_buffer[i]);
    }

    return mono;
}

// WAV file writing

bool write_wav(const char* path, const std::vector<float>& buffer,
               int sample_rate, int channels)
{
    FILE* f = fopen(path, "wb");
    if (!f) return false;

    uint32_t num_samples = static_cast<uint32_t>(buffer.size());
    uint32_t data_size = num_samples * sizeof(int16_t);
    uint32_t file_size = 36 + data_size;

    // RIFF header
    fwrite("RIFF", 1, 4, f);
    fwrite(&file_size, 4, 1, f);
    fwrite("WAVE", 1, 4, f);

    // fmt chunk
    fwrite("fmt ", 1, 4, f);
    uint32_t fmt_size = 16;
    uint16_t audio_format = 1;  // PCM
    uint16_t num_channels = static_cast<uint16_t>(channels);
    uint32_t sample_rate_u = static_cast<uint32_t>(sample_rate);
    uint32_t byte_rate = sample_rate_u * num_channels * sizeof(int16_t);
    uint16_t block_align = static_cast<uint16_t>(num_channels * sizeof(int16_t));
    uint16_t bits_per_sample = 16;

    fwrite(&fmt_size, 4, 1, f);
    fwrite(&audio_format, 2, 1, f);
    fwrite(&num_channels, 2, 1, f);
    fwrite(&sample_rate_u, 4, 1, f);
    fwrite(&byte_rate, 4, 1, f);
    fwrite(&block_align, 2, 1, f);
    fwrite(&bits_per_sample, 2, 1, f);

    // data chunk
    fwrite("data", 1, 4, f);
    fwrite(&data_size, 4, 1, f);

    // Convert float to 16-bit PCM and write
    for (float sample : buffer) {
        // Clamp to [-1, 1]
        if (sample > 1.0f) sample = 1.0f;
        if (sample < -1.0f) sample = -1.0f;
        int16_t pcm = static_cast<int16_t>(sample * 32767.0f);
        fwrite(&pcm, sizeof(int16_t), 1, f);
    }

    fclose(f);
    return true;
}

bool write_wav_mono(const char* path, const std::vector<float>& buffer, int sample_rate)
{
    return write_wav(path, buffer, sample_rate, 1);
}

} // namespace test
} // namespace sc
