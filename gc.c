#include "gc.h"

// Initialize the GC
void gc_init() {
    gc.objs=NULL;
#ifdef DEBUG_LOG_GC
    printf("[GC] Initialized\n");
#endif
}


void gc_add_obj(vm_object* obj) {

#ifdef DEBUG_LOG_GC
    //printf("[GC] Allocated %zu bytes for %s (%p)\n", obj, tag, ptr);
#endif

    if (obj == NULL) {
        printf("obj is null while inserting into gc\n");
        exit(1);
    }
    if(gc.objs == NULL) {
        gc.objs = obj;
    } else {
        // attach object to linked list of all objects
        // move pointer to added object
        gc.objs->next = obj;
        gc.objs = gc.objs->next;
    }
}

// Free all tracked allocations
void gc_collect() {

#ifdef DEBUG_LOG_GC
    //printf("[GC] Collection complete. Freed %zu objects.\n", freed);
#endif
}

// Optional cleanup on exit
void gc_shutdown() {
    gc_collect();
#ifdef DEBUG_LOG_GC
    printf("[GC] Shutdown complete\n");
#endif
}
