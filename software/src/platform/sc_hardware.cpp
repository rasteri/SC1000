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

// SC1000 Hardware Implementation
// Handles A13 GPIO, PIC ADC/buttons, AS5601 rotary encoder

#include "sc_hardware.h"
#include "platform.h"
#include "../core/sc1000.h"
#include "../core/sc_settings.h"
#include "../control/actions.h"
#include "../control/mapping_registry.h"
#include "../engine/audio_engine.h"
#include "../player/track.h"
#include "../util/log.h"

#include <cmath>
#include <ctime>
#include <unordered_map>

namespace sc {
namespace platform {

using sc::control::dispatch_event;
using sc::control::ButtonState;

// Button state machine states (for PIC button processing)
enum class ButtonMachineState : uint8_t {
    None = 0,           // No buttons pressed
    Pressing = 1,       // At least one button pressed
    ActingInstant = 2,  // Act on instantaneous press
    ActingHeld = 3,     // Act on held buttons
    Waiting = 4         // Cooldown after action
};

//
// SC1000Hardware - Implementation of HardwareInput for SC1000/SC500
//
class SC1000Hardware : public HardwareInput {
public:
    bool init(Sc1000* engine) override;
    void poll(Sc1000* engine) override;
    void log_stats(Sc1000* engine) override;

private:
    // Platform hardware (GPIO, encoder, PIC)
    HardwareState hw_;

    // Button runtime state (separate from Mapping config)
    std::unordered_map<size_t, ButtonState> button_states_;

    // Shift key state
    bool shift_latched_ = false;

    // First-time initialization flag
    bool first_time_ = true;

    // Pitch mode transition tracking
    int old_pitch_mode_ = 0;

    // PIC readings cache
    PicReadings pic_readings_ = {};
    unsigned char total_buttons_[4] = {0, 0, 0, 0};
    ButtonMachineState button_machine_state_ = ButtonMachineState::None;
    unsigned int button_counter_ = 0;

    // Fader state
    bool fader_open1_ = false;
    bool fader_open2_ = false;

    // Rotary sensor filtering
    unsigned int num_blips_ = 0;

    // Polling rate control
    unsigned char pic_skip_counter_ = 0;

    // Internal methods
    void init_gpio(Sc1000* engine);
    void detect_sc500(ScSettings* settings);
    void process_gpio_buttons(Sc1000* engine);
    void process_pic_inputs(Sc1000* engine);
    void process_encoder(Sc1000* engine);
};

//
// Factory function
//
std::unique_ptr<HardwareInput> create_hardware()
{
    return std::make_unique<SC1000Hardware>();
}

//
// SC1000Hardware implementation
//

bool SC1000Hardware::init(Sc1000* engine)
{
    ScSettings* settings = engine->settings.get();

    // Initialize encoder (rotary sensor on I2C0)
    if (!encoder_init(&hw_.encoder))
    {
        LOG_WARN("Couldn't init rotary sensor");
    }
    else
    {
        LOG_INFO("Encoder initialized OK, present=%d", hw_.encoder.present);
    }

    // Initialize PIC input processor on I2C2
    if (!pic_init(&hw_.pic))
    {
        LOG_WARN("Couldn't init input processor");
    }
    else
    {
        LOG_INFO("PIC initialized OK, present=%d", hw_.pic.present);
    }

    // Initialize GPIO based on mappings
    init_gpio(engine);

    // Print settings for debugging
    LOG_INFO("Settings: platter_enabled=%d, platter_speed=%d, jog_reverse=%d",
           settings->platter_enabled, settings->platter_speed, settings->jog_reverse);

    // Initialize crossfader calibration
    engine->crossfader.set_calibration(
        settings->crossfader_adc_min,
        settings->crossfader_adc_max);

    // Detect SC500 variant
    detect_sc500(settings);

    // Return true if we have at least some hardware
    return hw_.pic.present || hw_.encoder.present || hw_.gpio.mmap_present;
}

void SC1000Hardware::poll(Sc1000* engine)
{
    if (hw_.pic.present)
    {
        // PIC polling is rate-limited (every 5th call)
        pic_skip_counter_++;
        if (pic_skip_counter_ > 4)
        {
            pic_skip_counter_ = 0;
            process_pic_inputs(engine);
            first_time_ = false;
        }

        process_encoder(engine);
    }
    else
    {
        // No PIC - provide fallback behavior for desktop/testing
        engine->scratch_deck.player.input.touched = true;
        engine->beat_deck.player.input.crossfader = 0.0;
        engine->scratch_deck.player.input.crossfader = 0.5;
        engine->beat_deck.player.input.just_play = true;
        engine->beat_deck.player.input.reset_pitch();

        // Advance scratch deck position based on time (simulated playback)
        static double last_input_time = 0;
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        double input_time = static_cast<double>(ts.tv_sec) + (static_cast<double>(ts.tv_nsec) / 1000000000.0);

        if (last_input_time != 0)
        {
            engine->scratch_deck.player.input.target_position += (input_time - last_input_time);
        }
        last_input_time = input_time;

        // Still process GPIO buttons even without PIC
        process_gpio_buttons(engine);
    }
}

void SC1000Hardware::log_stats(Sc1000* engine)
{
    // Get DSP stats
    struct DspStats dsp;
    audio_engine_get_stats(&dsp);

    LOG_STATS(
        "ADCS: %04u, %04u, %04u, %04u | XF: %.2f | "
        "DSP: %.1f%% (peak: %.1f%%, %.0fus/%.0fus, xruns: %lu) | "
        "Enc: %04d Cap: %d Buttons: %01u,%01u,%01u,%01u\n",
        pic_readings_.adc[0], pic_readings_.adc[1], pic_readings_.adc[2], pic_readings_.adc[3],
        engine->crossfader.position(),
        dsp.load_percent, dsp.load_peak, dsp.process_time_us, dsp.budget_time_us, dsp.xruns,
        engine->scratch_deck.encoder_state.angle,
        engine->scratch_deck.player.input.touched,
        pic_readings_.buttons[0], pic_readings_.buttons[1],
        pic_readings_.buttons[2], pic_readings_.buttons[3]);
}

void SC1000Hardware::init_gpio(Sc1000* engine)
{
    struct Mapping* map;

    // Initialize MCP23017 GPIO expander
    gpio_init_mcp23017(&hw_.gpio);

    // Configure MCP23017 pins based on mappings
    if (hw_.gpio.mcp23017_present)
    {
        for (int i = 0; i < 16; i++)
        {
            map = engine->mappings.find_gpio(0, static_cast<uint8_t>(i), EventType::BUTTON_PRESSED);

            // If pin is marked as ground, set as output
            if (map != nullptr && map->action_type == GND)
            {
                gpio_mcp23017_set_direction(&hw_.gpio, static_cast<uint8_t>(i), false);
            }
            else
            {
                gpio_mcp23017_set_direction(&hw_.gpio, static_cast<uint8_t>(i), true);
            }

            // Configure pullup
            bool pullup = (map == nullptr || map->pullup);
            gpio_mcp23017_set_pullup(&hw_.gpio, static_cast<uint8_t>(i), pullup);
            if (pullup) LOG_DEBUG("Pulling up pin %d", i);
        }
    }

    // Initialize A13 memory-mapped GPIO
    gpio_init_a13_mmap(&hw_.gpio);

    // Configure A13 GPIO pins based on mappings
    if (hw_.gpio.mmap_present)
    {
        for (int j = 1; j <= 6; j++)
        {
            for (int i = 0; i < 28; i++)
            {
                map = engine->mappings.find_gpio(static_cast<uint8_t>(j), static_cast<uint8_t>(i), EventType::BUTTON_PRESSED);

                if (map != nullptr)
                {
                    // dirty hack, don't map J7 SCL/SDA pins if MCP is present
                    if (hw_.gpio.mcp23017_present && j == 1 && (i == 15 || i == 16))
                    {
                        map->action_type = NOTHING;
                    }
                    else
                    {
                        gpio_a13_configure_input(&hw_.gpio, static_cast<uint8_t>(j),
                                                 static_cast<uint8_t>(i), map->pullup);
                    }
                }
            }
        }
    }
}

void SC1000Hardware::detect_sc500(ScSettings* settings)
{
    // Detect SC500 by seeing if G11 is pulled low
    if (hw_.gpio.mmap_present)
    {
        if (gpio_a13_read_pin(&hw_.gpio, 6, 11))
        {
            LOG_INFO("SC500 detected");
            settings->disable_volume_adc = true;
            settings->disable_pic_buttons = true;
        }
    }
}

void SC1000Hardware::process_gpio_buttons(Sc1000* engine)
{
    ScSettings* settings = engine->settings.get();

    // Read all MCP23017 pins at once (already inverted by platform layer)
    uint16_t mcp_pins = 0;
    if (hw_.gpio.mcp23017_present)
    {
        mcp_pins = gpio_mcp23017_read_all(&hw_.gpio);
    }

    // Capture shifted state ONCE before processing any mappings
    bool shifted_at_start = engine->input_state.is_shifted();

    // Use index-based iteration to associate ButtonState with each Mapping
    auto& mappings = engine->mappings.all();
    for (size_t idx = 0; idx < mappings.size(); ++idx)
    {
        const Mapping& m = mappings[idx];
        ButtonState& bs = button_states_[idx];

        // Only digital pins
        if (m.type == IO && (!(m.gpio_port == 0 && !hw_.gpio.mcp23017_present)))
        {
            bool pin_value = false;
            if (m.gpio_port == 0 && hw_.gpio.mcp23017_present)
            {
                pin_value = (mcp_pins >> m.pin) & 0x01;
            }
            else if (hw_.gpio.mmap_present)
            {
                pin_value = gpio_a13_read_pin(&hw_.gpio, m.gpio_port, m.pin);
            }

            // Button not pressed, check for button
            if (bs.debounce == 0)
            {
                if (pin_value)
                {
                    if (m.action_type == RECORD || m.action_type == LOOPERASE)
                    {
                        LOG_DEBUG("Button port=%d pin=%d pressed, shifted=%d, edge_type=%d, action=%d",
                                  m.gpio_port, m.pin, shifted_at_start, m.edge_type, m.action_type);
                    }
                    LOG_DEBUG("Button %d pressed", m.pin);

                    if (first_time_ && m.deck_no == 1 && (m.action_type == VOLUP || m.action_type == VOLDOWN))
                    {
                        engine->beat_deck.player.set_track(
                            track_acquire_by_import(engine->beat_deck.importer.c_str(), "/var/os-version.mp3"));
                        engine->beat_deck.cues.load_from_file(engine->beat_deck.player.track->path);
                        engine->scratch_deck.player.input.volume_knob = 0.0;
                    }
                    else
                    {
                        bs.shifted_at_press = shifted_at_start;

                        if (m.action_type == NEXTFILE || m.action_type == PREVFILE ||
                            m.action_type == RANDOMFILE || m.action_type == JOGPIT)
                        {
                            LOG_DEBUG("Checking Mapping port=%d pin=%d action=%d edge=%d shifted=%d",
                                      m.gpio_port, m.pin, m.action_type, m.edge_type, shifted_at_start);
                        }

                        if ((!shifted_at_start && m.edge_type == BUTTON_PRESSED) ||
                            (shifted_at_start && m.edge_type == BUTTON_PRESSED_SHIFTED))
                        {
                            if (m.action_type == NEXTFILE || m.action_type == PREVFILE ||
                                m.action_type == RANDOMFILE || m.action_type == JOGPIT)
                            {
                                LOG_DEBUG("FIRING action=%d for port=%d pin=%d deck=%d",
                                          m.action_type, m.gpio_port, m.pin, m.deck_no);
                            }
                            dispatch_event(&m, nullptr, engine, settings, engine->input_state);
                        }

                        bs.debounce++;
                    }
                }
            }
            else if (bs.debounce > 0 && bs.debounce < settings->debounce_time)
            {
                bs.debounce++;
            }
            else if (bs.debounce >= settings->debounce_time && bs.debounce < settings->hold_time)
            {
                if (!pin_value)
                {
                    LOG_DEBUG("Button %d released", m.pin);
                    if ((!bs.shifted_at_press && m.edge_type == BUTTON_RELEASED) ||
                        (bs.shifted_at_press && m.edge_type == BUTTON_RELEASED_SHIFTED))
                        dispatch_event(&m, nullptr, engine, settings, engine->input_state);
                    bs.debounce = -settings->debounce_time;
                }
                else
                {
                    bs.debounce++;
                }
            }
            else if (bs.debounce == settings->hold_time)
            {
                LOG_DEBUG("Button port=%d pin=%d HELD, shifted_at_press=%d, edge_type=%d, action=%d",
                          m.gpio_port, m.pin, bs.shifted_at_press, m.edge_type, m.action_type);
                if ((!bs.shifted_at_press && m.edge_type == BUTTON_HOLDING) ||
                    (bs.shifted_at_press && m.edge_type == BUTTON_HOLDING_SHIFTED))
                {
                    LOG_DEBUG("Triggering held action for port=%d pin=%d action=%d",
                              m.gpio_port, m.pin, m.action_type);
                    dispatch_event(&m, nullptr, engine, settings, engine->input_state);
                }
                bs.debounce++;
            }
            else if (bs.debounce > settings->hold_time)
            {
                if (pin_value)
                {
                    if (m.action_type == VOLUHOLD || m.action_type == VOLDHOLD)
                    {
                        if ((!bs.shifted_at_press && m.edge_type == BUTTON_HOLDING) ||
                            (bs.shifted_at_press && m.edge_type == BUTTON_HOLDING_SHIFTED))
                            dispatch_event(&m, nullptr, engine, settings, engine->input_state);
                    }
                }
                else
                {
                    LOG_DEBUG("Button %d released", m.pin);
                    if (m.edge_type == BUTTON_RELEASED && !bs.shifted_at_press)
                        dispatch_event(&m, nullptr, engine, settings, engine->input_state);
                    bs.debounce = -settings->debounce_time;
                }
            }
            else if (bs.debounce < 0)
            {
                bs.debounce++;
            }
        }
    }
}

void SC1000Hardware::process_pic_inputs(Sc1000* engine)
{
    ScSettings* settings = engine->settings.get();

    unsigned int i;
    unsigned int fader_cut_point1, fader_cut_point2;
    double fadertarget0, fadertarget1;

    // Read all PIC inputs
    pic_readings_ = pic_read_all(&hw_.pic);

    // Process GPIO buttons
    process_gpio_buttons(engine);

    // Apply volume and fader from ADC values
    if (!settings->disable_volume_adc)
    {
        // SC1000: Volume pots control crossfader level directly
        fadertarget0 = static_cast<double>(pic_readings_.adc[2]) / 1024.0;
        fadertarget1 = static_cast<double>(pic_readings_.adc[3]) / 1024.0;
    }
    else
    {
        // SC500: No volume pots, volume_knob is controlled via buttons.
        // Crossfader should be 1.0 (fully open) unless cut by crossfader position.
        // Don't use volume_knob here - it's already applied in audio engine!
        fadertarget0 = 1.0;
        fadertarget1 = 1.0;
    }

    // Fader Hysteresis
    fader_cut_point1 = static_cast<unsigned int>(fader_open1_ ? settings->fader_close_point : settings->fader_open_point);
    fader_cut_point2 = static_cast<unsigned int>(fader_open2_ ? settings->fader_close_point : settings->fader_open_point);

    fader_open1_ = true;
    fader_open2_ = true;

    if (pic_readings_.adc[0] < fader_cut_point1)
    {
        if (settings->cut_beats == 1) fadertarget0 = 0.0;
        else fadertarget1 = 0.0;
        fader_open1_ = false;
    }
    if (pic_readings_.adc[1] < fader_cut_point2)
    {
        if (settings->cut_beats == 2) fadertarget0 = 0.0;
        else fadertarget1 = 0.0;
        fader_open2_ = false;
    }

    engine->beat_deck.player.input.crossfader = fadertarget0;
    engine->scratch_deck.player.input.crossfader = fadertarget1;

    // Update crossfader from ADC
    engine->crossfader.update(pic_readings_.adc[0]);

    if (!settings->disable_pic_buttons)
    {
        auto& buttons = pic_readings_.buttons;
        auto& total_buttons = total_buttons_;

        switch (button_machine_state_)
        {
        case ButtonMachineState::None:
            if (buttons[0] || buttons[1] || buttons[2] || buttons[3])
            {
                button_machine_state_ = ButtonMachineState::Pressing;

                if (first_time_)
                {
                    engine->beat_deck.player.set_track(
                        track_acquire_by_import(engine->beat_deck.importer.c_str(), "/var/os-version.mp3"));
                    engine->beat_deck.cues.load_from_file(engine->beat_deck.player.track->path);
                    button_machine_state_ = ButtonMachineState::Waiting;
                }
            }
            break;

        case ButtonMachineState::Pressing:
            for (i = 0; i < 4; i++)
                total_buttons[i] |= buttons[i];

            if (!(buttons[0] || buttons[1] || buttons[2] || buttons[3]))
                button_machine_state_ = ButtonMachineState::ActingInstant;

            button_counter_++;
            if (button_counter_ > static_cast<unsigned int>(settings->hold_time))
            {
                button_counter_ = 0;
                button_machine_state_ = ButtonMachineState::ActingHeld;
            }
            break;

        case ButtonMachineState::ActingInstant:
            if (engine->input_state.pitch_mode())
            {
                engine->input_state.set_pitch_mode(0);
                old_pitch_mode_ = 0;
                LOG_DEBUG("Pitch mode Disabled");
            }
            else if (total_buttons[0] && !total_buttons[1] && !total_buttons[2] && !total_buttons[3] &&
                     engine->scratch_deck.nav_state.files_present)
                engine->scratch_deck.prev_file(engine, settings);
            else if (!total_buttons[0] && total_buttons[1] && !total_buttons[2] && !total_buttons[3] &&
                     engine->scratch_deck.nav_state.files_present)
                engine->scratch_deck.next_file(engine, settings);
            else if (total_buttons[0] && total_buttons[1] && !total_buttons[2] && !total_buttons[3] &&
                     engine->scratch_deck.nav_state.files_present)
                engine->input_state.set_pitch_mode(2);
            else if (!total_buttons[0] && !total_buttons[1] && total_buttons[2] && !total_buttons[3] &&
                     engine->beat_deck.nav_state.files_present)
                engine->beat_deck.prev_file(engine, settings);
            else if (!total_buttons[0] && !total_buttons[1] && !total_buttons[2] && total_buttons[3] &&
                     engine->beat_deck.nav_state.files_present)
                engine->beat_deck.next_file(engine, settings);
            else if (!total_buttons[0] && !total_buttons[1] && total_buttons[2] && total_buttons[3] &&
                     engine->beat_deck.nav_state.files_present)
                engine->input_state.set_pitch_mode(1);
            else if (total_buttons[0] && total_buttons[1] && total_buttons[2] && total_buttons[3])
                shift_latched_ = true;
            else
                LOG_WARN("Unknown action");

            button_machine_state_ = ButtonMachineState::Waiting;
            break;

        case ButtonMachineState::ActingHeld:
            if (buttons[0] && !buttons[1] && !buttons[2] && !buttons[3] &&
                engine->scratch_deck.nav_state.files_present)
                engine->scratch_deck.prev_folder(engine, settings);
            else if (!buttons[0] && buttons[1] && !buttons[2] && !buttons[3] &&
                     engine->scratch_deck.nav_state.files_present)
                engine->scratch_deck.next_folder(engine, settings);
            else if (buttons[0] && buttons[1] && !buttons[2] && !buttons[3] &&
                     engine->scratch_deck.nav_state.files_present)
                engine->scratch_deck.random_file(engine, settings);
            else if (!buttons[0] && !buttons[1] && buttons[2] && !buttons[3] &&
                     engine->beat_deck.nav_state.files_present)
                engine->beat_deck.prev_folder(engine, settings);
            else if (!buttons[0] && !buttons[1] && !buttons[2] && buttons[3] &&
                     engine->beat_deck.nav_state.files_present)
                engine->beat_deck.next_folder(engine, settings);
            else if (!buttons[0] && !buttons[1] && buttons[2] && buttons[3] &&
                     engine->beat_deck.nav_state.files_present)
                engine->beat_deck.random_file(engine, settings);
            else if (buttons[0] && buttons[1] && buttons[2] && buttons[3])
            {
                LOG_DEBUG("All buttons held!");
                if (engine->scratch_deck.nav_state.files_present)
                    engine->beat_deck.record(engine);
            }
            else
                LOG_WARN("Unknown action");

            button_machine_state_ = ButtonMachineState::Waiting;
            break;

        case ButtonMachineState::Waiting:
            button_counter_++;

            if (buttons[0] || buttons[1] || buttons[2] || buttons[3])
                button_counter_ = 0;

            if (button_counter_ > 20)
            {
                button_counter_ = 0;
                button_machine_state_ = ButtonMachineState::None;

                for (i = 0; i < 4; i++)
                    total_buttons[i] = 0;
            }
            break;
        }
    }
}

void SC1000Hardware::process_encoder(Sc1000* engine)
{
    ScSettings* settings = engine->settings.get();

    int8_t crossed_zero;
    int wrapped_angle = 0x0000;

    // Read encoder angle
    engine->scratch_deck.encoder_state.angle_raw = encoder_read_angle(&hw_.encoder);

    if (settings->jog_reverse)
    {
        engine->scratch_deck.encoder_state.angle_raw = 4095 - engine->scratch_deck.encoder_state.angle_raw;
    }

    // First time, make sure there's no difference
    if (engine->scratch_deck.encoder_state.angle == 0xffff)
        engine->scratch_deck.encoder_state.angle = engine->scratch_deck.encoder_state.angle_raw;

    // Handle wrapping at zero
    if (engine->scratch_deck.encoder_state.angle_raw < 1024 &&
        engine->scratch_deck.encoder_state.angle >= 3072)
    {
        crossed_zero = 1;
        wrapped_angle = engine->scratch_deck.encoder_state.angle - 4096;
    }
    else if (engine->scratch_deck.encoder_state.angle_raw >= 3072 &&
             engine->scratch_deck.encoder_state.angle < 1024)
    {
        crossed_zero = -1;
        wrapped_angle = engine->scratch_deck.encoder_state.angle + 4096;
    }
    else
    {
        crossed_zero = 0;
        wrapped_angle = engine->scratch_deck.encoder_state.angle;
    }

    // Blip filter
    if (abs(engine->scratch_deck.encoder_state.angle_raw - wrapped_angle) > 100 && num_blips_ < 2)
    {
        num_blips_++;
    }
    else
    {
        num_blips_ = 0;
        engine->scratch_deck.encoder_state.angle = engine->scratch_deck.encoder_state.angle_raw;

        int current_pitch_mode = engine->input_state.pitch_mode();
        if (current_pitch_mode)
        {
            if (!old_pitch_mode_)
            {
                if (current_pitch_mode == 0)
                {
                    engine->beat_deck.player.input.pitch_note = 1.0;
                }
                else
                {
                    engine->scratch_deck.player.input.pitch_note = 1.0;
                }

                engine->scratch_deck.encoder_state.offset = -engine->scratch_deck.encoder_state.angle;
                old_pitch_mode_ = 1;
                engine->scratch_deck.player.input.touched = false;
            }

            if (crossed_zero > 0)
            {
                engine->scratch_deck.encoder_state.offset += 4096;
            }
            else if (crossed_zero < 0)
            {
                engine->scratch_deck.encoder_state.offset -= 4096;
            }

            double pitch_offset = static_cast<double>(engine->scratch_deck.encoder_state.angle +
                engine->scratch_deck.encoder_state.offset) / 16384.0 + 1.0;

            if (current_pitch_mode == 0)
            {
                engine->scratch_deck.player.input.pitch_note = pitch_offset;
            }
            else
            {
                engine->scratch_deck.player.input.pitch_note = pitch_offset;
            }
        }
        else
        {
            if (settings->platter_enabled)
            {
                double scratch_pos = engine->audio ? engine->audio->get_position(1) : 0.0;
                double scratch_motor = engine->audio ? engine->audio->get_deck_state(1).motor_speed : 1.0;

                if (pic_readings_.cap_touched || scratch_motor == 0.0)
                {
                    if (!engine->scratch_deck.player.input.touched ||
                        (old_pitch_mode_ && !engine->scratch_deck.player.input.stopped))
                    {
                        engine->scratch_deck.encoder_state.offset = static_cast<int32_t>(
                            (scratch_pos * settings->platter_speed) -
                            engine->scratch_deck.encoder_state.angle);

                        LOG_DEBUG("touch!");
                        engine->scratch_deck.player.input.target_position = scratch_pos;
                        engine->scratch_deck.player.input.touched = true;
                    }
                }
                else
                {
                    engine->scratch_deck.player.input.touched = false;
                }
            }
            else
            {
                engine->scratch_deck.player.input.touched = true;
            }

            if (crossed_zero > 0)
            {
                engine->scratch_deck.encoder_state.offset += 4096;
            }
            else if (crossed_zero < 0)
            {
                engine->scratch_deck.encoder_state.offset -= 4096;
            }

            engine->scratch_deck.player.input.target_position =
                static_cast<double>(engine->scratch_deck.encoder_state.angle +
                    engine->scratch_deck.encoder_state.offset) / settings->platter_speed;
        }
        old_pitch_mode_ = engine->input_state.pitch_mode();
    }
}

} // namespace platform
} // namespace sc
