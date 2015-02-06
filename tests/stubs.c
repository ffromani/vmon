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

#include "vmon_int.h"

#ifdef STUB_SAMPLER

int
sampler_handle_request(VmonContext *ctx, const char *text, size_t size)
{
    UNUSED(ctx);
    UNUSED(text);
    UNUSED(size);
    return 0;
}


int
sampler_send_request(VmonContext *ctx, VmonRequest *req)
{
    UNUSED(ctx);
    UNUSED(req);
    return 0;
}

#endif /* STUB_SAMPLER */

#ifdef STUB_EXECUTOR

#include "executor.h"


int
executor_dispatch(Executor *exc,
                  TaskFunction work,
                  TaskCollect collect,
                  void *data,
                  size_t size,
                  int timeout)
{
    UNUSED(exc);
    UNUSED(work);
    UNUSED(collect);
    UNUSED(data);
    UNUSED(size);
    UNUSED(timeout);
    return 0;
}

#endif /* STUB_EXECUTOR */

#ifdef STUB_VMINFO

#include "vminfo.h"


void
vminfo_init(VmInfo *vm)
{
    UNUSED(vm);
    return;
}

int
vminfo_parse(VmInfo *vm,
             const virDomainStatsRecordPtr record)
{
    UNUSED(vm);
    UNUSED(record);
    return 0;
}

int
vminfo_print_json(VmInfo *vm, FILE *out)
{
    UNUSED(vm);
    UNUSED(out);
    return 0;
}

int
vminfo_send_events(VmInfo *vm, const VmChecks *checks, FILE *out)
{
    UNUSED(vm);
    UNUSED(checks);
    UNUSED(out);
    return 0;
}

void
vminfo_free(VmInfo *vm)
{
    UNUSED(vm);
    return;
}

#endif /* STUB_VMINFO */

