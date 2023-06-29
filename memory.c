#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"
#include <stdlib.h>
#include <time.h>

#include <stdio.h>
#include <stddef.h>

// #define DEBUG_LOG_GC

static bool enabled = false;
static size_t total_allocated = 0;
static size_t next_gc = 1024;

static size_t gray_capacity = 0;
static size_t gray_count = 0;
static struct object **gray_stack = NULL;

struct object *gc_objects = NULL;

struct vm *gc_vm = NULL;

void gc_init(struct vm *vm)
{
    gray_capacity = 8;
    gray_count = 0;
    gray_stack = (struct object **)realloc(gray_stack, sizeof(struct object *) * gray_capacity);
    gc_vm = vm;
    // total_allocated = 0;
    next_gc = 1024;
    enabled = true;
}

void gc_mark_object(struct object *object)
{
    if (object == NULL)
        return;
    if (object->marked)
        return;

#ifdef DEBUG_LOG_GC
    printf("%p mark ", (void *)object);
    object_print(object);
    printf("\n");
#endif

    object->marked = true;

    if (gray_capacity < gray_count + 1) {
        gray_capacity = (gray_count < 8) ? 8 : gray_capacity * 2;
        gray_stack = (struct object **)realloc(gray_stack, sizeof(struct object *) * gray_capacity);
        if (gray_stack == NULL)
            exit(1);
    }
    gray_stack[gray_count++] = object;
}

void gc_mark_value(value value)
{
    if (IS_OBJECT(value))
        gc_mark_object(AS_OBJECT(value));
}

static void gc_mark_array(struct value_array *array)
{
    for (int i = 0; i < array->count; i++) { gc_mark_value(array->values[i]); }
}

static void gc_blacken_object(struct object *object)
{
#ifdef DEBUG_LOG_GC
    printf("%p blacken ", (void *)object);
    value_print(OBJECT_VAL(object));
    printf("\n");
#endif
    switch (object->type) {
        case OBJECT_UPVALUE:
            gc_mark_value(((struct object_upvalue *)object)->closed);
            break;
        case OBJECT_FUNCTION: {
            struct object_function *function = (struct object_function *)object;
            gc_mark_object((struct object *)function->name);
            gc_mark_array(&function->chunk.constants);
            break;
        }
        case OBJECT_CLOSURE: {
            struct object_closure *closure = (struct object_closure *)object;
            gc_mark_object((struct object *)closure->function);
            for (int i = 0; i < closure->nupvalues; i++) { gc_mark_object((struct object *)closure->upvalues[i]); }
            break;
        }
        case OBJECT_NATIVE:
        case OBJECT_STRING:
            break;
    }
}

void gc_mark_table(struct table *table)
{
    for (int i = 0; i < table->capacity; i++) {
        struct entry *entry = &table->entries[i];
        gc_mark_object((struct object *)entry->key);
        gc_mark_value(entry->value);
    }
}

void gc_table_remove_white(struct table *table)
{
    for (int i = 0; i < table->capacity; i++) {
        struct entry *entry = &table->entries[i];
        if (entry->key != NULL && !entry->key->object.marked) {
            table_delete(table, entry->key);
        }
    }
}

static void gc_mark_roots(struct vm *vm)
{
    for (value *slot = vm->stack; slot < vm->sp; slot++) { gc_mark_value(*slot); }

    for (int i = 0; i < vm->frame_count; i++) { gc_mark_object((struct object *)vm->frames[i].closure); }

    for (struct object_upvalue *upvalue = vm->open_upvalues; upvalue != NULL; upvalue = upvalue->next) {
        gc_mark_object((struct object *)upvalue);
    }

    gc_mark_table(&vm->globals);
}

static void gc_trace_references()
{
    while (gray_count > 0) {
        struct object *object = gray_stack[--gray_count];
        gc_blacken_object(object);
    }
}

static void gc_sweep()
{
    struct object *previous = NULL;
    struct object *object = gc_objects;
    while (object != NULL) {
        if (object->marked) {
            object->marked = false;
            previous = object;
            object = object->next;
        } else {
            struct object *unreached = object;
            object = object->next;
            if (previous != NULL) {
                previous->next = object;
            } else {
                gc_objects = object;
            }
            object_free(unreached);
        }
    }
}

void *reallocate(void *p, size_t prev_size, size_t new_size)
{
    total_allocated += new_size - prev_size;
#ifdef DEBUG_STRESS_GC
    if (new_size > prev_size) {
        gc_collect();
    }
#endif

    if (new_size > prev_size && total_allocated > next_gc) {
        gc_collect();
    }

#ifdef DEBUG_LOG_GC
    if (prev_size == 0) {
        printf("allocating %zu bytes\n", new_size);
    } else if (new_size == 0) {
        printf("%p free %zu bytes\n", p, prev_size);
    } else {
        printf("%p re-alloc %zu => %zu\n", p, prev_size, new_size);
    }
#endif

    p = realloc(p, new_size);
    if (new_size > 0 && p == NULL) {
        // TODO: panic! out of memory
        exit(1);
    }
    return p;
}

void gc_collect()
{
    if (!enabled) {
#ifdef DEBUG_LOG_GC
        printf("Skipping GC collection\n");
#endif
        return;
    }

#ifdef DEBUG_LOG_GC
    clock_t start = clock();
    printf("--- gc begin\n");
    size_t before = total_allocated;
#endif
    gc_mark_roots(gc_vm);
    gc_trace_references();
    gc_table_remove_white(&gc_vm->strings);
    gc_sweep();

    next_gc = total_allocated * 2;
#ifdef DEBUG_LOG_GC
    double duration = ((double)(clock() - start)) / CLOCKS_PER_SEC;
    printf("--- gc end [ %zu bytes => %zu bytes; %zu bytes collected in %6.6f s; next at %zu]\n", before,
           total_allocated, before - total_allocated, duration, next_gc);
#endif
}