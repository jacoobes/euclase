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
#include "reader.h"


#include "darray.h"  // Assuming you have a header file defining the darray macros and functions




typedef struct Local {
    solid_str name;
    size_t depth;
    //can we squish this to a byte? need to bound check locals insert check to ensure
    size_t reg;
} Local;


void local_init(Local* lc, const solid_str name, const int depth, const int reg) {
    lc->depth = depth;
    lc->reg=reg;
    lc->name=name;
}

void local_free(Local* lc) {
    solid_str_free(&lc->name);
    free(lc);
}

typedef struct LocalChart {
    int cur_depth;
    darray(Local) locals;
} LocalChart;

void localchart_create(LocalChart* lc) {
    lc->cur_depth = 0;
    darray_init(lc->locals);
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
        Local* cur = &(darray_item(lc->locals, darray_size(lc->locals)-1));
        if(cur->depth <= lc->cur_depth) break;
        darray_pop(lc->locals);
        removed++;
    }
    return removed;
}
void localchart_enter_scope(LocalChart* lc) {
    lc->cur_depth++;
}

size_t localchart_resolvesymbol(const LocalChart* lc, char* symbol) {
    size_t found = -1;

    Local* local;
    darray_foreach(local, lc->locals) {
        if(local->depth <= lc->cur_depth && strcmp(symbol, SOLID_STR_TO_C(local->name)) == 0) {
            found = local->reg;
        }
    }
    return found;
}
size_t localchart_insert(LocalChart *lc, char* symbol) {
    if(darray_size(lc->locals) == 256) {
        printf("Variable overflow. Reached maximum variables per scope (256)");
        return -1;
    }
    Local* local;
    darray_foreach(local, lc->locals) {
        if(local->depth == lc->cur_depth && strcmp(symbol, SOLID_STR_TO_C(local->name)) != 0) {
            return -1;
        }
    }
    size_t prev_size = darray_size(lc->locals);
    Local new_local;
    local_init(&new_local, solid_str_new(symbol), lc->cur_depth, prev_size);
    darray_push(lc->locals, new_local);
    return prev_size;
}


typedef struct CompileUnit {
    darray(uint8_t) bytes;
    darray(lobj) constants;
    //owned: need to free this
    char* name;
    int arity;
    struct CompileUnit* next;
} CompileUnit;

// name is duplicated, need to manage memory
CompileUnit compileunit_create(const char* name, const int arity) {
    return (CompileUnit){
        .next=NULL,
        .bytes=darray_new(),
        .constants=darray_new(),
        .name=strdup(name),
        .arity=arity
    };
}


typedef struct {
    darray(CompileUnit) units;
    LocalChart* locals_chart;
    CompileUnit* current_unit;
    HashTable* globals;
    HashTable* strings;
} Compiler;

void compiler_append_unit(Compiler* compiler, char* name, int arity) {
    darray_append(compiler->units, compileunit_create(name, arity));

}

static inline CompileUnit* compiler_peekunit (const Compiler* compiler) {
    size_t size = darray_size(compiler->units);
    if (size == 0) {
        printf("Error: No units to peek!\n");
        return NULL;
    }
    return &darray_item(compiler->units, size-1);
}


static inline CompileUnit* compiler_cur_compileunit (const Compiler* compiler) {
    return compiler->current_unit;
}

void compiler_create(Compiler *compiler, HashTable *globals, HashTable *strings) {
    darray_init(compiler->units);
    compiler_append_unit(compiler, "__main__", 0);
    compiler->current_unit= &darray_item(compiler->units, 0);
    LocalChart* lc = malloc(sizeof(LocalChart));

    localchart_create(lc);
    compiler->locals_chart=lc;

    compiler->globals=globals;
    compiler->strings=strings;

}

void compiler_destroy(const Compiler *compiler) {
    localchart_free(compiler->locals_chart);
    CompileUnit* cu;
    darray_foreach(cu, compiler->units) {
        free(cu->name);
    }
}

static inline int compiler_writebyte(const Compiler *compiler, uint8_t byte) {
    CompileUnit* cur_unit = compiler_cur_compileunit(compiler);
    darray_push(cur_unit->bytes, byte);
    return darray_size(cur_unit->bytes)-1;
}



static inline void compiler_addconst(Compiler* compiler, lobj constant) {
    CompileUnit* cur_compileunit = compiler_cur_compileunit(compiler);
    compiler_writebyte(compiler, I_LOAD);
#ifdef DEBUG
    printf("I_LOAD "); lobj_print(&constant); printf(" to %s\n", compiler_cur_compileunit(compiler)->name);
#endif
    darray_push(cur_compileunit->constants, constant);
    compiler_writebyte(compiler, darray_size(cur_compileunit->constants) - 1);
}


char* read_file(const char *filename, size_t *size_read) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error: Could not open file '%s'\n", filename);
        return NULL;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
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

SExpr* expect_next_form(SExprType ty, SExpr* exp, char* assertmsg) {
    SExpr* rest = sexpr_list_rest(exp);
    if(ty == sexpr_list_first(rest)->type) {
        return rest;
    }
    fprintf(stderr, "%s\n", assertmsg);
    exit(1);
}

void emit(Compiler* compiler, SExpr* sexpr);

static inline void emit_bin_op(Compiler* compiler, enum Instruction op, SExpr* rest) {
    SExpr* cur;
    sexpr_list_foreach(cur, rest) {
        SExpr* hd = sexpr_list_first(cur);
        emit(compiler, hd);
    }
    compiler_writebyte(compiler, op);
}

int compiler_jump_emit(Compiler* compiler, enum Instruction instruction) {
    compiler_writebyte(compiler,instruction); //0
    //short
    int jmp_instruct_loc = compiler_writebyte(compiler, 0xff); //1 placeholder
                           compiler_writebyte(compiler, 0xff); //2 placeholder
    //end short
    return jmp_instruct_loc;
}

void compiler_jump_patch(const Compiler* compiler, size_t ushort_begin, size_t jump_loc) {
    const CompileUnit* cur_unit = compiler_cur_compileunit(compiler);
    
    cur_unit->bytes.item[ushort_begin] = (jump_loc >> 8) & 0xff;
    cur_unit->bytes.item[ushort_begin+1] = jump_loc & 0xff;
}
void compiler_write_ushort(const Compiler* compiler, unsigned short jump_loc) {
    CompileUnit* cur_unit = compiler_cur_compileunit(compiler);
    darray_push(cur_unit->bytes, (jump_loc >> 8) & 0xff );
    darray_push(cur_unit->bytes, jump_loc & 0xff);
}

void emit(Compiler* compiler, SExpr *expr) {
    SExpr *cur = expr;
    switch (cur->type) {
        case SEXPR_NUMBER: {
            lobj snum = lobj_num(cur->value.number);
#ifdef DEBUG
            printf("emitting number: I_LOAD "); lobj_println(&snum);
#endif
            compiler_addconst(compiler, snum);
        } break;
        case SEXPR_SYMBOL: {
            char *symbol = cur->value.symbol;

            const size_t local_index = localchart_resolvesymbol(compiler->locals_chart,symbol);
            if (local_index != -1) {
                compiler_writebyte(compiler,  I_GET_LOCAL);
                compiler_writebyte(compiler,  local_index);
            } else {
                compiler_addconst(compiler, lobj_str(symbol));
                compiler_writebyte(compiler,I_GET_GLOBAL);
            }
        }
        break;
        case SEXPR_STRING: {
            const char *str = cur->value.string;
            //todo: check in table for existence to reuse str object
            compiler_addconst(compiler, lobj_str(str));
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
                    //example form: (fn (name (a b))  
                    //                  (+ a b))
                    SExpr* fndef = sexpr_list_first(rest); // '(name (a b))
                    SExpr* fnname = sexpr_list_first(fndef); // "name"
                    SExpr* args = sexpr_list_first(sexpr_list_rest(fndef)); // '(a b)
                    SExpr* body = sexpr_list_rest(rest); // '(+ a b)

                    if(fnname->type != SEXPR_SYMBOL) {
                        printf("fnname must be a simple symbol\n");
                        exit(1);
                    }


                    compiler_append_unit(compiler, fnname->value.symbol, sexpr_list_len(args));

                    compiler->current_unit = &darray_pop(compiler->units);

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
                    };

                    if (sexpr_list_len(body)) {
#ifdef DEBUG
                        printf("emitting in function: has a body\n");
#endif
                        SExpr* cur;
                        sexpr_list_foreach(cur, body) {
                            emit(compiler, sexpr_list_first(cur));
                        };
                    } else {
#ifdef DEBUG
                        printf("emitting in function: has no body\n");
#endif
                        compiler_addconst(compiler, lobj_num(0));
                    }


                    compiler_writebyte(compiler, I_RETURN);


                    const CompileUnit* cu = compiler_cur_compileunit(compiler);
                    const lobj newclosure = into_lobj(LVAL_OBJ,
                        vm_closure_create(vm_function_create(cu->bytes.item, darray_size(cu->bytes),
                        cu->constants.item, darray_size(cu->constants),
                        cu->arity, lobj_str(cu->name))));


                    CompileUnit* cla;
#ifdef DEBUG
                    printf("compile units found:\n");
                    darray_foreach(cla, compiler->units) {
                        printf("%s,", cla->name);
                    }
                    printf(" cur unit '%s' \n", cu->name);
#endif
                    // Return to previous pointer as current unit. that is,
                    // the one before appending the unit ( size - 2)
                    compiler->current_unit = compiler_peekunit(compiler);
                    compiler_addconst(compiler, newclosure);

                }
                else if(strcmp(symbol, "+") == 0) { emit_bin_op(compiler, I_ADD, rest); }
                else if(strcmp(symbol, "-") == 0) { emit_bin_op(compiler, I_SUB, rest); }
                else if(strcmp(symbol, "/") == 0) { emit_bin_op(compiler, I_DIV, rest); }
                else if(strcmp(symbol, "*") == 0) { emit_bin_op(compiler, I_MUL, rest); }
                else if(strcmp(symbol, "%") == 0) { emit_bin_op(compiler, I_MOD, rest); }
                else if (strcmp(symbol, "let") == 0) { //locally scoped variables
                    SExpr* bindings = sexpr_list_first(rest);
                    int even = 1;
                    localchart_enter_scope(compiler->locals_chart);

                    SExpr* cur;
                    sexpr_list_foreach(cur, bindings) {
                        //empty list
                        if(sexpr_list_first(cur) == NULL) {
                            break;
                        }
                        SExpr* next = sexpr_list_rest(cur);
                        //if uneven forms count for let declaration
                        if(next == NULL && (even % 2 == 1)) {
                            printf("Uneven forms in let binding\n");
                            exit(1);
                        }

                        const SExpr* lhs = sexpr_list_first(cur);
                        SExpr* rhs = sexpr_list_first(next);
                        emit(compiler, rhs);

                        const size_t reg = localchart_insert(compiler->locals_chart, lhs->value.symbol);
                        compiler_writebyte(compiler, I_SET_LOCAL); compiler_writebyte(compiler, (uint8_t)reg);
                        even+=1;
                        cur = next;
                    }
                    SExpr* let_body = sexpr_list_rest(rest);
                    SExpr* bform;
                    sexpr_list_foreach(bform, let_body) {
                        emit(compiler, sexpr_list_first(bform));
                    }
                    const size_t removed = localchart_leave_scope(compiler->locals_chart);
                    printf("removed %zu\n", removed);
                    for(int i = 0; i < removed;i++ ) {
                        //compiler_writebyte(compiler, I_POP);
                    }

                } else if (strcmp(symbol, "var") == 0) { //global variable
                    SExpr* varname = sexpr_list_first(rest);
                    if(varname->type!= SEXPR_SYMBOL) {
                        printf("For var declaration, expected symbol");
                        exit(1);
                    }
                    SExpr* value = sexpr_list_first(sexpr_list_rest(rest));
                    emit(compiler,value);

                    lobj v;
                    compiler_addconst(compiler, lobj_str(varname->value.symbol));
                    compiler_writebyte(compiler, I_SET_GLOBAL);
                }  else if (strcmp(symbol, "if") == 0) {
                    #define BYTELEN \
                        (darray_size(compiler_cur_compileunit(compiler)->bytes))
                    //condition
                    emit(compiler, sexpr_list_first(rest));
                    // emit a jmp if falsy
                    int ushort_start = compiler_jump_emit(compiler, I_JMP_FALSY);
                    // we get the length of bytes after jmp emitted,
                    int cursize = BYTELEN;
                    rest = sexpr_list_rest(rest);

                    //true val
                    emit(compiler, sexpr_list_first(rest));
                    int ushort_true = compiler_jump_emit(compiler, I_JMP);
                    // we can patch the jmp needed after processing truth branch by getting difference 
                    // (length after processing truthy branch) - (length before) 
                    int jmpfalse_location = BYTELEN - cursize; 


                    cursize = BYTELEN;
                    //false val
                    emit(compiler, sexpr_list_first(sexpr_list_rest(rest)));
                    size_t cur_location = BYTELEN - cursize;

                    compiler_jump_patch(compiler, ushort_start, jmpfalse_location);
                    compiler_jump_patch(compiler, ushort_true, cur_location);

                    #undef BYTELEN
                } else if (strcmp(symbol, "or") == 0) {
                    

                } else if (strcmp(symbol, "while") == 0) {

                    #define BYTELEN \
                        (darray_size(compiler_cur_compileunit(compiler)->bytes))

                    //condition
                    emit(compiler, sexpr_list_first(rest));
                    int loop_back_loc = BYTELEN;
                    int ushort_begin = compiler_jump_emit(compiler, I_JMP_FALSY); // returns where the ushort begins +2

                    compiler_writebyte(compiler, I_POP);  
                    rest = sexpr_list_rest(rest);
                    emit(compiler, sexpr_list_first(rest)); 
                    compiler_writebyte(compiler, I_LOOP); 
                    compiler_write_ushort(compiler, BYTELEN - loop_back_loc + 2); 
                    compiler_jump_patch(compiler, ushort_begin, BYTELEN-ushort_begin-2);
                    #undef BYTELEN

                } else if(symbol[0] == '@')  {
                    char* ident = symbol+1;
                    size_t local_index = localchart_resolvesymbol(compiler->locals_chart,ident);
                    if (local_index != -1) {
                        SExpr* new_value = sexpr_list_first(rest);
                        emit(compiler, new_value);
                        compiler_writebyte(compiler, I_SET_LOCAL);
                        compiler_writebyte(compiler, local_index);
                    } else {
                        //todo: string intern, check for existence
                    }
                } else {
                    // if its none of the compiler specials, we assume it's a function call.
                    int argc = sexpr_list_len(rest);
                    if(argc > 256) {
                        printf("Too many arguments!\n");
                        exit(1);
                    }
                    const CompileUnit* found = NULL;
                    //CompileUnit* cu;
                    // darray_foreach(cu, compiler->units) {
                    //     if(strcmp(cu->name, symbol)==0 && cu->arity == argc) {
                    //         found=cu;
                    //         break;
                    //     }
                    // }
                    //if(found == NULL) { printf("function not found when calling\n"); exit(1); }
                    
                    SExpr* cur;
                    //emit bytecode for arguments
                    sexpr_list_foreach(cur, rest) { emit(compiler, sexpr_list_first(cur)); }

                    // need to emit I_CALL N where N is number of args
                    compiler_writebyte(compiler, I_CALL);
                    compiler_writebyte(compiler, argc);
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
            const char* buffer = "(fn (add ()) 10) "
                                 "(fn (sad ()) (let (a 5) (+ a a))) "
                                 "(sad)";

            if (!buffer) {
                return 1;  // Error already printed in read_file
            }

            printf("program:\n '%s'\n", buffer);
            size_t count;
            SExpr **exprs = read_all(buffer, &count); // or read appropriate function
            printf("Number of expressions: %zu\n", count);

            HashTable globals, strings;
            table_create(&globals);
            table_create(&strings);

            Compiler compiler;
            compiler_create(&compiler, &globals, &strings);
            for (size_t i = 0; i < count; i++) {
                emit(&compiler, exprs[i]);
            }
            

            CompileUnit* mmain= &darray_item(compiler.units, 0);

            if(mmain == NULL) {
                printf("main function entry point not found");
                exit(1);
            }

            darray_push(mmain->bytes, I_RETURN);
            lobj main = into_lobj(LVAL_OBJ,
                        vm_closure_create(vm_function_create(mmain->bytes.item, darray_size(mmain->bytes), 
                                                             mmain->constants.item, darray_size(mmain->constants),
                                                             0, lobj_str("__main__"))));
#ifdef DEBUG
            uint8_t* b;
            darray_foreach(b, mmain->bytes) {
                printf("%d ", *b);
            }
            printf("\n");
#endif
            VM vm;
            vm_create(&vm, &globals, &strings, &main);
            vm_exec(&vm, 0);

            for(int i =0; i < count; i++) {
                free_sexpr(exprs[i]);
            }

            table_free(&globals);
            table_free(&strings);
            compiler_destroy(&compiler);
 
        } else {
            return 1;
        }
    } else {
        while (true) {
            char *input = readline(">> ");
            if (input && *input)
                add_history(input);
            else {
                continue;
            }
            size_t count;
            SExpr** exprs = read_all(input, &count);

            HashTable globals, strings;
            table_create(&globals);
            table_create(&strings);

            Compiler compiler;
            compiler_create(&compiler, &globals, &strings);

            for (size_t i = 0; i < count; i++) {
                printf("Emitting bytecode for form:"); print_sexpr(exprs[i]);
                emit(&compiler, exprs[i]);
            }
            //VM vm;
            //vm_create(&vm, &globals, &strings, NULL);
            printf("\n");
            for (size_t i = 0; i < count; i++) {
                print_sexpr(exprs[i]);
                printf(" ");
            }
            printf("\n");
            //free(input);
            printf("Num expressions: %zu\n", count);
            for (size_t i = 0; i < count; i++) {
                free_sexpr(exprs[i]);
            }
            table_free(&globals);
            table_free(&strings);
            compiler_destroy(&compiler);
        }
    }
    return 0;
}
