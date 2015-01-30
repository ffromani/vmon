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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vminfo.h"


static void
vcpuinfo_free(VCpuInfo *vcpu)
{
    free(vcpu->xstats);
}


static void
blockinfo_free(BlockInfo *block)
{
    size_t i;
    const BlockStats *stats = (block->xstats) ?block->xstats :block->stats;

    for (i = 0; i < block->nstats; i++)
        free(stats[i].xname);

    free(block->xstats);
}

static void
ifaceinfo_free(IfaceInfo *iface)
{
    size_t i;
    const IfaceStats *stats = (iface->xstats) ?iface->xstats :iface->stats;

    for (i = 0; i < iface->nstats; i++)
        free(stats[i].xname);

    free(iface->xstats);
}

void
vminfo_init(VmInfo *vm)
{
    memset(vm, 0, sizeof(*vm));
}

void
vminfo_free(VmInfo *vm)
{
    vcpuinfo_free(&vm->vcpu);
    blockinfo_free(&vm->block);
    ifaceinfo_free(&vm->iface);
}

