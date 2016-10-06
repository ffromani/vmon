# Copyright 2014 Red Hat, Inc. and/or its affiliates.
#
# Licensed to you under the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.  See the files README and
# LICENSE_GPL_v2 which accompany this distribution.

noinst_bin_PROGRAMS = \
	test_executor \
	test_ringbuffer \
	test_sampler_request \
	$(NULL)
noinst_bindir = .

COMMON_CFLAGS = \
	-DVMON_PRIVATE=extern \
	-I$(top_srcdir)/contrib/jsmn \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/tests \
	$(GLIB2_CFLAGS) \
	$(GTHREAD2_CFLAGS) \
	$(LIBVIRT_CFLAGS) \
	$(UUID_CFLAGS) \
	$(VMON_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

COMMON_LDFLAGS = \
	-L$(top_srcdir)/contrib/jsmn -ljsmn \
	-L$(top_srcdir)/lib -lvmon \
	$(LIBJSMN_LIBS) \
	$(GLIB2_LIBS) \
	$(GTHREAD2_LIBS) \
	$(LIBVIRT_LIBS) \
	$(UUID_LIBS) \
	$(AM_LDFLAGS) \
	$(NULL)

test_executor_CFLAGS = \
	$(COMMON_CFLAGS) \
	-DSTUB_SAMPLER=1 \
	$(NULL)
test_executor_LDFLAGS = \
	$(COMMON_LDFLAGS) \
	$(NULL)
test_executor_SOURCES = \
	$(top_srcdir)/src/vmon_int.c \
	threading.c \
	test_executor.c \
	stubs.c \
	$(NULL)

test_ringbuffer_CFLAGS = \
	$(COMMON_CFLAGS) \
	-DSTUB_SAMPLER=1 \
	$(NULL)
test_ringbuffer_LDFLAGS = \
	$(COMMON_LDFLAGS) \
	$(NULL)
test_ringbuffer_SOURCES = \
	$(top_srcdir)/src/vmon_int.c \
	test_ringbuffer.c \
	stubs.c \
	$(NULL)

test_sampler_request_CFLAGS = \
	-DSTUB_EXECUTOR=1 \
	-DSTUB_VMINFO=1 \
	$(COMMON_CFLAGS) \
	$(NULL)
test_sampler_request_LDFLAGS = \
	$(COMMON_LDFLAGS) \
	$(NULL)
test_sampler_request_SOURCES = \
	$(top_srcdir)/src/sampler.c \
	$(top_srcdir)/src/vmon_int.c \
	test_sampler_request.c \
	stubs.c \
	$(NULL)

noinst_HEADERS = \
	threading.h \
	test_int.h \
	$(NULL)


check-local:
	run-parts $(top_srcdir)/tests/
