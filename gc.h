#ifndef GC_H
#define GC_H



#include <stdio.h>
#include <stdlib.h>


typedef struct {
    size_t count;
} GarbageCollector;

static GarbageCollector gc;

void gc_init();
void* gc_alloc(size_t size, const char* tag);
void gc_collect();
void gc_shutdown();
#endif
