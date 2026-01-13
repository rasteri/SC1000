/*
 * Copyright (C) 2018 Mark Hills <mark@xwax.org>
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

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <cassert>
#include <cstdlib>
#include <sys/poll.h>
#include <alsa/asoundlib.h>

#include "../util/log.h"
#include "../core/sc1000.h"
#include "../core/sc_settings.h"
#include "../engine/audio_engine.h"
#include "../engine/cv_engine.h"
#include "../player/player.h"
#include "../player/track.h"

#include "alsa.h"

//
// Constants
//
static constexpr int TARGET_SAMPLE_RATE = 48000;
static constexpr int DEVICE_CHANNELS = 2;
static constexpr snd_pcm_format_t TARGET_SAMPLE_FORMAT = SND_PCM_FORMAT_S16_LE;

//
// Internal PCM state
//
struct AlsaPcm {
    snd_pcm_t* pcm = nullptr;
    struct pollfd* pe = nullptr;
    size_t pe_count = 0;
    signed short* buf = nullptr;
    int rate = 0;
    snd_pcm_uframes_t period_size = 0;
};

//
// ALSA device info (for discovery)
//
struct AlsaDeviceInfo {
    bool is_present = false;
    int device_id = -1;
    int subdevice_id = -1;
    unsigned int input_channels = 0;
    unsigned int output_channels = 0;
    bool is_internal = false;
    bool supports_48k_samplerate = false;
    bool supports_16bit_pcm = false;
    unsigned int period_size = 256;
    unsigned int buffer_period_factor = 2;
    char card_name[128] = {};
};

static constexpr int MAX_ALSA_DEVICES = 8;
static AlsaDeviceInfo alsa_devices[MAX_ALSA_DEVICES];

//
// AlsaAudio class - implements AudioHardware interface
//
class AlsaAudio : public AudioHardware {
public:
    AlsaAudio(Sc1000* engine) : engine_(engine) {}
    ~AlsaAudio() override;

    // AudioHardware interface
    ssize_t pollfds(struct pollfd* pe, size_t z) override;
    int handle() override;
    unsigned int sample_rate() const override { return playback_.rate; }
    void start() override {}  // PCM started on first write
    void stop() override;     // Stop PCM devices for clean shutdown

    // Recording control (delegated to audio engine)
    bool start_recording(int deck, double playback_position) override;
    void stop_recording(int deck) override;
    bool is_recording(int deck) const override;
    bool has_loop(int deck) const override;
    bool has_capture() const override { return capture_enabled_; }
    void reset_loop(int deck) override;
    Track* get_loop_track(int deck) override;
    Track* peek_loop_track(int deck) override;

    // Query API (delegated to audio engine)
    sc::audio::DeckProcessingState get_deck_state(int deck) const override;
    double get_position(int deck) const override;
    double get_pitch(int deck) const override;
    double get_volume(int deck) const override;

    // Setup (called by factory)
    bool setup(AlsaDeviceInfo* device_info, AudioInterface* config, int num_channels, ScSettings* settings);

private:
    Sc1000* engine_;
    AlsaPcm capture_{};
    AlsaPcm playback_{};
    bool started_ = false;
    bool capture_enabled_ = false;
    int num_channels_ = 2;
    int capture_channels_ = 0;
    int capture_left_ = 0;
    int capture_right_ = 1;
    AudioInterface* config_ = nullptr;
    CvState cv_{};
    snd_pcm_format_t playback_format_ = SND_PCM_FORMAT_S16_LE;
    snd_pcm_format_t capture_format_ = SND_PCM_FORMAT_S16_LE;
    std::unique_ptr<sc::audio::AudioEngineBase> audio_engine_;

    int process_audio();
    static ssize_t pcm_pollfds(AlsaPcm* pcm, struct pollfd* pe, size_t z);
    static int pcm_revents(AlsaPcm* pcm, unsigned short* revents);
    static void pcm_close(AlsaPcm* pcm);
};

//
// Helper functions
//
static void alsa_error(const char* msg, int r) {
    LOG_ERROR("ALSA %s: %s", msg, snd_strerror(r));
}

static bool chk(const char* s, int r) {
    if (r < 0) {
        alsa_error(s, r);
        return false;
    }
    return true;
}

static void create_alsa_device_id_string(char* str, unsigned int size, int dev, int subdev, bool is_plughw) {
    if (!is_plughw) {
        snprintf(str, size, "hw:%d,%d", dev, subdev);
    } else {
        snprintf(str, size, "plughw:%d,%d", dev, subdev);
    }
}

static void print_alsa_device_info(AlsaDeviceInfo* iface) {
    LOG_INFO("Device info: card='%s' dev=%d sub=%d present=%d internal=%d in=%d out=%d 48k=%d 16bit=%d period=%d",
             iface->card_name, iface->device_id, iface->subdevice_id,
             iface->is_present, iface->is_internal,
             iface->input_channels, iface->output_channels,
             iface->supports_48k_samplerate, iface->supports_16bit_pcm,
             iface->period_size);
}

static int format_bytes_per_sample(snd_pcm_format_t fmt) {
    switch (fmt) {
        case SND_PCM_FORMAT_S16_LE:    return 2;
        case SND_PCM_FORMAT_S24_3LE:   return 3;
        case SND_PCM_FORMAT_S24_LE:    return 4;
        case SND_PCM_FORMAT_S32_LE:    return 4;
        case SND_PCM_FORMAT_FLOAT_LE:  return 4;
        default:                       return 2;
    }
}

static snd_pcm_format_t select_best_format(snd_pcm_t* pcm, snd_pcm_hw_params_t* hw_params) {
    static const snd_pcm_format_t formats[] = {
        SND_PCM_FORMAT_S16_LE,
        SND_PCM_FORMAT_S24_3LE,
        SND_PCM_FORMAT_S24_LE,
        SND_PCM_FORMAT_S32_LE,
        SND_PCM_FORMAT_FLOAT_LE
    };

    for (auto fmt : formats) {
        if (snd_pcm_hw_params_test_format(pcm, hw_params, fmt) == 0) {
            LOG_INFO("Selected audio format: %s", snd_pcm_format_name(fmt));
            return fmt;
        }
    }

    LOG_ERROR("No supported audio format found");
    return SND_PCM_FORMAT_UNKNOWN;
}

static void* buffer_generic(const snd_pcm_channel_area_t* area,
                            snd_pcm_uframes_t offset,
                            int num_channels,
                            int bytes_per_sample) {
    assert(area->first % 8 == 0);
    unsigned int expected_step = static_cast<unsigned int>(num_channels * bytes_per_sample * 8);
    if (area->step != expected_step) {
        // Log mismatch but continue - might indicate format issue
        static bool logged_step_mismatch = false;
        if (!logged_step_mismatch) {
            LOG_WARN("ALSA area step mismatch: expected %u bits, got %u bits (channels=%d, bps=%d)",
                     expected_step, area->step, num_channels, bytes_per_sample);
            logged_step_mismatch = true;
        }
    }
    unsigned int bitofs = area->first + area->step * offset;
    return static_cast<char*>(area->addr) + bitofs / 8;
}

//
// Device discovery
//
static void fill_audio_interface_info(ScSettings* settings) {
    int err;
    int card_id = -1, last_card_id = -1;
    char str[64];
    char pcm_name[32];

    snd_pcm_format_mask_t* fmask;
    snd_pcm_format_mask_alloca(&fmask);

    LOG_INFO("Scanning ALSA audio interfaces");

    while ((err = snd_card_next(&card_id)) >= 0 && card_id < 0) {
        LOG_DEBUG("First call returned -1, retrying...");
    }

    if (card_id >= 0) {
        do {
            LOG_DEBUG("card_id %d, last_card_id %d", card_id, last_card_id);
            snd_ctl_t* card_handle;
            sprintf(str, "hw:%i", card_id);

            if ((err = snd_ctl_open(&card_handle, str, 0)) < 0) {
                LOG_WARN("Can't open card %d: %s", card_id, snd_strerror(err));
            } else {
                snd_ctl_card_info_t* card_info = nullptr;
                snd_ctl_card_info_alloca(&card_info);

                if ((err = snd_ctl_card_info(card_handle, card_info)) < 0) {
                    LOG_WARN("Can't get info for card %d: %s", card_id, snd_strerror(err));
                } else {
                    const char* card_name = snd_ctl_card_info_get_name(card_info);
                    LOG_INFO("Card %d = %s", card_id, card_name);

                    if (card_id >= MAX_ALSA_DEVICES) {
                        LOG_WARN("Skipping card %d (max %d devices supported)", card_id, MAX_ALSA_DEVICES);
                        snd_ctl_close(card_handle);
                        continue;
                    }

                    alsa_devices[card_id].is_present = true;
                    strncpy(alsa_devices[card_id].card_name, card_name, sizeof(alsa_devices[card_id].card_name) - 1);

                    if (strcmp(card_name, "sun4i-codec") == 0) {
                        alsa_devices[card_id].is_internal = true;
                    }
                    alsa_devices[card_id].period_size = settings->period_size;
                    alsa_devices[card_id].buffer_period_factor = settings->buffer_period_factor;

                    unsigned int playback_count = 0;
                    unsigned int capture_count = 0;
                    int device_id = -1;

                    while (snd_ctl_pcm_next_device(card_handle, &device_id) >= 0 && device_id >= 0) {
                        create_alsa_device_id_string(pcm_name, sizeof(pcm_name), card_id, device_id, false);

                        snd_pcm_t* pcm;
                        if (snd_pcm_open(&pcm, pcm_name, SND_PCM_STREAM_PLAYBACK, 0) >= 0) {
                            snd_pcm_hw_params_t* params;
                            snd_pcm_hw_params_alloca(&params);
                            snd_pcm_hw_params_any(pcm, params);

                            alsa_devices[card_id].supports_48k_samplerate =
                                (snd_pcm_hw_params_test_rate(pcm, params, TARGET_SAMPLE_RATE, 0) == 0);
                            alsa_devices[card_id].supports_16bit_pcm =
                                (snd_pcm_hw_params_test_format(pcm, params, TARGET_SAMPLE_FORMAT) == 0);

                            unsigned int min, max;
                            if (snd_pcm_hw_params_get_channels_min(params, &min) >= 0 &&
                                snd_pcm_hw_params_get_channels_max(params, &max) >= 0) {
                                if (!snd_pcm_hw_params_test_channels(pcm, params, max)) {
                                    playback_count = max;
                                }
                            }
                            snd_pcm_close(pcm);
                        }

                        if (snd_pcm_open(&pcm, pcm_name, SND_PCM_STREAM_CAPTURE, 0) >= 0) {
                            snd_pcm_hw_params_t* params;
                            snd_pcm_hw_params_alloca(&params);
                            snd_pcm_hw_params_any(pcm, params);

                            unsigned int min, max;
                            if (snd_pcm_hw_params_get_channels_min(params, &min) >= 0 &&
                                snd_pcm_hw_params_get_channels_max(params, &max) >= 0) {
                                if (!snd_pcm_hw_params_test_channels(pcm, params, max)) {
                                    capture_count = max;
                                }
                            }
                            snd_pcm_close(pcm);
                        }

                        alsa_devices[card_id].input_channels = capture_count;
                        alsa_devices[card_id].output_channels = playback_count;
                        alsa_devices[card_id].device_id = card_id;
                        alsa_devices[card_id].subdevice_id = 0;
                    }
                    snd_ctl_close(card_handle);
                }
            }
            last_card_id = card_id;
        } while ((err = snd_card_next(&card_id)) >= 0 && card_id >= 0);
    }

    snd_config_update_free_global();
}

static bool contains_substring_ci(const char* haystack, const char* needle) {
    if (!haystack || !needle) return false;
    if (needle[0] == '\0') return true;

    size_t haylen = strlen(haystack);
    size_t needlelen = strlen(needle);
    if (needlelen > haylen) return false;

    for (size_t i = 0; i <= haylen - needlelen; i++) {
        bool match = true;
        for (size_t j = 0; j < needlelen; j++) {
            char h = haystack[i + j];
            char n = needle[j];
            if (h >= 'A' && h <= 'Z') h = static_cast<char>(h + ('a' - 'A'));
            if (n >= 'A' && n <= 'Z') n = static_cast<char>(n + ('a' - 'A'));
            if (h != n) { match = false; break; }
        }
        if (match) return true;
    }
    return false;
}

static AlsaDeviceInfo* find_matching_device(AudioInterface* config) {
    int card_num = -1;
    if (sscanf(config->device.c_str(), "hw:%d", &card_num) == 1 ||
        sscanf(config->device.c_str(), "plughw:%d", &card_num) == 1) {
        if (card_num >= 0 && card_num < MAX_ALSA_DEVICES && alsa_devices[card_num].is_present) {
            // Verify device has audio output capability (not just MIDI)
            if (alsa_devices[card_num].output_channels > 0) {
                return &alsa_devices[card_num];
            }
            LOG_DEBUG("Device hw:%d has no audio output, skipping", card_num);
        }
    }

    // Substring match on card name - only consider devices with audio output
    for (int i = 0; i < MAX_ALSA_DEVICES; i++) {
        if (!alsa_devices[i].is_present) continue;
        if (alsa_devices[i].output_channels == 0) continue;  // Skip MIDI-only devices
        if (contains_substring_ci(alsa_devices[i].card_name, config->device.c_str()) ||
            contains_substring_ci(alsa_devices[i].card_name, config->name.c_str())) {
            return &alsa_devices[i];
        }
    }
    return nullptr;
}

//
// PCM open helper
//
static int pcm_open(AlsaPcm* alsa, const char* device_name,
                    snd_pcm_stream_t stream, AlsaDeviceInfo* device_info,
                    uint8_t num_channels, snd_pcm_format_t* out_format = nullptr) {
    int err, dir;
    snd_pcm_hw_params_t* hw_params;
    snd_pcm_uframes_t frames;

    err = snd_pcm_open(&alsa->pcm, device_name, stream, SND_PCM_NONBLOCK);
    if (!chk("open", err)) return -1;

    snd_pcm_hw_params_alloca(&hw_params);
    if (!chk("hw_params_any", snd_pcm_hw_params_any(alsa->pcm, hw_params))) return -1;
    if (!chk("hw_params_set_access", snd_pcm_hw_params_set_access(alsa->pcm, hw_params, SND_PCM_ACCESS_MMAP_INTERLEAVED))) return -1;

    snd_pcm_format_t format = select_best_format(alsa->pcm, hw_params);
    if (format == SND_PCM_FORMAT_UNKNOWN) return -1;
    if (!chk("hw_params_set_format", snd_pcm_hw_params_set_format(alsa->pcm, hw_params, format))) return -1;
    if (out_format) *out_format = format;

    if (!chk("hw_params_set_rate_resample", snd_pcm_hw_params_set_rate_resample(alsa->pcm, hw_params, 0))) return -1;
    if (!chk("hw_params_set_rate", snd_pcm_hw_params_set_rate(alsa->pcm, hw_params, TARGET_SAMPLE_RATE, 0))) return -1;
    alsa->rate = TARGET_SAMPLE_RATE;

    if (!chk("hw_params_set_channels", snd_pcm_hw_params_set_channels(alsa->pcm, hw_params, num_channels))) return -1;

    snd_pcm_uframes_t period_size = device_info->period_size;
    if (!chk("hw_params_set_period_size_near", snd_pcm_hw_params_set_period_size_near(alsa->pcm, hw_params, &period_size, &dir))) return -1;
    if (!chk("hw_params_get_period_size", snd_pcm_hw_params_get_period_size(hw_params, &period_size, nullptr))) return -1;
    alsa->period_size = period_size;
    LOG_INFO("Period size: %lu frames", period_size);

    auto buffer_size = static_cast<snd_pcm_uframes_t>(period_size * device_info->buffer_period_factor);
    if (stream == SND_PCM_STREAM_CAPTURE) {
        // Use same buffer size as playback for consistent timing
        // Using set_buffer_size_last can result in very large buffers that overflow
        if (!chk("hw_params_set_buffer_size", snd_pcm_hw_params_set_buffer_size(alsa->pcm, hw_params, buffer_size))) {
            // Fall back to largest if requested size isn't supported
            LOG_WARN("Capture buffer size %lu not supported, using maximum", buffer_size);
            if (!chk("hw_params_set_buffer_size_last", snd_pcm_hw_params_set_buffer_size_last(alsa->pcm, hw_params, &frames))) return -1;
        }
    } else {
        if (!chk("hw_params_set_buffer_size", snd_pcm_hw_params_set_buffer_size(alsa->pcm, hw_params, buffer_size))) return -1;
    }

    // Log actual buffer size
    snd_pcm_uframes_t actual_buffer;
    snd_pcm_hw_params_get_buffer_size(hw_params, &actual_buffer);
    LOG_INFO("%s buffer size: %lu frames (%.1f ms)",
             stream == SND_PCM_STREAM_CAPTURE ? "Capture" : "Playback",
             actual_buffer, (actual_buffer * 1000.0) / TARGET_SAMPLE_RATE);

    if (!chk("hw_params", snd_pcm_hw_params(alsa->pcm, hw_params))) return -1;
    return 0;
}

//
// AlsaAudio implementation
//
AlsaAudio::~AlsaAudio() {
    audio_engine_.reset();
    if (capture_enabled_) pcm_close(&capture_);
    pcm_close(&playback_);

    // Free global ALSA config to ensure clean state for next run
    snd_config_update_free_global();
}

void AlsaAudio::stop() {
    // Drop pending audio to stop PCM immediately
    // This ensures poll() in RT thread returns and thread can exit
    if (capture_.pcm) {
        snd_pcm_drop(capture_.pcm);
    }
    if (playback_.pcm) {
        snd_pcm_drop(playback_.pcm);
    }
    started_ = false;
}

void AlsaAudio::pcm_close(AlsaPcm* pcm) {
    if (pcm->pcm) {
        // Drop any pending frames and stop the PCM before closing
        snd_pcm_drop(pcm->pcm);
        snd_pcm_close(pcm->pcm);
        pcm->pcm = nullptr;
    }
    if (pcm->buf) {
        free(pcm->buf);
        pcm->buf = nullptr;
    }
}

ssize_t AlsaAudio::pcm_pollfds(AlsaPcm* pcm, struct pollfd* pe, size_t z) {
    int count = snd_pcm_poll_descriptors_count(pcm->pcm);
    auto ucount = static_cast<unsigned int>(count);
    if (ucount > z) return -1;
    if (ucount == 0) {
        pcm->pe = nullptr;
    } else {
        int r = snd_pcm_poll_descriptors(pcm->pcm, pe, ucount);
        if (r < 0) {
            alsa_error("poll_descriptors", r);
            return -1;
        }
        pcm->pe = pe;
    }
    pcm->pe_count = ucount;
    return count;
}

int AlsaAudio::pcm_revents(AlsaPcm* pcm, unsigned short* revents) {
    int r = snd_pcm_poll_descriptors_revents(pcm->pcm, pcm->pe, static_cast<unsigned int>(pcm->pe_count), revents);
    if (r < 0) {
        alsa_error("poll_descriptors_revents", r);
        return -1;
    }
    return 0;
}

ssize_t AlsaAudio::pollfds(struct pollfd* pe, size_t z) {
    return pcm_pollfds(&playback_, pe, z);
}

int AlsaAudio::handle() {
    unsigned short revents;
    if (pcm_revents(&playback_, &revents) < 0) return -1;

    if (revents & POLLOUT) {
        int r = process_audio();
        if (r < 0) {
            if (r == -EPIPE) {
                LOG_WARN("ALSA: playback xrun");
                if (snd_pcm_prepare(playback_.pcm) < 0) return -1;
                started_ = false;
            } else {
                alsa_error("playback", r);
                return -1;
            }
        }
    }
    return 0;
}

int AlsaAudio::process_audio() {
    const snd_pcm_channel_area_t* playback_areas;
    const snd_pcm_channel_area_t* capture_areas = nullptr;
    snd_pcm_uframes_t playback_offset, playback_frames;
    snd_pcm_uframes_t capture_offset = 0, capture_frames = 0;
    snd_pcm_sframes_t commitres, avail;
    int err;

    engine_->handle_deck_recording();

    avail = snd_pcm_avail_update(playback_.pcm);
    if (avail < 0) return static_cast<int>(avail);
    if (static_cast<snd_pcm_uframes_t>(avail) < playback_.period_size) return 0;

    playback_frames = playback_.period_size;
    err = snd_pcm_mmap_begin(playback_.pcm, &playback_areas, &playback_offset, &playback_frames);
    if (err < 0) return err;

    int bytes_per_sample = format_bytes_per_sample(playback_format_);
    void* playback_pcm = buffer_generic(&playback_areas[0], playback_offset, num_channels_, bytes_per_sample);

    if (num_channels_ > 2) {
        memset(playback_pcm, 0, playback_frames * num_channels_ * bytes_per_sample);
    }

    const void* capture_pcm = nullptr;
    int capture_bps = 0;
    bool capture_valid = false;

    if (capture_enabled_) {
        snd_pcm_sframes_t capture_avail = snd_pcm_avail_update(capture_.pcm);
        if (capture_avail >= static_cast<snd_pcm_sframes_t>(capture_.period_size)) {
            capture_frames = capture_.period_size;
            err = snd_pcm_mmap_begin(capture_.pcm, &capture_areas, &capture_offset, &capture_frames);
            if (err >= 0) {
                capture_bps = format_bytes_per_sample(capture_format_);
                capture_pcm = buffer_generic(&capture_areas[0], capture_offset, capture_channels_, capture_bps);
                capture_valid = true;

                // One-time debug: log capture format info
                static bool logged_capture_info = false;
                if (!logged_capture_info) {
                    LOG_INFO("Capture: format=%s bps=%d channels=%d left=%d right=%d frames=%lu",
                             snd_pcm_format_name(capture_format_), capture_bps,
                             capture_channels_, capture_left_, capture_right_, capture_frames);
                    logged_capture_info = true;
                }
            }
        } else if (capture_avail == -EPIPE) {
            LOG_RT_WARN("Capture overrun (EPIPE)");
            snd_pcm_prepare(capture_.pcm);
            snd_pcm_start(capture_.pcm);
        } else if (capture_avail < 0) {
            LOG_RT_WARN("Capture error: %ld", capture_avail);
        } else {
            // Not enough capture data available - this causes a gap in recording!
            static unsigned long capture_underrun_count = 0;
            capture_underrun_count++;
            if ((capture_underrun_count % 100) == 1) {
                LOG_RT_WARN("Capture underrun #%lu: avail=%ld need=%lu",
                           capture_underrun_count, capture_avail, capture_.period_size);
            }
        }
    }

    AudioCapture capture_info = {};
    if (capture_valid && capture_pcm) {
        capture_info.buffer = capture_pcm;
        capture_info.format = static_cast<int>(capture_format_);
        capture_info.bytes_per_sample = capture_bps;
        capture_info.channels = capture_channels_;
        capture_info.left_channel = capture_left_;
        capture_info.right_channel = capture_right_;

        constexpr float BASE_VOLUME = 7.0f / 8.0f;
        int rec_deck = audio_engine_->recording_deck();
        if (rec_deck == 0) {
            audio_engine_->set_monitoring_volume(BASE_VOLUME * static_cast<float>(audio_engine_->get_volume(0)));
        } else if (rec_deck == 1) {
            audio_engine_->set_monitoring_volume(BASE_VOLUME * static_cast<float>(audio_engine_->get_volume(1)));
        } else {
            audio_engine_->set_monitoring_volume(0.0f);
        }
    }

    if (num_channels_ == 2) {
        audio_engine_->process(engine_, capture_valid ? &capture_info : nullptr, playback_pcm, 2, playback_frames);
        audio_engine_update_global_stats(audio_engine_.get());
    } else {
        alignas(16) static uint8_t stereo_buf[1024 * 4 * 2];
        audio_engine_->process(engine_, capture_valid ? &capture_info : nullptr, stereo_buf, 2, playback_frames);
        audio_engine_update_global_stats(audio_engine_.get());

        // Zero entire output buffer first (required for unmapped channels)
        const int frame_stride = num_channels_ * bytes_per_sample;
        const int stereo_bytes = 2 * bytes_per_sample;
        memset(playback_pcm, 0, playback_frames * frame_stride);

        // Scatter stereo to channels 0-1 using pointer increment (no per-frame multiply)
        uint8_t* out = static_cast<uint8_t*>(playback_pcm);
        const uint8_t* in = stereo_buf;
        for (unsigned long i = 0; i < playback_frames; i++) {
            memcpy(out, in, stereo_bytes);  // Copy L+R together (6 bytes for S24)
            out += frame_stride;
            in += stereo_bytes;
        }

        if (config_ && config_->supports_cv) {
            // Use query API for audio engine output state
            auto deck_state = audio_engine_->get_deck_state(1);  // Scratch deck = 1
            CvControllerInput cv_input = {
                .pitch = deck_state.pitch,
                .encoder_angle = engine_->scratch_deck.encoder_state.angle,
                .sample_position = deck_state.position,
                .sample_length = engine_->scratch_deck.player.track ? engine_->scratch_deck.player.track->length : 0,
                .fader_volume = deck_state.fader_current,  // Smoothed crossfader position (NOT audio volume!)
                .fader_target = engine_->scratch_deck.player.input.crossfader,  // Input (target)
                .crossfader_position = engine_->crossfader.position()
            };
            cv_engine_update(&cv_, &cv_input);
            cv_engine_process_format(&cv_, playback_pcm, num_channels_,
                                     playback_format_, bytes_per_sample, playback_frames);
        }
    }

    if (capture_valid) {
        commitres = snd_pcm_mmap_commit(capture_.pcm, capture_offset, capture_frames);
        if (commitres < 0 || static_cast<snd_pcm_uframes_t>(commitres) != capture_frames) {
            if (commitres == -EPIPE) {
                snd_pcm_prepare(capture_.pcm);
                snd_pcm_start(capture_.pcm);
            }
        }
    }

    commitres = snd_pcm_mmap_commit(playback_.pcm, playback_offset, playback_frames);
    if (commitres < 0 || static_cast<snd_pcm_uframes_t>(commitres) != playback_frames) {
        return commitres < 0 ? static_cast<int>(commitres) : -EPIPE;
    }

    if (!started_) {
        err = snd_pcm_start(playback_.pcm);
        if (err < 0) return err;
        started_ = true;
    }

    return 0;
}

bool AlsaAudio::start_recording(int deck, double playback_position) {
    if (!capture_enabled_) {
        LOG_WARN("Recording not available: no capture device");
        return false;
    }
    return audio_engine_->start_recording(deck, playback_position);
}

void AlsaAudio::stop_recording(int deck) {
    audio_engine_->stop_recording(deck);
}

bool AlsaAudio::is_recording(int deck) const {
    return audio_engine_->is_recording(deck);
}

bool AlsaAudio::has_loop(int deck) const {
    return audio_engine_->has_loop(deck);
}

void AlsaAudio::reset_loop(int deck) {
    audio_engine_->reset_loop(deck);
}

Track* AlsaAudio::get_loop_track(int deck) {
    return audio_engine_->get_loop_track(deck);
}

Track* AlsaAudio::peek_loop_track(int deck) {
    return audio_engine_->peek_loop_track(deck);
}

sc::audio::DeckProcessingState AlsaAudio::get_deck_state(int deck) const {
    return audio_engine_->get_deck_state(deck);
}

double AlsaAudio::get_position(int deck) const {
    return audio_engine_->get_position(deck);
}

double AlsaAudio::get_pitch(int deck) const {
    return audio_engine_->get_pitch(deck);
}

double AlsaAudio::get_volume(int deck) const {
    return audio_engine_->get_volume(deck);
}

bool AlsaAudio::setup(AlsaDeviceInfo* device_info, AudioInterface* config, int num_channels, ScSettings* settings) {
    print_alsa_device_info(device_info);

    bool needs_plughw = !device_info->supports_48k_samplerate;
    char device_name[64];
    create_alsa_device_id_string(device_name, sizeof(device_name), device_info->device_id, device_info->subdevice_id, needs_plughw);
    LOG_INFO("Opening device %s with %d channels, period size %d...", device_name, num_channels, device_info->period_size);

    if (pcm_open(&playback_, device_name, SND_PCM_STREAM_PLAYBACK, device_info, num_channels, &playback_format_) < 0) {
        LOG_ERROR("Failed to open device for playback");
        return false;
    }

    auto interp_mode = audio_engine_get_interpolation() == INTERP_SINC
                       ? sc::audio::InterpolationMode::Sinc
                       : sc::audio::InterpolationMode::Cubic;
    audio_engine_ = sc::audio::AudioEngineBase::create(interp_mode, playback_format_);
    if (!audio_engine_) {
        LOG_ERROR("Failed to create audio engine for format %s", snd_pcm_format_name(playback_format_));
        return false;
    }
    LOG_INFO("Audio engine created: %s interpolation, %s format",
             interp_mode == sc::audio::InterpolationMode::Sinc ? "sinc" : "cubic",
             snd_pcm_format_name(playback_format_));

    num_channels_ = num_channels;
    config_ = config;
    capture_left_ = config ? config->input_left : 0;
    capture_right_ = config ? config->input_right : 1;

    int hw_input_channels = static_cast<int>(device_info->input_channels);
    if (hw_input_channels >= 2) {
        LOG_INFO("Opening capture with %d channels (using left=%d, right=%d)...",
                 hw_input_channels, capture_left_, capture_right_);
        if (pcm_open(&capture_, device_name, SND_PCM_STREAM_CAPTURE, device_info,
                     static_cast<uint8_t>(hw_input_channels), &capture_format_) >= 0) {
            capture_enabled_ = true;
            capture_channels_ = hw_input_channels;
            LOG_INFO("Capture device opened successfully (format: %s)", snd_pcm_format_name(capture_format_));
            if (snd_pcm_start(capture_.pcm) < 0) {
                LOG_WARN("Failed to start capture PCM");
            }
        } else {
            LOG_WARN("Failed to open capture device, recording disabled");
        }
    }

    int loop_max = settings ? settings->loop_max_seconds : 60;
    audio_engine_->init_loop_buffers(TARGET_SAMPLE_RATE, loop_max);

    if (config && config->supports_cv) {
        cv_engine_init(&cv_, TARGET_SAMPLE_RATE);
        cv_engine_set_mapping(&cv_, config);
        LOG_INFO("CV engine initialized for %s", config->name.c_str());
    }

    LOG_INFO("ALSA device setup complete");
    return true;
}

//
// Factory function
//
std::unique_ptr<AudioHardware> alsa_create(Sc1000* engine, ScSettings* settings) {
    LOG_INFO("ALSA init starting");
    sleep(settings->audio_init_delay);
    fill_audio_interface_info(settings);

    for (auto& config : settings->audio_interfaces) {
        AlsaDeviceInfo* device = find_matching_device(&config);
        if (device) {
            LOG_INFO("Matched config '%s' to device %s", config.name.c_str(), config.device.c_str());
            auto alsa = std::make_unique<AlsaAudio>(engine);
            if (alsa->setup(device, &config, config.channels, settings)) {
                return alsa;
            }
            return nullptr;
        }
        LOG_DEBUG("Config '%s' (%s) - device not found", config.name.c_str(), config.device.c_str());
    }

    LOG_INFO("No config match, using fallback");
    for (int i = 0; i < MAX_ALSA_DEVICES; i++) {
        if (alsa_devices[i].is_present && alsa_devices[i].output_channels > 0) {
            LOG_INFO("Using fallback device %d (%s)", i, alsa_devices[i].card_name);
            auto alsa = std::make_unique<AlsaAudio>(engine);
            if (alsa->setup(&alsa_devices[i], nullptr, DEVICE_CHANNELS, settings)) {
                return alsa;
            }
            return nullptr;
        }
    }

    LOG_ERROR("No audio device found!");
    return nullptr;
}

void alsa_clear_config_cache() {
    int r = snd_config_update_free_global();
    if (r < 0) {
        alsa_error("config_update_free_global", r);
    }
}
