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
#include "sc_playlist.h"
#include "xwax.h"

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
			  bool slave)
{
	unsigned int rate;

	if (!slave)
	{
		if (rt_add_device(rt, &d->device) == -1)
			return -1;
	}

	d->ncontrol = 0;
	d->punch = NO_PUNCH;
	d->protect = protect;
	assert(importer != NULL);
	d->importer = importer;
	d->shifted = 0;
	if (slave)
		rate = 48000;

	//timecoder_init(&d->timecoder, timecode, speed, rate, phono);
	player_init(&d->player, rate, track_acquire_empty());
	cues_reset(&d->cues);

	/* The timecoder and player are driven by requests from
	 * the audio device */

	//device_connect_timecoder(&d->device, &d->timecoder);

	d->FirstFolder = NULL;
	d->CurrentFile = NULL;
	d->CurrentFolder = NULL;
	d->NumFiles = 0;
	d->filesPresent = 0;

	d->angleOffset = 0;
	d->encoderAngle = 0xffff;
	d->newEncoderAngle = 0xffff;

	device_connect_player(&d->device, &d->player);
	return 0;
}

void deck_clear(struct deck *d)
{
	/* FIXME: remove from rig and rt */
	player_clear(&d->player);
	//timecoder_clear(&d->timecoder);
	device_clear(&d->device);
}

bool deck_is_locked(const struct deck *d)
{
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
void deck_recue(struct deck *d)
{
	if (deck_is_locked(d))
	{
		status_printf(STATUS_WARN, "Stop deck to recue");
		return;
	}

	player_recue(&d->player);
}

void deck_clone(struct deck *d, const struct deck *from)
{
	player_clone(&d->player, &from->player);
}

/*
 * Clear the cue point, ready to be set again
 */

void deck_unset_cue(struct deck *d, unsigned int label)
{
	cues_unset(&d->cues, label);
}

/*
 * Seek the current playback position to a cue point position,
 * or set the cue point if unset
 */

void deck_cue(struct deck *d, unsigned int label)
{
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

void deck_punch_in(struct deck *d, unsigned int label)
{
	double p, e;

	e = player_get_elapsed(&d->player);
	p = cues_get(&d->cues, label);
	if (p == CUE_UNSET)
	{
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

void deck_punch_out(struct deck *d)
{
	double e;

	if (d->punch == NO_PUNCH)
		return;

	e = player_get_elapsed(&d->player);
	player_seek_to(&d->player, e - d->punch);
	d->punch = NO_PUNCH;
}

void deck_load_folder(struct deck *d, char *FolderName)
{
	// Build index of all audio files on the USB stick
	if ((d->FirstFolder = LoadFileStructure("/media/sda/beats/", &d->NumFiles)) != NULL && d->NumFiles > 0)
	{
		printf("Folder '%s' Indexed with %d files: \n", FolderName, d->NumFiles);
		d->filesPresent = 1;
	}
	if (d->filesPresent)
	{
		//DumpFileStructure(FirstBeatFolder);
		d->CurrentFolder = d->FirstFolder;
		d->CurrentFile = d->CurrentFolder->FirstFile;
		// Load first beat
		player_set_track(&d->player, track_acquire_by_import(d->importer, d->CurrentFile->FullPath));
		cues_load_from_file(&d->cues, d->player.track->path);
	}
}

void load_track(struct deck *d, struct track *track)
{
	struct player *pl = &d->player;
	cues_save_to_file(&d->cues, pl->track->path);
	player_set_track(pl, track);
	pl->target_position = 0;
	pl->position = 0;
	pl->offset = 0;
	cues_load_from_file(&d->cues, pl->track->path);
	pl->nominal_pitch = 1.0;
	if (!d->player.justPlay)
	{
		// If touch sensor is enabled, set the "zero point" to the current encoder angle
		if (scsettings.platterenabled)
			d->angleOffset = 0 - d->encoderAngle;

		else // If touch sensor is disabled, set the "zero point" to encoder zero point so sticker is exactly on each time sample is loaded
			d->angleOffset = (pl->position * scsettings.platterspeed) - d->encoderAngle;
	}
}

void deck_next_file(struct deck *d)
{
	if (d->CurrentFile->next != NULL)
	{
		d->CurrentFile = d->CurrentFile->next;
	}
	load_track(d, track_acquire_by_import(d->importer, d->CurrentFile->FullPath));
}

void deck_prev_file(struct deck *d)
{
	if (d->CurrentFile->prev != NULL)
	{
		d->CurrentFile = d->CurrentFile->prev;
	}
	load_track(d, track_acquire_by_import(d->importer, d->CurrentFile->FullPath));
}

void deck_next_folder(struct deck *d)
{
	if (d->CurrentFolder->next != NULL)
	{
		d->CurrentFolder = d->CurrentFolder->next;
		d->CurrentFile = d->CurrentFolder->FirstFile;
		load_track(d, track_acquire_by_import(deck[1].importer, d->CurrentFile->FullPath));
	}
}
void deck_prev_folder(struct deck *d)
{
	if (d->CurrentFolder->prev != NULL)
	{
		d->CurrentFolder = d->CurrentFolder->prev;
		d->CurrentFile = d->CurrentFolder->FirstFile;
		load_track(d, track_acquire_by_import(deck[1].importer, d->CurrentFile->FullPath));
	}
}

void deck_random_file(struct deck *d)
{
	int r = rand() % d->NumFiles;
	printf("Playing file %d/%d\n", r, d->NumFiles);
	load_track(d, track_acquire_by_import(d->importer, GetFileAtIndex(r, d->FirstFolder)->FullPath));
	deck[1].player.nominal_pitch = 1.0;
}

void deck_record(struct deck *d)
{
	d->player.recordingStarted = !d->player.recordingStarted;
}