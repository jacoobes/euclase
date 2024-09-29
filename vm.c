#include "./vm.h"
#include <stdio.h>
#include <string.h>
#include "./lobj.h"
#include "table.h"


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
void vm_create(VM* vm, lobj* main) {
    reset_stack(vm);
    vm->frame_count=0;
    table_create(&vm->globals);
    table_create(&vm->strings);
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
                    printf("\tSTACK %d ", i); lobj_println(&vm->stack[i]);
                }
                puts("");
            } return;
            case I_LOAD: {
                int const_idx = READ_BYTE(); 
#ifdef DEBUG
                printf("I_LOAD "); lobj_println(&CONSTANT_AT(const_idx));
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
                if (IS_NUM(a) && IS_NUM(b)) {
                    lobj result = lobj_num(a.v.num + b.v.num);
                    printf("ADD: "); lobj_println(&result);
                    vm_push(vm, result);
                } else {
                    puts("typeerror");
                    exit(1);
                }
            } break;
            case I_SUB: {
                lobj b = vm_pop(vm); lobj a = vm_pop(vm);
                if (IS_NUM(a) && IS_NUM(b)) {
                    vm_push(vm, lobj_num(a.v.num - b.v.num));
                } else {
                    puts("typeerror");
                    exit(1);
                }
            } break;
            case I_MUL: {
                lobj b = vm_pop(vm); lobj a = vm_pop(vm);
                if (IS_NUM(a) && IS_NUM(b)) {

                    vm_push(vm, lobj_num(a.v.num * b.v.num));
                } else {
                    puts("typeerror");
                    exit(1);
                }
            } break;
            case I_DIV: {
                lobj b = vm_pop(vm); lobj a = vm_pop(vm);
                if (IS_NUM(a) && IS_NUM(b)) {
                    vm_push(vm, lobj_num(AS_NUM(a) / AS_NUM(b)));
                } else {
                    puts("typeerror");
                    exit(1);
                }
            } break;
            case I_MOD: {
                lobj b = vm_pop(vm); lobj a = vm_pop(vm);
                if (IS_NUM(a) && IS_NUM(b)) {
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
                lobj maybe_callable = vm_peek(vm, argc);
#ifdef DEBUG
                printf("argc %d\n", argc);
                printf("About to call function "); lobj_println(&maybe_callable);
#endif
                if(!apply(vm, &maybe_callable, argc)) { 
                    printf("Call failed");
                    exit(1);
                }
                cur_frame = &vm->call_stack[vm->frame_count - 1];
                
            } break;
            case I_JMP: {
                //advance current frame's instruction pointer by two bytes (uint16_t)
                cur_frame->ip += 2; 
                unsigned short jump = parse_ushort(cur_frame->ip, *cur_frame->ip);
                //after parsing the jump, we advance the instruction pointer by that
                cur_frame->ip += jump;
            } break;
            case I_JMP_FALSY: {
                lobj lval = vm_pop(vm);
                if(IS_NUM(lval) && !lval.v.num) {
                    cur_frame->ip += 2; 
                    unsigned short jump = parse_ushort(cur_frame->ip, *cur_frame->ip);
                    cur_frame->ip += jump;
                }
            } break;
            case I_GET_LOCAL: {
                uint8_t slot = READ_BYTE();
                vm_push(vm, cur_frame->slots[slot]);
#ifdef DEBUG
                printf("GET LOCAL %d ", slot); lobj_println(&vm->stack[slot]);
#endif
            } break;
            case I_SET_LOCAL: {
                uint8_t slot = READ_BYTE();
                lobj v  = vm_peek(vm, 0);
#ifdef DEBUG
                printf("SET LOCAL %d  ", slot); lobj_println(&v);
#endif
                //given a stack index, set the value on the stack at slot to where stack_top is currently 
                cur_frame->slots[slot] = v;
            } break;
            case I_POP: { vm_pop(vm); }  break;
            case I_RETURN: {
                lobj value = vm_pop(vm);
#ifdef DEBUG
                printf("I_RETURN "); lobj_println(&value);
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

void vm_free(VM* vm) {
    exit(1);    
}
