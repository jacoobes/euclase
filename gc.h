#ifndef GC_H
#define GC_H

#include "vm.h"
#include <stdio.h>
#include <stdlib.h>

#define GC_HEAP_GROW_FACTOR 2

typedef struct {
    vm_object* objs;
    size_t bytes_allocated;
    size_t next_gc;

    bool is_init;
    VM* vm;
} GarbageCollector;

extern GarbageCollector gc;

void gc_init(VM*);
void gc_add_obj(vm_object* obj);

void gc_shutdown();
#endif
