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

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "midi.h"
#include "../util/log.h"

int listdev(const char* devname, char names[64][64])
{
    char** hints;
    int err;
    char** n;
    char* name;
    int num = 0;

    /* Enumerate sound devices */
    err = snd_device_name_hint(-1, devname, reinterpret_cast<void***>(&hints));
    if (err != 0) {
        LOG_ERROR("Cannot get device names");
        exit(1);
    }

    n = hints;
    while (*n != nullptr) {
        name = snd_device_name_get_hint(*n, "NAME");

        strcpy(names[num++], name);

        LOG_DEBUG("MIDI device: %s", name);

        if (name != nullptr && strcmp("null", name) != 0) {
            free(name);
        }
        n++;
    }

    // Free hint buffer too
    snd_device_name_free_hint(reinterpret_cast<void**>(hints));
    return num;
}

/*
 * Print error code from ALSA
 */
static void alsa_error(const char* msg, int r)
{
    LOG_ERROR("ALSA %s: %s", msg, snd_strerror(r));
}

int midi_open(struct Midi* m, const char* name)
{
    int r;

    r = snd_rawmidi_open(&m->in, &m->out, name, SND_RAWMIDI_NONBLOCK);
    if (r < 0) {
        alsa_error("rawmidi_open", r);
        return -1;
    }

    return 0;
}

void midi_close(struct Midi* m)
{
    if (snd_rawmidi_close(m->in) < 0) {
        abort();
    }
    if (snd_rawmidi_close(m->out) < 0) {
        abort();
    }
}

/*
 * Get the poll descriptors for reading on this MIDI device
 *
 * Pre: len is maximum size of array pe
 * Return: -1 if len is not large enough, otherwise n on success
 * Post: on success, pe is filled with n entries
 */
ssize_t midi_pollfds(struct Midi* m, struct pollfd* pe, size_t len)
{
    int r;

    if (static_cast<size_t>(snd_rawmidi_poll_descriptors_count(m->in)) > len) {
        return -1;
    }

    r = snd_rawmidi_poll_descriptors(m->in, pe, static_cast<unsigned int>(len));
    assert(r >= 0);

    return r;
}

/*
 * Read raw bytes of input
 *
 * Pre: len is maximum size of buffer
 * Return: -1 on error, otherwise n on success
 * Post: on success, buf is filled with n bytes of data
 */
ssize_t midi_read(struct Midi* m, void* buf, size_t len)
{
    ssize_t r;

    r = snd_rawmidi_read(m->in, buf, len);
    if (r < 0) {
        if (r == -EAGAIN) {
            return 0;
        }
        alsa_error("rawmidi_read", static_cast<int>(r));
        return -1;
    }

    return r;
}

ssize_t midi_write(struct Midi* m, const void* buf, size_t len)
{
    ssize_t r;

    r = snd_rawmidi_write(m->out, buf, len);
    if (r < 0) {
        if (r == -EAGAIN) {
            return 0;
        }
        alsa_error("rawmidi_write", static_cast<int>(r));
        return -1;
    }

    return r;
}
