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

#include <string.h>

#include <unistd.h>

#include "sampler.h"
#include "vmon_int.h"


static VmonContext ctx;


static void
config_defaults(VmonConfig *conf)
{
    memset(conf, 0, sizeof(*conf));
    conf->timeout =  TIMEOUT;
    conf->threads = MAX_THREADS;
    conf->tasks = MAX_THREADS * TASKS_PER_THREAD;
    conf->period = 0; /* better explicit than implicit */
}


static int
config_parse_cmdline(VmonConfig *conf, int argc, char *argv[])
{
    GOptionEntry entries[] = {
        {
            "timeout", 'T', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT,
            &conf->timeout, "Timeout (seconds). 0 to disable", "TIMEOUT"
        },
        {
            "max-tasks", 't', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT,
            &conf->tasks, "Maximum amount of tasks to be queued", "MAX_TASKS"
        },
        {
            "max-threads", 'c', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT,
            &conf->threads, "Max threads to be used", "MAX_THREADS"
        },
        {
            "polling-period", 'p', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT,
            &conf->period, "Autonomously poll libvirt (seconds)", "PERIOD"
        },
        {
            "log-level", 'd', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT,
            &conf->log_level, "Control the amount of logging", "LEVEL"
        },
        {
            "log-file", 'l', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING,
            &conf->log_file, "Send log to (default: stderr)", "FILE"
        },
        {
            "bulk-sampling", 'B', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
            &conf->bulk_sampling, "Use bulk sampling", NULL
        },
        {
            "disk-usage-monitor", 'U', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT,
            &conf->disk_usage_perc, "Deliver events when disk usage exceeds PERCentage of the physical size", "PERC"
        },
        {
            "events-only", 'E', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
            &conf->events_only, "Send in output only events", NULL
        },
        { NULL }
    };

    GError *error = NULL;
    GOptionContext *context;
    int ret = -1;

    context = g_option_context_new("- vm sampling speedup tool");
    g_option_context_add_main_entries(context, entries, NULL);

    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_print("option parsing failed: %s\n", error->message);
        goto clean;
    }

    if (conf->threads <= 0) {
      g_print("option 'max-threads' cannot be negative\n");
      goto clean;
    }

    if (conf->tasks <= 0) {
      g_print("option 'max-tasks' cannot be negative\n");
      goto clean;
    }

    if (conf->disk_usage_perc < 0 || conf->disk_usage_perc > 99) {
      g_print("option 'disk-usage-monitor' must be in range [0,99]");
      goto clean;
    }

    ret = 0;

clean:
    g_option_context_free(context);

    return ret;
}

static void
context_defaults(VmonContext *ctx)
{
    memset(ctx, 0, sizeof(*ctx));

    ctx->out = stdout;
    ctx->flags |= VIR_CONNECT_GET_ALL_DOMAINS_STATS_ACTIVE;
    ctx->flags |= VIR_CONNECT_GET_ALL_DOMAINS_STATS_RUNNING;
    ctx->flags |= VIR_CONNECT_GET_ALL_DOMAINS_STATS_PAUSED;
}

int
main(int argc, char *argv[])
{
    int err = 0;

    vmon_init();

    context_defaults(&ctx);

    config_defaults(&ctx.conf);
    if (config_parse_cmdline(&ctx.conf, argc, argv) < 0) {
        g_critical("failed to parse the command line arguments");
        err = -1;
        goto done;
    }

    vmon_setup_log(&ctx);

    g_message("starting vmon with %i threads and %i tasks",
              ctx.conf.threads, ctx.conf.tasks);

    g_message("connecting to libvirt...");

    ctx.conn = virConnectOpenReadOnly("qemu:///system");
    if (ctx.conn == NULL) {
        g_critical("failed to open connection to libvirt");
        err = -1;
        goto done;
    }

    g_message("connected to libvirt!");

    err = scheduler_init(&ctx.scheduler, FALSE);
    if (err) {
        g_critical("failed to initialize the task scheduler");
        err = -1;
        goto cleanup_libvirt;
    }

    err = scheduler_start(ctx.scheduler);
    if (err) {
        g_critical("failed to start the task scheduler");
        err = -1;
        goto cleanup_sched;
    }

    err = executor_init(&ctx.executor,
                        ctx.scheduler,
                        ctx.conf.threads,
                        ctx.conf.tasks);
    if (err) {
        g_critical("failed to initialze the task executor");
        err = -1;
        goto cleanup_sched;
    }

    err = executor_start(ctx.executor);
    if (err) {
        g_critical("failed to start the the task executor");
        err = -1;
        goto cleanup_exec;
    }

    vmon_setup_io(&ctx);

    g_message("running");

    g_main_loop_run(ctx.loop);
  
    g_main_loop_unref(ctx.loop);

    scheduler_stop(ctx.scheduler, TRUE);
    executor_stop(ctx.executor, TRUE);

    g_message("about to disconnected from libvirt...");

cleanup_exec:
    executor_free(ctx.executor);

cleanup_sched:
    scheduler_free(ctx.scheduler);

cleanup_libvirt:
    virConnectClose(ctx.conn);

    g_message("disconnected from libvirt.");

done:
    return err;
}

