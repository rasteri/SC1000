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

#include "../player/track.h"
#include <vector>
#include <pthread.h>

/*
 * The rig handles non-realtime I/O operations (track importing).
 * It runs the main event loop and manages file descriptor polling.
 */
struct Rig {
    int event[2];                       // pipe to wake up service thread
    std::vector<Track*> importing_tracks;
    pthread_mutex_t lock;

    // Initialize the rig (creates event pipe, initializes mutex)
    int init();

    // Clean up resources
    void clear();

    // Run the main event loop (blocks until quit)
    int main();

    // Request the rig to exit from another thread
    int quit();

    // Lock/unlock for thread-safe operations
    void acquire_lock();
    void release_lock();

    // Track import management
    void post_track(Track* t);
    void remove_track(Track* t);

private:
    int post_event(char e);
};

// Global rig instance (defined in rig.cpp)
extern struct Rig g_rig;
