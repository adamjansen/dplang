#ifndef DPLANG_COMPILER_H
#define DPLANG_COMPILER_H
#include "chunk.h"
struct object_function *compile(const char *source);

void compiler_gc_roots(void);

#endif
