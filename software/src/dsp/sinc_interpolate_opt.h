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


// Optimized sinc interpolation for SC1000 audio resampling
//
// Key optimizations:
// 1. Direct track pointer access - avoids get_sample() per tap
// 2. Pre-lerped kernel computation - done once per sample, not per tap
// 3. NEON SIMD for 16-tap convolution - 4 taps per instruction
// 4. Dual-deck parallel processing
//
// Cache-friendly: samples are contiguous within track blocks (99%+ of time)

#pragma once

#include "sinc_table.h"
#include "../player/track.h"
#include <cmath>
#include <cstring>

// ARM NEON intrinsics
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#define SINC_USE_NEON 1
#else
#define SINC_USE_NEON 0
#endif

namespace sc {
namespace dsp {

//
// Pre-lerped kernel (computed once per sample, used for all channels)
//
struct alignas(16) PreLerpedKernel {
    float coeffs[SINC_NUM_TAPS];  // 16 floats, 64 bytes
};

// Compute pre-lerped kernel for a given fractional position and bandwidth
inline void compute_lerped_kernel(float frac, int bw_idx, PreLerpedKernel& out) {
    float phase_f = frac * SINC_NUM_PHASES;
    int phase0 = static_cast<int>(phase_f);
    float w1 = phase_f - phase0;
    float w0 = 1.0f - w1;

    if (phase0 >= SINC_NUM_PHASES - 1) {
        phase0 = SINC_NUM_PHASES - 2;
        w1 = 1.0f;
        w0 = 0.0f;
    }
    if (phase0 < 0) {
        phase0 = 0;
        w1 = 0.0f;
        w0 = 1.0f;
    }

    const float* k0 = sinc_tables[bw_idx][phase0];
    const float* k1 = sinc_tables[bw_idx][phase0 + 1];

#if SINC_USE_NEON
    // NEON: lerp 4 coefficients at a time
    float32x4_t vw0 = vdupq_n_f32(w0);
    float32x4_t vw1 = vdupq_n_f32(w1);

    for (int i = 0; i < SINC_NUM_TAPS; i += 4) {
        float32x4_t vk0 = vld1q_f32(k0 + i);
        float32x4_t vk1 = vld1q_f32(k1 + i);
        float32x4_t result = vmlaq_f32(vmulq_f32(vk0, vw0), vk1, vw1);
        vst1q_f32(out.coeffs + i, result);
    }
#else
    // Scalar fallback
    for (int i = 0; i < SINC_NUM_TAPS; ++i) {
        out.coeffs[i] = k0[i] * w0 + k1[i] * w1;
    }
#endif
}

//
// Direct track sample access
//
// Check if sample range fits within a single track block.
// If yes, return direct pointer to avoid copies.
// This is true 99%+ of the time.
//

struct TrackSampleWindow {
    const signed short* samples;  // Direct pointer or nullptr if spanning blocks
    int start_sample;             // First sample index
    int block_idx;                // Which block we're in
    int offset_in_block;          // Offset within block
    bool valid;                   // True if direct access possible
};

inline TrackSampleWindow get_sample_window(Track* tr, int center_sample, int tr_len) {
    TrackSampleWindow w;
    w.valid = false;

    if (tr_len == 0) return w;

    // Wrap center_sample to track bounds first (position can grow indefinitely)
    center_sample = center_sample % tr_len;
    if (center_sample < 0) center_sample += tr_len;

    constexpr int HALF_TAPS = SINC_NUM_TAPS / 2;
    int start = center_sample - HALF_TAPS;
    int end = center_sample + HALF_TAPS - 1;

    // Handle wrap-around and boundary conditions
    // Use slow path near boundaries for safety during concurrent recording
    if (start < 0 || end >= tr_len) {
        return w;  // Spanning boundary, need slow path
    }

    // Check if all samples fit in same block
    int start_block = start / TRACK_BLOCK_SAMPLES;
    int end_block = end / TRACK_BLOCK_SAMPLES;

    if (start_block != end_block) {
        return w;  // Spanning blocks, need slow path
    }

    // Direct access possible
    w.block_idx = start_block;
    w.offset_in_block = (start % TRACK_BLOCK_SAMPLES) * TRACK_CHANNELS;
    w.samples = tr->block[start_block]->pcm + w.offset_in_block;
    w.start_sample = start;
    w.valid = true;

    return w;
}

//
// NEON-optimized sinc convolution
//

#if SINC_USE_NEON

// Apply pre-lerped kernel to stereo samples (interleaved in track)
// samples points to [L0,R0,L1,R1,...L15,R15]
inline void sinc_convolve_stereo_direct(
    const PreLerpedKernel& kernel,
    const signed short* samples,  // Interleaved stereo
    float& out_l, float& out_r)
{
    float32x4_t sum_l = vdupq_n_f32(0.0f);
    float32x4_t sum_r = vdupq_n_f32(0.0f);

    // Process 4 stereo pairs at a time (8 samples)
    for (int i = 0; i < SINC_NUM_TAPS; i += 4) {
        // Load 4 kernel coefficients
        float32x4_t k = vld1q_f32(kernel.coeffs + i);

        // Load 8 interleaved samples: [L0,R0,L1,R1,L2,R2,L3,R3]
        int16x8_t samp_i16 = vld1q_s16(samples + i * 2);

        // Convert to float
        int32x4_t samp_lo_i32 = vmovl_s16(vget_low_s16(samp_i16));
        int32x4_t samp_hi_i32 = vmovl_s16(vget_high_s16(samp_i16));
        float32x4_t samp_lo = vcvtq_f32_s32(samp_lo_i32);  // [L0,R0,L1,R1]
        float32x4_t samp_hi = vcvtq_f32_s32(samp_hi_i32);  // [L2,R2,L3,R3]

        // Deinterleave: extract L and R
        // samp_lo = [L0,R0,L1,R1], samp_hi = [L2,R2,L3,R3]
        float32x4x2_t deint_lo = vuzpq_f32(samp_lo, samp_hi);
        // deint_lo.val[0] = [L0,L1,L2,L3], deint_lo.val[1] = [R0,R1,R2,R3]

        // Multiply-accumulate
        sum_l = vmlaq_f32(sum_l, k, deint_lo.val[0]);
        sum_r = vmlaq_f32(sum_r, k, deint_lo.val[1]);
    }

    // Horizontal sum
    float32x2_t sum_l_2 = vadd_f32(vget_low_f32(sum_l), vget_high_f32(sum_l));
    float32x2_t sum_r_2 = vadd_f32(vget_low_f32(sum_r), vget_high_f32(sum_r));
    out_l = vget_lane_f32(vpadd_f32(sum_l_2, sum_l_2), 0);
    out_r = vget_lane_f32(vpadd_f32(sum_r_2, sum_r_2), 0);
}

// Fallback for boundary cases (samples copied to buffer)
inline void sinc_convolve_stereo_buffered(
    const PreLerpedKernel& kernel,
    const float* samples_l,
    const float* samples_r,
    float& out_l, float& out_r)
{
    float32x4_t sum_l = vdupq_n_f32(0.0f);
    float32x4_t sum_r = vdupq_n_f32(0.0f);

    for (int i = 0; i < SINC_NUM_TAPS; i += 4) {
        float32x4_t k = vld1q_f32(kernel.coeffs + i);
        float32x4_t sl = vld1q_f32(samples_l + i);
        float32x4_t sr = vld1q_f32(samples_r + i);
        sum_l = vmlaq_f32(sum_l, k, sl);
        sum_r = vmlaq_f32(sum_r, k, sr);
    }

    float32x2_t sum_l_2 = vadd_f32(vget_low_f32(sum_l), vget_high_f32(sum_l));
    float32x2_t sum_r_2 = vadd_f32(vget_low_f32(sum_r), vget_high_f32(sum_r));
    out_l = vget_lane_f32(vpadd_f32(sum_l_2, sum_l_2), 0);
    out_r = vget_lane_f32(vpadd_f32(sum_r_2, sum_r_2), 0);
}

#else  // !SINC_USE_NEON

// Scalar fallback for non-ARM platforms
inline void sinc_convolve_stereo_direct(
    const PreLerpedKernel& kernel,
    const signed short* samples,
    float& out_l, float& out_r)
{
    float sum_l = 0.0f;
    float sum_r = 0.0f;

    for (int i = 0; i < SINC_NUM_TAPS; ++i) {
        float k = kernel.coeffs[i];
        sum_l += k * static_cast<float>(samples[i * 2]);
        sum_r += k * static_cast<float>(samples[i * 2 + 1]);
    }

    out_l = sum_l;
    out_r = sum_r;
}

inline void sinc_convolve_stereo_buffered(
    const PreLerpedKernel& kernel,
    const float* samples_l,
    const float* samples_r,
    float& out_l, float& out_r)
{
    float sum_l = 0.0f;
    float sum_r = 0.0f;

    for (int i = 0; i < SINC_NUM_TAPS; ++i) {
        float k = kernel.coeffs[i];
        sum_l += k * samples_l[i];
        sum_r += k * samples_r[i];
    }

    out_l = sum_l;
    out_r = sum_r;
}

#endif  // SINC_USE_NEON

//
// Slow path: collect samples across block/track boundaries
//
inline void collect_samples_slow(
    Track* tr,
    int center_sample,
    int tr_len,
    float* samples_l,
    float* samples_r)
{
    constexpr int HALF_TAPS = SINC_NUM_TAPS / 2;
    int start = center_sample - HALF_TAPS;

    for (int i = 0; i < SINC_NUM_TAPS; ++i) {
        int idx = start + i;

        // Wrap to track boundary using modulo (O(1) instead of O(n) while loop)
        if (tr_len != 0) {
            idx = idx % tr_len;
            if (idx < 0) idx += tr_len;  // Handle negative modulo
        }

        if (idx >= 0 && idx < tr_len) {
            signed short* s = tr->get_sample(idx);
            samples_l[i] = static_cast<float>(s[0]);
            samples_r[i] = static_cast<float>(s[1]);
        } else {
            samples_l[i] = 0.0f;
            samples_r[i] = 0.0f;
        }
    }
}

//
// Main optimized interpolation function for one deck
//
struct SincInterpResult {
    float left;
    float right;
};

inline SincInterpResult sinc_interpolate_track_opt(
    Track* tr,
    double sample_pos,
    int tr_len,
    float abs_pitch)
{
    SincInterpResult result = {0.0f, 0.0f};

    if (tr_len == 0) return result;

    // Note: sample_pos is expected to be pre-wrapped by caller (audio engine)
    // This avoids expensive fmod() on every sample

    // Get integer and fractional parts
    int center = static_cast<int>(sample_pos);
    if (sample_pos < 0.0) center--;
    float frac = static_cast<float>(sample_pos - center);

    // Select bandwidth based on pitch
    int bw_idx = sinc_select_bandwidth(abs_pitch);

    // Pre-compute lerped kernel (done once, used for both L and R)
    PreLerpedKernel kernel;
    compute_lerped_kernel(frac, bw_idx, kernel);

    // Try direct access first
    auto window = get_sample_window(tr, center, tr_len);

    if (window.valid) {
        // Fast path: direct convolution from track memory
        sinc_convolve_stereo_direct(kernel, window.samples, result.left, result.right);
    } else {
        // Slow path: collect samples through get_sample
        alignas(16) float samples_l[SINC_NUM_TAPS];
        alignas(16) float samples_r[SINC_NUM_TAPS];
        collect_samples_slow(tr, center, tr_len, samples_l, samples_r);
        sinc_convolve_stereo_buffered(kernel, samples_l, samples_r, result.left, result.right);
    }

    return result;
}

//
// Dual-deck optimized interpolation
//
struct DualDeckResultOpt {
    float l1, r1, l2, r2;
};

inline DualDeckResultOpt sinc_interpolate_dual_deck_opt(
    Track* tr1, double sample_pos1, int tr_len1, float pitch1,
    Track* tr2, double sample_pos2, int tr_len2, float pitch2)
{
    auto res1 = sinc_interpolate_track_opt(tr1, sample_pos1, tr_len1, pitch1);
    auto res2 = sinc_interpolate_track_opt(tr2, sample_pos2, tr_len2, pitch2);

    return {res1.left, res1.right, res2.left, res2.right};
}

} // namespace dsp
} // namespace sc
