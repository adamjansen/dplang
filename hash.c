#include "hash.h"

#define FNV_OFFSET_BASIS 0x811c9dc5
#define FNV_PRIME        0x01000193

// FNV1A hash
// See http://www.isthe.com/chongo/src/fnv/hash_32.c
//
//
//
// TODO: replace with SipHash?
hash_t hash_string(const char *key, size_t length)
{
    hash_t hash = FNV_OFFSET_BASIS;
    for (int i = 0; i < length; i++) {
        hash ^= (uint8_t)key[i];
        hash *= FNV_PRIME;
    }

    return hash;
}
