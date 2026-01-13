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

#include <cstdio>

#include "../util/debug.h"
#include "../player/deck.h"

#include "controller.h"

/*
 * Add a deck to this controller, if possible
 */
void controller_add_deck(Controller* c, struct Deck* d)
{
    debug("%p adding deck %p", c, d);

    if (c->add_deck(d) == 0) {
        debug("deck was added");
    }
}

/*
 * Get file descriptors which should be polled for this controller
 *
 * Return: the number of pollfd filled, or -1 on error
 */
ssize_t controller_pollfds(Controller* c, struct pollfd* pe, size_t z)
{
    return c->pollfds(pe, z);
}

void controller_handle(Controller* c)
{
    if (c->has_fault()) {
        return;
    }

    if (c->realtime() != 0) {
        c->set_fault();
        fputs("Error handling hardware controller; disabling it\n", stderr);
    }
}

void controller_clear(Controller* c)
{
    debug("%p", c);
    c->clear();
}
