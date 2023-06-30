#include "hash.h"

#define HASH_INITIAL_VALUE 0x811c9dc5
#define HASH_MULT_FACTOR   0x01000193

hash_t hash_string(const char *key, size_t length)
{
    hash_t hash = HASH_INITIAL_VALUE;
    for (int i = 0; i < length; i++) {
        hash ^= (uint8_t)key[i];
        hash *= HASH_MULT_FACTOR;
    }

    return hash;
}
