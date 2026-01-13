/*
 * Copyright (C) 2024-2026 Niklas Klügel <lodsb@lodsb.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, as published by the Free Software Foundation.
 */

#pragma once

#include <vector>
#include <variant>
#include <cstdint>

namespace sc {
namespace test {

//
// Input event types for test sequences
//

// Encoder/platter position event
struct EncoderEvent {
    int32_t angle;  // 0-4095 for one rotation
};

// Capacitive touch sensor event
struct TouchEvent {
    bool touched;
};

// Button press event (GPIO buttons)
struct ButtonEvent {
    int port;       // GPIO port (0 = MCP23017, 2/6 = A13)
    int pin;        // Pin number
    bool pressed;   // true = pressed, false = released
};

// ADC value event (crossfader, volume pots)
struct AdcEvent {
    int channel;    // ADC channel (0-3)
    int value;      // 0-1023
};

// MIDI event
struct MidiEvent {
    uint8_t status;
    uint8_t data1;
    uint8_t data2;
};

// Shift key event
struct ShiftEvent {
    bool shifted;
};

// Pitch mode event
struct PitchModeEvent {
    int mode;  // 0=off, 1=beat deck, 2=scratch deck
};

// Tagged union for all event types
using InputEvent = std::variant<
    EncoderEvent,
    TouchEvent,
    ButtonEvent,
    AdcEvent,
    MidiEvent,
    ShiftEvent,
    PitchModeEvent
>;

// Timestamped event
struct TimedEvent {
    double time;      // Time in seconds from start
    InputEvent event;
};

//
// InputSequence - Scripted sequence of input events
//
// Events are sorted by time and can be applied to the engine
// at the appropriate points during rendering.
//
class InputSequence {
public:
    InputSequence() = default;

    // Add events at specific times
    void add(double time, EncoderEvent event) { add_event(time, event); }
    void add(double time, TouchEvent event) { add_event(time, event); }
    void add(double time, ButtonEvent event) { add_event(time, event); }
    void add(double time, AdcEvent event) { add_event(time, event); }
    void add(double time, MidiEvent event) { add_event(time, event); }
    void add(double time, ShiftEvent event) { add_event(time, event); }
    void add(double time, PitchModeEvent event) { add_event(time, event); }

    // Helper: add encoder ramp (smooth rotation from start to end angle over duration)
    // Set wrap_around=true for circular encoder simulation (takes shorter path around 0-4095)
    void add_encoder_ramp(double start_time, double duration, int32_t start_angle, int32_t end_angle, int steps = 100, bool wrap_around = false);

    // Helper: add touch and release
    void add_touch_gesture(double touch_time, double release_time);

    // Sort events by time (call before playback)
    void finalize();

    // Get events in time range [start, end)
    std::vector<TimedEvent> events_in_range(double start, double end) const;

    // Get all events
    const std::vector<TimedEvent>& events() const { return events_; }

    // Clear all events
    void clear() { events_.clear(); current_index_ = 0; }

    // Reset playback position
    void reset() { current_index_ = 0; }

    // Get next events up to time (for incremental playback)
    std::vector<TimedEvent> get_events_until(double time);

private:
    void add_event(double time, InputEvent event);

    std::vector<TimedEvent> events_;
    size_t current_index_ = 0;
    bool finalized_ = false;
};

//
// InputProvider - Interface for input hardware abstraction
//
// Implemented by real hardware (HardwareInputProvider) and
// test sequences (TestInputProvider) for deterministic testing.
//
class InputProvider {
public:
    virtual ~InputProvider() = default;

    // Read current encoder angle (0-4095)
    virtual int32_t encoder_angle() const = 0;

    // Read capacitive touch state
    virtual bool cap_touched() const = 0;

    // Read button state
    virtual bool button_pressed(int port, int pin) const = 0;

    // Read ADC value (0-1023)
    virtual int adc_value(int channel) const = 0;

    // Check if shift is active
    virtual bool is_shifted() const = 0;

    // Get pitch mode
    virtual int pitch_mode() const = 0;

    // Update state (for test provider, advances through sequence)
    virtual void update(double dt) = 0;
};

//
// TestInputProvider - Plays back an InputSequence
//
class TestInputProvider : public InputProvider {
public:
    explicit TestInputProvider(InputSequence* sequence);

    int32_t encoder_angle() const override { return encoder_angle_; }
    bool cap_touched() const override { return cap_touched_; }
    bool button_pressed(int port, int pin) const override;
    int adc_value(int channel) const override;
    bool is_shifted() const override { return shifted_; }
    int pitch_mode() const override { return pitch_mode_; }

    void update(double dt) override;

    // Set current time directly (for synchronous rendering)
    void set_time(double time);

    // Get current time
    double current_time() const { return current_time_; }

private:
    void apply_event(const InputEvent& event);

    InputSequence* sequence_;
    double current_time_ = 0.0;

    // Current state
    int32_t encoder_angle_ = 0;
    bool cap_touched_ = false;
    bool shifted_ = false;
    int pitch_mode_ = 0;

    // Button state: map of (port << 8 | pin) -> pressed
    std::vector<bool> button_state_;

    // ADC values (4 channels)
    int adc_values_[4] = {512, 512, 512, 512};  // Mid-range default
};

} // namespace test
} // namespace sc
