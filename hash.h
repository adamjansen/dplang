#ifndef DPLANG_HASH_H
#define DPLANG_HASH_H
#include <stdint.h>

typedef uint32_t hash_t;

hash_t hash_string(const char *key, int length);
#endif