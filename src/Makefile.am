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
	-I$(top_srcdir)/lib \
	$(GLIB2_CFLAGS) \
	$(GTHREAD2_CFLAGS) \
	$(LIBVIRTQEMU_CFLAGS) \
	$(UUID_CFLAGS) \
	$(VMON_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

vmon_LDFLAGS = \
	-L$(top_srcdir)/contrib/jsmn -ljsmn \
	-L$(top_srcdir)/lib -lvmon \
	$(LIBJSMN_LIBS) \
	$(GLIB2_LIBS) \
	$(GTHREAD2_LIBS) \
	$(LIBVIRTQEMU_LIBS) \
	$(UUID_LIBS) \
	$(AM_LDFLAGS) \
	$(NULL)

vmon_SOURCES = \
	sampler.c \
	vmon.c \
	vmon_int.c \
	$(NULL)

noinst_HEADERS = \
	sampler.h \
	vmon.h \
	vmon_int.h \
	$(NULL)

