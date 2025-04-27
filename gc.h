#ifndef GC_H
#define GC_H

#include "vm.h"
#include <stdio.h>
#include <stdlib.h>

#define GC_HEAP_GROW_FACTOR 2

typedef struct {
    vm_object* objs;
    VM* vm;

    size_t bytes_allocated;
    size_t next_gc;
} GarbageCollector;

static GarbageCollector gc;

void gc_init(VM* vm);
void gc_add_obj(vm_object* obj);

void gc_shutdown();
#endif
