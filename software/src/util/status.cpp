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
#include <cstdarg>

#include "status.h"

static const char *message = "";
static int level = 0;

/*
 * Return: current status string
 */

const char* status()
{
    return message;
}

int status_level()
{
    return level;
}

/*
 * Set status to reference a static string
 *
 * Post: reference on s is held
 */

void status_set(int l, const char *s)
{
    message = s;
    level = l;

    if (l >= STATUS_INFO) {
        fputs(s, stderr);
        fputc('\n', stderr);
    }
}

/*
 * Set status to a formatted string
 */

void status_printf(int lvl, const char *t, ...)
{
    static char buf[256];
    va_list l;

    va_start(l, t);
    vsnprintf(buf, sizeof buf, t, l);
    va_end(l);

    status_set(lvl, buf);
}
