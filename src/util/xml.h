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

#ifndef H_UTIL_XML
#define H_UTIL_XML

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

  /* new xml functions: */
#include <libxml/tree.h>


    typedef int (*xml_callback) (xmlDocPtr);

    void xml_register_callback(xml_callback callback);
    double xml_fvalue(xmlNodePtr node, const char *name, double dflt);
    int xml_ivalue(xmlNodePtr node, const char *name, int dflt);
    bool xml_bvalue(xmlNodePtr node, const char *name, bool dflt);

    void xml_done(void);
    int read_xml(const char *filename, const char *catalog);

#ifdef __cplusplus
}
#endif
#endif
