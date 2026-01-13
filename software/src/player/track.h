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

#pragma once

#include <cstddef>
#include <sys/poll.h>
#include <sys/types.h>

constexpr int TRACK_CHANNELS = 2;
constexpr int TRACK_MAX_BLOCKS = 64;
constexpr int TRACK_BLOCK_SAMPLES = 2048 * 1024;
constexpr int TRACK_PPM_RES = 64;
constexpr int TRACK_OVERVIEW_RES = 2048;

struct TrackBlock {
    signed short pcm[TRACK_BLOCK_SAMPLES * TRACK_CHANNELS];
};

struct Track {
    unsigned int refcount;
    int rate;

    // Pointers to external data (owned by caller)
    const char* importer;
    const char* path;

    size_t bytes;           // Bytes loaded
    unsigned int length;    // Track length in samples
    unsigned int blocks;    // Number of blocks allocated
    TrackBlock* block[TRACK_MAX_BLOCKS];

    // State of audio import
    pid_t pid;
    int fd;
    struct pollfd* pe;
    bool terminated;
    bool finished;

    // Return true if the track importer is running
    bool is_importing() const { return pid != 0; }

    // Return a pointer to the sample data at position s
    signed short* get_sample(int s) {
        TrackBlock* b = block[s / TRACK_BLOCK_SAMPLES];
        return &b->pcm[(s % TRACK_BLOCK_SAMPLES) * TRACK_CHANNELS];
    }

    // Ensure track has enough space for the given number of samples
    int ensure_space(unsigned int samples);

    // Set track length (for direct recording)
    void set_length(unsigned int samples);

    // Get entry for use by poll()
    void pollfd(struct pollfd* poll_entry);

    // Handle any file descriptor activity on this track
    void handle();
};

// Enable memory locking for track allocations
void track_use_mlock();

// Track acquisition functions (reference counted)
Track* track_acquire_by_import(const char* importer, const char* path);
Track* track_acquire_empty();
Track* track_acquire_for_recording(int sample_rate);
void track_acquire(Track* t);
void track_release(Track* t);

