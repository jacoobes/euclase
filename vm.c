#include "./vm.h"
#include "mpc.h"
#include <string.h>


void vm_create(
        VM* vm,
        uint8_t *code,
        int code_count, 
        int *globals, //globals
        int nglobals, //num globals
        lobj* values, //constants
        int values_count //num constants
    ) {
    vm->code = code;
    vm->code_count = code_count;
    vm->globals = globals;
    vm->nglobals = nglobals;

    vm->values = values;
    vm->values_count = values_count;
    vm->sp = 0;
    vm->ip = 0;
    printf("VM SP INITIALIZED %d\n", vm->sp); 
}


void vm_push(VM *vm, lobj value) {
    vm->stack[++vm->sp] = value;
}

lobj vm_pop(VM *vm) {
    return vm->stack[vm->sp--];
}

int vm_add_frame(VM* vm, CallFrame* frame) {
    if (vm->frame_count >= DEFAULT_CALL_STACK_SIZE) {
        printf("Stack overflow\n");
        exit(1);
    }
    int index = vm->frame_count++;
    vm->call_stack[index] = *frame;
    return index;
}

/* Create a new number type lval */
lobj lobj_num(double x) {
  return (lobj){.type=LVAL_NUM, .v.num = x };
}

lobj lobj_func() {
    return (lobj){.type=LVAL_FUNC };
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
    case LVAL_NUM: printf("%.2f", o.v.num); break;

    /* In the case the type is an error */
    case LVAL_ERR:
      /* Check what type of error it is and print it */
      if (o.v.err == LERR_DIV_ZERO) { printf("Error: Division By Zero!"); }
      if (o.v.err == LERR_BAD_OP)   { printf("Error: Invalid Operator!"); }
      if (o.v.err == LERR_BAD_NUM)  { printf("Error: Invalid Number!"); }
    break;
  }
}

/* Print an "lobj" followed by a newline */
void lobj_println(lobj v) { lobj_print(v); putchar('\n'); }

unsigned short parse_ushort (uint8_t* const chunk, size_t location) {
    return (chunk[location-1] << 8) | chunk[location];
}

void run(VM* vm, vm_function* fn) {
    CallFrame* frame = &vm->call_stack[vm->frame_count++];
    frame->function = fn;
    frame->slots = vm->stack;
}

void vm_exec(VM *vm, int start_ip, int flags) {
    
    CallFrame* cur_frame = &vm->call_stack[vm->frame_count - 1]; // framecount will always have one
    while (vm->ip < vm->code_count) {
        int instruction = vm->code[vm->ip];
        switch (instruction) {
            case I_HALT: {
                printf("HALTED:\n");
                for(int i = 0; i < vm->sp; i++) {
                    printf("STACK %d", i);lobj_println(vm->stack[i]);
                }
                puts("");
            } return;
            case I_LOAD: {
                int const_idx = cur_frame->function->chunk[*(++cur_frame->ip)]; 
                if(const_idx > vm ->values_count) {
                    puts("const idx exceeds value count");
                    exit(1);
                }
                vm_push(vm, cur_frame->slots[const_idx]);
            } break;
            case I_NEGATE:{
                lobj a = vm_pop(vm);
                if (a.type == LVAL_NUM) {
                    lobj result = {LVAL_NUM, {.num = -a.v.num}};
                    vm_push(vm, result);
                } else {
                    puts("typeerror");
                    exit(1);
                }
            } break;
            case I_ADD: {
                lobj b = vm_pop(vm); lobj a = vm_pop(vm);
                if (a.type == LVAL_NUM && b.type == LVAL_NUM) {
                    lobj result = {LVAL_NUM, {.num = a.v.num + b.v.num}};
                    printf("RESULT ADD:");lobj_println(result);
                    vm_push(vm, result);
                } else {
                    puts("typeerror");
                    exit(1);
                }
            } break;
            case I_SUB: {
                lobj b = vm_pop(vm); lobj a = vm_pop(vm);
                if (a.type == LVAL_NUM && b.type == LVAL_NUM) {
                    lobj result = {LVAL_NUM, {.num = a.v.num - b.v.num}};
                    vm_push(vm, result);
                } else {
                    puts("typeerror");
                    exit(1);
                }
            } break;
            case I_MUL: {
                lobj b = vm_pop(vm); lobj a = vm_pop(vm);
                if (a.type == LVAL_NUM && b.type == LVAL_NUM) {
                    lobj result = {LVAL_NUM, {.num = a.v.num * b.v.num}};
                    vm_push(vm, result);
                } else {
                    puts("typeerror");
                    exit(1);
                }
            } break;
            case I_DIV: {
                lobj b = vm_pop(vm); lobj a = vm_pop(vm);
                if (a.type == LVAL_NUM && b.type == LVAL_NUM) {
                    lobj result = {LVAL_NUM, {.num = a.v.num / b.v.num}};
                    vm_push(vm, result);
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
                int argc = vm->code[++vm->ip];
                vm->frame_count ++;
                //TODO, implement funciton calling
            } break;
            case I_JMP: {
                vm->ip += 2;
                unsigned short jump = parse_ushort(vm->code, vm->ip);
                vm->ip += jump;
            } break;
            case I_GET_LOCAL: {
                uint8_t slot = vm->code[++vm->ip];
                printf("GET LOCAL %d ", slot); lobj_println(vm->stack[slot]);
                vm_push(vm, cur_frame->slots[slot]);
            } break;
            case I_SET_LOCAL: {
                uint8_t slot = vm->code[++vm->ip];
                printf("SET LOCAL %d,  ", slot); lobj_println(vm->stack[vm->sp]);
                //given a stack index, set the value on the stack at slot to where vm->sp is currently 
                cur_frame->slots[slot] = vm->stack[vm->sp];
            } break;
            case I_POP: vm_pop(vm);  break;
            default:
                printf("Unknown opcode, misalignment?\n");
                exit(1);
        }
        vm->ip++;  // Move to next instruction
    }

}
