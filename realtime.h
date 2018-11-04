/*
 * Copyright (C) 2018 Mark Hills <mark@xwax.org>
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

#ifndef REALTIME_H
#define REALTIME_H

#include <poll.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>

/*
 * State data for the realtime thread, maintained during rt_start and
 * rt_stop
 */

struct rt {
    pthread_t ph;
    sem_t sem;
    bool finished;
    int priority;

    size_t ndv;
    struct device *dv[3];

    size_t nctl;
    struct controller *ctl[3];

    size_t npt;
    struct pollfd pt[32];
};

int rt_global_init();
void rt_not_allowed();

void rt_init(struct rt *rt);
void rt_clear(struct rt *rt);

int rt_add_device(struct rt *rt, struct device *dv);
int rt_add_controller(struct rt *rt, struct controller *c);

int rt_start(struct rt *rt, int priority);
void rt_stop(struct rt *rt);

#endif
