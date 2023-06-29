#ifndef DPLANG_MEMORY_H
#define DPLANG_MEMORY_H
#include <stddef.h>
#include "vm.h"

void *reallocate(void *p, size_t prev_size, size_t new_size);
void gc_collect();

void gc_init(struct vm *vm);
#endif
