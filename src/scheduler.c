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

#include <stdlib.h>

#include "scheduler.h"
#include "vmon_int.h"


struct Scheduler {
    GMainContext *context;
    GMainLoop *loop;
    GThread *thread;
    gboolean standalone;
};


int scheduler_init(Scheduler **sched, gboolean standalone)
{
    int err = -1;
    Scheduler *sc = calloc(1, sizeof(*sc));
    if (sc) {
        *sched = sc;
        sc->standalone = standalone;
        err = 0;
    }
    return err;
}

int scheduler_free(Scheduler *sched)
{
    free(sched);
    return 0;
}

static void *
scheduler_run(void *data)
{
    Scheduler *sched = data;

    sched->context = g_main_context_new();
    g_main_context_push_thread_default(sched->context);

    sched->loop = g_main_loop_new(sched->context, FALSE);

    g_main_loop_run(sched->loop);

    g_main_loop_quit(sched->loop);

    return NULL;

}

static int
scheduler_start_standalone(Scheduler *sched)
{
    int err = 0;
    GError *error = NULL;
    sched->thread = g_thread_try_new("scheduler", scheduler_run, sched, &error);
    if (sched->thread) {
        g_message("standalone scheduler started");
    } else {
        g_message("standalone scheduler start failed: %s", error->message);
        g_error_free(error);
        err = -1;
    }
    return err;
}

int scheduler_start(Scheduler *sched)
{
    if (!sched->standalone) {
        sched->context = g_main_context_default();
        sched->loop = NULL; /* MUST be null */
        return 0;
    }
    return scheduler_start_standalone(sched);
}

int scheduler_stop(Scheduler *sched, int wait)
{
    UNUSED(wait);
    if (sched->standalone) {
        g_main_loop_quit(sched->loop);
        g_thread_join(sched->thread);
    }
    return 0; /* TODO */
}

guint scheduler_add(Scheduler *sched, int delay, ScheduledFunction task, gpointer data)
{
    guint id;
    GSource *source = g_timeout_source_new(delay); /* ms */

    g_source_set_callback(source, task, data, NULL);
    id = g_source_attach(source, sched->context);
    g_source_unref(source);

    return id;
}

int scheduler_del(Scheduler *sched, guint id)
{
    UNUSED(sched);
    g_source_remove(id);
    return 0;
}

