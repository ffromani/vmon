/*
 * vmon - Virtual Machine MONitor speedup helper for VDSM
 * Copyright (C) 2014 Red Hat, Inc.
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

#include <glib.h>

#include "ringbuffer.h"


void
test_init_empty(void)
{
    RingBuffer *rb = NULL;
    int err = 0;

    err = ringbuffer_init(&rb, 2, sizeof(int));
    g_assert_cmpint(err, ==, 0);

    g_assert(ringbuffer_empty(rb));

    ringbuffer_free(rb);
}

void
test_put_get(void)
{
    RingBuffer *rb = NULL;
    int err = 0;
    int in = 42, out;

    err = ringbuffer_init(&rb, 2, sizeof(int));
    g_assert_cmpint(err, ==, 0);

    g_assert(ringbuffer_empty(rb));

    err = ringbuffer_put(rb, &in);   
    g_assert_cmpint(err, ==, 0);

    g_assert(!ringbuffer_empty(rb));

    err = ringbuffer_get(rb, &out);   
    g_assert_cmpint(err, ==, 0);

    g_assert_cmpint(in, ==, out);

    ringbuffer_free(rb);
}

void
test_put_full(void)
{
    RingBuffer *rb = NULL;
    int err = 0;
    int i, size = 2;

    err = ringbuffer_init(&rb, size, sizeof(int));
    g_assert_cmpint(err, ==, 0);

    g_assert(ringbuffer_empty(rb));

    for (i = 0; i < size; i++) {
        err = ringbuffer_put(rb, &i);   
        g_assert_cmpint(err, ==, 0);
        g_assert(!ringbuffer_empty(rb));
    }
    g_assert(ringbuffer_full(rb));

    ringbuffer_free(rb);
}

void
test_put_full_overwrite(void)
{
    RingBuffer *rb = NULL;
    int err = 0;
    int i, size = 2;

    err = ringbuffer_init(&rb, size, sizeof(int));
    g_assert_cmpint(err, ==, 0);

    g_assert(ringbuffer_empty(rb));

    for (i = 0; i < size; i++) {
        err = ringbuffer_put(rb, &i);
        g_assert_cmpint(err, ==, 0);
    }

    i = 42;
    err = ringbuffer_put(rb, &i);
    g_assert_cmpint(err, <, 0);

    for (i = 0; i < size; i++) {
        int out;
        err = ringbuffer_get(rb, &out);   
        g_assert_cmpint(err, ==, 0);
        g_assert_cmpint(i, ==, out);
    }

    ringbuffer_free(rb);
}



int
main(int argc, char *argv[])
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/vmon/ringbuffer/init_empty", test_init_empty);
    g_test_add_func("/vmon/ringbuffer/put_get", test_put_get);
    g_test_add_func("/vmon/ringbuffer/put_full", test_put_full);
    g_test_add_func("/vmon/ringbuffer/put_full_overwrite", test_put_full_overwrite);
    return g_test_run();
}

