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

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <fcntl.h>
#include <unistd.h>
#include <sys/poll.h>

#include "mutex.h"
#include "realtime.h"
#include "rig.h"
#include "../util/log.h"

#define EVENT_WAKE 0
#define EVENT_QUIT 1

// Global rig instance
struct Rig g_rig;

//
// rig member function implementations
//

int Rig::init()
{
    /* Create a pipe which will be used to wake us from other threads */

    if (pipe(event) == -1) {
        perror("pipe");
        return -1;
    }

    if (fcntl(event[0], F_SETFL, O_NONBLOCK) == -1) {
        perror("fcntl");
        if (close(event[1]) == -1)
            abort();
        if (close(event[0]) == -1)
            abort();
        return -1;
    }

    mutex_init(&lock);

    return 0;
}

void Rig::clear()
{
    mutex_clear(&lock);

    if (close(event[0]) == -1)
        abort();
    if (close(event[1]) == -1)
        abort();
}

/*
 * Main thread which handles input and output
 *
 * The rig is the main thread of execution. It is responsible for all
 * non-priority event driven operations (eg. everything but audio).
 */

int Rig::main()
{
    constexpr size_t MAX_POLL_ENTRIES = 4;
    struct pollfd pt[MAX_POLL_ENTRIES];

    /* Monitor event pipe from external threads */

    pt[0].fd = event[0];
    pt[0].revents = 0;
    pt[0].events = POLLIN;

    mutex_lock(&lock);

    for (;;) { /* exit via EVENT_QUIT */
        int r;
        size_t poll_count = 1;  // Start after the event pipe entry

        /* Set up poll entries for importing tracks */
        for (Track* t : importing_tracks) {
            if (poll_count >= MAX_POLL_ENTRIES)
                break;
            t->pollfd(&pt[poll_count]);
            poll_count++;
        }

        mutex_unlock(&lock);

        r = poll(pt, static_cast<nfds_t>(poll_count), -1);
        if (r == -1) {
            if (errno == EINTR) {
                mutex_lock(&lock);
                continue;
            } else {
                perror("poll");
                return -1;
            }
        }

        /* Process all events on the event pipe */

        if (pt[0].revents != 0) {
            for (;;) {
                char e;
                ssize_t z;

                z = read(event[0], &e, 1);
                if (z == -1) {
                    if (errno == EAGAIN) {
                        break;
                    } else {
                        perror("read");
                        return -1;
                    }
                }

                switch (e) {
                case EVENT_WAKE:
                    break;

                case EVENT_QUIT:
                    goto finish;

                default:
                    abort();
                }
            }
        }

        mutex_lock(&lock);

        /* Flush any RT log messages */
        sc::log::flush_rt_logs();

        /* Handle track events - iterate with index for safe removal */
        for (size_t i = 0; i < importing_tracks.size(); ) {
            Track* t = importing_tracks[i];
            bool was_importing = t->is_importing();

            t->handle();

            // If track finished importing, it was removed from the vector
            // by remove_track(), so don't increment index
            if (was_importing && !t->is_importing()) {
                // Track was removed, don't increment i
            } else {
                i++;
            }
        }
    }
finish:

    return 0;
}

/*
 * Post a simple event into the rig event loop
 */

int Rig::post_event(char e)
{
    rt_not_allowed();

    if (write(event[1], &e, 1) == -1) {
        perror("write");
        return -1;
    }

    return 0;
}

/*
 * Ask the rig to exit from another thread or signal handler
 */

int Rig::quit()
{
    return post_event(EVENT_QUIT);
}

void Rig::acquire_lock()
{
    mutex_lock(&lock);
}

void Rig::release_lock()
{
    mutex_unlock(&lock);
}

/*
 * Add a track to be handled until import has completed
 */

void Rig::post_track(Track* t)
{
    track_acquire(t);
    importing_tracks.push_back(t);
    post_event(EVENT_WAKE);
}

/*
 * Remove a track from the import list (called when import completes)
 */

void Rig::remove_track(Track* t)
{
    auto it = std::find(importing_tracks.begin(), importing_tracks.end(), t);
    if (it != importing_tracks.end()) {
        importing_tracks.erase(it);
    }
}
