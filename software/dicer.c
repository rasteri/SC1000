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
 * Specialised functions for the Novation Dicer controller
 *
 * The Dicer is a standard MIDI device, with buttons on input and the
 * corresponding LEDs on output. A single MIDI device consists of two
 * units, one for each turntable.
 *
 * Each unit has 5 buttons, but there are three 'pages' of buttons
 * controlled in the firmware, and then a shift mode for each. So we
 * see the full MIDI device as 60 possible buttons.
 */

#include <stdlib.h>

#include "controller.h"
#include "debug.h"
#include "deck.h"
#include "dicer.h"
#include "midi.h"
#include "realtime.h"

#define NBUTTONS 5

#define CUE 0
#define NOTE 1

/*#define LOOP 1
#define ROLL 2
#define NOTE 3*/

#define NUMDECKS 2

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

struct dicer
{
    struct midi midi;
    struct deck *decks[NUMDECKS];

    char obuf[180];
    size_t ofill;
};

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
	
	for (i = 0; i < NUMDECKS; i++){
		if (d->decks[i] == NULL){
			d->decks[i] = k;
			break;
		}
	}

    return 0;
}


/*
 * Act on an event, and update the given LED status
 */

static void event_decoded(struct deck *d,
                          unsigned char action, bool shift,
                          unsigned char button, bool on)
{

    if (shift && on)
    {
        deck_unset_cue(d, button);
		deck_cue(d, button); 
    }

    if (shift)
        return;

    if (action == CUE && on)
    {
        deck_cue(d, button); 
    }

    /*if (action == LOOP)
    {
        if (on)
        {
            deck_punch_in(d, button);
        }
        else
        {
            deck_punch_out(d);
        }
    }*/

    if (action == NOTE){
        //Middle is 0x3C
        
        d->player.nominal_pitch = pow(pow(2, (double)1/12), button - 0x3C); // equal temperament
        printf("Button: %x %f\n", button, d->player.nominal_pitch);
    }
}

/*
 * Process an event from the device, given the MIDI control codes
 */

static void event(struct dicer *d, unsigned char buf[3])
{
    unsigned char action, button;
    bool on, shift;
	struct deck *affdeck;

    switch (buf[0])
    {
    case 0x90:
        action = NOTE;
		affdeck = d->decks[0];
        break;
	case 0x91:
        action = NOTE;
		affdeck = d->decks[1];
        break;
    case 0x92:
        action = CUE;
		affdeck = d->decks[0];
        break;
	case 0x93:
        action = CUE;
		affdeck = d->decks[1];
        break;

    default:

        return;
    }

	button = buf[1];

    switch (buf[2])
    {
		case 0x00:
			on = false;
			break;

		default:
			on = true;
			break;
    }
	
	// If deck0 is shifted, engage shift
	shift = d->decks[0]->shifted;

    event_decoded(affdeck, action, shift, button, on);
	
	 d->decks[0]->shifted = 0;
}

static ssize_t pollfds(struct controller *c, struct pollfd *pe, size_t z)
{
    struct dicer *d = c->local;

    return midi_pollfds(&d->midi, pe, z);
}

/*
 * Handler in the realtime thread, which polls on both input
 * and output
 */

static int realtime(struct controller *c)
{
    struct dicer *d = c->local;

    for (;;)
    {
        unsigned char buf[3];
        ssize_t z;

        z = midi_read(&d->midi, buf, sizeof buf);
        if (z == -1)
            return -1;
        if (z == 0)
            break;

        debug("got event");

        event(d, buf);
    }


    return 0;
}

static void clear(struct controller *c)
{
    struct dicer *d = c->local;
    size_t n;

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
    size_t n;
    struct dicer *d;
	int i;

    printf("init %p from %s\n", c, hw);

    d = malloc(sizeof *d);
    if (d == NULL)
    {
        perror("malloc");
        return -1;
    }

    if (midi_open(&d->midi, hw) == -1)
        goto fail;

    d->ofill = 0;
	
	for (i = 0; i < NUMDECKS; i++){
		d->decks[i] = NULL;
	}


    if (controller_init(c, &dicer_ops, d, rt) == -1)
        goto fail_midi;

    return 0;

fail_midi:
    midi_close(&d->midi);
fail:
    free(d);
    return -1;
}
