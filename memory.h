#ifndef memoryy_h 
#define memoryy_h

#include <stddef.h>
#include "vm.h"
#include "gc.h"

void* reallocate (void* ptr, size_t old_size, size_t new_size); 
void mark_value(VM* vm, lobj lobj);
void mark_object(VM* vm, vm_object* object);
void collect_garbage(VM* vm);


#define FREE_ARRAY(type, pointer, oldCount) \
    reallocate(pointer, sizeof(type) * (oldCount), 0)

// MALLOC with casting
#define ALLOCATE(ty, count) \
    ((ty*)reallocate(NULL, 0, sizeof(ty) * (count)))



#endif
