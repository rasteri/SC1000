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


// Sample format traits for SC1000 audio engine
//
// Provides compile-time format handling via template policy classes.
// Each format type has static methods for reading/writing samples
// and constexpr properties for bytes_per_sample, scale, etc.
//
// Usage:
//   FormatS16::write(ptr, sample);
//   float val = FormatS16::read(ptr);

#pragma once

#include <alsa/asoundlib.h>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <cmath>

namespace sc {
namespace audio {

//
// TPDF (Triangular PDF) dither generator
// Uses two uniform randoms summed for triangular distribution
// Thread-local state for lock-free operation in audio callback
//
class TpdfDither {
public:
    // Returns dither value in range [-1, +1] LSB (triangular distribution)
    static inline float generate() {
        // Simple fast PRNG (xorshift32)
        state_ ^= state_ << 13;
        state_ ^= state_ >> 17;
        state_ ^= state_ << 5;
        float r1 = static_cast<float>(static_cast<int32_t>(state_)) * (1.0f / 2147483648.0f);

        state_ ^= state_ << 13;
        state_ ^= state_ >> 17;
        state_ ^= state_ << 5;
        float r2 = static_cast<float>(static_cast<int32_t>(state_)) * (1.0f / 2147483648.0f);

        // Sum of two uniform [-1,1] gives triangular [-2,2], scale to [-1,1]
        return (r1 + r2) * 0.5f;
    }

private:
    static thread_local uint32_t state_;
};

// Initialize with a non-zero seed
inline thread_local uint32_t TpdfDither::state_ = 0x12345678;

//
// Format trait tags for template specialization
//

struct FormatS16 {
    static constexpr snd_pcm_format_t alsa_format = SND_PCM_FORMAT_S16_LE;
    static constexpr int bytes_per_sample = 2;
    static constexpr float scale = 32767.0f;
    static constexpr bool needs_dither = true;

    static inline void write(void* dst, float sample) {
        auto* p = static_cast<int16_t*>(dst);
        // Apply TPDF dither (1 LSB amplitude) before quantization
        float dither = TpdfDither::generate();  // [-1, +1]
        float dithered = sample * scale + dither;
        float clamped = std::fmax(-32768.0f, std::fmin(32767.0f, dithered));
        *p = static_cast<int16_t>(clamped);
    }

    static inline float read(const void* src) {
        const auto* p = static_cast<const int16_t*>(src);
        return static_cast<float>(*p) / scale;
    }
};

struct FormatS24_3LE {
    static constexpr snd_pcm_format_t alsa_format = SND_PCM_FORMAT_S24_3LE;
    static constexpr int bytes_per_sample = 3;
    static constexpr float scale = 8388607.0f;
    static constexpr bool needs_dither = false;  // 24-bit noise floor is -144dB, inaudible

    static inline void write(void* dst, float sample) {
        // Fast path: branchless clamp + single memcpy (compiler optimizes small memcpy)
        float scaled = sample * scale;
        int32_t val = static_cast<int32_t>(scaled);
        // Branchless clamp to 24-bit signed range
        val = val < -8388608 ? -8388608 : (val > 8388607 ? 8388607 : val);
        std::memcpy(dst, &val, 3);  // Compiler optimizes to efficient store
    }

    // Convert float to clamped 24-bit integer (helper for batch operations)
    static inline int32_t to_s24(float sample) {
        float scaled = sample * scale;
        int32_t val = static_cast<int32_t>(scaled);
        return val < -8388608 ? -8388608 : (val > 8388607 ? 8388607 : val);
    }

    // Batch write 4 samples as 3 × 32-bit words (12 bytes)
    // Layout: [S0:24][S1:24][S2:24][S3:24] -> [W0:32][W1:32][W2:32]
    // This eliminates unaligned 3-byte writes in favor of aligned 32-bit stores
    static inline void write_batch4(void* dst, float s0, float s1, float s2, float s3) {
        uint32_t v0 = static_cast<uint32_t>(to_s24(s0)) & 0xFFFFFF;
        uint32_t v1 = static_cast<uint32_t>(to_s24(s1)) & 0xFFFFFF;
        uint32_t v2 = static_cast<uint32_t>(to_s24(s2)) & 0xFFFFFF;
        uint32_t v3 = static_cast<uint32_t>(to_s24(s3)) & 0xFFFFFF;

        uint32_t* out = static_cast<uint32_t*>(dst);
        out[0] = v0 | (v1 << 24);
        out[1] = (v1 >> 8) | (v2 << 16);
        out[2] = (v2 >> 16) | (v3 << 8);
    }

    // Batch write stereo pair (L, R) - 6 bytes
    static inline void write_stereo(void* dst, float l, float r) {
        write(dst, l);
        write(static_cast<uint8_t*>(dst) + 3, r);
    }

    // Batch read stereo pair (L, R) - 6 bytes (for consecutive channels)
    static inline void read_stereo(const void* src, float& l, float& r) {
        l = read(src);
        r = read(static_cast<const uint8_t*>(src) + 3);
    }

    // Batch write 2 stereo frames (L0, R0, L1, R1) = 4 samples = 12 bytes = 3 words
    static inline void write_stereo2(void* dst, float l0, float r0, float l1, float r1) {
        write_batch4(dst, l0, r0, l1, r1);
    }

    static inline float read(const void* src) {
        const auto* p = static_cast<const uint8_t*>(src);
        // Sign-extend from 24-bit
        int32_t val = static_cast<int32_t>(p[0])
                    | (static_cast<int32_t>(p[1]) << 8)
                    | (static_cast<int32_t>(p[2]) << 16);
        // Sign extend if negative (bit 23 set)
        if (val & 0x800000) {
            val |= 0xFF000000;
        }
        return static_cast<float>(val) / scale;
    }

    // Convert 24-bit integer to float (helper for batch operations)
    static inline float from_s24(int32_t val) {
        // Sign extend if negative (bit 23 set)
        if (val & 0x800000) {
            val |= 0xFF000000;
        }
        return static_cast<float>(val) / scale;
    }

    // Batch read 4 samples from 3 × 32-bit words (12 bytes)
    // Inverse of write_batch4
    static inline void read_batch4(const void* src, float& s0, float& s1, float& s2, float& s3) {
        const uint32_t* in = static_cast<const uint32_t*>(src);
        uint32_t w0 = in[0];
        uint32_t w1 = in[1];
        uint32_t w2 = in[2];

        // Extract 24-bit values (inverse of write_batch4 packing)
        int32_t v0 = static_cast<int32_t>(w0 & 0xFFFFFF);
        int32_t v1 = static_cast<int32_t>((w0 >> 24) | ((w1 & 0xFFFF) << 8));
        int32_t v2 = static_cast<int32_t>((w1 >> 16) | ((w2 & 0xFF) << 16));
        int32_t v3 = static_cast<int32_t>(w2 >> 8);

        s0 = from_s24(v0);
        s1 = from_s24(v1);
        s2 = from_s24(v2);
        s3 = from_s24(v3);
    }

    // Batch read 2 stereo frames (L0, R0, L1, R1) = 4 samples = 12 bytes
    static inline void read_stereo2(const void* src, float& l0, float& r0, float& l1, float& r1) {
        read_batch4(src, l0, r0, l1, r1);
    }
};

struct FormatS24_LE {
    static constexpr snd_pcm_format_t alsa_format = SND_PCM_FORMAT_S24_LE;
    static constexpr int bytes_per_sample = 4;  // Stored in low 24 bits of 32-bit word
    static constexpr float scale = 8388607.0f;
    static constexpr bool needs_dither = false;  // 24-bit noise floor is -144dB, inaudible

    static inline void write(void* dst, float sample) {
        float clamped = std::fmax(-8388608.0f, std::fmin(8388607.0f, sample * scale));
        int32_t val = static_cast<int32_t>(clamped);
        // Store in low 24 bits, leave top 8 bits as zero/sign extension
        *static_cast<int32_t*>(dst) = val & 0x00FFFFFF;
    }

    static inline float read(const void* src) {
        int32_t val = *static_cast<const int32_t*>(src);
        // Sign extend from 24-bit
        if (val & 0x800000) {
            val |= 0xFF000000;
        }
        return static_cast<float>(val) / scale;
    }
};

struct FormatS32 {
    static constexpr snd_pcm_format_t alsa_format = SND_PCM_FORMAT_S32_LE;
    static constexpr int bytes_per_sample = 4;
    static constexpr float scale = 2147483647.0f;
    static constexpr bool needs_dither = false;  // 32-bit has plenty of precision

    static inline void write(void* dst, float sample) {
        // Use double for better precision with large scale
        double clamped = std::fmax(-2147483648.0, std::fmin(2147483647.0,
                                   static_cast<double>(sample) * static_cast<double>(scale)));
        *static_cast<int32_t*>(dst) = static_cast<int32_t>(clamped);
    }

    static inline float read(const void* src) {
        return static_cast<float>(*static_cast<const int32_t*>(src)) / scale;
    }
};

struct FormatFloat {
    static constexpr snd_pcm_format_t alsa_format = SND_PCM_FORMAT_FLOAT_LE;
    static constexpr int bytes_per_sample = 4;
    static constexpr float scale = 1.0f;
    static constexpr bool needs_dither = false;

    static inline void write(void* dst, float sample) {
        // Clamp to [-1.0, 1.0] for safety
        *static_cast<float*>(dst) = std::fmax(-1.0f, std::fmin(1.0f, sample));
    }

    static inline float read(const void* src) {
        return *static_cast<const float*>(src);
    }
};

//
// Helper to select format at runtime (returns bytes per sample)
//
inline int get_bytes_per_sample(snd_pcm_format_t format) {
    switch (format) {
        case SND_PCM_FORMAT_S16_LE:   return FormatS16::bytes_per_sample;
        case SND_PCM_FORMAT_S24_3LE:  return FormatS24_3LE::bytes_per_sample;
        case SND_PCM_FORMAT_S24_LE:   return FormatS24_LE::bytes_per_sample;
        case SND_PCM_FORMAT_S32_LE:   return FormatS32::bytes_per_sample;
        case SND_PCM_FORMAT_FLOAT_LE: return FormatFloat::bytes_per_sample;
        default: return 2;  // Default to S16
    }
}

//
// Runtime format reader - reads a sample from any format and returns float [-1, 1]
// Used for capture input where format is determined at runtime
//
inline float read_sample_as_float(const void* src, snd_pcm_format_t format) {
    switch (format) {
        case SND_PCM_FORMAT_S16_LE:   return FormatS16::read(src);
        case SND_PCM_FORMAT_S24_3LE:  return FormatS24_3LE::read(src);
        case SND_PCM_FORMAT_S24_LE:   return FormatS24_LE::read(src);
        case SND_PCM_FORMAT_S32_LE:   return FormatS32::read(src);
        case SND_PCM_FORMAT_FLOAT_LE: return FormatFloat::read(src);
        default: return 0.0f;
    }
}

//
// Read a sample from capture buffer at given frame/channel position
// Returns float [-1, 1]
//
inline float read_capture_sample(const void* buffer, int format, int bytes_per_sample,
                                  unsigned long frame, int channel, int num_channels) {
    const uint8_t* ptr = static_cast<const uint8_t*>(buffer);
    ptr += (frame * num_channels + channel) * bytes_per_sample;
    return read_sample_as_float(ptr, static_cast<snd_pcm_format_t>(format));
}

} // namespace audio
} // namespace sc
