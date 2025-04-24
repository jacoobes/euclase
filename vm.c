#include <stdio.h>
#include <stdlib.h>
#include "table.h"
#include "./vm.h"
#include "./memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void vm_object_init(vm_object* vmobj, LOBJ_TYPE lty) {
   vmobj->is_marked=false; 
   vmobj->type=lty;
   // gc_add_obj adds to the chain of objects
   gc_add_obj(vmobj);
}

vm_upvalue* vm_upvalue_create(lobj* ref) {
    vm_upvalue *up = ALLOCATE(vm_upvalue, 1);
    up->location = ref;
    vm_object_init(&up->obj, OBJ_UPVALUE);
    up->closed = lobj_num(false);
    up->next=NULL;
    return up;
}
vm_string* vm_string_create(const char* contents) {
    vm_string* s = ALLOCATE(vm_string, 1);
    vm_object_init(&s->obj, OBJ_STR);
    s->contents=solid_str_new(contents);
    return s;
}


vm_string* vm_string_from_ss(solid_str ss) {
    vm_string* s = ALLOCATE(vm_string, 1);
    vm_object_init(&s->obj, OBJ_STR);
    s->contents = ss;
    return s;
}

void vm_string_free(vm_string* s) {
    solid_str_free(&s->contents);
    free(s);

}
vm_closure* vm_closure_create(vm_function* fn) {
    vm_closure* closure = ALLOCATE(vm_closure, 1);
    vm_object_init(&closure->obj, OBJ_CLOSURE);
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

    return closure;
}


vm_native* vm_native_create(int argc, native_func fn) {
    vm_native* nativeobj = ALLOCATE(vm_native, 1);
    vm_object_init(&nativeobj->obj, OBJ_NATIVE);

    nativeobj->argc=argc;
    nativeobj->native=fn;
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
   vm_function* fn = ALLOCATE(vm_function, 1);
   vm_object_init(&fn->obj, OBJ_FUNC);
   fn->chunk=initial_code;
   fn->chunk_ct=code_ct;
   fn->values=values;
   fn->values_ct=values_count;
   fn->arity=arity;
   fn->name=name;
   fn->upvalues_count = upvalues_count;
   return fn;
}

void vm_object_free(vm_object* obj) {
    int obj_ty = obj->type;
    switch(obj_ty) {
        case OBJ_STR: { 
            vm_string * fn=(vm_string*)obj;
            solid_str_free(&fn->contents);
            free(fn);
        } break;
        case OBJ_FUNC : {
            vm_function* fn=(vm_function*)obj;
            free(fn->name.v.obj);
            free(fn);
        } break;
        case OBJ_CLOSURE: {
            // todo: do we need to free the function here?
            const vm_closure* closure = (vm_closure*)obj;
            vm_function* fn= closure->fn;
            vm_object_free((vm_object*)fn);
            // we don't free the underlying upvalues, that's up to the garbage collector in the future to do that
            free(closure->upvalues);
        } break;
        case OBJ_UPVALUE: {
            free(obj);
        } break;
        case OBJ_USR: {
        } break;
        default: {
            printf("Unknown object type to free");
        }break;
    }
}

void lobj_free(lobj* o) {
  switch (o->type) {
    /* Do nothing special for number type */
    case LVAL_NUM: break;
    /* For Err or Sym free the string data */
    case LVAL_ERR:  break;
    case LVAL_SYM: break;
    case LVAL_OBJ: {
        vm_object_free(o->v.obj);
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
            case OBJ_NATIVE: { 
                printf("<native fn>");
            } break;
            default: { 
                printf("obj, yo no se"); 
            } break;
        }
    } break;
    default: {
        printf("Unknown type %d?", o->type);
    }
  }
}

/* Print an "lobj" followed by a newline */
void lobj_println(lobj* v) { lobj_print(v); putchar('\n'); }

void vm_push(VM *vm, lobj value) {
    *vm->stack_top = value;
    vm->stack_top++;
}

lobj vm_pop(VM *vm) {
    vm->stack_top--;
    // get the element at top of stack.
    return *vm->stack_top;
}

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


lobj vm_peek(const VM* vm, const int distance) {
    return vm->stack_top[-1 - distance];
}


lobj WAF_PRINT(int argc, lobj* args) {
    lobj_println(args);
    return lobj_num(0);
}

void vm_def_native(VM* vm, const char* symbol, vm_native* nativeobj) {
    vm_string* vbs = vm_string_create(symbol);
    vm_string* interned = table_find_string(vm->strings, SOLID_STR_TO_C(vbs->contents), vbs->contents.hash);

    if (interned == NULL) {
        table_set(vm->strings, vbs, lobj_num(1));
        interned = vbs;
    } else {
        vm_string_free(vbs);
    }
    
    vm_push(vm, into_lobj(interned));
    vm_push(vm, into_lobj(nativeobj));
    table_set(vm->globals, AS_STR(vm_pop(vm)), vm_pop(vm));
}

void vm_create(VM* vm, HashTable* globals, HashTable* strings, lobj* main) {
    reset_stack(vm);
    vm->frame_count=0;
    vm->open_upvalues = NULL;
    // tables should be already initiated
    vm->globals = globals;
    vm->strings = strings;
        
    vm->objects = NULL;

    vm->gray_capacity=0;
    vm->gray_count=0;
    vm->gray_stack = NULL;

    if(!is_closure(main)) {
        fprintf(stderr, "Main is not callable, got %d\n", main->type);
        exit(1);
    }
    vm_closure* main_closure = as_closure(main);
    vm->main_fn=main_closure;

    // define print function
    vm_def_native(vm, "print", vm_native_create(1, WAF_PRINT));

    if(apply(vm, main_closure, 0) != 0) {
        printf("main fn failed");
    }
}

static vm_upvalue* captureUpvalue(VM* vm, lobj* local) {
    vm_upvalue* prevUpvalue = NULL;
    vm_upvalue* upvalue = vm->open_upvalues;
    while (upvalue != NULL && upvalue->location > local) {
        prevUpvalue = upvalue;
        upvalue = upvalue->next;
    }

    if (upvalue != NULL && upvalue->location == local) {
        return upvalue;
    }
    vm_upvalue* createdUpvalue = vm_upvalue_create(local);


    createdUpvalue->next = upvalue;
  
    if (prevUpvalue == NULL) {
      vm->open_upvalues = createdUpvalue;
    } else {
      prevUpvalue->next = createdUpvalue;
    }

    return createdUpvalue;
}

static void closeUpvalues(VM* vm, lobj* last) {
  while (vm->open_upvalues != NULL &&
         vm->open_upvalues->location >= last) {
    vm_upvalue* upvalue = vm->open_upvalues;
    upvalue->closed = *upvalue->location;
    upvalue->location = &upvalue->closed;
    vm->open_upvalues = upvalue->next;
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
    #define READ_STRING() AS_STR(CONSTANT_AT(READ_BYTE()))

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
#ifdef DEBUG
                    printf("ADD: "); lobj_println(&result);
#endif
                    vm_push(vm, result);
                } else { printf("b:%d a:%d\n",b.type, a.type);
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
                    lobj result = {LVAL_NUM, {.num = (a.v.num > b.v.num)}};
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
                int res = 1;
                if (is_closure(&maybe_callable)) {
                    vm_closure* closure = as_closure(&maybe_callable);

                    res = apply(vm, closure, argc);
                    // afterward, the current callstack is set to one less than previous.
                    cur_frame = &vm->call_stack[vm->frame_count - 1];
                }
                else if (is_native(&maybe_callable)) {
                    vm_native* nativeobj = as_native(&maybe_callable);

                    lobj result = nativeobj->native(argc, vm->stack_top-argc);

                    vm->stack_top -= argc + 1;
                    vm_push(vm,result);
                    res = 0;
                } else {
                    solid_str stack_trace = vm_stacktrace(vm, "Uncallable lobj -> needs callable object");
                    printf("%s", SOLID_STR_TO_C(stack_trace));
                    return 1;
                }

                if(res != 0) { 
                    solid_str stack_trace = vm_stacktrace(vm, "Failed to call an lobj");
                    printf("%s", SOLID_STR_TO_C(stack_trace));
                    return 1;
                }
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
                printf("I_JMP_FALSY "); lobj_print(&lval); printf(" %d\n", jump); 
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

                closeUpvalues(vm, cur_frame->slots);

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
                printf("I_LOOP %d\n", jump);
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
                vm_push(vm, into_lobj(closure));
                // I_CLOSURE is a variable length instruction, depending on
                // upvalue count.
                for (int i = 0; i < closure->upvalues_ct; i++) {
                    uint8_t isLocal = READ_BYTE();
                    uint8_t index = READ_BYTE();
                    if (isLocal) {
                         closure->upvalues[i] =
                             captureUpvalue(vm,cur_frame->slots + index);
                    } else {
//                        printf("%s\n", SOLID_STR_TO_C(AS_STR(cur_frame->closure->fn->name)->contents));
                        // take upvalues from currently 'executing' function
                        closure->upvalues[i] = cur_frame->closure->upvalues[index];
                    }
                }
            } break;
            case I_GET_GLOBAL: {
                // the name of our string is reference in bytecode's constants by index
                // this string is interned and only 1 instance of it is kept.
                vm_string *name = READ_STRING();
#ifdef DEBUG
                printf("GET GLOBAL "); printf("%s\n", SOLID_STR_TO_C(name->contents));
#endif
                lobj v;
                if(!table_get(vm->globals, name, &v)) {
                    printf("variable '%s' not found", SOLID_STR_TO_C(name->contents));
                    //variable not found
                    return 1;
                }
                vm_push(vm, v);
            } break;
            // I_SET_GLOBAL VALUE NAME
            case I_SET_GLOBAL: {
                // the name of the variable
                vm_string *name = READ_STRING();
#ifdef DEBUG
                printf("I_SET_GLOBAL "); printf("%s ", SOLID_STR_TO_C(name->contents));
#endif

                lobj value = vm_peek(vm, 0);
#ifdef DEBUG
                lobj_println(&value);
#endif

                // set value
                table_set(vm->globals, name, value);
//                table_print(vm->globals);
                vm_pop(vm);
            } break;
            case I_GET_UPVALUE: {
                uint8_t index = READ_BYTE();
                vm_push(vm, *cur_frame->closure->upvalues[index]->location);
            } break;
            case I_SET_UPVALUE: {
                uint8_t index = READ_BYTE();
#ifdef DEBUG
                printf("I_SET_UPVALUE %d\n", index);
#endif
                *cur_frame->closure->upvalues[index]->location = vm_peek(vm, 0);
            } break;
            case I_CLOSE_UPVALUE: {
#ifdef DEBUG
                printf("I_CLOSE_UPVALUE\n");
#endif
                closeUpvalues(vm, vm->stack_top - 1);
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
    #undef READ_STRING
    #undef CONSTANT_AT

}

void vm_free(const VM* vm) {
    table_free(vm->globals);
    table_free(vm->strings);
}
