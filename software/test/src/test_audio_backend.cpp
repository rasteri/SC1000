/*
 * Copyright (C) 2024-2026 Niklas Klügel <lodsb@lodsb.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, as published by the Free Software Foundation.
 */

#include "test_audio_backend.h"
#include <cstring>
#include <algorithm>

namespace sc {
namespace test {

TestAudioBackend::TestAudioBackend(Sc1000* engine, unsigned int sample_rate)
    : engine_(engine)
    , sample_rate_(sample_rate)
    , period_size_(DEFAULT_PERIOD_SIZE)
{
    // Create audio engine (default to sinc interpolation, float format)
    audio_engine_ = sc::audio::AudioEngineBase::create(
        sc::audio::InterpolationMode::Sinc,
        SND_PCM_FORMAT_FLOAT_LE
    );

    // Initialize loop buffers
    audio_engine_->init_loop_buffers(sample_rate_, 60);  // 60 sec max loop

    // Pre-allocate period buffer
    period_buffer_.resize(period_size_ * 2);  // stereo
}

TestAudioBackend::~TestAudioBackend() = default;

bool TestAudioBackend::start_recording(int deck, double playback_position)
{
    return audio_engine_->start_recording(deck, playback_position);
}

void TestAudioBackend::stop_recording(int deck)
{
    audio_engine_->stop_recording(deck);
}

bool TestAudioBackend::is_recording(int deck) const
{
    return audio_engine_->is_recording(deck);
}

bool TestAudioBackend::has_loop(int deck) const
{
    return audio_engine_->has_loop(deck);
}

void TestAudioBackend::reset_loop(int deck)
{
    audio_engine_->reset_loop(deck);
}

Track* TestAudioBackend::get_loop_track(int deck)
{
    return audio_engine_->get_loop_track(deck);
}

Track* TestAudioBackend::peek_loop_track(int deck)
{
    return audio_engine_->peek_loop_track(deck);
}

sc::audio::DeckProcessingState TestAudioBackend::get_deck_state(int deck) const
{
    return audio_engine_->get_deck_state(deck);
}

double TestAudioBackend::get_position(int deck) const
{
    return audio_engine_->get_deck_state(deck).position;
}

double TestAudioBackend::get_pitch(int deck) const
{
    return audio_engine_->get_deck_state(deck).pitch;
}

double TestAudioBackend::get_volume(int deck) const
{
    return audio_engine_->get_deck_state(deck).volume;
}

void TestAudioBackend::render(unsigned long frames)
{
    if (!running_) return;

    // Ensure buffer is large enough
    if (period_buffer_.size() < frames * 2) {
        period_buffer_.resize(frames * 2);
    }

    // Clear period buffer
    std::fill(period_buffer_.begin(), period_buffer_.begin() + frames * 2, 0.0f);

    // Set up capture if available
    AudioCapture capture = {};
    std::vector<float> capture_buffer;

    if (capture_enabled_ && capture_offset_ < capture_input_.size()) {
        size_t available = capture_input_.size() - capture_offset_;
        size_t needed = frames * 2;  // stereo
        size_t to_copy = std::min(available, needed);

        capture_buffer.resize(needed, 0.0f);
        std::copy(
            capture_input_.begin() + capture_offset_,
            capture_input_.begin() + capture_offset_ + to_copy,
            capture_buffer.begin()
        );

        capture.buffer = capture_buffer.data();
        capture.format = SND_PCM_FORMAT_FLOAT_LE;
        capture.channels = 2;
        capture.bytes_per_sample = sizeof(float);
        capture.left_channel = 0;
        capture.right_channel = 1;

        capture_offset_ += to_copy;
    }

    // Process audio
    audio_engine_->process(
        engine_,
        capture_enabled_ ? &capture : nullptr,
        period_buffer_.data(),
        2,  // stereo
        frames
    );

    // Append to output buffer
    output_buffer_.insert(
        output_buffer_.end(),
        period_buffer_.begin(),
        period_buffer_.begin() + frames * 2
    );

    total_samples_ += frames;
}

void TestAudioBackend::render_seconds(double seconds)
{
    unsigned long total_frames = static_cast<unsigned long>(seconds * sample_rate_);
    unsigned long rendered = 0;

    while (rendered < total_frames) {
        unsigned long chunk = std::min(
            static_cast<unsigned long>(period_size_),
            total_frames - rendered
        );
        render(chunk);
        rendered += chunk;
    }
}

void TestAudioBackend::set_capture_input(const std::vector<float>& input)
{
    capture_input_ = input;
    capture_offset_ = 0;
}

} // namespace test
} // namespace sc
