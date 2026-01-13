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

#include <poll.h>
#include <pthread.h>
#include <semaphore.h>

class Controller;

/*
 * State data for the realtime thread, maintained during start() and
 * stop()
 */

struct Rt {
    pthread_t ph;
    sem_t sem;
    bool finished;
    int priority;

    struct Sc1000 *engine;

    size_t nctl;
    Controller *ctl[3];

    size_t npt;
    struct pollfd pt[32];

    // Member functions
    void init();
    void clear();
    int set_engine(struct Sc1000 *engine);
    int add_controller(Controller *c);
    int start(int priority);
    void stop();
};

int rt_global_init();
void rt_not_allowed();
