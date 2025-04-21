#include "lobj.h"
#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>



vm_upvalue* vm_upvalue_create(lobj* ref) {
    vm_upvalue *up = ALLOCATE(vm_upvalue, 1);
    up->location = ref;
    up->obj.type = OBJ_UPVALUE;
    up->closed = lobj_num(false);
    up->next=NULL;
    return up;
}
vm_string* vm_string_create(const char* contents) {
    vm_string* s = malloc(sizeof(vm_string));
    s->obj.type=OBJ_STR;
    s->contents=solid_str_new(contents);
    return s;
}
void vm_string_free(vm_string* s) {
    solid_str_free(&s->contents);
    free(s);

}
vm_closure* vm_closure_create(vm_function* fn) {
    vm_closure* closure = ALLOCATE(vm_closure, 1);
    closure->upvalues_ct=fn->upvalues_count;
    // pointer to vm_function
    closure->fn=fn;

    vm_upvalue** upvalues = ALLOCATE(vm_upvalue*, closure->upvalues_ct);
    // current tracking size
    // zero out closure->upvalues
    for (int i = 0; i < closure->upvalues_ct; i++) {
        upvalues[i] = NULL;
    }
    closure->upvalues = upvalues;

    // union tag
    closure->obj.type = OBJ_CLOSURE;

    return closure;
}


vm_native* vm_native_create(int argc, native_func fn) {
    vm_native* nativeobj = ALLOCATE(vm_native, 1);
    nativeobj->argc=argc;
    nativeobj->native=fn;
    nativeobj->obj.type = OBJ_NATIVE;
    return nativeobj;
}

// NOTE: contents MUST BE NULL TERMINATED
lobj lobj_str(const char* contents) {
    // solid string is heap allocated
    vm_string* s = vm_string_create(contents);
    return (lobj) {
        .type=LVAL_OBJ,
        .v.obj=(vm_object*)s
    };
}

/* Create a new number type lval */

vm_function* vm_function_create(
    uint8_t* initial_code,
    const uint8_t code_ct,
    lobj* values,
    const int values_count,
    const uint8_t arity,
    const lobj name,
    const int upvalues_count
) {
   vm_function* fn = malloc(sizeof (vm_function));
                   fn->obj.type=OBJ_FUNC;
                   fn->chunk=initial_code;
                   fn->chunk_ct=code_ct;
                   fn->values=values;
                   fn->values_ct=values_count;
                   fn->arity=arity;
                   fn->name=name;
                   fn->upvalues_count = upvalues_count;
   return fn;
}


void lobj_free(lobj* o) {
  switch (o->type) {
    /* Do nothing special for number type */
    case LVAL_NUM: break;
    /* For Err or Sym free the string data */
    case LVAL_ERR:  break;
    case LVAL_SYM: break;
    case LVAL_OBJ: {
        int obj_ty = o->v.obj->type;
        switch(obj_ty) {
            case OBJ_STR: { 
                vm_string * fn=(vm_string*)o->v.obj;
                solid_str_free(&fn->contents);
                free(fn);
            } break;
            case OBJ_FUNC : {
                vm_function* fn=(vm_function*)o->v.obj;
                free(fn->name.v.obj);
                free(fn);
            } break;
            case OBJ_CLOSURE: {
                // todo: do we need to free the function here?
                const vm_closure* closure = (vm_closure*)o->v.obj;
                vm_function* fn= closure->fn;
                // we don't free the underlying upvalues, that's up to the garbage collector in the future to do that
                free(closure->upvalues);
                free(fn->name.v.obj);
                free(fn);
            } break;
            case OBJ_UPVALUE: {
                free(o->v.obj);
            } break;
            case OBJ_USR: {
            } break;
            default: {
                printf("Unknown object type to free");
            }break;
        }
    } break;
    
    }
  /* Free the memory allocated for the "lval" struct itself */
  /* when we create heap objs this will be necessary*/
  //free(o);
}

/* Print an "lobj" */
void lobj_print(lobj* o) {
  if(!o) {
    printf("null o passed into lobj_print");
    return;
  }
  switch (o->type) {
    /* In the case the type is a number print it */
    /* Then 'break' out of the switch. */
    case LVAL_NUM: printf("(num) %.2f", o->v.num); break;

    /* In the case the type is an error */
    case LVAL_ERR:
      /* Check what type of error it is and print it */
      if (o->v.err == LERR_DIV_ZERO) { printf("Error: Division By Zero!"); }
      if (o->v.err == LERR_BAD_OP)   { printf("Error: Invalid Operator!"); }
      if (o->v.err == LERR_BAD_NUM)  { printf("Error: Invalid Number!"); }
    break;

    case LVAL_SYM: {
        printf("sym");
    } break;
    case LVAL_OBJ: {
        switch(o->v.obj->type) {
            case OBJ_FUNC: {
                const vm_function* fn = ((vm_function*)o->v.obj);
                //todo: fix
                printf("func %s(arity=%d)", SOLID_STR_TO_C(AS_STR(fn->name)->contents), fn->arity);
            } break;
            case OBJ_CLOSURE: {
                printf("closure ");
                vm_closure* closure = as_closure(o);
                vm_function* fn = closure->fn;
                solid_str contents = AS_STR(fn->name)->contents;
                printf("func %s(arity=%d)", SOLID_STR_TO_C(contents), fn->arity);
            } break;
            case OBJ_STR: {
                printf("'%s'", SOLID_STR_TO_C(AS_STR(*o)->contents));
            } break;
            case OBJ_UPVALUE: {
                printf("upvalue");
            }
            default: { 
                printf("obj, yo no se"); 
            } break;
        }
    } break;
    default: {
        printf("type: %d\n", o->type);
        printf("Unknown lobj to print");
    }
  }
}

/* Print an "lobj" followed by a newline */
void lobj_println(lobj* v) { lobj_print(v); putchar('\n'); }
