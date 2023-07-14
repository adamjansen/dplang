#ifndef DPLANG_HASH_H
#define DPLANG_HASH_H
#include <stdint.h>
#include <stddef.h>
#include "value.h"

typedef uint32_t hash_t;

hash_t hash_string(const char *s, size_t length);
hash_t hash_value(value v);
hash_t hash_double(double d);
#endif