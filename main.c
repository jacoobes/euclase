#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "./vm.h"

/* If we are compiling on Windows compile these functions */
#ifdef _WIN32
#include <string.h>

static char buffer[2048];

/* Fake readline function */
char* readline(char* prompt) {
  fputs(prompt, stdout);
  fgets(buffer, 2048, stdin);
  char* cpy = malloc(strlen(buffer)+1);
  strcpy(cpy, buffer);
  cpy[strlen(cpy)-1] = '\0';
  return cpy;
}

/* Fake add_history function */
void add_history(char* unused) {}

/* Otherwise include the editline headers */
#else
#include <editline/readline.h>
#include <editline/history.h>
#endif//may need to use editline for mac linux repl
#include "mpc.h"


int add_local(Compiler* compiler, char* name) {
    if (compiler->local_count == MAX_LOCALS) {
        printf("Variable overflow. Reached maximum variables per scope (256)\n");
        return -1;
    }

    // Check for existing variable with same name at current depth
    for (int i = 0; i < compiler->local_count; i++) {
        if (compiler->locals[i].depth == compiler->scope_depth 
                && strcmp(compiler->locals[i].name.contents, name) == 0) {
            return -1;
        }
    }
    int prevcount = compiler->local_count;
    // Add new local
    compiler->locals[prevcount].depth = compiler->scope_depth;
    compiler->locals[prevcount].name = (vm_string){ .len=strlen(name), .contents=name }; //probably make a wrapper function
    compiler->local_count++;
    return prevcount;
}




void emit_code(mpc_ast_t* ast, Compiler* compiler) { 
    if (strstr(ast->tag, "number")) {
        /* Check if there is some error in conversion */
        errno = 0;
        double x = strtod(ast->contents, NULL);
        if(errno == ERANGE) {
            printf("errange");
            exit(1);
        }
        int idx = compiler_add_constant(compiler, lobj_num(x)); 
        compiler_emit(compiler, I_LOAD);
        compiler_emit(compiler, (uint8_t)idx);
        printf("LOADCONST at %d\n", idx);
    } 

    if(strstr(ast->tag, "symbol")) {
        char* op = ast->contents;
        if (strcmp("+", op) == 0) {
            compiler_emit(compiler, I_ADD);
        } else if (strcmp("-", op) == 0) {
            compiler_emit(compiler, I_SUB);
        } else if (strcmp("*", op) == 0) {
            compiler_emit(compiler, I_MUL);
        } else if (strcmp("/", op) == 0) {
            compiler_emit(compiler, I_DIV);
        } else if (strcmp("%", op) == 0) {
            compiler_emit(compiler, I_MOD);
        } 
    }
    mpc_ast_t* exp = NULL;
    printf("TAG VISITED %s\n", ast->tag);

    if (strcmp(ast->tag, ">") == 0) { exp = *ast->children; } 
    printf("contents of tag %s\n", exp->tag);
    /* Iterate s expression */
    for(int i = ast->children_num-1; i >= 0; i--) {
        if (strcmp(ast->children[i]->contents, "(") == 0) { continue; }
        if (strcmp(ast->children[i]->contents, ")") == 0) { continue; }
        if (strcmp(ast->children[i]->tag,  "regex") == 0) { continue; }
        emit_code(ast->children[i], compiler);
    }
}




int main(int argc, char** argv) {
  puts("Version 0.0.0.0.3");
  puts("Press Ctrl+c to Exit\n");


  mpc_parser_t* Number   = mpc_new("number");
  mpc_parser_t* Symbol = mpc_new("symbol");
  mpc_parser_t* Sexpr  = mpc_new("sexpr");
  mpc_parser_t* Expr     = mpc_new("expr");
  mpc_parser_t* Euclase = mpc_new("prog");
  /* Define them with the following Language */
  mpca_lang(MPCA_LANG_DEFAULT,
    "                                                     \
        number   : /-?[0-9]+\\.?[0-9]*/ ;                 \
        symbol   : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ;     \
        sexpr    : '(' <expr>* ')' ;                      \
        expr     : <number> | <symbol> | <sexpr>;         \
        prog : /^/ <expr>* /$/ ;                          \
    ",
    Number, Symbol, Sexpr, Expr, Euclase);
  while(1) {
    char* input = readline("euclase>");
    add_history(input);
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Euclase, &r)) {
        /* On Success Print the AST */
        mpc_ast_print(r.output);
        mpc_ast_t* a = r.output;
        Compiler compiler;
        compiler_create(&compiler);
        emit_code(a, &compiler);
        compiler_emit(&compiler, I_HALT);
//      for(int i = 0; i < compiler->values_count; i++) {
//          lobj_print(compiler->values[i]); printf(",");
//      }

        printf("code_count %d\n", compiler.code_count);
        printf("values_count %d\n", compiler.values_count);
        VM vm;
        vm_create(&vm, compiler.code, compiler.code_count,
                    NULL, 0, compiler.values, compiler.values_count);

        vm_exec(&vm, 0, 0);
        //lobj_println(result);
        mpc_ast_delete(r.output);
    } else {
        /* Otherwise Print the Error */
        mpc_err_print(r.error);
        mpc_err_delete(r.error);
    }
    /* Echo input back to user */
    printf("No you're a %s\n", input);
    free(input);
  }
  mpc_cleanup(5, Number, Symbol, Sexpr, Expr, Euclase);
  return 0;
}
