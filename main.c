#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "./vm.h"
#include "lobj.h"
#include <string.h>
#include <stdbool.h>
#include "reader.h"
#include "compiler.h"
#include "darray.h"  


#define QUOTE(...) #__VA_ARGS__


char* read_file(const char *filename, size_t *size_read) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error: Could not open file '%s'\n", filename);
        return NULL;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    const long file_size = ftell(file);
    rewind(file);

    // Allocate buffer for entire file content
    char *buffer = (char *)malloc(file_size + 1);
    if (!buffer) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        fclose(file);
        return NULL;
    }

    // Read file content
    *size_read = fread(buffer, 1, file_size, file);
    buffer[*size_read] = '\0';  // Null terminate the string
    fclose(file);

    return buffer;
}

SExpr* expect_next_form(const SExprType ty, SExpr* exp, char* assert_msg) {
    SExpr* rest = sexpr_list_rest(exp);
    if(ty == sexpr_list_first(rest)->type) {
        return rest;
    }
    fprintf(stderr, "%s\n", assert_msg);
    exit(1);
}

void emit(Compiler* compiler, SExpr* sexpr);

static inline void emit_bin_op(Compiler* compiler, const enum Instruction op, SExpr* rest) {
    SExpr* cur;
    sexpr_list_foreach(cur, rest) {
        SExpr* hd = sexpr_list_first(cur);
        emit(compiler, hd);
    }
    compiler_write_byte(compiler, op);
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
        printf("UpvalueResolution: '%s' no upper scope found. yielding -1\n", symbol);
        return -1;
    }
    //printf("resolving %s in %s\n", symbol, cu->name);
    const LocalChart* enclosing_localchart = cu->enclosing->locals_chart;
    const int result = localchart_resolve_symbol(enclosing_localchart, symbol);
    if (result != -1) { // if it is not a local
        printf("UpvalueResolution: Found local '%s' in enclosing local chart.\n", symbol);
        Local* captured_local = &(darray_item(enclosing_localchart->locals, result));
        captured_local->is_captured = true;
        return add_upvalue(cu, (uint8_t)result, true);
    }

    const int res = resolve_upvalue(cu->enclosing, symbol); // recursively check enclosing compile unit
    if (res != -1) {
        printf("UpvalueResolution: Recursively resolved upvalue '%s'\n", symbol);
        return add_upvalue(cu, (uint8_t)res, false);
    }

    printf("UpvalueResolution: could not resolve an upvalue\n");
    return -1;
}

void compiler_bind_localsymbol(const Compiler* compiler, char* symbol) {
    const CompileUnit* cur_unit = compiler->current_unit;
    const LocalChart* lc = cur_unit->locals_chart;

    int op;
    int arg = localchart_resolve_symbol(lc, symbol);
    if (arg != -1) {
        op = I_SET_LOCAL;
#ifdef DEBUG
        printf("emitting symbol: I_SET_LOCAL %s, %d\n",symbol, arg); ;
#endif
    } else if ((arg = resolve_upvalue(compiler->current_unit, symbol)) != -1) {
        op = I_SET_UPVALUE;
#ifdef DEBUG
        printf("emitting symbol: I_SET_UPVALUE %s, %d\n",symbol, arg); ;
#endif
    } else {
        const int str_loc = compiler_add_const(compiler, lobj_str(symbol));
        op = I_SET_GLOBAL;
        arg = str_loc;

#ifdef DEBUG
        printf("emitting symbol: I_SET_UPVALUE %s, %d\n",symbol, arg); ;
#endif
    }
    // printf("Could not bind %s\n", symbol);
    // exit(1);
    compiler_write_byte(compiler, op);
    compiler_write_byte(compiler, (uint8_t)arg);

}

void compiler_resolve_symbol(const Compiler *compiler, const char* name) {

    const LocalChart* lc = compiler->current_unit->locals_chart;
    int arg = localchart_resolve_symbol(lc, name);
    //if local was found
    if (arg != -1) {
#ifdef DEBUG
        printf("emitting symbol: I_GET_LOCAL %s, %d\n", name, arg); ;
#endif
        compiler_write_byte(compiler,  I_GET_LOCAL);
        compiler_write_byte(compiler,  arg);
    }
    else if ((arg = resolve_upvalue(compiler->current_unit, name)) != -1) {
        compiler_write_byte(compiler,  I_GET_UPVALUE);
        compiler_write_byte(compiler,  arg);
#ifdef DEBUG
        printf("emitting symbol: I_GET_UPVALUE %s, %d\n", name, arg); ;
#endif
    }
    else {
        // str dupe symbol
        arg = compiler_add_const(compiler, lobj_str(name));
        compiler_write_byte(compiler,I_GET_GLOBAL);
        compiler_write_byte(compiler, arg-1);
#ifdef DEBUG
        printf("emitting symbol: I_GET_GLOBAL %s, %d\n", name, arg-1); ;
#endif
    }
}

// emit essentially transitions parsing lifetime to compile lifetime
void emit(Compiler* compiler, SExpr *sexpr) {
    SExpr *cur = sexpr;
    switch (cur->type) {
        case SEXPR_NUMBER: {
            lobj snum = lobj_num(cur->value.number);
#ifdef DEBUG
            printf("emitting number: I_LOAD "); lobj_println(&snum);
#endif
            compiler_load_const(compiler, snum);
        } break;
        case SEXPR_SYMBOL: {
            compiler_resolve_symbol(compiler,cur->value.symbol);
        }
        break;
        case SEXPR_STRING: {
            vm_string* str = vm_string_create(cur->value.string);
            const vm_string* dupe = table_find_string(compiler->strings, &str->contents, str->hash);
            // if dupe doesn't exist (it's new)
            if(!dupe) {
                compiler_load_const(compiler, into_lobj(str));
                table_set(compiler->strings, str, lobj_num(1));
            } else {
                compiler_load_const(compiler, into_lobj((void*)dupe));
                vm_string_free(str);
            }
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
#ifdef DEBUG
                    printf("emitting fn: '%s'\n", fn_name->value.symbol);
#endif
                    // BEGIN WRITING TO NEW FUNCTION

                    // first class functions are just variables in disguise, that's why clojure's
                    // defn is just syntax over def and f
                    CompileUnit* cu = compileunit_create(fn_name->value.symbol, sexpr_list_len(args) );
                    cu->enclosing = compiler->current_unit;
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
                            emit(compiler, sexpr_list_first(cur_exp));
                        };
                    // else emit a 0, or falsy in this lang
                    } else {
                        compiler_load_const(compiler, lobj_num(0));
                    }

                    compiler_write_byte(compiler, I_RETURN);

                    // END WRITING TO NEW FUNCTION

                    const lobj new_fn = compile_unit_get_fn(cu);
                    // Return to previous pointer as current unit.
                    compiler->current_unit = compiler->current_unit->enclosing;

                    // adding the closure unit into the constant pool
                    compiler_load_const(compiler, new_fn);
                    compiler_write_byte(compiler,I_CLOSURE);
                    for (int i = 0; i < cu->upvalue_count; i++) {
                        const Upvalue* up = &cu->upvalues[i];
#ifdef DEBUG
                        printf("Writing upvalue to runtime islocal=%d index=%d\n", up->is_local, up->index);
#endif
                        compiler_write_byte(compiler, up->is_local ?  1 : 0);
                        compiler_write_byte(compiler,(uint8_t)up->index);
                    }

                    if (compiler->current_unit->locals_chart->cur_depth > 0) {
                        printf("Setting local\n");
                        const int reg = localchart_insert(compiler->current_unit->locals_chart, fn_name->value.symbol);
                        if (reg == -1) {
                            fprintf(stderr,"function already declared\n");
                            exit(1);
                        }
                        compiler_write_byte(compiler, I_SET_LOCAL);
                        compiler_write_byte(compiler, reg);
                    } else {
                        // printf("Setting global\n");
                        const int str_loc = compiler_add_const(compiler, lobj_str(fn_name->value.symbol));
                        compiler_write_byte(compiler, I_SET_GLOBAL);
                        // why the offset -1? because when we insert with compiler_add_const,
                        // it returns size after insert. thus, we need to write byte of str_loc - 1 to get location of string
                        compiler_write_byte(compiler, str_loc-1);
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
                        emit(compiler, rhs);

                        const int reg = localchart_insert(compiler->current_unit->locals_chart, lhs->value.symbol);
                        compiler_write_byte(compiler, I_SET_LOCAL);
                        compiler_write_byte(compiler, reg);
//                        printf("I_SET_LOCAL %s -> reg %d\n", lhs->value.symbol, reg);
                        even+=1;
                        cur_exp = next;
                    }
                    SExpr* let_body = sexpr_list_rest(rest);
                    SExpr* b_form;
                    sexpr_list_foreach(b_form, let_body) {
                        emit(compiler, sexpr_list_first(b_form));
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


                    emit(compiler,value);

                    const int str_loc = compiler_add_const(compiler, lobj_str(varname->value.symbol));
                    compiler_write_byte(compiler, I_SET_GLOBAL);
                    compiler_write_byte(compiler, str_loc - 1);
                }  else if (strcmp(symbol, "if") == 0) {

                    #define BYTE_LEN \
                        (darray_size(compiler->current_unit->bytes))

                    //condition
                    emit(compiler, sexpr_list_first(rest));
                    // emit a jmp if falsy
                    const int ushort_start = compiler_jump_emit(compiler, I_JMP_FALSY);
                    // we get the length of bytes after jmp emitted,
                    int cur_size = BYTE_LEN;
                    rest = sexpr_list_rest(rest);

                    //true val
                    emit(compiler, sexpr_list_first(rest));
                    const int ushort_true = compiler_jump_emit(compiler, I_JMP);
                    // we can patch the jmp needed after processing truth branch by getting difference
                    // (length after processing truthy branch) - (length before)
                    const int jump_loc = BYTE_LEN - cur_size;


                    cur_size = BYTE_LEN;
                    //false val
                    emit(compiler, sexpr_list_first(sexpr_list_rest(rest)));
                    const size_t cur_location = BYTE_LEN - cur_size;

                    compiler_jump_patch(compiler, ushort_start, jump_loc);
                    compiler_jump_patch(compiler, ushort_true, cur_location);

                    #undef BYTE_LEN
                } else if (strcmp(symbol, "and") == 0) {
                    // evaluate first. if false, evaluate second one
                    #define BYTE_LEN \
                        (darray_size(compiler->current_unit->bytes))

                    // emit first
                    emit(compiler, sexpr_list_first(rest));

                    const int ushort_begin = compiler_jump_emit(compiler, I_JMP_FALSY);
                    const int cur_size = BYTE_LEN;

                    rest = sexpr_list_rest(rest);
                    // emit second
                    emit(compiler, sexpr_list_first(rest));

                    const size_t cur_location = BYTE_LEN - cur_size;

                    compiler_jump_patch(compiler, ushort_begin, cur_location);

                    #undef BYTE_LEN
                } else if (strcmp(symbol, "while") == 0) {

                    #define BYTE_LEN \
                        (darray_size(compiler->current_unit->bytes))

                    //condition
                    emit(compiler, sexpr_list_first(rest));
                    const int loop_back_loc = BYTE_LEN;
                    const int ushort_begin = compiler_jump_emit(compiler, I_JMP_FALSY); // returns where the ushort begins +2

                    compiler_write_byte(compiler, I_POP);
                    rest = sexpr_list_rest(rest);
                    emit(compiler, sexpr_list_first(rest)); 
                    compiler_write_byte(compiler, I_LOOP);
                    compileunit_write_ushort(compiler->current_unit, BYTE_LEN - loop_back_loc + 2);
                    compiler_jump_patch(compiler, ushort_begin, BYTE_LEN-ushort_begin-2);
                    #undef BYTE_LEN

                } else if(symbol[0] == '@')  {
                    const char* ident = symbol+1;
                    compiler_bind_localsymbol(compiler, symbol);
                } else {
                    // if its none of the compiler specials, we assume it's a function call.
                    const int argc = sexpr_list_len(rest);
                    if(argc > 256) {
                        printf("Too many arguments!\n");
                        exit(1);
                    }

                    //emit I_GET_GLOBAL symbol
                    emit(compiler, list_head);

                    SExpr* cur_exp;
                    //emit bytecode for arguments
                    sexpr_list_foreach(cur_exp, rest) {
                        emit(compiler, sexpr_list_first(cur_exp));
                    }

                    // need to emit I_CALL N where N is number of args
                    compiler_write_byte(compiler, I_CALL);
                    compiler_write_byte(compiler, argc);
#ifdef DEBUG
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


int main(int argc, char **argv) {
    puts("Version 0.0.0.0.3");
    puts("Press Ctrl+c to Exit\n");
    
    if (argc > 1 && strcmp(argv[1], "-e") == 0) {
        if (argc == 2) {
//          const char *filename = argv[2];
//          size_t size_read;
//          char *buffer = read_file(filename, &size_read);
            const char* fib = QUOTE(
                 (fn (cookie ())
                    (let (a 15)
                        (fn (bookie ()) 
                                (+ a 10))
                            (bookie)))
                 (cookie)
            );

            const char* global_vars = "(var a 5)\n"
                                            "(+ a (+ 120 1))\n";

            const char* looper = "(let (a 10) "
                                    "(while a (@a (- a 1))))";

            printf("program:\n '%s'\n", fib);
            size_t count;
            SExpr **exprs = read_all(fib, &count); // or read appropriate function
            printf("Number of expressions: %zu\n", count);

            HashTable globals, strings;
            table_create(&globals);
            table_create(&strings);

            Compiler compiler;
            compiler_create(&compiler, &globals, &strings);
            for (size_t i = 0; i < count; i++) {
                emit(&compiler, exprs[i]);
            }

            // done with s expressions. we free all of them
            for(int i =0; i < count; i++) {
                free_sexpr(exprs[i]);
            }

            CompileUnit* main_fn= compiler.current_unit;
            if(main_fn == NULL) {
                printf("main function entry point not found");
                exit(1);
            }
            darray_push(main_fn->bytes, I_RETURN);


            lobj main = compile_unit_get_closure(main_fn);
            compiler_destroy(&compiler);

            VM vm;
            vm_create(&vm, &globals, &strings, &main);
            vm_exec(&vm, 0);

            vm_free(&vm);

        } else {
            size_t read;
            const char* program = read_file(argv[2], &read);
            if (program == NULL) {
                printf("Could not read file\n");
                return 1;
            }
            printf("program:\n '%s'\n", program);
            size_t count;
            SExpr **exprs = read_all(program, &count); // or read appropriate function
            printf("Number of expressions: %zu\n", count);

            HashTable globals, strings;
            table_create(&globals);
            table_create(&strings);

            Compiler compiler;
            compiler_create(&compiler, &globals, &strings);
            for (size_t i = 0; i < count; i++) {
                emit(&compiler, exprs[i]);
            }

            // done with s expressions. we free all of them
            for(int i =0; i < count; i++) {
                free_sexpr(exprs[i]);
            }

            CompileUnit* main_fn= compiler.current_unit;
            if(main_fn == NULL) {
                printf("main function entry point not found");
                exit(1);
            }
            darray_push(main_fn->bytes, I_RETURN);


            lobj main = compile_unit_get_closure(main_fn);
#ifdef DEBUG
            uint8_t* b;
            darray_foreach(b, main_fn->bytes) {
                printf("%d ", *b);
            }
            printf("\n");
#endif
            // lifetime of compiler is done.
            // I have transformed all compile units into functions / constants
            compiler_destroy(&compiler);

            VM vm;
            vm_create(&vm, &globals, &strings, &main);
            vm_exec(&vm, 0);


            vm_free(&vm);
            free((void*)program);


            return 0;
        }
    } else {
        // while (true) {
        //     char *input = readline(">> ");
        //     if (input && *input)
        //         add_history(input);
        //     else {
        //         continue;
        //     }
        //     Arena a = {.begin=NULL, .end=NULL};
        //     arena_alloc(&a, 65536);
        //     size_t count;
        //     SExpr** exprs = read_all(&a, input, &count);
        //
        //     HashTable globals, strings;
        //     table_create(&globals);
        //     table_create(&strings);
        //
        //     Compiler compiler;
        //     compiler_create(&compiler, &globals, &strings);
        //
        //     for (size_t i = 0; i < count; i++) {
        //         printf("Emitting bytecode for form:"); print_sexpr(exprs[i]);
        //         emit(&compiler, exprs[i]);
        //     }
        //
        //     printf("\n");
        //     for (size_t i = 0; i < count; i++) {
        //         print_sexpr(exprs[i]);
        //         printf(" ");
        //     }
        //     printf("\n");
        //     //free(input);
        //     printf("Num expressions: %zu\n", count);
        //     for (size_t i = 0; i < count; i++) {
        //         free_sexpr(exprs[i]);
        //     }
        //     table_free(&globals);
        //     table_free(&strings);
        //     compiler_destroy(&compiler);
        // }
    }
    return 0;
}
