#ifdef _MSC_VER
#include <platform.h>
#endif
#include <kernel/config.h>
#include "economy.h"

#include <util/message.h>
#include <kernel/building.h>
#include <kernel/item.h>
#include <kernel/faction.h>
#include <kernel/messages.h>
#include <kernel/order.h>
#include <kernel/pool.h>
#include <kernel/race.h>
#include <kernel/region.h>
#include <kernel/resources.h>
#include <kernel/ship.h>
#include <kernel/terrain.h>
#include <kernel/terrainid.h>
#include <kernel/unit.h>

#include <util/attrib.h>
#include <util/language.h>
#include <util/macros.h>

#include <CuTest.h>
#include <tests.h>
#include <assert.h>

static void test_give_control_building(CuTest * tc)
{
    unit *u1, *u2;
    building *b;
    struct faction *f;
    region *r;

    test_setup();
    f = test_create_faction(NULL);
    r = test_create_region(0, 0, NULL);
    b = test_create_building(r, NULL);
    u1 = test_create_unit(f, r);
    u_set_building(u1, b);
    u2 = test_create_unit(f, r);
    u_set_building(u2, b);
    CuAssertPtrEquals(tc, u1, building_owner(b));
    give_control(u1, u2);
    CuAssertPtrEquals(tc, u2, building_owner(b));
    test_teardown();
}

static void test_give_control_ship(CuTest * tc)
{
    unit *u1, *u2;
    ship *sh;
    struct faction *f;
    region *r;

    test_setup();
    f = test_create_faction(NULL);
    r = test_create_region(0, 0, NULL);
    sh = test_create_ship(r, NULL);
    u1 = test_create_unit(f, r);
    u_set_ship(u1, sh);
    u2 = test_create_unit(f, r);
    u_set_ship(u2, sh);
    CuAssertPtrEquals(tc, u1, ship_owner(sh));
    give_control(u1, u2);
    CuAssertPtrEquals(tc, u2, ship_owner(sh));
    test_teardown();
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

    test_setup();
    ter = test_create_terrain("plain", LAND_REGION);
    rc = test_create_race("human");
    rc->flags = 0;
    setup_steal(&env, ter, rc);
    CuAssertPtrEquals(tc, 0, steal_message(env.u, 0));
    test_teardown();
}

static void test_steal_nosteal(CuTest * tc) {
    struct steal env;
    race *rc;
    terrain_type *ter;
    message *msg;

    test_setup();
    ter = test_create_terrain("plain", LAND_REGION);
    rc = test_create_race("human");
    rc->flags = RCF_NOSTEAL;
    setup_steal(&env, ter, rc);
    CuAssertPtrNotNull(tc, msg = steal_message(env.u, 0));
    msg_release(msg);
    test_teardown();
}

static void test_steal_ocean(CuTest * tc) {
    struct steal env;
    race *rc;
    terrain_type *ter;
    message *msg;

    test_setup();
    ter = test_create_terrain("ocean", SEA_REGION);
    rc = test_create_race("human");
    setup_steal(&env, ter, rc);
    CuAssertPtrNotNull(tc, msg = steal_message(env.u, 0));
    msg_release(msg);
    test_teardown();
}

static struct unit *create_recruiter(void) {
    region *r;
    faction *f;
    unit *u;
    const resource_type* rtype;

    r=test_create_region(0, 0, NULL);
    rsetpeasants(r, 999);
    f = test_create_faction(NULL);
    u = test_create_unit(f, r);
    rtype = get_resourcetype(R_SILVER);
    change_resource(u, rtype, 1000);
    return u;
}

static void setup_production(void) {
    init_resources();
    mt_register(mt_new_va("produce", "unit:unit", "region:region", "amount:int", "wanted:int", "resource:resource", NULL));
    mt_register(mt_new_va("income", "unit:unit", "region:region", "amount:int", "wanted:int", "mode:int", NULL));
    mt_register(mt_new_va("buy", "unit:unit", "money:int", NULL));
    mt_register(mt_new_va("buyamount", "unit:unit", "amount:int", "resource:resource", NULL));
}

static void test_heroes_dont_recruit(CuTest * tc) {
    unit *u;

    test_setup();
    setup_production();
    u = create_recruiter();

    fset(u, UFL_HERO);
    unit_addorder(u, create_order(K_RECRUIT, default_locale, "1"));

    economics(u->region);

    CuAssertIntEquals(tc, 1, u->number);
    CuAssertPtrNotNull(tc, test_find_messagetype(u->faction->msgs, "error_herorecruit"));

    test_teardown();
}

static void test_normals_recruit(CuTest * tc) {
    unit *u;

    test_setup();
    setup_production();
    u = create_recruiter();
    unit_addorder(u, create_order(K_RECRUIT, default_locale, "1"));

    economics(u->region);

    CuAssertIntEquals(tc, 2, u->number);

    test_teardown();
}

/** 
 * Create any terrain types that are used by the trade rules.
 * 
 * This should prevent newterrain from returning NULL.
 */
static void setup_terrains(CuTest *tc) {
    test_create_terrain("plain", LAND_REGION | FOREST_REGION | WALK_INTO | CAVALRY_REGION | FLY_INTO);
    test_create_terrain("ocean", SEA_REGION | SWIM_INTO | FLY_INTO);
    test_create_terrain("swamp", LAND_REGION | WALK_INTO | FLY_INTO);
    test_create_terrain("desert", LAND_REGION | WALK_INTO | FLY_INTO);
    test_create_terrain("mountain", LAND_REGION | WALK_INTO | FLY_INTO);
    init_terrains();
    CuAssertPtrNotNull(tc, newterrain(T_MOUNTAIN));
    CuAssertPtrNotNull(tc, newterrain(T_OCEAN));
    CuAssertPtrNotNull(tc, newterrain(T_PLAIN));
    CuAssertPtrNotNull(tc, newterrain(T_SWAMP));
    CuAssertPtrNotNull(tc, newterrain(T_DESERT));
}

static region *setup_trade_region(CuTest *tc, const struct terrain_type *terrain) {
    region *r;
    item_type *it_luxury;
    struct locale * lang = default_locale;

    new_luxurytype(it_luxury = test_create_itemtype("jewel"), 5);
    locale_setstring(lang, it_luxury->rtype->_name, it_luxury->rtype->_name);
    CuAssertStrEquals(tc, it_luxury->rtype->_name, LOC(lang, resourcename(it_luxury->rtype, 0)));

    new_luxurytype(it_luxury = test_create_itemtype("balm"), 5);
    locale_setstring(lang, it_luxury->rtype->_name, it_luxury->rtype->_name);
    CuAssertStrEquals(tc, it_luxury->rtype->_name, LOC(lang, resourcename(it_luxury->rtype, 0)));

    r = test_create_region(0, 0, terrain);
    return r;
}

static unit *setup_trade_unit(CuTest *tc, region *r, const struct race *rc) {
    unit *u;

    UNUSED_ARG(tc);
    u = test_create_unit(test_create_faction(rc), r);
    set_level(u, SK_TRADE, 2);
    return u;
}

static void test_trade_insect(CuTest *tc) {
    /* Insekten k�nnen in W�sten und S�mpfen auch ohne Burgen handeln. */
    unit *u;
    region *r;
    const item_type *it_luxury;
    const item_type *it_silver;

    test_setup();
    setup_production();
    test_create_locale();
    setup_terrains(tc);
    r = setup_trade_region(tc, get_terrain("swamp"));
    init_terrains();

    it_luxury = r_luxury(r);
    CuAssertPtrNotNull(tc, it_luxury);
    it_silver = get_resourcetype(R_SILVER)->itype;

    u = setup_trade_unit(tc, r, test_create_race("insect"));
    unit_addorder(u, create_order(K_BUY, u->faction->locale, "1 %s",
        LOC(u->faction->locale, resourcename(it_luxury->rtype, 0))));

    set_item(u, it_silver, 10);
    CuAssertPtrEquals(tc, r, u->region);
    CuAssertPtrEquals(tc, (void *)it_luxury, (void *)r_luxury(u->region));
    produce(u->region);
    CuAssertIntEquals(tc, 1, get_item(u, it_luxury));
    CuAssertIntEquals(tc, 5, get_item(u, it_silver));

    terraform_region(r, get_terrain("swamp"));
    test_teardown();
}

static void test_buy_cmd(CuTest *tc) {
    region * r;
    unit *u;
    building *b;
    const resource_type *rt_silver;
    const item_type *it_luxury;
    test_setup();
    setup_production();
    test_create_locale();
    setup_terrains(tc);
    r = setup_trade_region(tc, test_create_terrain("swamp", LAND_REGION));
    init_terrains();

    it_luxury = r_luxury(r);
    CuAssertPtrNotNull(tc, it_luxury);
    rt_silver = get_resourcetype(R_SILVER);
    CuAssertPtrNotNull(tc, rt_silver);
    CuAssertPtrNotNull(tc, rt_silver->itype);

    u = test_create_unit(test_create_faction(NULL), r);
    unit_addorder(u, create_order(K_BUY, u->faction->locale, "1 %s", LOC(u->faction->locale, resourcename(it_luxury->rtype, 0))));
    set_item(u, rt_silver->itype, 1000);

    produce(r);
    CuAssertPtrNotNullMsg(tc, "trading requires a castle", test_find_messagetype(u->faction->msgs, "error119"));
    test_clear_messages(u->faction);
    freset(u, UFL_LONGACTION);

    b = test_create_building(r, test_create_buildingtype("castle"));
    produce(r);
    CuAssertPtrNotNullMsg(tc, "castle must have size >=2", test_find_messagetype(u->faction->msgs, "error119"));
    test_clear_messages(u->faction);
    freset(u, UFL_LONGACTION);

    b->size = 2;
    produce(r);
    CuAssertPtrEquals(tc, NULL, test_find_messagetype(u->faction->msgs, "error119"));
    CuAssertPtrNotNullMsg(tc, "traders need SK_TRADE skill", test_find_messagetype(u->faction->msgs, "error102"));
    test_clear_messages(u->faction);
    freset(u, UFL_LONGACTION);

    /* at last, the happy case: */
    set_level(u, SK_TRADE, 1);
    produce(r);
    CuAssertPtrNotNull(tc, test_find_messagetype(u->faction->msgs, "buy"));
    CuAssertPtrNotNull(tc, test_find_messagetype(u->faction->msgs, "buyamount"));
    CuAssertIntEquals(tc, 1, get_item(u, it_luxury));
    CuAssertIntEquals(tc, 995, get_item(u, rt_silver->itype));
    test_teardown();
}

static void test_tax_cmd(CuTest *tc) {
    order *ord;
    faction *f;
    region *r;
    unit *u;
    item_type *sword, *silver;
    econ_request *taxorders = 0;

    test_setup();
    setup_production();
    config_set("taxing.perlevel", "20");
    f = test_create_faction(NULL);
    r = test_create_region(0, 0, NULL);
    assert(r && f);
    u = test_create_unit(f, r);

    ord = create_order(K_TAX, f->locale, "");
    assert(ord);

    tax_cmd(u, ord, &taxorders);
    CuAssertPtrNotNull(tc, test_find_messagetype(u->faction->msgs, "error48"));
    test_clear_messages(u->faction);

    silver = get_resourcetype(R_SILVER)->itype;

    sword = test_create_itemtype("sword");
    new_weapontype(sword, 0, frac_zero, NULL, 0, 0, 0, SK_MELEE);
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
    test_teardown();
}

static void setup_economy(void) {
    mt_register(mt_new_va("recruit", "unit:unit", "region:region", "amount:int", "want:int", NULL));
    mt_register(mt_new_va("maintenance", "unit:unit", "building:building", NULL));
    mt_register(mt_new_va("maintenancefail", "unit:unit", "building:building", NULL));
    mt_register(mt_new_va("maintenance_nowork", "building:building", NULL));
    mt_register(mt_new_va("maintenance_noowner", "building:building", NULL));
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

    test_setup();
    setup_economy();
    btype = test_create_buildingtype("Hort");
    btype->maxsize = 10;
    r = test_create_region(0, 0, NULL);
    f = test_create_faction(NULL);
    u = test_create_unit(f, r);
    b = test_create_building(r, btype);
    itype = test_create_itemtype("money");
    b->size = btype->maxsize;
    u_set_building(u, b);

    /* this building has no upkeep, it just works: */
    b->flags = 0;
    maintain_buildings(r);
    CuAssertIntEquals(tc, BLD_MAINTAINED, fval(b, BLD_MAINTAINED));
    CuAssertPtrEquals(tc, 0, f->msgs);
    CuAssertPtrEquals(tc, 0, r->msgs);

    req = calloc(2, sizeof(maintenance));
    req[0].number = 100;
    req[0].rtype = itype->rtype;
    btype->maintenance = req;

    /* we cannot afford to pay: */
    b->flags = 0;
    maintain_buildings(r);
    CuAssertIntEquals(tc, 0, fval(b, BLD_MAINTAINED));
    CuAssertPtrNotNull(tc, test_find_messagetype(f->msgs, "maintenancefail"));
    CuAssertPtrNotNull(tc, test_find_messagetype(r->msgs, "maintenance_nowork"));
    test_clear_messagelist(&f->msgs);
    test_clear_messagelist(&r->msgs);
    
    /* we can afford to pay: */
    i_change(&u->items, itype, 100);
    b->flags = 0;
    maintain_buildings(r);
    CuAssertIntEquals(tc, BLD_MAINTAINED, fval(b, BLD_MAINTAINED));
    CuAssertIntEquals(tc, 0, i_get(u->items, itype));
    CuAssertPtrEquals(tc, 0, r->msgs);
    CuAssertPtrEquals(tc, 0, test_find_messagetype(f->msgs, "maintenance_nowork"));
    CuAssertPtrNotNull(tc, test_find_messagetype(f->msgs, "maintenance"));
    test_clear_messagelist(&f->msgs);

    /* this building has no owner, it doesn't work: */
    u_set_building(u, NULL);
    b->flags = 0;
    maintain_buildings(r);
    CuAssertIntEquals(tc, 0, fval(b, BLD_MAINTAINED));
    CuAssertPtrEquals(tc, 0, f->msgs);
    CuAssertPtrNotNull(tc, test_find_messagetype(r->msgs, "maintenance_noowner"));
    test_clear_messagelist(&r->msgs);

    test_teardown();
}

static void test_recruit(CuTest *tc) {
    unit *u;
    faction *f;

    test_setup();
    setup_economy();
    f = test_create_faction(NULL);
    u = test_create_unit(f, test_create_region(0, 0, NULL));
    CuAssertIntEquals(tc, 1, u->number);
    CuAssertIntEquals(tc, 1, f->num_people);
    CuAssertIntEquals(tc, 1, f->num_units);
    add_recruits(u, 1, 1);
    CuAssertIntEquals(tc, 2, u->number);
    CuAssertIntEquals(tc, 2, f->num_people);
    CuAssertIntEquals(tc, 1, f->num_units);
    CuAssertPtrEquals(tc, u, f->units);
    CuAssertPtrEquals(tc, NULL, u->nextF);
    CuAssertPtrEquals(tc, NULL, u->prevF);
    CuAssertPtrEquals(tc, NULL, test_find_messagetype(f->msgs, "recruit"));
    add_recruits(u, 1, 2);
    CuAssertIntEquals(tc, 3, u->number);
    CuAssertPtrNotNull(tc, test_find_messagetype(f->msgs, "recruit"));
    test_teardown();
}

static void test_income(CuTest *tc)
{
    race *rc;
    unit *u;
    test_setup();
    rc = test_create_race("nerd");
    u = test_create_unit(test_create_faction(rc), test_create_region(0, 0, NULL));
    CuAssertIntEquals(tc, 20, income(u));
    u->number = 5;
    CuAssertIntEquals(tc, 100, income(u));
    test_teardown();
}

static void test_modify_material(CuTest *tc) {
    unit *u;
    struct item_type *itype;
    resource_type *rtype;
    resource_mod *mod;

    test_setup();
    setup_production();

    u = test_create_unit(test_create_faction(NULL), test_create_region(0, 0, NULL));
    set_level(u, SK_WEAPONSMITH, 1);

    /* the unit's race gets 2x savings on iron used to produce goods */
    itype = test_create_itemtype("iron");
    rtype = itype->rtype;
    mod = rtype->modifiers = calloc(2, sizeof(resource_mod));
    mod[0].type = RMT_USE_SAVE;
    mod[0].value = frac_make(2, 1);
    mod[0].race = u_race(u);

    itype = test_create_itemtype("sword");
    make_item(u, itype, 1);
    CuAssertPtrNotNull(tc, test_find_messagetype(u->faction->msgs, "error_cannotmake"));
    CuAssertIntEquals(tc, 0, get_item(u, itype));
    test_clear_messages(u->faction);
    itype->construction = calloc(1, sizeof(construction));
    itype->construction->skill = SK_WEAPONSMITH;
    itype->construction->minskill = 1;
    itype->construction->maxsize = 1;
    itype->construction->reqsize = 1;
    itype->construction->materials = calloc(2, sizeof(requirement));
    itype->construction->materials[0].rtype = rtype;
    itype->construction->materials[0].number = 2;

    set_item(u, rtype->itype, 1); /* 1 iron should get us 1 sword */
    make_item(u, itype, 1);
    CuAssertIntEquals(tc, 1, get_item(u, itype));
    CuAssertIntEquals(tc, 0, get_item(u, rtype->itype));

    u_setrace(u, test_create_race("smurf"));
    set_item(u, rtype->itype, 2); /* 2 iron should be required now */
    make_item(u, itype, 1);
    CuAssertIntEquals(tc, 2, get_item(u, itype));
    CuAssertIntEquals(tc, 0, get_item(u, rtype->itype));

    test_teardown();
}

static void test_modify_skill(CuTest *tc) {
    unit *u;
    struct item_type *itype;
    /* building_type *btype; */
    resource_type *rtype;
    resource_mod *mod;

    test_setup();
    setup_production();

    u = test_create_unit(test_create_faction(NULL), test_create_region(0, 0, NULL));
    set_level(u, SK_WEAPONSMITH, 1);

    itype = test_create_itemtype("iron");
    rtype = itype->rtype;

    itype = test_create_itemtype("sword");
    make_item(u, itype, 1);
    CuAssertPtrNotNull(tc, test_find_messagetype(u->faction->msgs, "error_cannotmake"));
    CuAssertIntEquals(tc, 0, get_item(u, itype));
    test_clear_messages(u->faction);
    itype->construction = calloc(1, sizeof(construction));
    itype->construction->skill = SK_WEAPONSMITH;
    itype->construction->minskill = 1;
    itype->construction->maxsize = -1;
    itype->construction->reqsize = 1;
    itype->construction->materials = calloc(2, sizeof(requirement));
    itype->construction->materials[0].rtype = rtype;
    itype->construction->materials[0].number = 1;

    /* our race gets a +1 bonus to the item's production skill */
    mod = itype->rtype->modifiers = calloc(2, sizeof(resource_mod));
    mod[0].type = RMT_PROD_SKILL;
    mod[0].value.sa[0] = SK_WEAPONSMITH;
    mod[0].value.sa[1] = 1;
    mod[0].race = u_race(u);

    set_item(u, rtype->itype, 2); /* 2 iron should get us 2 swords */
    make_item(u, itype, 2);
    CuAssertIntEquals(tc, 2, get_item(u, itype));
    CuAssertIntEquals(tc, 0, get_item(u, rtype->itype));

    mod[0].value.sa[0] = NOSKILL; /* match any skill */
    set_item(u, rtype->itype, 2);
    make_item(u, itype, 2);
    CuAssertIntEquals(tc, 4, get_item(u, itype));
    CuAssertIntEquals(tc, 0, get_item(u, rtype->itype));


    u_setrace(u, test_create_race("smurf"));
    set_item(u, rtype->itype, 2);
    make_item(u, itype, 1); /* only enough skill to make 1 now */
    CuAssertIntEquals(tc, 5, get_item(u, itype));
    CuAssertIntEquals(tc, 1, get_item(u, rtype->itype));

    test_teardown();
}


static void test_modify_production(CuTest *tc) {
    unit *u;
    struct item_type *itype;
    const struct resource_type *rt_silver;
    resource_type *rtype;
    double d = 0.6;

    test_setup();
    setup_production();

    /* make items from other items (turn silver to stone) */
    rt_silver = get_resourcetype(R_SILVER);
    itype = test_create_itemtype("stone");
    rtype = itype->rtype;
    u = test_create_unit(test_create_faction(NULL), test_create_region(0, 0, NULL));
    make_item(u, itype, 1);
    CuAssertPtrNotNull(tc, test_find_messagetype(u->faction->msgs, "error_cannotmake"));
    CuAssertIntEquals(tc, 0, get_item(u, itype));
    test_clear_messages(u->faction);
    itype->construction = calloc(1, sizeof(construction));
    itype->construction->skill = SK_ALCHEMY;
    itype->construction->minskill = 1;
    itype->construction->maxsize = 1;
    itype->construction->reqsize = 1;
    itype->construction->materials = calloc(2, sizeof(requirement));
    itype->construction->materials[0].rtype = rt_silver;
    itype->construction->materials[0].number = 1;
    set_level(u, SK_ALCHEMY, 1);
    set_item(u, rt_silver->itype, 1);
    make_item(u, itype, 1);
    CuAssertIntEquals(tc, 1, get_item(u, itype));
    CuAssertIntEquals(tc, 0, get_item(u, rt_silver->itype));

    /* make level-based raw materials, no materials used in construction */
    free(itype->construction->materials);
    itype->construction->materials = 0;
    rtype->flags |= RTF_LIMITED;
    rmt_create(rtype);
    add_resource(u->region, 1, 300, 150, rtype); /* there are 300 stones at level 1 */
    CuAssertIntEquals(tc, 300, region_getresource(u->region, rtype));
    set_level(u, SK_ALCHEMY, 10);

    make_item(u, itype, 10);
    split_allocations(u->region);
    CuAssertIntEquals(tc, 11, get_item(u, itype));
    CuAssertIntEquals(tc, 290, region_getresource(u->region, rtype)); /* used 10 stones to make 10 stones */

    rtype->modifiers = calloc(3, sizeof(resource_mod));
    rtype->modifiers[0].type = RMT_PROD_SAVE;
    rtype->modifiers[0].race = u->_race;
    rtype->modifiers[0].value.sa[0] = (short)(0.5+100*d);
    rtype->modifiers[0].value.sa[1] = 100;
    rtype->modifiers[1].type = RMT_END;
    make_item(u, itype, 10);
    split_allocations(u->region);
    CuAssertIntEquals(tc, 21, get_item(u, itype));
    CuAssertIntEquals(tc, 284, region_getresource(u->region, rtype)); /* 60% saving = 6 stones make 10 stones */

    make_item(u, itype, 1);
    split_allocations(u->region);
    CuAssertIntEquals(tc, 22, get_item(u, itype));
    CuAssertIntEquals(tc, 283, region_getresource(u->region, rtype)); /* no free lunches */

    rtype->modifiers[0].value = frac_make(1, 2);
    make_item(u, itype, 6);
    split_allocations(u->region);
    CuAssertIntEquals(tc, 28, get_item(u, itype));
    CuAssertIntEquals(tc, 280, region_getresource(u->region, rtype)); /* 50% saving = 3 stones make 6 stones */

    rtype->modifiers[0].type = RMT_PROD_REQUIRE;
    rtype->modifiers[0].race = NULL;
    rtype->modifiers[0].btype = bt_get_or_create("mine");

    test_clear_messages(u->faction);
    make_item(u, itype, 10);
    CuAssertIntEquals(tc, 28, get_item(u, itype));
    CuAssertPtrNotNull(tc, test_find_messagetype(u->faction->msgs, "building_needed"));

    rtype->modifiers[0].type = RMT_PROD_REQUIRE;
    rtype->modifiers[0].race = test_create_race("smurf");
    rtype->modifiers[0].btype = NULL;

    test_clear_messages(u->faction);
    make_item(u, itype, 10);
    CuAssertIntEquals(tc, 28, get_item(u, itype));
    CuAssertPtrNotNull(tc, test_find_messagetype(u->faction->msgs, "error117"));

    rtype->modifiers[1].type = RMT_PROD_REQUIRE;
    rtype->modifiers[1].race = u_race(u);
    rtype->modifiers[1].btype = NULL;
    rtype->modifiers[2].type = RMT_END;

    test_clear_messages(u->faction);
    make_item(u, itype, 10);
    CuAssertPtrEquals(tc, NULL, u->faction->msgs);
    split_allocations(u->region);
    CuAssertIntEquals(tc, 38, get_item(u, itype));

    test_teardown();
}

CuSuite *get_economy_suite(void)
{
    CuSuite *suite = CuSuiteNew();
    SUITE_ADD_TEST(suite, test_give_control_building);
    SUITE_ADD_TEST(suite, test_give_control_ship);
    SUITE_ADD_TEST(suite, test_income);
    SUITE_ADD_TEST(suite, test_modify_production);
    SUITE_ADD_TEST(suite, test_modify_skill);
    SUITE_ADD_TEST(suite, test_modify_material);
    SUITE_ADD_TEST(suite, test_steal_okay);
    SUITE_ADD_TEST(suite, test_steal_ocean);
    SUITE_ADD_TEST(suite, test_steal_nosteal);
    SUITE_ADD_TEST(suite, test_normals_recruit);
    SUITE_ADD_TEST(suite, test_heroes_dont_recruit);
    SUITE_ADD_TEST(suite, test_tax_cmd);
    SUITE_ADD_TEST(suite, test_buy_cmd);
    SUITE_ADD_TEST(suite, test_trade_insect);
    SUITE_ADD_TEST(suite, test_maintain_buildings);
    SUITE_ADD_TEST(suite, test_recruit);
    return suite;
}
