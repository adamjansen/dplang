#include <string.h>
#include <stdio.h>

#include "util.h"
#include "memory.h"
#include "table.h"

#define TABLE_MAX_LOAD 75

#define TABLE_MIN_CAPACITY  8
#define TABLE_GROWTH_FACTOR 2

/**
 * Hash table load factor, as integer percent
 */
static inline int table_load(struct table *table)
{
    if (table->capacity == 0) {
        return TABLE_MAX_LOAD + 1;
    }
    return 100 * table->count / table->capacity;  // NOLINT(readability-magic-numbers)
}

static inline int table_next_size(struct table *table)
{
    return (table->capacity < TABLE_MIN_CAPACITY) ? TABLE_MIN_CAPACITY : table->capacity * TABLE_GROWTH_FACTOR;
}

int table_init(struct table *table)
{
    if (unlikely(table == NULL)) {
        return -1;
    }
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
    return 0;
}

void table_free(struct table *table)
{
    if (unlikely(table == NULL)) {
        return;
    }
    table->entries = (struct entry *)reallocate(table->entries, table->capacity * sizeof(struct entry), 0);
    table->capacity = 0;
    table->count = 0;
}

static struct entry *find_entry(struct entry *entries, int capacity, value key)
{
    uint32_t index = hash_value(key) & (capacity - 1);
    struct entry *tombstone = NULL;
    while (1) {
        struct entry *entry = &entries[index];
        if (IS_EMPTY(entry->key)) {
            if (IS_NIL(entry->value)) {
                return tombstone != NULL ? tombstone : entry;
            } else {
                if (tombstone == NULL) {
                    tombstone = entry;
                }
            }
        } else if (value_equal(key, entry->key)) {
            return entry;
        }
        index = (index + 1) & (capacity - 1);
    }
}

static void adjust_capacity(struct table *table)
{
    int capacity = table_next_size(table);
    struct entry *entries = (struct entry *)reallocate(NULL, 0, sizeof(struct entry) * capacity);
    for (int i = 0; i < capacity; i++) {
        entries[i].key = EMPTY_VAL;
        entries[i].value = NIL_VAL;
    }

    table->count = 0;
    for (int i = 0; i < table->capacity; i++) {
        struct entry *entry = &table->entries[i];
        if (IS_EMPTY(entry->key)) {
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
    if (unlikely(from == NULL) || unlikely(to == NULL) || unlikely(from == to)) {
        return;
    }
    for (int i = 0; i < from->capacity; i++) {
        struct entry *entry = &from->entries[i];
        if (!IS_EMPTY(entry->key)) {
            table_set(to, entry->key, entry->value);
        }
    }
}
// NOLINTEND(bugprone-easily-swappable-parameters)

// NOLINTBEGIN(bugprone-easily-swappable-parameters)
struct object_string *table_find_string(struct table *table, const char *chars, size_t length, hash_t hash)
{
    if (unlikely(table == NULL) || table->count == 0) {
        return NULL;
    }

    uint32_t index = hash & (table->capacity - 1);

    while (1) {
        struct entry *entry = &table->entries[index];
        if (IS_EMPTY(entry->key) || !is_object_type(entry->key, OBJECT_STRING)) {
            return NULL;
        }
        struct object_string *string = AS_STRING(entry->key);
        if (string->length == length && memcmp(string->data, chars, length) == 0) {
            return string;
        }

        index = (index + 1) & (table->capacity - 1);
    }
}
// NOLINTEND(bugprone-easily-swappable-parameters)

// NOLINTBEGIN(bugprone-easily-swappable-parameters)
bool table_set(struct table *table, value key, value value)
{
    if (unlikely(table == NULL)) {
        return false;
    }

    int load = table_load(table);
    if (load > TABLE_MAX_LOAD) {
        adjust_capacity(table);
    }
    struct entry *entry = find_entry(table->entries, table->capacity, key);
    // It's a new key if there's no entry there, or if it's a tombstone
    bool is_new_key = IS_EMPTY(entry->key);

    entry->key = key;
    entry->value = value;
    if (is_new_key) {
        table->count++;
    }
    return is_new_key;
}
// NOLINTEND(bugprone-easily-swappable-parameters)

bool table_get(struct table *table, value key, value *value)
{
    if (unlikely(table == NULL) || unlikely(value == NULL) || table->count == 0) {
        return false;
    }

    struct entry *entry = find_entry(table->entries, table->capacity, key);
    if (IS_EMPTY(entry->key)) {
        return false;
    }

    *value = entry->value;
    return true;
}

bool table_delete(struct table *table, value key)
{
    if (unlikely(table == NULL)) {
        return false;
    }

    if (table->count == 0) {
        return false;
    }
    struct entry *entry = find_entry(table->entries, table->capacity, key);
    if (IS_EMPTY(entry->key)) {
        return false;
    }

    table->count--;

    // Tombstone sentinel value
    entry->key = EMPTY_VAL;
    entry->value = BOOL_VAL(true);
    return true;
}

void table_dump(struct table *table)
{
    if (unlikely(table == NULL)) {
        return;
    }
    for (int i = 0; i < table->capacity; i++) {
        struct entry *entry = &table->entries[i];
        if (!IS_EMPTY(entry->key)) {
            value_print(entry->key);
            printf("=");
            value_print(entry->value);
            printf("\n");
        }
    }
}
