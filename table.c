#include "memory.h"
#include "table.h"
#include <string.h>
#include <stdio.h>

#define TABLE_MAX_LOAD (3 / 4)

#define TABLE_MIN_CAPACITY  8
#define TABLE_GROWTH_FACTOR 2

int table_init(struct table *table)
{
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
    return 0;
}

void table_free(struct table *table)
{
    table->entries = (struct entry *)reallocate(table->entries, table->capacity, 0);
}
static struct entry *find_entry(struct entry *entries, int capacity, struct object_string *key)
{
    uint32_t index = key->hash & (capacity - 1);
    struct entry *tombstone = NULL;
    while (1) {
        struct entry *entry = &entries[index];
        if (entry->key == NULL) {
            if (IS_NIL(entry->value)) {
                return tombstone != NULL ? tombstone : entry;
            } else {
                if (tombstone == NULL) {
                    tombstone = entry;
                }
            }
        } else if (entry->key == key) {
            return entry;
        } else if (entry->key->length == key->length && entry->key->hash == key->hash &&
                   memcmp(entry->key->data, key->data, key->length) == 0) {
            return entry;
        }
        index = (index + 1) % (capacity - 1);
    }
}

static void adjust_capacity(struct table *table, int capacity)
{
    struct entry *entries = (struct entry *)reallocate(NULL, 0, sizeof(struct entry) * capacity);
    for (int i = 0; i < capacity; i++) {
        entries[i].key = NULL;
        entries[i].value = NIL_VAL;
    }

    table->count = 0;
    for (int i = 0; i < table->capacity; i++) {
        struct entry *entry = &table->entries[i];
        if (entry->key == NULL) {
            continue;
        }

        struct entry *dest = find_entry(entries, capacity, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;
        table->count++;
    }

    table->entries = (struct entry *)reallocate(table->entries, table->capacity * sizeof(struct entry), 0);

    table->entries = entries;
    table->capacity = capacity;
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters)
void table_add_all(struct table *from, struct table *to)
{
    for (int i = 0; i < from->capacity; i++) {
        struct entry *entry = &from->entries[i];
        if (entry->key != NULL) {
            table_set(to, entry->key, entry->value);
        }
    }
}
// NOLINTEND(bugprone-easily-swappable-parameters)

struct object_string *table_find_string(struct table *table, const char *chars, size_t length, hash_t hash)
{
    if (table->count == 0) {
        return NULL;
    }

    uint32_t index = hash & (table->capacity - 1);

    while (1) {
        struct entry *entry = &table->entries[index];
        if (entry->key == NULL) {
            if (IS_NIL(entry->value)) {
                // stop on empty non-tombstone entry
                return NULL;
            }
        } else if (entry->key->length == length && entry->key->hash == hash &&
                   memcmp(entry->key->data, chars, length) == 0) {
            return entry->key;
        }

        index = (index + 1) & (table->capacity - 1);
    }
}

bool table_set(struct table *table, struct object_string *key, value value)
{
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        int capacity =
            (table->capacity < TABLE_MIN_CAPACITY) ? TABLE_MIN_CAPACITY : table->capacity * TABLE_GROWTH_FACTOR;
        adjust_capacity(table, capacity);
    }
    struct entry *entry = find_entry(table->entries, table->capacity, key);
    bool is_new_key = entry->key == NULL;
    if (is_new_key && IS_NIL(entry->value)) {
        table->count++;
    }

    entry->key = key;
    entry->value = value;
    return is_new_key;
}

bool table_get(struct table *table, struct object_string *key, value *value)
{
    if (table->count == 0) {
        return false;
    }

    struct entry *entry = find_entry(table->entries, table->capacity, key);
    if (entry->key == NULL) {
        return false;
    }

    *value = entry->value;
    return true;
}

bool table_delete(struct table *table, struct object_string *key)
{
    if (table->count == 0) {
        return false;
    }
    struct entry *entry = find_entry(table->entries, table->capacity, key);
    if (entry->key == NULL) {
        return false;
    }

    // Tombstone sentinel value
    entry->key = NULL;
    entry->value = BOOL_VAL(true);
    return true;
}

void table_dump(struct table *table)
{
    for (int i = 0; i < table->capacity; i++) {
        struct entry *entry = &table->entries[i];
        if (entry->key != NULL) {
            printf("%s=", entry->key->data);
            value_print(entry->value);
            printf("\n");
        }
    }
}
