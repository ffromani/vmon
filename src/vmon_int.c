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

#include <time.h>
#include <sys/time.h>

#include "sampler.h"
#include "vmon_int.h"



static void
vmon_log(const gchar *log_domain,
         GLogLevelFlags log_level,
         const gchar *message,
         gpointer user_data);


void
vmon_setup_log(VmonContext *ctx)
{
    g_log_set_handler ("vmon", G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL
                       | G_LOG_FLAG_RECURSION, vmon_log, ctx);

    if (ctx->conf.log_file) {
        ctx->log = fopen(ctx->conf.log_file, "at");
        if (!ctx->log) {
            g_error("failed to open log file '%s'", ctx->conf.log_file);
        }
        g_free(ctx->conf.log_file); /* either way no longer needed */
    } else {
        ctx->log = stderr;
    }

    g_info("vmon startup");
}

void
vmon_teardown_log(VmonContext *ctx)
{
    g_info("vmon exiting");

    fflush(ctx->log);
    if (ctx->log != stderr) {
        fclose(ctx->log);
    }
}

static const char *
g_io_cond_to_str(GIOCondition c)
{
    switch (c) {
    case G_IO_IN: return "r";
    case G_IO_OUT: return "w";
    case G_IO_PRI: return "p";
    case G_IO_ERR: return "!";
    case G_IO_HUP: return "^";
    case G_IO_NVAL: return "_";
    }
    return "?";
}

static gboolean
vmon_io_callback(GIOChannel *io, GIOCondition condition, gpointer data)
{
    VmonContext *ctx = data;
    gchar *line = NULL;
    gsize size = 0;
    gsize terminator_pos = 0;
    GError *error = NULL;
    gboolean ret = FALSE;
    int err;

    g_debug("fired: %s", g_io_cond_to_str(condition));

    switch (g_io_channel_read_line(io, &line, &size, &terminator_pos, &error)) {
    case G_IO_STATUS_NORMAL:
      err = sampler_handle_request(ctx, line, size);
      if (!err) {
        ret = TRUE;
      } else {
        g_warning("error handling request: %i", err);
        g_main_loop_quit(ctx->loop);
      }
      break;

    case G_IO_STATUS_ERROR:
      g_warning("IO error: %s", error->message);
      g_error_free(error);
      break;

    case G_IO_STATUS_EOF:
      g_warning("no input data available");
      vmon_teardown_log(ctx);
      g_main_loop_quit(ctx->loop);
      break;

    case G_IO_STATUS_AGAIN:
      ret = TRUE;
      break;

    default:
      break;
  }

  g_free(line);
  g_message("done");

  return ret;
}

static const char *
g_log_level_to_str(GLogLevelFlags log_level)
{
    switch (log_level) {
    case G_LOG_LEVEL_ERROR:     return "ERR";
    case G_LOG_LEVEL_CRITICAL:  return "CRI";
    case G_LOG_LEVEL_WARNING:   return "WRN";
    case G_LOG_LEVEL_MESSAGE:   return "MSG";
    case G_LOG_LEVEL_INFO:      return "INF";
    case G_LOG_LEVEL_DEBUG:     return "DBG";
    default:                    return "???";
    }
    return "!!!";
}

static gboolean
log_enabled(guint current, guint configured)
{
    /* internal flags are mapped on LSB (0, 1) */
    return (current & 0xFC) <= (configured & 0xFC);
}

static void
vmon_log(const gchar *log_domain,
         GLogLevelFlags log_level,
         const gchar *message,
         gpointer user_data)
{
    char            fmt[64], buf[64];
    struct timeval  tv;
    struct tm       tmbuf;
    VmonContext     *ctx = user_data;

    if (!log_enabled(log_level, ctx->conf.log_level)) {
        return;
    }

    gettimeofday(&tv, NULL);
    localtime_r(&tv.tv_sec, &tmbuf);
    strftime(fmt, sizeof(fmt), "%Y-%m-%d %H:%M:%S.%%06u", &tmbuf);
    snprintf(buf, sizeof(buf), fmt, tv.tv_usec);

    fprintf(ctx->log,
            "%s [%s] %s: %s\n",
            buf,
            g_log_level_to_str(log_level),
            log_domain,
            message);
}
                            
void
vmon_init(void)
{
#if !GLIB_CHECK_VERSION(2, 32, 0)
    g_thread_init(NULL);
#endif
}

static gboolean
poll_libvirt(gpointer ud)
{
    int err = 0;
    VmonContext *ctx = ud;
    VmonRequest req;
    memset(&req, 0, sizeof(req));

    req.ctx = ctx;
    uuid_generate(req.sr.uuid);

    err = sampler_send_request(ctx, &req);
    if (err) {
        g_warning("error polling libvirt: %i", err);
    } else {
        g_message("polling libvirt: loop #%zu", ctx->counter);
        ctx->counter++;
    }

    return TRUE;
}


void
vmon_setup_io(VmonContext *ctx)
{
    ctx->loop = g_main_loop_new(NULL, FALSE);

    if (ctx->conf.period) {
        ctx->polling_id = scheduler_add(ctx->scheduler,
                                        ctx->conf.period * 1000,
                                        poll_libvirt,
                                        ctx);
    } else {
        ctx->io = g_io_channel_unix_new(STDIN_FILENO);
        ctx->io_watch_id = g_io_add_watch(ctx->io,
                                          G_IO_IN|G_IO_HUP|G_IO_ERR,
                                          vmon_io_callback,
                                          ctx);
        //g_io_channel_unref(ctx->io);
    }
    return;
}

