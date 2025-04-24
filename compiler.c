#include "compiler.h"
#include <stdio.h>
#include "vm.h"


void local_init(Local* lc, const solid_str name, const int depth, const int reg) {
    lc->depth = depth;
    lc->reg=reg;
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

int localchart_resolve_symbol(const LocalChart* lc, const char* symbol) {
    int found = -1;

    Local* local;
    darray_foreach(local, lc->locals) {
        if(local->depth <= lc->cur_depth && strcmp(symbol, SOLID_STR_TO_C(local->name)) == 0) {
            found = local->reg;
        }
    }
    //printf("Found %d\n", found);
    return found;
}
int localchart_insert(LocalChart *lc, const char* symbol) {
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
    const int prev_size = darray_size(lc->locals);
    const Local new_local = local_create(solid_str_new(symbol), lc->cur_depth, prev_size);
    darray_push(lc->locals, new_local);
    return prev_size;
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

    cu->enclosing=enclosing;
    return cu;
}


void compileunit_free(CompileUnit* cu) {
    if (!cu) return;
    free(cu->name);

    // NOTE:
    // VM takes the ownership of compile unit, so we don't need to free the compiler units
    localchart_free(cu->locals_chart);
}

lobj compile_unit_get_closure(CompileUnit* cu) {
    if (!cu) {
        printf("compile_unit_get_fn: NULL\n");
        exit(1);
    };
    //this is reallying ugly lol
    return into_lobj(vm_closure_create(vm_function_create(cu->bytes.item, darray_size(cu->bytes),
                                                          cu->constants.item, darray_size(cu->constants),
                                                          cu->arity, lobj_str(cu->name),
                                                          cu->upvalue_count)));
}


lobj compile_unit_get_fn(const CompileUnit* cu) {
    if (!cu) {
        printf("compile_unit_get_fn: NULL\n");
        exit(1);
    };
    //this is reallying ugly lol
    return into_lobj(vm_function_create(cu->bytes.item,
                                        darray_size(cu->bytes),
                                        cu->constants.item,
                                        darray_size(cu->constants),
                                        cu->arity,
                                        lobj_str(cu->name),
                                        cu->upvalue_count));
}


void compiler_create(Compiler *compiler, HashTable *globals, HashTable *strings) {
    // darray_init(compiler->units);
    compiler->current_unit = compileunit_create(NULL, "__main__", 0);

    compiler->globals=globals;
    compiler->strings=strings;

}

void compiler_destroy(Compiler *compiler) {
    while (compiler->current_unit->enclosing != NULL) {
        compileunit_free(compiler->current_unit);
        compiler->current_unit= compiler->current_unit->enclosing;
    }
}


int compiler_write_byte(const Compiler *compiler, const uint8_t byte) {
    CompileUnit* cur_unit = compiler->current_unit;
    darray_push(cur_unit->bytes, byte);
    return darray_size(cur_unit->bytes)-1;
}

vm_string* compiler_fetch_string(const Compiler* compiler, const char* symbol) {

    vm_string* dupe = table_find_string(compiler->strings, symbol, hash_str(symbol, strlen(symbol)));
    // if dupe doesn't exist (it's new)
    if(!dupe) {
        vm_string* vmstr = vm_string_create(symbol);
        table_set(compiler->strings, vmstr, lobj_num(1));
        return vmstr;
    } else {
        return dupe;
    }
}

// adds value to current units' compiler pool. Returns the size of array before pushing
int compiler_add_const(const Compiler *compiler, const lobj value) {
    CompileUnit* cu = compiler->current_unit;

    // if is string, try to find cached string of same hash and contents
    darray_push(cu->constants, value);
    return darray_size(cu->constants)-1;
}

// emits I_LOAD and adds constant to current unit's compiler pool
void compiler_load_const(const Compiler* compiler, lobj constant) {
    compiler_write_byte(compiler, I_LOAD);
#ifdef DEBUG_COMPILER
    printf("I_LOAD "); lobj_print(&constant); printf(" to %s\n", compiler->current_unit->name);
#endif


    int size = compiler_add_const(compiler, constant);
    compiler_write_byte(compiler, size);
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

    int removed = 0;
    while(lc->locals.size != 0) {
        const Local* cur = &(darray_item(lc->locals, darray_size(lc->locals) - 1));
        if(cur->depth <= lc->cur_depth) break;

        // if the local was captured by a scope lower, we close it
        if (cur->is_captured) {
            compiler_write_byte(compiler, I_CLOSE_UPVALUE); 
#ifdef DEBUG_COMPILER
        printf("ScopeLeave: I_CLOSE_UPVALUE %s\n", SOLID_STR_TO_C(cur->name));
#endif
        } else {
            compiler_write_byte(compiler, I_POP);
#ifdef DEBUG_COMPILER
        //printf("ScopeLeave: I_POP %s\n", SOLID_STR_TO_C(cur->name));
#endif
        };

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
    compiler_write_byte(compiler,instruction); //0
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
    compiler_write_byte(compiler, op);
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
    //printf("resolving %s in %s\n", symbol, cu->name);
    const LocalChart* enclosing_localchart = cu->enclosing->locals_chart;
    const int result = localchart_resolve_symbol(enclosing_localchart, symbol);
    if (result != -1) { // if it is not a local
#ifdef DEBUG_COMPILER
        printf("UpvalueResolution: Found local '%s' in enclosing local chart.\n", symbol);
#endif
        Local* captured_local = &(darray_item(enclosing_localchart->locals, result));
        captured_local->is_captured = true;
        return add_upvalue(cu, (uint8_t)result, true);
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

void compiler_rebind_localsymbol(const Compiler* compiler, char* symbol) {
    const CompileUnit* cur_unit = compiler->current_unit;
    const LocalChart* lc = cur_unit->locals_chart;

    int op;
    int arg = localchart_resolve_symbol(lc, symbol);
    if (arg != -1) {
        op = I_SET_LOCAL;
#ifdef DEBUG_COMPILER
        printf("emitting symbol: I_SET_LOCAL %s, %d\n",symbol, arg); ;
#endif
    } else if ((arg = resolve_upvalue(compiler->current_unit, symbol)) != -1) {
        op = I_SET_UPVALUE;
#ifdef DEBUG_COMPILER
        printf("emitting symbol: I_SET_UPVALUE %s, %d\n",symbol, arg); ;
#endif
    } else {
        vm_string* s  = compiler_fetch_string(compiler, symbol);
        const int str_loc = compiler_add_const(compiler, into_lobj(s));
        op = I_SET_GLOBAL;
        arg = str_loc;

#ifdef DEBUG_COMPILER
        printf("emitting symbol: I_SET_GLOBAL %s, %d\n",symbol, arg); ;
#endif
    }
    compiler_write_byte(compiler, op);
    compiler_write_byte(compiler, (uint8_t)arg);

}

void compiler_resolve_symbol(const Compiler *compiler, const char* name) {

    const LocalChart* lc = compiler->current_unit->locals_chart;
    int arg = localchart_resolve_symbol(lc, name);
    //if local was found
    if (arg != -1) {
#ifdef DEBUG_COMPILER
        printf("emitting symbol: I_GET_LOCAL %s, %d\n", name, arg); ;
#endif
        compiler_write_byte(compiler,  I_GET_LOCAL);
        compiler_write_byte(compiler,  arg);
    }
    else if ((arg = resolve_upvalue(compiler->current_unit, name)) != -1) {
        compiler_write_byte(compiler,  I_GET_UPVALUE);
        compiler_write_byte(compiler,  arg);
#ifdef DEBUG_COMPILER
        printf("emitting symbol: I_GET_UPVALUE %s, %d\n", name, arg); ;
#endif
    }
    else {
        // str dupe symbol
        arg = compiler_add_const(compiler, into_lobj(compiler_fetch_string(compiler, name)));
        compiler_write_byte(compiler,I_GET_GLOBAL);
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
            lobj snum = lobj_num(cur->value.number);
#ifdef DEBUG_COMPILER
            printf("emitting number: I_LOAD "); lobj_println(&snum);
#endif
            compiler_load_const(compiler, snum);
        } break;
        case SEXPR_SYMBOL: {
            compiler_resolve_symbol(compiler,cur->value.symbol);
        }
        break;
        case SEXPR_STRING: {
            // strings are interned. I need someway to create a string 
            // which is  
            vm_string* resolved_vmstring = compiler_fetch_string(compiler, cur->value.string);
            compiler_load_const(compiler, into_lobj(resolved_vmstring));
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
                    SExpr* fn_def = sexpr_list_first(rest); // '(name (a b))
                    const SExpr* fn_name = sexpr_list_first(fn_def); // "name"
                    SExpr* args = sexpr_list_first(sexpr_list_rest(fn_def)); // (a b)
                    SExpr* body = sexpr_list_rest(rest); // '(+ a b)

                    if(fn_name->type != SEXPR_SYMBOL) {
                        printf("fnname must be a simple symbol\n");
                        exit(1);
                    }
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
                            printf("arg must be simple symbol");
                            exit(1);
                        }
                        
                        localchart_insert(compiler->current_unit->locals_chart, el->value.symbol);
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
                        compiler_load_const(compiler, lobj_num(0));
                    }

                    compiler_write_byte(compiler, I_RETURN);
                    compiler_leave_scope(compiler);
                    // END WRITING TO NEW FUNCTION
                    //
                    const lobj new_fn = compile_unit_get_fn(cu);
                    // Return to previous pointer as current unit.
                    compiler->current_unit = compiler->current_unit->enclosing;

                    // adding the closure unit into the constant pool
                    compiler_load_const(compiler, new_fn);
                    compiler_write_byte(compiler,I_CLOSURE);

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
                        const int reg = localchart_insert(compiler->current_unit->locals_chart, fn_name->value.symbol);
                        if (reg == -1) {
                            fprintf(stderr,"function already declared\n");
                            exit(1);
                        }
                        compiler_write_byte(compiler, I_SET_LOCAL);
                        compiler_write_byte(compiler, reg);
                    } else {
                        vm_string* c = compiler_fetch_string(compiler, fn_name->value.symbol);
                        const int str_loc = compiler_add_const(compiler, into_lobj(c));
                        compiler_write_byte(compiler, I_SET_GLOBAL);
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
                    SExpr* bindings = sexpr_list_first(rest);
                    int even = 1;
                    compiler_enter_scope(compiler);

                    SExpr* cur_exp;
                    sexpr_list_foreach(cur_exp, bindings) {
                        //empty list
                        if(sexpr_list_first(cur_exp) == NULL) {
                            break;
                        }
                        SExpr* next = sexpr_list_rest(cur_exp);
                        //if uneven forms count for let declaration
                        if(next == NULL && (even % 2 == 1)) {
                            printf("Uneven forms in let binding\n");
                            exit(1);
                        }

                        const SExpr* lhs = sexpr_list_first(cur_exp);
                        SExpr* rhs = sexpr_list_first(next);
                        compiler_emit(compiler, rhs);

                        const int reg = localchart_insert(compiler->current_unit->locals_chart, lhs->value.symbol);
                        compiler_write_byte(compiler, I_SET_LOCAL);
                        compiler_write_byte(compiler, reg);
                        even+=1;
                        cur_exp = next;
                    }
                    SExpr* let_body = sexpr_list_rest(rest);
                    SExpr* b_form;
                    sexpr_list_foreach(b_form, let_body) {
                        compiler_emit(compiler, sexpr_list_first(b_form));
                    }

                    compiler_leave_scope(compiler);

                } else if (strcmp(symbol, "var") == 0) { //global variable
                    const SExpr* varname = sexpr_list_first(rest);
                    if(varname->type!= SEXPR_SYMBOL) {
                        printf("For var declaration, expected symbol");
                        exit(1);
                    }
                    if (sexpr_list_rest(rest) == NULL) {
                        printf("For var declaration, expected a value");
                        exit(1);
                    }
                    SExpr* value = sexpr_list_first(sexpr_list_rest(rest));


                    compiler_emit(compiler,value);
                    
                    vm_string* vms = compiler_fetch_string(compiler, varname->value.symbol);
                    const int str_loc = compiler_add_const(compiler, into_lobj(vms));
                    compiler_write_byte(compiler, I_SET_GLOBAL);
                    compiler_write_byte(compiler, str_loc);
                }  else if (strcmp(symbol, "if") == 0) {

                    #define BYTE_LEN \
                        (darray_size(compiler->current_unit->bytes))

                    //condition
                    compiler_emit(compiler, sexpr_list_first(rest));
                    // emit a jmp if falsy
                    const int ushort_start = compiler_jump_emit(compiler, I_JMP_FALSY);
                    // we get the length of bytes after jmp emitted,
                    int cur_size = BYTE_LEN;
                    rest = sexpr_list_rest(rest);

                    //true val
                    compiler_emit(compiler, sexpr_list_first(rest));
                    const int ushort_true = compiler_jump_emit(compiler, I_JMP);
                    // we can patch the jmp needed after processing truth branch by getting difference
                    // (length after processing truthy branch) - (length before)
                    const int jump_loc = BYTE_LEN - cur_size;


                    cur_size = BYTE_LEN;
                    //false val
                    compiler_emit(compiler, sexpr_list_first(sexpr_list_rest(rest)));
                    const size_t cur_location = BYTE_LEN - cur_size;

                    compiler_jump_patch(compiler, ushort_start, jump_loc);
                    compiler_jump_patch(compiler, ushort_true, cur_location);

                    #undef BYTE_LEN
                } else if (strcmp(symbol, "and") == 0) {
                    // evaluate first. if false, evaluate second one
                    #define BYTE_LEN \
                        (darray_size(compiler->current_unit->bytes))

                    // emit first
                    compiler_emit(compiler, sexpr_list_first(rest));

                    const int ushort_begin = compiler_jump_emit(compiler, I_JMP_FALSY);
                    const int cur_size = BYTE_LEN;

                    rest = sexpr_list_rest(rest);
                    // emit second
                    compiler_emit(compiler, sexpr_list_first(rest));

                    const size_t cur_location = BYTE_LEN - cur_size;

                    compiler_jump_patch(compiler, ushort_begin, cur_location);

                    #undef BYTE_LEN
                } else if (strcmp(symbol, "while") == 0) {

                    #define BYTE_LEN \
                        (darray_size(compiler->current_unit->bytes))

                    //condition
                    compiler_emit(compiler, sexpr_list_first(rest));
                    const int loop_back_loc = BYTE_LEN;
                    const int ushort_begin = compiler_jump_emit(compiler, I_JMP_FALSY); // returns where the ushort begins +2

                    compiler_write_byte(compiler, I_POP);
                    rest = sexpr_list_rest(rest);
                    compiler_emit(compiler, sexpr_list_first(rest)); 
                    compiler_write_byte(compiler, I_LOOP);
                    compileunit_write_ushort(compiler->current_unit, BYTE_LEN - loop_back_loc + 2);
                    compiler_jump_patch(compiler, ushort_begin, BYTE_LEN-ushort_begin-2);
                    #undef BYTE_LEN

                } else if(symbol[0] == '@')  {
                    char* ident = symbol+1;
                    SExpr* new_value = sexpr_list_first(rest);
                    compiler_emit(compiler, new_value);
                    compiler_rebind_localsymbol(compiler, ident);
                } else {
                    // if its none of the compiler specials, we assume it's a function call.
                    const int argc = sexpr_list_len(rest);
                    if(argc > 256) {
                        printf("Too many arguments!\n");
                        exit(1);
                    }

                    //emit I_GET_GLOBAL symbol
                    compiler_emit(compiler, list_head);

                    SExpr* cur_exp;
                    //emit bytecode for arguments
                    sexpr_list_foreach(cur_exp, rest) {
                        compiler_emit(compiler, sexpr_list_first(cur_exp));
                    }

                    // need to emit I_CALL N where N is number of args
                    compiler_write_byte(compiler, I_CALL);
                    compiler_write_byte(compiler, argc);
#ifdef DEBUG_COMPILER
                    printf("emitting: I_CALL %d for %s\n", argc, symbol);
#endif
                }
            }  
            else {
                printf("TODO\n");
                exit(1);
            }
        }
        break;
    }
}
