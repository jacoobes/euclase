#include "lobj.h"
#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//djb2
unsigned long
hash(const unsigned char *str)
{
    unsigned long hash = 5381;
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

vm_closure* vm_closure_create(vm_function* fn) {
    vm_closure* closure = ALLOCATE(vm_closure, 1);

    // current tracking size
    closure->upvalues_ct=0;
    // zero out closure->upvalues
    memset(closure->upvalues, 0, sizeof(closure->upvalues));

    // union tag
    closure->obj.type = OBJ_CLOSURE;

    // pointer to vm_function
    closure->fn=fn;
    return closure;
}

// NOTE: contents MUST BE NULL TERMINATED
lobj lobj_str(const char* contents) {
    solid_str ss = solid_str_new(contents);
    vm_string* s = ALLOCATE(vm_string, 1);
    s->hash= hash(contents);
    s->obj.type=OBJ_STR;
    s->contents=ss;
    return (lobj) {
        .type=LVAL_OBJ,
        .v.obj=(vm_object*)s
    };
}

/* Create a new number type lval */

vm_function* vm_function_create(
    uint8_t* initial_code,
    uint8_t code_ct,
    lobj* values,
    int values_count,
    uint8_t arity,
    lobj name
) {
   vm_function* fn = ALLOCATE(vm_function, 1);
   fn->obj.type=OBJ_FUNC;
   fn->chunk=initial_code;
   fn->chunk_ct=code_ct;
   fn->values=values;
   fn->values_ct=values_count;
   fn->arity=arity;
   fn->name=name; 
   return fn;
}

lobj lobj_func(uint8_t* initial_code,
               uint8_t code_ct,
               lobj* values,
               int values_count,
               uint8_t arity,
               lobj name) {
    //FIX ME: PROPERLY MANAGE MEMORY
    vm_function* fn = vm_function_create(
               initial_code,
               code_ct,
               values,
               values_count,
               arity,
               name ); 
    return (lobj){
        .type=LVAL_OBJ,
        .v.obj=(vm_object*)fn
    };
}



void lobj_free(lobj* o) {
  switch (o->type) {
    /* Do nothing special for number type */
    case LVAL_NUM: break;
    /* For Err or Sym free the string data */
    case LVAL_ERR:  break;
    case LVAL_SYM: break;
    case LVAL_OBJ: {
        int objty = o->v.obj->type;
        switch(objty) {
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
                vm_function* fn=(vm_function*)o->v.obj;
                free(fn->name.v.obj);
                free(fn);
            } break;
            case OBJ_USR: {
            } break;
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
