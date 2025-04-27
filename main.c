#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "gc.h"
#include "vm.h"
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




int main(int argc, char **argv) {
    puts("Version 0.0.0.0.3");
    puts("Press Ctrl+c to Exit\n");
    
    if (argc > 1 && strcmp(argv[1], "-e") == 0) {
        if (argc == 2) {
//          const char *filename = argv[2];
//          size_t size_read;
//          char *buffer = read_file(filename, &size_read);
//
            const char* fib = QUOTE( 
                (fn (cookie (a))
                    (fn (brownie (b))
                       (+ a b)))
                (var s (cookie 20))

                (print (s 20))
            );

            const char* natives = QUOTE(
                (print (+ 2 2)) 
            );

            const char* global_vars = QUOTE( 
                (var a 5)
                (+ a (+ 120 1))
            );

            const char* looper = QUOTE( 
                (let (a 10) 
                    (while a 
                        (@a (- a 1))))
            );

            printf("program:\n '%s'\n", looper);
            size_t count;
            SExpr **exprs = read_all(looper, &count); // or read appropriate function
            printf("Number of expressions: %zu\n", count);

            HashTable globals, strings;
            table_create(&globals);
            table_create(&strings);

            Compiler compiler;
            compiler_create(&compiler, &globals, &strings);
            for (size_t i = 0; i < count; i++) {
                compiler_emit(&compiler, exprs[i]);
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
            gc_init(&vm);

            vm_exec(&vm, 0);

            vm_free(&vm);

            gc_shutdown();

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
                compiler_emit(&compiler, exprs[i]);
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
#ifdef DEBUG_COMPILER
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
