#include <platform.h>
#include <kernel/config.h>
#include "economy.h"

#include <util/attrib.h>
#include <util/message.h>
#include <kernel/building.h>
#include <kernel/item.h>
#include <kernel/faction.h>
#include <kernel/messages.h>
#include <kernel/order.h>
#include <kernel/pool.h>
#include <kernel/race.h>
#include <kernel/region.h>
#include <kernel/ship.h>
#include <kernel/terrain.h>
#include <kernel/unit.h>

#include <util/language.h>

#include <CuTest.h>
#include <tests.h>
#include <assert.h>

static void test_give_control_building(CuTest * tc)
{
    unit *u1, *u2;
    building *b;
    struct faction *f;
    region *r;

    test_cleanup();
    f = test_create_faction(0);
    r = test_create_region(0, 0, 0);
    b = test_create_building(r, 0);
    u1 = test_create_unit(f, r);
    u_set_building(u1, b);
    u2 = test_create_unit(f, r);
    u_set_building(u2, b);
    CuAssertPtrEquals(tc, u1, building_owner(b));
    give_control(u1, u2);
    CuAssertPtrEquals(tc, u2, building_owner(b));
    test_cleanup();
}

static void test_give_control_ship(CuTest * tc)
{
    unit *u1, *u2;
    ship *sh;
    struct faction *f;
    region *r;

    test_cleanup();
    f = test_create_faction(0);
    r = test_create_region(0, 0, 0);
    sh = test_create_ship(r, 0);
    u1 = test_create_unit(f, r);
    u_set_ship(u1, sh);
    u2 = test_create_unit(f, r);
    u_set_ship(u2, sh);
    CuAssertPtrEquals(tc, u1, ship_owner(sh));
    give_control(u1, u2);
    CuAssertPtrEquals(tc, u2, ship_owner(sh));
    test_cleanup();
}

struct steal {
    struct unit *u;
    struct region *r;
    struct faction *f;
};

static void setup_steal(struct steal *env, struct terrain_type *ter, struct race *rc) {
    env->r = test_create_region(0, 0, ter);
    env->f = test_create_faction(rc);
    env->u = test_create_unit(env->f, env->r);
}

static void test_steal_okay(CuTest * tc) {
    struct steal env;
    race *rc;
    struct terrain_type *ter;

    test_cleanup();
    ter = test_create_terrain("plain", LAND_REGION);
    rc = test_create_race("human");
    rc->flags = 0;
    setup_steal(&env, ter, rc);
    CuAssertPtrEquals(tc, 0, check_steal(env.u, 0));
    test_cleanup();
}

static void test_steal_nosteal(CuTest * tc) {
    struct steal env;
    race *rc;
    terrain_type *ter;
    message *msg;

    test_cleanup();
    ter = test_create_terrain("plain", LAND_REGION);
    rc = test_create_race("human");
    rc->flags = RCF_NOSTEAL;
    setup_steal(&env, ter, rc);
    CuAssertPtrNotNull(tc, msg = check_steal(env.u, 0));
    msg_release(msg);
    test_cleanup();
}

static void test_steal_ocean(CuTest * tc) {
    struct steal env;
    race *rc;
    terrain_type *ter;
    message *msg;

    test_cleanup();
    ter = test_create_terrain("ocean", SEA_REGION);
    rc = test_create_race("human");
    setup_steal(&env, ter, rc);
    CuAssertPtrNotNull(tc, msg = check_steal(env.u, 0));
    msg_release(msg);
    test_cleanup();
}

static struct unit *create_recruiter(void) {
    region *r;
    faction *f;
    unit *u;
    const resource_type* rtype;

    test_cleanup();
    test_create_world();

    r=findregion(0, 0);
    rsetpeasants(r, 999);
    f = test_create_faction(rc_find("human"));
    u = test_create_unit(f, r);
    rtype = get_resourcetype(R_SILVER);
    change_resource(u, rtype, 1000);
    return u;
}

static void test_heroes_dont_recruit(CuTest * tc) {
    unit *u;

    test_cleanup();

    u = create_recruiter();
    fset(u, UFL_HERO);
    unit_addorder(u, create_order(K_RECRUIT, default_locale, "1"));

    economics(u->region);

    CuAssertIntEquals(tc, 1, u->number);
    CuAssertPtrNotNull(tc, test_find_messagetype(u->faction->msgs, "error_herorecruit"));

    test_cleanup();
}

static void test_normals_recruit(CuTest * tc) {
    unit *u;

    test_cleanup();

    u = create_recruiter();
    unit_addorder(u, create_order(K_RECRUIT, default_locale, "1"));

    economics(u->region);

    CuAssertIntEquals(tc, 2, u->number);

    test_cleanup();
}

typedef struct request {
    struct request *next;
    struct unit *unit;
    struct order *ord;
    int qty;
    int no;
    union {
        bool goblin;             /* stealing */
        const struct luxury_type *ltype;    /* trading */
    } type;
} request;

static void test_tax_cmd(CuTest *tc) {
    order *ord;
    faction *f;
    region *r;
    unit *u;
    item_type *sword, *silver;
    request *taxorders = 0;


    test_cleanup();
    config_set("taxing.perlevel", "20");
    test_create_world();
    f = test_create_faction(NULL);
    r = findregion(0, 0);
    assert(r && f);
    u = test_create_unit(f, r);

    ord = create_order(K_TAX, f->locale, "");
    assert(ord);

    tax_cmd(u, ord, &taxorders);
    CuAssertPtrNotNull(tc, test_find_messagetype(u->faction->msgs, "error48"));
    test_clear_messages(u->faction);

    silver = get_resourcetype(R_SILVER)->itype;

    sword = it_get_or_create(rt_get_or_create("sword"));
    new_weapontype(sword, 0, 0.0, NULL, 0, 0, 0, SK_MELEE, 1);
    i_change(&u->items, sword, 1);
    set_level(u, SK_MELEE, 1);

    tax_cmd(u, ord, &taxorders);
    CuAssertPtrNotNull(tc, test_find_messagetype(u->faction->msgs, "error_no_tax_skill"));
    test_clear_messages(u->faction);

    set_level(u, SK_TAXING, 1);
    tax_cmd(u, ord, &taxorders);
    CuAssertPtrEquals(tc, 0, test_find_messagetype(u->faction->msgs, "error_no_tax_skill"));
    CuAssertPtrNotNull(tc, taxorders);

    rsetmoney(r, 11);
    expandtax(r, taxorders);
    CuAssertPtrNotNull(tc, test_find_messagetype(u->faction->msgs, "income"));
    /* taxing is in multiples of 10 */
    CuAssertIntEquals(tc, 10, i_get(u->items, silver));
    test_clear_messages(u->faction);
    i_change(&u->items, silver, -i_get(u->items, silver));

    rsetmoney(r, 1000);
    taxorders = 0;
    tax_cmd(u, ord, &taxorders);
    expandtax(r, taxorders);
    CuAssertIntEquals(tc, 20, i_get(u->items, silver));
    test_clear_messages(u->faction);

    free_order(ord);
    test_cleanup();
}

/** 
 * see https://bugs.eressea.de/view.php?id=2234
 */
static void test_maintain_buildings(CuTest *tc) {
    region *r;
    building *b;
    building_type *btype;
    unit *u;
    faction *f;
    maintenance *req;
    item_type *itype;

    test_cleanup();
    btype = test_create_buildingtype("Hort");
    btype->maxsize = 10;
    r = test_create_region(0, 0, 0);
    f = test_create_faction(0);
    u = test_create_unit(f, r);
    b = test_create_building(r, btype);
    itype = test_create_itemtype("money");
    b->size = btype->maxsize;
    u_set_building(u, b);

    // this building has no upkeep, it just works:
    b->flags = 0;
    maintain_buildings(r);
    CuAssertIntEquals(tc, BLD_MAINTAINED, fval(b, BLD_MAINTAINED));
    CuAssertPtrEquals(tc, 0, f->msgs);
    CuAssertPtrEquals(tc, 0, r->msgs);

    req = calloc(2, sizeof(maintenance));
    req[0].number = 100;
    req[0].rtype = itype->rtype;
    btype->maintenance = req;

    // we cannot afford to pay:
    b->flags = 0;
    maintain_buildings(r);
    CuAssertIntEquals(tc, 0, fval(b, BLD_MAINTAINED));
    CuAssertPtrNotNull(tc, test_find_messagetype(f->msgs, "maintenancefail"));
    CuAssertPtrNotNull(tc, test_find_messagetype(r->msgs, "maintenance_nowork"));
    test_clear_messagelist(&f->msgs);
    test_clear_messagelist(&r->msgs);
    
    // we can afford to pay:
    i_change(&u->items, itype, 100);
    b->flags = 0;
    maintain_buildings(r);
    CuAssertIntEquals(tc, BLD_MAINTAINED, fval(b, BLD_MAINTAINED));
    CuAssertIntEquals(tc, 0, i_get(u->items, itype));
    CuAssertPtrEquals(tc, 0, r->msgs);
    CuAssertPtrEquals(tc, 0, test_find_messagetype(f->msgs, "maintenance_nowork"));
    CuAssertPtrNotNull(tc, test_find_messagetype(f->msgs, "maintenance"));
    test_clear_messagelist(&f->msgs);

    // this building has no owner, it doesn't work:
    u_set_building(u, NULL);
    b->flags = 0;
    maintain_buildings(r);
    CuAssertIntEquals(tc, 0, fval(b, BLD_MAINTAINED));
    CuAssertPtrEquals(tc, 0, f->msgs);
    CuAssertPtrNotNull(tc, test_find_messagetype(r->msgs, "maintenance_noowner"));
    test_clear_messagelist(&r->msgs);

    test_cleanup();
}

static void test_recruit(CuTest *tc) {
    unit *u;
    faction *f;

    test_setup();
    f = test_create_faction(0);
    u = test_create_unit(f, test_create_region(0, 0, 0));
    CuAssertIntEquals(tc, 1, u->number);
    add_recruits(u, 1, 1);
    CuAssertIntEquals(tc, 2, u->number);
    CuAssertPtrEquals(tc, u, f->units);
    CuAssertPtrEquals(tc, NULL, u->nextF);
    CuAssertPtrEquals(tc, NULL, u->prevF);
    CuAssertPtrEquals(tc, NULL, test_find_messagetype(f->msgs, "recruit"));
    add_recruits(u, 1, 2);
    CuAssertIntEquals(tc, 3, u->number);
    CuAssertPtrNotNull(tc, test_find_messagetype(f->msgs, "recruit"));
    test_cleanup();
}

static void test_income(CuTest *tc)
{
    race *rc;
    unit *u;
    test_setup();
    rc = test_create_race("nerd");
    u = test_create_unit(test_create_faction(rc), test_create_region(0, 0, 0));
    CuAssertIntEquals(tc, 20, income(u));
    u->number = 5;
    CuAssertIntEquals(tc, 100, income(u));
    test_cleanup();
}

static int limit_logs(const struct region * r, const struct resource_type * rtype)
{
    return 10;
}

static void setup_produce(CuTest *tc) {
    unit *u;
    item_type * itype;
    struct locale *lang;
    resource_limit *rdata;
    attrib *a;
 
    lang = test_create_locale();
    locale_setstring(lang, "log", "Holz");
    itype = test_create_itemtype("log");
    itype->rtype->flags |= RTF_LIMITED;
    a = a_add(&itype->rtype->attribs, a_new(&at_resourcelimit));
    rdata = (resource_limit *)a->data.v;
    rdata->guard |= GUARD_TREES;
    rdata->limit = limit_logs;
    itype->construction = calloc(1, sizeof(construction));
    itype->construction->skill = SK_LUMBERJACK;
    itype->construction->minskill = 1;
    CuAssertPtrNotNull(tc, itype);
    CuAssertPtrNotNull(tc, finditemtype("Holz", lang));
    u = test_create_unit(test_create_faction(0), test_create_region(0, 0, 0));
    set_level(u, SK_LUMBERJACK, 1);
    u->faction->locale = lang;
    CuAssertPtrNotNull(tc, u);
}

static void test_produce(CuTest *tc) {
    unit *u;
    item_type * itype;
    faction *f;

    test_setup();
    setup_produce(tc);
    itype = rt_get_or_create("log")->itype;
    u = regions->units;
    f = u->faction;

    // happy case: 1 person w. skill 1 produces 1 log
    u->thisorder = create_order(K_MAKE, f->locale, "Holz");
    CuAssertPtrNotNull(tc, u->thisorder);
    CuAssertIntEquals(tc, 0, make_cmd(u, u->thisorder));
    split_allocations(u->region);
    CuAssertIntEquals(tc, 1, i_get(u->items, itype));
    CuAssertPtrNotNull(tc, test_find_messagetype(f->msgs, "produce"));
    test_clear_messagelist(&f->msgs);

    // limit_logs() is 10, do not produce more:
    scale_number(u, 20);
    CuAssertIntEquals(tc, 0, make_cmd(u, u->thisorder));
    split_allocations(u->region);
    CuAssertIntEquals(tc, 11, i_get(u->items, itype));
    CuAssertPtrNotNull(tc, test_find_messagetype(f->msgs, "produce"));
    test_cleanup();
}

CuSuite *get_economy_suite(void)
{
    CuSuite *suite = CuSuiteNew();
    SUITE_ADD_TEST(suite, test_give_control_building);
    SUITE_ADD_TEST(suite, test_give_control_ship);
    SUITE_ADD_TEST(suite, test_income);
    SUITE_ADD_TEST(suite, test_steal_okay);
    SUITE_ADD_TEST(suite, test_steal_ocean);
    SUITE_ADD_TEST(suite, test_steal_nosteal);
    SUITE_ADD_TEST(suite, test_normals_recruit);
    SUITE_ADD_TEST(suite, test_heroes_dont_recruit);
    SUITE_ADD_TEST(suite, test_tax_cmd);
    SUITE_ADD_TEST(suite, test_maintain_buildings);
    SUITE_ADD_TEST(suite, test_recruit);
    SUITE_ADD_TEST(suite, test_produce);
    return suite;
}
