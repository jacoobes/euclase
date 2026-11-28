#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "gc.h"
#include "vm.h"
#include "reader.h"
#include "io.h"
#include "compiler.h"


#define QUOTE(...) #__VA_ARGS__


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

                (print (s 20)));


            printf("program:\n '%s'\n", fib);
            size_t count;
            SExpr **exprs = read_all(fib, &count); // or read appropriate function
            printf("Number of expressions: %zu\n", count);


            Compiler compiler; compiler_create(&compiler);
            for (size_t i = 0; i < count; i++) {
                compiler_emit(&compiler, exprs[i]);
            }

            VM vm; 
            gc_init(&vm);
            vm_create(&vm);

            lobj main = compiler_link(&compiler, &vm);
            

            vm_exec(&vm, &main, 0);
            vm_free(&vm);

            gc_shutdown();

            compiler_destroy(&compiler);
            // done with s expressions. we free all of them
            for(int i =0; i < count; i++) {
                free_sexpr(exprs[i]);
            }
            free(exprs);

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

            
            Compiler compiler; compiler_create(&compiler);
            for (size_t i = 0; i < count; i++) {
                compiler_emit(&compiler, exprs[i]);
            }

            // done with s expressions. we free all of them
            for(int i =0; i < count; i++) {
                free_sexpr(exprs[i]);
            }
            free(exprs);

            CompileUnit* main_fn= compiler.current_unit;
            if(main_fn == NULL) {
                printf("main function entry point not found");
                exit(1);
            }
            // compiling makes no vm objects; compiler_link creates them (and
            // appends main's I_RETURN), so the GC and VM only need to exist by then
            VM vm;
            gc_init(&vm);
            vm_create(&vm);
            lobj main = compiler_link(&compiler, &vm);
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

            vm_exec(&vm, &main, 0);


            vm_free(&vm);
            gc_shutdown();
            free((void*)program);

            return 0;
        }
    } 
    return 0;
}
