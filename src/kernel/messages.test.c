#include <platform.h>
#include "messages.h"

#include <util/object.h>

#include <CuTest.h>
#include <tests.h>

static void test_create_message(CuTest *tc) {
    message *msg;
    object *obj;
    msg = msg_create_args("custom", "unit number", arg_unit(NULL), arg_int(42));
    CuAssertPtrNotNull(tc, msg);
    CuAssertPtrNotNull(tc, (obj = msg_get_arg(msg, "unit")));
    CuAssertPtrEquals(tc, NULL, obj->data.v);
    CuAssertPtrNotNull(tc, (obj = msg_get_arg(msg, "number")));
    CuAssertIntEquals(tc, 42, obj->data.i);
}

static void test_missing_message(CuTest *tc) {
    message *msg;
    msg = msg_message("unknown", "unit", NULL);
    CuAssertPtrNotNull(tc, msg);
    CuAssertPtrNotNull(tc, msg->type);
    CuAssertStrEquals(tc, msg->type->name, "missing_message");
    msg_release(msg);
}

static void test_message(CuTest *tc) {
    message *msg;
//    const char * args[] = { }
    message_type *mtype = mt_new("custom", NULL);
    mt_register(mtype);
    CuAssertPtrEquals(tc, mtype, (void *)mt_find("custom"));
    CuAssertIntEquals(tc, 0, mtype->nparameters);
    CuAssertPtrEquals(tc, NULL, (void *)mtype->pnames);
    CuAssertPtrEquals(tc, NULL, (void *)mtype->types);
    msg = msg_message("custom", "");
    CuAssertPtrNotNull(tc, msg);
    CuAssertIntEquals(tc, 1, msg->refcount);
    CuAssertPtrEquals(tc, mtype, (void *)msg->type);

    CuAssertPtrEquals(tc, msg, msg_addref(msg));
    CuAssertIntEquals(tc, 2, msg->refcount);
    msg_release(msg);
    CuAssertIntEquals(tc, 1, msg->refcount);
    msg_release(msg);
    test_cleanup();
}

CuSuite *get_messages_suite(void) {
    CuSuite *suite = CuSuiteNew();
    SUITE_ADD_TEST(suite, test_create_message);
    SUITE_ADD_TEST(suite, test_missing_message);
    SUITE_ADD_TEST(suite, test_message);
    return suite;
}