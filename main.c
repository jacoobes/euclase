#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "./vm.h"
#include "lobj.h"
#include <string.h>
#include <stdbool.h>
#include "reader.h"
/* If we are compiling on Windows compile these functions */
#ifdef _WIN32

static char buffer[2048];

/* Fake readline function */
char *readline(const char *prompt) {
    fputs(prompt, stdout);
    fgets(buffer, 2048, stdin);
    char *cpy = malloc(strlen(buffer) + 1);
    strcpy(cpy, buffer);
    cpy[strlen(cpy) - 1] = '\0';
    return cpy;
}

/* Fake add_history function */
void add_history(char *unused) {}

/* Otherwise include the edit line headers */
#else
#include <editline/readline.h>
#include <editline/history.h>
#endif//_WIN32


#include "darray.h"  // Assuming you have a header file defining the darray macros and functions


typedef struct Local {
    solid_str name;
    size_t depth;
    //can we squish this to a byte? need to bound check locals insert check to ensure
    int reg;
} Local;


void local_init(Local* lc, const solid_str name, const int depth, const int reg) {
    lc->depth = depth;
    lc->reg=reg;
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

typedef struct LocalChart {
    int cur_depth;
    darray(Local) locals;
} LocalChart;

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

size_t localchart_leave_scope(LocalChart* lc) {
    if(lc->cur_depth == 0) {
        return 0;
    }

    lc->cur_depth--;
    int removed = 0;
    while(lc->locals.size != 0) {
        const Local* cur = &(darray_item(lc->locals, darray_size(lc->locals)-1));
        if(cur->depth <= lc->cur_depth) break;
        darray_pop(lc->locals);
        removed++;
    }
    return removed;
}
void localchart_enter_scope(LocalChart* lc) {
    lc->cur_depth++;
}

int localchart_resolve_symbol(const LocalChart* lc, const char* symbol) {
    int found = -1;

    Local* local;
    darray_foreach(local, lc->locals) {
        if(local->depth <= lc->cur_depth && strcmp(symbol, SOLID_STR_TO_C(local->name)) == 0) {
            found = local->reg;
        }
    }
    return found;
}
int localchart_insert(LocalChart *lc, const char* symbol) {
    if(darray_size(lc->locals) == 256) {
        printf("Variable overflow. Reached maximum variables per scope (256)");
        return -1;
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


typedef struct CompileUnit {
    darray(uint8_t) bytes;
    darray(lobj) constants;
    LocalChart* locals_chart;
    //owned: need to free this
    char* name;
    int arity;
    struct CompileUnit* next;
} CompileUnit;

// name is duplicated, need to manage memory
CompileUnit* compileunit_create(const char* name, const int arity) {
    LocalChart* lc = malloc(sizeof(LocalChart));
    localchart_init(lc);
    // add function name to the 0th slot in locals
    darray_push(lc->locals, local_create(solid_str_new(name), 0, 0));
    CompileUnit* cu = malloc(sizeof(CompileUnit));
        cu->next=NULL;
        darray_init(cu->bytes);
        darray_init(cu->constants);
        //duplicate the string name
        cu->name=strdup(name);
        cu->arity=arity;
        cu->locals_chart = lc;
    return cu;
}
void compileunit_free(CompileUnit* cu) {
    if (!cu) return;
    free(cu->name);

    // NOTE:
    // VM takes the ownership of compile unit, so we don't need to free the compiler units

    localchart_free(cu->locals_chart);
}


// a compile unit is transformed into a lobj of type OBJ with CLOSURE via copy
lobj compile_unit_get_closure(const CompileUnit* cu) {
    if (!cu) {
        printf("compile_unit_get_closure: NULL\n");
        exit(1);
    };

    //this is realllllly ugly lol
    return into_lobj(LVAL_OBJ, vm_closure_create(
                                        vm_function_create(cu->bytes.item, darray_size(cu->bytes),
                                                              cu->constants.item, darray_size(cu->constants),
                                                              cu->arity, lobj_str(cu->name))));
}

typedef struct {
    darray(CompileUnit*) units;
    // current_unit belongs to units, so when we clean up, we set to NULL, and then
    // just free units
    CompileUnit* current_unit;
    HashTable* globals;
    HashTable* strings;
} Compiler;


void compiler_append_unit(Compiler* compiler, const char* name, const int arity) {
    darray_append(compiler->units, compileunit_create(name, arity));

}

static inline CompileUnit* compiler_peek_unit (const Compiler* compiler) {
    const size_t size = darray_size(compiler->units);
    if (size == 0) {
        printf("Error: No units to peek!\n");
        return NULL;
    }
    return darray_item(compiler->units, size-1);
}


static inline CompileUnit* compiler_cur_compile_unit (const Compiler* compiler) {
    return compiler->current_unit;
}

void compiler_create(Compiler *compiler, HashTable *globals, HashTable *strings) {
    darray_init(compiler->units);
    compiler_append_unit(compiler, "__main__", 0);
    compiler->current_unit= darray_item(compiler->units, 0);

    compiler->globals=globals;
    compiler->strings=strings;

}

void compiler_destroy(Compiler *compiler) {
    compiler->current_unit = NULL;
    CompileUnit** cu;
    darray_foreach(cu, compiler->units) {
        compileunit_free(*cu);
    }
}

static inline int compiler_write_byte(const Compiler *compiler, const uint8_t byte) {
    CompileUnit* cur_unit = compiler_cur_compile_unit(compiler);
    darray_push(cur_unit->bytes, byte);
    return darray_size(cur_unit->bytes)-1;
}

// adds value to current units' compiler pool. Returns the size of array after pushing
static inline int compiler_add_const(const Compiler *compiler, const lobj value) {
    CompileUnit* cu = compiler_cur_compile_unit(compiler);
    if (IS_STR(value)) {
        vm_string* str = AS_STR(value);
        const vm_string* dupe = table_find_string(compiler->strings, &str->contents, str->hash);

        // if dupe doesn't exist (it's new)
        if(!dupe) {
            darray_push(cu->constants, value);
            table_set(compiler->strings, str, lobj_num(1));
            return darray_size(cu->constants);
        }
        darray_push(cu->constants, into_lobj(LVAL_OBJ, dupe));
        vm_string_free(str);
        return darray_size(cu->constants);
    }
    darray_push(cu->constants, value);
    return darray_size(cu->constants);
}

// emits I_LOAD and adds constant to current unit's compiler pool
static inline void compiler_load_const(const Compiler* compiler, lobj constant) {
    compiler_write_byte(compiler, I_LOAD);
#ifdef DEBUG
    printf("I_LOAD "); lobj_print(&constant); printf(" to %s\n", compiler_cur_compile_unit(compiler)->name);
#endif


    compiler_add_const(compiler, constant);
    compiler_write_byte(compiler, darray_size(compiler_cur_compile_unit(compiler)->constants) - 1);
}


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
    const CompileUnit* cur_unit = compiler_cur_compile_unit(compiler);
    
    cur_unit->bytes.item[ushort_begin] = (jump_loc >> 8) & 0xff;
    cur_unit->bytes.item[ushort_begin+1] = jump_loc & 0xff;
}
void compiler_write_ushort(const Compiler* compiler, unsigned short jump_loc) {
    CompileUnit* cur_unit = compiler_cur_compile_unit(compiler);
    darray_push(cur_unit->bytes, (jump_loc >> 8) & 0xff );
    darray_push(cur_unit->bytes, jump_loc & 0xff);
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
            const char *symbol = cur->value.symbol;

            const int local_index = localchart_resolve_symbol(compiler->current_unit->locals_chart,symbol);
            if (local_index != -1) {
#ifdef DEBUG
                printf("emitting symbol: I_GET_LOCAL %s, %d\n", symbol, local_index); ;
#endif
                compiler_write_byte(compiler,  I_GET_LOCAL);
                compiler_write_byte(compiler,  local_index);
            } else {
                // str dupe symbol
                const int new_size = compiler_add_const(compiler, lobj_str(symbol));

                compiler_write_byte(compiler,I_GET_GLOBAL);
                compiler_write_byte(compiler, new_size-1);
            }
        }
        break;
        case SEXPR_STRING: {
            vm_string* str = vm_string_create(cur->value.string);
            const vm_string* dupe = table_find_string(compiler->strings, &str->contents, str->hash);
            // if dupe doesn't exist (it's new)
            if(!dupe) {
                compiler_load_const(compiler, into_lobj(LVAL_OBJ, str));
                table_set(compiler->strings, str, lobj_num(1));
            } else {
                compiler_load_const(compiler, into_lobj(LVAL_OBJ, dupe));
                vm_string_free(str);
            }
        }
        break;
        case SEXPR_LIST: {
            const SExpr *list_head = sexpr_list_first(cur);
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
                    SExpr* args = sexpr_list_first(sexpr_list_rest(fn_def)); // '(a b)
                    SExpr* body = sexpr_list_rest(rest); // '(+ a b)

                    if(fn_name->type != SEXPR_SYMBOL) {
                        printf("fnname must be a simple symbol\n");
                        exit(1);
                    }
#ifdef DEBUG
                    printf("emitting fn: %s\n", fn_name->value.symbol);
#endif

                    // first class functions are just variables in disguise, that's why clojure's
                    // defn is just syntax over def and f


                    compiler_append_unit(compiler, fn_name->value.symbol, sexpr_list_len(args));
                    compiler->current_unit = darray_pop(compiler->units);


                    //enter the scope of the currently compiling function
                    localchart_enter_scope(compiler->current_unit->locals_chart);

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

                    if (sexpr_list_len(body)) {
#ifdef DEBUG
                        printf("\temitting in function: has a body\n");
#endif
                        SExpr* cur_exp;
                        sexpr_list_foreach(cur_exp, body) {
                            emit(compiler, sexpr_list_first(cur_exp));
                        };
                    } else {
#ifdef DEBUG
                        printf("\temitting in function: has no body\n");
#endif
                        compiler_load_const(compiler, lobj_num(0));
                    }
                    compiler_write_byte(compiler, I_RETURN);


                    const CompileUnit* cu = compiler_cur_compile_unit(compiler);

                    const lobj new_closure = compile_unit_get_closure(cu);


#ifdef DEBUG
                    // CompileUnit* cla;
                    // printf("compile units found:\n");
                    // darray_foreach(cla, compiler->units) {
                    //     printf("%s,", cla->name);
                    // }
                    // printf(" cur unit '%s' \n", cu->name);
                    printf("emitting function done %s\n", fn_name->value.symbol);
#endif
                    // Return to previous pointer as current unit. that is,
                    // the one before appending the unit ( size - 2)

                    compiler->current_unit = compiler_peek_unit(compiler);
                    compiler_load_const(compiler, new_closure);

                    const int str_loc = compiler_add_const(compiler, lobj_str(fn_name->value.symbol));
                    compiler_write_byte(compiler, I_SET_GLOBAL);
                    compiler_write_byte(compiler, str_loc-1);
                }
                else if(strcmp(symbol, "+") == 0) { emit_bin_op(compiler, I_ADD, rest); }
                else if(strcmp(symbol, "-") == 0) { emit_bin_op(compiler, I_SUB, rest); }
                else if(strcmp(symbol, "/") == 0) { emit_bin_op(compiler, I_DIV, rest); }
                else if(strcmp(symbol, "*") == 0) { emit_bin_op(compiler, I_MUL, rest); }
                else if(strcmp(symbol, "%") == 0) { emit_bin_op(compiler, I_MOD, rest); }
                else if(strcmp(symbol, "<=") == 0) {  emit_bin_op(compiler, I_LTE, rest);}
                else if(strcmp(symbol, ">") == 0)  {   emit_bin_op(compiler, I_GT, rest);}
                else if (strcmp(symbol, "let") == 0) { //locally scoped variables
                    SExpr* bindings = sexpr_list_first(rest);
                    int even = 1;
                    localchart_enter_scope(compiler->current_unit->locals_chart);

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
                        compiler_write_byte(compiler, I_SET_LOCAL); compiler_write_byte(compiler, reg);
                        even+=1;
                        cur_exp = next;
                    }
                    SExpr* let_body = sexpr_list_rest(rest);
                    SExpr* b_form;
                    sexpr_list_foreach(b_form, let_body) {
                        emit(compiler, sexpr_list_first(b_form));
                    }
                    const size_t removed = localchart_leave_scope(compiler->current_unit->locals_chart);
                    printf("removed %zu\n", removed);
                    for(int i = 0; i < removed; i++ ) {
                        compiler_write_byte(compiler, I_POP);
                    }

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
                    compiler_write_byte(compiler, str_loc);
                }  else if (strcmp(symbol, "if") == 0) {
                    #define BYTELEN \
                        (darray_size(compiler_cur_compile_unit(compiler)->bytes))
                    //condition
                    emit(compiler, sexpr_list_first(rest));
                    // emit a jmp if falsy
                    const int ushort_start = compiler_jump_emit(compiler, I_JMP_FALSY);
                    // we get the length of bytes after jmp emitted,
                    int cur_size = BYTELEN;
                    rest = sexpr_list_rest(rest);

                    //true val
                    emit(compiler, sexpr_list_first(rest));
                    const int ushort_true = compiler_jump_emit(compiler, I_JMP);
                    // we can patch the jmp needed after processing truth branch by getting difference
                    // (length after processing truthy branch) - (length before)
                    const int jump_loc = BYTELEN - cur_size;


                    cur_size = BYTELEN;
                    //false val
                    emit(compiler, sexpr_list_first(sexpr_list_rest(rest)));
                    const size_t cur_location = BYTELEN - cur_size;

                    compiler_jump_patch(compiler, ushort_start, jump_loc);
                    compiler_jump_patch(compiler, ushort_true, cur_location);

                    #undef BYTELEN
                } else if (strcmp(symbol, "and") == 0) {
                    // evaluate first. if false, evaluate second one
                    #define BYTELEN \
                        (darray_size(compiler_cur_compile_unit(compiler)->bytes))

                    // emit first
                    emit(compiler, sexpr_list_first(rest));

                    const int ushort_begin = compiler_jump_emit(compiler, I_JMP_FALSY);
                    const int cur_size = BYTELEN;

                    rest = sexpr_list_rest(rest);
                    // emit second
                    emit(compiler, sexpr_list_first(rest));

                    const size_t cur_location = BYTELEN - cur_size;

                    compiler_jump_patch(compiler, ushort_begin, cur_location);

                    #undef BYTELEN
                } else if (strcmp(symbol, "while") == 0) {

                    #define BYTELEN \
                        (darray_size(compiler_cur_compile_unit(compiler)->bytes))

                    //condition
                    emit(compiler, sexpr_list_first(rest));
                    const int loop_back_loc = BYTELEN;
                    const int ushort_begin = compiler_jump_emit(compiler, I_JMP_FALSY); // returns where the ushort begins +2

                    compiler_write_byte(compiler, I_POP);
                    rest = sexpr_list_rest(rest);
                    emit(compiler, sexpr_list_first(rest)); 
                    compiler_write_byte(compiler, I_LOOP);
                    compiler_write_ushort(compiler, BYTELEN - loop_back_loc + 2); 
                    compiler_jump_patch(compiler, ushort_begin, BYTELEN-ushort_begin-2);
                    #undef BYTELEN

                } else if(symbol[0] == '@')  {
                    const char* ident = symbol+1;
                    const size_t local_index = localchart_resolve_symbol(compiler->current_unit->locals_chart,ident);
                    if (local_index != -1) {
                        SExpr* new_value = sexpr_list_first(rest);
                        emit(compiler, new_value);
                        compiler_write_byte(compiler, I_SET_LOCAL);
                        compiler_write_byte(compiler, local_index);
                    } else {
                        //todo: string intern, check for existence

                    }
                } else {
                    // if its none of the compiler specials, we assume it's a function call.
                    const int argc = sexpr_list_len(rest);
                    if(argc > 256) {
                        printf("Too many arguments!\n");
                        exit(1);
                    }

#ifdef DEBUG
                    printf("emitting call\n");
#endif
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
        if (argc >= 2) {
//          const char *filename = argv[2];
//          size_t size_read;
//          char *buffer = read_file(filename, &size_read);
            const char* fib ="(fn (fact (x))"
                                "   (if (<= x 2) x  (* x (fact (- x 1)))))"
            " (fact 10)";
            const char* global_vars = "(var a 5)\n"
                                            "(+ a (+ 120 1))\n";
            if (!fib) {
                return 1;  // Error already printed in read_file
            }

            printf("program:\n '%s'\n", global_vars);
            size_t count;
            SExpr **exprs = read_all(global_vars, &count); // or read appropriate function
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

            CompileUnit* main_fn= darray_item(compiler.units, 0);
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

        } else {
            return 1;
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
