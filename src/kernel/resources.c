/*
 +-------------------+  Christian Schlittchen <corwin@amber.kn-bremen.de>
 |                   |  Enno Rehling <enno@eressea.de>
 | Eressea PBEM host |  Katja Zedel <katze@felidae.kn-bremen.de>
 | (c) 1998 - 2003   |  Henning Peters <faroul@beyond.kn-bremen.de>
 |                   |  Ingo Wilken <Ingo.Wilken@informatik.uni-oldenburg.de>
 +-------------------+  Stefan Reich <reich@halbling.de>

 This program may not be used, modified or distributed
 without prior permission by the authors of Eressea.
 */

#include <platform.h>
#include "resources.h"

/* kernel includes */
#include <kernel/config.h>
#include <kernel/callbacks.h>
#include "build.h"
#include "building.h"
#include "item.h"
#include "race.h"
#include "region.h"
#include "save.h"
#include "terrain.h"

#include <util/gamedata.h>
#include <util/rand.h>
#include <util/rng.h>

#include <storage.h>

#include <stdlib.h>
#include <assert.h>
#include <string.h>

static double ResourceFactor(void)
{
    return config_get_flt("resource.factor", 1.0);
}

void update_resources(region * r)
{
    struct rawmaterial *res = r->resources;
    while (res) {
        struct rawmaterial_type *raw = rmt_get(res->rtype);
        if (raw && raw->update) {
            raw->update(res, r);
        }
        res = res->next;
    }
}

extern int dice_rand(const char *s);

static void update_resource(struct rawmaterial *res, double modifier)
{
    double amount = (res->level - res->startlevel) / 100.0 * res->divisor + 1;
    amount = ResourceFactor() * res->base * amount * modifier;
    if (amount < 1.0)
        res->amount = 1;
    else
        res->amount = (int)amount;
    assert(res->amount > 0);
}

struct rawmaterial *
add_resource(region * r, int level, int base, int divisor,
const resource_type * rtype)
{
    struct rawmaterial *rm = calloc(sizeof(struct rawmaterial), 1);

    rm->next = r->resources;
    r->resources = rm;
    rm->level = level;
    rm->startlevel = level;
    rm->base = base;
    rm->amount = base;
    rm->divisor = divisor;
    rm->flags = 0;
    rm->rtype = rtype;
    return rm;
}

void terraform_resources(region * r)
{
    int i;
    const terrain_type *terrain = r->terrain;
    bool terraform_all = config_get_int("rules.terraform.all", 0) != 0;

    if (terrain->production == NULL)
        return;
    for (i = 0; terrain->production[i].type; ++i) {
        rawmaterial *rm;
        const terrain_production *production = terrain->production + i;
        const resource_type *rtype = production->type;

        for (rm = r->resources; rm; rm = rm->next) {
            if (rm->rtype == rtype)
                break;
        }
        if (rm) {
            continue;
        }

        if (terraform_all || chance(production->chance)) {
            rawmaterial *rm;
            rawmaterial_type *raw;
            rm = add_resource(r, dice_rand(production->startlevel),
                dice_rand(production->base), dice_rand(production->divisor),
                production->type);
            update_resource(rm, 1.0);
            raw = rmt_get(rm->rtype);
            if (raw && raw->terraform) {
                raw->terraform(rm, r);
            }
        }
    }
}

static void terraform_default(struct rawmaterial *res, const region * r)
{
#define SHIFT 70
    double modifier =
        1.0 + ((rng_int() % (SHIFT * 2 + 1)) - SHIFT) * ((rng_int() % (SHIFT * 2 +
        1)) - SHIFT) / 10000.0;
    res->amount = (int)(res->amount * modifier);  /* random adjustment, +/- 91% */
    if (res->amount < 1)
        res->amount = 1;
    UNUSED_ARG(r);
}

#ifdef RANDOM_CHANGE
static void resource_random_change(int *pvalue, bool used)
{
    int split = 5;
    int rnd = rng_int() % 100;

    if (pvalue == 0 || rnd % 10 >= 10)
        return;
    if (used)
        split = 4;
    /* if a resource was mined this round, there is a 6% probability
     * of a decline and a 4% probability of a raise. */
    /* if it wasn't mined this round, there is an equal probability
     * of 5% for a decline or a raise. */
    if (rnd < split) {
        (*pvalue)++;
    } else {
        (*pvalue)--;
    }
    if ((*pvalue) < 0)
        (*pvalue) = 0;
}
#endif

static int visible_default(const rawmaterial * res, int skilllevel)
/* resources are visible, if skill equals minimum skill to mine them
 * plus current level of difficulty */
{
    const struct item_type *itype = res->rtype->itype;
    if (res->level <= 1
        && res->level + itype->construction->minskill <= skilllevel + 1) {
        assert(res->amount > 0);
        return res->amount;
    }
    else if (res->level + itype->construction->minskill <= skilllevel + 2) {
        assert(res->amount > 0);
        return res->amount;
    }
    return -1;
}

static void use_default(rawmaterial * res, const region * r, int amount)
{
    assert(res->amount > 0 && amount >= 0 && amount <= res->amount);
    res->amount -= amount;
    while (res->amount == 0) {
        double modifier =
            1.0 + ((rng_int() % (SHIFT * 2 + 1)) - SHIFT) * ((rng_int() % (SHIFT * 2 +
            1)) - SHIFT) / 10000.0;
        int i;

        for (i = 0; r->terrain->production[i].type; ++i) {
            if (res->rtype == r->terrain->production[i].type)
                break;
        }

        ++res->level;
        update_resource(res, modifier);
    }
}

struct rawmaterial *rm_get(region * r, const struct resource_type *rtype)
{
    struct rawmaterial *rm = r->resources;
    while (rm && rm->rtype != rtype) {
        rm = rm->next;
    }
    return rm;
}

struct rawmaterial_type *rmt_get(const struct resource_type *rtype)
{
    return rtype->raw;
}

struct rawmaterial_type *rmt_create(struct resource_type *rtype)
{
    rawmaterial_type *rmtype;
    rtype->flags |= RTF_MATERIAL;
    assert(!rtype->raw);
    assert(!rtype->itype || rtype->itype->construction);
    rmtype = rtype->raw = malloc(sizeof(rawmaterial_type));
    rmtype->rtype = rtype;
    rmtype->terraform = terraform_default;
    rmtype->update = NULL;
    rmtype->use = use_default;
    rmtype->visible = visible_default;
    return rmtype;
}

int limit_resource(const struct region *r, const resource_type *rtype)
{
    assert(!rtype->raw);
    if (callbacks.limit_resource) {
        return callbacks.limit_resource(r, rtype);
    }
    return -1;
}

void produce_resource(struct region *r, const struct resource_type *rtype, int amount)
{
    assert(!rtype->raw);
    if (callbacks.produce_resource) {
        callbacks.produce_resource(r, rtype, amount);
    }
}

resource_mod *read_modifiers(gamedata *data)
{
    resource_mod *modifiers = NULL;
    int n;
    char zName[32];

    READ_INT(data->store, &n);
    if (n>0) {
        resource_mod * mod = modifiers = calloc(n + 1, sizeof(resource_mod));
        while (n--) {
            int i;
            READ_INT(data->store, &i);
            mod->type = (resource_modifier_type)i;
            READ_TOK(data->store, zName, sizeof(zName));
            if (zName[0]) {
                mod->btype = bt_get_or_create(zName);
            }
            mod->race = read_race_reference(data->store);
            read_fraction(data->store, &mod->value);
            ++mod;
        }
    }
    return modifiers;
}

void write_modifiers(gamedata *data, const resource_mod *modifiers)
{
    if (modifiers) {
        const resource_mod * mod;
        int n = 0;
        for (mod = modifiers; mod->type != RMT_END; ++mod) {
            ++n;
        }
        WRITE_INT(data->store, n);
        for (mod = modifiers; mod->type != RMT_END; ++mod) {
            WRITE_INT(data->store, mod->type);
            WRITE_TOK(data->store, mod->btype ? mod->btype->_name : NULL);
            write_race_reference(mod->race, data->store);
            write_fraction(data->store, mod->value);
        }
    }
    else {
        WRITE_INT(data->store, 0);
    }
}
