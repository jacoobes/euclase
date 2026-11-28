#include "compiler.h"
#include <stdio.h>
#include "vm.h"
#include "table.h"


void local_init(Local* lc, const solid_str name, const int depth, const int slot) {
    lc->depth = depth;
    lc->slot=slot;
    lc->is_captured=false;
    lc->name=name;
}

Local local_create(const solid_str name, const int depth, const int reg) {
    Local ret;
    local_init(&ret, name, depth, reg);
    return ret;
}
void local_free(Local* lc) {
    solid_str_free(&lc->name);
    free(lc);
}


void localchart_init(LocalChart* lc) {
    if (lc==NULL) {
        printf("localchart_init: NULL\n");
        exit(1);
    }
    lc->cur_depth = 0;
    darray_init(lc->locals);
}
LocalChart localchart_create() {
    LocalChart lc;

    localchart_init(&lc);
    return lc;
}

void localchart_free(LocalChart* lc) {
    lc->cur_depth = 0;

    Local* l;
    darray_foreach(l, lc->locals) {
        solid_str_free(&l->name);
    }
    darray_free(lc->locals);
    free(lc);
}

// innermost visible local named symbol, or NULL. The pointer is into
// lc->locals, so it dies on the next insert.
Local* localchart_find(const LocalChart* lc, const char* symbol) {
    Local* found = NULL;
    Local* local;
    darray_foreach(local, lc->locals) {
        if(local->depth <= lc->cur_depth && strcmp(symbol, SOLID_STR_TO_C(local->name)) == 0) {
            found = local;
        }
    }
    return found;
}

// binds symbol to stack slot `slot`; returns slot, or -1 if already bound in this scope
int localchart_insert(LocalChart *lc, const char* symbol, int slot) {
    if(darray_size(lc->locals) == 256) {
        fprintf(stderr, "Variable overflow. Reached maximum variables per scope (256)");
        exit(1);
    }
    Local* local;
    darray_foreach(local, lc->locals) {
        if(local->depth == lc->cur_depth && strcmp(symbol, SOLID_STR_TO_C(local->name)) == 0) {
            return -1;
        }
    }
    darray_push(lc->locals, local_create(solid_str_new(symbol), lc->cur_depth, slot));
    return slot;
}
// name is duplicated, need to manage memory
CompileUnit* compileunit_create(CompileUnit* enclosing, const char* name, const int arity) {
    LocalChart* lc = malloc(sizeof(LocalChart));
    localchart_init(lc);

    // add function name to the 0th slot in locals
    darray_push(lc->locals, local_create(solid_str_new(name), 0, 0));

    CompileUnit* cu = malloc(sizeof(CompileUnit));
    darray_init(cu->bytes);
    darray_init(cu->constants);

    //duplicate the string name
    cu->name=strdup(name);

    cu->arity=arity;
    cu->locals_chart = lc;

    cu->upvalue_count = 0;
    
    // slot 0 is the callee, slots 1..arity are its arguments
    cu->stack_depth = 1 + arity;

    cu->enclosing=enclosing;
    return cu;
}


// Frees the unit and every nested unit in its constant pool. bytes is not
// freed: compiler_link hands it to the vm_function.
void compileunit_free(CompileUnit* cu) {
    if (!cu) return;
    Const* k;
    darray_foreach(k, cu->constants) {
        if (k->tag == K_FN) compileunit_free(k->as.fn);
    }
    darray_free(cu->constants);
    free(cu->name);
    localchart_free(cu->locals_chart);
    free(cu);
}

void compiler_create(Compiler *compiler) {
    compiler->current_unit = compileunit_create(NULL, "__main__", 0);
    darray_init(compiler->strings);
}

void compiler_destroy(Compiler *compiler) {
    // only units still being compiled are on the enclosing chain; finished
    // nested units are owned by their parent's K_FN constant
    CompileUnit* cu = compiler->current_unit;
    while (cu != NULL) {
        CompileUnit* enclosing = cu->enclosing;
        compileunit_free(cu);
        cu = enclosing;
    }
    compiler->current_unit = NULL;

    solid_str* str;
    darray_foreach(str, compiler->strings) {
        solid_str_free(str);
    }
    darray_free(compiler->strings);
}


int compiler_write_byte(const Compiler *compiler, const uint8_t byte) {
    CompileUnit* cur_unit = compiler->current_unit;
    darray_push(cur_unit->bytes, byte);
    return darray_size(cur_unit->bytes)-1;
}

// net change in stack size for each opcode, read off vm.c. I_CALL and
// I_POP_SCOPE depend on their operand, so their call sites apply it.
static int stack_effect(enum Instruction op) {
    switch (op) {
        case I_LOAD: case I_GET_LOCAL: case I_GET_UPVALUE: case I_GET_GLOBAL:
            return 1;
        case I_ADD: case I_SUB: case I_MUL: case I_DIV: case I_MOD:
        case I_LTE: case I_GT: case I_STR_ADD:
        case I_POP: case I_CLOSE_UPVALUE: case I_RETURN:
        case I_SET_GLOBAL: // peeks, stores, then pops
            return -1;
        default: // NEGATE, SET_LOCAL/SET_UPVALUE (peek), JMP*, LOOP, CLOSURE (fn -> closure)
            return 0;
    }
}

void compiler_emit_op(const Compiler* compiler, enum Instruction op) {
    compiler_write_byte(compiler, op);
    compiler->current_unit->stack_depth += stack_effect(op);
}

// ponytail: linear scan, swap for a hash map if compiling big programs gets slow
int compiler_fetch_string(Compiler* compiler, const char* symbol) {
    const uint32_t hash = hash_str(symbol, strlen(symbol));
    for (size_t i = 0; i < darray_size(compiler->strings); i++) {
        solid_str* s = &darray_item(compiler->strings, i);
        if (s->hash == hash && strcmp(SOLID_STR_TO_C(*s), symbol) == 0) {
            return i;
        }
    }
    darray_push(compiler->strings, solid_str_new(symbol));
    return darray_size(compiler->strings) - 1;
}

static Const const_num(double x) { return (Const){ .tag = K_NUM, .as.num = x }; }
static Const const_str(int idx)  { return (Const){ .tag = K_STR, .as.str = idx }; }
static Const const_fn(CompileUnit* cu) { return (Const){ .tag = K_FN, .as.fn = cu }; }

// adds value to current units' compiler pool. Returns the size of array before pushing
int compiler_add_const(const Compiler *compiler, const Const value) {
    CompileUnit* cu = compiler->current_unit;
    darray_push(cu->constants, value);
    // the size of array before pushing
    return darray_size(cu->constants)-1;
}

// emits I_LOAD and adds constant to current unit's compiler pool
void compiler_load_const(const Compiler* compiler, Const constant) {
    compiler_emit_op(compiler, I_LOAD);
#ifdef DEBUG_COMPILER
    switch (constant.tag) {
        case K_NUM: printf("I_LOAD %g", constant.as.num); break;
        case K_STR: printf("I_LOAD \"%s\"", SOLID_STR_TO_C(darray_item(compiler->strings, constant.as.str))); break;
        case K_FN:  printf("I_LOAD <fn %s>", constant.as.fn->name); break;
    }
    printf(" to %s\n", compiler->current_unit->name);
#endif
    int size = compiler_add_const(compiler, constant);
    compiler_write_byte(compiler, size);
}

static vm_string* intern(VM* vm, const solid_str* s) {
    vm_string* found = table_find_string(vm->strings, SOLID_STR_TO_C(*s), s->hash);
    if (found) return found;
    vm_string* created = vm_string_create(SOLID_STR_TO_C(*s));
    // vm->strings is weak and table_set can allocate (collect), so root it
    vm_push(vm, into_lobj(created));
    table_set(vm->strings, created, lobj_num(1));
    vm_pop(vm);
    return created;
}

static vm_function* link_unit(const Compiler* compiler, CompileUnit* cu, VM* vm) {
    const size_t n = darray_size(cu->constants);
    lobj* values = malloc(sizeof(lobj) * (n ? n : 1));
    for (size_t i = 0; i < n; i++) {
        const Const* k = &darray_item(cu->constants, i);
        switch (k->tag) {
            case K_NUM: {
                values[i] = lobj_num(k->as.num); 
            } break;
            case K_STR: {
                values[i] = into_lobj(intern(vm, &darray_item(compiler->strings, k->as.str)));
            } break;
            case K_FN: {
                values[i] = into_lobj(link_unit(compiler, k->as.fn, vm));
            } break;
        }
        vm_push(vm, values[i]); // mark as a root for gc to track.
    }
    solid_str name = solid_str_new(cu->name);
    lobj nm = into_lobj(intern(vm, &name));
    vm_push(vm, nm); // mark as a root for gc to track.
    vm_function* fn = vm_function_create(cu->bytes.item, darray_size(cu->bytes),
                                         values, n,
                                         cu->arity, nm,
                                         cu->upvalue_count);
    solid_str_free(&name);
    // n constants + the name; fn traces them from here on
    for (size_t i = 0; i <= n; i++) vm_pop(vm);
    return fn;
}

lobj compiler_link(Compiler* compiler, VM* vm) {
    CompileUnit* main_unit = compiler->current_unit;
    if (main_unit == NULL || main_unit->enclosing != NULL) {
        printf("compiler_link: expected the __main__ unit\n");
        exit(1);
    }
    darray_push(main_unit->bytes, I_RETURN);
    vm_function* fn = link_unit(compiler, main_unit, vm);
    vm_push(vm, into_lobj(fn));   // unrooted while its closure is allocated
    vm_closure* mainfn = vm_closure_create(fn);
    vm_pop(vm);
    // mainfn is unrooted until vm_exec puts it in a call frame (apply doesn't
    // allocate), so nothing may allocate between here and vm_exec
    return into_lobj(mainfn);
}



// compiler leave scope. my thought process on 'let blocks' 
// a let block is a scope, and the last expression is returned 
// but currently im thinking this is not the case now since if i POP 
// or CLOSE upvalue for let block scopes maybe for let block scopes it only does compile time stuff -- it won't emit opcodes for i_close or i_pop but when 
// in lisp this is implemented as a macro. maybe i can do it like that, but i barely have
// macros

size_t compiler_leave_scope(Compiler* compiler) {
    LocalChart* lc = compiler->current_unit->locals_chart;
    if(lc->cur_depth == 0) {
        return 0;
    }
    lc->cur_depth--;

    // only drops the scope's locals from the chart; the caller emits the
    // stack cleanup (I_POP_SCOPE for let, nothing after a fn's I_RETURN)
    int removed = 0;
    while(lc->locals.size != 0) {
        Local* cur = &(darray_item(lc->locals, darray_size(lc->locals) - 1));
        if(cur->depth <= lc->cur_depth) break;
        solid_str_free(&cur->name);
        darray_pop(lc->locals);
        removed++;
    }
#ifdef DEBUG_COMPILER
    printf("leaving scope\n");
#endif
    return removed;
}

void compiler_enter_scope(const Compiler* compiler) {
    compiler->current_unit->locals_chart->cur_depth++;
}

int compiler_jump_emit(const Compiler* compiler, const enum Instruction instruction) {
    compiler_emit_op(compiler,instruction); //0
    //short
    int jmp_instruct_loc = compiler_write_byte(compiler, 0xff); //1 placeholder
                           compiler_write_byte(compiler, 0xff); //2 placeholder
    //end short
    return jmp_instruct_loc;
}

void compiler_jump_patch(const Compiler* compiler, size_t ushort_begin, size_t jump_loc) {
    const CompileUnit* cur_unit = compiler->current_unit;
    cur_unit->bytes.item[ushort_begin] = (jump_loc >> 8) & 0xff;
    cur_unit->bytes.item[ushort_begin+1] = jump_loc & 0xff;
}


void compileunit_write_ushort(CompileUnit* cur_unit, const unsigned short jump_loc) {
    darray_push(cur_unit->bytes, (jump_loc >> 8) & 0xff );
    darray_push(cur_unit->bytes, jump_loc & 0xff);
}

SExpr* expect_next_form(const SExprType ty, SExpr* exp, char* assert_msg) {
    SExpr* rest = sexpr_list_rest(exp);
    if(ty == sexpr_list_first(rest)->type) {
        return rest;
    }
    fprintf(stderr, "%s\n", assert_msg);
    exit(1);
}

void compiler_emit(Compiler* compiler, SExpr* sexpr);

static inline void emit_bin_op(Compiler* compiler, const enum Instruction op, SExpr* rest) {
    SExpr* cur;
    sexpr_list_foreach(cur, rest) {
        SExpr* hd = sexpr_list_first(cur);
        compiler_emit(compiler, hd);
    }
    compiler_emit_op(compiler, op);
}


int add_upvalue(CompileUnit* cu, const uint8_t index, const bool is_local ) {
    const int upv_count = cu->upvalue_count;
    // find duplicates for upvalue, since a "closure may reference the same variable in a surrounding function multiple times" 
    for (int i = 0; i < upv_count; i++) {
        const Upvalue* upv = &cu->upvalues[i];
        if(upv->index == index && upv->is_local == is_local) {
            return i; 
        }
    }
    if (upv_count == UINT8_MAX) {
        printf("Maximum upvalue count exceeded: %d\n", UINT8_MAX);
        exit(1);
    }
    cu->upvalues[upv_count].index = index;
    cu->upvalues[upv_count].is_local = is_local;
    return cu->upvalue_count++;
}

int resolve_upvalue(CompileUnit* cu, const char* symbol) {
    // going up the compile-unit stack 
    // resolving where the upvalue is.
    // we also know that we checked local symbols already.
    if (cu->enclosing == NULL) {
#ifdef DEBUG_COMPILER
        printf("UpvalueResolution: '%s' no upper scope found. yielding -1\n", symbol);
#endif
        return -1;
    }
    const LocalChart* enclosing_localchart = cu->enclosing->locals_chart;
    Local* captured_local = localchart_find(enclosing_localchart, symbol);
    if (captured_local != NULL) {
#ifdef DEBUG_COMPILER
        printf("UpvalueResolution: Found local '%s' in enclosing local chart.\n", symbol);
#endif
        captured_local->is_captured = true;
        return add_upvalue(cu, (uint8_t)captured_local->slot, true);
    }

    const int res = resolve_upvalue(cu->enclosing, symbol); // recursively check enclosing compile unit
    if (res != -1) {
#ifdef DEBUG_COMPILER
        printf("UpvalueResolution: Recursively resolved upvalue '%s'\n", symbol);
#endif
        return add_upvalue(cu, (uint8_t)res, false);
    }
#ifdef DEBUG
    printf("UpvalueResolution: could not resolve an upvalue\n");
#endif
    return -1;
}

void compiler_rebind_localsymbol(Compiler* compiler, char* symbol) {
    const CompileUnit* cur_unit = compiler->current_unit;
    const LocalChart* lc = cur_unit->locals_chart;

    int op;
    int arg;
    const Local* local = localchart_find(lc, symbol);
    if (local != NULL) {
        op = I_SET_LOCAL;
        arg = local->slot;
#ifdef DEBUG_COMPILER
        printf("emitting symbol: I_SET_LOCAL %s, %d\n",symbol, arg); ;
#endif
    } else if ((arg = resolve_upvalue(compiler->current_unit, symbol)) != -1) {
        op = I_SET_UPVALUE;
#ifdef DEBUG_COMPILER
        printf("emitting symbol: I_SET_UPVALUE %s, %d\n",symbol, arg); ;
#endif
    } else {
        const int str_loc = compiler_add_const(compiler, const_str(compiler_fetch_string(compiler, symbol)));
        op = I_SET_GLOBAL;
        arg = str_loc;

#ifdef DEBUG_COMPILER
        printf("emitting symbol: I_SET_GLOBAL %s, %d\n",symbol, arg); ;
#endif
    }
    compiler_emit_op(compiler, op);
    compiler_write_byte(compiler, (uint8_t)arg);

}

void compiler_resolve_symbol(Compiler *compiler, const char* name) {

    const LocalChart* lc = compiler->current_unit->locals_chart;
    int arg;
    const Local* local = localchart_find(lc, name);
    if (local != NULL) {
        arg = local->slot;
#ifdef DEBUG_COMPILER
        printf("emitting symbol: I_GET_LOCAL %s, %d\n", name, arg); ;
#endif
        compiler_emit_op(compiler,  I_GET_LOCAL);
        compiler_write_byte(compiler,  arg);
    }
    else if ((arg = resolve_upvalue(compiler->current_unit, name)) != -1) {
        compiler_emit_op(compiler,  I_GET_UPVALUE);
        compiler_write_byte(compiler,  arg);
#ifdef DEBUG_COMPILER
        printf("emitting symbol: I_GET_UPVALUE %s, %d\n", name, arg); ;
#endif
    }
    else {
        // str dupe symbol
        arg = compiler_add_const(compiler, const_str(compiler_fetch_string(compiler, name)));
        compiler_emit_op(compiler,I_GET_GLOBAL);
        compiler_write_byte(compiler, arg);
#ifdef DEBUG_COMPILER
        printf("emitting symbol: I_GET_GLOBAL %s, %d\n", name, arg); ;
#endif
    }
}

// emit essentially transitions parsing lifetime to compile lifetime
void compiler_emit(Compiler* compiler, SExpr *sexpr) {
    SExpr *cur = sexpr;
    switch (cur->type) {
        case SEXPR_NUMBER: {
            compiler_load_const(compiler, const_num(cur->value.number));
        } break;
        case SEXPR_SYMBOL: {
            compiler_resolve_symbol(compiler,cur->value.symbol);
        }
        break;
        case SEXPR_STRING: {
            // strings are interned into compiler->strings, then into the vm at link time
            compiler_load_const(compiler, const_str(compiler_fetch_string(compiler, cur->value.string)));
        }
        break;
        case SEXPR_LIST: {
            SExpr *list_head = sexpr_list_first(cur);
            if (list_head == NULL) {
                printf("Error: Empty list\n");
                exit(1);
            }
            if (list_head->type == SEXPR_SYMBOL) {
                char *symbol = list_head->value.symbol;

                SExpr* rest = sexpr_list_rest(cur); //head should be a list
                if(strcmp(symbol, "exit") == 0) {
                    printf("Exiting...\n");
                    exit(0);
                }
                if(strcmp(symbol, "fn") == 0) {
                    // (fn (name (args...)) body...): check the shape before reading any part
                    SExpr* fn_def = rest == NULL ? NULL : sexpr_list_first(rest); // '(name (a b))
                    if(fn_def == NULL || fn_def->type != SEXPR_LIST) {
                        printf("Special form 'fn' error: expected (fn (name (args...)) body...)\n");
                        exit(1);
                    }
                    const SExpr* fn_name = sexpr_list_first(fn_def); // "name"
                    if(fn_name == NULL || fn_name->type != SEXPR_SYMBOL) {
                        printf("fnname must be a simple symbol\n");
                        exit(1);
                    }
                    SExpr* args_cell = sexpr_list_rest(fn_def);
                    SExpr* args = args_cell == NULL ? NULL : sexpr_list_first(args_cell); // (a b)
                    if(args == NULL || args->type != SEXPR_LIST) {
                        printf("Special form 'fn' error: expected an argument list after '%s'\n", fn_name->value.symbol);
                        exit(1);
                    }
                    if(sexpr_list_rest(args_cell) != NULL) {
                        printf("Special form 'fn' error: expected only a name and an argument list in (%s ...)\n", fn_name->value.symbol);
                        exit(1);
                    }
                    SExpr* body = sexpr_list_rest(rest); // '(+ a b)
#ifdef DEBUG_COMPILER
                    printf("emitting fn: '%s'\n", fn_name->value.symbol);
#endif
                    // BEGIN WRITING TO NEW FUNCTION

                    // first class functions are just variables in disguise, that's why clojure's
                    // defn is just syntax over def and f
                    CompileUnit* cu = compileunit_create(compiler->current_unit,
                                                         fn_name->value.symbol,
                                                         sexpr_list_len(args));
                    compiler->current_unit = cu;


                    //enter the scope of the currently compiling function
                    compiler_enter_scope(compiler);

                    SExpr* arg;
                    sexpr_list_foreach(arg, args) {
                        const SExpr* el = sexpr_list_first(arg);
                        if (el == NULL) {
                            break;
                        }
                        if(el->type != SEXPR_SYMBOL) {
                            printf("arg must be simple symbol\n");
                            exit(1);
                        }
                        // params follow the callee: slots 1..arity
                        const int slot = darray_size(compiler->current_unit->locals_chart->locals);
                        if(localchart_insert(compiler->current_unit->locals_chart, el->value.symbol, slot) == -1) {
                            printf("Special form 'fn' error: duplicate argument '%s'\n", el->value.symbol);
                            exit(1);
                        }
                        //printf(result != -1 ? "inserted\n" : "removed\n");
                    };

                    // if body of function has some forms, 
                    if (sexpr_list_len(body)) {
                        SExpr* cur_exp;
                        sexpr_list_foreach(cur_exp, body) {
                            // printf("cur form"); print_sexpr(sexpr_list_first(cur_exp)); printf("\n");
                            compiler_emit(compiler, sexpr_list_first(cur_exp));
                        };
                    // else emit a 0, or falsy in this lang
                    } else {
                        compiler_load_const(compiler, const_num(0));
                    }

                    compiler_emit_op(compiler, I_RETURN);
                    compiler_leave_scope(compiler);
                    // END WRITING TO NEW FUNCTION
                    //
                    // Return to previous pointer as current unit.
                    compiler->current_unit = compiler->current_unit->enclosing;

                    // adding the closure unit into the constant pool
                    compiler_load_const(compiler, const_fn(cu));
                    compiler_emit_op(compiler,I_CLOSURE);

                    // writing upvalue information from new function to its enclosing function
                    for (int i = 0; i < cu->upvalue_count; i++) {
                        const Upvalue* up = &cu->upvalues[i];
#ifdef DEBUG_COMPILER
                        printf("Writing upvalue to runtime islocal=%d index=%d\n", up->is_local, up->index);
#endif
                        compiler_write_byte(compiler, up->is_local ?  1 : 0);
                        compiler_write_byte(compiler,(uint8_t)up->index);
                    }

                    if (compiler->current_unit->enclosing != NULL) {
                        // the closure is on top of the stack: that slot is the local
                        // ponytail: a fn in argument position (#43 point 3) still gets a
                        // local whose slot dies when the call consumes it
                        const int slot = localchart_insert(compiler->current_unit->locals_chart, fn_name->value.symbol,
                                                           compiler->current_unit->stack_depth - 1);
                        if (slot == -1) {
                            fprintf(stderr,"function already declared\n");
                            exit(1);
                        }
                    } else {
                        const int str_loc = compiler_add_const(compiler, const_str(compiler_fetch_string(compiler, fn_name->value.symbol)));
                        compiler_emit_op(compiler, I_SET_GLOBAL);
                        compiler_write_byte(compiler, str_loc);
                    }
                }
                else if(strcmp(symbol, "+") == 0)  { emit_bin_op(compiler, I_ADD, rest); }
                else if(strcmp(symbol, "-") == 0)  { emit_bin_op(compiler, I_SUB, rest); }
                else if(strcmp(symbol, "/") == 0)  { emit_bin_op(compiler, I_DIV, rest); }
                else if(strcmp(symbol, "*") == 0)  { emit_bin_op(compiler, I_MUL, rest); }
                else if(strcmp(symbol, "%") == 0)  { emit_bin_op(compiler, I_MOD, rest); }
                else if(strcmp(symbol, "<=") == 0) {  emit_bin_op(compiler, I_LTE, rest);}
                else if(strcmp(symbol, ">") == 0)  {   emit_bin_op(compiler, I_GT, rest);}
                else if (strcmp(symbol, "let") == 0) { //locally scoped variables
                    if(rest == NULL) {
                        printf("Invalid special form 'let'\n");
                        exit(1);
                    }
                    SExpr* bindings = sexpr_list_first(rest); // list (a 1 b 1)
                    if(bindings->type != SEXPR_LIST) {
                            printf("Invalid special form 'let'. Does not have local variable declaration\n");
                            exit(1);
                    }
                    if((sexpr_list_len(bindings) % 2) == 1) {
                            printf("Uneven forms in let binding\n");
                            exit(1);
                    }
                    compiler_enter_scope(compiler);
                    CompileUnit* cu = compiler->current_unit;
                    // where the let's first local lands; everything from here up is the let's
                    const int first_slot = cu->stack_depth;

                    // For each declaration 
                    SExpr* cur_exp;
                    sexpr_list_foreach(cur_exp, bindings) {
                        //empty list
                        if(sexpr_list_first(cur_exp) == NULL) {
                            break;
                        }
                        SExpr* next = sexpr_list_rest(cur_exp);

                        const SExpr* lhs = sexpr_list_first(cur_exp);
                        if(lhs->type != SEXPR_SYMBOL) {
                            printf("Special form 'let' must have symbol on left hand side\n");
                            exit(1);
                        }
                        SExpr* rhs = sexpr_list_first(next);
                        compiler_emit(compiler, rhs);

                        // the value is on top of the stack: that slot becomes the local
                        LocalChart* lc = cu->locals_chart;
                        if (localchart_insert(lc, lhs->value.symbol, cu->stack_depth - 1) == -1) {
                            // name already bound earlier in this let: the later binding
                            // overwrites it, and the pushed copy is dropped
                            compiler_emit_op(compiler, I_SET_LOCAL);
                            //localchart_find may turn null, as it is a Local*. However, the if check asserts it exists
                            compiler_write_byte(compiler, localchart_find(lc, lhs->value.symbol)->slot);
                            compiler_emit_op(compiler, I_POP);
                        }
                        cur_exp = next;
                    }
                    SExpr* let_body = sexpr_list_rest(rest);

                    // let body: 
                    SExpr* b_form;
                    sexpr_list_foreach(b_form, let_body) {
                        compiler_emit(compiler, sexpr_list_first(b_form));
                    }
                    if (sexpr_list_len(let_body) == 0) {
                        compiler_load_const(compiler, const_num(0));
                    }

                    // drop the let's slots (locals and leftover body values), keep the result
                    const int n = cu->stack_depth - 1 - first_slot;
                    if (n > UINT8_MAX) {
                        printf("Special form 'let' error: too many values in one let (max %d)\n", UINT8_MAX);
                        exit(1);
                    }
                    if (n > 0) {
                        compiler_emit_op(compiler, I_POP_SCOPE);
                        compiler_write_byte(compiler, n);
                        cu->stack_depth -= n;
                    }
                    compiler_leave_scope(compiler);

                } else if (strcmp(symbol, "var") == 0) { //global variable
                    if(rest == NULL) {
                        printf("No name found for declaration 'var'");
                        exit(1);
                    }
                    const SExpr* varname = sexpr_list_first(rest);
                    
                    if(varname->type != SEXPR_SYMBOL) {
                        printf("For var declaration, expected symbol");
                        exit(1);
                    }
                    if (sexpr_list_len(rest) != 2) {
                        printf("For var declaration, expected exactly a name and one value");
                        exit(1);
                    }
                    SExpr* value = sexpr_list_first(sexpr_list_rest(rest));

                    compiler_emit(compiler,value);
                    
                    const int str_loc = compiler_add_const(compiler, const_str(compiler_fetch_string(compiler, varname->value.symbol)));
                    compiler_emit_op(compiler, I_SET_GLOBAL);
                    compiler_write_byte(compiler, str_loc);
                }  else if (strcmp(symbol, "if") == 0) {

                    int if_len = sexpr_list_len(rest);
                    if(if_len != 3) {
                        printf("Special form 'if' error: Expected 3 expressions");
                        exit(1);
                    }

                    #define BYTE_LEN \
                        (darray_size(compiler->current_unit->bytes))

                    SExpr* condition = sexpr_list_first(rest);

                    //condition
                    compiler_emit(compiler, condition);
                    // emit a jmp if falsy
                    const int ushort_start = compiler_jump_emit(compiler, I_JMP_FALSY);
                    // we get the length of bytes after jmp emitted,
                    int cur_size = BYTE_LEN;
                    rest = sexpr_list_rest(rest);

                    //true val; only one arm runs, so the else arm starts from the same depth
                    const int arm_depth = compiler->current_unit->stack_depth;
                    compiler_emit(compiler, sexpr_list_first(rest));
                    const int ushort_true = compiler_jump_emit(compiler, I_JMP);
                    // we can patch the jmp needed after processing truth branch by getting difference
                    // (length after processing truthy branch) - (length before)
                    const int jump_loc = BYTE_LEN - cur_size;


                    cur_size = BYTE_LEN;
                    compiler->current_unit->stack_depth = arm_depth;
                    //false val
                    compiler_emit(compiler, sexpr_list_first(sexpr_list_rest(rest)));
                    const size_t cur_location = BYTE_LEN - cur_size;

                    compiler_jump_patch(compiler, ushort_start, jump_loc);
                    compiler_jump_patch(compiler, ushort_true, cur_location);

                    #undef BYTE_LEN
                } else if (strcmp(symbol, "and") == 0) {

                    int and_len = sexpr_list_len(rest); // rest = (1 2)
                    if(and_len < 2 || and_len > MAX_ARGS) {
                        printf("Special form 'and' error: Expected 2 to %d expressions\n", MAX_ARGS);
                        exit(1);
                    }
                    // (and a b c): evaluate operands left to right. After each one but
                    // the last, jump to the end if it is falsy, leaving it on the stack
                    // as the result; otherwise pop it and evaluate the next one.
                    #define BYTE_LEN \
                        (darray_size(compiler->current_unit->bytes))

                    // one jump per operand except the last; the guard above keeps
                    // jump_count below MAX_ARGS, so these arrays cannot overflow
                    int jumps[MAX_ARGS - 1];
                    int jump_ends[MAX_ARGS - 1];
                    int jump_count = 0;

                    compiler_emit(compiler, sexpr_list_first(rest));
                    for (SExpr* next = sexpr_list_rest(rest); next != NULL && sexpr_list_first(next) != NULL;
                         next = sexpr_list_rest(next)) {
                        jumps[jump_count] = compiler_jump_emit(compiler, I_JMP_FALSY);
                        jump_ends[jump_count] = BYTE_LEN; // offsets are measured from after the jump
                        jump_count++;
                        compiler_emit_op(compiler, I_POP); // I_JMP_FALSY only peeks
                        compiler_emit(compiler, sexpr_list_first(next));
                    }
                    for (int j = 0; j < jump_count; j++) {
                        compiler_jump_patch(compiler, jumps[j], BYTE_LEN - jump_ends[j]);
                    }
                    #undef BYTE_LEN
                } else if (strcmp(symbol, "while") == 0) {
                    int while_bodylen = sexpr_list_len(rest);
                    if(while_bodylen < 2) {
                        printf("Special form 'while' error: Expected a condition and a body\n");
                        exit(1);
                    }

                    #define BYTE_LEN \
                        (darray_size(compiler->current_unit->bytes))

                    // loop: cond; JMP_FALSY exit; POP; (body; POP)*; LOOP loop
                    // exit: the falsy condition stays on the stack as the while's value
                    const int loop_back_loc = BYTE_LEN;
                    compiler_emit(compiler, sexpr_list_first(rest));
                    const int ushort_begin = compiler_jump_emit(compiler, I_JMP_FALSY); // returns where the ushort begins +2
                    // the exit path jumps out with the condition still on the stack
                    const int exit_depth = compiler->current_unit->stack_depth;
                    compiler_emit_op(compiler, I_POP);

                    SExpr* cur_exp;
                    sexpr_list_foreach(cur_exp, sexpr_list_rest(rest)) {
                        compiler_emit(compiler, sexpr_list_first(cur_exp));
                        compiler_emit_op(compiler, I_POP);
                    }

                    compiler_emit_op(compiler, I_LOOP);
                    compiler->current_unit->stack_depth = exit_depth;
                    compileunit_write_ushort(compiler->current_unit, BYTE_LEN - loop_back_loc + 2);
                    compiler_jump_patch(compiler, ushort_begin, BYTE_LEN-ushort_begin-2);
                    #undef BYTE_LEN

                } else if(symbol[0] == '@')  {
                    char* ident = symbol+1;
                    if(ident[0] == '\0') {
                        printf("Expected a symbol after '@' \n");
                        exit(1);
                    }
                    if(sexpr_list_len(rest) != 1) {
                        printf("special form needs 1 element to assign '@' \n");
                        exit(1);
                    }

                    if (localchart_find(compiler->current_unit->locals_chart, ident) == NULL
                     && resolve_upvalue(compiler->current_unit, ident) == -1) {
                        printf("Could not resolve symbol %s to assign value\n", ident);
                        exit(1);
                    }

                    compiler_emit(compiler, sexpr_list_first(rest));
                    compiler_rebind_localsymbol(compiler, ident);
                } else {
                    // if its none of the compiler specials, we assume it's a function call.
                    const int argc = sexpr_list_len(rest);
                    if(argc > MAX_ARGS) {
                        printf("Too many arguments!\n");
                        exit(1);
                    }

                    compiler_emit(compiler, list_head);

                    SExpr* cur_exp;
                    //emit bytecode for arguments
                    sexpr_list_foreach(cur_exp, rest) {
                        compiler_emit(compiler, sexpr_list_first(cur_exp));
                    }

                    // need to emit I_CALL N where N is number of args
                    compiler_emit_op(compiler, I_CALL);
                    compiler_write_byte(compiler, argc);
                    compiler->current_unit->stack_depth -= argc; // callee + args -> result
#ifdef DEBUG_COMPILER
                    printf("emitting: I_CALL %d for %s\n", argc, symbol);
#endif
                }
            }  else { // HEAD OF LIST IS NOT A SYMBOL
                if(list_head->type == SEXPR_LIST) {
                    SExpr* rest = sexpr_list_rest(cur);
                    const int argc = sexpr_list_len(rest);
                    if(argc > MAX_ARGS) {
                        printf("Too many arguments!\n");
                        exit(1);
                    }
                    compiler_emit(compiler, list_head); // function is now at top of stack

                    SExpr* cur_exp;
                    //emit bytecode for arguments
                    sexpr_list_foreach(cur_exp, rest) {
                        compiler_emit(compiler, sexpr_list_first(cur_exp));
                    }

                    // need to emit I_CALL N where N is number of args
                    compiler_emit_op(compiler, I_CALL);
                    compiler_write_byte(compiler, argc);
                    compiler->current_unit->stack_depth -= argc; // callee + args -> result
#ifdef DEBUG_COMPILER
                    printf("emitting: I_CALL %d\n", argc);
#endif
                } else {
                    printf("TODO\n");
                    exit(1);
                }
                
            }
        }
        break;
    }
}
