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

#ifndef VMONLIB_H
#define VMONLIB_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>

#include <uuid.h>
#include <glib.h>
#include <libvirt/libvirt.h>


enum {
    TIMEOUT = 1 * 1000, /* milliseconds */
    MAX_THREADS = 5,
    TASKS_PER_THREAD = 200,
};

enum {
    UUID_LEN = 16,
    UUID_STRING_LEN = 36 + 1
};


#define UNUSED(IDENT) ((void)(IDENT))

#endif /* VMONLIB_H */

