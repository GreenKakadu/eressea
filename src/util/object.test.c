#include <platform.h>
#include "object.h"

#include <CuTest.h>

static void test_object(CuTest *tc) {
    object *obj = NULL;
    CuAssertPtrEquals(tc, NULL, obj);
}

CuSuite *get_object_suite(void)
{
    CuSuite *suite = CuSuiteNew();
    SUITE_ADD_TEST(suite, test_object);
    return suite;
}
