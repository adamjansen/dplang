#ifndef DPLANG_UTIL_H
#define DPLANG_UTIL_H

#include <stdint.h>

#define U16LSB(w) (((uint16_t)(w)) & 0xFF)
#define U16MSB(w) ((((uint16_t)(w)) >> 8) & 0xFF)

#define likely(exp)   __builtin_expect((exp), 1)
#define unlikely(exp) __builtin_expect((exp), 0)

#endif
