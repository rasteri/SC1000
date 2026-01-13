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

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <pthread.h>

#include "thread.h"
#include "../util/log.h"

static pthread_key_t key;

/*
 * Put in place checks for realtime and non-realtime threads
 *
 * Return: 0 on success, otherwise -1
 */
int thread_global_init()
{
    int r;

    r = pthread_key_create(&key, nullptr);
    if (r != 0) {
        errno = r;
        perror("pthread_key_create");
        return -1;
    }

    if (pthread_setspecific(key, reinterpret_cast<void*>(false)) != 0) {
        abort();
    }

    return 0;
}

void thread_global_clear()
{
    if (pthread_key_delete(key) != 0) {
        abort();
    }
}

/*
 * Inform that this thread is a realtime thread, for assertions later
 */
void thread_to_realtime()
{
    if (pthread_setspecific(key, reinterpret_cast<void*>(true)) != 0) {
        abort();
    }
}

/*
 * Check for programmer error
 *
 * Pre: the current thread is non realtime
 */
void rt_not_allowed()
{
    bool rt = (pthread_getspecific(key) != nullptr);
    if (rt) {
        LOG_ERROR("Realtime thread called a blocking function");
        abort();
    }
}
