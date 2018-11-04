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

#include <assert.h>

#include "controller.h"
#include "cues.h"
#include "deck.h"
#include "status.h"
#include "rig.h"

/*
 * An empty record, is used briefly until a record is loaded
 * to a deck
 */

/*
 * Initialise a deck
 *
 * A deck is a logical grouping of the various components which
 * reflects the user's view on a deck in the system.
 *
 * Pre: deck->device is valid
 */

int deck_init(struct deck *d, struct rt *rt, 
		const char *importer, double speed, bool phono, bool protect,
		bool slave) {
	unsigned int rate;

	if (!slave) {
		if (rt_add_device(rt, &d->device) == -1)
			return -1;
	}

	d->ncontrol = 0;
	d->punch = NO_PUNCH;
	d->protect = protect;
	assert(importer != NULL);
	d->importer = importer;
	if (slave)
		rate = 48000;

	//timecoder_init(&d->timecoder, timecode, speed, rate, phono);
	player_init(&d->player, rate, track_acquire_empty());
	cues_reset(&d->cues);

	/* The timecoder and player are driven by requests from
	 * the audio device */

	//device_connect_timecoder(&d->device, &d->timecoder);

	device_connect_player(&d->device, &d->player);
	printf("Here\n");
	return 0;
}

void deck_clear(struct deck *d) {
	/* FIXME: remove from rig and rt */
	player_clear(&d->player);
	//timecoder_clear(&d->timecoder);
	device_clear(&d->device);
}

bool deck_is_locked(const struct deck *d) {
	return (d->protect && player_is_active(&d->player));
}

/*
 * Load a record from the library to a deck
 */
/*
void deck_load(struct deck *d, struct record *record) {
	struct track *t;

	if (deck_is_locked(d)) {
		status_printf(STATUS_WARN, "Stop deck to load a different track");
		return;
	}

	t = track_acquire_by_import(d->importer, record->pathname);
	if (t == NULL )
		return;

	d->record = record;
	player_set_track(&d->player, t); // passes reference 
}
*/
void deck_recue(struct deck *d) {
	if (deck_is_locked(d)) {
		status_printf(STATUS_WARN, "Stop deck to recue");
		return;
	}

	player_recue(&d->player);
}

void deck_clone(struct deck *d, const struct deck *from) {
	d->record = from->record;
	player_clone(&d->player, &from->player);
}

/*
 * Clear the cue point, ready to be set again
 */

void deck_unset_cue(struct deck *d, unsigned int label) {
	cues_unset(&d->cues, label);
}

/*
 * Seek the current playback position to a cue point position,
 * or set the cue point if unset
 */

void deck_cue(struct deck *d, unsigned int label) {
	double p;

	p = cues_get(&d->cues, label);
	if (p == CUE_UNSET)
		cues_set(&d->cues, label, player_get_elapsed(&d->player));
	else
		player_seek_to(&d->player, p);
}

/*
 * Seek to a cue point ready to return from it later. Overrides an
 * existing punch operation.
 */

void deck_punch_in(struct deck *d, unsigned int label) {
	double p, e;

	e = player_get_elapsed(&d->player);
	p = cues_get(&d->cues, label);
	if (p == CUE_UNSET) {
		cues_set(&d->cues, label, e);
		return;
	}

	if (d->punch != NO_PUNCH)
		e -= d->punch;

	player_seek_to(&d->player, p);
	d->punch = p - e;
}

/*
 * Return from a cue point
 */

void deck_punch_out(struct deck *d) {
	double e;

	if (d->punch == NO_PUNCH)
		return;

	e = player_get_elapsed(&d->player);
	player_seek_to(&d->player, e - d->punch);
	d->punch = NO_PUNCH;
}
