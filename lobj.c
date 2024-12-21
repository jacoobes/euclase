#include "lobj.h"
#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint32_t hash_str(const char * contents, int len) {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < len; i++) {
        hash ^= (uint8_t)contents[i];
        hash *= 16777619;
    }
    return hash;
}

// NOTE: contents MUST BE NULL TERMINATED
lobj lobj_str(const char* contents) {
    int lencontents = strlen(contents);
    vm_string* s = ALLOCATE(vm_string, 1);
    s->contents = malloc(lencontents + 1);
    strncpy(s->contents, contents, lencontents);
    s->contents[lencontents] = '\0'; 
    s->len = lencontents;
    s->hash= hash_str(contents, lencontents);
    s->obj.type=OBJ_STR;
    return (lobj) {
        .type=LVAL_OBJ,
        .v.obj=(vm_object*)s
    };
}

/* Create a new number type lval */
lobj lobj_num(double x) {
  return (lobj){.type=LVAL_NUM, .v.num = x };
}

void vm_function_create(vm_function* fn) {
    fn->obj.type=OBJ_FUNC;
}

lobj lobj_func(uint8_t* initial_code,
               uint8_t code_ct,
               lobj* values,
               int values_count,
               uint8_t arity,
               lobj name) {
    //FIX ME: PROPERLY MANAGE MEMORY
    vm_function* fn = ALLOCATE(vm_function, 1);
    vm_function_create(fn);

    fn->chunk=initial_code;
    fn->chunk_ct=code_ct;

    fn->values=values;
    fn->values_ct=values_count;

    fn->arity=arity;
    fn->name=name; 
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
                free(fn->contents);
                free(fn);
            } break;
            case OBJ_FUNC : {
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
                vm_function* fn = ((vm_function*)o->v.obj);
                printf("func %s(arity=%d)", ((vm_string*)fn->name.v.obj)->contents, fn->arity);
            }break;
            default: printf("obj, yo no se");
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
