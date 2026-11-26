#include "./memory.h"
#include <stdio.h>
#include <stdlib.h>

/*
 * If the first argument is a null pointer, realloc() allocates a new memory block, behaving similarly to malloc().
 * Otherwise, if the pointer argument does not point to a memory block allocated by malloc(), calloc(), or realloc(),
 * or if it points to such a block that has since been released by a call to free() or realloc(), then the behavior is undefined.
 */
void* reallocate (void* ptr, size_t old_size, size_t new_size) {
    gc.bytes_allocated += new_size - old_size;
    if(new_size == 0) {
        free(ptr);
        return NULL;
    }
    if(old_size > new_size) {
        printf("While reallocating, Why is old size greated than new size?\n");
        exit(1);
    }

#ifdef DEBUG_STRESS_GC
     printf("[GC] stress test\n");
     collect_garbage(gc.vm);
#endif
     if(gc.bytes_allocated > gc.next_gc) {
         collect_garbage(gc.vm);
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

void mark_value(VM* vm, const lobj obj) {
  if (IS_OBJ(obj))  {
    mark_object(vm, obj.v.obj);
  }
}

static void mark_array(VM* vm, const lobj* array, const int count) {
    for (int i =0 ;i < count; i++) {
        mark_value(vm, array[i]);
    }
}

void mark_object(VM* vm, vm_object* object) {
    if(object == NULL) return;

    // prevent cycle 
    if (object->is_marked) return;

    object->is_marked=true;
    
    #ifdef DEBUG_LOG_GC
      printf("%p marked object, type %d ", (void*)object, object->type);
      printf("\n");
    #endif
    if (vm->gray_capacity < vm->gray_count + 1) {
        vm->gray_capacity = vm->gray_capacity < 8 ? 8 : vm->gray_capacity * 8;
        vm->gray_stack = (vm_object**)realloc(vm->gray_stack,
                                              sizeof(vm_object*) * vm->gray_capacity);
    }
    vm->gray_stack[vm->gray_count++] = object;
}



void mark_roots(VM* vm) {
  printf("[GC] Marking stack content\n");
  for (const lobj* slot = vm->stack; slot < vm->stack_top; slot++) {
    mark_value(vm, *slot);
  }


  printf("[GC] Marking frames\n");
  for (int i = 0; i < vm->frame_count; i++) {
    mark_object(vm, (vm_object*)vm->call_stack[i].closure);
  }

  for (vm_upvalue* upvalue = vm->open_upvalues;
       upvalue != NULL;
       upvalue = upvalue->next) {
    mark_object(vm,(vm_object*)upvalue);
  }
  mark_table(vm, vm->globals);
}

void blacken_obj(VM* vm, vm_object* o) {
    switch(o->type) {
        case OBJ_CLOSURE: {
            vm_closure* cls = ((vm_closure*)o);
    #ifdef DEBUG_LOG_GC
            printf("blackening closure ") ; lobj_println(&cls->fn->name);
    #endif
            mark_object(vm, (vm_object*)cls->fn);
            for(uint8_t i = 0; i < cls->upvalues_ct; i++) {
                mark_object(vm, (vm_object*)cls->upvalues[i]);
            }
        } break;
        case OBJ_STR: {
        } break;
        case OBJ_FUNC: { 
            vm_function* fn = ((vm_function*)o);
            mark_value(vm, fn->name);
            mark_array(vm, fn->values, fn->values_ct);
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
        //blackening means dead and ready to clean
        blacken_obj(vm, o);
    }
}


void sweep() {
    vm_object* previous = NULL;
    vm_object* object = gc.objs;
    while(object != NULL) {
        if(object->is_marked) {
            object->is_marked=false;
            previous = object;
            object = object->next;
        } else {
            vm_object* unreached = object;
            object = object->next;
            if(previous != NULL) {
                previous->next = object;
            } else {
                gc.objs = object;
            }
            vm_object_free(unreached);
        }
    }
}

void collect_garbage(VM* vm) {
    #ifdef DEBUG_LOG_GC
      printf("-- collecting gc\n");
      size_t before = gc.bytes_allocated;
    #endif

    #ifdef DEBUG_LOG_GC
      printf("GC mark roots\n");
    #endif
    mark_roots(vm);
    

    #ifdef DEBUG_LOG_GC
        printf("GC trace refs \n");
    #endif
    trace_refs(vm);

    #ifdef DEBUG_LOG_GC
        printf("GC table mark white, weak references\n");
    #endif
    table_mark_white(vm->strings);

    #ifdef DEBUG_LOG_GC
        printf("GC sweep\n");
    #endif
    sweep();


    gc.next_gc = gc.bytes_allocated * GC_HEAP_GROW_FACTOR;

    #ifdef DEBUG_LOG_GC
      printf("-- gc end\n");
      printf("   collected %zu bytes (from %zu to %zu) next at %zu\n", 
            before -  gc.bytes_allocated, before, gc.bytes_allocated, gc.next_gc
        );
    #endif
}
