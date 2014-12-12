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
#include "test_int.h"


static void
test_null(void)
{
    SampleRequest sr;
    int err = 0;

    memset(&sr, 0, sizeof(sr));
    err = sampler_parse_request(&sr, NULL, 0);

    g_assert_cmpint(err, <, 0);
}

static void
test_empty_string(void)
{
    SampleRequest sr;
    int err = 0;

    memset(&sr, 0, sizeof(sr));
    err = sampler_parse_request(&sr, "", 0);

    g_assert_cmpint(err, <, 0);
}

static void
test_zero_size_string(void)
{
    SampleRequest sr;
    int err = 0;

    memset(&sr, 0, sizeof(sr));
    err = sampler_parse_request(&sr, "{}", 0);

    g_assert_cmpint(err, <, 0);
}


static void
test_helper_malformed_req(const char *req)
{
    SampleRequest sr;
    int err = 0;

    memset(&sr, 0, sizeof(sr));
    err = sampler_parse_request(&sr, req, strlen(req));

    g_assert_cmpint(err, <, 0);
}

static void
test_bad_req_id_type(void)
{
    test_helper_malformed_req("{ \"req-id\": 1 }");
}

static void
test_bad_req_id_too_long(void)
{
    test_helper_malformed_req(
        "{ \"req-id\":"
        " \"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
           "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
           "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\" }");
}

static void
test_bad_stats_type(void)
{
    test_helper_malformed_req("{ \"stats\": 1 }");
}

static void
test_bad_stats_string_not_array(void)
{
    test_helper_malformed_req("{ \"stats\": \"foobar\" }");
}

static void
test_bad_stats_array_type(void)
{
    test_helper_malformed_req("{ \"stats\": [1] }");
}

static void
test_bad_stats_array_invalid_string(void)
{
    test_helper_malformed_req("{ \"stats\": [\"foobar\"] }");
}


static void
test_bad_stats_array_type_last(void)
{
    test_helper_malformed_req("{ \"stats\": [\"vcpu\", 1] }");
}

static void
test_bad_stats_array_type_middle(void)
{
    test_helper_malformed_req("{ \"stats\": [\"vcpu\", 1, \"block\"] }");
}




static void
test_helper_correct_req(SampleRequest *req,
                        const char *text)
{
    int err = 0;

    memset(req, 0, sizeof(*req));
    err = sampler_parse_request(req, text, strlen(text));

    g_assert_cmpint(err, ==, 0);
}

static void
test_good_empty_data(void)
{
    SampleRequest sr;
    uuid_t null_uuid;
    uuid_clear(null_uuid);

    test_helper_correct_req(&sr, "{}");

    g_assert_cmpint(sr.stats, ==, 0);
    g_assert_cmpint(uuid_compare(null_uuid, sr.uuid), ==, 0);
}


#define REQ_ID "9ec2b64f-e432-4020-98df-8dac9931f5f7"

static void
test_good_block_only(void)
{
    uuid_t req_id;
    SampleRequest sr;

    uuid_parse(REQ_ID, req_id);

    test_helper_correct_req(&sr,
        "{ \"req-id\": \""REQ_ID"\","
        " \"stats\": [ \"block\" ] }");

    g_assert_cmpint(sr.stats, ==, VIR_DOMAIN_STATS_BLOCK);
    g_assert_cmpint(uuid_compare(req_id, sr.uuid), ==, 0);
}

#undef REQ_ID


int
main(int argc, char *argv[])
{
    VmonContext ctx;
    memset(&ctx, 0, sizeof(ctx));

    vmon_init();
    vmon_setup_log(&ctx);

    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/vmon/sample_request/null", test_null);
    g_test_add_func("/vmon/sample_request/empty_string", test_empty_string);
    g_test_add_func("/vmon/sample_request/zero_size_string", test_zero_size_string);

    g_test_add_func("/vmon/sample_request/bad_req_id_type", test_bad_req_id_type);
    g_test_add_func("/vmon/sample_request/bad_req_id_too_long", test_bad_req_id_too_long);
    g_test_add_func("/vmon/sample_request/bad_stats_type", test_bad_stats_type);
    g_test_add_func("/vmon/sample_request/bad_stats_string_not_array", test_bad_stats_string_not_array);
    g_test_add_func("/vmon/sample_request/bad_stats_array_type", test_bad_stats_array_type);
    g_test_add_func("/vmon/sample_request/bad_stats_array_invalid_string", test_bad_stats_array_invalid_string);
    g_test_add_func("/vmon/sample_request/bad_stats_array_type_last", test_bad_stats_array_type_last);
    g_test_add_func("/vmon/sample_request/bad_stats_array_type_middle", test_bad_stats_array_type_middle);

    g_test_add_func("/vmon/sample_request/good_empty_data", test_good_empty_data);
    g_test_add_func("/vmon/sample_request/good_block_only", test_good_block_only);

    return g_test_run();
}

