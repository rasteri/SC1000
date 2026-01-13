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
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h> /* mlock() */

#include "debug.h"
#include "external.h"
#include "list.h"
#include "realtime.h"
#include "rig.h"
#include "status.h"
#include "track.h"

#define RATE 44100

#define SAMPLE (sizeof(signed short) * TRACK_CHANNELS) /* bytes per sample */
#define TRACK_BLOCK_PCM_BYTES (TRACK_BLOCK_SAMPLES * SAMPLE)

#define _STR(tok) #tok
#define STR(tok) _STR(tok)

static struct list tracks = LIST_INIT(tracks);
static bool use_mlock = false;

/*
 * An empty track is used rarely, and is easier than
 * continuous checks for NULL throughout the code
 */

static struct track empty = {
    .refcount = 1,

    .rate = RATE,
    .bytes = 0,
    .length = 0,
    .blocks = 0,

    .pid = 0
};

/*
 * Request that memory for tracks is locked into RAM as it is
 * allocated
 */

void track_use_mlock(void)
{
    use_mlock = true;
}

/*
 * Allocate more memory
 *
 * Return: -1 if memory could not be allocated, otherwize 0
 */

static int more_space(struct track *tr)
{
    struct track_block *block;

    rt_not_allowed();

    if (tr->blocks >= TRACK_MAX_BLOCKS) {
        fprintf(stderr, "Maximum track length reached.\n");
        return -1;
    }

    block = malloc(sizeof(struct track_block));
    if (block == NULL) {
        perror("malloc");
        return -1;
    }

    if (use_mlock && mlock(block, sizeof(struct track_block)) == -1) {
        perror("mlock");
        free(block);
        return -1;
    }

    /* No memory barrier is needed here, because nobody else tries to
     * access these blocks until tr->length is actually incremented */

    tr->block[tr->blocks++] = block;

    debug("allocated new track block (%d blocks, %zu bytes)",
          tr->blocks, tr->blocks * TRACK_BLOCK_SAMPLES * SAMPLE);

    return 0;
}

/*
 * Get access to the PCM buffer for incoming audio
 *
 * Return: pointer to buffer
 * Post: len contains the length of the buffer, in bytes
 */

static void* access_pcm(struct track *tr, size_t *len)
{
    unsigned int block;
    size_t fill;

    block = tr->bytes / TRACK_BLOCK_PCM_BYTES;
    if (block == tr->blocks) {
        if (more_space(tr) == -1)
            return NULL;
    }

    fill = tr->bytes % TRACK_BLOCK_PCM_BYTES;
    *len = TRACK_BLOCK_PCM_BYTES - fill;

    return (void*)tr->block[block]->pcm + fill;
}

/*
 * Notify that audio has been placed in the buffer
 *
 * The parameter is the number of stereo samples which have been
 * placed in the buffer.
 */

static void commit_pcm_samples(struct track *tr, unsigned int samples)
{
    unsigned int fill, n;
    signed short *pcm;
    struct track_block *block;

    block = tr->block[tr->length / TRACK_BLOCK_SAMPLES];
    fill = tr->length % TRACK_BLOCK_SAMPLES;
    pcm = block->pcm + TRACK_CHANNELS * fill;

    assert(samples <= TRACK_BLOCK_SAMPLES - fill);

    /* Meter the new audio */

    for (n = samples; n > 0; n--) {
        unsigned short v;
        unsigned int w;

        v = abs(pcm[0]) + abs(pcm[1]);

        /* PPM-style fast meter approximation */

        if (v > tr->ppm)
            tr->ppm += (v - tr->ppm) >> 3;
        else
            tr->ppm -= (tr->ppm - v) >> 9;

        block->ppm[fill / TRACK_PPM_RES] = tr->ppm >> 8;

        /* Update the slow-metering overview. Fixed point arithmetic
         * going on here */

        w = v << 16;

        if (w > tr->overview)
            tr->overview += (w - tr->overview) >> 8;
        else
            tr->overview -= (tr->overview - w) >> 17;

        block->overview[fill / TRACK_OVERVIEW_RES] = tr->overview >> 24;

        fill++;
        pcm += TRACK_CHANNELS;
    }

    /* Increment the track length. A memory barrier ensures the
     * realtime or UI thread does not access garbage audio */

    __sync_fetch_and_add(&tr->length, samples);
}

/*
 * Notify that data has been placed in the buffer
 *
 * This function passes any whole samples to commit_pcm_samples()
 * and leaves the residual in the buffer ready for next time.
 */

static void commit(struct track *tr, size_t len)
{
    tr->bytes += len;
    commit_pcm_samples(tr, tr->bytes / SAMPLE - tr->length);
}

/*
 * Initialise object which will hold PCM audio data, and start
 * importing the data
 *
 * Post: track is initialised
 * Post: track is importing
 */

static int track_init(struct track *t, const char *importer, const char *path)
{
    pid_t pid;

    fprintf(stderr, "Importing '%s'...\n", path);

    pid = fork_pipe_nb(&t->fd, importer, "import", path, STR(RATE), NULL);
    if (pid == -1)
        return -1;

    t->pid = pid;
    t->pe = NULL;
    t->terminated = false;

    t->refcount = 0;

    t->blocks = 0;
    t->rate = RATE;

    t->bytes = 0;
    t->length = 0;
    t->ppm = 0;
    t->overview = 0;

    t->importer = importer;
    t->path = path;
t->finished = 0;


    list_add(&t->tracks, &tracks);
    rig_post_track(t);

    return 0;
}

/*
 * Destroy this track from memory
 *
 * Terminates any import processes and frees any memory allocated by
 * this object.
 *
 * Pre: track is not importing
 * Pre: track is initialised
 */

static void track_clear(struct track *tr)
{
    int n;

    assert(tr->pid == 0);

    for (n = 0; n < tr->blocks; n++)
        free(tr->block[n]);

    list_del(&tr->tracks);
}

/*
 * Get a pointer to a track object already in memory
 *
 * Return: pointer, or NULL if no such track exists
 */

static struct track* track_get_again(const char *importer, const char *path)
{
    struct track *t;

    list_for_each(t, &tracks, tracks) {
        if (t->importer == importer && t->path == path) {
            track_acquire(t);
            return t;
        }
    }

    return NULL;
}

/*
 * Get a pointer to a track object for the given importer and path
 *
 * Return: pointer, or NULL if not enough resources
 */

struct track* track_acquire_by_import(const char *importer, const char *path)
{
    struct track *t;

    t = track_get_again(importer, path);
    if (t != NULL)
        return t;

    t = malloc(sizeof *t);
    if (t == NULL) {
        perror("malloc");
        return NULL;
    }

    if (track_init(t, importer, path) == -1) {
        free(t);
        return NULL;
    }

    track_acquire(t);

    return t;
}

/*
 * Get a pointer to a static track containing no audio
 *
 * Return: pointer, not NULL
 */

struct track* track_acquire_empty(void)
{
    empty.refcount++;
    return &empty;
}

void track_acquire(struct track *t)
{
    t->refcount++;
}

/*
 * Request premature termination of an import operation
 */

static void terminate(struct track *t)
{
    assert(t->pid != 0);

    if (kill(t->pid, SIGTERM) == -1)
        abort();

    t->terminated = true;
}

/*
 * Finish use of a track object
 */

void track_release(struct track *t)
{
    t->refcount--;

    /* When importing, a reference is held. If it's the
     * only one remaining terminate it to save resources */

    if (t->refcount == 1 && t->pid != 0) {
        terminate(t);
        return;
    }

    if (t->refcount == 0) {
        assert(t != &empty);
        track_clear(t);
        free(t);
    }
}

/*
 * Get entry for use by poll()
 *
 * Pre: track is importing
 * Post: *pe contains poll entry
 */

void track_pollfd(struct track *t, struct pollfd *pe)
{
    assert(t->pid != 0);

    pe->fd = t->fd;
    pe->events = POLLIN;

    t->pe = pe;
}

/*
 * Read the next block of data from the file handle into the track's
 * PCM data
 *
 * Return: -1 on completion, otherwise zero
 */

static int read_from_pipe(struct track *tr)
{
    for (;;) {
        void *pcm;
        size_t len;
        ssize_t z;

        pcm = access_pcm(tr, &len);
        if (pcm == NULL)
            return -1;

        z = read(tr->fd, pcm, len);
        if (z == -1) {
            if (errno == EAGAIN) {
                return 0;
            } else {
                perror("read");
                return -1;
            }
        }

        if (z == 0) /* EOF */
            break;

        commit(tr, z);
    }

    return -1; /* completion without error */
}

/*
 * Synchronise with the import process and complete it
 *
 * Pre: track is importing
 * Post: track is not importing
 */

static void stop_import(struct track *t)
{
    int status;

    assert(t->pid != 0);

    if (close(t->fd) == -1)
        abort();

    if (waitpid(t->pid, &status, 0) == -1)
        abort();

    if (WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS) {
        fprintf(stderr, "Track import completed\n");
	t->finished = 1;
    } else {
        fprintf(stderr, "Track import completed with status %d\n", status);
        if (!t->terminated)
            status_printf(STATUS_ALERT, "Error importing %s", t->path);
    }

    t->pid = 0;
}

/*
 * Handle any file descriptor activity on this track
 *
 * Return: true if import has completed, otherwise false
 */

void track_handle(struct track *tr)
{
    assert(tr->pid != 0);

    /* A track may be added while poll() was waiting,
     * in which case it has no return data from poll */

    if (tr->pe == NULL)
        return;

    if (tr->pe->revents == 0)
        return;

    if (read_from_pipe(tr) != -1)
        return;

    stop_import(tr);
    list_del(&tr->rig);
    track_release(tr); /* may delete the track */
}
