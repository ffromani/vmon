/*
 * vmon - Virtual Machine MONitor for oVirt (et. al.)
 * Copyright (C) 2014-2016 Red Hat, Inc.
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

#ifndef EXECUTOR_H
#define EXECUTOR_H

#include <stdio.h>
#include <stdlib.h>

#include <glib.h>

#include <libvirt/libvirt.h>

#include "scheduler.h"


typedef gint (*TaskFunction)(gpointer data);

typedef gint (*TaskCollect)(gpointer data, gint error, gboolean timeout);

typedef struct TaskBaseData TaskBaseData;
struct TaskBaseData {
    TaskFunction work;
    TaskCollect collect;
    gint timeout;
    gboolean discarded;
};

typedef struct TaskUserData TaskUserData;
struct TaskUserData {
    void *xdata;
    size_t size;
};

enum {
    TASK_DATA_SIZE = 128, /* keep this multiple of 2 */
    TASK_DATA_EMBED_MAX_SIZE = TASK_DATA_SIZE - sizeof(TaskBaseData) - sizeof(TaskUserData)
};

typedef struct TaskData TaskData;
struct TaskData {
    TaskBaseData td;
    TaskUserData ud;
    uint8_t data[TASK_DATA_EMBED_MAX_SIZE];
};


enum {
    EXECUTOR_ERROR_NONE = 0,
    EXECUTOR_ERROR_NOT_RUNNING = -1,
    EXECUTOR_ERROR_ALREADY_STARTED = -2,
    EXECUTOR_ERROR_TOO_MANY_TASKS = -3,
    EXECUTOR_ERROR_TOO_MUCH_DATA = -4
};

typedef struct Executor Executor;


int
executor_init(Executor **exc,
              Scheduler *sched,
              int workers_count,
              int max_tasks);


int
executor_free(Executor *exc);


int
executor_start(Executor *exc);


int
executor_stop(Executor *exc, int wait);


int
executor_dispatch(Executor *exc,
                  TaskFunction work,
                  TaskCollect collect,
                  void *data,
                  size_t size,
                  int timeout);

#endif /* EXECUTOR_H */

