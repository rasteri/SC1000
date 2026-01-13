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

#include <stdlib.h>

#include "controller.h"
#include "debug.h"
#include "deck.h"
#include "dicer.h"
#include "midi.h"
#include "realtime.h"
#include "sc_midimap.h"
#include "xwax.h"
#define NBUTTONS 5

#define CUE 0
#define NOTE 1

/*#define LOOP 1
#define ROLL 2
#define NOTE 3*/

#ifdef DEBUG
static const char *actions[] = {
	"CUE",
	"LOOP",
	"ROLL"};
#endif

/* LED states */

typedef unsigned char led_t;

#define ON 0x1
#define PRESSED 0x2
#define SYNCED 0x4

extern struct mapping *maps;
extern bool shifted;

/*
 * Add a deck to the dicer or pair of dicer
 *
 * Return: -1 if the deck could not be added, otherwise zero
 */

static int add_deck(struct controller *c, struct deck *k)
{
	struct dicer *d = c->local;
	int i;

	debug("%p add deck %p", d, k);

	for (i = 0; i < NUMDECKS; i++)
	{
		if (d->decks[i] == NULL)
		{
			d->decks[i] = k;
			break;
		}
	}

	return 0;
}

/*
 * Process an event from the device, given the MIDI control codes
 */
 
 // This is so dumb

static void event(struct dicer *d)
{
	printf("%x %x %x\n",d->MidiBuffer[0], d->MidiBuffer[1], d->MidiBuffer[2]);
		
	// This is so dumb, there should be a proper event buffer, but oh well
	QueuedMidiCommand = find_MIDI_mapping(maps, d->MidiBuffer, shifted ? 3 : 1);
	QueuedMidiBuffer[0] = d->MidiBuffer[0];
	QueuedMidiBuffer[1] = d->MidiBuffer[1];
	QueuedMidiBuffer[2] = d->MidiBuffer[2];


/*
	if (map != NULL)
	{
		//		printf("Map notnull %d %d %d\n", map->DeckNo, map->Action, map->Param);

		if (map->Action == ACTION_CUE)
		{
			if (d->shifted)
				deck_unset_cue(d->decks[map->DeckNo], map->MidiBytes[1]);
			else
				deck_cue(d->decks[map->DeckNo], map->MidiBytes[1]);
		}
		else if (map->Action == ACTION_NOTE)
		{
			d->decks[map->DeckNo]->player.nominal_pitch = pow(pow(2, (double)1 / 12), map->Param - 0x3C); // equal temperament
		}
		else if (map->Action == ACTION_STARTSTOP)
		{
			d->decks[map->DeckNo]->player.stopped = !d->decks[map->DeckNo]->player.stopped;
		}
		else if (map->Action == ACTION_SHIFTON)
		{
			d->shifted = 1;
		}
		else if (map->Action == ACTION_SHIFTOFF)
		{
			d->shifted = 0;
		}
		else if (map->Action == ACTION_PITCH)
		{
			double pitch = 0.0;
			// If this came from a pitch bend message, use 14 bit accuracy
			if ((d->MidiBuffer[0] & 0xF0) == 0xE0)
			{
				pval = (((unsigned int)d->MidiBuffer[2]) << 7) | ((unsigned int)d->MidiBuffer[1]);
				pitch = (((double)pval - 8192.0) * ((double)scsettings.pitchrange / 819200.0)) + 1;
			}
			// Otherwise 7bit (boo)
			else
			{
				pitch = (((double)d->MidiBuffer[2] - 64.0) * ((double)scsettings.pitchrange / 6400.0) + 1);
			}

			d->decks[map->DeckNo]->player.nominal_pitch = pitch;
		}
	}*/
}

static ssize_t pollfds(struct controller *c, struct pollfd *pe, size_t z)
{
	struct dicer *d = c->local;

	return midi_pollfds(&d->midi, pe, z);
}

/*
 * Handler in the realtime thread, which polls on both input
 * and output
 * Modified MIDI parser to be re-entrant
 */

static int realtime(struct controller *c)
{
	struct dicer *d = c->local;
	for (;;)
	{
		unsigned char buf;
		unsigned char command;
		ssize_t z;

		z = midi_read(&d->midi, &buf, 1);
		if (z == -1)
			return -1;
		if (z == 0)
			return 0;

		// Bit 7 set, this is a status byte
		if (buf & 0x80)
		{
			command = buf & 0xF0;
			if (command == 0x80 || command == 0x90 || command == 0xB0 || command == 0xE0)
			{
				d->parsing = 1;
				d->MidiBuffer[0] = buf;
			}
			else
				d->parsing = 0;
		}

		// Bit 7 unset, this is a data byte
		else
		{
			// If we're currently in a MIDI message, add to buffer
			if (d->parsing)
			{
				d->MidiBuffer[++(d->ParsedBytes)] = buf;

				// If we've reached the second byte, process
				if (d->ParsedBytes == 2)
				{
					d->parsing = 0;
					d->ParsedBytes = 0;
					event(d);
				}
			}
		}

		debug("got event");
	}

	return 0;
}

static void clear(struct controller *c)
{
	struct dicer *d = c->local;

	debug("%p", d);

	midi_close(&d->midi);
	free(c->local);
}

static struct controller_ops dicer_ops = {
	.add_deck = add_deck,
	.pollfds = pollfds,
	.realtime = realtime,
	.clear = clear,
};

int dicer_init(struct controller *c, struct rt *rt, const char *hw)
{
	struct dicer *d;
	int i;

	printf("init %p from %s\n", c, hw);

	d = malloc(sizeof *d);
	if (d == NULL)
	{
		perror("malloc");
		return -1;
	}

	strcpy(d->PortName, hw);

	if (midi_open(&d->midi, hw) == -1)
		goto fail;

	d->ofill = 0;

	for (i = 0; i < NUMDECKS; i++)
	{
		d->decks[i] = NULL;
	}

	if (controller_init(c, &dicer_ops, d, rt) == -1)
		goto fail_midi;

	d->shifted = 0;
	d->parsing = 0;
	d->ParsedBytes = 0;

	return 0;

fail_midi:
	midi_close(&d->midi);
fail:
	free(d);
	return -1;
}
