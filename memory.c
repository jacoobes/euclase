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

#ifdef DEBUG_STRESS_GC
    gc_collect();
#endif
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

void mark_value(VM* vm, lobj value) {
  if (IS_OBJ(value))  { 
    mark_object(vm, value.v.obj);
  } 
}

void mark_object(VM* vm, vm_object* object) {
    if(object == NULL) return;

    // prevent cycle 
    if (object->is_marked) return;

    object->is_marked=true;
    #ifdef DEBUG_LOG_GC
      printf("%p marked object", (void*)object);
      printf("\n");
    #endif

    if (vm->gray_capacity < vm->gray_count + 1) {
        vm->gray_capacity = vm->gray_capacity * 8;
        vm->gray_stack = (vm_object**)realloc(vm->gray_stack,
                                      sizeof(vm_object*) * vm->gray_capacity);
    }
    vm->gray_stack[vm->gray_count++] = object;
}



void mark_roots(VM* vm) {
  for (lobj * slot = vm->stack; slot < vm->stack_top; slot++) {
    mark_value(vm, *slot);
  }


  for (int i = 0; i < vm->frame_count; i++) {
    mark_object(vm, (vm_object*)vm->call_stack[i].closure);
  }

  for (vm_upvalue* upvalue = vm->open_upvalues;
       upvalue != NULL;
       upvalue = upvalue->next) {
    mark_object(vm,(vm_object*)upvalue);
  }
  mark_table(vm->globals);
}

void blacken_obj(VM* vm, vm_object* o) {
    switch(o->type) {
        case OBJ_CLOSURE: {

        } break;
        case OBJ_STR: {

        } break;
        case OBJ_FUNC: { 
            vm_function* fn = ((vm_function*)o);
            mark_value(vm, fn->name);
        } break;
        case OBJ_UPVALUE: {
            //  When an upvalue is closed,
            //  it contains a reference to the closed-over value.
            //  Since the value is no longer on the stack,
            //  we need to make sure we trace the reference to it from the upvalue.
           mark_value(vm, ((vm_upvalue*)o)->closed); 
        };
        case OBJ_NATIVE: 
        case OBJ_USR:
        { } break;
    }
}

void trace_refs(VM* vm){
    while (vm->gray_count > 0) {
        vm_object* o = vm->gray_stack[--vm->gray_count];
        blacken_obj(vm, o);
    }
}


void sweep(VM* vm) {
    
}
void collect_garbage(VM* vm) {
    #ifdef DEBUG_LOG_GC
      printf("-- gc begin\n");
    #endif

    mark_roots(vm);
    
    trace_refs(vm);
    
    sweep(vm);
    #ifdef DEBUG_LOG_GC
      printf("-- gc end\n");
    #endif
}
