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

#ifndef DICER_H
#define DICER_H

#include "midi.h"

#define NUMDECKS 2

struct controller;
struct rt;

struct dicer
{
    struct midi midi;
    struct deck *decks[NUMDECKS];

    char obuf[180];
    size_t ofill;
	bool shifted;
	
	bool parsing;
	unsigned char ParsedBytes;
	unsigned char MidiBuffer[3];

    char PortName[32];
};

int dicer_init(struct controller *c, struct rt *rt, const char *hw);

#endif
