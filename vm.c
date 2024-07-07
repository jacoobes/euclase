#include "./vm.h"
#include "mpc.h"
#include <string.h>

void vm_push(VM *vm, lobj value);

void vm_create(VM* vm,
               int *globals, //globals
               int nglobals, //num globals
               lobj* main) {
    vm->globals = globals;
    vm->nglobals = nglobals;


    vm->sp=0;
    vm->frame_count=0;
    if(main->type != LVAL_OBJ) {
        printf("Main lobj is not an object. needs to be a function");
        exit(1);
    }
    if(main->v.obj->type != 1) {
        printf("Main lobj is not a function");
        exit(1);
    }
    vm_function* fn = (vm_function*)main->v.obj;
    vm_push(vm, *main);
    //set the main funciton as first in the callstack
    vm->call_stack[0] = (CallFrame){
        .ip=fn->chunk,
        .slots=vm->stack, //values is pointer to first element in array. 
        .function=fn
    };
    vm->frame_count += 1;
}


void vm_push(VM *vm, lobj value) {
    vm->stack[++vm->sp] = value;
}

lobj vm_pop(VM *vm) {
    // get the element at top of stack.
    return vm->stack[vm->sp--];
}


/* Create a new number type lval */
lobj lobj_num(double x) {
  return (lobj){.type=LVAL_NUM, .v.num = x };
}

lobj lobj_func(uint8_t* initial_code, uint8_t code_ct, lobj* values, int values_count) {
    vm_function* fn = malloc(sizeof(vm_function));
    fn->obj.type=1;
    fn->chunk=initial_code;
    fn->chunk_ct=code_ct;
    fn->values=values;
    fn->values_ct=values_count;
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



void compiler_create(Compiler* compiler) {
    compiler->values = malloc(sizeof(lobj) * 256);
    compiler->values_count = 0;
    compiler->val_cap = 256;

    compiler->code = (uint8_t*)malloc(sizeof(uint8_t) * 1024);
    compiler->code_count = 0;
    compiler->code_cap = 1024;

    compiler->local_count = 0;
    compiler->scope_depth = 0;
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

void compiler_free(Compiler* c) {
    if (c == NULL) return;
    // Free the code array
    if (c->code != NULL) {
        free(c->code);
    }

    // Free the constants array
    if (c->values != NULL) {
        // If any constant contains dynamically allocated memory (like strings),
        // we need to free that first
        for (int i = 0; i < c->values_count ; i++) {
            lobj_free(&c->values[i]);
        }
        free(c->values);
    }
    // Free the compiler struct itself
    free(c);
}

int compiler_add_constant(Compiler* c, lobj constant) {
//    for (int i = 0; i < c->constant_count; i++) {
//        if (lobj_eq(&c->values[i], &constant)) {
//            return i;  // Constant already exists
//        }
//    }
    
    if (c->values_count >= c->val_cap) {
        c->val_cap *= 2;
        c->values = realloc(c->values, sizeof(lobj) * c->val_cap);
    }
    
    int index = c->values_count++;
    c->values[index] = constant;
    return index;
}

void compiler_emit(Compiler* c, uint8_t byte) {
    if (c->code_count >= c->code_cap) {
        c->code_cap *= 2;
        c->code = realloc(c->code, sizeof(uint8_t) * c->code_cap);
    }
    c->code[c->code_count++] = byte;
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
        printf("obj");
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

static int apply(lobj obj, int argc) {
    if(obj.type != LVAL_OBJ) {
        puts("Object is not callable"); 
        exit(1);
    }
    switch(obj.v.obj->type) {
        case 1 : { 
            
           // todo 
        }break;
    }
    return 0;
}

void vm_exec(VM *vm, int flags) {
    // framecount will always have one
    CallFrame* cur_frame = &vm->call_stack[vm->frame_count - 1]; 
    #define READ_BYTE() *cur_frame->ip++
    while (1) {
        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
            case I_HALT: {
                printf("HALTED:\n");
                for(int i = 0; i <= vm->sp; i++) {
                    printf("\tSTACK %d ", i); lobj_println(vm->stack[i]);
                }
                puts("");
            } return;
            case I_LOAD: {
                int const_idx = READ_BYTE(); 
                printf("I_LOAD "); lobj_println(cur_frame->slots[const_idx]);
                if(const_idx > cur_frame->function->values_ct) {
                    puts("const idx exceeds value count");
                    exit(1);
                }
                vm_push(vm, cur_frame->function->values[const_idx]);
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
                    printf("\tSET sp %d \n", vm->sp+1);
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
                vm->frame_count++;
                lobj val = vm_pop(vm);
                int result = apply(val, argc);
                if(result) { 
                    cur_frame = &vm->call_stack[vm->frame_count - 1 ];
                } else {
                    printf("Call failed");
                    exit(1);
                }
            } break;
            case I_JMP: {
                cur_frame->ip += 2;
                unsigned short jump = parse_ushort(cur_frame->ip, *cur_frame->ip);
                cur_frame->ip += jump;
            } break;
            case I_GET_LOCAL: {
                uint8_t slot = READ_BYTE();
                printf("GET LOCAL %d ", slot); lobj_println(vm->stack[slot]);
                vm_push(vm, cur_frame->slots[slot]);
            } break;
            case I_SET_LOCAL: {
                uint8_t slot = READ_BYTE();
                printf("SET LOCAL %d  ", slot); lobj_println(vm->stack[vm->sp]);
                //given a stack index, set the value on the stack at slot to where vm->sp is currently 
                cur_frame->slots[slot] = vm->stack[vm->sp];
            } break;
            case I_POP: vm_pop(vm);  break;
            default: {
                printf("Unknown opcode, misalignment?\n");
                exit(1);
            }
        }
    }
    #undef READ_BYTE

}
