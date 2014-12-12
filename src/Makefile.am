# Copyright 2014 Red Hat, Inc. and/or its affiliates.
#
# Licensed to you under the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.  See the files README and
# LICENSE_GPL_v2 which accompany this distribution.

libexec_PROGRAMS = vmon
vmondir = .


vmon_CFLAGS = \
	-DVMON_PRIVATE=static \
	-I$(top_srcdir)/contrib/jsmn \
	$(GLIB2_CFLAGS) \
	$(GTHREAD2_CFLAGS) \
	$(LIBVIRT_CFLAGS) \
	$(UUID_CFLAGS) \
	$(VMON_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

vmon_LDFLAGS = \
	-L$(top_srcdir)/contrib/jsmn -ljsmn \
	$(LIBJSMN_LIBS) \
	$(GLIB2_LIBS) \
	$(GTHREAD2_LIBS) \
	$(LIBVIRT_LIBS) \
	$(UUID_LIBS) \
	$(AM_LDFLAGS) \
	$(NULL)

vmon_SOURCES = \
	executor.c \
	ringbuffer.c \
	sampler.c \
	scheduler.c \
	vminfo.c \
	vmon.c \
	vmon_int.c \
	$(NULL)

noinst_HEADERS = \
	executor.h \
	ringbuffer.h \
	sampler.h \
	scheduler.h \
	vminfo.h \
	vmon.h \
	vmon_int.h \
	$(NULL)


