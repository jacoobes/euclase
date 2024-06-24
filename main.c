#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

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

/////////////////////////////// VM

enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR };
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };
enum Instruction {
    I_HALT        , // End execution 
    I_LOAD        , // I_LOAD
    I_NEGATE      , // negate a number
    I_ADD         , // add two values
    I_SUB         , // subtract two numbers
    I_MUL         , // multiply two numbers
    I_DIV         , // divide two numbers
    I_MOD         , // modulus two numbers
    I_CALL        ,
};

typedef union {
    double num;
    int err;
} Value;

/* Declare New lval Struct */
typedef struct {
  int type;
  Value v;
} lobj; //lisp object

#define DEFAULT_CALL_STACK_SIZE 256
#define DEFAULT_NUM_LOCALS 10

typedef struct {
    uint8_t returnip;
    lobj* locals; //the array of locals
} Context;

typedef struct {
    uint8_t *code;
    int code_count;

    // global variable space
    int *globals;
    int nglobals;
    //program counter
    int pc;

    // Operand stack, grows upwards
    lobj stack[256];
    
    lobj* values;
    int values_count;

    //stack pointer
    int sp;


    Context call_stack[DEFAULT_CALL_STACK_SIZE];
    int frame_count;

} VM;

VM *vm_create(uint8_t *code,
        int code_count, 
        int *globals, //globals
        int nglobals, //num globals
        lobj* values, //constants
        int values_count //num constants
    ) {
    VM* vm = calloc(1, (sizeof(VM)));
    vm->code = code;
    vm->code_count = code_count;
    vm->globals = globals;
    vm->nglobals = nglobals;

    vm->values = values;
    vm->values_count = values_count;


    
    printf("VM SP INITIALIZED %d\n", vm->sp); 
    return vm;
}

void vm_push(VM *vm, lobj value) {
    vm->stack[vm->sp++] = value;
}

lobj vm_pop(VM *vm) {
    return vm->stack[--vm->sp];
}

void vm_add_frame(VM* vm, Context frame) {
    if (vm->frame_count >= DEFAULT_CALL_STACK_SIZE) {
        printf("Stack overflow\n");
        exit(1);
    }
    
    int index = vm->frame_count++;
    vm->call_stack[index] = frame;
}


/* Create a new number type lval */
lobj lobj_num(double x) {
  lobj obj;
  obj.type = LVAL_NUM;
  obj.v.num = x;
  return obj;
}

/* Create a new error type lobj */
lobj lobj_err(int x) {
  lobj o;
  o.type = LVAL_ERR ;
  o.v.err = x;
  return o;
}

/////////////////////////////// VM



typedef struct {
    int const_count;
    uint8_t* code;
    int code_cap;
    int code_count;

    lobj* values; 
    int values_count;
    int val_cap;
} Compiler;

Compiler* compiler_create() {
    Compiler* compiler = malloc(sizeof(Compiler));
    
    compiler->values = malloc(sizeof(lobj) * 256);
    compiler->values_count = 0;
    compiler->val_cap = 256;

    compiler->code = (uint8_t*)malloc(sizeof(uint8_t) * 1024);
    compiler->code_count = 0;
    compiler->code_cap = 1024;
    return compiler;
}

void lobj_free(lobj* o) {

  switch (o->type) {
    /* Do nothing special for number type */
    case LVAL_NUM: break;

    /* For Err or Sym free the string data */
    case LVAL_ERR:  break;
    case LVAL_SYM: break;

    /* If Sexpr then delete all elements inside */
    case LVAL_SEXPR: break;
  }
  /* Free the memory allocated for the "lval" struct itself */
  /* when we create heap objs this will be necessary*/
  //free(o);
}

void compiler_free(Compiler* c) {
    if (c == NULL) return;
    // Free the code array
    if (c->code != NULL) {
        free(c->code);
    }

    // Free the constants array
    if (c->values != NULL) {
        // If any constant contains dynamically allocated memory (like strings),
        // we need to free that first
        for (int i = 0; i < c->values_count ; i++) {
            lobj_free(&c->values[i]);
        }
        free(c->values);
    }
    // Free the compiler struct itself
    free(c);
}

int compiler_add_constant(Compiler* c, lobj constant) {
//    for (int i = 0; i < c->constant_count; i++) {
//        if (lobj_eq(&c->values[i], &constant)) {
//            return i;  // Constant already exists
//        }
//    }
    
    if (c->values_count >= c->val_cap) {
        c->val_cap *= 2;
        c->values = realloc(c->values, sizeof(lobj) * c->val_cap);
    }
    
    int index = c->values_count++;
    c->values[index] = constant;
    return index;
}

void compiler_emit(Compiler* c, uint8_t byte) {
    if (c->code_count >= c->code_cap) {
        c->code_cap *= 2;
        c->code = realloc(c->code, sizeof(uint8_t) * c->code_cap);
    }
    c->code[c->code_count++] = byte;
}

/* Print an "lobj" */
void lobj_print(lobj o) {
  switch (o.type) {
    /* In the case the type is a number print it */
    /* Then 'break' out of the switch. */
    case LVAL_NUM: printf("%f", o.v.num); break;

    /* In the case the type is an error */
    case LVAL_ERR:
      /* Check what type of error it is and print it */
      if (o.v.err == LERR_DIV_ZERO) { printf("Error: Division By Zero!"); }
      if (o.v.err == LERR_BAD_OP)   { printf("Error: Invalid Operator!"); }
      if (o.v.err == LERR_BAD_NUM)  { printf("Error: Invalid Number!"); }
    break;
  }
}

/* Print an "lobj" followed by a newline */
void lobj_println(lobj v) { lobj_print(v); putchar('\n'); }



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
    /* Iterate s expression */
    for(int i = ast->children_num-1; i >= 0; i--) {
        if (strcmp(ast->children[i]->contents, "(") == 0) { continue; }
        if (strcmp(ast->children[i]->contents, ")") == 0) { continue; }
        if (strcmp(ast->children[i]->tag,  "regex") == 0) { continue; }
        emit_code(ast->children[i], compiler);
    }
}


inline unsigned short parse_ushort (uint8_t* const chunk, size_t location) {
    return (chunk[location-1] << 8) | chunk[location];
}

void vm_exec(VM *vm, int start_ip, int flags) {


    // callstackp 
    int callstackp = 0;
    while (vm->pc < vm->code_count) {
        int instruction = vm->code[vm->pc];
        switch (instruction) {
            case I_HALT: {
                printf("HALTED: ");
                for(int i = 0; i < vm->sp; i++) {
                    lobj_println(vm->stack[i]);
                }
                puts("");
            } return;
            case I_LOAD: {
                int const_idx = vm->code[++vm->pc]; 
                if(const_idx > vm ->values_count) {
                    puts("const idx exceeds value count");
                    exit(1);
                }
                vm_push(vm, vm->values[const_idx]);
            } break;
            case I_NEGATE:{
                lobj a = vm_pop(vm);
                if (a.type == LVAL_NUM) {
                    lobj result = {LVAL_NUM, {.num = -a.v.num}};
                    vm_push(vm, result);
                } else {
                    puts("typeerror");
                    exit(1);
                }
            } break;
            case I_ADD: {
                lobj b = vm_pop(vm); lobj a = vm_pop(vm);
                if (a.type == LVAL_NUM && b.type == LVAL_NUM) {
                    lobj result = {LVAL_NUM, {.num = a.v.num + b.v.num}};
                    printf("RESULT ADD:");lobj_println(result);
                    vm_push(vm, result);
                } else {
                    puts("typeerror");
                    exit(1);
                }
            } break;
            case I_SUB: {
                lobj b = vm_pop(vm); lobj a = vm_pop(vm);
                if (a.type == LVAL_NUM && b.type == LVAL_NUM) {
                    lobj result = {LVAL_NUM, {.num = a.v.num - b.v.num}};
                    vm_push(vm, result);
                } else {
                    puts("typeerror");
                    exit(1);
                }
            } break;
            
            case I_MUL: {
                lobj b = vm_pop(vm); lobj a = vm_pop(vm);
                if (a.type == LVAL_NUM && b.type == LVAL_NUM) {
                    lobj result = {LVAL_NUM, {.num = a.v.num * b.v.num}};
                    vm_push(vm, result);
                } else {
                    puts("typeerror");
                    exit(1);
                }
            } break;
            case I_DIV: {
                lobj b = vm_pop(vm); lobj a = vm_pop(vm);
                if (a.type == LVAL_NUM && b.type == LVAL_NUM) {
                    lobj result = {LVAL_NUM, {.num = a.v.num / b.v.num}};
                    vm_push(vm, result);
                } else {
                    puts("typeerror");
                    exit(1);
                }
            } break;
            case I_MOD: {
                lobj b = vm_pop(vm); lobj a = vm_pop(vm);
                if (a.type == LVAL_NUM && b.type == LVAL_NUM) {
                    lobj result = {LVAL_NUM, {.num = (int)a.v.num % (int)b.v.num}};
                    vm_push(vm, result);
                } else {
                    puts("typeerror");
                    exit(1);
                }
            } break;
            case I_CALL: {
                

            } break;
            default:
                // Handle unknown instruction
                break;
        }
        vm->pc++;  // Move to next instruction
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
        symbol   : '+' | '-' | '*' | '/' | '%';           \
        sexpr    : '(' <expr>* ')' ;                      \
        expr     : <number> | <symbol> | <sexpr>;         \
        prog : /^/ <expr>* /$/ ;                       \
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
        Compiler* compiler = compiler_create();
        emit_code(a, compiler);
        compiler_emit(compiler, I_HALT);
//      for(int i = 0; i < compiler->values_count; i++) {
//          lobj_print(compiler->values[i]); printf(",");
//      }

        printf("code_count %d\n", compiler->code_count);
        printf("values_count %d\n", compiler->values_count);
        VM* vm = vm_create(compiler->code, compiler->code_count,
                           NULL, 0,
                           compiler->values, compiler->values_count);

        vm_exec(vm, 0, 0);
        compiler_free(compiler);
        free(vm);
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
