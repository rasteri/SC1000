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

#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "thread.h"

static pthread_key_t key;

/*
 * Put in place checks for realtime and non-realtime threads
 *
 * Return: 0 on success, otherwise -1
 */

int thread_global_init(void)
{
    int r;

    r = pthread_key_create(&key, NULL);
    if (r != 0) {
        errno = r;
        perror("pthread_key_create");
        return -1;
    }

    if (pthread_setspecific(key, (void*)false) != 0)
        abort();

    return 0;
}

void thread_global_clear(void)
{
    if (pthread_key_delete(key) != 0)
        abort();
}

/*
 * Inform that this thread is a realtime thread, for assertions later
 */

void thread_to_realtime(void)
{
    if (pthread_setspecific(key, (void*)true) != 0)
        abort();
}

/*
 * Check for programmer error
 *
 * Pre: the current thread is non realtime
 */

void rt_not_allowed()
{
    bool rt;

    rt = (bool)pthread_getspecific(key);
    if (rt) {
        fprintf(stderr, "Realtime thread called a blocking function\n");
        abort();
    }
}
