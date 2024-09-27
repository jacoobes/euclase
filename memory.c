#include "./memory.h"
#include <stdio.h>
#include <stdlib.h>

/*
 * If the first argument is a null pointer, realloc() allocates a new memory block, behaving similarly to malloc().
 * Otherwise, if the pointer argument does not point to a memory block allocated by malloc(), calloc(), or realloc(),
 * or if it points to such a block that has since been released by a call to free() or realloc(), then the behavior is undefined.
 */
void* reallocate (void* ptr, size_t old_size, size_t new_size) {
    if(new_size == 0) {
        free(ptr);
        return NULL;
    }
    if(old_size > new_size) {
        printf("While reallocating, Why is old size greated than new size?\n");
        exit(1);
    }
    /*
     * If the first argument is a null pointer, realloc() allocates a new memory block, behaving similarly to malloc().
     * Otherwise, if the pointer argument does not point to a memory block allocated by malloc(), calloc(), or realloc(),
     * or if it points to such a block that has since been released by a call to free() or realloc(), then the behavior is undefined.
     */
    void* result = realloc(ptr, new_size);
    if (result == NULL) {
        puts("Result reallocated to null\n");
        exit(1);
    }
    return result;
}
