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

#include <unistd.h>

#include "test_int.h"
#include "threading.h"

#include "executor.h"
#include "scheduler.h"

#include "vmon_int.h"


typedef struct TestTask TestTask;
struct TestTask {
    gint wait;
    gint error;
    Event *executed;
};

static void
testtask_init(TestTask *tt, gint wait, gint error, Event *event)
{
    tt->wait = wait;
    tt->error = error;
    tt->executed = event;
}

#ifdef DEBUG_DUMP
static void
tt_dump(void *ud, const void *item)
{
    const TaskData *td = item;
    const TestTask *tt = (td->ud.xdata) ?td->ud.xdata :td->data;
    fprintf(ud, "TaskData@%p = {\n", (void *)td);
    fprintf(ud, "  work=%p\n", td->td.work);
    fprintf(ud, "  collect=%p\n", td->td.work);
    fprintf(ud, "  timeout=%i\n", td->td.timeout);
    fprintf(ud, "  size=%lu\n", td->ud.size);
    fprintf(ud, "  data=%p : ", (void *)tt);
    fprintf(ud, "  TestTask@%p = {\n", (void *)tt);
    fprintf(ud, "    wait=%i\n", tt->wait);
    fprintf(ud, "    error=%i\n", tt->error);
    if (tt->executed) {
        fprintf(ud, "    executed=%i\n", event_is_set(tt->executed));
    } else {
        fprintf(ud, "    executed=0\n");
    }
    fprintf(ud, "  }\n");
    fprintf(ud, "}\n");
}
#endif

static gint
TestTaskFunction(gpointer data)
{
    TestTask *tt = data;
    if (tt->wait) {
        g_message("TestTaskFunction will sleep for %i ms", tt->wait);
        usleep(tt->wait * 1000);
        g_message("TestTaskFunction awake");
    }
    event_set(tt->executed);
    g_message("TestTaskFunction executed!");
    return tt->error;
}


typedef struct TestData TestData;
struct TestData {
    Scheduler *sched;
    Executor *exec;
    int err;
};


#define RETURN_IF_FAILED(err) do { \
    if ((err)) { \
        return err; \
    } \
} while (0)

static int
setup_td(TestData *td)
{
    td->err = scheduler_init(&td->sched, TRUE);
    RETURN_IF_FAILED(td->err);

    td->err = scheduler_start(td->sched);
    RETURN_IF_FAILED(td->err);

    td->err = executor_init(&td->exec, td->sched, 10, 20);
    RETURN_IF_FAILED(td->err);

#ifdef DEBUG_DUMP
    executor_set_dump(td->exec, tt_dump, stderr);
#endif

    td->err = executor_start(td->exec);
    RETURN_IF_FAILED(td->err);

    return td->err;
}

static void
setup(TestData *td)
{
    setup_td(td);
    g_assert_cmpint(td->err, ==, 0);
}


static int
teardown_td(TestData *td)
{
    td->err = scheduler_stop(td->sched, TRUE)
          || executor_stop(td->exec, TRUE);
    return td->err;
}

static void
teardown(TestData *td)
{
    teardown_td(td);
    g_assert_cmpint(td->err, ==, 0);
}


static gint
NullFunction(gpointer data)
{
    UNUSED(data);
    return 0;
}


static gint
NullCollect(gpointer data, gint error, gboolean timeout)
{
    UNUSED(data);
    UNUSED(error);
    UNUSED(timeout);
    return 0;
}


void
test_dispatch_not_running(void)
{
    TestData td;
    setup(&td);

    td.err = executor_stop(td.exec, FALSE);
    g_assert_cmpint(td.err, ==, 0);

    td.err = executor_dispatch(td.exec, NullFunction, NullCollect, NULL, 0, 0);
    g_assert_cmpint(td.err, ==, EXECUTOR_ERROR_NOT_RUNNING);

    teardown(&td);
}

void
test_start_twice(void)
{
    TestData td;
    setup(&td);

    td.err = executor_start(td.exec);
    g_assert_cmpint(td.err, ==, EXECUTOR_ERROR_ALREADY_STARTED);

    teardown(&td);
}

void
test_dispatch(void)
{
    TestTask tt;
    TestData td;
    Event executed;
    gboolean run = FALSE;

    event_init(&executed);
    testtask_init(&tt, 0, 0, &executed);

    setup(&td);

    td.err = executor_dispatch(td.exec, TestTaskFunction, NullCollect, &tt, sizeof(tt), 0);
    g_assert_cmpint(td.err, ==, 0);

    run = event_wait(&executed, 100);
    g_assert(run);

    teardown(&td);
}

void
test_dispatch_with_timeout(void)
{
    TestTask tt;
    TestData td;
    Event executed;
    gboolean run = FALSE;

    event_init(&executed);
    testtask_init(&tt, 200, 0, &executed);

    setup(&td);

    td.err = executor_dispatch(td.exec, TestTaskFunction, NullCollect, &tt, sizeof(tt), 100);
    g_assert_cmpint(td.err, ==, 0);

    /* FIXME: there is a race lurking nearby */

    run = event_wait(&executed, 300);
    g_assert(run);

    teardown(&td);
}


int
main(int argc, char *argv[])
{
    VmonContext ctx;
    memset(&ctx, 0, sizeof(ctx));

    vmon_init();
    vmon_setup_log(&ctx);

    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/vmon/executor/dispatch_not_running", test_dispatch_not_running);
    g_test_add_func("/vmon/executor/start_twice", test_start_twice);
    g_test_add_func("/vmon/executor/dispatch", test_dispatch);
    g_test_add_func("/vmon/executor/dispatch_with_timeout", test_dispatch_with_timeout);
    return g_test_run();
}

