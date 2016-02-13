#include <platform.h>

#include <kernel/config.h>
#include <kernel/region.h>
#include <kernel/unit.h>
#include <kernel/version.h>
#include <util/attrib.h>
#include <util/gamedata.h>
#include <util/message.h>
#include <binarystore.h>
#include <filestream.h>
#include <memstream.h>
#include <storage.h>
#include <stream.h>
#include <tests.h>

#include "curse.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <CuTest.h>


static void test_curse(CuTest * tc)
{
    attrib *attrs = NULL;
    curse *c, *result;
    int cid;

    curse_type ct_dummy = { "dummy", CURSETYP_NORM, 0, M_SUMEFFECT, NULL };
    c = create_curse(NULL, &attrs, &ct_dummy, 1.0, 1, 1, 1);
    cid = c->no;
    result = findcurse(cid);
    CuAssertPtrEquals(tc, c, result);
    a_remove(&attrs, attrs);
    result = findcurse(cid);
    CuAssertPtrEquals(tc, NULL, result);
    test_cleanup();
}

typedef struct {
    curse *c;
    region *r;
    unit *u;
} curse_fixture;

static void setup_curse(curse_fixture *fix, const char *name) {
    test_cleanup();
    fix->r = test_create_region(0, 0, NULL);
    fix->u = test_create_unit(test_create_faction(NULL), fix->r);
    fix->c = create_curse(fix->u, &fix->r->attribs, ct_find(name), 1.0, 1, 1.0, 0);
}

static void test_magicstreet(CuTest *tc) {
    curse_fixture fix;
    message *msg;
    setup_curse(&fix, "magicstreet");
    fix.c->duration = 2;
    msg = fix.c->type->curseinfo(fix.r, TYP_REGION, fix.c, 0);
    CuAssertStrEquals(tc, "curseinfo::magicstreet", test_get_messagetype(msg));
    msg_release(msg);
    test_cleanup();
}

static void test_magicstreet_warning(CuTest *tc) {
    curse_fixture fix;
    message *msg;
    setup_curse(&fix, "magicstreet");
    fix.c->duration = 1;
    msg = fix.c->type->curseinfo(fix.r, TYP_REGION, fix.c, 0);
    CuAssertStrEquals(tc, "curseinfo::magicstreetwarn", test_get_messagetype(msg));
    msg_release(msg);
    test_cleanup();
}

static void test_good_dreams(CuTest *tc) {
    curse_fixture fix;
    message *msg;
    setup_curse(&fix, "gbdream");
    fix.c->effect = 1;
    msg = fix.c->type->curseinfo(fix.r, TYP_REGION, fix.c, 0);
    CuAssertStrEquals(tc, "curseinfo::gooddream", test_get_messagetype(msg));
    msg_release(msg);
    test_cleanup();
}

static void test_bad_dreams(CuTest *tc) {
    curse_fixture fix;
    message *msg;
    setup_curse(&fix, "gbdream");
    fix.c->effect = -1;
    msg = fix.c->type->curseinfo(fix.r, TYP_REGION, fix.c, 0);
    CuAssertStrEquals(tc, "curseinfo::baddream", test_get_messagetype(msg));
    msg_release(msg);
    test_cleanup();
}

static void test_memstream(CuTest *tc) {
    storage store;
    stream out = { 0 };
    char buf[1024];
    int val=0;

    mstream_init(&out);
    binstore_init(&store, &out);
    store.handle.data = &out;

    WRITE_INT(&store, 999999);
    WRITE_TOK(&store, "fortytwo");
    WRITE_INT(&store, 42);

    out.api->rewind(out.handle);
    READ_INT(&store, &val);
    READ_TOK(&store, buf, 1024);
    CuAssertIntEquals(tc, 999999, val);
    CuAssertStrEquals(tc, "fortytwo", buf);
    READ_INT(&store, &val);
    CuAssertIntEquals(tc, 42, val);
    mstream_done(&out);
}

static void test_write_flag(CuTest *tc) {
    curse_fixture fix;
    gamedata data;
    storage store;

    mstream_init(&data.strm);
    gamedata_init(&data, &store, RELEASE_VERSION);

    setup_curse(&fix, "gbdream");
    fix.c->flags = 42 | CURSE_ISNEW;
    curse_write(fix.r->attribs, fix.r, &store);
    data.strm.api->rewind(data.strm.handle);
    curse_read(fix.r->attribs, fix.r, &data);
    CuAssertIntEquals(tc, 42 | CURSE_ISNEW, ((curse *) fix.r->attribs->data.v)->flags);
    global.data_version = RELEASE_VERSION; // FIXME: should not need this!
    CuAssertIntEquals(tc, RELEASE_VERSION, global.data_version);

    mstream_done(&data.strm);
    gamedata_done(&data);
    test_cleanup();
}

CuSuite *get_curse_suite(void)
{
    CuSuite *suite = CuSuiteNew();
    SUITE_ADD_TEST(suite, test_curse);
    SUITE_ADD_TEST(suite, test_magicstreet);
    SUITE_ADD_TEST(suite, test_magicstreet_warning);
    SUITE_ADD_TEST(suite, test_good_dreams);
    SUITE_ADD_TEST(suite, test_bad_dreams);
    SUITE_ADD_TEST(suite, test_memstream);
    SUITE_ADD_TEST(suite, test_write_flag);
    return suite;
}
