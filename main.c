#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "./vm.h"
#include "lobj.h"
#include "memory.h"
#include <string.h>
#include <ctype.h>
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



typedef struct Local {
    solid_str* name;
    size_t reg;
    size_t depth;
} Local;


void local_create(Local* lc, solid_str* name) {
    lc->depth = 0;
    lc->reg=0;
    lc->name=name;
}

void local_free(Local* lc) {
    solid_str_free(lc->name);
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
    darray_free(lc->locals);
    free(lc);
}

void localchart_leave_scope(LocalChart* lc) {
    lc->cur_depth--;
}
void localchart_enter_scope(LocalChart* lc) {
    lc->cur_depth++;
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
    CompileUnit* current_unit;
    HashTable* globals;
    HashTable* strings;
    LocalChart* locals_chart;
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

    compiler->globals=globals;
    compiler->strings=strings;

}

void compiler_destroy(Compiler *compiler) {
    //darray_free(compiler->bytes);
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
        if(hd->type == SEXPR_NUMBER) {
            compiler_addconst(compiler, lobj_num(hd->value.number));
        } else {
            emit(compiler, hd);
        }
    }
    compiler_writebyte(compiler, op);
}
void emit(Compiler* compiler, SExpr *expr) {
    SExpr *cur = expr;
    switch (cur->type) {
        case SEXPR_NUMBER: {
            compiler_addconst(compiler, lobj_num(cur->value.number));
        }
        break;
        case SEXPR_SYMBOL: {
            char *symbol = cur->value.symbol;
            //auto local_index = local_chart.resolve_symbol(symbol);
            if (symbol) {
                //push_byte(I_GET_LOCAL);
                //push_byte(local_index);
            } else if (false) {
                //push_byte(I_GET_GLOBAL );
                //auto idx = word_mappings.at(s->lexeme);
                //push_byte(idx);
            } else {
                //throw std::runtime_error(s->lexeme + " is an unresolved symbol");
            }
        }
        break;
        case SEXPR_STRING: {
            char *str = cur->value.string;
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
                    
                } else if (strcmp(symbol, "var") == 0) { //global variable
                    SExpr* varname = sexpr_list_first(rest);
                    if(varname->type!= SEXPR_SYMBOL) {
                        printf("For var declaration, expected symbol");
                        exit(1);
                    }
                    SExpr* value = sexpr_list_first(sexpr_list_rest(rest));
                    emit(compiler,value);
                    //todo global
                }  else {
                    // try function call
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
    /* Define them with the following Language */
    if (argc > 1 && strcmp(argv[1], "-e") == 0) {
        if (argc >= 2) {
//          const char *filename = argv[2];
//          size_t size_read;
//          char *buffer = read_file(filename, &size_read);
            const char* buffer = 
            "(+ 1 (+ 2 3))";

            if (!buffer) {
                return 1;  // Error already printed in read_file
            }

            printf("%s\n", buffer);
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
            printf("%zu\n", cur->bytes.size);
            printf("%zu\n", cur->constants.size);
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
