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

/*
 * Spinlock routines for synchronising with the realtime thread
 */

#pragma once

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <pthread.h>

#include "realtime.h"

using spin = pthread_spinlock_t;

inline void spin_init(spin *s)
{
    if (pthread_spin_init(s, PTHREAD_PROCESS_PRIVATE) != 0)
        abort();
}

inline void spin_clear(spin *s)
{
    if (pthread_spin_destroy(s) != 0)
        abort();
}

/*
 * Take a spinlock
 *
 * Pre: lock is initialised
 * Pre: lock is not held by the current thread
 * Pre: current thread is not realtime
 * Post: lock is held by the current thread
 */

inline void spin_lock(spin *s)
{
    rt_not_allowed();

    if (pthread_spin_lock(s) != 0)
        abort();
}

/*
 * Try and take a spinlock
 *
 * Pre: lock is initialised
 * Pre: lock is not held by the current thread
 * Post: if true is returned, lock is held by the current thread
 */

inline bool spin_try_lock(spin *s)
{
    int r;

    r = pthread_spin_trylock(s);
    switch (r) {
    case 0:
        return true;
    case EBUSY:
        return false;
    default:
        abort();
    }
}

/*
 * Release a spinlock
 *
 * Pre: lock is held by this thread
 * Post: lock is not held
 */

inline void spin_unlock(spin *s)
{
    if (pthread_spin_unlock(s) != 0)
        abort();
}
