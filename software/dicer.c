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
#define LOOP 1
#define ROLL 2
#define NOTE 3

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
    struct deck *deck;

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

    debug("%p add deck %p", d, k);

    d->deck = k;

    return 0;
}

/*
 * Write a MIDI command sequence which would bring the given LED
 * up-to-date
 *
 * Return: n, or -1 if not enough buffer space
 * Post: if buffer space is available, n bytes are written to buf
 */

/*static ssize_t led_cmd(led_t led, char *buf, size_t len,
                       bool right, unsigned char action,
                       bool shift, unsigned char button)
{
    if (len < 3)
        return -1;

    assert(action <= ROLL);
    buf[0] = (right ? 0x9d : 0x9a) + action;

    assert(button < NBUTTONS);
    buf[1] = (shift ? 0x41 : 0x3c) + button;


    switch (action) {
    case CUE:
        buf[2] = 0x00;
        break;

    case LOOP:
        buf[2] = 0x70;
        break;

    case ROLL:
        buf[2] = 0x40;
        break;

    default:
        abort();
    }

    if (led & ON)
        buf[2] += 0xa;
    if (led & PRESSED)
        buf[2] += 0x5;

    debug("compiling LED command: %02hhx %02hhx %02hhx",
          buf[0], buf[1], buf[2]);

    return 3;
}*/

/*
 * Push control code for a particular output LED
 *
 * Return: n, or -1 if not enough buffer space
 * Post: if buf is large enough, LED is synced and n bytes are written
 */

/*static ssize_t sync_one_led(led_t *led, char *buf, size_t len,
                            bool right, unsigned char button)
{
    unsigned int a;
    size_t t;

    if (*led & SYNCED)
        return 0;

    debug("syncing LED: %s %d", right ? "right" : "left", button);

    t = 0;

    for (a = 0; a <= ROLL; a++) {
        ssize_t z;

        z = led_cmd(*led, buf, len, right, a, false, button);
        if (z == -1)
            return -1;

        buf += z;
        len -= z;
        t += z;

        z = led_cmd(*led, buf, len, right, a, true, button);
        if (z == -1)
            return -1;

        buf += z;
        len -= z;
        t += z;
    }

    *led |= SYNCED;

    return t;
}*/

/*
 * Return: number of bytes written to the buffer
 */

/*static size_t sync_one_dicer(led_t led[NBUTTONS], bool right,
                             char *buf, size_t len)
{
    size_t n, t;

    t = 0;

    for (n = 0; n < NBUTTONS; n++) {
        ssize_t z;

        z = sync_one_led(&led[n], buf, len, right, n);
        if (z == -1) {
            debug("output buffer full; expect incorrect LEDs");
            break;
        }

        buf += z;
        len -= z;
        t += z;
    }

    return t;
}*/

/*
 * Write a MIDI command sequence to sync all hardware LEDs with the
 * state held in memory.
 *
 * The Dicer first appears to only have five output LEDs on two
 * controllers. But there are three modes for each, and then shift
 * on/off modes: total (5 * 2 * 3 * 2) = 60
 */

/*static void sync_all_leds(struct dicer *d)
{
    size_t w;
    char *buf;
    size_t len;

    buf = d->obuf + d->ofill;
    len = sizeof(d->obuf) - d->ofill;


    w = sync_one_dicer(d->left_led, false, buf, len);
    buf += w;
    len -= w;
    d->ofill += w;

    w = sync_one_dicer(d->right_led, true, buf, len);
    buf += w;
    len -= w;
    d->ofill += w;

    if (d->ofill > 0) {
        ssize_t z;

        debug("writing %zd bytes of MIDI command", d->ofill);

        z = midi_write(&d->midi, d->obuf, d->ofill);
        if (z == -1)
            return;

        if (z < d->ofill)
            memmove(d->obuf, d->obuf + z, z);

        d->ofill -= z;
    }
}*/

/*
 * Modify state flags of an LED
 *
 * Post: *led is updated with the new flags
 */

/*static void set_led(led_t *led, unsigned char set, unsigned char clear)
{
    led_t n;

    n = (*led & ~clear) | set;
    if (n != *led)
        *led = n & ~SYNCED;
}*/

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
    }

    if (shift)
        return;

    if (action == CUE && on)
    {
        deck_cue(d, button);
        printf("Cue: %x \n", button);
    }

    if (action == LOOP)
    {
        if (on)
        {
            deck_punch_in(d, button);
        }
        else
        {
            deck_punch_out(d);
        }
    }

    if (action == NOTE){
        //center is 0x24
        
        d->player.nominal_pitch = pow(pow(2, (double)1/12), button - 0x24); // equal temperament
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

    switch (buf[0])
    {
    case 0x90:
        action = NOTE;
        break;
    case 0x91:

        action = CUE;
        break;

    default:

        return;
    }

    if (action == CUE || action == LOOP)
    {
        switch (buf[1])
        {
        case 0x24:
        case 0x25:
        case 0x26:
        case 0x27:
            button = buf[1] - 0x24;
            shift = false;
            break;

        case 0x28:
        case 0x29:
        case 0x30:
        case 0x31:
            button = buf[1] - 0x28;
            shift = true;
            break;

        default:
            return;
        }
    }
    else if (action == NOTE)
    {
        button = buf[1];
    }

    switch (buf[2])
    {
    case 0x00:
        on = false;
        break;

    case 0x7f:
        on = true;
        break;

    default:
        abort();
    }

    event_decoded(d->deck, action, shift, button, on);
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


    if (controller_init(c, &dicer_ops, d, rt) == -1)
        goto fail_midi;

    return 0;

fail_midi:
    midi_close(&d->midi);
fail:
    free(d);
    return -1;
}
