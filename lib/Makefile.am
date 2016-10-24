# Copyright 2014 Red Hat, Inc. and/or its affiliates.
#
# Licensed to you under the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.  See the files README and
# LICENSE_GPL_v2 which accompany this distribution.

noinst_LIBRARIES = libvmon.a
libvmon_a_SOURCES = \
	executor.c \
	ringbuffer.c \
	scheduler.c \
	threading.c \
	vminfo.c \
	vminfo_parse.c \
	vminfo_print.c \
	$(NULL)


libvmondir = .

libvmon_a_CFLAGS = \
	-DVMON_PRIVATE=static \
	-I$(top_srcdir)/contrib/jsmn \
	$(GLIB2_CFLAGS) \
	$(GTHREAD2_CFLAGS) \
	$(LIBVIRT_CFLAGS) \
	$(UUID_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

libvmon_a_LDFLAGS = \
	-shared \
	-avoid-version \
	$(GLIB2_LIBS) \
	$(GTHREAD2_LIBS) \
	$(LIBVIRT_LIBS) \
	$(UUID_LIBS) \
	$(AM_LDFLAGS) \
	$(NULL)

noinst_HEADERS = \
	executor.h \
	ringbuffer.h \
	scheduler.h \
	threading.h \
	vminfo.h \
	vmonlib.h \
	$(NULL)
