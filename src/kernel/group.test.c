#include <platform.h>
#include "types.h"
#include "ally.h"
#include "group.h"
#include "faction.h"
#include "unit.h"
#include "region.h"
#include "save.h"
#include "version.h"
#include <util/gamedata.h>
#include <stream.h>
#include <filestream.h>
#include <storage.h>
#include <binarystore.h>

#include <CuTest.h>
#include <tests.h>
#include <assert.h>

static void test_group_readwrite(CuTest * tc)
{
    faction * f;
    group *g;
    ally *al;
    gamedata *data;

    test_cleanup();
    test_create_world();
    data = gamedata_open("test.dat", "wb", RELEASE_VERSION);
    f = test_create_faction(0);
    g = new_group(f, "test", 42);
    al = ally_add(&g->allies, f);
    al->status = HELP_GIVE;
    write_groups(data->store, f);
    binstore_done(data->store);
    gamedata_close(data);

    f->groups = 0;
    free_group(g);
    data = gamedata_open("test.dat", "rb", RELEASE_VERSION);
    read_groups(data, f);
    gamedata_close(data);

    CuAssertPtrNotNull(tc, f->groups);
    CuAssertPtrNotNull(tc, f->groups->allies);
    CuAssertPtrEquals(tc, 0, f->groups->allies->next);
    CuAssertPtrEquals(tc, f, f->groups->allies->faction);
    CuAssertIntEquals(tc, HELP_GIVE, f->groups->allies->status);
    remove("test.dat");
    test_cleanup();
}

static void test_group(CuTest * tc)
{
    unit *u;
    region *r;
    faction *f;
    group *g;

    test_cleanup();
    test_create_world();
    r = findregion(0, 0);
    f = test_create_faction(0);
    assert(r && f);
    u = test_create_unit(f, r);
    assert(u);
    CuAssertTrue(tc, join_group(u, "hodor"));
    CuAssertPtrNotNull(tc, (g = get_group(u)));
    CuAssertStrEquals(tc, "hodor", g->name);
    CuAssertIntEquals(tc, 1, g->members);
    set_group(u, 0);
    CuAssertIntEquals(tc, 0, g->members);
    CuAssertPtrEquals(tc, 0, get_group(u));
    set_group(u, g);
    CuAssertIntEquals(tc, 1, g->members);
    CuAssertPtrEquals(tc, g, get_group(u));
    test_cleanup();
}

CuSuite *get_group_suite(void)
{
    CuSuite *suite = CuSuiteNew();
    SUITE_ADD_TEST(suite, test_group);
    SUITE_ADD_TEST(suite, test_group_readwrite);
    return suite;
}

