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


// Optimized cubic (Catmull-Rom) interpolation for SC1000 audio resampling
//
// Key optimizations (matching sinc_interpolate_opt.h):
// 1. Direct track pointer access - avoids get_sample() per tap
// 2. NEON SIMD for interpolation
// 3. Dual-deck parallel processing
//
// Cubic uses 4 taps vs sinc's 16, so simpler but same optimization pattern.

#pragma once

#include "../player/track.h"
#include <cmath>

// ARM NEON intrinsics
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#define CUBIC_OPT_USE_NEON 1
#else
#define CUBIC_OPT_USE_NEON 0
#endif

namespace sc {
namespace dsp {

// Use same constants as interpolate.h
constexpr int CUBIC_OPT_NUM_TAPS = 4;
constexpr int CUBIC_OPT_CENTER_OFFSET = 1;  // Sample window is [-1, 0, 1, 2] relative to position

//
// Direct track sample access for cubic (4 samples)
//
struct CubicSampleWindow {
    const signed short* samples;  // Direct pointer or nullptr if spanning blocks
    int start_sample;             // First sample index
    bool valid;                   // True if direct access possible
};

inline CubicSampleWindow get_cubic_sample_window(Track* tr, int center_sample, int tr_len) {
    CubicSampleWindow w;
    w.valid = false;

    if (tr_len == 0) return w;

    // Wrap center_sample to track bounds first (position can grow indefinitely)
    center_sample = center_sample % tr_len;
    if (center_sample < 0) center_sample += tr_len;

    // Cubic window: samples at positions [center-1, center, center+1, center+2]
    int start = center_sample - CUBIC_OPT_CENTER_OFFSET;
    int end = center_sample + (CUBIC_OPT_NUM_TAPS - CUBIC_OPT_CENTER_OFFSET) - 1;

    // Handle wrap-around and boundary conditions
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
    w.start_sample = start;
    w.samples = tr->block[start_block]->pcm + (start % TRACK_BLOCK_SAMPLES) * TRACK_CHANNELS;
    w.valid = true;

    return w;
}

//
// Cubic interpolation result
//
struct CubicResult {
    float left;
    float right;
};

//
// NEON-optimized cubic interpolation from direct pointer
//
#if CUBIC_OPT_USE_NEON

inline CubicResult cubic_interpolate_direct(const signed short* samples, float frac) {
    // Load 8 samples: [L0,R0,L1,R1,L2,R2,L3,R3]
    int16x8_t samp_i16 = vld1q_s16(samples);

    // Convert to float - keep interleaved format
    int32x4_t lo_i32 = vmovl_s16(vget_low_s16(samp_i16));   // [L0,R0,L1,R1]
    int32x4_t hi_i32 = vmovl_s16(vget_high_s16(samp_i16));  // [L2,R2,L3,R3]
    float32x4_t lo = vcvtq_f32_s32(lo_i32);  // [L0,R0,L1,R1]
    float32x4_t hi = vcvtq_f32_s32(hi_i32);  // [L2,R2,L3,R3]

    // Extract stereo pairs - no lane extraction needed!
    // t0 = [L0, R0], t1 = [L1, R1], t2 = [L2, R2], t3 = [L3, R3]
    float32x2_t t0 = vget_low_f32(lo);   // [L0, R0]
    float32x2_t t1 = vget_high_f32(lo);  // [L1, R1]
    float32x2_t t2 = vget_low_f32(hi);   // [L2, R2]
    float32x2_t t3 = vget_high_f32(hi);  // [L3, R3]

    // Catmull-Rom using NEON 2-wide vectors (L and R in parallel)
    float32x2_t mu = vdup_n_f32(frac);
    float32x2_t mu2 = vmul_f32(mu, mu);
    float32x2_t mu3 = vmul_f32(mu2, mu);

    float32x2_t half = vdup_n_f32(0.5f);
    float32x2_t two = vdup_n_f32(2.0f);
    float32x2_t three = vdup_n_f32(3.0f);
    float32x2_t four = vdup_n_f32(4.0f);
    float32x2_t five = vdup_n_f32(5.0f);

    // a0 = 0.5 * (-t0 + 3*t1 - 3*t2 + t3)
    float32x2_t a0 = vmul_f32(half,
        vadd_f32(vsub_f32(vsub_f32(vmul_f32(three, t1), vmul_f32(three, t2)), t0), t3));

    // a1 = 0.5 * (2*t0 - 5*t1 + 4*t2 - t3)
    float32x2_t a1 = vmul_f32(half,
        vsub_f32(vadd_f32(vsub_f32(vmul_f32(two, t0), vmul_f32(five, t1)), vmul_f32(four, t2)), t3));

    // a2 = 0.5 * (-t0 + t2)
    float32x2_t a2 = vmul_f32(half, vsub_f32(t2, t0));

    // a3 = t1
    float32x2_t a3 = t1;

    // result = a0*mu^3 + a1*mu^2 + a2*mu + a3
    float32x2_t result = vmla_f32(a3, a2, mu);       // a2*mu + a3
    result = vmla_f32(result, a1, mu2);              // a1*mu^2 + ...
    result = vmla_f32(result, a0, mu3);              // a0*mu^3 + ...

    return {vget_lane_f32(result, 0), vget_lane_f32(result, 1)};
}

#else  // !CUBIC_OPT_USE_NEON

inline CubicResult cubic_interpolate_direct(const signed short* samples, float frac) {
    // Load samples (interleaved stereo)
    float t0_l = static_cast<float>(samples[0]);
    float t0_r = static_cast<float>(samples[1]);
    float t1_l = static_cast<float>(samples[2]);
    float t1_r = static_cast<float>(samples[3]);
    float t2_l = static_cast<float>(samples[4]);
    float t2_r = static_cast<float>(samples[5]);
    float t3_l = static_cast<float>(samples[6]);
    float t3_r = static_cast<float>(samples[7]);

    float mu = frac;
    float mu2 = mu * mu;
    float mu3 = mu2 * mu;

    // Catmull-Rom for left
    float a0_l = 0.5f * (-t0_l + 3.0f*t1_l - 3.0f*t2_l + t3_l);
    float a1_l = 0.5f * (2.0f*t0_l - 5.0f*t1_l + 4.0f*t2_l - t3_l);
    float a2_l = 0.5f * (-t0_l + t2_l);
    float a3_l = t1_l;
    float out_l = a0_l*mu3 + a1_l*mu2 + a2_l*mu + a3_l;

    // Catmull-Rom for right
    float a0_r = 0.5f * (-t0_r + 3.0f*t1_r - 3.0f*t2_r + t3_r);
    float a1_r = 0.5f * (2.0f*t0_r - 5.0f*t1_r + 4.0f*t2_r - t3_r);
    float a2_r = 0.5f * (-t0_r + t2_r);
    float a3_r = t1_r;
    float out_r = a0_r*mu3 + a1_r*mu2 + a2_r*mu + a3_r;

    return {out_l, out_r};
}

#endif  // CUBIC_OPT_USE_NEON

//
// Slow path: collect samples through get_sample() for boundary cases
//
inline CubicResult cubic_interpolate_slow(Track* tr, int center_sample, int tr_len, float frac) {
    int start = center_sample - CUBIC_OPT_CENTER_OFFSET;

    float samples[8];  // 4 stereo pairs

    for (int i = 0; i < CUBIC_OPT_NUM_TAPS; ++i) {
        int idx = start + i;

        // Wrap to track boundary using modulo (O(1) instead of O(n) while loop)
        if (tr_len != 0) {
            idx = idx % tr_len;
            if (idx < 0) idx += tr_len;  // Handle negative modulo
        }

        if (idx >= 0 && idx < tr_len) {
            signed short* s = tr->get_sample(idx);
            samples[i * 2] = static_cast<float>(s[0]);
            samples[i * 2 + 1] = static_cast<float>(s[1]);
        } else {
            samples[i * 2] = 0.0f;
            samples[i * 2 + 1] = 0.0f;
        }
    }

    float t0_l = samples[0], t0_r = samples[1];
    float t1_l = samples[2], t1_r = samples[3];
    float t2_l = samples[4], t2_r = samples[5];
    float t3_l = samples[6], t3_r = samples[7];

    float mu = frac;
    float mu2 = mu * mu;
    float mu3 = mu2 * mu;

    float a0_l = 0.5f * (-t0_l + 3.0f*t1_l - 3.0f*t2_l + t3_l);
    float a1_l = 0.5f * (2.0f*t0_l - 5.0f*t1_l + 4.0f*t2_l - t3_l);
    float a2_l = 0.5f * (-t0_l + t2_l);
    float a3_l = t1_l;
    float out_l = a0_l*mu3 + a1_l*mu2 + a2_l*mu + a3_l;

    float a0_r = 0.5f * (-t0_r + 3.0f*t1_r - 3.0f*t2_r + t3_r);
    float a1_r = 0.5f * (2.0f*t0_r - 5.0f*t1_r + 4.0f*t2_r - t3_r);
    float a2_r = 0.5f * (-t0_r + t2_r);
    float a3_r = t1_r;
    float out_r = a0_r*mu3 + a1_r*mu2 + a2_r*mu + a3_r;

    return {out_l, out_r};
}

//
// Main optimized interpolation function for one deck
//
inline CubicResult cubic_interpolate_track_opt(
    Track* tr,
    double sample_pos,
    int tr_len)
{
    if (tr_len == 0) return {0.0f, 0.0f};

    // Note: sample_pos is expected to be pre-wrapped by caller (audio engine)
    // This avoids expensive fmod() on every sample

    // Get integer and fractional parts
    int center = static_cast<int>(sample_pos);
    if (sample_pos < 0.0) center--;
    float frac = static_cast<float>(sample_pos - center);

    // Try direct access first
    auto window = get_cubic_sample_window(tr, center, tr_len);

    if (window.valid) {
        // Fast path: direct interpolation from track memory
        return cubic_interpolate_direct(window.samples, frac);
    } else {
        // Slow path: collect samples through get_sample
        return cubic_interpolate_slow(tr, center, tr_len, frac);
    }
}

//
// Dual-deck optimized interpolation
//
struct DualDeckCubicResultOpt {
    float l1, r1, l2, r2;
};

inline DualDeckCubicResultOpt cubic_interpolate_dual_deck_opt(
    Track* tr1, double sample_pos1, int tr_len1,
    Track* tr2, double sample_pos2, int tr_len2)
{
    auto res1 = cubic_interpolate_track_opt(tr1, sample_pos1, tr_len1);
    auto res2 = cubic_interpolate_track_opt(tr2, sample_pos2, tr_len2);

    return {res1.left, res1.right, res2.left, res2.right};
}

} // namespace dsp
} // namespace sc
