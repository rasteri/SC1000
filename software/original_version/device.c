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
#include <stddef.h>

#include "debug.h"
#include "device.h"
#include "player.h"

void device_init(struct device *dv, struct device_ops *ops)
{
    debug("%p", dv);
    dv->fault = false;
    dv->ops = ops;
}

/*
 * Clear (destruct) the device. The corresponding constructor is
 * specific to each particular audio system
 */

void device_clear(struct device *dv)
{
    if (dv->ops->clear != NULL)
        dv->ops->clear(dv);
}

void device_connect_timecoder(struct device *dv, struct timecoder *tc)
{
    dv->timecoder = tc;
}

void device_connect_player(struct device *dv, struct player *pl)
{
    dv->player = pl;
}

/*
 * Return: the sample rate of the device in Hz
 */

unsigned int device_sample_rate(struct device *dv)
{
    assert(dv->ops->sample_rate != NULL);
    return dv->ops->sample_rate(dv);
}

/*
 * Start the device inputting and outputting audio
 */

void device_start(struct device *dv)
{
    if (dv->ops->start != NULL)
        dv->ops->start(dv);
}

/*
 * Stop the device
 */

void device_stop(struct device *dv)
{
    if (dv->ops->stop != NULL)
        dv->ops->stop(dv);
}

/*
 * Get file descriptors which should be polled for this device
 *
 * Do not return anything for callback-based audio systems. If the
 * return value is > 0, there must be a handle() function available.
 *
 * Return: the number of pollfd filled, or -1 on error
 */

ssize_t device_pollfds(struct device *dv, struct pollfd *pe, size_t z)
{
    if (dv->ops->pollfds != NULL)
        return dv->ops->pollfds(dv, pe, z);
    else
        return 0;
}

/*
 * Handle any available input or output on the device
 *
 * This function can be called when there is activity on any file
 * descriptor, not specifically one returned by this device.
 */

void device_handle(struct device *dv)
{
    if (dv->fault)
        return;

    if (dv->ops->handle == NULL)
        return;

    if (dv->ops->handle(dv) != 0) {
        dv->fault = true;
        fputs("Error handling audio device; disabling it\n", stderr);
    }
}

/*
 * Send audio from a device for processing
 *
 * Pre: buffer pcm contains n stereo samples
 */

void device_submit(struct device *dv, signed short *pcm, size_t n)
{
    //assert(dv->timecoder != NULL);
    //timecoder_submit(dv->timecoder, pcm, n);
}

/*
 * Collect audio from the processing to send to a device
 *
 * Post: buffer pcm is filled with n stereo samples
 */

void device_collect(struct device *dv, signed short *pcm, size_t n)
{
    assert(dv->player != NULL);
    player_collect(dv->player, pcm, n);
}
