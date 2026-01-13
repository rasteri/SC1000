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


#pragma once

#include "../core/sc1000.h"
#include <stdint.h>
#include <stdbool.h>
#include <memory>

struct LoopBuffer;

//
// C-compatible types and API (for backward compatibility)
//

// TODO: RIAA curve / vinyl EQ
// Investigate adding RIAA equalization curve for authentic vinyl playback character.
// The RIAA curve has specific time constants:
// - Bass boost below ~500Hz (turnover at 500.5Hz)
// - Treble cut above ~2122Hz
// Could be implemented as a biquad filter chain applied during playback.
// Reference: https://en.wikipedia.org/wiki/RIAA_equalization

/* Interpolation mode selection */
typedef enum {
    INTERP_CUBIC = 0,  /* 4-tap Catmull-Rom (fast, no anti-aliasing) */
    INTERP_SINC = 1    /* 16-tap sinc (slower, proper anti-aliasing) */
} interpolation_mode_t;

/* DSP performance metrics */
struct DspStats {
    double load_percent;      /* Current DSP load (0-100+) */
    double load_peak;         /* Peak load since last reset */
    double process_time_us;   /* Last process time in microseconds */
    double budget_time_us;    /* Time budget per period in microseconds */
    unsigned long xruns;      /* Count of times we exceeded budget */
};

/* Capture input info passed to audio engine (I/O data only, no state) */
struct AudioCapture {
    const void* buffer;         /* Capture samples in device format (nullptr if not available) */
    int format;                 /* ALSA format (snd_pcm_format_t) - use int for C compatibility */
    int bytes_per_sample;       /* Bytes per sample for this format */
    int channels;               /* Total channels in capture buffer */
    int left_channel;           /* Index of left channel */
    int right_channel;          /* Index of right channel */
};

#ifdef __cplusplus
extern "C" {
#endif

/* Set interpolation mode (call before audio engine starts) */
void audio_engine_set_interpolation(interpolation_mode_t mode);

/* Get current interpolation mode */
interpolation_mode_t audio_engine_get_interpolation();

/* Main audio processing function (legacy S16 interface)
 * - capture: input from capture device (can have nullptr buffer if not available)
 * - playback: output buffer to fill
 * - playback_channels: channels in playback buffer (usually 2)
 * - frames: number of frames to process
 */
void audio_engine_process(
    struct Sc1000* engine,
    struct AudioCapture* capture,
    int16_t* playback,
    int playback_channels,
    unsigned long frames
);

void audio_engine_get_stats(struct DspStats* stats);
void audio_engine_reset_peak();

#ifdef __cplusplus
// Update global stats from a specific engine instance (for direct C++ engine usage)
namespace sc { namespace audio { class AudioEngineBase; } }
void audio_engine_update_global_stats(sc::audio::AudioEngineBase* engine);
#endif

#ifdef __cplusplus
}
#endif


#ifdef __cplusplus
//
// C++ class-based interface
//

#include "sample_format.h"
#include "interpolation_policy.h"
#include "loop_buffer.h"
#include "deck_processing_state.h"
#include <alsa/asoundlib.h>

namespace sc {
namespace audio {

//
// DSP statistics (C++ version)
//
struct DspStats {
    double load_percent = 0.0;
    double load_peak = 0.0;
    double process_time_us = 0.0;
    double budget_time_us = 0.0;
    unsigned long xruns = 0;
};

//
// Abstract base class for runtime dispatch
// Virtual dispatch happens once per buffer (~256 samples), negligible overhead.
// Owns loop buffers and recording state - platform layer only handles I/O.
//
class AudioEngineBase {
public:
    virtual ~AudioEngineBase() = default;

    // Initialize loop buffers (call after construction, before processing)
    virtual void init_loop_buffers(int sample_rate, int max_seconds) = 0;

    // Main processing function
    // capture: input audio (nullptr if not available)
    // playback: output buffer (format determined by template)
    // frames: number of frames to process
    virtual void process(
        Sc1000* engine,
        AudioCapture* capture,
        void* playback,
        int playback_channels,
        unsigned long frames) = 0;

    // Recording control
    virtual bool start_recording(int deck, double playback_position = 0.0) = 0;
    virtual void stop_recording(int deck) = 0;
    virtual bool is_recording(int deck) const = 0;
    virtual int recording_deck() const = 0;

    // Loop track access
    virtual Track* get_loop_track(int deck) = 0;      // Acquires reference
    virtual Track* peek_loop_track(int deck) = 0;     // No ref change (RT-safe)
    virtual bool has_loop(int deck) const = 0;
    virtual void reset_loop(int deck) = 0;

    // Monitoring volume for active recording deck
    virtual void set_monitoring_volume(float volume) = 0;
    virtual float monitoring_volume() const = 0;

    // Stats
    virtual const DspStats& get_stats() const = 0;
    virtual void reset_peak() = 0;

    // === Query API for external code ===
    // These provide read-only access to deck state.
    // Thread-safe: audio engine writes, external code reads.

    // Get full deck state (returns copy for thread safety)
    virtual DeckProcessingState get_deck_state(int deck) const = 0;

    // Convenience getters for common queries
    virtual double get_position(int deck) const = 0;
    virtual double get_pitch(int deck) const = 0;
    virtual double get_volume(int deck) const = 0;
    virtual double get_elapsed(int deck) const = 0;
    virtual bool is_deck_active(int deck) const = 0;

    // Factory: creates correct template instantiation based on mode and format
    static std::unique_ptr<AudioEngineBase> create(
        InterpolationMode interp,
        snd_pcm_format_t format);
};

//
// Templated audio engine with compile-time optimized internals
//
// InterpPolicy: CubicInterpolation or SincInterpolation
// FormatPolicy: FormatS16, FormatS24_3LE, FormatS24_LE, FormatS32, FormatFloat
//
template<typename InterpPolicy, typename FormatPolicy>
class AudioEngine final : public AudioEngineBase {
public:
    AudioEngine();
    ~AudioEngine() override;

    void init_loop_buffers(int sample_rate, int max_seconds) override;

    void process(
        Sc1000* engine,
        AudioCapture* capture,
        void* playback,
        int playback_channels,
        unsigned long frames) override;

    // Recording control
    bool start_recording(int deck, double playback_position = 0.0) override;
    void stop_recording(int deck) override;
    bool is_recording(int deck) const override;
    int recording_deck() const override { return active_recording_deck_; }

    // Loop track access
    Track* get_loop_track(int deck) override;
    Track* peek_loop_track(int deck) override;
    bool has_loop(int deck) const override;
    void reset_loop(int deck) override;

    // Monitoring
    void set_monitoring_volume(float volume) override { monitoring_volume_ = volume; }
    float monitoring_volume() const override { return monitoring_volume_; }

    // Stats
    const DspStats& get_stats() const override { return stats_; }
    void reset_peak() override {
        stats_.load_peak = 0.0;
        stats_.xruns = 0;
    }

    // Query API
    DeckProcessingState get_deck_state(int deck) const override {
        if (deck < 0 || deck > 1) return DeckProcessingState{};
        return deck_state_[deck];  // Returns copy
    }

    double get_position(int deck) const override {
        if (deck < 0 || deck > 1) return 0.0;
        return deck_state_[deck].position;
    }

    double get_pitch(int deck) const override {
        if (deck < 0 || deck > 1) return 0.0;
        return deck_state_[deck].pitch;
    }

    double get_volume(int deck) const override {
        if (deck < 0 || deck > 1) return 0.0;
        return deck_state_[deck].volume;
    }

    double get_elapsed(int deck) const override {
        if (deck < 0 || deck > 1) return 0.0;
        return deck_state_[deck].elapsed();
    }

    bool is_deck_active(int deck) const override {
        if (deck < 0 || deck > 1) return false;
        return deck_state_[deck].is_active();
    }

private:
    DspStats stats_{};
    DeckProcessingState deck_state_[2]{};  // Per-deck audio engine internal state
    LoopBuffer loop_[2]{};              // Loop buffers for both decks
    int active_recording_deck_ = -1;     // Which deck is recording (-1 = none)
    float monitoring_volume_ = 0.0f;     // Monitoring volume for recording
    bool loop_buffers_initialized_ = false;

    // Setup player parameters for the block
    void setup_player(Player* pl, DeckProcessingState* state, unsigned long samples,
                      const ScSettings* settings, double track_length_seconds,
                      double* target_volume, double* filtered_pitch);

    // Process and mix both players
    void process_players(
        Sc1000* engine,
        AudioCapture* capture,
        void* playback,
        int channels,
        unsigned long frames);
};

//
// Extern template declarations to prevent multiple instantiations
// Actual instantiations are in audio_engine.cpp
//

// Cubic interpolation variants
extern template class AudioEngine<CubicInterpolation, FormatS16>;
extern template class AudioEngine<CubicInterpolation, FormatS24_3LE>;
extern template class AudioEngine<CubicInterpolation, FormatS24_LE>;
extern template class AudioEngine<CubicInterpolation, FormatS32>;
extern template class AudioEngine<CubicInterpolation, FormatFloat>;

// Sinc interpolation variants
extern template class AudioEngine<SincInterpolation, FormatS16>;
extern template class AudioEngine<SincInterpolation, FormatS24_3LE>;
extern template class AudioEngine<SincInterpolation, FormatS24_LE>;
extern template class AudioEngine<SincInterpolation, FormatS32>;
extern template class AudioEngine<SincInterpolation, FormatFloat>;

} // namespace audio
} // namespace sc

#endif // __cplusplus
