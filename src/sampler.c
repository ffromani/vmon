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

#include <stdio.h>
#include <string.h>

#include <libvirt/libvirt.h>

#include "contrib/jsmn/jsmn.h"

#include "sampler.h"
#include "vminfo.h"
#include "vmon_int.h"


/* FIXME */
static int
parse_stats_string(SampleRequest *sr, const char *filter, size_t len)
{
    int err = 0;
    unsigned int stat = 0;

    if (!strncmp(filter, "state", len)) {
        stat = VIR_DOMAIN_STATS_STATE;
    } else if (!strncmp(filter, "cpu-total", len)) {
        stat = VIR_DOMAIN_STATS_CPU_TOTAL;
    } else if (!strncmp(filter, "balloon", len)) {
        stat = VIR_DOMAIN_STATS_BALLOON;
    } else if (!strncmp(filter, "vcpu", len)) {
        stat = VIR_DOMAIN_STATS_VCPU;
    } else if (!strncmp(filter, "interface", len)) {
        stat = VIR_DOMAIN_STATS_INTERFACE;
    } else if (!strncmp(filter, "block", len)) {
        stat = VIR_DOMAIN_STATS_BLOCK;
    } else {
        char req_uuid[VIR_UUID_STRING_BUFLEN] = { '\0' };

        uuid_unparse(sr->uuid, req_uuid);
        g_message("req-id=\"%s\" ignored unknown stat: %.*s",
                  req_uuid, (int)len, filter);
        err = -1;
    }

    sr->stats |= stat;
    return err;
}

static gboolean
is_token(const char *json, const jsmntok_t *tok, const char *s)
{
    if (tok->type == JSMN_STRING &&
        strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
        return TRUE;
    }
    return FALSE;
}

static gboolean
has_next(int i, int r)
{
    return (i + 1) < r;
}

enum {
    JSON_REQUEST_MAX_TOKENS = 32
};

VMON_PRIVATE int
sampler_parse_request(SampleRequest *sr, const char *text, size_t size)
{
    int i;
    int r;
    jsmn_parser parser;
    jsmntok_t tokens[JSON_REQUEST_MAX_TOKENS];

    memset(sr, 0, sizeof(*sr));
    uuid_clear(sr->uuid);

    jsmn_init(&parser);
    r = jsmn_parse(&parser, text, size, tokens, sizeof(tokens)/sizeof(tokens[0]));

    if (r < 0) {
        /* warning */
        g_message("failed to parse JSON request: %i", r);
        return -1;
    }

    /* Assume the top-level element is an object */
    if (r < 1 || tokens[0].type != JSMN_OBJECT) {
        /* warning */
        g_message("JSON request top level object is not object");
        return -1;
    }

    for (i = 1; i < r; i++) {
        if (is_token(text, &tokens[i], "req-id") && has_next(i, r)) {
            char uuidbuf[UUID_STRING_LEN] = { '\0' };
            size_t len = tokens[i+1].end - tokens[i+1].start;
            if (tokens[i+1].type != JSMN_STRING) {
                /* warning */
                g_message("JSON request malformed: req-id is not a string");
                return -1;
            }
            if (len > VIR_UUID_STRING_BUFLEN) {
                /* warning */
                g_message("JSON request malformed: req-id too long");
                return -1;
            }
            strncpy(uuidbuf, text + tokens[i+1].start,
                    MIN(len, UUID_STRING_LEN)); /* FIXME */
            uuidbuf[UUID_STRING_LEN-1] = '\0';
            uuid_parse(uuidbuf, sr->uuid);
            i += 1;
        } else if (is_token(text, &tokens[i], "get-stats") && has_next(i, r)) {
            int j;

            if (tokens[i+1].type != JSMN_ARRAY) {
                /* warning */
                g_message("JSON request malformed: stats is not an array");
                return -1;
            }

            for (j = 0; j < tokens[i+1].size; j++) {
                jsmntok_t *stat = &tokens[i+1+j+1];

                if (stat->type != JSMN_STRING) {
                    /* warning */
                    g_message("JSON request malformed:"
                              " stat item is not a string");
                    return -1;
                }

                if (parse_stats_string(sr,
                                       text + stat->start,
                                       stat->end - stat->start) < 0) {
                    return -1;
                }
            }
            i += tokens[i+1].size + 1;
        } else {
            g_message("unexpected key: %.*s",
                      tokens[i].end - tokens[i].start,
                      text + tokens[i].start);
        }
    }
    return 0;
}

static int
write_response(FILE *out, const char *response, ssize_t length)
{
    ssize_t ret = 0;

    ret = write(fileno(out), response, length);
    if (ret != length) {
        g_warning("write_response failure: expected=%zu wrote=%zu",
                  length, ret);
    }
    return 0; /* always succesfull */
}

static gint
collect_error(VmonRequest *req, gint error, gboolean timeout)
{
    char dom_uuid[VIR_UUID_STRING_BUFLEN] = { '\0' };
    char req_uuid[VIR_UUID_STRING_BUFLEN] = { '\0' };
    char buffer[4096]; /* TODO */

    uuid_unparse(req->sr.uuid, req_uuid);
    if (req->dom) {
        virDomainGetUUIDString(req->dom, dom_uuid);
    }
    snprintf(buffer, sizeof(buffer),
            "{"
            " \"req-id\": \"%s\","
            " \"timestamp\": %zu,"
            " \"data\": {"
             " \"vm-id\": \"%s\","
             " \"error\": {"
               " \"code\": %i,"
               " \"message\": \"%s\""
              " },"
             " \"timeout\": \"%s\""
             " } "
            "}\n",
            req_uuid,
            time(NULL),
            dom_uuid,
            error,
            "",
            (timeout) ?"yes" :"no");

    write_response(req->ctx->out, buffer, strlen(buffer));
    return 0;
}


static gint
sample_domain_work(gpointer data)
{
    int ret = 0;
    VmonRequest *req = data;
    ret = virConnectGetAllDomainStats(req->ctx->conn, req->sr.stats, &req->records, 0); /* FIXME */
    req->records_num = ret;
    return 0;
}

static gint
bulk_sampling_work(gpointer data)
{
    int ret = 0;
    VmonRequest *req = data;
    virDomainPtr doms[] = { req->dom, NULL };
    ret = virDomainListGetStats(doms, req->sr.stats, &req->records, 0); /* FIXME */
    req->records_num = ret;
    return 0;
}

typedef struct VmonResponse VmonResponse;
struct VmonResponse {
    FILE *out;
    char *ptr;
    size_t len;
    time_t ts;
};

static void
response_init(VmonResponse *res)
{
    memset(res, 0, sizeof(*res));
    res->ts = time(NULL);
}

static void
response_begin(VmonResponse *res, uuid_t req_id)
{
    char req_uuid[VIR_UUID_STRING_BUFLEN] = { '\0' };

    res->out = open_memstream(&res->ptr, &res->len);

    uuid_unparse(req_id, req_uuid);

    fprintf(res->out,
            "{"
            " \"req-id\": \"%s\","
            " \"timestamp\": %zu,"
            " \"data\": ",
            req_uuid,
            res->ts);
}

static void
response_finish(VmonResponse *res, FILE *out)
{
    fputs(" }\n", res->out);

    fclose(res->out);

    write_response(out, res->ptr, res->len);

    free(res->ptr);
}


static gint
collect_success(VmonRequest *req)
{
    int j = 0;
    VmonResponse res;
    response_init(&res);

    if (req->ctx->conf.bulk_response) {
        response_begin(&res, req->sr.uuid);
    }

    for (j = 0; j < req->records_num; j++) {
        VmInfo vm;
        vminfo_init(&vm);

        if (!req->ctx->conf.bulk_response) {
            response_begin(&res, req->sr.uuid);
        }

        vminfo_parse(&vm, req->records[j]); /* FIXME */

        vminfo_print_json(&vm, res.out);

        vminfo_free(&vm);
        
        if (!req->ctx->conf.bulk_response) {
            response_finish(&res, req->ctx->out);
        }
    }

    virDomainStatsRecordListFree(req->records);

    if (req->ctx->conf.bulk_response) {
        response_finish(&res, req->ctx->out);
    }
    return 0;
}

static gint
sampling_collect(gpointer data, gint error, gboolean timeout)
{
    VmonRequest *req = data;
    gboolean ret;

    if (error || timeout) {
        ret = collect_error(req, error, timeout);
    } else {
        ret = collect_success(req);
    }
    if (req->dom) {
        virDomainFree(req->dom);
    }
    return ret;
}

static gint
list_domains_work(gpointer data)
{
    VmonRequest *req = data;
    virDomainPtr *domains;
    size_t i;
    int ret = -1;
    int err = 0;

    ret = virConnectListAllDomains(req->ctx->conn,
                                   &domains,
                                   req->ctx->flags);
    if (ret < 0) {
        collect_error(req, ret, FALSE);
        return ret;
    }

    for (i = 0; i < (size_t)ret; i++) {
        VmonRequest vreq;
        memcpy(&vreq, req, sizeof(vreq));
        vreq.dom = domains[i];

        err = executor_dispatch(vreq.ctx->executor,
                                sample_domain_work,
                                sampling_collect,
                                &vreq,
                                sizeof(vreq),
                                vreq.ctx->conf.timeout);
        if (err) {
            collect_error(&vreq, err, FALSE);
        }
    }

    free(domains);
    return (err) ?err :0;
}

int
sampler_handle_request(VmonContext *ctx, const char *text, size_t size)
{
    int err = 0 ;
    VmonRequest req;
    memset(&req, 0, sizeof(req));
    req.ctx = ctx;

    err = sampler_parse_request(&req.sr, text, size);
    if (!err) {
        err = sampler_send_request(ctx, &req);
    } else {
        /* warning */
        g_message("error parsing request: %s", text);
    }
    return err;
}

int
sampler_send_request(VmonContext *ctx, VmonRequest *req)
{
    TaskFunction task;

    if (ctx->conf.bulk_sampling) {
        task = bulk_sampling_work;
    } else {
        task = list_domains_work;
    }

    return executor_dispatch(ctx->executor,
                             task,
                             sampling_collect,
                             req,
                             sizeof(*req),
                             ctx->conf.timeout);
}

