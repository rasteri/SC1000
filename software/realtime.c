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
#include <unistd.h>        //Needed for I2C port
#include <fcntl.h>         //Needed for I2C port
#include <sys/ioctl.h>     //Needed for I2C port
#include <linux/i2c-dev.h> //Needed for I2C port

#include "controller.h"
#include "debug.h"
#include "device.h"
#include "realtime.h"
#include "thread.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*x))

int file_i2c;
unsigned char buf[60] = {0};

/*
 * Raise the priority of the current thread
 *
 * Return: -1 if priority could not be satisfactorily raised, otherwise 0
 */

static int raise_priority(int priority)
{
    int max_pri;
    struct sched_param sp;

    max_pri = sched_get_priority_max(SCHED_FIFO);

    if (priority > max_pri)
    {
        fprintf(stderr, "Invalid scheduling priority (maximum %d).\n", max_pri);
        return -1;
    }

    if (sched_getparam(0, &sp))
    {
        perror("sched_getparam");
        return -1;
    }

    sp.sched_priority = priority;

    if (sched_setscheduler(0, SCHED_FIFO, &sp))
    {
        perror("sched_setscheduler");
        fprintf(stderr, "Failed to get realtime priorities\n");
        return -1;
    }

    return 0;
}

/*
 * The realtime thread
 */

static void rt_main(struct rt *rt)
{
    int r;
    size_t n;

    debug("%p", rt);

    thread_to_realtime();

    if (rt->priority != 0)
    {
        if (raise_priority(rt->priority) == -1)
            rt->finished = true;
    }

    if (sem_post(&rt->sem) == -1)
        abort(); /* under our control; see sem_post(3) */

    while (!rt->finished)
    {
        r = poll(rt->pt, rt->npt, -1);
        if (r == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }
            else
            {
                perror("poll2");
                abort();
            }
        }

        for (n = 0; n < rt->nctl; n++)
            controller_handle(rt->ctl[n]);

        /*for (n = 0; n < rt->ndv; n++)
            device_handle(rt->dv[n]);
*/

        device_handle(rt->dv[0]);

        /*

*/
    }
}

static void *launch(void *p)
{
    rt_main(p);
    return NULL;
}

/*
 * Initialise state of realtime handler
 */

void rt_init(struct rt *rt)
{
    debug("%p", rt);

    rt->finished = false;
    rt->ndv = 0;
    rt->nctl = 0;
    rt->npt = 0;
}

/*
 * Clear resources associated with the realtime handler
 */

void rt_clear(struct rt *rt)
{
}

/*
 * Add a device to this realtime handler
 *
 * Return: -1 if the device could not be added, otherwise 0
 * Post: if 0 is returned the device is added
 */

int rt_add_device(struct rt *rt, struct device *dv)
{
    ssize_t z;

    debug("%p adding device %p", rt, dv);

    if (rt->ndv == ARRAY_SIZE(rt->dv))
    {
        fprintf(stderr, "Too many audio devices\n");
        return -1;
    }

    /* The requested poll events never change, so populate the poll
     * entry table before entering the realtime thread */

    z = device_pollfds(dv, &rt->pt[rt->npt], sizeof(rt->pt) - rt->npt);
    if (z == -1)
    {
        fprintf(stderr, "Device failed to return file descriptors.\n");
        return -1;
    }

    rt->npt += z;

    rt->dv[rt->ndv] = dv;
    rt->ndv++;

    return 0;
}

/*
 * Add a controller to the realtime handler
 *
 * Return: -1 if the device could not be added, otherwise 0
 */

int rt_add_controller(struct rt *rt, struct controller *c)
{
    ssize_t z;

    debug("%p adding controller %p", rt, c);

    if (rt->nctl == ARRAY_SIZE(rt->ctl))
    {
        fprintf(stderr, "Too many controllers\n");
        return -1;
    }

    /* Similar to adding a PCM device */

    z = controller_pollfds(c, &rt->pt[rt->npt], sizeof(rt->pt) - rt->npt);
    if (z == -1)
    {
        fprintf(stderr, "Controller failed to return file descriptors.\n");
        return -1;
    }

    rt->npt += z;
    rt->ctl[rt->nctl++] = c;

    return 0;
}

/*
 * Start realtime handling of the given devices
 *
 * This forks the realtime thread if it is required (eg. ALSA). Some
 * devices (eg. JACK) start their own thread.
 *
 * Return: -1 on error, otherwise 0
 */

int rt_start(struct rt *rt, int priority)
{
    size_t n;

    assert(priority >= 0);
    rt->priority = priority;

    /* If there are any devices which returned file descriptors for
     * poll() then launch the realtime thread to handle them */

    if (rt->npt > 0)
    {
        int r;

        fprintf(stderr, "Launching realtime thread to handle devices...\n");

        if (sem_init(&rt->sem, 0, 0) == -1)
        {
            perror("sem_init");
            return -1;
        }

        r = pthread_create(&rt->ph, NULL, launch, (void *)rt);
        if (r != 0)
        {
            errno = r;
            perror("pthread_create");
            if (sem_destroy(&rt->sem) == -1)
                abort();
            return -1;
        }

        /* Wait for the realtime thread to declare it is initialised */

        if (sem_wait(&rt->sem) == -1)
            abort();
        if (sem_destroy(&rt->sem) == -1)
            abort();

        if (rt->finished)
        {
            if (pthread_join(rt->ph, NULL) != 0)
                abort();
            return -1;
        }
    }

    for (n = 0; n < rt->ndv; n++)
        device_start(rt->dv[n]);

    return 0;
}

/*
 * Stop realtime handling, which was previously started by rt_start()
 */

void rt_stop(struct rt *rt)
{
    size_t n;

    rt->finished = true;

    /* Stop audio rolling on devices */

    for (n = 0; n < rt->ndv; n++)
        device_stop(rt->dv[n]);

    if (rt->npt > 0)
    {
        if (pthread_join(rt->ph, NULL) != 0)
            abort();
    }
}
