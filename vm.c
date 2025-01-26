#include "./vm.h"
#include <stdio.h>
#include <stdlib.h>
#include "./lobj.h"
#include "table.h"


static int apply(VM* vm, vm_closure* obj, int argc) {
    vm_function* fn = obj->fn;
    if(fn->arity != argc) {
        printf("Invalid arity of function "); lobj_println(&fn->name);
        return 1;
    }

    if (vm->frame_count == FRAMES_MAX) {
        puts("stack overflow");
        return 1;
    }
    //top of call_stack is now the new function after being called
    vm->call_stack[vm->frame_count] = (CallFrame){ 
        .ip=fn->chunk,
        .closure=obj,
        // slots is the constants on the stack which this callframe can use. 
        // we set it to the beginning of where we can use it
        .slots=vm->stack_top - argc - 1 
    };
    
    vm->frame_count += 1;
    return 0;
}


static void reset_stack(VM* vm) {
    vm->stack_top = vm->stack;
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

void vm_create(VM* vm, HashTable* globals, HashTable* strings, lobj* main) {
    reset_stack(vm);
    vm->frame_count=0;
    // tables should be already init'd
    vm->globals = globals;
    vm->strings = strings;

    if(!is_closure(main)) {
        fprintf(stderr, "Main is not callable\n");
        exit(1);
    }
    vm_closure* main_closure = as_closure(main);
    if(apply(vm, main_closure, 0) != 0) {
        printf("main fn failed");
    }
}



/*
 * Returns a heap allocated string of stack trace.
 */
solid_str vm_stacktrace(VM* vm, char* error) {
    solid_str stack_trace = solid_str_new(NULL);
    printf("Error: stacktrace(frames=%d)\n", vm->frame_count);
    for(int i = 0; i < vm->frame_count; i++) {
        solid_str fnname = AS_STR(vm->call_stack[i].closure->fn->name)->contents;

        solid_str_appends(&stack_trace, "\tIn \t");
        solid_str_appends(&stack_trace, SOLID_STR_TO_C(fnname));
        solid_str_appends(&stack_trace, "\n");

        if(i == 0) {
            solid_str_appends(&stack_trace,  error);
            solid_str_appends(&stack_trace,  "\n");
        }
    }
    return stack_trace;
}




int vm_exec(VM *vm, int flags) {
    // framecount will always have one, the main function
    CallFrame* cur_frame = &vm->call_stack[vm->frame_count - 1]; 

    #define READ_BYTE() *cur_frame->ip++
    #define CONSTANT_AT(idx) cur_frame->closure->fn->values[(idx)]
    #define READ_SHORT() \
        (cur_frame->ip += 2, \
            (uint16_t)(cur_frame->ip[-2] << 8 | cur_frame->ip[-1]))

    while (1) {
        uint8_t instruction;
        switch ((instruction = READ_BYTE()) ) {
            case I_HALT: {
                printf("HALTED:\n");
                for(int i = 0; &vm->stack[i] != vm->stack_top; i++) {
                    printf("\tSTACK %d ", i); lobj_println(&vm->stack[i]);
                }
                puts("");
            } return 0;
            case I_LOAD: {
                int const_idx = READ_BYTE(); 
#ifdef DEBUG
                printf("I_LOAD "); lobj_println(&CONSTANT_AT(const_idx));
#endif
                if(const_idx > cur_frame->closure->fn->values_ct) {
                    char* err = "const idx exceeds value count";
                    printf("%s", SOLID_STR_TO_C(vm_stacktrace(vm, err)));
                    return 1;
                }
                vm_push(vm, CONSTANT_AT(const_idx));
            } break;
            case I_NEGATE: {
                lobj a = vm_pop(vm);
                if (a.type == LVAL_NUM) {
                    vm_push(vm, (lobj){LVAL_NUM, {.num = -a.v.num}});
                } else {
                    printf("%s", SOLID_STR_TO_C(vm_stacktrace(vm, "Typeerror: Negate")));
                    return 1;
                }
            } break;
            case I_ADD: {
                lobj b = vm_pop(vm); lobj a = vm_pop(vm);
                if (IS_NUM(a) && IS_NUM(b)) {
                    lobj result = lobj_num(a.v.num + b.v.num);
                    printf("ADD: "); lobj_println(&result);
                    vm_push(vm, result);
                } else {
                    printf("%s", SOLID_STR_TO_C(vm_stacktrace(vm, "Typeerror ADD")));
                    return 1;
                }
            } break;
            case I_SUB: {
                lobj b = vm_pop(vm); lobj a = vm_pop(vm);
                if (IS_NUM(a) && IS_NUM(b)) {
#ifdef DEBUG
                    printf("SUB: "); lobj_println(&b);
#endif
                    vm_push(vm, lobj_num(a.v.num - b.v.num));
                } else {
                    printf("%s", SOLID_STR_TO_C(vm_stacktrace(vm, "Typeerror SUB")));
                    return 1;
                }
            } break;
            case I_MUL: {
                lobj b = vm_pop(vm); lobj a = vm_pop(vm);
                if (IS_NUM(a) && IS_NUM(b)) {

                    vm_push(vm, lobj_num(a.v.num * b.v.num));
                } else {
                    printf("%s", SOLID_STR_TO_C(vm_stacktrace(vm, "Typeerror MUL")));
                    return 1;
                }
            } break;
            case I_DIV: {
                lobj b = vm_pop(vm); lobj a = vm_pop(vm);
                if (IS_NUM(a) && IS_NUM(b)) {
                    vm_push(vm, lobj_num(AS_NUM(a) / AS_NUM(b)));
                } else {
                    printf("%s", SOLID_STR_TO_C(vm_stacktrace(vm, "Typeerror MUL")));
                    return 1;
                }
            } break;
            case I_MOD: {
                lobj b = vm_pop(vm); lobj a = vm_pop(vm);
                if (IS_NUM(a) && IS_NUM(b)) {
                    lobj result = {LVAL_NUM, {.num = (int)a.v.num % (int)b.v.num}};
                    vm_push(vm, result);
                } else {
                    printf("%s", SOLID_STR_TO_C(vm_stacktrace(vm, "Typeerror MOD")));
                    return 1;
                }
            } break;
            case I_LTE : {
                lobj b = vm_pop(vm); lobj a = vm_pop(vm);
                if (IS_NUM(a) && IS_NUM(b)) {
                    lobj result = {LVAL_NUM, {.num = (a.v.num <= b.v.num)}};
                    vm_push(vm, result);
                } else {
                    printf("%s", SOLID_STR_TO_C(vm_stacktrace(vm, "Typeerror LTE")));
                    return 1;
                }
            } break;
            case I_GT : {
                lobj b = vm_pop(vm); lobj a = vm_pop(vm);
                if (IS_NUM(a) && IS_NUM(b)) {
                    lobj result = {LVAL_NUM, {.num = (a.v.num <= b.v.num)}};
                    vm_push(vm, result);
                } else {
                    printf("%s", SOLID_STR_TO_C(vm_stacktrace(vm, "Typeerror LTE")));
                    return 1;
                }
            } break;
            case I_CALL: {
                // arg count is operand.
                uint8_t argc = READ_BYTE();
                lobj maybe_callable = vm_peek(vm, argc);
#ifdef DEBUG
                printf("argc %d\n", argc);
                printf("About to call function "); lobj_println(&maybe_callable);
#endif
                if(!is_closure(&maybe_callable)) {
                    solid_str stack_trace = vm_stacktrace(vm, "Uncallable lobj -> needs closure");
                    printf("%s", SOLID_STR_TO_C(stack_trace));
                    return 1;
                }
                vm_closure* closure = as_closure(&maybe_callable);
                // apply: we take the maybe callable
                if(apply(vm, closure, argc)) { 
                    //strlcpy(vm->error,, sizeof vm->error);
                    solid_str stack_trace = vm_stacktrace(vm, "Failed to call an lobj");
                    printf("%s", SOLID_STR_TO_C(stack_trace));
                    return 1;
                }
                // afterwards, the current callstack is set to one less than previous.
                cur_frame = &vm->call_stack[vm->frame_count - 1];
            } break;
            case I_JMP: {
                //advance current frame's instruction pointer by two bytes (uint16_t)
                uint16_t jump = READ_SHORT();
                //after parsing the jump, we advance the instruction pointer by that
                cur_frame->ip += jump;
            } break;
            case I_JMP_FALSY: {
                lobj lval = vm_peek(vm, 0);
                uint16_t jump = READ_SHORT(); 
#ifdef DEBUG
                printf("I_JMP_FALSY "); 
                lobj_print(&lval);
                printf(" %d\n", jump); 
#endif

                if(IS_NUM(lval) && !lval.v.num) {
                    // increment the pointer by jump
                    cur_frame->ip += jump;
                }
            } break;
            case I_GET_LOCAL: {
                uint8_t slot = READ_BYTE();
                vm_push(vm, cur_frame->slots[slot]);
#ifdef DEBUG
                printf("GET_LOCAL %d ", slot); lobj_println(&vm->stack[slot]);
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

                vm->frame_count--;
#ifdef DEBUG
                printf("frame_count %d \n", vm->frame_count);
                printf("I_RETURN "); lobj_println(&value);
#endif
                
                //return from last frame
                if(vm->frame_count == 0) {
                    vm_pop(vm);
                    return 0;
                }
                // push to top of stack
                vm->stack_top = cur_frame->slots;
                vm_push(vm, value);
                cur_frame = &vm->call_stack[vm->frame_count - 1];
            } break;
            case I_LOOP: {
                uint16_t jump = READ_SHORT();
#ifdef DEBUG
                printf("I_LOOP %zu\n", jump);
#endif
                // when going backwards in the opcode, you must ensure that you account for the offset of 3 
                // for the 2 bytes in the short and the LOOP instruction (one byte)
                cur_frame->ip -= jump;
            } break;
            case I_CLOSURE: {
                //pointer to existing function
                vm_function* fn = AS_FUNC(vm_pop(vm));
                //create new closure from existing fn
                vm_closure* closure = vm_closure_create(fn);
                //create lobj 
                vm_push(vm, into_lobj(LVAL_OBJ, closure));
            } break;
            case I_GET_GLOBAL: {
                uint8_t slot = READ_BYTE();
                // the name of our string is reference in bytecode's constants by index
                vm_string *name = AS_STR(CONSTANT_AT(slot));
                vm_string* dupe = table_find_string(vm->strings, &name->contents, name->hash);
                if(dupe) {
                    name=dupe;
                } else {
                    table_set(vm->strings, name, lobj_num(slot));
                }
#ifdef DEBUG
                printf("GET GLOBAL "); printf("%s\n", SOLID_STR_TO_C(name->contents));
#endif
                lobj v;
                if(!table_get(vm->globals, name, &v)) {
                    printf("variable not found");
                    //variable not found
                    return 1;
                }
                vm_push(vm, v);
            } break;
            // I_SET_GLOBAL VALUE NAME
            case I_SET_GLOBAL: {
                // the name of the variable
                uint8_t peek_vl = READ_BYTE();
                vm_string *name = AS_STR(CONSTANT_AT(peek_vl));
#ifdef DEBUG
                printf("I_SET_GLOBAL "); printf("%s ", SOLID_STR_TO_C(name->contents));
#endif

                //check intern
                vm_string* dupe = table_find_string(vm->strings, &name->contents, name->hash);
                if(dupe) {
                    name=dupe;
                } else {
                    table_set(vm->strings, name, lobj_num(peek_vl));
                }
                lobj value = vm_peek(vm, 0);
#ifdef DEBUG
                lobj_println(&value);
#endif

                // set value
                table_set(vm->globals, name, value);
                vm_pop(vm);
            } break;

            default: {
                printf("Unknown opcode '%c', misalignment?", instruction);
                return 1;
            }
        }
    }
    #undef READ_BYTE
    #undef READ_SHORT
    #undef CONSTANT_AT

}

void vm_free(VM* vm) {
    printf("Need to manage vm memory\n");
}
