/*
 * vmon - Virtual Machine MONitor speedup helper for VDSM
 * Copyright (C) 2014 Red Hat, Inc.
 * Written by Francesco Romani <fromani@redhat.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program;
 * if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "threading.h"


void
event_init(Event *ev)
{
    pthread_mutex_init(&ev->lock, NULL);
    pthread_cond_init(&ev->cond, NULL);
    ev->nwaiters = 0;
    ev->flag = FALSE;
}

void
event_set(Event *ev)
{
    pthread_mutex_lock(&ev->lock);
    ev->flag = TRUE;
    if (ev->nwaiters) {
        pthread_cond_signal(&ev->cond);
    }
    pthread_mutex_unlock(&ev->lock);
}

void
event_clear(Event *ev)
{
    pthread_mutex_lock(&ev->lock);
    ev->flag = FALSE;
    pthread_mutex_unlock(&ev->lock);
}

gboolean
event_is_set(Event *ev)
{
    gboolean ret;
    pthread_mutex_lock(&ev->lock);
    ret = ev->flag;
    pthread_mutex_unlock(&ev->lock);
    return ret;
}

static void
timespec_addms(struct timespec *ts, long ms)
{
	int sec = ms / 1000;
	ms = ms - sec * 1000;

	ts->tv_nsec += ms * 1000000;

	ts->tv_sec += ts->tv_nsec / 1000000000 + sec;
	ts->tv_nsec = ts->tv_nsec % 1000000000;
}

gboolean
event_wait(Event *ev, gint timeout) /* ms */
{
    int rc = 0;
    struct timespec ts;
    gboolean ret;
    pthread_mutex_lock(&ev->lock);

    clock_gettime(CLOCK_REALTIME, &ts);
    timespec_addms(&ts, timeout);

    ev->nwaiters++;
    while (!ev->flag && rc == 0) {
        g_message("waiting...");
        rc = pthread_cond_timedwait(&ev->cond, &ev->lock, &ts);
        g_message("waited %i ms: flag=%s rc=%i",
                  timeout, (ev->flag) ?"true" :"false", rc);
    }
    ev->nwaiters--;

    ret = ev->flag;
    pthread_mutex_unlock(&ev->lock);
    return ret;
}

