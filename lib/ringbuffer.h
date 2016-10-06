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

#ifndef RINGBUFFER_H
#define RINGBUFFER_H

typedef struct RingBuffer RingBuffer;

int
ringbuffer_init(RingBuffer **rb, int size, size_t elem_size);
 
void
ringbuffer_free(RingBuffer *rb);
 
void
ringbuffer_clear(RingBuffer *rb);

int
ringbuffer_full(RingBuffer *rb);
 
int
ringbuffer_empty(RingBuffer *rb);
 
int
ringbuffer_put(RingBuffer *rb, void *elem);
 
int
ringbuffer_get(RingBuffer *rb, void *elem);


#endif /* RINGBUFFER_H */

