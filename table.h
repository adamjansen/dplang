#ifndef DPLANG_TABLE_H
#define DPLANG_TABLE_H

#include "value.h"
#include "object.h"
#include "hash.h"

struct entry {
    struct object_string *key;
    value value;
};

struct table {
    int count;
    int capacity;
    struct entry *entries;
};

int table_init(struct table *table);
bool table_set(struct table *table, struct object_string *key, value value);
bool table_get(struct table *table, struct object_string *key, value *value);
void table_free(struct table *table);
bool table_delete(struct table *table, struct object_string *key);
void table_add_all(struct table *from, struct table *to);
struct object_string *table_find_string(struct table *table, const char *chars, int length, hash_t hash);
void table_dump(struct table *table);
#endif
