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

#ifndef MIDI_H
#define MIDI_H

#include <alsa/asoundlib.h>

/*
 * State information for an open, non-blocking MIDI device
 */

struct midi {
    snd_rawmidi_t *in, *out;
};

int midi_open(struct midi *m, const char *name);
void midi_close(struct midi *m);

ssize_t midi_pollfds(struct midi *m, struct pollfd *pe, size_t len);
ssize_t midi_read(struct midi *m, void *buf, size_t len);
ssize_t midi_write(struct midi *m, const void *buf, size_t len);
int listdev(char *devname, char names[64][64]);

#endif
