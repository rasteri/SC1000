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

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/poll.h>
#include <alsa/asoundlib.h>
#include <stdint.h>

#include "alsa.h"
#include "player.h"
#include "track.h"

/* This structure doesn't have corresponding functions to be an
 * abstraction of the ALSA calls; it is merely a container for these
 * variables. */

struct alsa_pcm {
    snd_pcm_t *pcm;

    struct pollfd *pe;
    size_t pe_count; /* number of pollfd entries */

    signed short *buf, *buf2;
    snd_pcm_uframes_t period;
    int rate;
};


struct alsa {
    struct alsa_pcm capture, playback;
};


static void alsa_error(const char *msg, int r)
{
    fprintf(stderr, "ALSA %s: %s\n", msg, snd_strerror(r));
}


static bool chk(const char *s, int r)
{
    if (r < 0) {
        alsa_error(s, r);
        return false;
    } else {
        return true;
    }
}


static int pcm_open(struct alsa_pcm *alsa, const char *device_name,
                    snd_pcm_stream_t stream, int rate, int buffer_size)
{
    int r, dir;
    unsigned int p;
    size_t bytes;
    snd_pcm_hw_params_t *hw_params;
    
    r = snd_pcm_open(&alsa->pcm, device_name, stream, SND_PCM_NONBLOCK);
    if (!chk("open", r))
        return -1;

    snd_pcm_hw_params_alloca(&hw_params);

    r = snd_pcm_hw_params_any(alsa->pcm, hw_params);
    if (!chk("hw_params_any", r))
        return -1;
    
    r = snd_pcm_hw_params_set_access(alsa->pcm, hw_params,
                                     SND_PCM_ACCESS_RW_INTERLEAVED);
    if (!chk("hw_params_set_access", r))
        return -1;
    
    r = snd_pcm_hw_params_set_format(alsa->pcm, hw_params, SND_PCM_FORMAT_S16);
    if (!chk("hw_params_set_format", r)) {
        fprintf(stderr, "16-bit signed format is not available. "
                "You may need to use a 'plughw' device.\n");
        return -1;
    }

    r = snd_pcm_hw_params_set_rate(alsa->pcm, hw_params, rate, 0);
    if (!chk("hw_params_set_rate", r )) {
        fprintf(stderr, "%dHz sample rate not available. You may need to use "
                "a 'plughw' device.\n", rate);
        return -1;
    }
    alsa->rate = rate;

    r = snd_pcm_hw_params_set_channels(alsa->pcm, hw_params, DEVICE_CHANNELS);
    if (!chk("hw_params_set_channels", r)) {
        fprintf(stderr, "%d channel audio not available on this device.\n",
                DEVICE_CHANNELS);
        return -1;
    }

   /* p = buffer_time * 1000; 
    dir = -1;
    r = snd_pcm_hw_params_set_buffer_time_near(alsa->pcm, hw_params, &p, &dir);
    if (!chk("hw_params_set_buffer_time_near", r)) {
        fprintf(stderr, "Buffer of %dms may be too small for this hardware.\n",
                buffer_time);
        return -1;
    }*/


    if (snd_pcm_hw_params_set_buffer_size(alsa->pcm, hw_params, buffer_size) < 0) {        
	fprintf(stderr, "Error setting buffersize.\n");
        return(-1);
    }

    p = 2; /* double buffering */
    dir = 1;
    r = snd_pcm_hw_params_set_periods_min(alsa->pcm, hw_params, &p, &dir);
    if (!chk("hw_params_set_periods_min", r)) {
        fprintf(stderr, "Buffer may be too small for this hardware.\n");
        return -1;
    }

    r = snd_pcm_hw_params(alsa->pcm, hw_params);
    if (!chk("hw_params", r))
        return -1;
    
    r = snd_pcm_hw_params_get_period_size(hw_params, &alsa->period, &dir);
    if (!chk("get_period_size", r))
        return -1;
snd_pcm_uframes_t fun;
	 r = snd_pcm_hw_params_get_buffer_size(hw_params, &fun);
	

    bytes = alsa->period * DEVICE_CHANNELS * sizeof(signed short);
    alsa->buf = malloc(bytes);
    if (!alsa->buf) {
        perror("malloc");
        return -1;
    }
    alsa->buf2 = malloc(bytes);
    if (!alsa->buf2) {
        perror("malloc");
        return -1;
    }


    /* snd_pcm_readi() returns uninitialised memory on first call,
     * possibly caused by premature POLLIN. Keep valgrind happy. */

    memset(alsa->buf, 0, bytes);
    memset(alsa->buf2, 0, bytes);

    return 0;
}


static void pcm_close(struct alsa_pcm *alsa)
{
    if (snd_pcm_close(alsa->pcm) < 0)
        abort();
    free(alsa->buf);
}


static ssize_t pcm_pollfds(struct alsa_pcm *alsa, struct pollfd *pe,
			   size_t z)
{
    int r, count;

    count = snd_pcm_poll_descriptors_count(alsa->pcm);
    if (count > z)
        return -1;

    if (count == 0)
        alsa->pe = NULL;
    else {
        r = snd_pcm_poll_descriptors(alsa->pcm, pe, count);
        if (r < 0) {
            alsa_error("poll_descriptors", r);
            return -1;
        }
        alsa->pe = pe;
    }

    alsa->pe_count = count;
    return count;
}


static int pcm_revents(struct alsa_pcm *alsa, unsigned short *revents) {
    int r;

    r = snd_pcm_poll_descriptors_revents(alsa->pcm, alsa->pe, alsa->pe_count,
                                         revents);
    if (r < 0) {
        alsa_error("poll_descriptors_revents", r);
        return -1;
    }
    
    return 0;
}



/* Start the audio device capture and playback */

static void start(struct device *dv)
{
}


/* Register this device's interest in a set of pollfd file
 * descriptors */

static ssize_t pollfds(struct device *dv, struct pollfd *pe, size_t z)
{
    int total, r;
    struct alsa *alsa = (struct alsa*)dv->local;

    total = 0;
/*
    r = pcm_pollfds(&alsa->capture, pe, z);
    if (r < 0)
        return -1;
    
    pe += r;
    z -= r;
    total += r;
  */
    r = pcm_pollfds(&alsa->playback, pe, z);
    if (r < 0)
        return -1;
    
    total += r;
    
    return total;
}
    

/* Collect audio from the player and push it into the device's buffer,
 * for playback */

static int playback(struct device *dv)
{
    int r, i;
    struct alsa *alsa = (struct alsa*)dv->local;
	static int32_t adder = 0; 
    static char writeFileName[100];
    static unsigned int nextRecordingNumber = 0;

    if (dv->player->recordingStarted && !dv->player->recording){
        nextRecordingNumber = 0;
        while (1){
            sprintf(writeFileName, "/media/sda/sc%06d.raw", nextRecordingNumber);
            if( access( writeFileName, F_OK ) != -1 ) {
                // file exists
            } else {
                // file doesn't exist
                break;
            }
        }
        printf("Opening file %s for recording\n");
        dv->player->recordingFile = fopen (writeFileName, "w");
        dv->player->recording = 1;
    }
    if (!dv->player->recordingStarted && dv->player->recording){
        fclose(dv->player->recordingFile);
        dv->player->recording = 0;
    }

	/*if ((dv->player->GoodToGo && dv->player2->GoodToGo) || (dv->player->track->finished == 1 && dv->player2->track->finished)){

	dv->player->GoodToGo = 1;dv->player2->GoodToGo = 1;*/

    player_collect(dv->player, alsa->playback.buf, alsa->playback.period);
    player_collect(dv->player2, alsa->playback.buf2, alsa->playback.period);

    // mix 2 players together
    for (i=0; i < alsa->playback.period * 2; i++){
		adder = (int32_t)alsa->playback.buf[i] + (int32_t)alsa->playback.buf2[i];
		
		// saturate add
    	if (adder > INT16_MAX) adder = INT16_MAX;
		if (adder < INT16_MIN) adder = INT16_MIN;
		alsa->playback.buf[i] = (int16_t)adder;
    }
//}
    r = snd_pcm_writei(alsa->playback.pcm, alsa->playback.buf,
                       alsa->playback.period);
    if (dv->player->recording)
        fwrite(alsa->playback.buf, alsa->playback.period * DEVICE_CHANNELS * sizeof(signed short),  1, dv->player->recordingFile);
    if (r < 0)
        return r;
        
    if (r < alsa->playback.period) {
        fprintf(stderr, "alsa: playback underrun %d/%ld.\n", r,
                alsa->playback.period);
    }

    return 0;
}


/* After poll() has returned, instruct a device to do all it can at
 * the present time. Return zero if success, otherwise -1 */

static int handle(struct device *dv)
{
    int r;
    unsigned short revents;
    struct alsa *alsa = (struct alsa*)dv->local;

    /* Check the output buffer for playback */
    
    r = pcm_revents(&alsa->playback, &revents);
    if (r < 0)
        return -1;
    
    if (revents & POLLOUT) {
        r = playback(dv);
        
        if (r < 0) {
            if (r == -EPIPE) {
                fputs("ALSA: playback xrun.\n", stderr);
                
                r = snd_pcm_prepare(alsa->playback.pcm);
                if (r < 0) {
                    alsa_error("prepare", r);
                    return -1;
                }

                /* The device starts when data is written. POLLOUT
                 * events are generated in prepared state. */

            } else {
                alsa_error("playback", r);
                return -1;
            }
        }
    }

    return 0;
}


static unsigned int sample_rate(struct device *dv)
{
    struct alsa *alsa = (struct alsa*)dv->local;

    return alsa->playback.rate;
}


/* Close ALSA device and clear any allocations */

static void clear(struct device *dv)
{
    struct alsa *alsa = (struct alsa*)dv->local;

    pcm_close(&alsa->capture);
    pcm_close(&alsa->playback);
    free(dv->local);
}


static struct device_ops alsa_ops = {
    .pollfds = pollfds,
    .handle = handle,
    .sample_rate = sample_rate,
    .start = start,
    .clear = clear
};


/* Open ALSA device. Do not operate on audio until device_start() */

int alsa_init(struct device *dv, const char *device_name,
              int rate, int buffer_size, bool slave)
{
    struct alsa *alsa;

    alsa = malloc(sizeof *alsa);
    if (alsa == NULL) {
        perror("malloc");
        return -1;
    }

    if (!slave){
		if (pcm_open(&alsa->playback, device_name, SND_PCM_STREAM_PLAYBACK,
					rate, buffer_size) < 0)
		{
			fputs("Failed to open device for playback.\n", stderr);
			goto fail_capture;
		}
    }
    device_init(dv, &alsa_ops);
    dv->local = alsa;

    
    alsa->recording = 0;


    return 0;

 fail_capture:
    pcm_close(&alsa->capture);
    return 0;
}


/* ALSA caches information when devices are open. Provide a call
 * to clear these caches so that valgrind output is clean. */

void alsa_clear_config_cache(void)
{
    int r;

    r = snd_config_update_free_global();
    if (r < 0)
        alsa_error("config_update_free_global", r);
}
