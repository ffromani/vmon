/*
 * vmon - Virtual Machine MONitor speedup helper for VDSM
 * Copyright (C) 2014-2015 Red Hat, Inc.
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

#ifndef VMON_INT_H
#define VMON_INT_H

#include "vmon.h"


enum {
    UUID_LEN = 16,
    UUID_STRING_LEN = 36 + 1
};


#define UNUSED(IDENT) ((void)(IDENT))


void
vmon_init(void);

void
vmon_setup_log(VmonContext *ctx);

void
vmon_setup_io(VmonContext *ctx);

#endif /* VMON_INT_H */

