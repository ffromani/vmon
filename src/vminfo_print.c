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


static int
pcpu_print_json(const PCpuInfo *pcpu, FILE *out)
{
    fprintf(out, "\"pcpu\": "
                 "{"
                 " \"cpu.time\": %llu,"
                 " \"cpu.user\": %llu,"
                 " \"cpu.system\": %llu "
                 "}",
                 pcpu->time,
                 pcpu->user,
                 pcpu->system);
    return 0;
}

static int
balloon_print_json(const BalloonInfo *balloon, FILE *out)
{
    fprintf(out, "\"balloon\": "
                 "{"
                 " \"balloon.current\": %llu,"
                 " \"balloon.maximum\": %llu "
                 "}",
                 balloon->current,
                 balloon->maximum);
    return 0;
}


static int
vcpu_print_json(const VCpuInfo *vcpu, FILE *out)
{
    size_t i;
    const VCpuStats *stats = (vcpu->xstats) ?vcpu->xstats :vcpu->stats;

    fprintf(out, "\"vcpu\":"
                 " {");

    for (i = 0; i < vcpu->nstats; i++) {
        if (!stats[i].present) {
            continue;
        }

        fprintf(out, " \"%zu\": { \"state\": %i,"
                     " \"time\": %llu,"
                     " }%s",
                i,
                stats[i].state,
                stats[i].time,
                (i == vcpu->nstats-1) ?"" :",");
    }

    fputs(" }", out);

    return 0;
}


static int
block_print_json(const BlockInfo *block, FILE *out)
{
    size_t i;
    const BlockStats *stats = (block->xstats) ?block->xstats :block->stats;

    fprintf(out, "\"block\":"
                 " {");

    for (i = 0; i < block->nstats; i++) {
        const char *name = (stats[i].xname) ?stats[i].xname :stats[i].name;
        fprintf(out, " \"%s\": { \"rd_bytes\": %llu,"
                     " \"rd_operations\": %llu,"
                     " \"rd_total_times\": %llu,"
                     " \"wr_bytes\": %llu,"
                     " \"wr_operations\": %llu,"
                     " \"wr_total_times\": %llu,"
                     " \"allocation\": %llu,"
                     " \"capacity\": %llu,"
                     " \"physical\": %llu "
                     " }%s",
                name,
                stats[i].rd_bytes,
                stats[i].rd_reqs,
                stats[i].rd_times,
                stats[i].wr_bytes,
                stats[i].wr_reqs,
                stats[i].wr_times,
                stats[i].allocation,
                stats[i].capacity,
                stats[i].physical,
                (i == block->nstats-1) ?"" :",");
    }

    fputs(" }", out);

    return 0;
}


static int
iface_print_json(const IfaceInfo *iface, FILE *out)
{
    size_t i;
    const IfaceStats *stats = (iface->xstats) ?iface->xstats :iface->stats;

    fprintf(out, "\"iface\":"
                 " {");

    for (i = 0; i < iface->nstats; i++) {
        const char *name = (stats[i].xname) ?stats[i].xname :stats[i].name;
        fprintf(out, " \"%s\": { \"rx_bytes\": %llu,"
                     " \"rx_pkts\": %llu,"
                     " \"rx_errs\": %llu,"
                     " \"rx_drop\": %llu,"
                     " \"tx_bytes\": %llu,"
                     " \"tx_pkts\": %llu,"
                     " \"tx_errs\": %llu,"
                     " \"tx_drop\": %llu,"
                     " }%s",
                name,
                stats[i].rx_bytes,
                stats[i].rx_pkts,
                stats[i].rx_errs,
                stats[i].rx_drop,
                stats[i].tx_bytes,
                stats[i].tx_pkts,
                stats[i].tx_errs,
                stats[i].tx_drop,
                (i == iface->nstats-1) ?"" :",");
    }

    fputs(" }", out);

    return 0;
}


int
vminfo_print_json(VmInfo *vm, FILE *out)
{
    fprintf(out,
            "{"
            " \"vm-id\": \"%s\",",
            vm->uuid);

    /* intentionally ignore state, yet */
    pcpu_print_json(&vm->pcpu, out);
    fputs(", ", out);

    balloon_print_json(&vm->balloon, out);
    fputs(", ", out);

    vcpu_print_json(&vm->vcpu, out);
    fputs(", ", out);

    block_print_json(&vm->block, out);
    fputs(", ", out);

    iface_print_json(&vm->iface, out);
    fputs(" }", out);
    return 0;
}

