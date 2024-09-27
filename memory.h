#ifndef memoryy_h 
#define memoryy_h

#include <stddef.h>

void* reallocate (void* ptr, size_t old_size, size_t new_size); 


#define FREE_ARRAY(type, pointer, oldCount) \
    reallocate(pointer, sizeof(type) * (oldCount), 0)

#endif
