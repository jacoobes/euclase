#include "gc.h"

GarbageCollector gc;


void gc_init(VM* vm) {
    gc.objs=NULL;
    gc.is_init=true;

    // The starting threshold here is arbitrary
    gc.next_gc = 1024 * 1024;
    gc.vm = vm;
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
    // head insert: gc.objs is always the newest object and reaches every other one
    obj->next = gc.objs;
    gc.objs = obj;
}


// Optional cleanup on exit
void gc_shutdown() {
#ifdef DEBUG_LOG_GC
    printf("[GC] Shutdown complete\n");
#endif
}
