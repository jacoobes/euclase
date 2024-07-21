#include "./vm.h"
#include "mpc.h"
#include <string.h>

static int apply(VM* vm, lobj* obj, int argc) {
    if(obj->type != LVAL_OBJ) {
        puts("Object is not callable"); 
        exit(1);
    }
    if(obj->v.obj->type != 1) {
        puts("Can only call functions");
        exit(1);
    }
    vm_function* fn = (vm_function*) obj->v.obj;
    if(fn->arity != argc) {
        puts("Invalid arity of function");
        exit(1);
    }

    if (vm->frame_count == FRAMES_MAX) {
        puts("stack overflow");
        exit(1);
    }
    //top of call_stack is now the new function after being called
    vm->call_stack[vm->frame_count] = (CallFrame){ 
        .ip=fn->chunk,
        .function=fn,
        // slots is the constants on the stack which this callframe can use. 
        // we set it to the beginning of where we can use it
        .slots=vm->stack_top - argc - 1 
    };
    
    vm->frame_count += 1;
    return 1;
}


void reset_stack(VM* vm) {
    vm->stack_top = vm->stack;
}
void vm_create(VM* vm,
               int *globals, //globals
               int nglobals, //num globals
               lobj* main) {
    vm->globals = globals;
    vm->nglobals = nglobals;

    reset_stack(vm);
    vm->frame_count=0;

    if(apply(vm, main, 0) != 1) {
        printf("main fn failed");
        exit(1);
    }
}


void vm_push(VM *vm, lobj value) {
    *vm->stack_top = value;
    vm->stack_top++;
}

lobj vm_pop(VM *vm) {
    vm->stack_top--;
    // get the element at top of stack.
    return *vm->stack_top;
}

lobj vm_peek(VM* vm, int distance) {
    return vm->stack_top[-1 - distance];
}

lobj lobj_str(char * contents, int len) {
    //FIX ME: PROPERLY MANAGE MEMORY
    vm_string* s = malloc(sizeof(vm_string));
    //FIX ME: PROPERLY MANAGE MEMORY
    s->contents = malloc(len + 1);

    strncpy(s->contents, contents, len);

    s->contents[len] = '\0'; 
    s->len = len;

    s->obj.type=0;
    return (lobj) {
        .type=LVAL_OBJ,
        .v.obj=(vm_object*)s
    };
}

/* Create a new number type lval */
lobj lobj_num(double x) {
  return (lobj){.type=LVAL_NUM, .v.num = x };
}

lobj lobj_func(uint8_t* initial_code,
               uint8_t code_ct,
               lobj* values,
               int values_count,
               uint8_t arity,
               lobj name) {
    //FIX ME: PROPERLY MANAGE MEMORY
    vm_function* fn = malloc(sizeof(vm_function));

    fn->obj.type=OBJ_FUNC;

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


/* Create a new error type lobj */
lobj lobj_err(int x) {
  lobj o;
  o.type = LVAL_ERR ;
  o.v.err = x;
  return o;
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
            case 0 : break;
            case 1 : {
                free(o->v.obj);
            } break;
        }
    } break;
  }
  /* Free the memory allocated for the "lval" struct itself */
  /* when we create heap objs this will be necessary*/
  //free(o);
}

/* Print an "lobj" */
void lobj_print(lobj o) {
  switch (o.type) {
    /* In the case the type is a number print it */
    /* Then 'break' out of the switch. */
    case LVAL_NUM: printf("(num) %.2f", o.v.num); break;

    /* In the case the type is an error */
    case LVAL_ERR:
      /* Check what type of error it is and print it */
      if (o.v.err == LERR_DIV_ZERO) { printf("Error: Division By Zero!"); }
      if (o.v.err == LERR_BAD_OP)   { printf("Error: Invalid Operator!"); }
      if (o.v.err == LERR_BAD_NUM)  { printf("Error: Invalid Number!"); }
    break;

    case LVAL_SYM: {
        printf("sym");
    } break;
    case LVAL_OBJ: {
        switch(o.v.obj->type) {
            case OBJ_FUNC: {
                vm_function* fn = ((vm_function*)o.v.obj);
                printf("func %s(arity=%d)", ((vm_string*)fn->name.v.obj)->contents, fn->arity);
            }break;
            default: printf("obj, yo no se");
        }
    } break;
    default: {
        printf("Unknown lobj to print");
    }

  }
}

/* Print an "lobj" followed by a newline */
void lobj_println(lobj v) { lobj_print(v); putchar('\n'); }

unsigned short parse_ushort (uint8_t* const chunk, size_t location) {
    return (chunk[location-1] << 8) | chunk[location];
}



void vm_exec(VM *vm, int flags) {
    // framecount will always have one, the main function
    CallFrame* cur_frame = &vm->call_stack[vm->frame_count - 1]; 

    #define READ_BYTE() *cur_frame->ip++
    #define CONSTANT_AT(idx) cur_frame->function->values[idx]

    while (1) {
        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
            case I_HALT: {
                printf("HALTED:\n");
                for(int i = 0; &vm->stack[i] != vm->stack_top; i++) {
                    printf("\tSTACK %d ", i); lobj_println(vm->stack[i]);
                }
                puts("");
            } return;
            case I_LOAD: {
                int const_idx = READ_BYTE(); 
#ifdef DEBUG
                printf("I_LOAD "); lobj_println(CONSTANT_AT(const_idx));
#endif
                if(const_idx > cur_frame->function->values_ct) {
                    puts("const idx exceeds value count");
                    exit(1);
                }
                vm_push(vm, CONSTANT_AT(const_idx));
            } break;
            case I_NEGATE:{
                lobj a = vm_pop(vm);
                if (a.type == LVAL_NUM) {
                    vm_push(vm, (lobj){LVAL_NUM, {.num = -a.v.num}});
                } else {
                    puts("typeerror");
                    exit(1);
                }
            } break;
            case I_ADD: {
                lobj b = vm_pop(vm); lobj a = vm_pop(vm);
                if (a.type == LVAL_NUM && b.type == LVAL_NUM) {
                    lobj result = lobj_num(a.v.num + b.v.num);
                    printf("ADD: "); lobj_println(result);
                    vm_push(vm, result);
                } else {
                    puts("typeerror");
                    exit(1);
                }
            } break;
            case I_SUB: {
                lobj b = vm_pop(vm); lobj a = vm_pop(vm);
                if (a.type == LVAL_NUM && b.type == LVAL_NUM) {
                    vm_push(vm, lobj_num(a.v.num - b.v.num));
                } else {
                    puts("typeerror");
                    exit(1);
                }
            } break;
            case I_MUL: {
                lobj b = vm_pop(vm); lobj a = vm_pop(vm);
                if (a.type == LVAL_NUM && b.type == LVAL_NUM) {
                    vm_push(vm, lobj_num(a.v.num * b.v.num));
                } else {
                    puts("typeerror");
                    exit(1);
                }
            } break;
            case I_DIV: {
                lobj b = vm_pop(vm); lobj a = vm_pop(vm);
                if (a.type == LVAL_NUM && b.type == LVAL_NUM) {
                    vm_push(vm, lobj_num(a.v.num / b.v.num));
                } else {
                    puts("typeerror");
                    exit(1);
                }
            } break;
            case I_MOD: {
                lobj b = vm_pop(vm); lobj a = vm_pop(vm);
                if (a.type == LVAL_NUM && b.type == LVAL_NUM) {
                    lobj result = {LVAL_NUM, {.num = (int)a.v.num % (int)b.v.num}};
                    vm_push(vm, result);
                } else {
                    puts("typeerror");
                    exit(1);
                }
            } break;
            case I_CALL: {
                uint8_t argc = READ_BYTE();
                printf("argc %d\n", argc);
                vm->frame_count++;
                lobj maybe_callable = vm_peek(vm, argc);
                printf("About to call function "); lobj_println(maybe_callable);
                if(!apply(vm, &maybe_callable, argc)) { 
                    printf("Call failed");
                    exit(1);
                }
                cur_frame = &vm->call_stack[vm->frame_count - 1];
                
            } break;
            case I_JMP: {
                cur_frame->ip += 2;
                unsigned short jump = parse_ushort(cur_frame->ip, *cur_frame->ip);
                cur_frame->ip += jump;
            } break;
            case I_GET_LOCAL: {
                uint8_t slot = READ_BYTE();
                vm_push(vm, cur_frame->slots[slot]);
#ifdef DEBUG
                printf("GET LOCAL %d ", slot); lobj_println(vm->stack[slot]);
#endif
            } break;
            case I_SET_LOCAL: {
                uint8_t slot = READ_BYTE();
                lobj v  = vm_peek(vm, 0);
#ifdef DEBUG
                printf("SET LOCAL %d  ", slot); lobj_println(v);
#endif
                //given a stack index, set the value on the stack at slot to where stack_top is currently 
                cur_frame->slots[slot] = v;
            } break;
            case I_POP: { vm_pop(vm); }  break;
            case I_RETURN: {
                lobj value = vm_pop(vm);
#ifdef DEBUG
                printf("I_RETURN "); lobj_println(value);
#endif
                vm->frame_count--;
                if(vm->frame_count == 0) {
                    vm_pop(vm);
                    exit(0);
                }
                // push to top of stack
                vm->stack_top = cur_frame->slots;
                vm_push(vm, value);
            } break;
            default: {
                puts("Unknown opcode, misalignment?");
                exit(1);
            }
        }
    }
    #undef READ_BYTE
    #undef CONSTANT_AT

}
