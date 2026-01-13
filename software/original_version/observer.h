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

/*
 * Implementation of "observer pattern"
 *
 * There are several cases in the code where we need to notify
 * when something changes (eg. to update a UI.)
 *
 * The use of simple function calls is problematic because it creates
 * cyclical dependencies in header files, and is not sufficiently
 * modular to allow the code to be re-used in a self-contained test.
 *
 * So, reluctantly introduce a slots and signals concept; xwax is
 * getting to be quite a lot of code and structure now.
 */

#ifndef OBSERVE_H
#define OBSERVE_H

#include <assert.h>

#include "list.h"

struct event {
    struct list observers;
};

struct observer {
    struct list event;
    void (*func)(struct observer*, void*);
};

#define EVENT_INIT(event) { \
    .observers = LIST_INIT(event.observers) \
}

static inline void event_init(struct event *s)
{
    list_init(&s->observers);
}

static inline void event_clear(struct event *s)
{
    assert(list_empty(&s->observers));
}

/*
 * Pre: observer is not watching anything
 * Post: observer is watching the given event
 */

static inline void watch(struct observer *observer, struct event *sig,
                         void (*func)(struct observer*, void*))
{
    list_add(&observer->event, &sig->observers);
    observer->func = func;
}

static inline void ignore(struct observer *observer)
{
    list_del(&observer->event);
}

/*
 * Call the callback in all slots which are watching the given event
 */

static inline void fire(struct event *s, void *data)
{
    struct observer *t;

    list_for_each(t, &s->observers, event) {
        assert(t->func != NULL);
        t->func(t, data);
    }
}

#endif
