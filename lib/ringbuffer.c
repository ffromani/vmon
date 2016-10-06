/*
 * vmon - Virtual Machine MONitor for oVirt (et. al.)
 * Copyright (C) 2014-2016 Red Hat, Inc.
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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>


#define UNUSED(IDENT) ((void)(IDENT))


typedef void (*rb_dump)(void *ud, const void *item);

static void
no_dump(void *ud, const void *item)
{
    UNUSED(ud);
    UNUSED(item);
    return;
}

typedef struct RingBuffer RingBuffer;
struct RingBuffer {
    pthread_mutex_t lock;
    pthread_cond_t cond;
    int waiters;

    int size;
    int used;
    int start;
    int end;
    void *elems;
    size_t elem_size;

    rb_dump dump;
    void *dump_ud;
};

void
ringbuffer_clear(RingBuffer *rb)
{
    pthread_mutex_lock(&rb->lock);
    rb->start = 0;
    rb->end = 0;
    pthread_mutex_unlock(&rb->lock);
    return;
}

int
ringbuffer_init(RingBuffer **rb, int size, size_t elem_size)
{
    void *ptr =NULL;

    ptr = calloc(1, sizeof(RingBuffer) + (size * elem_size));
    if (ptr) {
        RingBuffer *buf = ptr;

        pthread_mutex_init(&buf->lock, 0);
        pthread_cond_init(&buf->cond, 0);
        buf->size = size;
        buf->elems = buf + 1;
        buf->elem_size = elem_size;
        ringbuffer_clear(buf);

        buf->dump = no_dump;
        buf->dump_ud = NULL;
        *rb = buf;
        return 0;
    }
    return -1;
}

void
ringbuffer_free(RingBuffer *rb)
{
    free(rb);
}

static int
rb_is_full(const RingBuffer *rb)
{
    return rb->used == rb->size;
}

int
ringbuffer_full(RingBuffer *rb)
{
    int ret;
    pthread_mutex_lock(&rb->lock);
    ret = rb_is_full(rb);
    pthread_mutex_unlock(&rb->lock);
    return ret;
}

static int
rb_is_empty(const RingBuffer *rb)
{
    return !(rb->used);
}

int
ringbuffer_empty(RingBuffer *rb)
{
    int ret;
    pthread_mutex_lock(&rb->lock);
    ret = rb_is_empty(rb);
    pthread_mutex_unlock(&rb->lock);
    return ret;
}

static void *
rb_item_at(RingBuffer *rb, int pos)
{
    int off = (pos * rb->elem_size);
    return ((uint8_t*)rb->elems) + off;
}

int
ringbuffer_put(RingBuffer *rb, void *elem)
{
    int ret = 0;
    pthread_mutex_lock(&rb->lock);

    if (!rb_is_full(rb)) {
        rb->used++;
        memcpy(rb_item_at(rb, rb->end), elem, rb->elem_size);
        rb->dump(rb->dump_ud, rb_item_at(rb, rb->end));
        rb->end = (rb->end + 1) % rb->size;

        if (rb->waiters > 0) {
            pthread_cond_signal(&rb->cond);
        }
    } else {
        ret = -1;
    }

    pthread_mutex_unlock(&rb->lock);
    return ret;
}
 
int
ringbuffer_get(RingBuffer *rb, void *elem)
{
    pthread_mutex_lock(&rb->lock);

    rb->waiters++;
    while (rb_is_empty(rb)) {
        pthread_cond_wait(&rb->cond, &rb->lock);
    }
    rb->waiters--;

    rb->used--;
    memcpy(elem, rb_item_at(rb, rb->start), rb->elem_size);
    rb->dump(rb->dump_ud, rb_item_at(rb, rb->start));
    rb->start = (rb->start + 1) % rb->size;

    pthread_mutex_unlock(&rb->lock);
    return 0;
}

void
ringbuffer_set_dump(RingBuffer *rb, rb_dump dump, void *ud)
{
    rb->dump = dump;
    rb->dump_ud = ud;
}

