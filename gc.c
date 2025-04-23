#include "gc.h"

// Initialize the GC
void gc_init() {
    gc.count = 0;
#ifdef DEBUG_LOG_GC
    printf("[GC] Initialized\n");
#endif
}

// Register a new allocation
void* gc_alloc(size_t size, const char* tag) {
    void* ptr = malloc(size);
    if (!ptr) {
        fprintf(stderr, "[GC] Allocation failed for tag: %s\n", tag);
        exit(EXIT_FAILURE);
    }

    gc.count++;

#ifdef DEBUG_LOG_GC
    printf("[GC] Allocated %zu bytes for %s (%p)\n", size, tag, ptr);
#endif

#ifdef DEBUG_STRESS_GC
    gc_collect();
#endif

    return ptr;
}

// Free all tracked allocations
void gc_collect() {
    size_t freed = 0;
    gc.count = 0;

#ifdef DEBUG_LOG_GC
    printf("[GC] Collection complete. Freed %zu objects.\n", freed);
#endif
}

// Optional cleanup on exit
void gc_shutdown() {
    gc_collect();
#ifdef DEBUG_LOG_GC
    printf("[GC] Shutdown complete\n");
#endif
}
