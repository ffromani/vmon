/*
 * vmon - Virtual Machine MONitor speedup helper for VDSM
 * Copyright (C) 2014-2015 Red Hat, Inc.
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

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>

#include <glib.h>


typedef struct Scheduler Scheduler;

/* automatically rescheduled until returns FALSE */
typedef gboolean (*ScheduledFunction)(gpointer data);

/*
 * standalone if you DO NOT want integration with implicit
 * main loop from glib
 */
int scheduler_init(Scheduler **sched, gboolean standalone);

int scheduler_free(Scheduler *sched);

int scheduler_start(Scheduler *sched);

int scheduler_stop(Scheduler *sched, int wait);

/*
 * delay: milliseconds
 * returns 0 on error
 */
guint scheduler_add(Scheduler *sched,
                    int delay,
                    ScheduledFunction task,
                    gpointer data);

int scheduler_del(Scheduler *sched, guint id);


#endif /* SCHEDULER_H */

