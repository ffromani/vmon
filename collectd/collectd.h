/**
 * collectd.h - collectd API header
 * amalgamation of (parts of) collectd.h, plugin.h, common.h
 * NOT SUPPORTED NOR ENDORSED by collectd project.
 *
 * the original code is
 * Copyright (C) 2005-2014  Florian octo Forster
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
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
 *   Florian octo Forster <octo at collectd.org>
 *   Sebastian Harl <sh at tokkee.org>
 *   Niki W. Waibel <niki.waibel@gmx.net>
 *   Lubos Stanek <lubek at users.sourceforge.net>
 **/

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <errno.h>
#include <pthread.h>
#include <strings.h>


#ifndef COLLECTD_PLUGIN_H
#define COLLECTD_PLUGIN_H

char *sstrncpy (char *dest, const char *src, size_t n);
int ssnprintf (char *dest, size_t n, const char *format, ...);
char *ssnprintf_alloc (char const *format, ...);
char *sstrdup(const char *s);
void *smalloc(size_t size);
char *sstrerror (int errnum, char *buf, size_t buflen);
int strsplit (char *string, char **fields, size_t size);
int strjoin (char *dst, size_t dst_len, char **fields, size_t fields_num, const char *sep);

#define sfree(ptr) \
	do { \
		free((void *)(ptr)); \
		(ptr) = NULL; \
	} while (0)

#define STATIC_ARRAY_SIZE(a) (sizeof (a) / sizeof (*(a)))

#define IS_TRUE(s) ((strcasecmp ("true", (s)) == 0) \
		|| (strcasecmp ("yes", (s)) == 0) \
		|| (strcasecmp ("on", (s)) == 0))
#define IS_FALSE(s) ((strcasecmp ("false", (s)) == 0) \
		|| (strcasecmp ("no", (s)) == 0) \
		|| (strcasecmp ("off", (s)) == 0))

#define LOG_ERR 3
#define LOG_WARNING 4
#define LOG_NOTICE 5
#define LOG_INFO 6
#define LOG_DEBUG 7

#define DATA_MAX_NAME_LEN 128
#define NOTIF_MAX_MSG_LEN 256

#define DS_TYPE_COUNTER  0
#define DS_TYPE_GAUGE    1
#define DS_TYPE_DERIVE   2
#define DS_TYPE_ABSOLUTE 3

#define DS_TYPE_TO_STRING(t) (t == DS_TYPE_COUNTER)     ? "counter"  : \
				(t == DS_TYPE_GAUGE)    ? "gauge"    : \
				(t == DS_TYPE_DERIVE)   ? "derive"   : \
				(t == DS_TYPE_ABSOLUTE) ? "absolute" : \
				"unknown"

#define DOUBLE_TO_CDTIME_T(d) ((cdtime_t) ((d) * 1073741824.0))
#define CDTIME_T_TO_TIME_T(t) ((time_t) (((t) + (1 << 29)) >> 30))

typedef uint64_t cdtime_t;

cdtime_t cdtime(void);

typedef unsigned long long counter_t;
typedef double gauge_t;
typedef int64_t derive_t;
typedef uint64_t absolute_t;

union value_u
{
	counter_t  counter;
	gauge_t    gauge;
	derive_t   derive;
	absolute_t absolute;
};
typedef union value_u value_t;

struct meta_data_s;
typedef struct meta_data_s meta_data_t;

struct value_list_s
{
	value_t *values;
	size_t   values_len;
	cdtime_t time;
	cdtime_t interval;
	char     host[DATA_MAX_NAME_LEN];
	char     plugin[DATA_MAX_NAME_LEN];
	char     plugin_instance[DATA_MAX_NAME_LEN];
	char     type[DATA_MAX_NAME_LEN];
	char     type_instance[DATA_MAX_NAME_LEN];
	meta_data_t *meta;
};
typedef struct value_list_s value_list_t;

#define VALUE_LIST_INIT { .values = NULL, .meta = NULL }


struct ignorelist_s;
typedef struct ignorelist_s ignorelist_t;

ignorelist_t *ignorelist_create (int invert);
void ignorelist_free (ignorelist_t *il);
void ignorelist_set_invert (ignorelist_t *il, int invert);
int ignorelist_add (ignorelist_t *il, const char *entry);
int ignorelist_match (ignorelist_t *il, const char *entry);

enum notification_meta_type_e
{
	NM_TYPE_STRING,
	NM_TYPE_SIGNED_INT,
	NM_TYPE_UNSIGNED_INT,
	NM_TYPE_DOUBLE,
	NM_TYPE_BOOLEAN
};

typedef struct notification_meta_s
{
	char name[DATA_MAX_NAME_LEN];
	enum notification_meta_type_e type;
	union
	{
		const char *nm_string;
		int64_t nm_signed_int;
		uint64_t nm_unsigned_int;
		double nm_double;
		_Bool nm_boolean;
	} nm_value;
	struct notification_meta_s *next;
} notification_meta_t;

typedef struct notification_s
{
	int    severity;
	cdtime_t time;
	char   message[NOTIF_MAX_MSG_LEN];
	char   host[DATA_MAX_NAME_LEN];
	char   plugin[DATA_MAX_NAME_LEN];
	char   plugin_instance[DATA_MAX_NAME_LEN];
	char   type[DATA_MAX_NAME_LEN];
	char   type_instance[DATA_MAX_NAME_LEN];
	notification_meta_t *meta;
} notification_t;

struct user_data_s
{
	void *data;
	void (*free_func) (void *);
};
typedef struct user_data_s user_data_t;


int plugin_register_config (const char *name,
		int (*callback) (const char *key, const char *val),
		const char **keys, int keys_num);
int plugin_register_init (const char *name,
        int (*callback) (void));
int plugin_register_read (const char *name,
		int (*callback) (void));
int plugin_register_complex_read (const char *group, const char *name,
        int (*callback) (user_data_t *),
		cdtime_t interval,
		user_data_t const *user_data);
int plugin_register_shutdown (const char *name,
        int (*callback) (void));
int plugin_register_notification (const char *name,
        int (*callback) (const notification_t *, user_data_t *),
        user_data_t const *user_data);

int plugin_unregister_config (const char *name);
int plugin_unregister_init (const char *name);
int plugin_unregister_read (const char *name);
int plugin_unregister_shutdown (const char *name);
int plugin_unregister_notification (const char *name);

int plugin_dispatch_values (value_list_t const *vl);

extern char     hostname_g[];
extern cdtime_t interval_g;
extern int      timeout_g;

int plugin_thread_create(pthread_t *thread, const pthread_attr_t *attr,
                		 void *(*start_routine) (void *), void *arg);

typedef struct
{
	cdtime_t last;
	cdtime_t interval;
	_Bool complained_once;
} c_complain_t;

#define C_COMPLAIN_INIT_STATIC { 0, 0, 0 }
#define C_COMPLAIN_INIT(c) do { \
	(c)->last = 0; \
	(c)->interval = 0; \
	(c)->complained_once = 0; \
} while (0)

void c_complain (int level, c_complain_t *c, const char *format, ...);
void c_complain_once (int level, c_complain_t *c, const char *format, ...);
#define c_would_release(c) ((c)->interval != 0)
void c_do_release (int level, c_complain_t *c, const char *format, ...);
#define c_release(level, c, ...) \
	do { \
		if (c_would_release (c)) \
			c_do_release(level, c, __VA_ARGS__); \
	} while (0)

void plugin_log (int level, const char *format, ...);
#define ERROR(...)   plugin_log (LOG_ERR,     __VA_ARGS__)
#define WARNING(...) plugin_log (LOG_WARNING, __VA_ARGS__)
#define NOTICE(...)  plugin_log (LOG_NOTICE,  __VA_ARGS__)
#define INFO(...)    plugin_log (LOG_INFO,    __VA_ARGS__)
#define DEBUG(...)   plugin_log (LOG_DEBUG,   __VA_ARGS__)

void module_register (void);

#endif /* COLLECTD_PLUGIN_H */
