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
#include <string.h>

#include <stdint.h>
#include <pthread.h>

#include "vmon.h"
#include "vmon_int.h"
#include "ringbuffer.h"
#include "scheduler.h"
#include "executor.h"


typedef struct Worker Worker;

typedef unsigned long WorkerID;

struct Executor {
    Worker *workers;
    WorkerID workers_count;
    RingBuffer *tasks;
    Scheduler *scheduler;
    int running;
    pthread_mutex_t lock;
    size_t max_data;
};

static int
executor_replace(Executor *exc, WorkerID worker_id);


struct Worker {
    WorkerID id;
    Executor *executor;
    Scheduler *scheduler;
    pthread_t thread;
    guint sched_id;
    TaskData *current;
};

static gint
StopWorker(gpointer data)
{
    UNUSED(data);
    g_message("stopping worker");
    return 1;
}

static gint StopCollect(gpointer data, gint error, gboolean timeout)
{
    UNUSED(data);
    g_message("collecting worker stop: error=%i in_timeout=%s",
              error, (timeout) ?"yes" :"no");
    return error;
}

static gboolean
worker_discard(gpointer w)
{
    Worker *wo = w;
    g_message("issuing discard for worker: %lu", wo->id);
    if (wo->current) { /* paranoia */
        wo->current->td.discarded = 1;
    }
    executor_replace(wo->executor, wo->id);
    return FALSE;
}


static int
worker_execute(Worker *wo)
{
    TaskData task;
    int err = 0;

    memset(&task, 0, sizeof(task));

    err = ringbuffer_get(wo->executor->tasks, &task);
    if (!err) {
        void *data = (task.ud.xdata) ?task.ud.xdata :task.data;
        int timeout = task.td.timeout;

        if (task.td.timeout) {
            wo->sched_id = scheduler_add(wo->scheduler,
                                         timeout,
                                         worker_discard,
                                         wo);
            g_message("timeout for worker: %lu = %ims (sched_id=%u)",
                      wo->id, timeout, wo->sched_id);
        }
        /* FIXME: scheduler_add failed */

        wo->current = &task;
        err = task.td.work(data);
        wo->current = NULL;

        g_message("worker done: timeout=%i discarded=%i",
                  timeout, task.td.discarded);

        if (timeout && !task.td.discarded) {
            g_message("deleting timeout for worker: %lu (sched_id=%u)",
                      wo->id, wo->sched_id);
            scheduler_del(wo->scheduler, wo->sched_id);
        }
        err = task.td.collect(data, err, task.td.discarded);
    }

    g_message("worker executed err=%i", err);
    return err;
}

static void *
worker_run(void *w)
{
    Worker *wo = w;
    gint err = 0;

    g_message("worker %lu started", wo->id);

    while (!err) {
        err = worker_execute(wo);
    }

    g_message("worker %lu done", wo->id);
    return NULL;
}

static int
worker_init(Worker *wo, int id, Executor *exec, Scheduler *sched)
{
    memset(wo, 0, sizeof(*wo) + exec->max_data);
    wo->id = id;
    wo->executor = exec;
    wo->scheduler = sched;
    return pthread_create(&wo->thread, 0, worker_run, wo);
}

static int
worker_join(Worker *wo, int wait)
{
    if (wait && wo->current) {
        return -1;
    }
    return pthread_join(wo->thread, NULL);
}

int
executor_init(Executor **exc, Scheduler *sched,
              int workers_count, int max_tasks)
{
    int err = -1;
    Executor *ex = calloc(1, sizeof(*ex));
    if (ex) {
        pthread_mutex_init(&ex->lock, 0);

        ex->scheduler = sched;

        err = ringbuffer_init(&ex->tasks, max_tasks, sizeof(TaskData));

        ex->workers_count = workers_count;

        ex->workers = calloc(workers_count, sizeof(Worker));
        *exc = ex;
        err = 0;
        g_message("executor started with %i workers", workers_count);
    }
    return err;
}

int
executor_free(Executor *exc)
{
    UNUSED(exc);
    return 0; // TODO
}

int
executor_start(Executor *exc)
{
    int err = 0;
    size_t j;

    if (exc->running) {
        return EXECUTOR_ERROR_ALREADY_STARTED;
    }

    exc->running = 1;

    pthread_mutex_lock(&exc->lock);

    for (j = 0; j < exc->workers_count; j++) {
        err = worker_init(exc->workers + j, j, exc, exc->scheduler);
    }

    pthread_mutex_unlock(&exc->lock);
    return err;
}

static int
executor_stop_worker(Executor *exc)
{
    TaskData task;
    memset(&task, 0, sizeof(task));
    task.td.work = StopWorker;
    task.td.collect = StopCollect;
    return ringbuffer_put(exc->tasks, &task);
}

int
executor_stop(Executor *exc, int wait)
{
    int err = 0;
    size_t j;

    for (j = 0; j < exc->workers_count; j++) {
        executor_stop_worker(exc);
    }

    if (wait) {
        for (j = 0; j < exc->workers_count; j++) {
            err = worker_join(exc->workers + j, wait);
        }
    }

    exc->running = 0;
    return err;
}

int
executor_dispatch(Executor *exc, TaskFunction work, TaskCollect collect,
                  void *data, size_t size, int timeout)
{
    TaskData task;
    memset(&task, 0, sizeof(task));

    if (size > TASK_DATA_EMBED_MAX_SIZE) {
        g_message("could not embed task data: %lu > %i", /* FIXME */
                  size, TASK_DATA_EMBED_MAX_SIZE);
        return EXECUTOR_ERROR_TOO_MUCH_DATA; /* FIXME */
    }
    if (!exc->running) {
        return EXECUTOR_ERROR_NOT_RUNNING;
    }

    task.td.work = work;
    task.td.collect = collect;
    task.td.timeout = timeout;
    task.ud.size = size;
    task.ud.xdata = NULL;
    memcpy(task.data, data, size);
    return ringbuffer_put(exc->tasks, &task);
}

static int
executor_replace(Executor *exc, WorkerID worker_id)
{
    int ret;
    pthread_mutex_lock(&exc->lock);
    if (worker_id < exc->workers_count) {
        g_warning("replacing worker: %lu", worker_id);
        ret = worker_init(exc->workers + worker_id, worker_id, exc, exc->scheduler);
    } else {
        ret = -1;
    }
    pthread_mutex_unlock(&exc->lock);
    return ret;
}


typedef void (*rb_dump)(void *ud, const void *item);

extern void
ringbuffer_set_dump(RingBuffer *rb, rb_dump dump, void *ud);

void
executor_set_dump(Executor *exc, rb_dump dump, void *ud)
{
    ringbuffer_set_dump(exc->tasks, dump, ud);
}

