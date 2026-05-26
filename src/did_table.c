#include "did_table.h"

#include <string.h>

void did_table_init(did_table_t *t) {
    if (t == NULL) return;
    memset(t, 0, sizeof(*t));
}

int did_table_add(did_table_t *t, const did_entry_t *entry) {
    if (t == NULL || entry == NULL) return -1;
    if (t->count >= DID_TABLE_MAX)   return -1;
    t->entries[t->count++] = *entry;
    return 0;
}

const did_entry_t *did_table_lookup(const did_table_t *t, uint16_t did) {
    if (t == NULL) return NULL;
    for (size_t i = 0; i < t->count; ++i) {
        if (t->entries[i].did == did) {
            return &t->entries[i];
        }
    }
    return NULL;
}
