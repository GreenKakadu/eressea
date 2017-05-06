/*
Copyright (c) 1998-2015, Enno Rehling <enno@eressea.de>
Katja Zedel <katze@felidae.kn-bremen.de
Christian Schlittchen <corwin@amber.kn-bremen.de>

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
**/

#include <platform.h>
#include "spell.h"

#include <kernel/config.h>
#include <kernel/item.h>

/* util includes */
#include <util/gamedata.h>
#include <util/language.h>
#include <util/log.h>
#include <util/strings.h>
#include <util/umlaut.h>

#include <storage.h>
#include <critbit.h>
#include <selist.h>

/* libc includes */
#include <assert.h>
#include <stdlib.h>
#include <string.h>


static critbit_tree cb_spell_fun;
void add_spellcast(const char *sname, spell_f fun)
{
    size_t len;
    char data[64];

    len = cb_new_kv(sname, strlen(sname), &fun, sizeof(fun), data);
    assert(len <= sizeof(data));
    cb_insert(&cb_spell_fun, data, len);
}

spell_f get_spellcast(const char *sname)
{
    void * match;
    spell_f result = NULL;

    if (cb_find_prefix(&cb_spell_fun, sname, strlen(sname) + 1, &match, 1, 0)) {
        cb_get_kv(match, &result, sizeof(result));
    }
    return result;
}

static critbit_tree cb_fumble_fun;
void add_fumble(const char *sname, fumble_f fun)
{
    size_t len;
    char data[64];

    len = cb_new_kv(sname, strlen(sname), &fun, sizeof(fun), data);
    assert(len <= sizeof(data));
    cb_insert(&cb_fumble_fun, data, len);
}

fumble_f get_fumble(const char *sname)
{
    void * match;
    fumble_f result = NULL;

    if (cb_find_prefix(&cb_fumble_fun, sname, strlen(sname) + 1, &match, 1, 0)) {
        cb_get_kv(match, &result, sizeof(result));
    }
    return result;
}

static critbit_tree cb_spells;
selist * spells;

static void free_spell(spell *sp) {
    free(sp->syntax);
    free(sp->parameter);
    free(sp->sname);
    free(sp->components);
    free(sp);
}

static void free_spell_cb(void *cbdata) {
    free_spell((spell *)cbdata);
}

void free_spells(void) {
    cb_clear(&cb_fumble_fun);
    cb_clear(&cb_spell_fun);
    cb_clear(&cb_spells);
    selist_foreach(spells, free_spell_cb);
    selist_free(spells);
    spells = 0;
}

void add_spell(struct selist **slistp, spell * sp)
{
    if (!selist_set_insert(slistp, sp, NULL)) {
        log_error("add_spell: the list already contains the spell '%s'.\n", sp->sname);
    }
}

spell * create_spell(const char * name)
{
    spell * sp;
    char buffer[64];
    size_t len = strlen(name);

    assert(len + sizeof(sp) < sizeof(buffer));

    if (cb_find_str(&cb_spells, name)) {
        log_error("create_spell: duplicate name '%s'", name);
        return 0;
    }
    sp = (spell *)calloc(1, sizeof(spell));
    len = cb_new_kv(name, len, &sp, sizeof(sp), buffer);
    if (cb_insert(&cb_spells, buffer, len) == CB_SUCCESS) {
        sp->sname = strdup(name);
        add_spell(&spells, sp);
        return sp;
    }
    free(sp);
    return 0;
}

static const char *sp_aliases[][2] = {
    { "gwyrrdfamiliar", "summon_familiar" },
    { "illaunfamiliar", "summon_familiar" },
    { "draigfamiliar", "summon_familiar" },
    { "commonfamiliar", "summon_familiar" },
    { "cerrdorfumbleshield", "cerddorfumbleshield" },
    { NULL, NULL },
};

static const char *sp_alias(const char *zname)
{
    int i;
    for (i = 0; sp_aliases[i][0]; ++i) {
        if (strcmp(sp_aliases[i][0], zname) == 0)
            return sp_aliases[i][1];
    }
    return zname;
}

spell *find_spell(const char *name)
{
    const char * match;
    spell * sp = 0;
    const char * alias = sp_alias(name);

    match = cb_find_str(&cb_spells, alias);
    if (match) {
        cb_get_kv(match, &sp, sizeof(sp));
    }
    else {
        log_debug("find_spell: could not find spell '%s'\n", name);
    }
    return sp;
}

struct spellref *spellref_create(spell *sp, const char *name)
{
    spellref *spref = malloc(sizeof(spellref));

    if (sp) {
        spref->sp = sp;
        spref->name = strdup(sp->sname);
    }
    else if (name) {
        spref->name = strdup(name);
        spref->sp = NULL;
    }
    return spref;
}

void spellref_free(spellref *spref)
{
    if (spref) {
        free(spref->name);
        free(spref);
    }
}

struct spell *spellref_get(struct spellref *spref)
{
    if (!spref->sp) {
        assert(spref->name);
        spref->sp = find_spell(spref->name);
        if (spref->sp) {
            free(spref->name);
            spref->name = NULL;
        }
    }
    return spref->sp;
}

static spell *read_spell(gamedata *data) {
    spell *sp;
    int i, n;
    char zName[32];
    READ_INT(data->store, &i);
    if (i<0) {
        return NULL;;
    }
    READ_TOK(data->store, zName, sizeof(zName));
    sp = create_spell(zName);
    sp->sptyp = i;
    READ_TOK(data->store, zName, sizeof(zName));
    if (zName[0]) {
        sp->syntax = strdup(zName);
    }
    READ_TOK(data->store, zName, sizeof(zName));
    if (zName[0]) {
        sp->parameter = strdup(zName);
    }
    READ_INT(data->store, &sp->rank);

    READ_INT(data->store, &n);
    if (n > 0) {
        sp->components = calloc(n + 1, sizeof(spell_component));
        for (i = 0; i != n; ++i) {
            spell_component *sc = sp->components + i;
            READ_INT(data->store, &sc->amount);
            READ_INT(data->store, &sc->cost);
            READ_TOK(data->store, zName, sizeof(zName));
            if (zName[0]) {
                sc->type = rt_get_or_create(zName);
            }
        }
    }
    return sp;
}

static void write_spell(gamedata *data, spell *sp) {
    int i, n = 0;
    assert(sp->sptyp > 0);
    WRITE_INT(data->store, sp->sptyp);
    WRITE_TOK(data->store, sp->sname);
    WRITE_TOK(data->store, sp->syntax);
    WRITE_TOK(data->store, sp->parameter);
    WRITE_INT(data->store, sp->rank);
    if (sp->components) {
        for (i = 0; sp->components[i].type; ++i) ++n;

        WRITE_INT(data->store, n);
        for (i = 0; i != n; ++i) {
            spell_component *sc = sp->components + i;
            WRITE_INT(data->store, sc->amount);
            WRITE_INT(data->store, sc->cost);
            WRITE_TOK(data->store, sc->type->_name);
        }
    }
    else {
        WRITE_INT(data->store, 0);
    }
}

static bool write_spell_cb(void *el, void *cbdata) {
    spell *sp = (spell *)el;
    gamedata *data = (gamedata *)cbdata;
    write_spell(data, sp);
    return true;
}

void write_spells(gamedata *data) {
    selist_foreach_ex(spells, write_spell_cb, data);
    WRITE_INT(data->store, -1);
}

void read_spells(gamedata *data) {
    spell *sp;

    do {
        sp = read_spell(data);
    } while (sp);
}

