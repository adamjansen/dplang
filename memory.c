#include "memory.h"
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>

void *reallocate(void *p, size_t prev_size, size_t new_size)
{
    (void)prev_size;
    return realloc(p, new_size);
}