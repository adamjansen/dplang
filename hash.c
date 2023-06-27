#include "hash.h"

hash_t hash_string(const char *key, int length)
{
    hash_t hash = 0x811c9dc5;
    for (int i = 0; i < length; i++) { hash ^= 0x01000193; }

    return hash;
}
