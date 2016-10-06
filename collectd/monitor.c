/**
 * vmon - collectd/monitor.c
 * Copyright (C) 2016 Francesco Romani <fromani at redhat.com>
 * Based on
 * collectd - src/unixsock.c
 * Copyright (C) 2007,2008  Florian octo Forster
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *   Francesco Romani <fromani at redhat.com>
 *   Florian octo Forster <octo at collectd.org> (unixsock)
 **/

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <grp.h>

#include <pthread.h>

#include "collectd.h"

#ifdef __GNUC__
#define UNUSED __attribute__((unused))
#endif

#ifndef UNIX_PATH_MAX
# define UNIX_PATH_MAX sizeof (((struct sockaddr_un *)0)->sun_path)
#endif

#define DEFAULT_GROUP_NAME "root"
#define DEFAULT_SOCKET_PATH "/var/run/collectd-mon.sock"
#define DEFAULT_SOCKET_PERMS (S_IRWXU|S_IRWXG)
#define PLUGIN_NAME "monitor"

/*
 * Private variables
 */
/* valid configuration file keys */
static const char *config_keys[] =
{
	"SocketFile",
	"SocketGroup",
	"SocketPerms",
	"DeleteSocket",
    "OutputFormat",
};
static int config_keys_num = STATIC_ARRAY_SIZE (config_keys);

enum {
    OUTPUT_FORMAT_MINIMAL = 0,
    OUTPUT_FORMAT_TERSE,
    OUTPUT_FORMAT_FULL,
};


#define MESSAGE_MAX_TEXT ((256) - sizeof(void*))


typedef struct item_s item_t;
struct item_s {
    item_t *next;
};

typedef struct itempair_s itempair_t;
struct itempair_s {
    item_t *head;
    item_t *tail;
};

typedef struct queue_s queue_t;
struct queue_s {
    itempair_t q;
    pthread_mutex_t lock;
    pthread_cond_t empty;
    size_t waiters;
    size_t payload_size;
};


static int queue_init(queue_t *queue, size_t payload_size)
{
    int err = -1;
    if (queue && payload_size) {
        queue->payload_size = payload_size;
        queue->waiters = 0;
        queue->q.head = NULL;
        queue->q.tail = NULL;
        pthread_mutex_init(&queue->lock, NULL);
        pthread_cond_init(&queue->empty, NULL);
    }
    return err;
}

static int queue_item_new(queue_t *queue, item_t **item, const void *data, size_t len)
{
    if (!queue || !item || !data || !len) {
        return -1;
    }
    if (len > queue->payload_size) {
        return -2;
    }
    void *buf = calloc(1, sizeof(item_t) + queue->payload_size);
    if (!buf) {
        return -3;
    }

    *item = (item_t *)buf;
    void *payload = ((uint8_t *)buf) + sizeof(item_t);
    memcpy(payload, data, len);

    return 0;
}

static void itempair_append(itempair_t *ip, item_t *item)
{
    if (ip->head == NULL) {
        ip->head = item;
    } else {
        ip->tail->next = item;
    }
    ip->tail = item;
    return;
}

static void itempair_fix(itempair_t *ip)
{
    if (ip->head == NULL) {
        ip->tail = NULL;
    }
    return;
}

static int queue_push_tail(queue_t *queue, const void *data, size_t len)
{
    item_t *item = NULL;
    int err = queue_item_new(queue, &item, data, len);
    if (err) {
        return err;
    }

    pthread_mutex_lock(&queue->lock);
    itempair_append(&queue->q, item);
    if (queue->waiters > 0) {
        pthread_cond_signal(&queue->empty);
    }
    pthread_mutex_unlock(&queue->lock);
    return 0;
}

static int queue_pop_head(queue_t *queue, void *data, size_t len)
{
    item_t *item = NULL;
    if (!queue || !data || !len) {
        return -1;
    }

    pthread_mutex_lock(&queue->lock);
    queue->waiters++;
    while (queue->q.head == NULL) {
        pthread_cond_wait(&queue->empty, &queue->lock);
    }
    queue->waiters--;
        
    item = queue->q.head;
    queue->q.head = queue->q.head->next;
    itempair_fix(&queue->q);
    pthread_mutex_unlock(&queue->lock);

    memcpy(data, ((uint8_t *)(item)) + sizeof(item_t), len);
    free(item);
    if (len < queue->payload_size) {
        return 1;
    }
    return 0;
}

/* 
 * Visitor:
 * !0 => abort
 *  0 => go ahead
 */
static int queue_foreach(queue_t *queue,
                         int (*visitor)(void *payload, void *data), void *data)
{
    if (!queue || !visitor) {
        return -1;
    }

    int err = 0;
    pthread_mutex_lock(&queue->lock);
    for (item_t *item = queue->q.head; !err && item != NULL; item = item->next) {
        void *payload = ((uint8_t *)(item)) + sizeof(item_t);
        err = visitor(payload, data);
        if (err) {
            break;
        }
            
    }
    pthread_mutex_unlock(&queue->lock);
    return err;
}

static int queue_free(queue_t *queue,
                      int (*visitor)(void *payload, void *data), void *data)
{
    if (!queue || !visitor) {
        return -1;
    }

    int err = 0;
    pthread_mutex_lock(&queue->lock);

    itempair_t it = { NULL, NULL };
    item_t *item = queue->q.head;
    while (item != NULL) {
        item_t *next = item->next;
        void *payload = ((uint8_t *)(item)) + sizeof(item_t);
        err = visitor(payload, data);
        
        if (!err) {
            free(item);
        } else {
            itempair_append(&it, item);
        }
        item = next;
    }

    itempair_fix(&it);
    queue->q = it;
    pthread_mutex_unlock(&queue->lock);
    return err;
}


typedef struct client_s client_t;
struct client_s {
    FILE *fout;
    int fd;
    int done;
};

typedef struct server_s server_t;
struct server_s {
    const char *pathname;
    const char *groupname;
    int fd;
    int perms;
    int looping;
    int inited;
    int opened;
    int delete_socket;
    int output_format;
    /* TODO: proper event loop */
    pthread_t listener;
    pthread_t dispatcher;
    queue_t clients;
    queue_t messages;
};

static server_t default_server;

static int server_init(server_t *serv)
{
    if (!serv) {
        return -1;
    }
    if (serv->inited) {
        return 0;
    }

    serv->pathname = DEFAULT_SOCKET_PATH;
    serv->groupname = DEFAULT_GROUP_NAME;
    serv->fd = -1;
    serv->perms = DEFAULT_SOCKET_PERMS;
    serv->looping = 0;
    serv->inited = 1;
    serv->delete_socket = 0;
    serv->output_format = OUTPUT_FORMAT_TERSE;
    serv->listener = (pthread_t) 0;
    serv->dispatcher = (pthread_t) 0;
    queue_init(&serv->clients, sizeof(client_t));
    queue_init(&serv->messages, MESSAGE_MAX_TEXT);
    return 0;
}

static void *server_listener_body(void *arg);
static void *server_dispatcher_body(void *arg);


static int server_abort(server_t *serv, const char *reason)
{
	char errbuf[1024];
	sstrerror(errno, errbuf, sizeof(errbuf));
	ERROR(PLUGIN_NAME " plugin: %s: %s", reason, errbuf);
	close(serv->fd);
	serv->fd = -1;
    // TODO: fix the flags?
	return -1;
}

static int server_start_threads(server_t *serv)
{
    int err = 0;
	serv->looping = 1;

	err = plugin_thread_create(&serv->listener, NULL,
                               server_listener_body, NULL);
	if (err) {
        return server_abort(serv,
                            "plugin_thread_create(listener) failed");
	}

    err = plugin_thread_create(&serv->dispatcher, NULL,
                               server_dispatcher_body, NULL);
	if (err) {
        return server_abort(serv,
                            "plugin_thread_create(dispatcher) failed");
	}

	return err;
}

static int server_delete_socket(server_t *serv)
{
	errno = 0;
	int err = unlink(serv->pathname);
    if ((err != 0) && (errno != ENOENT)) {
		char errbuf[1024];
		WARNING(PLUGIN_NAME " plugin: Deleting socket file \"%s\" failed: %s",
			    serv->pathname,
			    sstrerror (errno, errbuf, sizeof (errbuf)));
	} else if (!err) {
		INFO(PLUGIN_NAME " plugin: deleted socket file \"%s\".",
             serv->pathname);
	}
    return err;
}

static int server_set_socket_group(server_t *serv)
{
	struct group *g = NULL;
	struct group sg;
	char grbuf[2048];
    int err = 0;

	err = getgrnam_r(serv->groupname, &sg, grbuf, sizeof (grbuf), &g);
	if (err) {
		char errbuf[1024];
		WARNING (PLUGIN_NAME " plugin: getgrnam_r(%s) failed: %s",
                 serv->groupname,
				 sstrerror(errno, errbuf, sizeof (errbuf)));
		return -1;
	}
	if (g == NULL) {
		WARNING(PLUGIN_NAME " plugin: No such group: `%s'",
				serv->groupname);
		return -1;
	}

	if (chown(serv->pathname, (uid_t) -1, g->gr_gid) != 0) {
		char errbuf[1024];
		WARNING (PLUGIN_NAME" plugin: chown (%s, -1, %i) failed: %s",
                 serv->pathname,
				 (int) g->gr_gid,
				 sstrerror(errno, errbuf, sizeof (errbuf)));
	}
    return 0;
}

static int server_open_socket(server_t *serv)
{
	struct sockaddr_un sa = { 0 };
	int err = 0;

	serv->fd = socket (PF_UNIX, SOCK_STREAM, 0);
	if (serv->fd < 0) {
        return server_abort(serv, "socket failed");
	}

	sa.sun_family = AF_UNIX;
	sstrncpy(sa.sun_path, serv->pathname, sizeof (sa.sun_path));

	DEBUG (PLUGIN_NAME " plugin: socket path = %s", sa.sun_path);

	if (serv->delete_socket) {
        server_delete_socket(serv);
	}

	err = bind (serv->fd, (struct sockaddr *) &sa, sizeof (sa));
	if (err) {
        return server_abort(serv, "bind failed");
	}

	err = chmod (sa.sun_path, serv->perms);
	if (err) {
        return server_abort(serv, "chmod failed");
	}

	err = listen (serv->fd, 8);
	if (err) {
        return server_abort(serv, "listen failed");
	}

    /* this may fail, and it is OK. */
    server_set_socket_group(serv);
    return 0;
}

static int server_close_socket(server_t *serv)
{
    int err = close(serv->fd);
    serv->fd = -1;
    return server_delete_socket(serv);
}

static int client_free(void *payload, void *data)
{
    client_t *cli = payload;
    if (cli->done) {
        DEBUG(PLUGIN_NAME
              " plugin: client=%d was done, removing", cli->fd);
        return 0;
    }
    return 1;
}

static void *server_listener_body(void *arg)
{
    server_t *serv = arg;
	while (serv->looping) {
		DEBUG (PLUGIN_NAME " plugin: accepting clients..");
		int fd = accept(serv->fd, NULL, NULL);
		if (fd < 0) {
			if (errno == EINTR) {
				continue;
            }
            server_abort(serv, "accept failed");
			pthread_exit(NULL);
		}

		DEBUG(PLUGIN_NAME " plugin: handling client=%d", fd);

        FILE *fout = fdopen(fd, "w");
        if (!fout) {
            WARNING(PLUGIN_NAME 
                    " plugin: client=%d fopen() failed, skipped",
                    fd);
            continue;
        }

        client_t cli = {
            .fd = fd,
            .fout = fout,
            .done = 0,
        };
        int err = queue_push_tail(&serv->clients, &cli, sizeof(cli));
        if (!err) {
            INFO(PLUGIN_NAME " plugin: client=%d registered", cli.fd);
        } else {
            WARNING(PLUGIN_NAME 
                    " plugin: client=%d register failed, error=%d",
                    cli.fd, err);
        }

        // janitorial duties
        queue_free(&serv->clients, client_free, NULL);
	}

    server_close_socket(serv);

	return NULL;
}

static int server_dispatch(void *payload, void *data)
{
    const char *text = data;
    client_t *cli = payload;
    if (cli->done) {
        return 0;
    }

    size_t len = strlen(text);
    size_t w = fwrite(text, 1, len, cli->fout);
    if (w != len) {
        INFO(PLUGIN_NAME " plugin: client=%d write failed, closing",
             cli->fd);
        fclose(cli->fout);
        cli->done = 1;
    } else {
        DEBUG(PLUGIN_NAME " plugin: sent to client=%d", cli->fd);
    }
    return 0;
}

static void *server_dispatcher_body(void *arg)
{
    server_t *serv = arg;
	while (serv->looping) {
        char text[MESSAGE_MAX_TEXT];

        int err = queue_pop_head(&serv->messages, text, sizeof(text));
        if (err) {
            WARNING(PLUGIN_NAME " plugin: message fetch failed error=%d",
                    err);
            continue;
        }

        err = queue_foreach(&serv->clients, server_dispatch, text);
    }

    return NULL;
}

static int server_stop_threads(server_t *serv)
{
	serv->looping = 0;

	if (serv->listener != (pthread_t) 0) {
        pthread_kill(serv->listener, SIGTERM);
		pthread_join(serv->listener, NULL);
		serv->listener = (pthread_t) 0;
	}
	if (serv->dispatcher != (pthread_t) 0) {
        pthread_kill(serv->listener, SIGTERM);
		pthread_join(serv->dispatcher, NULL);
		serv->dispatcher = (pthread_t) 0;
	}
	return 0;
}

static int strequals(const char *sa, const char *sb)
{
    return (strcasecmp(sa, sb) == 0);
}

#define SETSTRING(ATTR, VAL, DEF) do { \
	char *new_value = strdup((VAL)); \
	if (new_value == NULL) { \
		return 1; \
    } \
    if ((DEF) == NULL || (ATTR) != (DEF)) { \
		sfree((ATTR)); \
    } \
	(ATTR) = new_value; \
} while (0)


static int mon_config(const char *key, const char *val)
{
    server_t *serv = &default_server;
    server_init(serv);

	if (strequals(key, "SocketFile")) {
        SETSTRING(serv->pathname, val, DEFAULT_SOCKET_PATH);
	} else if (strequals(key, "SocketGroup")) {
        SETSTRING(serv->groupname, val, NULL);
	} else if (strequals(key, "SocketPerms")) {
		serv->perms = (int) strtol (val, NULL, 8);
	} else if (strequals(key, "DeleteSocket")) {
        serv->delete_socket = IS_TRUE((val));
    } else if (strequals(key, "OutputFormat")) {
        if (strequals(val, "minimal")) {
            serv->output_format = OUTPUT_FORMAT_MINIMAL;
        } else if (strequals(val, "terse")) {
            serv->output_format = OUTPUT_FORMAT_TERSE;
        } else if (strequals(val, "full")) {
            serv->output_format = OUTPUT_FORMAT_FULL;
        } else {
            return 1;
        }
	} else {
		return -1;
	}
	return 0;
}

static int mon_init(void)
{
    int err = 0;
    server_t *serv = &default_server; /* shortcut */
    server_init(serv);

    if (serv->opened) {
        WARNING(PLUGIN_NAME " plugin: mon_init already called");
        return 0;
    }

    err = server_open_socket(serv);
    if (err) {
        WARNING(PLUGIN_NAME "plugin: open socket failed error=%d", err); 
        return err;
    }
    err = server_start_threads(serv);
    if (err) {
        WARNING(PLUGIN_NAME " plugin: open socket failed error=%d", err); 
        return err;
    }

    serv->opened = 1;
    INFO(PLUGIN_NAME " plugin: initialized and ready");
    return 0;
}

static int mon_shutdown(void)
{
    server_t *serv = &default_server; /* shortcut */
    int err = server_stop_threads(serv);
	plugin_unregister_init(PLUGIN_NAME);
	plugin_unregister_shutdown(PLUGIN_NAME);
    plugin_unregister_notification(PLUGIN_NAME);
	return 0;
}

static int mon_notify(const notification_t *n, user_data_t UNUSED *user_data)
{
    server_t *serv = &default_server; /* shortcut */

    char text[1024] = ""; // TODO
	char timestamp_str[64] = { '\0' };

    if (serv->output_format == OUTPUT_FORMAT_TERSE ||
       serv->output_format == OUTPUT_FORMAT_FULL) {
        cdtime_t ts = (n->time != 0) ? n->time : cdtime();
    	struct tm timestamp_tm;
	    time_t tt = CDTIME_T_TO_TIME_T(ts);
    	localtime_r(&tt, &timestamp_tm);
	    strftime(timestamp_str, sizeof (timestamp_str),
                 "%Y-%m-%d %H:%M:%S",
		    	 &timestamp_tm);
    }

    if (serv->output_format == OUTPUT_FORMAT_MINIMAL) {
        ssnprintf(text, sizeof(text),
                  "NOTIF message=%s\n",
                  n->message);
    } else if (serv->output_format == OUTPUT_FORMAT_TERSE) {
        ssnprintf(text, sizeof(text),
                  "%s NOTIF host=%s plugin=%s message=%s\n",
                  timestamp_str,
                  n->host,
                  n->plugin,
                  n->message);
    } else { // OUTPUT_FORMAT_FULL
        ssnprintf(text, sizeof(text),
                  "%s NOTIF host=%s plugin=%s/%s type=%s/%s message=%s\n",
                  timestamp_str,
                  n->host,
                  n->plugin, n->plugin_instance,
                  n->type, n->type_instance,
                  n->message);
    }

    queue_push_tail(&serv->messages, text, strlen(text));

	return 0;
}


void module_register(void)
{
	plugin_register_config(PLUGIN_NAME, mon_config,
			               config_keys, config_keys_num);
	plugin_register_init(PLUGIN_NAME, mon_init);
	plugin_register_shutdown(PLUGIN_NAME, mon_shutdown);
    plugin_register_notification(PLUGIN_NAME, mon_notify, NULL);
    return;
}

/* vim: set sw=4 ts=4 sts=4 tw=78 : */
