#ifndef DPLANG_BUILTINS_H
#define DPLANG_BUILTINS_H
#include "object.h"

struct builtin_function_info {
    const char *name;
    native_function function;
};

extern struct builtin_function_info builtins[];

#endif
