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


//
// Loop Buffer - records audio input into memory for immediate scratching
//
// Workflow:
// 1. First RECORD: captures audio, defines loop length when stopped
// 2. Subsequent RECORDs: punch-in mode, overwrites circularly from current position
// 3. Long-hold RECORD: resets/erases, next RECORD starts fresh
//

#include "loop_buffer.h"
#include "../player/track.h"
#include <cstring>
#include <cstdio>

void loop_buffer_init(struct LoopBuffer* lb, int sample_rate, int max_seconds)
{
    lb->write_pos = 0;
    lb->max_samples = static_cast<unsigned int>(sample_rate * max_seconds);
    lb->loop_length = 0;
    lb->sample_rate = sample_rate;
    lb->recording = false;
    lb->length_locked = false;
    lb->max_reached = false;

    // Pre-allocate track with full capacity to avoid RT allocation
    lb->track = track_acquire_for_recording(sample_rate);
    if (lb->track)
    {
        // Pre-allocate all the space we'll need
        if (lb->track->ensure_space(lb->max_samples) < 0)
        {
            fprintf(stderr, "LoopBuffer: failed to pre-allocate %u samples\n", lb->max_samples);
            track_release(lb->track);
            lb->track = nullptr;
        }
        else
        {
            printf("LoopBuffer: pre-allocated %u samples (%.1f sec)\n",
                   lb->max_samples, static_cast<float>(lb->max_samples) / sample_rate);
        }
    }
    else
    {
        fprintf(stderr, "LoopBuffer: failed to create track\n");
    }
}

void loop_buffer_clear(struct LoopBuffer* lb)
{
    if (lb->track)
    {
        track_release(lb->track);
        lb->track = nullptr;
    }
    lb->write_pos = 0;
    lb->loop_length = 0;
    lb->recording = false;
    lb->length_locked = false;
    lb->max_reached = false;
}

bool loop_buffer_start(struct LoopBuffer* lb)
{
    if (lb->recording)
    {
        return false;  // Already recording
    }

    if (!lb->track)
    {
        fprintf(stderr, "LoopBuffer: no track available (not pre-allocated)\n");
        return false;
    }

    if (lb->length_locked)
    {
        // Punch-in mode: keep existing data, start overwriting from current write_pos
        lb->recording = true;
        lb->max_reached = false;
        printf("LoopBuffer: punch-in recording started at pos %u (loop length %u samples, %.2f sec)\n",
               lb->write_pos, lb->loop_length,
               static_cast<float>(lb->loop_length) / lb->sample_rate);
        return true;
    }

    // Fresh recording: reset state but reuse pre-allocated track
    lb->write_pos = 0;
    lb->loop_length = 0;
    lb->length_locked = false;
    lb->max_reached = false;
    lb->recording = true;

    printf("LoopBuffer: fresh recording started (max %u samples)\n", lb->max_samples);
    return true;
}

void loop_buffer_stop(struct LoopBuffer* lb)
{
    if (!lb->recording)
    {
        return;
    }

    lb->recording = false;

    if (!lb->length_locked)
    {
        // First recording complete - lock the loop length
        if (lb->track && lb->write_pos > 0)
        {
            lb->loop_length = lb->write_pos;
            lb->length_locked = true;
            lb->track->set_length(lb->loop_length);
            printf("LoopBuffer: loop defined, %u samples (%.2f sec)\n",
                   lb->loop_length, static_cast<float>(lb->loop_length) / lb->sample_rate);
        }
        else
        {
            printf("LoopBuffer: recording stopped (empty)\n");
        }
    }
    else
    {
        // Punch-in complete
        printf("LoopBuffer: punch-in stopped at pos %u\n", lb->write_pos);
    }
}

// Helper to convert float [-1, 1] to S16
static inline int16_t float_to_s16(float sample)
{
    // Clamp and scale
    float clamped = sample;
    if (clamped > 1.0f) clamped = 1.0f;
    if (clamped < -1.0f) clamped = -1.0f;
    return static_cast<int16_t>(clamped * 32767.0f);
}

unsigned int loop_buffer_write_float(struct LoopBuffer* lb,
                                     float left,
                                     float right)
{
    if (!lb->recording || !lb->track)
    {
        return 0;
    }

    if (lb->length_locked)
    {
        // Punch-in mode: write circularly within loop_length
        if (lb->loop_length == 0)
        {
            return 0;  // Shouldn't happen, but safety check
        }

        unsigned int pos = lb->write_pos % lb->loop_length;
        signed short* dest = lb->track->get_sample(static_cast<int>(pos));

        dest[0] = float_to_s16(left);
        dest[1] = float_to_s16(right);

        lb->write_pos++;
        // Wrap write_pos to avoid overflow over time
        lb->write_pos = lb->write_pos % lb->loop_length;
    }
    else
    {
        // Fresh recording: linear write until max (space is pre-allocated)
        unsigned int remaining = lb->max_samples - lb->write_pos;
        if (remaining == 0)
        {
            if (!lb->max_reached)
            {
                lb->max_reached = true;
                printf("LoopBuffer: max length reached\n");
            }
            return 0;
        }

        // Write sample to track (space is pre-allocated, no RT allocation)
        signed short* dest = lb->track->get_sample(static_cast<int>(lb->write_pos));

        dest[0] = float_to_s16(left);
        dest[1] = float_to_s16(right);

        lb->write_pos++;

        // Update track length as we go (allows scratching while recording)
        lb->track->set_length(lb->write_pos);
    }

    return 1;
}

Track* loop_buffer_get_track(struct LoopBuffer* lb)
{
    if (!lb->track)
    {
        return nullptr;
    }

    unsigned int len = lb->length_locked ? lb->loop_length : lb->write_pos;
    if (len == 0)
    {
        return nullptr;
    }

    // Acquire a reference for the caller
    track_acquire(lb->track);
    return lb->track;
}

bool loop_buffer_is_recording(struct LoopBuffer* lb)
{
    return lb->recording;
}

bool loop_buffer_has_loop(struct LoopBuffer* lb)
{
    return lb->length_locked && lb->loop_length > 0;
}

unsigned int loop_buffer_get_length(struct LoopBuffer* lb)
{
    return lb->length_locked ? lb->loop_length : lb->write_pos;
}

void loop_buffer_reset(struct LoopBuffer* lb)
{
    // Stop recording if active
    if (lb->recording)
    {
        lb->recording = false;
    }

    // Reset state but keep pre-allocated track
    lb->write_pos = 0;
    lb->loop_length = 0;
    lb->length_locked = false;
    lb->max_reached = false;

    printf("LoopBuffer: reset/erased\n");
}

void loop_buffer_set_position(struct LoopBuffer* lb, unsigned int position_samples)
{
    if (!lb->length_locked || lb->loop_length == 0)
    {
        // No loop defined, can't set position
        return;
    }

    // Clamp to valid range
    lb->write_pos = position_samples % lb->loop_length;
}
