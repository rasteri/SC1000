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
 * Utility functions for launching external processes
 */

#pragma once

#include <cstdarg>
#include <unistd.h>

/*
 * A handy read buffer; an equivalent of fread() but for
 * non-blocking file descriptors
 */

struct Rb {
    char buf[4096];
    size_t len;
};

pid_t fork_pipe(int *fd, const char *path, char *arg, ...);
pid_t fork_pipe_nb(int *fd, const char *path, char *arg, ...);

void rb_reset(struct Rb *rb);
ssize_t get_line(int fd, struct Rb *rb, char **string);
