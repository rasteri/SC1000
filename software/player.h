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

#ifndef PLAYER_H
#define PLAYER_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "spin.h"
#include "track.h"

#define PLAYER_CHANNELS 2

// How many samples each beep stage lasts
#define BEEPSPEED 4800

#define BEEP_NONE -1
#define BEEP_RECORDINGSTART 0
#define BEEP_RECORDINGSTOP 1
#define BEEP_RECORDINGERROR 2




struct player {
    double sample_dt;

    spin lock;
    struct track *track;

    long samplesSoFar;


    /* Current playback parameters */

    double position, /* seconds */
        target_position, /* seconds, or TARGET_UNKNOWN */
        offset, /* track start point in timecode */
        last_difference, /* last known position minus target_position */
        pitch, /* from timecoder */
        sync_pitch, /* pitch required to sync to timecode signal */
        volume,
        nominal_pitch, // Pitch after any note/pitch fader changes
		motor_speed; // speed of virtual motor, usually same as nominal_pitch but affected by start/stop

    /* Timecode control */

    struct timecoder *timecoder;
    bool timecode_control,
        recalibrate; /* re-sync offset at next opportunity */
	bool justPlay;
	double faderTarget; // Player should slowly fade to this level
	double faderVolume; // current fader volume
	bool capTouch;
	
	bool GoodToGo;
	bool stopped;

    bool recording;
    bool recordingStarted;

    int playingBeep;
    unsigned long beepPos;
    
    FILE *recordingFile;
    char recordingFileName[256];
};

void player_init(struct player *pl, unsigned int sample_rate,
                 struct track *track);
void player_clear(struct player *pl);

void player_set_timecoder(struct player *pl, struct timecoder *tc);
void player_set_timecode_control(struct player *pl, bool on);
bool player_toggle_timecode_control(struct player *pl);
void player_set_internal_playback(struct player *pl);
 
void player_set_track(struct player *pl, struct track *track);
void player_clone(struct player *pl, const struct player *from);

double player_get_position(struct player *pl);
double player_get_elapsed(struct player *pl);
double player_get_remain(struct player *pl);
bool player_is_active(const struct player *pl);

void player_seek_to(struct player *pl, double seconds);
void player_recue(struct player *pl);

void player_collect(struct player *pl, signed short *pcm, unsigned samples);

#endif
