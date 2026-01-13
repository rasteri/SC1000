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
 * Utility functions for launching external processes
 */

#ifndef EXTERNAL_H
#define EXTERNAL_H

#include <stdarg.h>
#include <unistd.h>

/*
 * A handy read buffer; an equivalent of fread() but for
 * non-blocking file descriptors
 */

struct rb {
    char buf[4096];
    size_t len;
};

pid_t fork_pipe(int *fd, const char *path, char *arg, ...);
pid_t fork_pipe_nb(int *fd, const char *path, char *arg, ...);

void rb_reset(struct rb *rb);
ssize_t get_line(int fd, struct rb *rb, char **string);

#endif
