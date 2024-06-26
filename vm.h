#ifndef vm_h
#define vm_h

#define MAX_LOCALS 256
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_STR };
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
    I_CALL        , // call function
    I_JMP         , // jmp to instr ptr
    I_GET_LOCAL   ,
    I_SET_LOCAL   ,
    I_POP
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

typedef struct {
    uint8_t returnip;
    lobj* locals; //the array of locals
} Context;



#define DEFAULT_CALL_STACK_SIZE 256
#define DEFAULT_NUM_LOCALS 10


typedef struct {
  //Obj obj;
  int arity;
  uint8_t* chunk;
  uint8_t chunk_ct;
} vm_function;

typedef struct {
    char* contents; //must be null terminated
    int len; 
} vm_string;

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


typedef struct {
    vm_string name;
    int depth;
} Local;

typedef struct {
    int const_count;
    uint8_t* code;
    int code_cap;
    int code_count;

    lobj* values; 
    int values_count;
    int val_cap;


    Local locals[MAX_LOCALS]; //subject to change
    int scope_depth;
    int local_count; 

} Compiler;

lobj lobj_num(double x);
lobj lobj_err(int err);
void compiler_create(Compiler*);
int  compiler_add_constant(Compiler*, lobj);
void compiler_emit(Compiler* c, uint8_t byte);
void compiler_free(Compiler*);
void vm_create(VM* vm,
            uint8_t *code,
            int code_count, 
            int *globals, //globals
            int nglobals, //num globals
            lobj* values, //constants
            int values_count); 

void vm_exec(VM*, int, int);
#endif
