#include <platform.h>
#include <config.h>
#include "report.h"
#include "move.h"
#include "travelthru.h"
#include "keyword.h"

#include <kernel/building.h>
#include <kernel/faction.h>
#include <kernel/item.h>
#include <kernel/race.h>
#include <kernel/region.h>
#include <kernel/ship.h>
#include <kernel/unit.h>
#include <kernel/spell.h>
#include <kernel/spellbook.h>

#include <util/language.h>
#include <util/lists.h>
#include <util/message.h>

#include <quicklist.h>
#include <stream.h>
#include <memstream.h>

#include <CuTest.h>
#include <tests.h>

#include <string.h>

static void test_write_spaces(CuTest *tc) {
    stream out = { 0 };
    char buf[1024];
    size_t len;

    mstream_init(&out);
    write_spaces(&out, 4);
    out.api->rewind(out.handle);
    len = out.api->read(out.handle, buf, sizeof(buf));
    buf[len] = '\0';
    CuAssertStrEquals(tc, "    ", buf);
    CuAssertIntEquals(tc, ' ', buf[3]);
    mstream_done(&out);
}

static void test_write_many_spaces(CuTest *tc) {
    stream out = { 0 };
    char buf[1024];
    size_t len;

    mstream_init(&out);
    write_spaces(&out, 100);
    out.api->rewind(out.handle);
    len = out.api->read(out.handle, buf, sizeof(buf));
    buf[len] = '\0';
    CuAssertIntEquals(tc, 100, (int)len);
    CuAssertIntEquals(tc, ' ', buf[99]);
    mstream_done(&out);
}

static void test_write_travelthru(CuTest *tc) {
    stream out = { 0 };
    char buf[1024];
    size_t len;
    region *r;
    faction *f;
    unit *u;
    struct locale *lang;

    test_cleanup();
    lang = get_or_create_locale("de");
    locale_setstring(lang, "travelthru_header", "Durchreise: ");
    mstream_init(&out);
    r = test_create_region(0, 0, 0);
    r->flags |= RF_TRAVELUNIT;
    f = test_create_faction(0);
    f->locale = lang;
    u = test_create_unit(f, test_create_region(0, 1, 0));
    unit_setname(u, "Hodor");
    unit_setid(u, 1);

    write_travelthru(&out, r, f);
    out.api->rewind(out.handle);
    len = out.api->read(out.handle, buf, sizeof(buf));
    CuAssertIntEquals_Msg(tc, "no travelers, no report", 0, (int)len);
    mstream_done(&out);

    mstream_init(&out);
    travelthru_add(r, u);
    write_travelthru(&out, r, f);
    out.api->rewind(out.handle);
    len = out.api->read(out.handle, buf, sizeof(buf));
    buf[len] = '\0';
    CuAssertStrEquals_Msg(tc, "list one unit", "Durchreise: Hodor (1).\n", buf);
    mstream_done(&out);

    mstream_init(&out);
    move_unit(u, r, 0);
    write_travelthru(&out, r, f);
    out.api->rewind(out.handle);
    len = out.api->read(out.handle, buf, sizeof(buf));
    CuAssertIntEquals_Msg(tc, "do not list units that stopped in the region", 0, (int)len);

    mstream_done(&out);
    test_cleanup();
}

typedef struct {
    struct locale *lang;
    spell *sp;
    spellbook *spb;
    spellbook_entry * sbe;
} spell_fixture;

static void setup_spell_fixture(spell_fixture * spf) {
    spf->lang = test_create_locale();
    locale_setstring(spf->lang, mkname("spell", "testspell"), "Testzauber");
    locale_setstring(spf->lang, "nr_spell_type", "Typ:");
    locale_setstring(spf->lang, "sptype_normal", "Normal");
    locale_setstring(spf->lang, "nr_spell_modifiers", "Modifier:");
    locale_setstring(spf->lang, "smod_none", "Keine");
    locale_setstring(spf->lang, keyword(K_CAST), "ZAUBERE");
    locale_setstring(spf->lang, parameters[P_REGION], "REGION");
    locale_setstring(spf->lang, parameters[P_LEVEL], "STUFE");
    locale_setstring(spf->lang, "par_unit", "enr");
    locale_setstring(spf->lang, "par_ship", "snr");
    locale_setstring(spf->lang, "par_building", "bnr");
    locale_setstring(spf->lang, "spellpar::hodor", "Hodor");

    spf->spb = create_spellbook("testbook");
    spf->sp = test_create_spell();
    spellbook_add(spf->spb, spf->sp, 1);
    spf->sbe = spellbook_get(spf->spb, spf->sp);
}

static void cleanup_spell_fixture(spell_fixture *spf) {
    spellbook_clear(spf->spb);
    free(spf->spb);
    test_cleanup();
}

static void set_parameter(spell_fixture spell, char *value) {
    free(spell.sp->parameter);
    spell.sp->parameter = _strdup(value);
}

static void check_spell_syntax(CuTest *tc, char *msg, spell_fixture *spell, char *syntax) {
    stream strm;
    char buf[1024];
    char *linestart, *newline;
    size_t len;

    mstream_init(&strm);
    nr_spell_syntax(&strm, spell->sbe, spell->lang);
    strm.api->rewind(strm.handle);
    len = strm.api->read(strm.handle, buf, sizeof(buf));
    buf[len] = '\0';

    linestart = strtok(buf, "\n");
    while (linestart && !strstr(linestart, "ZAUBERE"))
        linestart = strtok(NULL, "\n");

    CuAssertPtrNotNull(tc, linestart);

    newline = strtok(NULL, "\n");
    while (newline) {
        *(newline - 1) = '\n';
        newline = strtok(NULL, "\n");
    }

    CuAssertStrEquals_Msg(tc, msg, syntax, linestart);

    mstream_done(&strm);
}

static void test_write_spell_syntax(CuTest *tc) {
    spell_fixture spell;

    test_cleanup();
    setup_spell_fixture(&spell);

    check_spell_syntax(tc, "vanilla", &spell, "  ZAUBERE \"Testzauber\"");

    spell.sp->sptyp |= FARCASTING;
    check_spell_syntax(tc, "far", &spell, "  ZAUBERE [REGION x y] \"Testzauber\"");

    spell.sp->sptyp |= SPELLLEVEL;
    check_spell_syntax(tc, "farlevel", &spell, "  ZAUBERE [REGION x y] [STUFE n] \"Testzauber\"");
    spell.sp->sptyp = 0;

    set_parameter(spell, "kc");
    check_spell_syntax(tc, "kc", &spell, "  ZAUBERE \"Testzauber\" ( REGION | EINHEIT <enr> | SCHIFF <snr> | BURG <bnr> )");

    spell.sp->sptyp |= BUILDINGSPELL;
    check_spell_syntax(tc, "kc typed", &spell, "  ZAUBERE \"Testzauber\" BURG <bnr>");
    spell.sp->sptyp = 0;

    set_parameter(spell, "b");
    check_spell_syntax(tc, "b", &spell, "  ZAUBERE \"Testzauber\" <bnr>");

    set_parameter(spell, "s");
    check_spell_syntax(tc, "s", &spell, "  ZAUBERE \"Testzauber\" <snr>");

    set_parameter(spell, "s+");
    check_spell_syntax(tc, "s+", &spell, "  ZAUBERE \"Testzauber\" <snr> [<snr> ...]");

    set_parameter(spell, "u");
    check_spell_syntax(tc, "u", &spell, "  ZAUBERE \"Testzauber\" <enr>");

    set_parameter(spell, "r");
    check_spell_syntax(tc, "r", &spell, "  ZAUBERE \"Testzauber\" <x> <y>");

    set_parameter(spell, "bc");
    free(spell.sp->syntax);
    spell.sp->syntax = _strdup("hodor");
    check_spell_syntax(tc, "bc hodor", &spell, "  ZAUBERE \"Testzauber\" <bnr> <Hodor>");
    free(spell.sp->syntax);
    spell.sp->syntax = 0;

    set_parameter(spell, "c?");
    free(spell.sp->syntax);
    spell.sp->syntax = _strdup("hodor");
    check_spell_syntax(tc, "c?", &spell, "  ZAUBERE \"Testzauber\" [<Hodor>]");
    free(spell.sp->syntax);
    spell.sp->syntax = 0;

    set_parameter(spell, "kc+");
    check_spell_syntax(tc, "kc+", &spell,
        "  ZAUBERE \"Testzauber\" ( REGION | EINHEIT <enr> [<enr> ...] | SCHIFF <snr>\n  [<snr> ...] | BURG <bnr> [<bnr> ...] )");

    cleanup_spell_fixture(&spell);
}

CuSuite *get_report_suite(void)
{
    CuSuite *suite = CuSuiteNew();
    SUITE_ADD_TEST(suite, test_write_spaces);
    SUITE_ADD_TEST(suite, test_write_many_spaces);
    SUITE_ADD_TEST(suite, test_write_travelthru);
    SUITE_ADD_TEST(suite, test_write_spell_syntax);
    return suite;
}