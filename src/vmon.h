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

#ifndef VMON_H
#define VMON_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>

#include <uuid.h>
#include <glib.h>
#include <libvirt/libvirt.h>

#include "executor.h"
#include "vmonlib.h"


typedef struct SampleRequest SampleRequest;
struct SampleRequest {
    uuid_t uuid;
    unsigned int stats;
};

typedef struct VmonConfig VmonConfig;
struct VmonConfig {
    int threads;
    int tasks;
    int timeout; /* seconds */
    int period; /* seconds */
    int log_level;
    gchar *log_file;
    int bulk_sampling;
    int disk_usage_perc;
    int events_only;
};

typedef struct VmonContext VmonContext;
struct VmonContext {
    VmonConfig conf;

    FILE *log;

    virConnectPtr conn;
    FILE *out;
    int flags;

    GMainLoop *loop;
    GIOChannel *io;
    guint io_watch_id;
    guint polling_id;

    Executor *executor;
    Scheduler *scheduler;

    unsigned long counter;
};

typedef struct VmonRequest VmonRequest;
struct VmonRequest {
    VmonContext *ctx;
    SampleRequest sr;
    virDomainPtr dom;
    virDomainStatsRecordPtr *records;
    int records_num;
};

#endif /* VMON_H */

