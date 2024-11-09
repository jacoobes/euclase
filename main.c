#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "./vm.h"
#include "lobj.h"
#include "memory.h"
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
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

typedef struct {
    darray(uint8_t) bytes;
    HashTable* globals;
    HashTable* strings;
    vm_function* cur_function;
    darray(vm_function*) functions;
} Compiler;


void compiler_set_cur_function(Compiler* compiler, vm_function* function) {
    if (function == NULL) {
        fprintf(stderr, "Error: function to be set is NULL\n");
        exit(1);
    }

    if (function == compiler->cur_function) {
        printf("Current function is already set\n");
        return;
    }

    // Store the current function in the array
    darray_push(compiler->functions, compiler->cur_function);
    compiler->cur_function = function;
}

vm_function* compiler_cur_function(Compiler* compiler) {
    return compiler->cur_function;
}

void compiler_create(Compiler *compiler, HashTable *globals, HashTable *strings) {
    darray_init(compiler->bytes);
    darray_init(compiler->functions);
    compiler->globals=globals;
    compiler->strings=strings;
    vm_function *initial = ALLOCATE(vm_function, 1);
    vm_function_create(initial);

}

void compiler_load_const(Compiler *compiler, SExpr* expr, uint8_t byte) {
    //add expr to current vm compiling function, returning index
    // ?
    darray_append(compiler->bytes, byte);
    //push index of current vm function to bytes
    //   darray_append(compiler->bytes, idx);

}

void compiler_destroy(Compiler *compiler) {
    darray_free(compiler->bytes);
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

void emit(Compiler* compiler, SExpr *expr) {
    SExpr *cur = expr;
    switch (cur->type) {
        case SEXPR_NUMBER: {
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
            SExpr *listhead = cur->value.list;
            if (listhead == NULL) {
                printf("Error: Empty list\n");
                exit(1);
            }
            if (listhead->type == SEXPR_SYMBOL) {
                char *symbol = listhead->value.symbol;
                if(strcmp(symbol, "exit") == 0) {
                    printf("Exiting...\n");
                    exit(0);
                }
                if(strcmp(symbol, "+") == 0) {
                    SExpr *cur = listhead;

                }

            } else if (listhead->type == SEXPR_NUMBER) {
                darray_push(compiler->bytes, I_LOAD);
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
        if (argc > 2) {
            const char *filename = argv[2];
            size_t size_read;
            char *buffer = read_file(filename, &size_read);

            if (!buffer) {
                return 1;  // Error already printed in read_file
            }

            size_t count;
            SExpr **exprs = read_all(buffer, &count); // or read appropriate function
            printf("Number of expressions: %zu\n", count);
            for(int i =0; i < count; i++) {
                free_sexpr(exprs[i]);
            }

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
