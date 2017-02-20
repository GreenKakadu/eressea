#include <platform.h>

#include "names.h"

#include <kernel/race.h>
#include <kernel/unit.h>
#include <util/language.h>
#include <util/functions.h>

#include <CuTest.h>
#include "tests.h"

static void test_names(CuTest * tc)
{
    race_name_func foo;
    unit *u;
    race *rc;
    test_cleanup();
    register_names();
    default_locale = test_create_locale();
    u = test_create_unit(test_create_faction(0), test_create_region(0, 0, 0));
    rc = test_create_race("undead");
    locale_setstring(default_locale, "undead_name_0", "Graue");
    locale_setstring(default_locale, "undead_postfix_0", "Kobolde");
    CuAssertPtrNotNull(tc, foo = (race_name_func)get_function("nameundead"));
    rc->generate_name = foo;
    rc->generate_name(u);
    CuAssertStrEquals(tc, "Graue Kobolde", u->_name);
    CuAssertPtrNotNull(tc, get_function("nameskeleton"));
    CuAssertPtrNotNull(tc, get_function("namezombie"));
    CuAssertPtrNotNull(tc, get_function("nameghoul"));
    CuAssertPtrNotNull(tc, get_function("namedragon"));
    CuAssertPtrNotNull(tc, get_function("namedracoid"));
    CuAssertPtrNotNull(tc, get_function("namegeneric"));
    test_cleanup();
}

static void test_monster_names(CuTest *tc) {
    unit *u;
    race *rc;

    test_setup();
    register_names();
    default_locale = test_create_locale();
    locale_setstring(default_locale, "race::irongolem", "Eisengolem");
    locale_setstring(default_locale, "race::irongolem_p", "Eisengolems");
    rc = test_create_race("irongolem");
    u = test_create_unit(test_create_faction(rc), test_create_region(0, 0, 0));
    CuAssertPtrNotNull(tc, u->_name);
    rc->generate_name = (race_name_func)get_function("namegeneric");
    rc->generate_name(u);
    CuAssertPtrEquals(tc, 0, u->_name);
    CuAssertStrEquals(tc, "Eisengolem", unit_getname(u));
    u->number = 2;
    CuAssertStrEquals(tc, "Eisengolems", unit_getname(u));
    test_cleanup();
}

CuSuite *get_names_suite(void)
{
    CuSuite *suite = CuSuiteNew();
    SUITE_ADD_TEST(suite, test_names);
    SUITE_ADD_TEST(suite, test_monster_names);
    return suite;
}
