/*
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


// SC1000 Audio Engine - Templated Implementation
//
// Key features:
// - Template parameters for interpolation (Cubic/Sinc) and sample format (S16/S24/S32/Float)
// - Virtual dispatch once per buffer, compile-time optimization inside
// - Backward-compatible C API for legacy code

#include <iostream>
#include <cmath>
#include <climits>
#include <ctime>
#include <cstring>

#include "audio_engine.h"
#include "loop_buffer.h"
#include "../core/sc_settings.h"
#include "../core/sc1000.h"

#include "../player/track.h"
#include "../player/deck.h"

#include "../util/log.h"

namespace sc {
namespace audio {

//
// Timing utilities
//
static inline double get_time_us() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<double>(ts.tv_sec) * 1000000.0 + static_cast<double>(ts.tv_nsec) / 1000.0;
}

//
// Constants
//
constexpr double FADER_DECAY_TIME = 0.020;  // Time in seconds fader takes to decay
constexpr double DECAY_SAMPLES = FADER_DECAY_TIME * 48000;
constexpr double BASE_VOLUME = 7.0 / 8.0;  // Headroom for pitch > 1.0
constexpr double SAMPLE_RATE = 48000.0;

static bool nearly_equal(double val1, double val2, double tolerance) {
    return std::fabs(val1 - val2) < tolerance;
}

//
// Global state for C API backward compatibility
// (defined in namespace but accessed via namespace qualifier from C API)
//
static DspStats g_dsp_stats{};

} // namespace audio
} // namespace sc

// Global interpolation mode at file scope for C API
static interpolation_mode_t g_interpolation_mode = INTERP_SINC;

namespace sc {
namespace audio {

//
// AudioEngine template implementation
//

template<typename InterpPolicy, typename FormatPolicy>
AudioEngine<InterpPolicy, FormatPolicy>::AudioEngine() {
    // Loop buffers are zero-initialized, will be set up by init_loop_buffers()
}

template<typename InterpPolicy, typename FormatPolicy>
AudioEngine<InterpPolicy, FormatPolicy>::~AudioEngine() {
    if (loop_buffers_initialized_) {
        loop_buffer_clear(&loop_[0]);
        loop_buffer_clear(&loop_[1]);
    }
}

template<typename InterpPolicy, typename FormatPolicy>
void AudioEngine<InterpPolicy, FormatPolicy>::init_loop_buffers(int sample_rate, int max_seconds) {
    if (loop_buffers_initialized_) {
        loop_buffer_clear(&loop_[0]);
        loop_buffer_clear(&loop_[1]);
    }
    loop_buffer_init(&loop_[0], sample_rate, max_seconds);
    loop_buffer_init(&loop_[1], sample_rate, max_seconds);
    loop_buffers_initialized_ = true;
}

template<typename InterpPolicy, typename FormatPolicy>
bool AudioEngine<InterpPolicy, FormatPolicy>::start_recording(int deck, double playback_position) {
    if (deck < 0 || deck > 1) return false;
    if (!loop_buffers_initialized_) return false;

    // Only one deck can record at a time
    if (active_recording_deck_ >= 0 && active_recording_deck_ != deck) {
        return false;
    }

    // For punch-in, sync write position to current playback position
    LoopBuffer* lb = &loop_[deck];
    if (loop_buffer_has_loop(lb)) {
        if (playback_position < 0) playback_position = 0;
        auto pos_samples = static_cast<unsigned int>(playback_position * lb->sample_rate);
        loop_buffer_set_position(lb, pos_samples);
    }

    if (loop_buffer_start(lb)) {
        active_recording_deck_ = deck;
        return true;
    }
    return false;
}

template<typename InterpPolicy, typename FormatPolicy>
void AudioEngine<InterpPolicy, FormatPolicy>::stop_recording(int deck) {
    if (deck < 0 || deck > 1) return;
    loop_buffer_stop(&loop_[deck]);
    if (active_recording_deck_ == deck) {
        active_recording_deck_ = -1;
    }
}

template<typename InterpPolicy, typename FormatPolicy>
bool AudioEngine<InterpPolicy, FormatPolicy>::is_recording(int deck) const {
    if (deck < 0 || deck > 1) return false;
    return loop_buffer_is_recording(const_cast<LoopBuffer*>(&loop_[deck]));
}

template<typename InterpPolicy, typename FormatPolicy>
Track* AudioEngine<InterpPolicy, FormatPolicy>::get_loop_track(int deck) {
    if (deck < 0 || deck > 1) return nullptr;
    return loop_buffer_get_track(&loop_[deck]);
}

template<typename InterpPolicy, typename FormatPolicy>
Track* AudioEngine<InterpPolicy, FormatPolicy>::peek_loop_track(int deck) {
    if (deck < 0 || deck > 1) return nullptr;
    return loop_[deck].track;
}

template<typename InterpPolicy, typename FormatPolicy>
bool AudioEngine<InterpPolicy, FormatPolicy>::has_loop(int deck) const {
    if (deck < 0 || deck > 1) return false;
    return loop_buffer_has_loop(const_cast<LoopBuffer*>(&loop_[deck]));
}

template<typename InterpPolicy, typename FormatPolicy>
void AudioEngine<InterpPolicy, FormatPolicy>::reset_loop(int deck) {
    if (deck < 0 || deck > 1) return;
    loop_buffer_reset(&loop_[deck]);
}

//
// Encoder glitch protection chain:
//
// The rotary encoder can produce spurious readings that would cause extreme
// pitch values and corrupt audio playback. Multiple layers prevent this:
//
// 1. Blip filter (sc_input.cpp:664-674)
//    - Ignores encoder jumps > 100 ticks, accepts after 3 consecutive blips
//
// 2. Cap touch reset (deck.cpp: load_track_internal, recall_loop, goto_loop)
//    - Resets cap_touch on track changes to force angle_offset recalculation
//
// 3. Diff sanity check (below, lines ~215-221)
//    - If |position - target_position| > 0.5s, snaps instead of chasing
//
// 4. Pitch clamp in slipmat mode (below, lines ~201-202)
//    - Limits pitch to ±20 when platter is released
//
// 5. Position wrapping with fmod (process loop, lines ~347-354)
//    - Proper modulo wrap handles high pitch on short loops
//
// 6. Interpolation bounds (sinc_interpolate_opt.h:119, 280)
//    - Final safety net with modulo wrap before sample access
//

template<typename InterpPolicy, typename FormatPolicy>
void AudioEngine<InterpPolicy, FormatPolicy>::setup_player(
    struct Player* pl,
    DeckProcessingState* state,
    unsigned long samples,
    const struct ScSettings* settings,
    double track_length_seconds,
    double* target_volume,
    double* filtered_pitch)
{
    // Read from unified input struct
    const sc::DeckInput& in = pl->input;

    double target_pitch;
    auto samples_i = 1.0 / static_cast<double>(samples);

    // === External pitch (MIDI note/bend) ===
    // These transpose the sample directly, like changing the speed on a sampler
    double external_speed = in.external_pitch();

    // Detect significant external pitch changes for instant response
    // Only triggers on actual MIDI note/bend changes, not on play/pause
    bool external_changed = std::fabs(external_speed - state->last_external_speed) > 0.01;
    state->last_external_speed = external_speed;

    // === Motor/platter behavior ===
    if (in.stopped) {
        // Simulate braking: motor decelerates toward 0
        if (state->motor_speed > 0.1) {
            state->motor_speed -= samples_i * (settings->brake_speed * 10);
        } else {
            state->motor_speed = 0.0;
        }
    } else {
        state->motor_speed = external_speed;
    }

    // === Pitch calculation based on mode ===
    if (in.just_play ||  // Platter is always released on beat deck
        (!in.touched && !state->touched_prev))  // Don't do it on first iteration for backspins
    {
        // Platter released: slipmat simulation toward motor_speed
        if (state->pitch > 20.0) state->pitch = 20.0;
        if (state->pitch < -20.0) state->pitch = -20.0;

        // Simulate slipmat
        if (state->pitch < state->motor_speed - 0.1) {
            target_pitch = state->pitch + samples_i * settings->slippiness;
        } else if (state->pitch > state->motor_speed + 0.1) {
            target_pitch = state->pitch - samples_i * settings->slippiness;
        } else {
            target_pitch = state->motor_speed;
        }
    } else {
        // Platter touched: position-based control (user scratching)
        double diff = state->position - in.target_position;

        // Handle track wrap: find shortest path between position and target
        // This prevents infinite looping when position wraps but target doesn't
        if (track_length_seconds > 0.0) {
            double half_length = track_length_seconds / 2.0;
            if (diff > half_length) {
                diff -= track_length_seconds;
            } else if (diff < -half_length) {
                diff += track_length_seconds;
            }
        }

        // Calculate raw target pitch from position error
        double raw_pitch = (-diff) * 40;

        // Clamp to configurable range to prevent wild oscillation
        double max_pitch = settings->max_scratch_pitch;
        if (raw_pitch > max_pitch) {
            target_pitch = max_pitch;
        } else if (raw_pitch < -max_pitch) {
            target_pitch = -max_pitch;
        } else {
            target_pitch = raw_pitch;
        }
    }
    state->touched_prev = in.touched;

    // === Final pitch smoothing ===
    if (external_changed && !in.touched) {
        // Instant response for MIDI note/bend changes when not scratching
        *filtered_pitch = external_speed;
        state->pitch = external_speed;  // Also snap current pitch for immediate effect
    } else {
        // Normal IIR smoothing for all other cases
        *filtered_pitch = (0.1 * target_pitch) + (0.9 * state->pitch);
    }

    // Volume fader decay
    double vol_decay_amount = samples_i * DECAY_SAMPLES;

    if (nearly_equal(in.crossfader, state->fader_current, vol_decay_amount)) {
        state->fader_current = in.crossfader;
    } else if (in.crossfader > state->fader_current) {
        state->fader_current += vol_decay_amount;
    } else {
        state->fader_current -= vol_decay_amount;
    }

    // Apply all volume factors: pitch-based gain, crossfader, volume knob, and max_volume limit
    *target_volume = std::fabs(state->pitch) * BASE_VOLUME * state->fader_current * in.volume_knob;
    double max_vol = settings->max_volume;
    if (*target_volume > max_vol) *target_volume = max_vol;


    // Diagnostic: detect prolonged low-volume conditions
    static int dbg_count = 0;
    static int low_vol_count = 0;

    // Track consecutive frames with near-zero pitch/volume
    if (std::fabs(state->pitch) < 0.05 || *target_volume < 0.01) {
        low_vol_count++;
        // Log after ~0.5 seconds of silence (at 48kHz/256 samples = 187 buffers/sec)
        if (low_vol_count == 100) {
            LOG_INFO("DIAG: prolonged low volume - pitch=%.3f motor=%.3f stopped=%d touched=%d ext_speed=%.3f vol_knob=%.2f fader=%.2f",
                     state->pitch, state->motor_speed, in.stopped, in.touched, external_speed,
                     in.volume_knob, state->fader_current);
        }
    } else {
        low_vol_count = 0;
    }

    if (++dbg_count % 1000 == 0) {
        LOG_DEBUG("vol: pitch=%.2f knob=%.2f fader_cur=%.2f fader_tgt=%.2f target=%.2f",
                  state->pitch, in.volume_knob,
                  state->fader_current,
                  in.crossfader, *target_volume);
    }

}

template<typename InterpPolicy, typename FormatPolicy>
void AudioEngine<InterpPolicy, FormatPolicy>::process_players(
    Sc1000* engine,
    AudioCapture* capture,
    void* playback,
    int channels,
    unsigned long frames)
{
    struct Player* pl1 = &engine->beat_deck.player;
    struct Player* pl2 = &engine->scratch_deck.player;

    DeckProcessingState* state1 = &deck_state_[0];
    DeckProcessingState* state2 = &deck_state_[1];

    // Read input structs
    sc::DeckInput& in1 = pl1->input;
    sc::DeckInput& in2 = pl2->input;

    // Handle seek requests (from cue jumps, track loads, etc.)
    if (in1.seek_to >= 0.0) {
        state1->position = in1.seek_to;
        state1->position_offset = in1.position_offset;
        in1.seek_to = -1.0;  // Clear request
    }
    if (in2.seek_to >= 0.0) {
        state2->position = in2.seek_to;
        state2->position_offset = in2.position_offset;
        in2.seek_to = -1.0;  // Clear request
    }

    // Select track for each player based on source (needed for setup_player)
    bool use_loop_1 = (in1.source == sc::PlaybackSource::Loop) && has_loop(0);
    bool use_loop_2 = (in2.source == sc::PlaybackSource::Loop) && has_loop(1);
    Track* tr1 = use_loop_1 ? peek_loop_track(0) : pl1->track;
    Track* tr2 = use_loop_2 ? peek_loop_track(1) : pl2->track;

    const int tr_1_len = static_cast<int>(tr1->length);
    const int tr_2_len = static_cast<int>(tr2->length);
    const double tr_1_rate = tr1->rate;
    const double tr_2_rate = tr2->rate;

    // Calculate track length in seconds for position wrap handling
    double track_1_seconds = (tr_1_len > 0 && tr_1_rate > 0) ? tr_1_len / tr_1_rate : 0.0;
    double track_2_seconds = (tr_2_len > 0 && tr_2_rate > 0) ? tr_2_len / tr_2_rate : 0.0;

    double target_volume_1, filtered_pitch_1;
    double target_volume_2, filtered_pitch_2;

    setup_player(pl1, state1, frames, engine->settings.get(), track_1_seconds, &target_volume_1, &filtered_pitch_1);
    setup_player(pl2, state2, frames, engine->settings.get(), track_2_seconds, &target_volume_2, &filtered_pitch_2);

    // During fresh recording (recording active but no loop yet), mute track playback
    if (state1->is_recording && !state1->has_loop) target_volume_1 = 0.0;
    if (state2->is_recording && !state2->has_loop) target_volume_2 = 0.0;

    const double dt_rate_1 = pl1->sample_dt * tr_1_rate;
    const double dt_rate_2 = pl2->sample_dt * tr_2_rate;

    double sample_1 = (state1->position - state1->position_offset) * tr_1_rate;
    double sample_2 = (state2->position - state2->position_offset) * tr_2_rate;

    // Wrap sample positions once per buffer (avoids fmod per-sample in interpolation)
    if (tr_1_len > 0) {
        sample_1 = std::fmod(sample_1, static_cast<double>(tr_1_len));
        if (sample_1 < 0.0) sample_1 += tr_1_len;
    }
    if (tr_2_len > 0) {
        sample_2 = std::fmod(sample_2, static_cast<double>(tr_2_len));
        if (sample_2 < 0.0) sample_2 += tr_2_len;
    }

    const float ONE_OVER_SAMPLES = 1.0f / static_cast<float>(frames);

    float pitch_1 = static_cast<float>(state1->pitch);
    float pitch_2 = static_cast<float>(state2->pitch);
    float vol_1 = static_cast<float>(state1->volume);
    float vol_2 = static_cast<float>(state2->volume);

    const float volume_gradient_1 = (static_cast<float>(target_volume_1) - vol_1) * ONE_OVER_SAMPLES;
    const float pitch_gradient_1 = (static_cast<float>(filtered_pitch_1) - pitch_1) * ONE_OVER_SAMPLES;
    const float volume_gradient_2 = (static_cast<float>(target_volume_2) - vol_2) * ONE_OVER_SAMPLES;
    const float pitch_gradient_2 = (static_cast<float>(filtered_pitch_2) - pitch_2) * ONE_OVER_SAMPLES;

    // Output pointer - advance by bytes_per_sample * channels
    auto* out_ptr = static_cast<uint8_t*>(playback);
    constexpr int bytes_per_sample = FormatPolicy::bytes_per_sample;
    const int frame_size = bytes_per_sample * channels;

    double r1 = 0.0, r2 = 0.0;

    if (spin_try_lock(&pl1->lock) && spin_try_lock(&pl2->lock)) {
        // Main processing loop - all compile-time optimized
        for (unsigned long s = 0; s < frames; ++s) {
            double step_1 = dt_rate_1 * pitch_1;
            double step_2 = dt_rate_2 * pitch_2;

            // Interpolate both decks (compile-time policy selection)
            auto samples = InterpPolicy::interpolate(
                tr1, sample_1, tr_1_len, pitch_1,
                tr2, sample_2, tr_2_len, pitch_2);

            // Apply volume and mix
            float sum_l = samples.l1 * vol_1 + samples.l2 * vol_2;
            float sum_r = samples.r1 * vol_1 + samples.r2 * vol_2;

            // Scale from int16 range to normalized [-1, 1]
            // (tracks are stored as int16, interpolation returns same scale)
            constexpr float INT16_SCALE = 1.0f / 32768.0f;
            sum_l *= INT16_SCALE;
            sum_r *= INT16_SCALE;

            // Write output (format-aware, compile-time)
            FormatPolicy::write(out_ptr, sum_l);
            FormatPolicy::write(out_ptr + bytes_per_sample, sum_r);

            // Fill remaining channels with zeros (for multi-channel devices)
            for (int ch = 2; ch < channels; ++ch) {
                FormatPolicy::write(out_ptr + ch * bytes_per_sample, 0.0f);
            }

            out_ptr += frame_size;

            sample_1 += step_1;
            sample_2 += step_2;

            // Wrap when crossing track boundary
            // Use fmod for correctness with high pitch values on short loops
            if (tr_1_len > 0 && (sample_1 >= tr_1_len || sample_1 < 0.0)) {
                sample_1 = std::fmod(sample_1, static_cast<double>(tr_1_len));
                if (sample_1 < 0.0) sample_1 += tr_1_len;
            }
            if (tr_2_len > 0 && (sample_2 >= tr_2_len || sample_2 < 0.0)) {
                sample_2 = std::fmod(sample_2, static_cast<double>(tr_2_len));
                if (sample_2 < 0.0) sample_2 += tr_2_len;
            }
            vol_1 += volume_gradient_1;
            vol_2 += volume_gradient_2;
            pitch_1 += pitch_gradient_1;
            pitch_2 += pitch_gradient_2;
        }

        r1 = (sample_1 / tr_1_rate) - (state1->position - state1->position_offset);
        r2 = (sample_2 / tr_2_rate) - (state2->position - state2->position_offset);

        state1->pitch = filtered_pitch_1;
        state2->pitch = filtered_pitch_2;

        spin_unlock(&pl1->lock);
        spin_unlock(&pl2->lock);
    }

    state1->position += r1;
    state1->volume = target_volume_1;

    state2->position += r2;
    state2->volume = target_volume_2;

    // Handle capture: loop recording and monitoring
    int deck = active_recording_deck_;
    bool recording = (deck >= 0 && deck < 2);
    bool has_capture = (capture && capture->buffer);

    if (has_capture) {
        bool monitoring = (deck >= 0 && monitoring_volume_ > 0.0f);

        if (recording || monitoring) {
            float mon_vol = monitoring_volume_;
            out_ptr = static_cast<uint8_t*>(playback);

            for (unsigned long i = 0; i < frames; ++i) {
                // Read capture samples using format-aware reader
                float cap_l = read_capture_sample(capture->buffer, capture->format,
                                                  capture->bytes_per_sample, i,
                                                  capture->left_channel, capture->channels);
                float cap_r = read_capture_sample(capture->buffer, capture->format,
                                                  capture->bytes_per_sample, i,
                                                  capture->right_channel, capture->channels);

                // Write to loop buffer if recording (one sample at a time)
                if (recording) {
                    loop_buffer_write_float(&loop_[deck], cap_l, cap_r);
                }

                // Add monitoring: mix capture input into output
                if (monitoring) {
                    float out_l = FormatPolicy::read(out_ptr) + cap_l * mon_vol;
                    float out_r = FormatPolicy::read(out_ptr + bytes_per_sample) + cap_r * mon_vol;

                    FormatPolicy::write(out_ptr, out_l);
                    FormatPolicy::write(out_ptr + bytes_per_sample, out_r);
                }

                out_ptr += frame_size;
            }
        }
    } else if (recording) {
        // Capture not available but recording is active
        // Don't write anything - this preserves existing audio during punch-in
        // and avoids writing zeros at the start of first recording
        // The diagnostic in alsa.cpp will log when this happens
    }
}

template<typename InterpPolicy, typename FormatPolicy>
void AudioEngine<InterpPolicy, FormatPolicy>::process(
    Sc1000* engine,
    AudioCapture* capture,
    void* playback,
    int playback_channels,
    unsigned long frames)
{
    double start_time = get_time_us();

    process_players(engine, capture, playback, playback_channels, frames);

    double end_time = get_time_us();
    double process_time = end_time - start_time;

    // Calculate time budget
    double budget_time = (static_cast<double>(frames) / SAMPLE_RATE) * 1000000.0;
    double load = (process_time / budget_time) * 100.0;

    // Update stats with exponential moving average
    stats_.process_time_us = process_time;
    stats_.budget_time_us = budget_time;
    stats_.load_percent = 0.9 * stats_.load_percent + 0.1 * load;

    if (load > stats_.load_peak) {
        stats_.load_peak = load;
    }

    if (load > 100.0) {
        stats_.xruns++;
    }

}

//
// Explicit template instantiations
//

// Cubic interpolation variants
template class AudioEngine<CubicInterpolation, FormatS16>;
template class AudioEngine<CubicInterpolation, FormatS24_3LE>;
template class AudioEngine<CubicInterpolation, FormatS24_LE>;
template class AudioEngine<CubicInterpolation, FormatS32>;
template class AudioEngine<CubicInterpolation, FormatFloat>;

// Sinc interpolation variants
template class AudioEngine<SincInterpolation, FormatS16>;
template class AudioEngine<SincInterpolation, FormatS24_3LE>;
template class AudioEngine<SincInterpolation, FormatS24_LE>;
template class AudioEngine<SincInterpolation, FormatS32>;
template class AudioEngine<SincInterpolation, FormatFloat>;

//
// Factory function
//
std::unique_ptr<AudioEngineBase> AudioEngineBase::create(
    InterpolationMode interp,
    snd_pcm_format_t format)
{
#define MAKE_ENGINE(Interp, Format) \
    std::make_unique<AudioEngine<Interp, Format>>()

    if (interp == InterpolationMode::Sinc) {
        switch (format) {
            case SND_PCM_FORMAT_S16_LE:   return MAKE_ENGINE(SincInterpolation, FormatS16);
            case SND_PCM_FORMAT_S24_3LE:  return MAKE_ENGINE(SincInterpolation, FormatS24_3LE);
            case SND_PCM_FORMAT_S24_LE:   return MAKE_ENGINE(SincInterpolation, FormatS24_LE);
            case SND_PCM_FORMAT_S32_LE:   return MAKE_ENGINE(SincInterpolation, FormatS32);
            case SND_PCM_FORMAT_FLOAT_LE: return MAKE_ENGINE(SincInterpolation, FormatFloat);
            default: break;
        }
    } else {
        switch (format) {
            case SND_PCM_FORMAT_S16_LE:   return MAKE_ENGINE(CubicInterpolation, FormatS16);
            case SND_PCM_FORMAT_S24_3LE:  return MAKE_ENGINE(CubicInterpolation, FormatS24_3LE);
            case SND_PCM_FORMAT_S24_LE:   return MAKE_ENGINE(CubicInterpolation, FormatS24_LE);
            case SND_PCM_FORMAT_S32_LE:   return MAKE_ENGINE(CubicInterpolation, FormatS32);
            case SND_PCM_FORMAT_FLOAT_LE: return MAKE_ENGINE(CubicInterpolation, FormatFloat);
            default: break;
        }
    }

#undef MAKE_ENGINE

    // Fallback to S16 sinc
    return std::make_unique<AudioEngine<SincInterpolation, FormatS16>>();
}

} // namespace audio
} // namespace sc

//
// C API implementation (backward compatibility)
//
// Uses a static S16 engine instance for the legacy interface
//

static std::unique_ptr<sc::audio::AudioEngineBase> g_legacy_engine;

static sc::audio::AudioEngineBase* get_legacy_engine() {
    if (!g_legacy_engine) {
        auto mode = (g_interpolation_mode == INTERP_SINC)
            ? sc::audio::InterpolationMode::Sinc
            : sc::audio::InterpolationMode::Cubic;
        g_legacy_engine = sc::audio::AudioEngineBase::create(mode, SND_PCM_FORMAT_S16_LE);
    }
    return g_legacy_engine.get();
}

void audio_engine_process(
    struct Sc1000* engine,
    struct AudioCapture* capture,
    int16_t* playback,
    int playback_channels,
    unsigned long frames)
{
    auto* eng = get_legacy_engine();
    eng->process(engine, capture, playback, playback_channels, frames);

    // Copy stats to global for C API
    const auto& stats = eng->get_stats();
    sc::audio::g_dsp_stats = stats;
}

void audio_engine_get_stats(struct DspStats* stats) {
    stats->load_percent = sc::audio::g_dsp_stats.load_percent;
    stats->load_peak = sc::audio::g_dsp_stats.load_peak;
    stats->process_time_us = sc::audio::g_dsp_stats.process_time_us;
    stats->budget_time_us = sc::audio::g_dsp_stats.budget_time_us;
    stats->xruns = sc::audio::g_dsp_stats.xruns;
}

void audio_engine_update_global_stats(sc::audio::AudioEngineBase* engine) {
    if (engine) {
        sc::audio::g_dsp_stats = engine->get_stats();
    }
}

void audio_engine_reset_peak() {
    sc::audio::g_dsp_stats.load_peak = 0.0;
    sc::audio::g_dsp_stats.xruns = 0;

    if (g_legacy_engine) {
        g_legacy_engine->reset_peak();
    }
}

void audio_engine_set_interpolation(interpolation_mode_t mode) {
    g_interpolation_mode = mode;
    // Reset engine to pick up new mode
    g_legacy_engine.reset();
}

interpolation_mode_t audio_engine_get_interpolation() {
    return g_interpolation_mode;
}
