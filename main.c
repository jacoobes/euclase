#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "./vm.h"
#include "lobj.h"
#include <limits.h>
#include <string.h>
#include <stdbool.h>
#include "reader.h"
/* If we are compiling on Windows compile these functions */
#ifdef _WIN32
#include <string.h>

static char buffer[2048];

/* Fake readline function */
char *readline(char *prompt) {
    fputs(prompt, stdout);
    fgets(buffer, 2048, stdin);
    char *cpy = malloc(strlen(buffer) + 1);
    strcpy(cpy, buffer);
    cpy[strlen(cpy) - 1] = '\0';
    return cpy;
}

/* Fake add_history function */
void add_history(char *unused) {}

/* Otherwise include the editline headers */
#else
#include <editline/readline.h>
#include <editline/history.h>
#endif//_WIN32
#include "reader.h"


#include "darray.h"  // Assuming you have a header file defining the darray macros and functions


#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <limits.h>
#endif



typedef struct Local {
    solid_str name;
    size_t depth;
    //can we squish this to a byte? need to bounds check locals insert check to ensure
    size_t reg;
} Local;


void local_init(Local* lc, solid_str name, int depth, int reg) {
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

size_t localchart_resolvesymbol(LocalChart* lc, char* symbol) {
    int found = -1;

    Local* local;
    darray_foreach(local, lc->locals) {
        if(local->depth <= lc->cur_depth && strcmp(symbol, SOLID_STR_TO_C(local->name)) == 0) {
            found = local->reg;
        }
    }
    return found;
}
int localchart_insert(LocalChart *lc, char* symbol) {
    if(darray_size(lc->locals) == 256) {
        printf("Variable overflow. Reached maximum variables per scope (256)");
        return -1;
    }
    Local* local;
    darray_foreach(local, lc->locals) {
        if(local->depth == lc->cur_depth && strcmp(symbol, SOLID_STR_TO_C(local->name))) {
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
    struct CompileUnit* next;
} CompileUnit;

CompileUnit compileunit_create() {
    CompileUnit unit;
    unit.next=NULL;
    darray_init(unit.bytes);
    darray_init(unit.constants);
    return unit;
}


typedef struct {
    darray(CompileUnit) units;
    LocalChart* locals_chart;
    CompileUnit* current_unit;
    HashTable* globals;
    HashTable* strings;
} Compiler;

void compiler_append_unit(Compiler* compiler) {
    darray_append(compiler->units, compileunit_create());
}
static inline CompileUnit* compiler_peekunit (Compiler* compiler) {
    return &(darray_item(compiler->units, darray_size(compiler->units)-1));
}

static inline CompileUnit* compiler_cur_compileunit (Compiler* compiler) {
    return compiler->current_unit;
}
void compiler_create(Compiler *compiler, HashTable *globals, HashTable *strings) {
    darray_init(compiler->units);
    compiler_append_unit(compiler);
    compiler->current_unit=compiler_peekunit(compiler);
    LocalChart* lc = malloc(sizeof(LocalChart));
    localchart_create(lc);
    compiler->locals_chart=lc;
    compiler->globals=globals;
    compiler->strings=strings;

}

void compiler_destroy(Compiler *compiler) {
    localchart_free(compiler->locals_chart);
}

static inline int compiler_writebyte(Compiler *compiler, uint8_t byte) {
    CompileUnit* cur_unit = compiler_cur_compileunit(compiler);
    darray_push(cur_unit->bytes, byte);
    return darray_size(cur_unit->bytes)-1;
}

static inline void compiler_addconst(Compiler* compiler, lobj constant) {
    CompileUnit* cur_compileunit = compiler_cur_compileunit(compiler);
    compiler_writebyte(compiler, I_LOAD);
    darray_push(cur_compileunit->constants, constant);
    compiler_writebyte(compiler, 
        (uint8_t)(darray_size(cur_compileunit->constants)-1));
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
    //
    return jmp_instruct_loc;
}

void compiler_jump_patch(Compiler* compiler, size_t ushort_begin, size_t jump_loc) {
    CompileUnit* cur_unit = compiler_cur_compileunit(compiler);
    
    cur_unit->bytes.item[ushort_begin] = (jump_loc >> 8) & 0xff;
    cur_unit->bytes.item[ushort_begin+1] = jump_loc & 0xff;
}
void compiler_write_ushort(Compiler* compiler, unsigned short jump_loc) {
    CompileUnit* cur_unit = compiler_cur_compileunit(compiler);
    darray_push(cur_unit->bytes, (jump_loc >> 8) & 0xff );
    darray_push(cur_unit->bytes, jump_loc & 0xff);
}

void emit(Compiler* compiler, SExpr *expr) {
    SExpr *cur = expr;
    switch (cur->type) {
        case SEXPR_NUMBER: { compiler_addconst(compiler, lobj_num(cur->value.number)); } break;
        case SEXPR_SYMBOL: {
            char *symbol = cur->value.symbol;
                
            size_t local_index = localchart_resolvesymbol(compiler->locals_chart,symbol);
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
            char *str = cur->value.string;
            //todo: check in table for existence to reuse str object
            compiler_addconst(compiler, lobj_str(str));
        }
        break;
        case SEXPR_LIST: {
            SExpr *listhead = sexpr_list_first(cur);
            if (listhead == NULL) {
                printf("Error: Empty list\n");
                exit(1);
            }
            if (listhead->type == SEXPR_SYMBOL) {
                char *symbol = listhead->value.symbol;

                SExpr* rest = sexpr_list_rest(cur); //head should be a list
                if(strcmp(symbol, "exit") == 0) {
                    printf("Exiting...\n");
                    exit(0);
                }
                if(strcmp(symbol, "fn") == 0) {
                    SExpr* fndef = sexpr_list_first(rest);
                    SExpr* fnname = sexpr_list_first(fndef); SExpr* args = sexpr_list_rest(fndef);
                    SExpr* body = sexpr_list_rest(rest);
                    compiler_append_unit(compiler);
                    
                    SExpr* cur;
                    sexpr_list_foreach(cur, body) {
                        emit(compiler, cur);
                    };

                } else if(strcmp(symbol, "+") == 0) {
                    emit_bin_op(compiler, I_ADD, rest);
                } else if(strcmp(symbol, "-") == 0) {
                    emit_bin_op(compiler, I_SUB, rest);
                } else if(strcmp(symbol, "/") == 0) {
                    emit_bin_op(compiler, I_DIV, rest);
                } else if(strcmp(symbol, "*") == 0) {
                    emit_bin_op(compiler, I_MUL, rest);
                } else if(strcmp(symbol, "%") == 0) {
                    emit_bin_op(compiler, I_MOD, rest);
                }
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

                        SExpr* lhs = sexpr_list_first(cur);
                        SExpr* rhs = sexpr_list_first(next);
                        emit(compiler, rhs);

                        int reg = localchart_insert(compiler->locals_chart, lhs->value.symbol);
                        compiler_writebyte(compiler, I_SET_LOCAL);
                        compiler_writebyte(compiler, (uint8_t)reg);
                        even+=1;
                        cur = next;
                    }
                    SExpr* letbody = sexpr_list_rest(rest); 
                    SExpr* bform;
                    sexpr_list_foreach(bform, letbody) {
                        emit(compiler, sexpr_list_first(bform));
                    }
                    int removed = localchart_leave_scope(compiler->locals_chart);
                    printf("removed %d\n", removed);
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
                }

            } else if (listhead->type == SEXPR_NUMBER) {
            } else {
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
            const char* program_add = "(+ 1 3)";
            const char* buffer = "(var a 10) \n"  
                                 "(+ a a)";

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

            CompileUnit* cur = compiler_cur_compileunit(&compiler); 
            VM vm;
            darray_push(cur->bytes, I_RETURN);
            lobj main = into_lobj(LVAL_OBJ, 
                        vm_closure_create(vm_function_create(cur->bytes.item, cur->bytes.size, 
                                                             cur->constants.item, cur->constants.size,
                                                             0, lobj_str("main"))));
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
