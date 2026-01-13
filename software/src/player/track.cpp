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

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <string>
#include <unordered_map>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h> /* mlock() */
#include <unistd.h>

#include "../util/debug.h"
#include "../util/log.h"
#include "../util/status.h"
#include "../util/external.h"

#include "../thread/realtime.h"
#include "../thread/rig.h"
#include "track.h"


#define RATE 44100

#define SAMPLE (sizeof(signed short) * TRACK_CHANNELS) /* bytes per sample */
#define TRACK_BLOCK_PCM_BYTES (TRACK_BLOCK_SAMPLES * SAMPLE)

#define _STR(tok) #tok
#define STR(tok) _STR(tok)

// Track registry - maps path to track pointer for O(1) lookup
// Replaces the old intrusive linked list
static std::unordered_map<std::string, Track*> g_track_registry;

static bool use_mlock = false;

/*
 * An empty track is used rarely, and is easier than
 * continuous checks for NULL throughout the code
 */

static Track empty = {};

// Initialize empty track at startup
static struct EmptyTrackInit {
	EmptyTrackInit() {
		empty.refcount = 1;
		empty.rate = RATE;
		empty.importer = nullptr;
		empty.path = nullptr;
		empty.bytes = 0;
		empty.length = 0;
		empty.blocks = 0;
		empty.pid = 0;
		empty.fd = -1;
		empty.pe = nullptr;
		empty.terminated = false;
		empty.finished = false;
	}
} empty_track_init;

/*
 * Request that memory for tracks is locked into RAM as it is
 * allocated
 */

void track_use_mlock()
{
	use_mlock = true;
}

/*
 * Allocate more memory
 *
 * Return: -1 if memory could not be allocated, otherwise 0
 */

static int more_space(Track* tr)
{
	TrackBlock* block;

	rt_not_allowed();

	if (tr->blocks >= TRACK_MAX_BLOCKS)
	{
		LOG_WARN("Maximum track length reached");
		return -1;
	}

	block = static_cast<TrackBlock*>(malloc(sizeof(TrackBlock)));
	if (block == nullptr)
	{
		perror("malloc");
		return -1;
	}

	if (use_mlock && mlock(block, sizeof(TrackBlock)) == -1)
	{
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

static void* access_pcm(Track* tr, size_t* len)
{
	unsigned int block;
	size_t fill;

	block = static_cast<unsigned int>(tr->bytes / TRACK_BLOCK_PCM_BYTES);
	if (block == tr->blocks)
	{
		if (more_space(tr) == -1)
		{
			return nullptr;
		}
	}

	fill = tr->bytes % TRACK_BLOCK_PCM_BYTES;
	*len = TRACK_BLOCK_PCM_BYTES - fill;

	return static_cast<void*>(reinterpret_cast<char*>(tr->block[block]->pcm) + fill);
}

/*
 * Notify that audio has been placed in the buffer
 *
 * The parameter is the number of stereo samples which have been
 * placed in the buffer.
 */

static void commit_pcm_samples(Track* tr, unsigned int samples)
{
	unsigned int fill = tr->length % TRACK_BLOCK_SAMPLES;

	assert(samples <= TRACK_BLOCK_SAMPLES - fill);

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

static void commit(Track* tr, size_t len)
{
	tr->bytes += len;
	commit_pcm_samples(tr, static_cast<unsigned int>(tr->bytes / SAMPLE - tr->length));
}

/*
 * Initialise object which will hold PCM audio data, and start
 * importing the data
 *
 * Post: track is initialised
 * Post: track is importing
 */

static int track_init(Track* t, const char* importer, const char* path)
{
	pid_t pid;

	LOG_INFO("Importing '%s'...", path);

	pid = fork_pipe_nb(&t->fd, importer, "import", path, STR(RATE), nullptr);
	if (pid == -1)
	{
		return -1;
	}

	t->pid = pid;
	t->pe = nullptr;
	t->terminated = false;

	t->refcount = 0;

	t->blocks = 0;
	t->rate = RATE;

	t->bytes = 0;
	t->length = 0;

	t->importer = importer;
	t->path = path;
	t->finished = false;

	// Add to track registry for deduplication lookups
	g_track_registry[path] = t;

	g_rig.post_track(t);

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

static void track_clear(Track* tr)
{
	assert(tr->pid == 0);

	for (unsigned int n = 0; n < tr->blocks; n++)
	{
		free(tr->block[n]);
	}

	// Remove from track registry
	if (tr->path != nullptr) {
		g_track_registry.erase(tr->path);
	}
}

/*
 * Get a pointer to a track object already in memory
 *
 * Return: pointer, or NULL if no such track exists
 */

static Track* track_get_again(const char* importer, const char* path)
{
	// O(1) lookup in the track registry
	auto it = g_track_registry.find(path);
	if (it != g_track_registry.end()) {
		Track* t = it->second;
		// Verify importer matches (should always be the same in practice)
		if (t->importer == importer) {
			track_acquire(t);
			return t;
		}
	}

	return nullptr;
}

/*
 * Get a pointer to a track object for the given importer and path
 *
 * Return: pointer, or NULL if not enough resources
 */

Track* track_acquire_by_import(const char* importer, const char* path)
{
	Track* t;

	t = track_get_again(importer, path);
	if (t != nullptr)
	{
		return t;
	}

	t = static_cast<Track*>(malloc(sizeof *t));
	if (t == nullptr)
	{
		perror("malloc");
		return nullptr;
	}

	if (track_init(t, importer, path) == -1)
	{
		free(t);
		return nullptr;
	}

	track_acquire(t);

	return t;
}

/*
 * Get a pointer to a static track containing no audio
 *
 * Return: pointer, not NULL
 */

Track* track_acquire_empty()
{
	empty.refcount++;
	return &empty;
}

/*
 * Create a new track for recording audio directly into memory
 *
 * Unlike track_acquire_by_import(), this creates an empty track
 * that can be written to using track_get_sample() and track_set_length().
 *
 * Return: pointer to track, or NULL on failure
 */
Track* track_acquire_for_recording(int sample_rate)
{
	Track* t = static_cast<Track*>(malloc(sizeof *t));
	if (t == nullptr)
	{
		perror("malloc");
		return nullptr;
	}

	t->refcount = 1;
	t->rate = sample_rate;
	t->importer = nullptr;
	t->path = nullptr;  // No path for in-memory recordings
	t->bytes = 0;
	t->length = 0;
	t->blocks = 0;
	t->pid = 0;         // Not importing
	t->fd = -1;
	t->pe = nullptr;
	t->terminated = false;
	t->finished = true; // Already "finished" (not importing)

	return t;
}

/*
 * Ensure track has enough space for the given number of samples
 *
 * Return: 0 on success, -1 if allocation failed
 */
int Track::ensure_space(unsigned int samples)
{
	unsigned int blocks_needed = (samples + TRACK_BLOCK_SAMPLES - 1) / TRACK_BLOCK_SAMPLES;

	while (blocks < blocks_needed)
	{
		if (more_space(this) == -1)
		{
			return -1;
		}
	}

	return 0;
}

/*
 * Set track length (for direct recording)
 * Uses atomic increment for thread safety
 */
void Track::set_length(unsigned int samples)
{
	__sync_lock_test_and_set(&length, samples);
	bytes = samples * SAMPLE;
}

void track_acquire(Track* t)
{
	t->refcount++;
}

/*
 * Request premature termination of an import operation
 */

static void terminate(Track* t)
{
	assert(t->pid != 0);

	if (kill(t->pid, SIGTERM) == -1)
	{
		abort();
	}

	t->terminated = true;
}

/*
 * Finish use of a track object
 */

void track_release(Track* t)
{
	t->refcount--;

	/* When importing, a reference is held. If it's the
	 * only one remaining terminate it to save resources */

	if (t->refcount == 1 && t->pid != 0)
	{
		terminate(t);
		return;
	}

	if (t->refcount == 0)
	{
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

void Track::pollfd(struct pollfd* poll_entry)
{
	assert(pid != 0);

	poll_entry->fd = fd;
	poll_entry->events = POLLIN;

	pe = poll_entry;
}

/*
 * Read the next block of data from the file handle into the track's
 * PCM data
 *
 * Return: -1 on completion, otherwise zero
 */

static int read_from_pipe(Track* tr)
{
	for (;;)
	{
		void* pcm;
		size_t len;
		ssize_t z;

		pcm = access_pcm(tr, &len);
		if (pcm == nullptr)
		{
			return -1;
		}

		z = read(tr->fd, pcm, len);
		if (z == -1)
		{
			if (errno == EAGAIN)
			{
				return 0;
			}
			else
			{
				perror("read");
				return -1;
			}
		}

		if (z == 0)
		{ /* EOF */
			break;
		}

		commit(tr, static_cast<size_t>(z));
	}

	return -1; /* completion without error */
}

/*
 * Synchronise with the import process and complete it
 *
 * Pre: track is importing
 * Post: track is not importing
 */

static void stop_import(Track* t)
{
	int status;

	assert(t->pid != 0);

	if (close(t->fd) == -1)
	{
		abort();
	}

	if (waitpid(t->pid, &status, 0) == -1)
	{
		abort();
	}

	if (WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS)
	{
		LOG_DEBUG("Track import completed");
		t->finished = true;
	}
	else
	{
		LOG_WARN("Track import completed with status %d", status);
		if (!t->terminated)
		{
			status_printf(STATUS_ALERT, "Error importing %s", t->path);
		}
	}

	t->pid = 0;
}

/*
 * Handle any file descriptor activity on this track
 *
 * Return: true if import has completed, otherwise false
 */

void Track::handle()
{
	assert(pid != 0);

	/* A track may be added while poll() was waiting,
	 * in which case it has no return data from poll */

	if (pe == nullptr)
	{
		return;
	}

	if (pe->revents == 0)
	{
		return;
	}

	if (read_from_pipe(this) != -1)
	{
		return;
	}

	stop_import(this);
	g_rig.remove_track(this);
	track_release(this); /* may delete the track */
}

