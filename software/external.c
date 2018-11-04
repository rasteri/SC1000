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

#define _DEFAULT_SOURCE /* vfork() */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "debug.h"
#include "external.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*x))

/*
 * Fork a child process, attaching stdout to the given pipe
 *
 * Return: -1 on error, or pid on success
 * Post: on success, *fd is file handle for reading
 */

static pid_t do_fork(int pp[2], const char *path, char *argv[])
{
    pid_t pid;

    pid = vfork();
    if (pid == -1) {
        perror("vfork");
        return -1;
    }

    if (pid == 0) { /* child */
        if (close(pp[0]) != 0)
            abort();

        if (dup2(pp[1], STDOUT_FILENO) == -1) {
            perror("dup2");
            _exit(EXIT_FAILURE); /* vfork() was used */
        }

        if (close(pp[1]) != 0)
            abort();

        if (execv(path, argv) == -1) {
            perror(path);
            _exit(EXIT_FAILURE); /* vfork() was used */
        }

        abort(); /* execv() does not return */
    }

    if (close(pp[1]) != 0)
        abort();

    return pid;
}

/*
 * Wrapper on do_fork which uses va_list
 *
 * The caller passes in the pipe for use, rather us handing one
 * back. This is because if the caller wishes to have a non-blocking
 * pipe, then the cleanup is messy if the process has already been
 * forked.
 */

static pid_t vext(int pp[2], const char *path, char *arg, va_list ap)
{
    char *args[16];
    size_t n;

    args[0] = arg;
    n = 1;

    /* Convert to an array; there's no va_list variant of exec() */

    for (;;) {
        char *x;

        x = va_arg(ap, char*);
        assert(n < ARRAY_SIZE(args));
        args[n++] = x;

        if (x == NULL)
            break;
    }

    return do_fork(pp, path, args);
}

/*
 * Fork a child process with stdout connected to this process
 * via a pipe
 *
 * Return: PID on success, otherwise -1
 * Post: on success, *fd is file descriptor for reading
 */

pid_t fork_pipe(int *fd, const char *path, char *arg, ...)
{
    int pp[2];
    pid_t r;
    va_list va;

    if (pipe(pp) == -1) {
        perror("pipe");
        return -1;
    }

    va_start(va, arg);
    r = vext(pp, path, arg, va);
    va_end(va);

    if (r == -1) {
        if (close(pp[0]) != 0)
            abort();
        if (close(pp[1]) != 0)
            abort();
    }

    *fd = pp[0];
    return r;
}

/*
 * Make the given file descriptor non-blocking
 *
 * Return: 0 on success, otherwise -1
 * Post: if 0 is returned, file descriptor is non-blocking
 */

static int make_non_blocking(int fd)
{
    if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1) {
        perror("fcntl");
        return -1;
    }

    return 0;
}

/*
 * Fork a child process with stdout connected to this process
 * via a non-blocking pipe
 *
 * Return: PID on success, otherwise -1
 * Post: on success, *fd is non-blocking file descriptor for reading
 */

pid_t fork_pipe_nb(int *fd, const char *path, char *arg, ...)
{
    int pp[2];
    pid_t r;
    va_list va;

    if (pipe(pp) == -1) {
        perror("pipe");
        return -1;
    }

    if (make_non_blocking(pp[0]) == -1)
        goto fail;

    va_start(va, arg);
    r = vext(pp, path, arg, va);
    va_end(va);

    assert(r != 0);
    if (r < 0)
        goto fail;

    *fd = pp[0];
    return r;

fail:
    if (close(pp[0]) != 0)
        abort();
    if (close(pp[1]) != 0)
        abort();

    return -1;
}

void rb_reset(struct rb *rb)
{
    rb->len = 0;
}

bool rb_is_full(const struct rb *rb)
{
    return (rb->len == sizeof rb->buf);
}

/*
 * Read, within reasonable limits (ie. memory or time)
 * from the fd into the buffer
 *
 * Return: -1 on error, 0 on EOF, otherwise the number of bytes added
 */

static ssize_t top_up(struct rb *rb, int fd)
{
    size_t remain;
    ssize_t z;

    assert(rb->len < sizeof rb->buf);
    remain = sizeof(rb->buf) - rb->len;

    z = read(fd, rb->buf + rb->len, remain);
    if (z == -1)
        return -1;

    rb->len += z;
    return z;
}

/*
 * Pop the front of the buffer to end-of-line
 *
 * Return: 0 if not found, -1 if not enough memory,
 *    otherwise string length (incl. terminator)
 * Post: if return is > 0, q points to alloc'd string
 */

static ssize_t pop(struct rb *rb, char **q)
{
    const char *x;
    char *s;
    size_t len;

    x = memchr(rb->buf, '\n', rb->len);
    if (!x) {
        debug("pop %p exhausted", rb);
        return 0;
    }

    len = x - rb->buf;
    debug("pop %p got %u", rb, len);

    s = strndup(rb->buf, len);
    if (!s) {
        debug("strndup: %s", strerror(errno));
        return -1;
    }

    *q = s;

    /* Simple compact of the buffer. If this is a bottleneck of any
     * kind (unlikely) then a circular buffer should be used */

    memmove(rb->buf, x + 1, rb->len - len - 1);
    rb->len = rb->len - len - 1;

    return len + 1;
}

/*
 * Read a terminated string from the given file descriptor via
 * the buffer.
 *
 * Handles non-blocking file descriptors too. If fd is non-blocking,
 * then the semantics are the same as a non-blocking read() --
 * ie. EAGAIN may be returned as an error.
 *
 * Return: 0 on EOF, or -1 on error
 * Post: if -1 is returned, errno is set accordingly
 */

ssize_t get_line(int fd, struct rb *rb, char **string)
{
    ssize_t y, z;

    y = top_up(rb, fd);
    if (y < 0)
        return y;

    z = pop(rb, string);
    if (z != 0)
        return z;

    if (rb_is_full(rb))
        errno = ENOBUFS;
    else if (y > 0)
        errno = EAGAIN;
    else
        return 0; /* true EOF: no more data and empty buffer */

    return -1;
}
