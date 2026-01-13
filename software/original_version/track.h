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

#ifndef TRACK_H
#define TRACK_H

#include <stdbool.h>
#include <sys/poll.h>
#include <sys/types.h>

#include "list.h"

#define TRACK_CHANNELS 2

#define TRACK_MAX_BLOCKS 64
#define TRACK_BLOCK_SAMPLES (2048 * 1024)
#define TRACK_PPM_RES 64
#define TRACK_OVERVIEW_RES 2048

struct track_block {
    signed short pcm[TRACK_BLOCK_SAMPLES * TRACK_CHANNELS];
    unsigned char ppm[TRACK_BLOCK_SAMPLES / TRACK_PPM_RES],
        overview[TRACK_BLOCK_SAMPLES / TRACK_OVERVIEW_RES];
};

struct track {
    struct list tracks;
    unsigned int refcount;
    int rate;

    /* pointers to external data */
   
    const char *importer, *path;
    
    size_t bytes; /* loaded in */
    unsigned int length, /* track length in samples */
        blocks; /* number of blocks allocated */
    struct track_block *block[TRACK_MAX_BLOCKS];

    /* State of audio import */

    struct list rig;
    pid_t pid;
    int fd;
    struct pollfd *pe;
    bool terminated;

    /* Current value of audio meters when loading */
    
    unsigned short ppm;
    unsigned int overview;
    
   bool finished;
};

void track_use_mlock(void);

/* Tracks are dynamically allocated and reference counted */

struct track* track_acquire_by_import(const char *importer, const char *path);
struct track* track_acquire_empty(void);
void track_acquire(struct track *t);
void track_release(struct track *t);

/* Functions used by the rig and main thread */

void track_pollfd(struct track *tr, struct pollfd *pe);
void track_handle(struct track *tr);

/* Return true if the track importer is running, otherwise false */

static inline bool track_is_importing(struct track *tr)
{
    return tr->pid != 0;
}

/* Return the pseudo-PPM meter value for the given sample */

static inline unsigned char track_get_ppm(struct track *tr, int s)
{
    struct track_block *b;
    b = tr->block[s / TRACK_BLOCK_SAMPLES];
    return b->ppm[(s % TRACK_BLOCK_SAMPLES) / TRACK_PPM_RES];
}

/* Return the overview meter value for the given sample */

static inline unsigned char track_get_overview(struct track *tr, int s)
{
    struct track_block *b;
    b = tr->block[s / TRACK_BLOCK_SAMPLES];
    return b->overview[(s % TRACK_BLOCK_SAMPLES) / TRACK_OVERVIEW_RES];
}

/* Return a pointer to (not value of) the sample data for each channel */

static inline signed short* track_get_sample(struct track *tr, int s)
{
    struct track_block *b;
    b = tr->block[s / TRACK_BLOCK_SAMPLES];
    return &b->pcm[(s % TRACK_BLOCK_SAMPLES) * TRACK_CHANNELS];
}

#endif

