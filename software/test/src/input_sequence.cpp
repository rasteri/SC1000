/*
 * Copyright (C) 2024-2026 Niklas Klügel <lodsb@lodsb.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, as published by the Free Software Foundation.
 */

#include "input_sequence.h"
#include <algorithm>
#include <cmath>

namespace sc {
namespace test {

void InputSequence::add_event(double time, InputEvent event)
{
    events_.push_back({time, event});
    finalized_ = false;
}

void InputSequence::add_encoder_ramp(double start_time, double duration,
                                      int32_t start_angle, int32_t end_angle, int steps,
                                      bool wrap_around)
{
    for (int i = 0; i <= steps; ++i) {
        double t = start_time + (duration * i / steps);
        double frac = static_cast<double>(i) / steps;

        int32_t delta = end_angle - start_angle;

        if (wrap_around) {
            // Handle wrap-around (encoder is 0-4095)
            // Take the shorter path around the circle
            if (delta > 2048) delta -= 4096;
            if (delta < -2048) delta += 4096;
        }

        int32_t angle = start_angle + static_cast<int32_t>(delta * frac);

        if (wrap_around) {
            // Wrap to valid range
            while (angle < 0) angle += 4096;
            while (angle >= 4096) angle -= 4096;
        }

        add(t, EncoderEvent{angle});
    }
}

void InputSequence::add_touch_gesture(double touch_time, double release_time)
{
    add(touch_time, TouchEvent{true});
    add(release_time, TouchEvent{false});
}

void InputSequence::finalize()
{
    std::sort(events_.begin(), events_.end(),
              [](const TimedEvent& a, const TimedEvent& b) {
                  return a.time < b.time;
              });
    finalized_ = true;
    current_index_ = 0;
}

std::vector<TimedEvent> InputSequence::events_in_range(double start, double end) const
{
    std::vector<TimedEvent> result;
    for (const auto& ev : events_) {
        if (ev.time >= start && ev.time < end) {
            result.push_back(ev);
        }
    }
    return result;
}

std::vector<TimedEvent> InputSequence::get_events_until(double time)
{
    if (!finalized_) finalize();

    std::vector<TimedEvent> result;
    while (current_index_ < events_.size() && events_[current_index_].time <= time) {
        result.push_back(events_[current_index_]);
        ++current_index_;
    }
    return result;
}

// TestInputProvider implementation

TestInputProvider::TestInputProvider(InputSequence* sequence)
    : sequence_(sequence)
{
    button_state_.resize(256 * 256, false);  // Max port/pin combinations
}

bool TestInputProvider::button_pressed(int port, int pin) const
{
    size_t index = (static_cast<size_t>(port) << 8) | static_cast<size_t>(pin);
    if (index < button_state_.size()) {
        return button_state_[index];
    }
    return false;
}

int TestInputProvider::adc_value(int channel) const
{
    if (channel >= 0 && channel < 4) {
        return adc_values_[channel];
    }
    return 512;
}

void TestInputProvider::update(double dt)
{
    current_time_ += dt;

    auto events = sequence_->get_events_until(current_time_);
    for (const auto& ev : events) {
        apply_event(ev.event);
    }
}

void TestInputProvider::set_time(double time)
{
    // Get all events up to the new time
    if (time < current_time_) {
        // Going backwards - reset and replay
        sequence_->reset();
        current_time_ = 0.0;

        // Reset state
        encoder_angle_ = 0;
        cap_touched_ = false;
        shifted_ = false;
        pitch_mode_ = 0;
        std::fill(button_state_.begin(), button_state_.end(), false);
        for (int i = 0; i < 4; ++i) adc_values_[i] = 512;
    }

    current_time_ = time;
    auto events = sequence_->get_events_until(time);
    for (const auto& ev : events) {
        apply_event(ev.event);
    }
}

void TestInputProvider::apply_event(const InputEvent& event)
{
    std::visit([this](auto&& ev) {
        using T = std::decay_t<decltype(ev)>;

        if constexpr (std::is_same_v<T, EncoderEvent>) {
            encoder_angle_ = ev.angle;
        }
        else if constexpr (std::is_same_v<T, TouchEvent>) {
            cap_touched_ = ev.touched;
        }
        else if constexpr (std::is_same_v<T, ButtonEvent>) {
            size_t index = (static_cast<size_t>(ev.port) << 8) | static_cast<size_t>(ev.pin);
            if (index < button_state_.size()) {
                button_state_[index] = ev.pressed;
            }
        }
        else if constexpr (std::is_same_v<T, AdcEvent>) {
            if (ev.channel >= 0 && ev.channel < 4) {
                adc_values_[ev.channel] = ev.value;
            }
        }
        else if constexpr (std::is_same_v<T, MidiEvent>) {
            // MIDI events would be dispatched to MIDI handler
            // For now, just track them
        }
        else if constexpr (std::is_same_v<T, ShiftEvent>) {
            shifted_ = ev.shifted;
        }
        else if constexpr (std::is_same_v<T, PitchModeEvent>) {
            pitch_mode_ = ev.mode;
        }
    }, event);
}

} // namespace test
} // namespace sc
