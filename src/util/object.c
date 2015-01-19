#include "object.h"
#include <quicklist.h>

#if 0 // the types do not need to be dynamic like this
static quicklist * object_types;

ql_bool match_type(const void *match, const void *entry) {
    const char * name = (const char *)match;
    const object_type *type = (const object_type *)entry;
    return strcmp(type->name, name) == 0;
}

object_type *get_or_create_type(const char *name) {
    object_type *result = NULL;
    quicklist *ql = object_types;
    int qi;
    ql_iter qli;
    if (ql_find(&ql, &qi, name, match_type)) {
        result = (object_type *)ql_get(ql, qi);
    }
    else {
        result = calloc(1, sizeof(object_type));
        result->name = _strdup(name);
    }
    return result;
}
#endif
