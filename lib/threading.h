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

#ifndef THREADING_H
#define THREADING_H

#include <time.h>

#include <glib.h>


typedef struct Event Event;
struct Event {
    pthread_mutex_t lock;
    pthread_cond_t cond;
    gint nwaiters;
    gboolean flag;
};

void
event_init(Event *ev);

void
event_set(Event *ev);

void
event_clear(Event *ev);

gboolean
event_is_set(Event *ev);

gboolean
event_wait(Event *ev, gint timeout); /* ms */

#endif /* THREADING_H */

