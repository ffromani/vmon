/**
 * vmon - collectd/virt2.c
 * Copyright (C) 2016 Francesco Romani <fromani at redhat.com>
 * Based on
 * collectd - src/virt.c
 * Copyright (C) 2006-2008  Red Hat Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *   Francesco Romani <fromani at redhat.com>
 *   Richard W.M. Jones <rjones at redhat.com>
 **/

#include "collectd.h"

#include <libvirt/libvirt.h>


#define PLUGIN_NAME "virt2"

/*
 * Synopsis:
 * <Plugin "virt2">
 *   Connection "qemu:///system"
 *   RefreshInterval 60
 *   Instances 5
 *   DomainCheck true
 *   DomainAffinity true
 * </Plugin>
 */

static const char *config_keys[] = {
    "Connection",
    "RefreshInterval"
    "Instances",
    "DomainCheck",
    "DomainAffinity",
    NULL
};
#define NR_CONFIG_KEYS ((sizeof config_keys / sizeof config_keys[0]) - 1)

typedef struct virt2_config_s virt2_config_t;
struct virt2_config_s {
    char *connection_uri;
    size_t instances;
    cdtime_t interval; /* could be 0, and it's OK */
    int domain_check;
    int domain_affinity;
};

typedef struct virt2_state_s virt2_state_t;

typedef struct virt2_instance_s virt2_instance_t;
struct virt2_instance_s {
    virt2_state_t *state;
    const virt2_config_t *config;
    size_t id;
};

typedef struct virt2_user_data_s virt2_user_data_t;
struct virt2_user_data_s {
    user_data_t ud;
    virt2_instance_t inst;
};

typedef struct virt2_doms_s virt2_doms_t;
struct virt2_doms_s {
    virDomainPtr *doms;
    size_t num;
};


static virt2_doms_t *virt2_doms_alloc(size_t num);
static virt2_doms_t *virt2_doms_clone(virt2_doms_t *doms);
static void virt2_doms_free(virt2_doms_t *doms);


struct virt2_state_s {
    virConnectPtr conn;

    virt2_doms_t *doms;
    void **partitions; // placeholder
    pthread_mutex_t lock;

    int stats;
    int flags;
};

typedef struct virt2_context_s virt2_context_t;
struct virt2_context_s {
    virt2_user_data_t *user_data;
    virt2_state_t state;
    virt2_config_t conf;
};

/* *** */

static virt2_context_t default_context = {
    .user_data = NULL,
    .state = {
        .conn = NULL,
        .partitions = NULL,
        .flags = 0,
    },
    .conf = {
        .connection_uri = "qemu:///system",
        .instances = 5,
        .domain_check = 1,
        .domain_affinity = 1,
    },
};

static int virt2_refresh(virt2_instance_t *inst);
static int virt2_read_samples(virt2_instance_t *inst);
static int virt2_dispatch_samples(virt2_instance_t *inst, virDomainStatsRecordPtr *records, int records_num);

/* *** */

static int
virt2_shutdown (void)
{
    virt2_context_t *ctx = &default_context;

    if (ctx->state.conn != NULL)
        virConnectClose (ctx->state.conn);
    ctx->state.conn = NULL;

    if (ctx->state.doms != NULL)
        virt2_doms_free(ctx->state.doms);
    ctx->state.doms = NULL;

    sfree(ctx->user_data);
    return 0;
}

static int
virt2_config (const char *key, const char *value)
{
    virt2_config_t *cfg = &default_context.conf;

    if (strcasecmp (key, "Connection") == 0)
    {
        char *tmp = strdup (value);
        if (tmp == NULL) {
            ERROR (PLUGIN_NAME " plugin: Connection strdup failed.");
            return 1;
        }
        sfree (cfg->connection_uri);
        cfg->connection_uri = tmp;
        return 0;
    }
    if (strcasecmp (key, "Instances") == 0)
    {
        char *eptr = NULL;
        long val = strtol (value, &eptr, 10);
        if (eptr == NULL || *eptr != '\0')
            return 1;
        if (val <= 0)
            return 1;
        cfg->instances = val;
        return 0;
    }
    if (strcasecmp (key, "Interval") == 0)
    {
        char *eptr = NULL;
        double val = strtod (value, &eptr);
        if (eptr == NULL || *eptr != '\0')
            return 1;
        if (val <= 0)
            return 1;
        cfg->interval = DOUBLE_TO_CDTIME_T(val);
        return 0;
    }
    if (strcasecmp (key, "DomainCheck") == 0)
    {
        cfg->domain_check = IS_TRUE (value);
        return 0;
    }
    if (strcasecmp (key, "DomainAffinity") == 0)
    {
        cfg->domain_affinity = IS_TRUE (value);
        return 0;
    }

    /* Unrecognised option. */
    return -1;
}

static int
virt2_read (user_data_t *ud)
{
    virt2_instance_t *inst = ud->data;
    if (inst->id == 0) {
        return virt2_refresh (inst);
    }

    return virt2_read_samples (inst);
}

static int
virt2_init (void)
{
    virt2_context_t *ctx = &default_context;
    size_t instances_num = 1 + ctx->conf.instances;

    ctx->user_data = calloc(instances_num, sizeof(virt2_user_data_t));
    if (ctx->user_data == NULL)
    {
        ERROR (PLUGIN_NAME " plugin: cannot allocate %zu instances", instances_num);
        return -1;
    }

    ctx->state.conn = virConnectOpenReadOnly (ctx->conf.connection_uri);
    if (ctx->state.conn == NULL) {
        ERROR (PLUGIN_NAME " plugin: Unable to connect: "
               "virConnectOpenReadOnly (%s) failed.",
               ctx->conf.connection_uri);
        return -1;
    }

    for (size_t i = 0; i < instances_num; i++)
    {
        char name[DATA_MAX_NAME_LEN];  // TODO
        ssnprintf (name, sizeof(name), "virt-%zu", i);

        virt2_user_data_t *user_data = &(ctx->user_data[i]);

        virt2_instance_t *inst = &user_data->inst;
        inst->state = &ctx->state;
        inst->config = &ctx->conf;
        inst->id = i;
        
        user_data_t *ud = &user_data->ud;
        ud->data = inst;
        ud->free_func = NULL; // TODO

        // TODO: what if this fails?
        plugin_register_complex_read (NULL, name, virt2_read, ctx->conf.interval, ud);
    }

    return 0;
}

void
module_register (void)
{
    plugin_register_config (PLUGIN_NAME,
                            virt2_config,
                            config_keys,
                            NR_CONFIG_KEYS);
    plugin_register_init (PLUGIN_NAME, virt2_init);
    plugin_register_shutdown (PLUGIN_NAME, virt2_shutdown);
}

/* *** */

static int
virt2_refresh (virt2_instance_t *inst)
{
    return 0;
}

static int
virt2_read_samples (virt2_instance_t *inst)
{
    virt2_doms_t *doms = NULL;

    pthread_mutex_lock (&inst->state->lock);
    doms = virt2_doms_clone (&inst->state->doms[inst->id]);
    pthread_mutex_unlock (&inst->state->lock);
    /*
     * virDomainListGetStats below can block. So we choose to copy our
     * list of domains to make this call independent from the others
     * and to minimize the disruption.
     * instance#0 can change this asynchronously, so we cannot just
     * use it through a reference, we need a full copy.
     */
 
    virDomainStatsRecordPtr *records = NULL;
    int records_num = 0;
    int	ret = virDomainListGetStats (doms->doms, inst->state->stats, &records, inst->state->flags);
    if (ret == -1) {
        // TODO
    } else {
        ret = virt2_dispatch_samples (inst, records, records_num);
    }

    virDomainStatsRecordListFree (records);
    virt2_doms_free (doms);
    return ret;
}

static int
virt2_dispatch_samples (virt2_instance_t *inst, virDomainStatsRecordPtr *records, int records_num)
{
    return 0;
}

/* *** */

static virt2_doms_t *
virt2_doms_alloc (size_t num)
{
    virt2_doms_t *doms = calloc (1, sizeof(virt2_doms_t) + (num * sizeof(virDomainPtr)));
    if (doms)
    {
        doms->doms = (virDomainPtr *) (doms + 1);
        doms->num = num;
    }
    return doms;
}

static virt2_doms_t *
virt2_doms_clone(virt2_doms_t *doms)
{
    virt2_doms_t *newdoms = virt2_doms_alloc (doms->num);
    if (newdoms != NULL)
        memcpy (newdoms->doms, doms->doms, sizeof(virDomainPtr) * doms->num);
    return newdoms;
}

static void
virt2_doms_free(virt2_doms_t *doms)
{
    doms->num = 0;
    sfree (doms->doms);
    sfree (doms);
}

