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

#ifdef _MSC_VER
#include <platform.h>
#endif
#include <kernel/config.h>

#include "report.h"
#include "reports.h"
#include "guard.h"
#include "laws.h"
#include "market.h"
#include "monsters.h"
#include "travelthru.h"

#include <spells/regioncurse.h>
#include <spells/buildingcurse.h>

/* modules includes */
#include <modules/score.h>

/* attributes includes */
#include <attributes/overrideroads.h>
#include <attributes/otherfaction.h>
#include <attributes/reduceproduction.h>

/* gamecode includes */
#include "alchemy.h"
#include "calendar.h"
#include "economy.h"
#include "move.h"
#include "upkeep.h"
#include "vortex.h"
#include "teleport.h"

/* kernel includes */
#include <kernel/ally.h>
#include <kernel/connection.h>
#include <kernel/build.h>
#include <kernel/building.h>
#include <kernel/curse.h>
#include <kernel/faction.h>
#include <kernel/group.h>
#include <kernel/item.h>
#include <kernel/messages.h>
#include <kernel/objtypes.h>
#include <kernel/order.h>
#include <kernel/plane.h>
#include <kernel/pool.h>
#include <kernel/race.h>
#include <kernel/region.h>
#include <kernel/render.h>
#include <kernel/resources.h>
#include <kernel/ship.h>
#include <kernel/spell.h>
#include <kernel/spellbook.h>
#include <kernel/terrain.h>
#include <kernel/terrainid.h>
#include <kernel/unit.h>
#include <kernel/alliance.h>

/* util includes */
#include <util/attrib.h>
#include <util/base36.h>
#include <util/bsdstring.h>
#include <util/goodies.h>
#include <util/language.h>
#include <util/lists.h>
#include <util/log.h>
#include <util/message.h>
#include <util/nrmessage.h>
#include <util/rng.h>
#include <util/strings.h>

#include <selist.h>
#include <filestream.h>
#include <stream.h>

/* libc includes */
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <limits.h>
#include <stdlib.h>

#define ECHECK_VERSION "4.01"

extern int *storms;
extern int weeks_per_month;
extern int months_per_year;

static void check_errno(const char * file, int line) {
    if (errno) {
        char zText[64];
        sprintf(zText, "error %d during report at %s:%d", errno, file, line);
        perror(zText);
        errno = 0;
    }
}

#define CHECK_ERRNO() check_errno(__FILE__, __LINE__)

static char *gamedate_season(const struct locale *lang)
{
    static char buf[256]; /* FIXME: static return value */
    gamedate gd;

    assert(weeknames);

    get_gamedate(turn, &gd);

    sprintf(buf, (const char *)LOC(lang, "nr_calendar_season"),
        LOC(lang, mkname("calendar", weeknames[gd.week])),
        LOC(lang, mkname("calendar", calendar_month(gd.month))),
        gd.year,
        LOC(lang, mkname("calendar", calendar_era())), LOC(lang, mkname("calendar", seasonnames[gd.season])));

    return buf;
}

void newline(struct stream *out) {
    sputs("", out);
}

void write_spaces(struct stream *out, size_t num) {
    static const char spaces[REPORTWIDTH] = "                                                                             ";
    while (num > 0) {
        size_t bytes = (num > REPORTWIDTH) ? REPORTWIDTH : num;
        swrite(spaces, sizeof(char), bytes, out);
        num -= bytes;
    }
}

static void centre(struct stream *out, const char *s, bool breaking)
{
    /* Bei Namen die genau 80 Zeichen lang sind, kann es hier Probleme
     * geben. Seltsamerweise wird i dann auf MAXINT oder aehnlich
     * initialisiert. Deswegen keine Strings die laenger als REPORTWIDTH
     * sind! */

    if (breaking && REPORTWIDTH < strlen(s)) {
        strlist *T, *SP = 0;
        sparagraph(&SP, s, 0, 0);
        T = SP;
        while (SP) {
            centre(out, SP->s, false);
            SP = SP->next;
        }
        freestrlist(T);
    }
    else {
        write_spaces(out, (REPORTWIDTH - strlen(s) + 1) / 2);
        sputs(s, out);
    }
}

static void
paragraph(struct stream *out, const char *str, ptrdiff_t indent, int hanging_indent,
    char marker)
{
    size_t length = REPORTWIDTH;
    const char *end, *begin, *mark = 0;

    if (!str) return;
    /* find out if there's a mark + indent already encoded in the string. */
    if (!marker) {
        const char *x = str;
        while (*x == ' ')
            ++x;
        indent += x - str;
        if (x[0] && indent && x[1] == ' ') {
            indent += 2;
            mark = x;
            str = x + 2;
            hanging_indent -= 2;
        }
    }
    else {
        mark = &marker;
    }
    begin = end = str;

    do {
        const char *last_space = begin;

        if (mark && indent >= 2) {
            write_spaces(out, indent - 2);
            swrite(mark, sizeof(char), 1, out);
            write_spaces(out, 1);
            mark = 0;
        }
        else if (begin == str) {
            write_spaces(out, indent);
        }
        else {
            write_spaces(out, indent + hanging_indent);
        }
        while (*end && end <= begin + length - indent) {
            if (*end == ' ') {
                last_space = end;
            }
            ++end;
        }
        if (*end == 0)
            last_space = end;
        if (last_space == begin) {
            /* there was no space in this line. clip it */
            last_space = end;
        }
        swrite(begin, sizeof(char), last_space - begin, out);
        begin = last_space;
        while (*begin == ' ') {
            ++begin;
        }
        if (begin > end)
            begin = end;
        sputs("", out);
    } while (*begin);
}

static size_t write_spell_modifier(const spell * sp, int flag, const char * str, bool cont, char * bufp, size_t size) {
    if (sp->sptyp & flag) {
        size_t bytes = 0;
        if (cont) {
            bytes = str_strlcpy(bufp, ", ", size);
        }
        else {
            bytes = str_strlcpy(bufp, " ", size);
        }
        bytes += str_strlcpy(bufp + bytes, str, size - bytes);
        return bytes;
    }
    return 0;
}

void nr_spell_syntax(struct stream *out, spellbook_entry * sbe, const struct locale *lang)
{
    int bytes;
    char buf[4096];
    char *bufp = buf;
    size_t size = sizeof(buf) - 1;
    const spell * sp = sbe->sp;
    const char *params = sp->parameter;

    if (sp->sptyp & ISCOMBATSPELL) {
        bytes = (int)str_strlcpy(bufp, LOC(lang, keyword(K_COMBATSPELL)), size);
    }
    else {
        bytes = (int)str_strlcpy(bufp, LOC(lang, keyword(K_CAST)), size);
    }
    if (wrptr(&bufp, &size, bytes) != 0)
        WARN_STATIC_BUFFER();

    /* Reihenfolge beachten: Erst REGION, dann STUFE! */
    if (sp->sptyp & FARCASTING) {
        bytes = snprintf(bufp, size, " [%s x y]", LOC(lang, parameters[P_REGION]));
        if (wrptr(&bufp, &size, bytes) != 0)
            WARN_STATIC_BUFFER();
    }
    if (sp->sptyp & SPELLLEVEL) {
        bytes = snprintf(bufp, size, " [%s n]", LOC(lang, parameters[P_LEVEL]));
        if (wrptr(&bufp, &size, bytes) != 0)
            WARN_STATIC_BUFFER();
    }

    bytes = (int)snprintf(bufp, size, " \"%s\"", spell_name(sp, lang));
    if (wrptr(&bufp, &size, bytes) != 0)
        WARN_STATIC_BUFFER();

    while (params && *params) {
        typedef struct starget {
            param_t param;
            int flag;
            const char *vars;
        } starget;
        starget targets[] = {
            { P_REGION, REGIONSPELL, NULL },
            { P_UNIT, UNITSPELL, "par_unit" },
            { P_SHIP, SHIPSPELL, "par_ship" },
            { P_BUILDING, BUILDINGSPELL, "par_building" },
            { 0, 0, NULL }
        };
        starget *targetp;
        char cp = *params++;
        int i, maxparam = 0;
        const char *locp;
        const char *syntaxp = sp->syntax;

        if (cp == 'u') {
            targetp = targets + 1;
            locp = LOC(lang, targetp->vars);
            bytes = (int)snprintf(bufp, size, " <%s>", locp);
            if (*params == '+') {
                ++params;
                if (wrptr(&bufp, &size, bytes) != 0)
                    WARN_STATIC_BUFFER();
                bytes = (int)snprintf(bufp, size, " [<%s> ...]", locp);
            }
            if (wrptr(&bufp, &size, bytes) != 0)
                WARN_STATIC_BUFFER();
        }
        else if (cp == 's') {
            targetp = targets + 2;
            locp = LOC(lang, targetp->vars);
            bytes = (int)snprintf(bufp, size, " <%s>", locp);
            if (*params == '+') {
                ++params;
                if (wrptr(&bufp, &size, bytes) != 0)
                    WARN_STATIC_BUFFER();
                bytes = (int)snprintf(bufp, size, " [<%s> ...]", locp);
            }
            if (wrptr(&bufp, &size, bytes) != 0)
                WARN_STATIC_BUFFER();
        }
        else if (cp == 'r') {
            bytes = (int)str_strlcpy(bufp, " <x> <y>", size);
            if (*params == '+') {
                ++params;
                if (wrptr(&bufp, &size, bytes) != 0)
                    WARN_STATIC_BUFFER();
                bytes = (int)str_strlcpy(bufp, " [<x> <y> ...]", size);
            }
            if (wrptr(&bufp, &size, bytes) != 0)
                WARN_STATIC_BUFFER();
        }
        else if (cp == 'b') {
            targetp = targets + 3;
            locp = LOC(lang, targetp->vars);
            bytes = (int)snprintf(bufp, size, " <%s>", locp);
            if (*params == '+') {
                ++params;
                if (wrptr(&bufp, &size, bytes) != 0)
                    WARN_STATIC_BUFFER();
                bytes = (int)snprintf(bufp, size, " [<%s> ...]", locp);
            }
            if (wrptr(&bufp, &size, bytes) != 0)
                WARN_STATIC_BUFFER();
        }
        else if (cp == 'k') {
            bool multi = false;
            if (params && *params == 'c') {
                /* skip over a potential id */
                ++params;
            }
            if (params && *params == '+') {
                ++params;
                multi = true;
            }
            for (targetp = targets; targetp->flag; ++targetp) {
                if (sp->sptyp & targetp->flag)
                    ++maxparam;
            }
            if (!maxparam || maxparam > 1) {
                bytes = (int)str_strlcpy(bufp, " (", size);
                if (wrptr(&bufp, &size, bytes) != 0)
                    WARN_STATIC_BUFFER();
            }
            i = 0;
            for (targetp = targets; targetp->flag; ++targetp) {
                if (!maxparam || sp->sptyp & targetp->flag) {
                    if (i++ != 0) {
                        bytes = (int)str_strlcpy(bufp, " |", size);
                        if (wrptr(&bufp, &size, bytes) != 0)
                            WARN_STATIC_BUFFER();
                    }
                    if (targetp->param && targetp->vars) {
                        locp = LOC(lang, targetp->vars);
                        bytes =
                            (int)snprintf(bufp, size, " %s <%s>", parameters[targetp->param],
                                locp);
                        if (multi) {
                            if (wrptr(&bufp, &size, bytes) != 0)
                                WARN_STATIC_BUFFER();
                            bytes = (int)snprintf(bufp, size, " [<%s> ...]", locp);
                        }
                    }
                    else {
                        bytes =
                            (int)snprintf(bufp, size, " %s", parameters[targetp->param]);
                    }
                    if (wrptr(&bufp, &size, bytes) != 0)
                        WARN_STATIC_BUFFER();
                }
            }
            if (!maxparam || maxparam > 1) {
                bytes = (int)str_strlcpy(bufp, " )", size);
                if (wrptr(&bufp, &size, bytes) != 0)
                    WARN_STATIC_BUFFER();
            }
        }
        else if (cp == 'i' || cp == 'c') {
            const char *cstr;
            assert(syntaxp);
            cstr = strchr(syntaxp, ':');
            if (!cstr) {
                locp = LOC(lang, mkname("spellpar", syntaxp));
            }
            else {
                char substr[32];
                size_t len = cstr - syntaxp;
                assert(sizeof(substr) > len);
                memcpy(substr, syntaxp, len);
                substr[len] = 0;
                locp = LOC(lang, mkname("spellpar", substr));
                syntaxp = substr + 1;
            }
            if (*params == '?') {
                ++params;
                bytes = (int)snprintf(bufp, size, " [<%s>]", locp);
            }
            else {
                bytes = (int)snprintf(bufp, size, " <%s>", locp);
            }
            if (wrptr(&bufp, &size, bytes) != 0)
                WARN_STATIC_BUFFER();
        }
        else {
            log_error("unknown spell parameter %c for spell %s", cp, sp->sname);
        }
    }
    *bufp = 0;
    paragraph(out, buf, 2, 0, 0);

}

void nr_spell(struct stream *out, spellbook_entry * sbe, const struct locale *lang)
{
    int bytes, k, itemanz, costtyp;
    char buf[4096];
    char *startp, *bufp = buf;
    size_t size = sizeof(buf) - 1;
    const spell * sp = sbe->sp;

    newline(out);
    centre(out, spell_name(sp, lang), true);
    newline(out);
    paragraph(out, LOC(lang, "nr_spell_description"), 0, 0, 0);
    paragraph(out, spell_info(sp, lang), 2, 0, 0);

    bytes = (int)str_strlcpy(bufp, LOC(lang, "nr_spell_type"), size);
    if (wrptr(&bufp, &size, bytes) != 0)
        WARN_STATIC_BUFFER();

    if (size) {
        *bufp++ = ' ';
        --size;
    }
    if (sp->sptyp & PRECOMBATSPELL) {
        bytes = (int)str_strlcpy(bufp, LOC(lang, "sptype_precombat"), size);
    }
    else if (sp->sptyp & COMBATSPELL) {
        bytes = (int)str_strlcpy(bufp, LOC(lang, "sptype_combat"), size);
    }
    else if (sp->sptyp & POSTCOMBATSPELL) {
        bytes = (int)str_strlcpy(bufp, LOC(lang, "sptype_postcombat"), size);
    }
    else {
        bytes = (int)str_strlcpy(bufp, LOC(lang, "sptype_normal"), size);
    }
    if (wrptr(&bufp, &size, bytes) != 0)
        WARN_STATIC_BUFFER();
    *bufp = 0;
    paragraph(out, buf, 0, 0, 0);

    sprintf(buf, "%s %d", LOC(lang, "nr_spell_level"), sbe->level);
    paragraph(out, buf, 0, 0, 0);

    sprintf(buf, "%s %d", LOC(lang, "nr_spell_rank"), sp->rank);
    paragraph(out, buf, 0, 0, 0);

    paragraph(out, LOC(lang, "nr_spell_components"), 0, 0, 0);
    for (k = 0; sp->components[k].type; ++k) {
        const resource_type *rtype = sp->components[k].type;
        itemanz = sp->components[k].amount;
        costtyp = sp->components[k].cost;
        if (itemanz > 0) {
            size = sizeof(buf) - 1;
            bufp = buf;
            if (sp->sptyp & SPELLLEVEL) {
                bytes =
                    snprintf(bufp, size, "  %d %s", itemanz, LOC(lang, resourcename(rtype,
                        itemanz != 1)));
                if (wrptr(&bufp, &size, bytes) != 0)
                    WARN_STATIC_BUFFER();
                if (costtyp == SPC_LEVEL || costtyp == SPC_LINEAR) {
                    bytes = snprintf(bufp, size, " * %s", LOC(lang, "nr_level"));
                    if (wrptr(&bufp, &size, bytes) != 0)
                        WARN_STATIC_BUFFER();
                }
            }
            else {
                bytes = snprintf(bufp, size, "%d %s", itemanz, LOC(lang, resourcename(rtype, itemanz != 1)));
                if (wrptr(&bufp, &size, bytes) != 0) {
                    WARN_STATIC_BUFFER();
                }
            }
            *bufp = 0;
            paragraph(out, buf, 2, 2, '-');
        }
    }

    size = sizeof(buf) - 1;
    bufp = buf;
    bytes = (int)str_strlcpy(buf, LOC(lang, "nr_spell_modifiers"), size);
    if (wrptr(&bufp, &size, bytes) != 0)
        WARN_STATIC_BUFFER();

    startp = bufp;
    bytes = (int)write_spell_modifier(sp, FARCASTING, LOC(lang, "smod_far"), startp != bufp, bufp, size);
    if (bytes && wrptr(&bufp, &size, bytes) != 0) {
        WARN_STATIC_BUFFER();
    }
    bytes = (int)write_spell_modifier(sp, OCEANCASTABLE, LOC(lang, "smod_sea"), startp != bufp, bufp, size);
    if (bytes && wrptr(&bufp, &size, bytes) != 0) {
        WARN_STATIC_BUFFER();
    }
    bytes = (int)write_spell_modifier(sp, ONSHIPCAST, LOC(lang, "smod_ship"), startp != bufp, bufp, size);
    if (bytes && wrptr(&bufp, &size, bytes) != 0) {
        WARN_STATIC_BUFFER();
    }
    bytes = (int)write_spell_modifier(sp, NOTFAMILIARCAST, LOC(lang, "smod_nofamiliar"), startp != bufp, bufp, size);
    if (bytes && wrptr(&bufp, &size, bytes) != 0) {
        WARN_STATIC_BUFFER();
    }
    if (startp == bufp) {
        bytes = (int)write_spell_modifier(sp, NOTFAMILIARCAST, LOC(lang, "smod_none"), startp != bufp, bufp, size);
        if (bytes && wrptr(&bufp, &size, bytes) != 0) {
            WARN_STATIC_BUFFER();
        }
    }
    *bufp = 0;
    paragraph(out, buf, 0, 0, 0);
    paragraph(out, LOC(lang, "nr_spell_syntax"), 0, 0, 0);

    bufp = buf;
    size = sizeof(buf) - 1;

    nr_spell_syntax(out, sbe, lang);

    newline(out);
}

static void
nr_curses_i(struct stream *out, int indent, const faction *viewer, objtype_t typ, const void *obj, attrib *a, int self)
{
    for (; a; a = a->next) {
        char buf[4096];
        message *msg = 0;

        if (a->type == &at_curse) {
            curse *c = (curse *)a->data.v;

            self = curse_cansee(c, viewer, typ, obj, self);
            msg = msg_curse(c, obj, typ, self);
        }
        else if (a->type == &at_effect && self) {
            effect_data *data = (effect_data *)a->data.v;
            if (data->value > 0) {
                msg = msg_message("nr_potion_effect", "potion left",
                    data->type->rtype, data->value);
            }
        }
        if (msg) {
            newline(out);
            nr_render(msg, viewer->locale, buf, sizeof(buf), viewer);
            paragraph(out, buf, indent, 2, 0);
            msg_release(msg);
        }
    }
}

static void nr_curses(struct stream *out, int indent, const faction *viewer, objtype_t typ, const void *obj)
{
    int self = 0;
    attrib *a = NULL;
    region *r;

    /* Die Sichtbarkeit eines Zaubers und die Zaubermeldung sind bei
    * Gebäuden und Schiffen je nach, ob man Besitzer ist, verschieden.
    * Bei Einheiten sieht man Wirkungen auf eigene Einheiten immer.
    * Spezialfälle (besonderes Talent, verursachender Magier usw. werde
    * bei jedem curse gesondert behandelt. */
    if (typ == TYP_SHIP) {
        ship *sh = (ship *)obj;
        unit *owner = ship_owner(sh);
        a = sh->attribs;
        r = sh->region;
        if (owner) {
            if (owner->faction == viewer) {
                self = 2;
            }
            else {                  /* steht eine person der Partei auf dem Schiff? */
                unit *u = NULL;
                for (u = r->units; u; u = u->next) {
                    if (u->ship == sh) {
                        self = 1;
                        break;
                    }
                }
            }
        }
    }
    else if (typ == TYP_BUILDING) {
        building *b = (building *)obj;
        unit *owner;
        a = b->attribs;
        r = b->region;
        if ((owner = building_owner(b)) != NULL) {
            if (owner->faction == viewer) {
                self = 2;
            }
            else {                  /* steht eine Person der Partei in der Burg? */
                unit *u = NULL;
                for (u = r->units; u; u = u->next) {
                    if (u->building == b) {
                        self = 1;
                        break;
                    }
                }
            }
        }
    }
    else if (typ == TYP_UNIT) {
        unit *u = (unit *)obj;
        a = u->attribs;
        r = u->region;
        if (u->faction == viewer) {
            self = 2;
        }
    }
    else if (typ == TYP_REGION) {
        r = (region *)obj;
        a = r->attribs;
    }
    else {
        log_error("get_attribs: invalid object type %d", typ);
        assert(!"invalid object type");
    }
    nr_curses_i(out, indent, viewer, typ, obj, a, self);
}

static void rps_nowrap(struct stream *out, const char *s)
{
    const char *x = s;
    size_t indent = 0;

    if (!x || !*x) return;

    while (*x++ == ' ');
    indent = x - s - 1;
    if (*(x - 1) && indent && *x == ' ')
        indent += 2;
    x = s;
    while (*s) {
        if (s == x) {
            x = strchr(x + 1, ' ');
            if (!x)
                x = s + strlen(s);
        }
        swrite(s++, sizeof(char), 1, out);
    }
}

static void
nr_unit(struct stream *out, const faction * f, const unit * u, int indent, seen_mode mode)
{
    char marker;
    int dh;
    bool isbattle = (bool)(mode == seen_battle);
    char buf[8192];

    if (fval(u_race(u), RCF_INVISIBLE))
        return;

    newline(out);
    dh = bufunit(f, u, indent, mode, buf, sizeof(buf));

    if (u->faction == f) {
        marker = '*';
    }
    else if (is_allied(u->faction, f)) {
        marker = 'o';
    }
    else if (u->attribs && f != u->faction
        && !fval(u, UFL_ANON_FACTION) && get_otherfaction(u) == f) {
        marker = '!';
    }
    else {
        if (dh && !fval(u, UFL_ANON_FACTION)) {
            marker = '+';
        }
        else {
            marker = '-';
        }
    }
    paragraph(out, buf, indent, 0, marker);

    if (!isbattle) {
        nr_curses(out, indent, f, TYP_UNIT, u);
    }
}

static void
rp_messages(struct stream *out, message_list * msgs, faction * viewer, int indent,
    bool categorized)
{
    nrsection *section;

    if (!msgs)
        return;
    for (section = sections; section; section = section->next) {
        int k = 0;
        struct mlist *m = msgs->begin;
        while (m) {
            /* messagetype * mt = m->type; */
            if (!categorized || strcmp(nr_section(m->msg), section->name) == 0) {
                char lbuf[8192];

                if (!k && categorized) {
                    const char *section_title;
                    char cat_identifier[24];

                    newline(out);
                    sprintf(cat_identifier, "section_%s", section->name);
                    section_title = LOC(viewer->locale, cat_identifier);
                    if (section_title) {
                        centre(out, section_title, true);
                        newline(out);
                    }
                    else {
                        log_error("no title defined for section %s in locale %s", section->name, locale_name(viewer->locale));
                    }
                    k = 1;
                }
                nr_render(m->msg, viewer->locale, lbuf, sizeof(lbuf), viewer);
                paragraph(out, lbuf, indent, 2, 0);
            }
            m = m->next;
        }
        if (!categorized)
            break;
    }
}

static void rp_battles(struct stream *out, faction * f)
{
    if (f->battles != NULL) {
        struct bmsg *bm = f->battles;
        newline(out);
        centre(out, LOC(f->locale, "section_battle"), false);
        newline(out);

        while (bm) {
            char buf[256];
            RENDER(f, buf, sizeof(buf), ("header_battle", "region", bm->r));
            newline(out);
            centre(out, buf, true);
            newline(out);
            rp_messages(out, bm->msgs, f, 0, false);
            bm = bm->next;
        }
    }
}

static void prices(struct stream *out, const region * r, const faction * f)
{
    const luxury_type *sale = NULL;
    struct demand *dmd;
    message *m;
    int bytes, n = 0;
    char buf[4096], *bufp = buf;
    size_t size = sizeof(buf) - 1;

    if (r->land == NULL || r->land->demands == NULL)
        return;
    for (dmd = r->land->demands; dmd; dmd = dmd->next) {
        if (dmd->value == 0)
            sale = dmd->type;
        else if (dmd->value > 0)
            n++;
    }
    assert(sale != NULL);

    m = msg_message("nr_market_sale", "product price",
        sale->itype->rtype, sale->price);

    bytes = (int)nr_render(m, f->locale, bufp, size, f);
    if (wrptr(&bufp, &size, bytes) != 0)
        WARN_STATIC_BUFFER();
    msg_release(m);

    if (n > 0) {
        bytes = (int)str_strlcpy(bufp, " ", size);
        if (wrptr(&bufp, &size, bytes) != 0)
            WARN_STATIC_BUFFER();
        bytes = (int)str_strlcpy(bufp, LOC(f->locale, "nr_trade_intro"), size);
        if (wrptr(&bufp, &size, bytes) != 0)
            WARN_STATIC_BUFFER();
        bytes = (int)str_strlcpy(bufp, " ", size);
        if (wrptr(&bufp, &size, bytes) != 0)
            WARN_STATIC_BUFFER();

        for (dmd = r->land->demands; dmd; dmd = dmd->next) {
            if (dmd->value > 0) {
                m = msg_message("nr_market_price", "product price",
                    dmd->type->itype->rtype, dmd->value * dmd->type->price);
                bytes = (int)nr_render(m, f->locale, bufp, size, f);
                if (wrptr(&bufp, &size, bytes) != 0)
                    WARN_STATIC_BUFFER();
                msg_release(m);
                n--;
                if (n == 0) {
                    bytes = (int)str_strlcpy(bufp, LOC(f->locale, "nr_trade_end"),
                        size);
                    if (wrptr(&bufp, &size, bytes) != 0)
                        WARN_STATIC_BUFFER();
                }
                else if (n == 1) {
                    bytes = (int)str_strlcpy(bufp, " ", size);
                    if (wrptr(&bufp, &size, bytes) != 0)
                        WARN_STATIC_BUFFER();
                    bytes = (int)str_strlcpy(bufp, LOC(f->locale, "nr_trade_final"),
                        size);
                    if (wrptr(&bufp, &size, bytes) != 0)
                        WARN_STATIC_BUFFER();
                    bytes = (int)str_strlcpy(bufp, " ", size);
                    if (wrptr(&bufp, &size, bytes) != 0)
                        WARN_STATIC_BUFFER();
                }
                else {
                    bytes = (int)str_strlcpy(bufp, LOC(f->locale, "nr_trade_next"),
                        size);
                    if (wrptr(&bufp, &size, bytes) != 0)
                        WARN_STATIC_BUFFER();
                    bytes = (int)str_strlcpy(bufp, " ", size);
                    if (wrptr(&bufp, &size, bytes) != 0)
                        WARN_STATIC_BUFFER();
                }
            }
        }
    }
    /* Schreibe Paragraphen */
    *bufp = 0;
    paragraph(out, buf, 0, 0, 0);
}

bool see_border(const connection * b, const faction * f, const region * r)
{
    bool cs = b->type->fvisible(b, f, r);
    if (!cs) {
        cs = b->type->rvisible(b, r);
        if (!cs) {
            const unit *us = r->units;
            while (us && !cs) {
                if (us->faction == f) {
                    cs = b->type->uvisible(b, us);
                    if (cs)
                        break;
                }
                us = us->next;
            }
        }
    }
    return cs;
}

void report_region(struct stream *out, const region * r, faction * f)
{
    int n;
    bool dh;
    direction_t d;
    int trees;
    int saplings;
    attrib *a;
    const char *tname;
    struct edge {
        struct edge *next;
        char *name;
        bool transparent;
        bool block;
        bool exist[MAXDIRECTIONS];
        direction_t lastd;
    } *edges = NULL, *e;
    bool see[MAXDIRECTIONS];
    char buf[8192];
    char *bufp = buf;
    size_t size = sizeof(buf);
    int bytes;

    assert(out);
    assert(f);
    assert(r);

    for (d = 0; d != MAXDIRECTIONS; d++) {
        /* Nachbarregionen, die gesehen werden, ermitteln */
        region *r2 = rconnect(r, d);
        connection *b;
        see[d] = true;
        if (!r2)
            continue;
        for (b = get_borders(r, r2); b;) {
            struct edge *edg = edges;
            bool transparent = b->type->transparent(b, f);
            const char *name = border_name(b, r, f, GF_DETAILED | GF_ARTICLE);

            if (!transparent) {
                see[d] = false;
            }
            if (!see_border(b, f, r)) {
                b = b->next;
                continue;
            }
            while (edg && (edg->transparent != transparent || strcmp(name, edg->name)!=0)) {
                edg = edg->next;
            }
            if (!edg) {
                edg = calloc(sizeof(struct edge), 1);
                edg->name = str_strdup(name);
                edg->transparent = transparent;
                edg->next = edges;
                edges = edg;
            }
            edg->lastd = d;
            edg->exist[d] = true;
            b = b->next;
        }
    }

    bytes = (int)f_regionid(r, f, bufp, size);
    if (wrptr(&bufp, &size, bytes) != 0)
        WARN_STATIC_BUFFER();

    if (r->seen.mode == seen_travel) {
        bytes = snprintf(bufp, size, " (%s)", LOC(f->locale, "see_travel"));
    }
    else if (r->seen.mode == seen_neighbour) {
        bytes = snprintf(bufp, size, " (%s)", LOC(f->locale, "see_neighbour"));
    }
    else if (r->seen.mode == seen_lighthouse) {
        bytes = snprintf(bufp, size, " (%s)", LOC(f->locale, "see_lighthouse"));
    }
    else {
        bytes = 0;
    }
    if (wrptr(&bufp, &size, bytes) != 0)
        WARN_STATIC_BUFFER();

    /* Terrain */
    bytes = (int)str_strlcpy(bufp, ", ", size);
    if (wrptr(&bufp, &size, bytes) != 0)
        WARN_STATIC_BUFFER();

    tname = terrain_name(r);
    bytes = (int)str_strlcpy(bufp, LOC(f->locale, tname), size);
    if (wrptr(&bufp, &size, bytes) != 0)
        WARN_STATIC_BUFFER();

    /* Trees */
    trees = rtrees(r, 2);
    saplings = rtrees(r, 1);
    if (max_production(r)) {
        if (trees > 0 || saplings > 0) {
            bytes = snprintf(bufp, size, ", %d/%d ", trees, saplings);
            if (wrptr(&bufp, &size, bytes) != 0)
                WARN_STATIC_BUFFER();

            if (fval(r, RF_MALLORN)) {
                if (trees == 1) {
                    bytes = (int)str_strlcpy(bufp, LOC(f->locale, "nr_mallorntree"), size);
                }
                else {
                    bytes = (int)str_strlcpy(bufp, LOC(f->locale, "nr_mallorntree_p"), size);
                }
            }
            else if (trees == 1) {
                bytes = (int)str_strlcpy(bufp, LOC(f->locale, "tree"), size);
            }
            else {
                bytes = (int)str_strlcpy(bufp, LOC(f->locale, "tree_p"), size);
            }
            if (wrptr(&bufp, &size, bytes) != 0)
                WARN_STATIC_BUFFER();
        }
    }

    /* iron & stone */
    if (r->seen.mode >= seen_unit) {
        resource_report result[MAX_RAWMATERIALS];
        int numresults = report_resources(r, result, MAX_RAWMATERIALS, f, true);

        for (n = 0; n < numresults; ++n) {
            if (result[n].number >= 0 && result[n].level >= 0) {
                const char * name = resourcename(result[n].rtype, result[n].number!=1);
                bytes = snprintf(bufp, size, ", %d %s/%d", result[n].number,
                    LOC(f->locale, name), result[n].level);
                if (wrptr(&bufp, &size, bytes) != 0)
                    WARN_STATIC_BUFFER();
            }
        }
    }

    /* peasants & silver */
    if (rpeasants(r)) {
        int p = rpeasants(r);
        bytes = snprintf(bufp, size, ", %d", p);
        if (wrptr(&bufp, &size, bytes) != 0)
            WARN_STATIC_BUFFER();

        if (r->land->ownership) {
            const char *str =
                LOC(f->locale, mkname("morale", itoa10(region_get_morale(r))));
            bytes = snprintf(bufp, size, " %s", str);
            if (wrptr(&bufp, &size, bytes) != 0)
                WARN_STATIC_BUFFER();
        }
        bytes = (int)str_strlcpy(bufp, " ", size);
        if (wrptr(&bufp, &size, bytes) != 0)
            WARN_STATIC_BUFFER();
        bytes =
            (int)str_strlcpy(bufp, LOC(f->locale, p == 1 ? "peasant" : "peasant_p"),
                size);
        if (wrptr(&bufp, &size, bytes) != 0)
            WARN_STATIC_BUFFER();
        if (is_mourning(r, turn + 1)) {
            bytes = (int)str_strlcpy(bufp, LOC(f->locale, "nr_mourning"), size);
            if (wrptr(&bufp, &size, bytes) != 0)
                WARN_STATIC_BUFFER();
        }
    }
    if (rmoney(r) && r->seen.mode >= seen_travel) {
        bytes = snprintf(bufp, size, ", %d ", rmoney(r));
        if (wrptr(&bufp, &size, bytes) != 0)
            WARN_STATIC_BUFFER();
        bytes =
            (int)str_strlcpy(bufp, LOC(f->locale, resourcename(get_resourcetype(R_SILVER),
                rmoney(r) != 1)), size);
        if (wrptr(&bufp, &size, bytes) != 0)
            WARN_STATIC_BUFFER();
    }
    /* Pferde */

    if (rhorses(r)) {
        bytes = snprintf(bufp, size, ", %d ", rhorses(r));
        if (wrptr(&bufp, &size, bytes) != 0)
            WARN_STATIC_BUFFER();
        bytes =
            (int)str_strlcpy(bufp, LOC(f->locale, resourcename(get_resourcetype(R_HORSE),
            (rhorses(r) > 1) ? GR_PLURAL : 0)), size);
        if (wrptr(&bufp, &size, bytes) != 0)
            WARN_STATIC_BUFFER();
    }
    bytes = (int)str_strlcpy(bufp, ".", size);
    if (wrptr(&bufp, &size, bytes) != 0)
        WARN_STATIC_BUFFER();

    if (r->land && r->land->display && r->land->display[0]) {
        bytes = (int)str_strlcpy(bufp, " ", size);
        if (wrptr(&bufp, &size, bytes) != 0)
            WARN_STATIC_BUFFER();
        bytes = (int)str_strlcpy(bufp, r->land->display, size);
        if (wrptr(&bufp, &size, bytes) != 0)
            WARN_STATIC_BUFFER();

        n = r->land->display[strlen(r->land->display) - 1];
        if (n != '!' && n != '?' && n != '.') {
            bytes = (int)str_strlcpy(bufp, ".", size);
            if (wrptr(&bufp, &size, bytes) != 0)
                WARN_STATIC_BUFFER();
        }
    }

    if (rule_region_owners()) {
        const faction *owner = region_get_owner(r);
        message *msg;

        if (owner != NULL) {
            bytes = (int)str_strlcpy(bufp, " ", size);
            if (wrptr(&bufp, &size, bytes) != 0)
                WARN_STATIC_BUFFER();
            msg = msg_message("nr_region_owner", "faction", owner);
            bytes = (int)nr_render(msg, f->locale, bufp, size, f);
            msg_release(msg);
            if (wrptr(&bufp, &size, bytes) != 0)
                WARN_STATIC_BUFFER();
        }
    }
    a = a_find(r->attribs, &at_overrideroads);

    if (a) {
        bytes = (int)str_strlcpy(bufp, " ", size);
        if (wrptr(&bufp, &size, bytes) != 0)
            WARN_STATIC_BUFFER();
        bytes = (int)str_strlcpy(bufp, (char *)a->data.v, size);
        if (wrptr(&bufp, &size, bytes) != 0)
            WARN_STATIC_BUFFER();
    }
    else {
        int nrd = 0;

        /* Nachbarregionen, die gesehen werden, ermitteln */
        for (d = 0; d != MAXDIRECTIONS; d++) {
            if (see[d] && rconnect(r, d))
                nrd++;
        }
        /* list directions */

        dh = false;
        for (d = 0; d != MAXDIRECTIONS; d++) {
            if (see[d]) {
                region *r2 = rconnect(r, d);
                if (!r2)
                    continue;
                nrd--;
                if (dh) {
                    char regname[4096];
                    if (nrd == 0) {
                        bytes = (int)str_strlcpy(bufp, " ", size);
                        if (wrptr(&bufp, &size, bytes) != 0)
                            WARN_STATIC_BUFFER();
                        bytes = (int)str_strlcpy(bufp, LOC(f->locale, "nr_nb_final"), size);
                    }
                    else {
                        bytes = (int)str_strlcpy(bufp, LOC(f->locale, "nr_nb_next"), size);
                    }
                    if (wrptr(&bufp, &size, bytes) != 0)
                        WARN_STATIC_BUFFER();
                    bytes = (int)str_strlcpy(bufp, LOC(f->locale, directions[d]), size);
                    if (wrptr(&bufp, &size, bytes) != 0)
                        WARN_STATIC_BUFFER();
                    bytes = (int)str_strlcpy(bufp, " ", size);
                    if (wrptr(&bufp, &size, bytes) != 0)
                        WARN_STATIC_BUFFER();
                    f_regionid(r2, f, regname, sizeof(regname));
                    bytes = snprintf(bufp, size, trailinto(r2, f->locale), regname);
                    if (wrptr(&bufp, &size, bytes) != 0)
                        WARN_STATIC_BUFFER();
                }
                else {
                    bytes = (int)str_strlcpy(bufp, " ", size);
                    if (wrptr(&bufp, &size, bytes) != 0)
                        WARN_STATIC_BUFFER();
                    MSG(("nr_vicinitystart", "dir region", d, r2), bufp, size, f->locale,
                        f);
                    bufp += strlen(bufp);
                    dh = true;
                }
            }
        }
        /* Spezielle Richtungen */
        for (a = a_find(r->attribs, &at_direction); a && a->type == &at_direction;
            a = a->next) {
            spec_direction *spd = (spec_direction *)(a->data.v);
            bytes = (int)str_strlcpy(bufp, " ", size);
            if (wrptr(&bufp, &size, bytes) != 0)
                WARN_STATIC_BUFFER();
            bytes = (int)str_strlcpy(bufp, LOC(f->locale, spd->desc), size);
            if (wrptr(&bufp, &size, bytes) != 0)
                WARN_STATIC_BUFFER();
            bytes = (int)str_strlcpy(bufp, " (\"", size);
            if (wrptr(&bufp, &size, bytes) != 0)
                WARN_STATIC_BUFFER();
            bytes = (int)str_strlcpy(bufp, LOC(f->locale, spd->keyword), size);
            if (wrptr(&bufp, &size, bytes) != 0)
                WARN_STATIC_BUFFER();
            bytes = (int)str_strlcpy(bufp, "\")", size);
            if (wrptr(&bufp, &size, bytes) != 0)
                WARN_STATIC_BUFFER();
            bytes = (int)str_strlcpy(bufp, ".", size);
            if (wrptr(&bufp, &size, bytes) != 0)
                WARN_STATIC_BUFFER();
            dh = 1;
        }
    }
    *bufp = 0;
    paragraph(out, buf, 0, 0, 0);

    if (r->seen.mode >= seen_unit && is_astral(r) &&
        !is_cursed(r->attribs, &ct_astralblock)) {
        /* Sonderbehandlung Teleport-Ebene */
        region_list *rl = astralregions(r, inhabitable);
        region_list *rl2;

        if (rl) {
            bufp = buf;
            size = sizeof(buf) - 1;

            /* this localization might not work for every language but is fine for de and en */
            bytes = (int)str_strlcpy(bufp, LOC(f->locale, "nr_schemes_prefix"), size);
            if (wrptr(&bufp, &size, bytes) != 0)
                WARN_STATIC_BUFFER();
            rl2 = rl;
            while (rl2) {
                bytes = (int)f_regionid(rl2->data, f, bufp, size);
                if (wrptr(&bufp, &size, bytes) != 0)
                    WARN_STATIC_BUFFER();
                rl2 = rl2->next;
                if (rl2) {
                    bytes = (int)str_strlcpy(bufp, ", ", size);
                    if (wrptr(&bufp, &size, bytes) != 0)
                        WARN_STATIC_BUFFER();
                }
            }
            bytes = (int)str_strlcpy(bufp, LOC(f->locale, "nr_schemes_postfix"), size);
            if (wrptr(&bufp, &size, bytes) != 0)
                WARN_STATIC_BUFFER();
            free_regionlist(rl);
            /* Schreibe Paragraphen */
            newline(out);
            *bufp = 0;
            paragraph(out, buf, 0, 0, 0);
        }
    }

    /* Wirkungen permanenter Sprüche */
    nr_curses(out, 0, f, TYP_REGION, r);
    n = 0;

    if (edges)
        newline(out);
    for (e = edges; e; e = e->next) {
        bool first = true;
        message *msg;

        bufp = buf;
        size = sizeof(buf) - 1;
        for (d = 0; d != MAXDIRECTIONS; ++d) {
            if (!e->exist[d])
                continue;
            /* this localization might not work for every language but is fine for de and en */
            if (first)
                bytes = (int)str_strlcpy(bufp, LOC(f->locale, "nr_borderlist_prefix"), size);
            else if (e->lastd == d)
                bytes = (int)str_strlcpy(bufp, LOC(f->locale, "nr_borderlist_lastfix"), size);
            else
                bytes = (int)str_strlcpy(bufp, LOC(f->locale, "nr_borderlist_infix"), size);
            if (wrptr(&bufp, &size, bytes) != 0)
                WARN_STATIC_BUFFER();
            bytes = (int)str_strlcpy(bufp, LOC(f->locale, directions[d]), size);
            if (wrptr(&bufp, &size, bytes) != 0)
                WARN_STATIC_BUFFER();
            first = false;
        }
        /* TODO name is localized? Works for roads anyway... */
        /* TODO: creating messages during reporting makes them not show up in CR? */
        msg = msg_message("nr_borderlist_postfix", "transparent object",
            e->transparent, e->name);
        bytes = (int)nr_render(msg, f->locale, bufp, size, f);
        msg_release(msg);
        if (wrptr(&bufp, &size, bytes) != 0)
            WARN_STATIC_BUFFER();

        *bufp = 0;
        paragraph(out, buf, 0, 0, 0);
    }
    if (edges) {
        while (edges) {
            e = edges->next;
            free(edges->name);
            free(edges);
            edges = e;
        }
    }
}

static void statistics(struct stream *out, const region * r, const faction * f)
{
    const unit *u;
    int number = 0, p = rpeasants(r);
    message *m;
    item *itm, *items = NULL;
    char buf[4096];

    /* count */
    for (u = r->units; u; u = u->next) {
        if (u->faction == f && !fval(u_race(u), RCF_INVISIBLE)) {
            for (itm = u->items; itm; itm = itm->next) {
                i_change(&items, itm->type, itm->number);
            }
            number += u->number;
        }
    }
    /* print */
    newline(out);
    m = msg_message("nr_stat_header", "region", r);
    nr_render(m, f->locale, buf, sizeof(buf), f);
    msg_release(m);
    paragraph(out, buf, 0, 0, 0);
    newline(out);

    /* Region */
    if (skill_enabled(SK_ENTERTAINMENT) && fval(r->terrain, LAND_REGION)
        && rmoney(r)) {
        m = msg_message("nr_stat_maxentertainment", "max", entertainmoney(r));
        nr_render(m, f->locale, buf, sizeof(buf), f);
        paragraph(out, buf, 2, 2, 0);
        msg_release(m);
    }
    if (max_production(r) && (!fval(r->terrain, SEA_REGION)
        || f->race == get_race(RC_AQUARIAN))) {
        if (markets_module()) {     /* hack */
            m =
                msg_message("nr_stat_salary_new", "max", wage(r, NULL, NULL, turn + 1));
        }
        else {
            m = msg_message("nr_stat_salary", "max", wage(r, f, f->race, turn + 1));
        }
        nr_render(m, f->locale, buf, sizeof(buf), f);
        paragraph(out, buf, 2, 2, 0);
        msg_release(m);
    }

    if (p) {
        m = msg_message("nr_stat_recruits", "max", p / RECRUITFRACTION);
        nr_render(m, f->locale, buf, sizeof(buf), f);
        paragraph(out, buf, 2, 2, 0);
        msg_release(m);

        if (!markets_module()) {
            if (buildingtype_exists(r, bt_find("caravan"), true)) {
                m = msg_message("nr_stat_luxuries", "max", (p * 2) / TRADE_FRACTION);
            }
            else {
                m = msg_message("nr_stat_luxuries", "max", p / TRADE_FRACTION);
            }
            nr_render(m, f->locale, buf, sizeof(buf), f);
            paragraph(out, buf, 2, 2, 0);
            msg_release(m);
        }

        if (r->land->ownership) {
            m = msg_message("nr_stat_morale", "morale", region_get_morale(r));
            nr_render(m, f->locale, buf, sizeof(buf), f);
            paragraph(out, buf, 2, 2, 0);
            msg_release(m);
        }

    }
    /* info about units */

    m = msg_message("nr_stat_people", "max", number);
    nr_render(m, f->locale, buf, sizeof(buf), f);
    paragraph(out, buf, 2, 2, 0);
    msg_release(m);

    for (itm = items; itm; itm = itm->next) {
        sprintf(buf, "%s: %d",
            LOC(f->locale, resourcename(itm->type->rtype, GR_PLURAL)), itm->number);
        paragraph(out, buf, 2, 2, 0);
    }
    while (items)
        i_free(i_remove(&items, items));
}


static int buildingmaintenance(const building * b, const resource_type * rtype)
{
    const building_type *bt = b->type;
    int c, cost = 0;

    for (c = 0; bt->maintenance && bt->maintenance[c].number; ++c) {
        const maintenance *m = bt->maintenance + c;
        if (m->rtype == rtype) {
            if (fval(m, MTF_VARIABLE))
                cost += (b->size * m->number);
            else
                cost += m->number;
        }
    }
    return cost;
}

static int
report_template(const char *filename, report_context * ctx, const char *bom)
{
    const resource_type *rsilver = get_resourcetype(R_SILVER);
    const faction *f = ctx->f;
    const struct locale *lang = f->locale;
    region *r;
    FILE *F = fopen(filename, "w");
    stream strm = { 0 }, *out = &strm;
    char buf[8192], *bufp;
    size_t size;
    int bytes;

    if (F == NULL) {
        perror(filename);
        return -1;
    }
    fstream_init(&strm, F);

    if (bom) {
        swrite(bom, 1, strlen(bom), out);
    }

    newline(out);
    rps_nowrap(out, LOC(lang, "nr_template"));
    newline(out);
    newline(out);

    sprintf(buf, "%s %s \"password\"", LOC(lang, parameters[P_FACTION]), itoa36(f->no));
    rps_nowrap(out, buf);
    newline(out);
    newline(out);
    sprintf(buf, "; ECHECK -l -w4 -r%d -v%s", f->race->recruitcost,
        ECHECK_VERSION);
    rps_nowrap(out, buf);
    newline(out);

    for (r = ctx->first; r != ctx->last; r = r->next) {
        unit *u;
        int dh = 0;

        if (r->seen.mode < seen_unit)
            continue;

        for (u = r->units; u; u = u->next) {
            if (u->faction == f && !fval(u_race(u), RCF_INVISIBLE)) {
                order *ord;
                if (!dh) {
                    plane *pl = getplane(r);
                    int nx = r->x, ny = r->y;

                    pnormalize(&nx, &ny, pl);
                    adjust_coordinates(f, &nx, &ny, pl);
                    newline(out);
                    if (pl && pl->id != 0) {
                        sprintf(buf, "%s %d,%d,%d ; %s", LOC(lang,
                            parameters[P_REGION]), nx, ny, pl->id, rname(r, lang));
                    }
                    else {
                        sprintf(buf, "%s %d,%d ; %s", LOC(lang, parameters[P_REGION]),
                            nx, ny, rname(r, lang));
                    }
                    rps_nowrap(out, buf);
                    newline(out);
                    sprintf(buf, "; ECheck Lohn %d", wage(r, f, f->race, turn + 1));
                    rps_nowrap(out, buf);
                    newline(out);
                    newline(out);
                }
                dh = 1;

                bufp = buf;
                size = sizeof(buf) - 1;
                bytes = snprintf(bufp, size, "%s %s;    %s [%d,%d$",
                    LOC(u->faction->locale, parameters[P_UNIT]),
                    itoa36(u->no), unit_getname(u), u->number, get_money(u));
                if (wrptr(&bufp, &size, bytes) != 0)
                    WARN_STATIC_BUFFER();
                if (u->building && building_owner(u->building) == u) {
                    building *b = u->building;
                    if (!curse_active(get_curse(b->attribs, &ct_nocostbuilding))) {
                        int cost = buildingmaintenance(b, rsilver);
                        if (cost > 0) {
                            bytes = (int)str_strlcpy(bufp, ",U", size);
                            if (wrptr(&bufp, &size, bytes) != 0)
                                WARN_STATIC_BUFFER();
                            bytes = (int)str_strlcpy(bufp, itoa10(cost), size);
                            if (wrptr(&bufp, &size, bytes) != 0)
                                WARN_STATIC_BUFFER();
                        }
                    }
                }
                else if (u->ship) {
                    if (ship_owner(u->ship) == u) {
                        bytes = (int)str_strlcpy(bufp, ",S", size);
                    }
                    else {
                        bytes = (int)str_strlcpy(bufp, ",s", size);
                    }
                    if (wrptr(&bufp, &size, bytes) != 0)
                        WARN_STATIC_BUFFER();
                    bytes = (int)str_strlcpy(bufp, itoa36(u->ship->no), size);
                    if (wrptr(&bufp, &size, bytes) != 0)
                        WARN_STATIC_BUFFER();
                }
                if (lifestyle(u) == 0) {
                    bytes = (int)str_strlcpy(bufp, ",I", size);
                    if (wrptr(&bufp, &size, bytes) != 0)
                        WARN_STATIC_BUFFER();
                }
                bytes = (int)str_strlcpy(bufp, "]", size);
                if (wrptr(&bufp, &size, bytes) != 0)
                    WARN_STATIC_BUFFER();

                *bufp = 0;
                rps_nowrap(out, buf);
                newline(out);

                for (ord = u->old_orders; ord; ord = ord->next) {
                    /* this new order will replace the old defaults */
                    strcpy(buf, "   ");
                    write_order(ord, lang, buf + 2, sizeof(buf) - 2);
                    rps_nowrap(out, buf);
                    newline(out);
                }
                for (ord = u->orders; ord; ord = ord->next) {
                    keyword_t kwd = getkeyword(ord);
                    if (u->old_orders && is_repeated(kwd))
                        continue;           /* unit has defaults */
                    if (is_persistent(ord)) {
                        strcpy(buf, "   ");
                        write_order(ord, lang, buf + 2, sizeof(buf) - 2);
                        rps_nowrap(out, buf);
                        newline(out);
                    }
                }

                /* If the lastorder begins with an @ it should have
                 * been printed in the loop before. */
            }
        }
    }
    newline(out);
    str_strlcpy(buf, LOC(lang, parameters[P_NEXT]), sizeof(buf));
    rps_nowrap(out, buf);
    newline(out);
    fstream_done(&strm);
    return 0;
}

static void
show_allies(const faction * f, const ally * allies, char *buf, size_t size)
{
    int allierte = 0;
    int i = 0, h, hh = 0;
    int bytes, dh = 0;
    const ally *sf;
    char *bufp = buf;             /* buf already contains data */

    --size;                       /* leave room for a null-terminator */

    for (sf = allies; sf; sf = sf->next) {
        int mode = alliedgroup(NULL, f, sf->faction, sf, HELP_ALL);
        if (mode > 0)
            ++allierte;
    }

    for (sf = allies; sf; sf = sf->next) {
        int mode = alliedgroup(NULL, f, sf->faction, sf, HELP_ALL);
        if (mode <= 0)
            continue;
        i++;
        if (dh) {
            if (i == allierte) {
                bytes = (int)str_strlcpy(bufp, LOC(f->locale, "list_and"), size);
            }
            else {
                bytes = (int)str_strlcpy(bufp, ", ", size);
            }
            if (wrptr(&bufp, &size, bytes) != 0)
                WARN_STATIC_BUFFER();
        }
        dh = 1;
        hh = 0;
        bytes = (int)str_strlcpy(bufp, factionname(sf->faction), size);
        if (wrptr(&bufp, &size, bytes) != 0)
            WARN_STATIC_BUFFER();
        bytes = (int)str_strlcpy(bufp, " (", size);
        if (wrptr(&bufp, &size, bytes) != 0)
            WARN_STATIC_BUFFER();
        if ((mode & HELP_ALL) == HELP_ALL) {
            bytes = (int)str_strlcpy(bufp, LOC(f->locale, parameters[P_ANY]), size);
            if (wrptr(&bufp, &size, bytes) != 0)
                WARN_STATIC_BUFFER();
        }
        else {
            for (h = 1; h <= HELP_TRAVEL; h *= 2) {
                int p = MAXPARAMS;
                if ((mode & h) == h) {
                    switch (h) {
                    case HELP_TRAVEL:
                        p = P_TRAVEL;
                        break;
                    case HELP_MONEY:
                        p = P_MONEY;
                        break;
                    case HELP_FIGHT:
                        p = P_FIGHT;
                        break;
                    case HELP_GIVE:
                        p = P_GIVE;
                        break;
                    case HELP_GUARD:
                        p = P_GUARD;
                        break;
                    case HELP_FSTEALTH:
                        p = P_FACTIONSTEALTH;
                        break;
                    }
                }
                if (p != MAXPARAMS) {
                    if (hh) {
                        bytes = (int)str_strlcpy(bufp, ", ", size);
                        if (wrptr(&bufp, &size, bytes) != 0)
                            WARN_STATIC_BUFFER();
                    }
                    bytes = (int)str_strlcpy(bufp, LOC(f->locale, parameters[p]), size);
                    if (wrptr(&bufp, &size, bytes) != 0)
                        WARN_STATIC_BUFFER();
                    hh = 1;
                }
            }
        }
        bytes = (int)str_strlcpy(bufp, ")", size);
        if (wrptr(&bufp, &size, bytes) != 0)
            WARN_STATIC_BUFFER();
    }
    bytes = (int)str_strlcpy(bufp, ".", size);
    if (wrptr(&bufp, &size, bytes) != 0)
        WARN_STATIC_BUFFER();
    *bufp = 0;
}

static void allies(struct stream *out, const faction * f)
{
    const group *g = f->groups;
    char buf[16384];

    if (f->allies) {
        int bytes;
        size_t size = sizeof(buf);
        bytes = snprintf(buf, size, "%s ", LOC(f->locale, "faction_help"));
        size -= bytes;
        show_allies(f, f->allies, buf + bytes, size);
        paragraph(out, buf, 0, 0, 0);
        newline(out);
    }

    while (g) {
        if (g->allies) {
            int bytes;
            size_t size = sizeof(buf);
            bytes = snprintf(buf, size, "%s %s ", g->name, LOC(f->locale, "group_help"));
            size -= bytes;
            show_allies(f, g->allies, buf + bytes, size);
            paragraph(out, buf, 0, 0, 0);
            newline(out);
        }
        g = g->next;
    }
}

static void guards(struct stream *out, const region * r, const faction * see)
{
    /* die Partei  see  sieht dies; wegen
     * "unbekannte Partei", wenn man es selbst ist... */
    faction *guardians[512];
    int nextguard = 0;
    unit *u;
    int i;
    bool tarned = false;
    /* Bewachung */

    for (u = r->units; u; u = u->next) {
        if (is_guard(u) != 0) {
            faction *f = u->faction;
            faction *fv = visible_faction(see, u);

            if (fv != f && see != fv) {
                f = fv;
            }

            if (f != see && fval(u, UFL_ANON_FACTION)) {
                tarned = true;
            }
            else {
                for (i = 0; i != nextguard; ++i)
                    if (guardians[i] == f)
                        break;
                if (i == nextguard) {
                    guardians[nextguard++] = f;
                }
            }
        }
    }

    if (nextguard || tarned) {
        char buf[8192];
        char *bufp = buf;
        size_t size = sizeof(buf) - 1;
        int bytes;

        bytes = (int)str_strlcpy(bufp, LOC(see->locale, "nr_guarding_prefix"), size);
        if (wrptr(&bufp, &size, bytes) != 0)
            WARN_STATIC_BUFFER();

        for (i = 0; i != nextguard + (tarned ? 1 : 0); ++i) {
            if (i != 0) {
                if (i == nextguard - (tarned ? 0 : 1)) {
                    bytes = (int)str_strlcpy(bufp, LOC(see->locale, "list_and"), size);
                }
                else {
                    bytes = (int)str_strlcpy(bufp, ", ", size);
                }
                if (wrptr(&bufp, &size, bytes) != 0)
                    WARN_STATIC_BUFFER();
            }
            if (i < nextguard) {
                bytes = (int)str_strlcpy(bufp, factionname(guardians[i]), size);
            }
            else {
                bytes = (int)str_strlcpy(bufp, LOC(see->locale, "nr_guarding_unknown"), size);
            }
            if (wrptr(&bufp, &size, bytes) != 0)
                WARN_STATIC_BUFFER();
        }
        bytes = (int)str_strlcpy(bufp, LOC(see->locale, "nr_guarding_postfix"), size);
        if (wrptr(&bufp, &size, bytes) != 0)
            WARN_STATIC_BUFFER();
        newline(out);
        *bufp = 0;
        paragraph(out, buf, 0, 0, 0);
    }
}

static void rpline(struct stream *out)
{
    static char line[REPORTWIDTH + 1];
    if (line[0] != '-') {
        memset(line, '-', sizeof(line));
        line[REPORTWIDTH] = '\n';
    }
    swrite(line, sizeof(line), 1, out);
}

static void list_address(struct stream *out, const faction * uf, selist * seenfactions)
{
    int qi = 0;
    selist *flist = seenfactions;

    centre(out, LOC(uf->locale, "nr_addresses"), false);
    newline(out);

    while (flist != NULL) {
        const faction *f = (const faction *)selist_get(flist, qi);
        if (!is_monsters(f)) {
            char buf[8192];
            char label = '-';

            sprintf(buf, "%s: %s; %s", factionname(f), faction_getemail(f),
                f->banner ? f->banner : "");
            if (uf == f)
                label = '*';
            else if (is_allied(uf, f))
                label = 'o';
            else if (alliedfaction(NULL, uf, f, HELP_ALL))
                label = '+';
            paragraph(out, buf, 4, 0, label);
        }
        selist_advance(&flist, &qi, 1);
    }
    newline(out);
    rpline(out);
}

static void
nr_ship(struct stream *out, const region *r, const ship * sh, const faction * f,
    const unit * captain)
{
    char buffer[8192], *bufp = buffer;
    size_t size = sizeof(buffer) - 1;
    int bytes;
    char ch;

    newline(out);

    if (captain && captain->faction == f) {
        int n = 0, p = 0;
        getshipweight(sh, &n, &p);
        n = (n + 99) / 100;         /* 1 Silber = 1 GE */

        bytes = snprintf(bufp, size, "%s, %s, (%d/%d)", shipname(sh),
            LOC(f->locale, sh->type->_name), n, shipcapacity(sh) / 100);
    }
    else {
        bytes =
            snprintf(bufp, size, "%s, %s", shipname(sh), LOC(f->locale,
                sh->type->_name));
    }
    if (wrptr(&bufp, &size, bytes) != 0)
        WARN_STATIC_BUFFER();

    assert(sh->type->construction->improvement == NULL);  /* sonst ist construction::size nicht ship_type::maxsize */
    if (sh->size != sh->type->construction->maxsize) {
        bytes = snprintf(bufp, size, ", %s (%d/%d)",
            LOC(f->locale, "nr_undercons"), sh->size,
            sh->type->construction->maxsize);
        if (wrptr(&bufp, &size, bytes) != 0)
            WARN_STATIC_BUFFER();
    }
    if (sh->damage) {
        int percent = ship_damage_percent(sh);
        bytes =
            snprintf(bufp, size, ", %d%% %s", percent, LOC(f->locale, "nr_damaged"));
        if (wrptr(&bufp, &size, bytes) != 0)
            WARN_STATIC_BUFFER();
    }
    if (!fval(r->terrain, SEA_REGION)) {
        if (sh->coast != NODIRECTION) {
            bytes = (int)str_strlcpy(bufp, ", ", size);
            if (wrptr(&bufp, &size, bytes) != 0)
                WARN_STATIC_BUFFER();
            bytes = (int)str_strlcpy(bufp, LOC(f->locale, coasts[sh->coast]), size);
            if (wrptr(&bufp, &size, bytes) != 0)
                WARN_STATIC_BUFFER();
        }
    }
    ch = 0;
    if (sh->display && sh->display[0]) {
        bytes = (int)str_strlcpy(bufp, "; ", size);
        if (wrptr(&bufp, &size, bytes) != 0)
            WARN_STATIC_BUFFER();
        bytes = (int)str_strlcpy(bufp, sh->display, size);
        if (wrptr(&bufp, &size, bytes) != 0)
            WARN_STATIC_BUFFER();
        ch = sh->display[strlen(sh->display) - 1];
    }
    if (ch != '!' && ch != '?' && ch != '.') {
        bytes = (int)str_strlcpy(bufp, ".", size);
        if (wrptr(&bufp, &size, bytes) != 0)
            WARN_STATIC_BUFFER();
    }
    *bufp = 0;
    paragraph(out, buffer, 2, 0, 0);

    nr_curses(out, 4, f, TYP_SHIP, sh);
}

static void
nr_building(struct stream *out, const region *r, const building *b, const faction *f)
{
    int i, bytes;
    const char *name, *bname, *billusion = NULL;
    const struct locale *lang;
    char buffer[8192], *bufp = buffer;
    message *msg;
    size_t size = sizeof(buffer) - 1;

    assert(f);
    lang = f->locale;
    newline(out);
    bytes =
        snprintf(bufp, size, "%s, %s %d, ", buildingname(b), LOC(lang,
            "nr_size"), b->size);
    if (wrptr(&bufp, &size, bytes) != 0)
        WARN_STATIC_BUFFER();

    report_building(b, &bname, &billusion);
    name = LOC(lang, billusion ? billusion : bname);
    bytes = (int)str_strlcpy(bufp, name, size);
    if (wrptr(&bufp, &size, bytes) != 0)
        WARN_STATIC_BUFFER();
    if (billusion) {
        unit *owner = building_owner(b);
        if (owner && owner->faction == f) {
            /* illusion. report real type */
            name = LOC(lang, bname);
            bytes = snprintf(bufp, size, " (%s)", name);
            if (wrptr(&bufp, &size, bytes) != 0)
                WARN_STATIC_BUFFER();
        }
    }

    if (!building_finished(b)) {
        bytes = (int)str_strlcpy(bufp, LOC(lang, "nr_building_inprogress"), size);
        if (wrptr(&bufp, &size, bytes) != 0)
            WARN_STATIC_BUFFER();
    }

    if (b->besieged > 0 && r->seen.mode >= seen_lighthouse) {
        msg = msg_message("nr_building_besieged", "soldiers diff", b->besieged,
            b->besieged - b->size * SIEGEFACTOR);
        bytes = (int)nr_render(msg, lang, bufp, size, f);
        if (wrptr(&bufp, &size, bytes) != 0)
            WARN_STATIC_BUFFER();
        msg_release(msg);
    }
    i = 0;
    if (b->display && b->display[0]) {
        bytes = (int)str_strlcpy(bufp, "; ", size);
        if (wrptr(&bufp, &size, bytes) != 0)
            WARN_STATIC_BUFFER();
        bytes = (int)str_strlcpy(bufp, b->display, size);
        if (wrptr(&bufp, &size, bytes) != 0)
            WARN_STATIC_BUFFER();
        i = b->display[strlen(b->display) - 1];
    }

    if (i != '!' && i != '?' && i != '.') {
        bytes = (int)str_strlcpy(bufp, ".", size);
        if (wrptr(&bufp, &size, bytes) != 0)
            WARN_STATIC_BUFFER();
    }
    *bufp = 0;
    paragraph(out, buffer, 2, 0, 0);

    if (r->seen.mode >= seen_lighthouse) {
        nr_curses(out, 4, f, TYP_BUILDING, b);
    }
}

static void nr_paragraph(struct stream *out, message * m, faction * f)
{
    int bytes;
    char buf[4096], *bufp = buf;
    size_t size = sizeof(buf) - 1;

    assert(f);
    bytes = (int)nr_render(m, f->locale, bufp, size, f);
    if (wrptr(&bufp, &size, bytes) != 0)
        WARN_STATIC_BUFFER();
    msg_release(m);

    paragraph(out, buf, 0, 0, 0);
}

typedef struct cb_data {
    struct stream *out;
    char *start, *writep;
    size_t size;
    const faction *f;
    int maxtravel, counter;
} cb_data;

static void init_cb(cb_data *data, struct stream *out, char *buffer, size_t size, const faction *f) {
    data->out = out;
    data->writep = buffer;
    data->start = buffer;
    data->size = size;
    data->f = f;
    data->maxtravel = 0;
    data->counter = 0;
}

static void cb_write_travelthru(region *r, unit *u, void *cbdata) {
    cb_data *data = (cb_data *)cbdata;
    const faction *f = data->f;

    if (data->counter >= data->maxtravel) {
        return;
    }
    if (travelthru_cansee(r, f, u)) {
        ++data->counter;
        do {
            size_t len, size = data->size - (data->writep - data->start);
            const char *str;
            char *writep = data->writep;

            if (u->ship != NULL) {
                str = shipname(u->ship);
            }
            else {
                str = unitname(u);
            }
            len = strlen(str);
            if (len < size && data->counter <= data->maxtravel) {
                memcpy(writep, str, len);
                writep += len;
                size -= len;
                if (data->counter == data->maxtravel) {
                    str = ".";
                }
                else if (data->counter + 1 == data->maxtravel) {
                    str = LOC(f->locale, "list_and");
                }
                else {
                    str = ", ";
                }
                len = strlen(str);
                if (len < size) {
                    memcpy(writep, str, len);
                    writep += len;
                    size -= len;
                    data->writep = writep;
                }
            }
            if (len >= size || data->counter == data->maxtravel) {
                /* buffer is full */
                *writep = 0;
                paragraph(data->out, data->start, 0, 0, 0);
                data->writep = data->start;
                if (data->counter == data->maxtravel) {
                    break;
                }
            }
        } while (data->writep == data->start);
    }
}

void report_travelthru(struct stream *out, region *r, const faction *f)
{
    int maxtravel;
    char buf[8192];

    assert(r);
    assert(f);
    if (!fval(r, RF_TRAVELUNIT)) {
        return;
    }

    /* How many are we listing? For grammar. */
    maxtravel = count_travelthru(r, f);
    if (maxtravel > 0) {
        cb_data cbdata;

        init_cb(&cbdata, out, buf, sizeof(buf), f);
        cbdata.maxtravel = maxtravel;
        cbdata.writep +=
            str_strlcpy(buf, LOC(f->locale, "travelthru_header"), sizeof(buf));
        travelthru_map(r, cb_write_travelthru, &cbdata);
        return;
    }
}

int
report_plaintext(const char *filename, report_context * ctx,
    const char *bom)
{
    int flag = 0;
    char ch;
    int anyunits, no_units, no_people;
    region *r;
    faction *f = ctx->f;
    unit *u;
    char pzTime[64];
    attrib *a;
    message *m;
    unsigned char op;
    int maxh, bytes, ix = want(O_STATISTICS);
    int wants_stats = (f->options & ix);
    FILE *F = fopen(filename, "w");
    stream strm = { 0 }, *out = &strm;
    char buf[8192];
    char *bufp;
    size_t size;

    if (F == NULL) {
        perror(filename);
        return -1;
    }
    fstream_init(&strm, F);

    if (bom) {
        fwrite(bom, 1, strlen(bom), F);
    }

    strftime(pzTime, 64, "%A, %d. %B %Y, %H:%M", localtime(&ctx->report_time));
    m = msg_message("nr_header_date", "game date", game_name(), pzTime);
    nr_render(m, f->locale, buf, sizeof(buf), f);
    msg_release(m);
    centre(out, buf, true);

    centre(out, gamedate_season(f->locale), true);
    newline(out);
    sprintf(buf, "%s, %s/%s (%s)", factionname(f),
        LOC(f->locale, rc_name_s(f->race, NAME_PLURAL)),
        LOC(f->locale, mkname("school", magic_school[f->magiegebiet])), faction_getemail(f));
    centre(out, buf, true);
    if (f_get_alliance(f)) {
        centre(out, alliancename(f->alliance), true);
    }

    if (f->age <= 2) {
        const char *email;
        const char *subject;
        email = config_get("game.email");
        if (!email)
          log_error("game.email not set");
        else {
            subject = get_mailcmd(f->locale);

            m = msg_message("newbie_info_game", "email subject", email, subject);
            if (m) {
                nr_render(m, f->locale, buf, sizeof(buf), f);
                msg_release(m);
                centre(out, buf, true);
            }
        }
        if ((f->options & want(O_COMPUTER)) == 0) {
            const char *s;
            s = locale_getstring(f->locale, "newbie_info_cr");
            if (s) {
                newline(out);
                centre(out, s, true);
            }
            f->options |= want(O_COMPUTER);
        }
    }
    newline(out);
    if (f->options & want(O_SCORE) && f->age > DISPLAYSCORE) {
        char score[32], avg[32];
        write_score(score, sizeof(score), f->score);
        write_score(avg, sizeof(avg), average_score_of_age(f->age, f->age / 24 + 1));
        RENDER(f, buf, sizeof(buf), ("nr_score", "score average", score, avg));
        centre(out, buf, true);
    }
    no_units = f->num_units;
    no_people = f->num_people;
    m = msg_message("nr_population", "population units limit", no_people, no_units, rule_faction_limit());
    nr_render(m, f->locale, buf, sizeof(buf), f);
    msg_release(m);
    centre(out, buf, true);
    if (f->race == get_race(RC_HUMAN)) {
        int maxmig = count_maxmigrants(f);
        if (maxmig > 0) {
            m =
                msg_message("nr_migrants", "units maxunits", count_migrants(f), maxmig);
            nr_render(m, f->locale, buf, sizeof(buf), f);
            msg_release(m);
            centre(out, buf, true);
        }
    }
    if (f_get_alliance(f)) {
        m =
            msg_message("nr_alliance", "leader name id age",
                alliance_get_leader(f->alliance), f->alliance->name, f->alliance->id,
                turn - f->alliance_joindate);
        nr_render(m, f->locale, buf, sizeof(buf), f);
        msg_release(m);
        centre(out, buf, true);
    }
    maxh = maxheroes(f);
    if (maxh) {
        message *msg =
            msg_message("nr_heroes", "units maxunits", countheroes(f), maxh);
        nr_render(msg, f->locale, buf, sizeof(buf), f);
        msg_release(msg);
        centre(out, buf, true);
    }

    if (f->items != NULL) {
        message *msg = msg_message("nr_claims", "items", f->items);
        nr_render(msg, f->locale, buf, sizeof(buf), f);
        msg_release(msg);
        newline(out);
        centre(out, buf, true);
    }

    bufp = buf;
    size = sizeof(buf) - 1;
    bytes = snprintf(buf, size, "%s:", LOC(f->locale, "nr_options"));
    if (wrptr(&bufp, &size, bytes) != 0)
        WARN_STATIC_BUFFER();
    for (op = 0; op != MAXOPTIONS; op++) {
        if (f->options & want(op) && options[op]) {
            bytes = (int)str_strlcpy(bufp, " ", size);
            if (wrptr(&bufp, &size, bytes) != 0)
                WARN_STATIC_BUFFER();
            bytes = (int)str_strlcpy(bufp, LOC(f->locale, options[op]), size);
            if (wrptr(&bufp, &size, bytes) != 0)
                WARN_STATIC_BUFFER();

            flag++;
        }
    }
    if (flag > 0) {
        newline(out);
        *bufp = 0;
        centre(out, buf, true);
    }

    rp_messages(out, f->msgs, f, 0, true);
    rp_battles(out, f);
    a = a_find(f->attribs, &at_reportspell);
    if (a) {
        newline(out);
        centre(out, LOC(f->locale, "section_newspells"), true);
        while (a && a->type == &at_reportspell) {
            spellbook_entry *sbe = (spellbook_entry *)a->data.v;
            nr_spell(out, sbe, f->locale);
            a = a->next;
        }
    }

    ch = 0;
    CHECK_ERRNO();
    for (a = a_find(f->attribs, &at_showitem); a && a->type == &at_showitem;
        a = a->next) {
        const item_type *itype = (const item_type *)a->data.v;
        const char *description = NULL;
        if (itype) {
            const char *pname = resourcename(itype->rtype, 0);

            if (ch == 0) {
                newline(out);
                centre(out, LOC(f->locale, "section_newpotions"), true);
                ch = 1;
            }

            newline(out);
            centre(out, LOC(f->locale, pname), true);
            snprintf(buf, sizeof(buf), "%s %d", LOC(f->locale, "nr_level"),
                potion_level(itype));
            centre(out, buf, true);
            newline(out);

            bufp = buf;
            size = sizeof(buf) - 1;
            bytes = snprintf(bufp, size, "%s: ", LOC(f->locale, "nr_herbsrequired"));
            if (wrptr(&bufp, &size, bytes) != 0)
                WARN_STATIC_BUFFER();

            if (itype->construction) {
                requirement *rm = itype->construction->materials;
                while (rm->number) {
                    bytes =
                        (int)str_strlcpy(bufp, LOC(f->locale, resourcename(rm->rtype, 0)), size);
                    if (wrptr(&bufp, &size, bytes) != 0)
                        WARN_STATIC_BUFFER();
                    ++rm;
                    if (rm->number)
                        bytes = (int)str_strlcpy(bufp, ", ", size);
                    if (wrptr(&bufp, &size, bytes) != 0)
                        WARN_STATIC_BUFFER();
                }
            }
            *bufp = 0;
            centre(out, buf, true);
            newline(out);
            description = mkname("potion", pname);
            description = LOC(f->locale, description);
            centre(out, description, true);
        }
    }
    newline(out);
    CHECK_ERRNO();
    centre(out, LOC(f->locale, "nr_alliances"), false);
    newline(out);

    allies(out, f);

    rpline(out);

    CHECK_ERRNO();
    anyunits = 0;

    for (r = ctx->first; r != ctx->last; r = r->next) {
        int stealthmod = stealth_modifier(r, f, r->seen.mode);
        building *b = r->buildings;
        ship *sh = r->ships;

        if (r->seen.mode < seen_lighthouse)
            continue;
        /* Beschreibung */

        if (r->seen.mode >= seen_unit) {
            anyunits = 1;
            newline(out);
            report_region(out, r, f);
            if (markets_module() && r->land) {
                const item_type *lux = r_luxury(r);
                const item_type *herb = r->land->herbtype;

                m = NULL;
                if (herb && lux) {
                    m = msg_message("nr_market_info_p", "p1 p2",
                        lux->rtype, herb->rtype);
                }
                else if (lux || herb) {
                    m = msg_message("nr_market_info_s", "p1",
                        lux ? lux->rtype : herb->rtype);
                }
                if (m) {
                    newline(out);
                    nr_paragraph(out, m, f);
                }
            }
            else {
                if (!fval(r->terrain, SEA_REGION) && rpeasants(r) / TRADE_FRACTION > 0) {
                    newline(out);
                    prices(out, r, f);
                }
            }
            guards(out, r, f);
            newline(out);
            report_travelthru(out, r, f);
        }
        else {
            report_region(out, r, f);
            newline(out);
            report_travelthru(out, r, f);
        }

        if (wants_stats && r->seen.mode >= seen_unit)
            statistics(out, r, f);

        /* Nachrichten an REGION in der Region */
        if (r->seen.mode >= seen_travel) {
            message_list *mlist = r_getmessages(r, f);
            if (mlist) {
                struct mlist **split = merge_messages(mlist, r->msgs);
                rp_messages(out, mlist, f, 0, true);
                split_messages(mlist, split);
            }
            else {
                rp_messages(out, r->msgs, f, 0, true);
            }
        }

        /* report all units. they are pre-sorted in an efficient manner */
        u = r->units;
        while (b) {
            while (b && (!u || u->building != b)) {
                nr_building(out, r, b, f);
                b = b->next;
            }
            if (b) {
                nr_building(out, r, b, f);
                while (u && u->building == b) {
                    nr_unit(out, f, u, 6, r->seen.mode);
                    u = u->next;
                }
                b = b->next;
            }
        }
        while (u && !u->ship) {
            if (visible_unit(u, f, stealthmod, r->seen.mode)) {
                nr_unit(out, f, u, 4, r->seen.mode);
            }
            assert(!u->building);
            u = u->next;
        }
        while (sh) {
            while (sh && (!u || u->ship != sh)) {
                nr_ship(out, r, sh, f, NULL);
                sh = sh->next;
            }
            if (sh) {
                nr_ship(out, r, sh, f, u);
                while (u && u->ship == sh) {
                    nr_unit(out, f, u, 6, r->seen.mode);
                    u = u->next;
                }
                sh = sh->next;
            }
        }

        assert(!u);

        newline(out);
        rpline(out);
        CHECK_ERRNO();
    }
    if (!is_monsters(f)) {
        if (!anyunits) {
            newline(out);
            paragraph(out, LOC(f->locale, "nr_youaredead"), 0, 2, 0);
        }
        else {
            list_address(out, f, ctx->addresses);
        }
    }
    fstream_done(&strm);
    CHECK_ERRNO();
    return 0;
}

#define FMAXHASH 1021

struct fsee {
    struct fsee *nexthash;
    faction *f;
    struct see {
        struct see *next;
        faction *seen;
        unit *proof;
    } *see;
} *fsee[FMAXHASH];

#define REPORT_NR (1 << O_REPORT)
#define REPORT_CR (1 << O_COMPUTER)
#define REPORT_ZV (1 << O_ZUGVORLAGE)
#define REPORT_ZIP (1 << O_COMPRESS)
#define REPORT_BZIP2 (1 << O_BZIP2)

unit *can_find(faction * f, faction * f2)
{
    int key = f->no % FMAXHASH;
    struct fsee *fs = fsee[key];
    struct see *ss;
    if (f == f2)
        return f->units;
    while (fs && fs->f != f)
        fs = fs->nexthash;
    if (!fs)
        return NULL;
    ss = fs->see;
    while (ss && ss->seen != f2)
        ss = ss->next;
    if (ss) {
        /* bei TARNE PARTEI yxz muss die Partei von unit proof nicht
         * wirklich Partei f2 sein! */
         /* assert(ss->proof->faction==f2); */
        return ss->proof;
    }
    return NULL;
}

void register_nr(void)
{
    if (!nocr)
        register_reporttype("nr", &report_plaintext, 1 << O_REPORT);
    if (!nonr)
        register_reporttype("txt", &report_template, 1 << O_ZUGVORLAGE);
}

void report_cleanup(void)
{
    int i;
    for (i = 0; i != FMAXHASH; ++i) {
        while (fsee[i]) {
            struct fsee *fs = fsee[i]->nexthash;
            free(fsee[i]);
            fsee[i] = fs;
        }
    }
}
