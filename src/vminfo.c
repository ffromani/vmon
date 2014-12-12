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


enum {
    INDEX_BUF_SIZE = 128,
    PARAM_BUF_SIZE = 2048
};


#define DISPATCH(NAME, FIELD) do { \
    if (!strcmp(name, # NAME)) { \
        stats->FIELD = item->value.ul; \
        return; \
    } \
} while (0)

#define SETUP(STATS, NAME, ITEM) do { \
    if (!strcmp(NAME, "name")) { \
        size_t len = strlen(ITEM->value.s); \
        if (len > (STATS_NAME_LEN - 1)) { \
            STATS->xname = strdup(ITEM->value.s); \
        } else { \
            strncpy(STATS->name, ITEM->value.s, STATS_NAME_LEN); \
        } \
        return; \
    } \
} while (0)

static void
blockinfo_parse_field(BlockStats *stats, const char *name,
                      const virTypedParameterPtr item)
{
    SETUP(stats, name, item);

    DISPATCH(rd.reqs, rd_reqs);
    DISPATCH(rd.bytes, rd_bytes);
    DISPATCH(rd.times, rd_times);

    DISPATCH(wr.reqs, wr_reqs);
    DISPATCH(wr.bytes, wr_bytes);
    DISPATCH(wr.times, wr_times);

    DISPATCH(fl.bytes, fl_bytes);
    DISPATCH(fl.times, fl_times);

    DISPATCH(allocation, allocation);
    DISPATCH(capacity, capacity);
    DISPATCH(physical, physical);
}

#undef SETUP

#undef DISPATCH

static void
vcpuinfo_parse_field(VCpuStats *stats, const char *name,
                     const virTypedParameterPtr item)
{
    stats->present = 1;
    if (!strcmp(name, "state")) {
        stats->state = item->value.i;
    } else if (!strcmp(name, "time")) {
        stats->time = item->value.ul;
    }
}

static int
vminfo_setup(VmInfo *vm,  const virDomainStatsRecordPtr record)
{
    BlockInfo *block = &vm->block;
    IfaceInfo *iface = &vm->iface;
    VCpuInfo *vcpu = &vm->vcpu;
    int found_block = 0;
    int found_vcpu = 0;
    int i;

    for (i = 0; i < record->nparams; i++) {
        if (!strcmp(record->params[i].field, "block.count")) {
            block->nstats = record->params[i].value.ul;
            found_block = 1;
        }
        if (!strcmp(record->params[i].field, "net.count")) {
            iface->nstats = record->params[i].value.ul;
        }
        if (!strcmp(record->params[i].field, "vcpu.current")) {
            vcpu->current = record->params[i].value.ul;
        } else if (!strcmp(record->params[i].field, "vcpu.maximum")) {
            vcpu->nstats = record->params[i].value.ul;
            found_vcpu = 1;
        }
    }

    if (found_block && block->nstats > BLOCK_STATS_NUM) {
        block->xstats = calloc(block->nstats, sizeof(BlockStats));
        if (block->xstats == NULL) {
            goto cleanup;
        }
    }

    if (found_vcpu && vcpu->nstats > VCPU_STATS_NUM) {
        vcpu->xstats = calloc(vcpu->nstats, sizeof(VCpuInfo));
        if (vcpu->xstats == NULL) {
            goto cleanup;
        }
    }

    return 0;

cleanup:
    free(block->xstats);
    free(vcpu->xstats);
    return -1;
}

static int
pcpuinfo_parse(PCpuInfo *pcpu,
               const virDomainStatsRecordPtr record,
               size_t i)
{
    if (strcmp(record->params[i].field, "cpu.time") == 0) {
        pcpu->time = record->params[i].value.ul;
    } else if (strcmp(record->params[i].field, "cpu.user") == 0) {
        pcpu->user = record->params[i].value.ul;
    } else if (strcmp(record->params[i].field, "cpu.system") == 0) {
        pcpu->system = record->params[i].value.ul;
    }        
    return 0;
}


static int
ballooninfo_parse(BalloonInfo *balloon,
                  const virDomainStatsRecordPtr record,
                  size_t i)
{
    if (strcmp(record->params[i].field, "balloon.current") == 0) {
        balloon->current = record->params[i].value.ul;
    } else if (strcmp(record->params[i].field, "balloon.maximum") == 0) {
        balloon->maximum = record->params[i].value.ul;
    } 
    return 0;
}


enum {
    OFF_BUF_LEN = 128
};

static int
find_offset(char *pc, char **ret, size_t *off, size_t limit)
{
    char buf[OFF_BUF_LEN] = { '\0' };
    size_t j = 0;

    for (j = 0;  j < sizeof(buf)-1 && pc && isdigit(*pc); j++) {
        buf[j] = *pc++;
    }
    pc++; // skip '.' separator

    *ret = pc;
    *off = atol(buf);

    return (pc != NULL && *off < limit);
}

static int
vcpuinfo_parse(VCpuInfo *vcpu,
               const virDomainStatsRecordPtr record,
               size_t i)
{
    VCpuStats *stats = (vcpu->xstats) ?vcpu->xstats :vcpu->stats;
    if (strncmp(record->params[i].field, "vcpu.", 5) == 0
     && strcmp(record->params[i].field, "vcpu.current") != 0
     && strcmp(record->params[i].field, "vcpu.maximum") != 0) {
        size_t off = 0;
        char *pc = NULL;

        if (find_offset(record->params[i].field + 5,
                        &pc, &off, vcpu->nstats)) {
            vcpuinfo_parse_field(stats+off, pc, record->params + i);
        }
    }

    return 0;
}

static int
blockinfo_parse(BlockInfo *block,
                const virDomainStatsRecordPtr record,
                size_t i)
{
    BlockStats *stats = (block->xstats) ?block->xstats :block->stats;
    if (strncmp(record->params[i].field, "block.", 6) == 0
     && strcmp(record->params[i].field, "block.count") != 0) {
        size_t off = 0;
        char *pc = NULL;

        if (find_offset(record->params[i].field + 6,
                        &pc, &off, block->nstats)) {
            blockinfo_parse_field(stats+off, pc, record->params + i);
        }
    }

    return 0;
}

static int
pcpu_print_json(const PCpuInfo *pcpu, FILE *out, int last)
{
    fprintf(out, "\"pcpu\": "
                 "{"
                 " \"cpu.time\": %llu,"
                 " \"cpu.user\": %llu,"
                 " \"cpu.system\": %llu "
                 "}"
                 "%s",
                 pcpu->time,
                 pcpu->user,
                 pcpu->system,
                 (last) ?"" :", ");
    return 0;
}

static int
balloon_print_json(const BalloonInfo *balloon, FILE *out, int last)
{
    fprintf(out, "\"balloon\": "
                 "{"
                 " \"balloon.current\": %llu,"
                 " \"balloon.maximum\": %llu "
                 "}"
                 "%s",
                 balloon->current,
                 balloon->maximum,
                 (last) ?"" :", ");
    return 0;
}


static int
vcpu_print_json(const VCpuInfo *vcpu, FILE *out, int last)
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

    fprintf(out, " }"
                 "%s", (last) ?"" :", ");

    return 0;
}


static int
block_print_json(const BlockInfo *block, FILE *out, int last)
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

    fprintf(out, " }"
                 "%s", (last) ?"" :", ");

    return 0;
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

void
vminfo_init(VmInfo *vm)
{
    memset(vm, 0, sizeof(*vm));
}

#define TRY_TO_PARSE(subset, vm, record, i) do { \
    if (subset ## info_parse(&vm->subset, record, i) < 0) { \
        /* TODO: logging? */ \
        return -1; \
    } \
} while (0)

int
vminfo_parse(VmInfo *vm,
             const virDomainStatsRecordPtr record)
{
    int i = 0;

    if (vminfo_setup(vm, record)) {
        return -1;
    }
    if (virDomainGetUUIDString(record->dom, vm->uuid) < 0) {
        return -1;
    }

    for (i = 0; i < record->nparams; i++) {
        /* intentionally ignore state, yet */
        TRY_TO_PARSE(pcpu, vm, record, i);
        TRY_TO_PARSE(balloon, vm, record, i);
        TRY_TO_PARSE(vcpu, vm, record, i);
        TRY_TO_PARSE(block, vm, record, i);
        /* TODO: iface */
    }

    return 0;
}

#undef TRY_TO_PARSE

int
vminfo_print_json(VmInfo *vm, FILE *out)
{
    fprintf(out,
            "{"
            " \"vm-id\": \"%s\",",
            vm->uuid);

    /* intentionally ignore state, yet */
    pcpu_print_json(&vm->pcpu, out, 0);
    balloon_print_json(&vm->balloon, out, 0);
    vcpu_print_json(&vm->vcpu, out, 1);
    block_print_json(&vm->block, out, 1);
    /* TODO: iface */

    fputs(" }", out);
    return 0;
}


void
vminfo_free(VmInfo *vm)
{
    blockinfo_free(&vm->block);
}

