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
 * Implement a global one-line status console
 */

#ifndef STATUS_H
#define STATUS_H

#include <stdarg.h>

#include "observer.h"

#define STATUS_VERBOSE 0
#define STATUS_INFO    1
#define STATUS_WARN    2
#define STATUS_ALERT   3

extern struct event status_changed;

const char* status(void);
int status_level(void);

void status_set(int level, const char *s);
void status_printf(int level, const char *s, ...);

#endif
