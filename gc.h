#ifndef GC_H
#define GC_H

#include "vm.h"
#include <stdio.h>
#include <stdlib.h>


typedef struct {
    vm_object* objs;
} GarbageCollector;

static GarbageCollector gc;

void gc_init();
void gc_add_obj(vm_object* obj);

void gc_collect();
void gc_shutdown();
#endif
