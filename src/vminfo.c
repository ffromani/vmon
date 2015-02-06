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

static void
vm_send_event_block(const char *type, const BlockStats *stats, FILE *out)
{
    fprintf(out,
            "{\"event\": \"%s\","
            " \"class\": \"block\","
            " \"device\": \"%s\","
            " \"allocation\": %llu,"
            " \"capacity\": %llu,"
            " \"physical\": %llu}",
            type,
            (stats->xname) ?stats->xname :stats->name,
            stats->allocation,
            stats->capacity,
            stats->physical);
}

enum {
    MEGAB = 1024 * 1024, /* 2 ** 20 = 1024 ** 2 = 1 MiB */
    CHUNK = 1024 * 2
    /*
     * yep, pessimistic guess. If VDSM configuration changes
     * (it ever has?) we're badly screwed
     */
};

static int
vm_check_triggered_usage_threshold(const BlockStats *stats,
                                   int disk_usage_perc)
{
    int triggered = 0;
    unsigned long long disk_usage_threshold = \
        (disk_usage_perc * CHUNK * MEGAB) / 100;
    if ((stats->physical - stats->allocation) < disk_usage_threshold) {
        triggered = 1;
    }
    return triggered;
}

int
vminfo_send_events(VmInfo *vm, const VmChecks *checks, FILE *out)
{
    int err = 0;
    size_t i;
    BlockInfo *block = &(vm->block);
    const BlockStats *stats = (block->xstats) ?block->xstats :block->stats;

    for (i = 0; i < block->nstats; i++) {
        if (vm_check_triggered_usage_threshold(stats,
                                               checks->disk_usage_perc)) {
            vm_send_event_block("usage-threshold", stats, out);
        }
    }
    return err;
}

