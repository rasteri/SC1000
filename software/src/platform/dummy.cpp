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

#include "dummy.h"

static unsigned int sample_rate(struct Sc1000* /*d*/)
{
    return 48000;
}

static struct sc1000_ops dummy_ops = {
    .pollfds = nullptr,
    .handle = nullptr,
    .sample_rate = sample_rate,
    .start = nullptr,
    .stop = nullptr,
    .clear = nullptr,
};

void dummy_init(struct Sc1000* d)
{
    sc1000_audio_engine_init(d, &dummy_ops);
}
