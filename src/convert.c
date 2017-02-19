#include <platform.h>

#include <kernel/race.h>
#include <kernel/rules.h>
#include <kernel/xmlreader.h>

#include <races/races.h>

#include <util/xml.h>

#include <storage.h>

#define RULES_RACES 1
#define RULES_VERSION RULES_RACES

enum {
    TYPE_NONE,
    TYPE_RACE,
    TYPE_SHIP,
    TYPE_BUILDING,
};

int main(int argc, char **argv) {
    const char * xmlfile, *catalog;

    register_races();
    register_xmlreader();

    if (argc < 3) return -1;
    xmlfile = argv[1];
    catalog = argv[2];
    read_xml(xmlfile, catalog);
    write_rules("rules.dat");
    return 0;
}
